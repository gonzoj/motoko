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
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "clientman.h"
#include "d2gs.h"
#include "moduleman.h"
#include "packet.h"
#include "settings.h"
#include "gui.h"

#include <util/file.h>
#include <util/net.h>
#include <util/string.h>

#include <wardenc.h>

static char bin_p[MAX_PATH];
static char lib_p[MAX_PATH];
static char data_p[MAX_PATH];

static char *home;
_export char *profile;

static struct setting settings[] = (struct setting []) {
	SETTING("VersionByte", 0, INTEGER),
	SETTING("Hostname", .s_var = "", STRING),
	SETTING("BNCSPort", 0, INTEGER),
	SETTING("D2GSPort", 0, INTEGER),
	SETTING("ExpansionKey", .s_var = "", STRING),
	SETTING("ClassicKey", .s_var = "", STRING),
	SETTING("VersionInfo", .s_var = "", STRING),
	SETTING("Owner", .s_var = "", STRING),
	SETTING("BinaryDir", .s_var = "", STRING),
	SETTING("BNCSDisconnect", FALSE, BOOLEAN),
	SETTING("MCPDisconnect", FALSE, BOOLEAN),
	SETTING("ResponseWarden", FALSE, BOOLEAN),
	SETTING("Verbose", FALSE, BOOLEAN),
	SETTING("Logging", FALSE, BOOLEAN),
	SETTING("ReconnectDelay", 0, INTEGER),
	SETTING("PluginBlacklist", .s_var = "", STRING),
	SETTING("Debug", FALSE, BOOLEAN)
};

_export struct list settings_list = LIST(settings, struct setting, 17);

typedef void (*setting_cleaner_t)(struct setting *);

typedef struct {
	setting_cleaner_t cleanup;
	struct setting *set;
} setting_cleanup_t;

static struct list setting_cleaners = LIST(NULL, setting_cleanup_t, 0);

static bool populate_paths(char *argv) {
	int len = readlink("/proc/self/exe", bin_p, MAX_PATH);
	if (len < 0 || len >= MAX_PATH) {
		if (strlen(argv) >= MAX_PATH || *argv != '/') {
			return FALSE;
		} else {
			strcpy(bin_p, argv);
		}
	}
	char *exe = strrchr(bin_p, '/');
	if (exe) {
		*exe = exe == bin_p ? *exe : '\0';
		*++exe = '\0';
	} else {
		return FALSE;
	}

	if (!chdir(bin_p)) {
		if (!chdir(BINDIR_TO_LIBDIR)) {
			if (!getcwd(lib_p, MAX_PATH)) {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}

	if (!chdir(bin_p)) {
		if (!chdir(BINDIR_TO_DATADIR)) {
			if (!getcwd(data_p, MAX_PATH)) {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}

	return TRUE;
}

static bool create_appdata() {
	home = getenv("HOME");
	if (home) {
		if (!chdir(home)) {
			int status = FALSE;
			if (chdir(".motoko")) {
				printf(".motoko doesn't exist, creating directory structure... ");
				status |= mkdir(".motoko", S_IRWXU);
				status |= mkdir(".motoko/profiles", S_IRWXU);
				status |= mkdir(".motoko/profiles/default", S_IRWXU);
				status |= mkdir(".motoko/warden", S_IRWXU);
				status |= chdir(".motoko");
				printf(status ? "failed." : "done.");
			}
			if (!status) {
				if (chdir("warden")) {
					status |= mkdir("warden", S_IRWXU);
					if (status) {
						printf("failed to create directory for the warden client engine.");
					}
				}
				status |= chdir("..");
				if (!status && chdir("profiles")) {
					status |= mkdir("profiles", S_IRWXU);
					status |= mkdir("profiles/default", S_IRWXU);
					status |= chdir("profiles/default");
					if (!status) {
						char *binconf, *modconf;
						string_new(&binconf, data_p, "/motoko.conf.default", "");
						string_new(&modconf, data_p, "/plugin.conf.default", "");

						printf("copying %s...\n", binconf);
						status |= file_copy(binconf, "motoko.conf") < 0 ? TRUE
								: FALSE;
						printf("copying %s...\n", modconf);
						status |= file_copy(modconf, "plugin.conf") < 0 ? TRUE
								: FALSE;

						free(binconf);
						free(modconf);
					}
					if (status) {
						printf("failed to create the profile 'default'.\n");
					}
				}
				return !status;
			}
		}
	}
	return FALSE;
}

static void dump_settings() {
	printf("\nmotoko.conf:\n\n");
	struct iterator it = list_iterator(&settings_list);
	while (iterator_next(&it)) {
		struct setting *s = it.element;
		if (s->type == STRING && s->s_var) {
			printf("%s: %s\n", s->name, s->s_var);
		} else if (s->type == INTEGER) {
			printf("%s: %i\n", s->name, (int) s->i_var);
		} else if (s->type == BOOLEAN) {
			printf("%s: %s\n", s->name, s->b_var ? "yes" : "no");
		}
	}
	printf("\n");
}

static void cleanup_string_setting(struct setting *s) {
	if (s->s_var) {
		free(s->s_var);
	}
}

static void cleanup_settings() {
	/*struct iterator it = list_iterator(&settings_list);
	while (iterator_next(&it)) {
		struct setting *s = it.element;
		if (s->type == STRING && s->s_var) {
			if (strlen(s->s_var)) {
				free(s->s_var);
			}
		}
	}*/

	struct iterator it = list_iterator(&setting_cleaners);
	setting_cleanup_t *sc;
	while ((sc = iterator_next(&it))) {
		sc->cleanup(sc->set);
	}

	list_clear(&setting_cleaners);
}

static void process_config(struct setting_section *s) {
	int i;
	for (i = 0; i < s->entries; i++) {
		struct setting *set = setting(s->settings[i].name);
		if (set) {
			if (s->settings[i].type == INTEGER && set->type == INTEGER) {
				set->i_var = s->settings[i].i_var;
			} else if (s->settings[i].type == STRING) {
				if (set->type == BOOLEAN) {
					set->b_var = !strcmp(string_to_lower_case(s->settings[i].s_var), "yes");
				} else if (set->type == STRING){
					set->s_var = strdup(s->settings[i].s_var);

					setting_cleanup_t sc = { cleanup_string_setting, set };
					list_add(&setting_cleaners, &sc);
				} else if (set->type == INTEGER) {
					sscanf(s->settings[i].s_var, "%li", &set->i_var);
				}
			}
		}
	}
}

// WARDENC
static wardenc_callbacks imports = {
		d2gs_get_hash,
		d2gs_send_raw,
		NULL,
		NULL,
		NULL,
		NULL
};

static void wardenc_dump_packet(byte *packet, size_t len) {
	net_dump_data(packet, len, UI_WHITE, ui_print);
}

static char warden_p[MAX_PATH];

static bool sigint = FALSE;

void sigint_handler(int signal) {
	sigint = TRUE;
}

int main(int argc, char *argv[]) {

	signal(SIGPIPE, SIG_IGN);

	int status = 0;

	if (!populate_paths(argv[0])) {
		printf("failed to resolve locations of necessary data. program exits.\n");
		exit(EXIT_FAILURE);
	}

	if (!create_appdata()) {
		printf(".motoko corrupted. program exits.\n");
		exit(EXIT_FAILURE);
	}

	status |= chdir(home); // check return value to make gcc happy
	status |= chdir(".motoko");

	if (chdir("profiles") != 0) {
		printf(".motoko doesn't contain profiles. program exits.\n");
		exit(EXIT_FAILURE);
	}

	if (argc > 1) {
		profile = argv[1];
	} else {
		printf("no profile specified, using profile 'default'.\n");
		profile = "default";
	}

	if (chdir(profile) != 0) {
		printf("the specified profile '%s' does not exist. program exits.\n", profile);
		exit(EXIT_FAILURE);
	}

	printf("loading motoko.conf of profile '%s'... ", profile);
	if (config_load_settings("motoko.conf", process_config)) {
		printf("done.\n");
		// dump_settings();
	} else {
		printf("failed. program exits.\n");
		exit(EXIT_FAILURE);
	}

	status |= chdir(lib_p);
	if (chdir("motoko/plugins") != 0) {
		printf("failed to locate the directory for plugins. program exits.\n");
		exit(EXIT_FAILURE);
	}
	char mod_p[MAX_PATH];
	if (!getcwd(mod_p, MAX_PATH)) {
		printf("failed to resolve path to the plugins directory. program exits.\n");
		exit(EXIT_FAILURE);
	}

	status |= chdir(home);

	if (chdir(".motoko/warden") != 0) {
		printf("directory for the warden client engine doesn't exist. program exits.\n");
		exit(EXIT_FAILURE);
	}
	if (!getcwd(warden_p, MAX_PATH)) {
		printf("failed to resolve path to the directory for the warden client engine. program exits.\n");
		exit(EXIT_FAILURE);
	}

	if (setting("ResponseWarden")->b_var) {
		if (setting("Verbose")->b_var) {
			imports.dump_packet = wardenc_dump_packet;
		}
		wardenc_init(&imports, warden_p);
	}

	status |= chdir(home);
	status |= chdir(".motoko/profiles");
	status |= chdir(profile);

	ui_start();

	init_module_manager();

	init_packet_handler_list();

	load_modules(mod_p);

	start_client_manager();

	signal(SIGINT, sigint_handler);

	while (!sigint && !cm_fatal_error) {
		sleep(1);
	}

	if (sigint) print("\n");

	stop_client_manager();
	
	unload_modules();

	finit_module_manager();

	finit_packet_handler_list();

	print("\n--- motoko (%s) statistics ---\n", profile);

	ui_print_statistics();

	ui_stop();

	cleanup_settings();

	exit(cm_fatal_error ? EXIT_FAILURE : EXIT_SUCCESS);
}
