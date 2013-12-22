/*
 * copyright (c) 2010 gonzoj
 *
 * please check the CREDITS file for further information.
 *
 * this program is free software: you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program.  if not, see <http://www.gnu.org/licenses/>.
 */

#define _PLUGIN

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <util/compat.h>

#include <module.h>

#include <d2gs.h>
#include <internal.h>
#include <mcp.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>

#include <util/net.h>
#include <util/list.h>
#include <util/string.h>
#include <util/types.h>

static struct setting module_settings[] = (struct setting []) {
	SETTING("GameIdleTime", 0, INTEGER)
};

static struct list module_settings_list = LIST(module_settings, struct setting, 1);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

typedef void (*pthread_cleanup_handler_t)(void *);

const char *pina_colada_song[] = {
	"if you like pina coladas",
	"and getting caught in the rain",
	"if you're not into yoga",
	"if you have half a brain",
	"if you like making love at midnight",
	"in the dunes of the cape",
	"then I'm the love that you've looked for",
	"write to me and escape"
};

typedef struct {
	char *s_addr;
	int hits;
} hot_addr;

static struct list hot_addrs = LIST(NULL, hot_addr, 0);

static dword cur_addr = 0;
static bool relieved = FALSE;
static bool d2gs_cleanup = FALSE;

pthread_mutex_t d2gs_cleanup_m;
pthread_cond_t d2gs_cleanup_cv;

static int hits;

void thread_idle(int);
int on_mcp_join_game(void *);
int on_d2gs_cleanup(void *);
int on_d2gs_chat(void *);

_export const char * module_get_title() {
	return "dclone";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "hunts hot IPs";
}

_export int module_get_license() {
	return LICENSE_GPL_V3;
}

_export module_type_t module_get_type() {
	return (module_type_t) { MODULE_D2GS, MODULE_ACTIVE };
}

_export bool module_load_config(struct setting_section *s) {		
	int i;
	for (i = 0; i < s->entries; i++) {
		struct setting *set = module_setting(s->settings[i].name);
		if (set) {
			if (s->settings[i].type == INTEGER && set->type == INTEGER) {
				set->i_var = s->settings[i].i_var;
			} else if (s->settings[i].type == STRING) {
				if (set->type == BOOLEAN) {
					set->b_var = !strcmp(string_to_lower_case(s->settings[i].s_var), "yes");
				} else if (set->type == STRING){
					set->s_var = strdup(s->settings[i].s_var);
				} else if (set->type == INTEGER) {
					sscanf(s->settings[i].s_var, "%li", &set->i_var);
				}
			}
		} else if (!strcmp(s->settings[i].name, "HotIPs")) {
			char *c_setting = strdup(s->settings[i].s_var);
			char *tok = strtok(c_setting, ":");
			while (tok) {
				hot_addr hot = { strdup(tok), 0 };
				list_add(&hot_addrs, &hot);

				tok = strtok(NULL, ":");
			}
			free(c_setting);
		}
	}

	return TRUE;
}

_export bool module_init() {
	register_packet_handler(MCP_RECEIVED, 0x04, on_mcp_join_game);
	register_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, on_d2gs_cleanup);
	register_packet_handler(D2GS_RECEIVED, 0x26, on_d2gs_chat);

	pthread_mutex_init(&d2gs_cleanup_m, NULL);
	pthread_cond_init(&d2gs_cleanup_cv, NULL);

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(MCP_RECEIVED, 0x04, on_mcp_join_game);
	unregister_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, on_d2gs_cleanup);
	unregister_packet_handler(D2GS_RECEIVED, 0x26, on_d2gs_chat);

	pthread_mutex_destroy(&d2gs_cleanup_m);
	pthread_cond_destroy(&d2gs_cleanup_cv);

	ui_add_statistics_plugin("dclone", "found %i hot IPs (", hits);
	struct iterator it = list_iterator(&hot_addrs);
	hot_addr *hot;
	int i = 0;
	while ((hot = iterator_next(&it))) {
		if (i++) ui_add_statistics(" ");
		ui_add_statistics("%s: %i", hot->s_addr, hot->hits);
		free(hot->s_addr);
	}
	ui_add_statistics(")\n");

	list_clear(&hot_addrs);

	return TRUE;
}

_export void * module_thread(void *arg) {
	bool hit = FALSE;

	char s_addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &cur_addr, s_addr, INET_ADDRSTRLEN);
	char *addr = strrchr(s_addr, '.');

	plugin_print("dclone", "hit game server %s\n", addr);

	struct iterator it = list_iterator(&hot_addrs);
	hot_addr *hot;

	while ((hot = iterator_next(&it))) {
		if (!strcmp(addr, hot->s_addr)) {
			hit = TRUE;
			hits++;
			hot->hits++;
		}
	}

	if (hit) {
		plugin_print("dclone", "found hot address - idling\n");

		// sing the pina colada song in order to keep the game open
		// actually I'm not sure if it's enough to keep the game from closing
		while (!relieved && !d2gs_cleanup) {
			int i;
			for (i = 0; i < (sizeof(pina_colada_song) / sizeof(char *)) && !d2gs_cleanup && !relieved; i++) {
				d2gs_send(0x14, "00 00 %s 00 00 00", pina_colada_song[i]);

				thread_idle(2);
			}
		}
	} else {
		plugin_print("dclone", "sleeping for %i seconds\n", module_setting("GameIdleTime")->i_var);

		thread_idle(module_setting("GameIdleTime")->i_var);
	}

	plugin_print("dclone", "leaving game\n");

	d2gs_send(0x69, "");

	pthread_exit(NULL);
}

_export void module_cleanup() {
	relieved = FALSE;
	d2gs_cleanup = FALSE;;
}

void thread_idle(int s) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += s;

	pthread_mutex_lock(&d2gs_cleanup_m);

	if (!d2gs_cleanup) {
		pthread_cond_timedwait(&d2gs_cleanup_cv, &d2gs_cleanup_m, &ts);
	}

	pthread_mutex_unlock(&d2gs_cleanup_m);
}

int on_mcp_join_game(void *p) {
	mcp_packet_t *packet = MCP_CAST(p);

	dword status = net_get_data(packet->data, 14, dword);

	if (status == 0x00) {
		cur_addr = net_get_data(packet->data, 6, dword);
	}
	
	return FORWARD_PACKET;
}

int on_d2gs_cleanup(void *p) {
	internal_packet_t *packet = INTERNAL_CAST(p);

	if (*(dword *)(packet->data) == MODULES_CLEANUP) {
		pthread_mutex_lock(&d2gs_cleanup_m);

		d2gs_cleanup = TRUE;

		pthread_cond_signal(&d2gs_cleanup_cv);

		pthread_mutex_unlock(&d2gs_cleanup_m);
	}

	return FORWARD_PACKET;
}

int on_d2gs_chat(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	char *player  = (char *) (packet->data + 9);
	char *message = (char *) (packet->data + 9 + strlen(player) + 1);

	if (!strcmp(message, ".continue")) {
		plugin_print("dclone", "%s took over - continuing hunt\n", player);

		relieved = TRUE;
	}

	return FORWARD_PACKET;
}
