/*
 * Copyright (C) 2010 gonzoj
 *
 * Please check the CREDITS file for further information.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dirent.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "moduleman.h"

#include "settings.h"
#include "gui.h"

#include <util/config.h>
#include <util/list.h>
#include <util/string.h>

typedef void (*pthread_cleanup_handler_t)(void *);

typedef const char * (*module_get_info_t)();

typedef const int (*module_get_license_t)();

typedef module_type_t (*module_get_type_t)();

typedef void * (*module_thread_t)(void *);

typedef bool (*module_load_config_t)(struct setting_section *);

typedef bool (*module_init_t)();

typedef bool (*module_finit_t)();

typedef void (*module_cleanup_t)();

typedef struct {
	char *name;
	void *handle;
	module_get_info_t title;
	module_get_info_t version;
	module_get_info_t author;
	module_get_info_t description;
	module_get_license_t license;
	module_get_type_t type;
	module_load_config_t config;
	module_init_t init;
	module_finit_t finit;
	module_thread_t thread;
	module_cleanup_t cleanup;
	pthread_t tid;
	bool is_running;
	bool is_blocked[3];
	pthread_mutex_t block_mutex[3];
	pthread_cond_t block_cond[3];
	bool disabled;
} module_t;

static struct list mm_modules[3];

static pthread_mutex_t mm_block_m;

static struct list blacklist;

static struct list mm_schedule[3];

static pthread_mutex_t mm_schedule_m[3];

_export pthread_mutex_t schedule_update_m;
_export pthread_cond_t schedule_update_cond_v;

static struct list mm_extensions;

static void dump_module(module_t m) {
	print("%s:\n", m.name);
	char *bar = calloc(strlen(m.name) + 1, sizeof(char));
	memset(bar, '-', strlen(m.name));
	print("%s\n", bar);
	free(bar);
	print("title:       %s\n", m.title());
	print("version:     %s\n", m.version());
	print("author:      %s\n", m.author());
	print("description: %s\n", m.description());
}

void init_module_manager() {
	int i;
	for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
		mm_modules[i] = list_new(module_t);

		mm_schedule[i] = list_new(module_routine_t);
		pthread_mutex_init(&mm_schedule_m[i], NULL);
	}
	pthread_mutex_init(&mm_block_m, NULL);

	blacklist = list_new(char *);
	char *c = strdup(setting("PluginBlacklist")->s_var);
	if (c) {
		char *t = strtok(c, ":");
		while (t) {
			list_add(&blacklist, strdup(t));
			strtok(NULL, ":");
		}

		free(c);
	}

	pthread_mutex_init(&schedule_update_m, NULL);
	pthread_cond_init(&schedule_update_cond_v, NULL);

	mm_extensions = list_new(extension_t);
}

void finit_module_manager() {
	int i;
	for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
		list_clear(&mm_modules[i]);

		list_clear(&mm_schedule[i]);
		pthread_mutex_destroy(&mm_schedule_m[i]);
	}
	pthread_mutex_destroy(&mm_block_m);

	struct iterator it = list_iterator(&blacklist);
	while (iterator_next(&it)) {
		free(*(char **)it.element);
	}
	list_clear(&blacklist);

	pthread_mutex_destroy(&schedule_update_m);
	pthread_cond_destroy(&schedule_update_cond_v);

	list_clear(&mm_extensions);
}

static bool is_module_blacklisted(const char *module) {
	struct iterator it = list_iterator(&blacklist);
	char **t;
	while ((t = iterator_next(&it))) {
		if (strstr(module, *t)) {
			return TRUE;
		}
	}

	return FALSE;
}

#define MM_NUM_MODULE_SYMBOLS 11

static const char *mm_module_symbols[] =
{
	"module_get_title",
	"module_get_version",
	"module_get_author",
	"module_get_description",
	"module_get_license",
	"module_get_type",
	"module_load_config",
	"module_init",
	"module_finit",
	"module_thread",
	"module_cleanup"
};

static bool resolve_symbols(module_t *m) {
	int offset = sizeof(char *) + sizeof(void *);
	int i;
	for (i = 0; i < MM_NUM_MODULE_SYMBOLS; i++) {
		void *h = dlsym(m->handle, mm_module_symbols[i]);
		if (!h) {
			error("error: could not resolve symbol %s\n", mm_module_symbols[i]);
			return FALSE;
		} else {
			memcpy((byte *) m + offset + i * sizeof(void *), &h, sizeof(void *));
		}
	}
	return TRUE;
}

static bool load_module(const char *mod) {
	module_t m;

	m.name = (char *) malloc(strlen(mod) + 1);
	strcpy(m.name, mod);

	m.handle = dlopen(mod, RTLD_LAZY);

	if (!m.handle) {
		error("error: could not dlopen plugin %s (%s)\n", mod, dlerror());
		free(m.name);
		return FALSE;
	}

	if (!resolve_symbols(&m)) {
		dlclose(m.handle);
		free(m.name);
		return FALSE;
	}

	if (m.license() != LICENSE_GPL_V3) {
		dlclose(m.handle);
		free(m.name);
		error("error: plugin %s is not licensed under the GPL v3 and will therefore not be loaded.\n");
		return FALSE;
	}

	m.tid = 0;
	m.is_running = FALSE;
	memset(m.is_blocked, FALSE, 3 * sizeof(bool));

	m.disabled = FALSE;

	list_add(&mm_modules[m.type().engine], &m);

	print(" -- %s v%s (%s): %s\n", m.title(), m.version(), m.author(), m.description());

	return TRUE;
}

static int compare_module(module_t *m, const char *name) {
	return !strcmp(strrchr(m->name, '/') ? strrchr(m->name, '/') + 1 : m->name, name);
}

static void load_module_config(struct setting_section *s) {
	static bool (*load_config)(struct setting_section *) = NULL;
	static bool disable = FALSE;
	if (!strcmp(s->name, "Plugin")) {
		load_config = NULL;
		disable = FALSE;
		char *name;
		struct setting *set = config_get_setting(s, "Name");
		if (set) {
			if (set->type == STRING) {
				name = set->s_var;
			} else {
				return;
			}
		} else {
			return;
		}
		set = config_get_setting(s, "Disabled");
		if (set) {
			if (set->type == STRING) {
				disable = string_compare(set->s_var, "yes", FALSE) ? TRUE : FALSE;
			}
		}
		int i;
		for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
			module_t *m;
			if ((m = list_find(&mm_modules[i], (comparator_t) compare_module, name))) {
				if (disable) {
					m->disabled = TRUE;
					char *m_name = strrchr(m->name, '/');
					print("plugin %s disabled\n", m_name ? ++m_name : m->name);
				} else {
					load_config = m->config;
				}
				break;
			}
		}
	}
	if (load_config) {
		load_config(s);
	}
}

static void init_modules() {
	int i;
	for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
		module_t *m;
		struct iterator it = list_iterator(&mm_modules[i]);
		while ((m = iterator_next(&it))) {
			if (!m->disabled) {
				if (!m->init()) {
					error("error: failed to initialize plugin %s\n", m->name);
					dlclose(m->handle);
					iterator_remove(&it);
				}
			}
			pthread_mutex_init(&m->block_mutex[MODULE_BNCS], NULL);
			pthread_cond_init(&m->block_cond[MODULE_BNCS], NULL);
			pthread_mutex_init(&m->block_mutex[MODULE_MCP], NULL);
			pthread_cond_init(&m->block_cond[MODULE_MCP], NULL);
			pthread_mutex_init(&m->block_mutex[MODULE_D2GS], NULL);
			pthread_cond_init(&m->block_cond[MODULE_D2GS], NULL);
		}
	}
}

void load_modules(const char *moddir) {
	print("loading plugins...\n");
	DIR *modules = opendir(moddir);
	if (modules == NULL) {
		error("error: could not open sub-directory for plugins\n");

		return;
	}
	struct dirent *file;
	while ((file = readdir(modules))) {
		char *ext = strrchr(file->d_name, '_');
		if (!ext) {
			continue;	
		}
		if (strcmp(ext, "_plugin.so")) {
			continue;
		}
		if (is_module_blacklisted(file->d_name)) {
			print("plugin %s on blacklist... skip\n", file->d_name);
			continue;
		}
		char *module;
		string_new(&module, moddir, "/", file->d_name, "");
		print("loading plugin %s\n", file->d_name);
		if (!load_module(module)) {
			error("error: failed to load plugin %s\n", module);
		} /*else {
			print("%s loaded\n", file->d_name);
		}*/
		free(module);
	}
	closedir(modules);

	print("loading plugin.conf... \n");
	if (!config_load_settings("plugin.conf", load_module_config)) {
		print("warning: failed to load plugin.conf (plugins will fall back to default values)\n");
	}

	init_modules();
}

static bool unload_module(struct iterator *it) {
	module_t *m = it->element;

	if (!m->disabled) {
		if (!m->finit()) {
			error("error: plugin failed to unload correctly\n");
		}
	}

	pthread_mutex_destroy(&m->block_mutex[MODULE_BNCS]);
	pthread_cond_destroy(&m->block_cond[MODULE_BNCS]);
	pthread_mutex_destroy(&m->block_mutex[MODULE_MCP]);
	pthread_cond_destroy(&m->block_cond[MODULE_MCP]);
	pthread_mutex_destroy(&m->block_mutex[MODULE_D2GS]);
	pthread_cond_destroy(&m->block_cond[MODULE_D2GS]);

	void *h = m->handle;

	free(((module_t *) it->element)->name);
	iterator_remove(it);

	// remove for valgrind test
	if (dlclose(h)) {
		error("error: failed to dlclose plugin");
		return FALSE;
	}

	return TRUE;
}

void unload_modules() {
	print("unloading plugins...\n");

	int i;
	for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
		module_t *m;
		struct iterator it = list_iterator(&mm_modules[i]);
		while ((m = iterator_next(&it))) {
			if (!unload_module(&it)) {
				char *m_name = strrchr(m->name, '/');
				error("error: failed to unload plugin %s\n", m_name ? ++m_name : m->name);
			}
		}
	}
}

void start_modules(int engine) {
	print("starting %s plugins...\n", engine == MODULE_BNCS ? "BNCS" : engine == MODULE_MCP ? "MCP" : engine == MODULE_D2GS ? "D2GS" : "");

	struct iterator i = list_iterator(&mm_modules[engine]);
	module_t *m;
	while ((m = iterator_next(&i))) {
		if (m->type().type == MODULE_ACTIVE && !m->disabled) {
			print("starting plugin %s\n", m->title());
			pthread_create(&m->tid, NULL, m->thread, NULL);
			m->is_running = TRUE;
		}
	}
}

void cleanup_modules(int engine) {

	struct iterator i = list_iterator(&mm_modules[engine]);
	module_t *m;
	while ((m = iterator_next(&i))) {
		if (m->type().type == MODULE_ACTIVE && m->is_running) {

			if (m->is_blocked[engine]) {
				pthread_mutex_lock(&mm_block_m); // enclosed the whole function


				pthread_mutex_lock(&m->block_mutex[engine]);
				pthread_cond_signal(&m->block_cond[engine]);
				pthread_mutex_unlock(&m->block_mutex[engine]);
				m->is_blocked[engine] = FALSE;

				pthread_mutex_unlock(&mm_block_m);
			}

			pthread_join(m->tid, NULL);

			m->is_running = FALSE;
			memset(m->is_blocked, FALSE, 3 * sizeof(bool));
			// m->cleanup() used to get called here
		}
		m->cleanup();
	}
}

bool is_module_thread() {
	pthread_t tid = pthread_self();

	int i;
	for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
		struct iterator it = list_iterator(&mm_modules[i]);
		module_t *m;
		while ((m = iterator_next(&it))) {
			if (m->tid == tid && m->is_running) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

_export void block_modules(int engine) {
	pthread_mutex_lock(&mm_block_m);

	pthread_t tid = pthread_self();

	int i;
	for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
		struct iterator it = list_iterator(&mm_modules[i]);
		module_t *m;
		while ((m = iterator_next(&it))) {
			if (m->tid != tid && m->type().type == MODULE_ACTIVE && m->is_running) {
				m->is_blocked[engine] = TRUE;
			}
		}
	}

	pthread_mutex_unlock(&mm_block_m);
}

_export void unblock_modules(int engine) {
	pthread_mutex_lock(&mm_block_m);

	pthread_t tid = pthread_self();

	int i;
	for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
		struct iterator it = list_iterator(&mm_modules[i]);
		module_t *m;
		while ((m = iterator_next(&it))) {
			if (m->tid != tid && m->type().type == MODULE_ACTIVE && m->is_running && m->is_blocked[engine]) {
				pthread_mutex_lock(&m->block_mutex[engine]);
				pthread_cond_signal(&m->block_cond[engine]);
				pthread_mutex_unlock(&m->block_mutex[engine]);
				m->is_blocked[engine] = FALSE;
			}
		}
	}

	pthread_mutex_unlock(&mm_block_m);
}

void module_wait(int engine) {
	pthread_mutex_lock(&mm_block_m);

	pthread_t tid = pthread_self();

	int i;
	for (i = MODULE_BNCS; i <= MODULE_D2GS; i++) {
		struct iterator it = list_iterator(&mm_modules[i]);
		module_t *m;
		while ((m = iterator_next(&it))) {
			if (m->type().type == MODULE_ACTIVE && m->is_running && m->tid == tid) {
				if(m->is_blocked[engine]) {
					pthread_mutex_lock(&m->block_mutex[engine]);

					pthread_mutex_unlock(&mm_block_m);

					pthread_cond_wait(&m->block_cond[engine], &m->block_mutex[engine]);

					pthread_mutex_unlock(&m->block_mutex[engine]);
				} else {
					pthread_mutex_unlock(&mm_block_m);
				}
				iterator_destroy(&it);
				return;
			}
		}
	}

	pthread_mutex_unlock(&mm_block_m);
}

_export void schedule_module_routine(int engine, module_routine_t routine) {

	pthread_mutex_lock(&mm_schedule_m[engine]);

	list_add(&mm_schedule[engine], &routine);

	pthread_mutex_lock(&schedule_update_m);

	// signal waiting threads that a new routine has been added
	pthread_cond_broadcast(&schedule_update_cond_v);

	pthread_mutex_unlock(&schedule_update_m);

	pthread_mutex_unlock(&mm_schedule_m[engine]);

}

_export void execute_module_schedule(int engine) {

	struct iterator it;
	module_routine_t *r;

	pthread_mutex_lock(&mm_schedule_m[engine]);
	// necessary because r() could call *_send (implicit pthread_exit() possible)
	pthread_cleanup_push((pthread_cleanup_handler_t) pthread_mutex_unlock, (void *) &mm_schedule_m[engine]); 

	it = list_iterator(&mm_schedule[engine]);
	r = iterator_next(&it);

	pthread_cleanup_pop(1);

	while (r) {
		(*r)();

		pthread_mutex_lock(&mm_schedule_m[engine]);
		// necessary because r() could call *_send (implicit pthread_exit() possible)
		pthread_cleanup_push((pthread_cleanup_handler_t) pthread_mutex_unlock, (void *) &mm_schedule_m[engine]);

		r = iterator_next(&it);

		pthread_cleanup_pop(1);
	}

	pthread_mutex_lock(&mm_schedule_m[engine]);
	// necessary because r() could call *_send (implicit pthread_exit() possible)
	pthread_cleanup_push((pthread_cleanup_handler_t) pthread_mutex_unlock, (void *) &mm_schedule_m[engine]); 

	list_clear(&mm_schedule[engine]);

	pthread_cleanup_pop(1);

}

_export void clear_module_schedule(int engine) {

	pthread_mutex_lock(&mm_schedule_m[engine]);

	list_clear(&mm_schedule[engine]);

	pthread_mutex_unlock(&mm_schedule_m[engine]);

}

void unsupported_interface(char *caller, ...) {
	error("error: unsupported extension called by %s\n", caller);
}

extension_t unsupported_extension = { "unsupported extenstion", unsupported_interface };

_export void register_extension(char *name, extension_interface_t call) {
	extension_t e = { name, call };
	list_add(&mm_extensions, &e);
}

_export extension_t * extension(char *name) {
	struct iterator it = list_iterator(&mm_extensions);
	extension_t *e;
	while ((e = iterator_next(&it))) {
		if (!strcmp(e->name, name)) {
			return e;
		}
		
	}
	return &unsupported_extension;
}
