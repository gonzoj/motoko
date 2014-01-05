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

#define _PLUGIN

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include <util/compat.h>

#include <module.h>

#include <mcp.h>
#include <d2gs.h>
#include <internal.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>

#include <util/config.h>
#include <util/list.h>
#include <util/net.h>
#include <util/string.h>

#define PERCENT(a, b) ((a) != 0 ? (int) (((double) (b) / (double) (a)) * 100) : 0)

typedef void (*pthread_cleanup_handler_t)(void *);

static struct setting module_settings[] = (struct setting []) {
	SETTING("Difficulty", .s_var = "", STRING),
	SETTING("LobbyIdleTime", 0, INTEGER),
	SETTING("GameNamePass", .s_var = "", STRING),
	SETTING("RotateCDKeys", FALSE, BOOLEAN),
	SETTING("RotateAfterRuns", 1, INTEGER),
	SETTING("GameLimitPerHour", 0, INTEGER),
	SETTING("JoinPublicGames", FALSE, BOOLEAN)
	//SETTING("KeySetFile", .s_var = "", STRING)
};

static struct list module_settings_list = LIST(module_settings, struct setting, 7);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

typedef void (*setting_cleaner_t)(struct setting *);

typedef struct {
	setting_cleaner_t cleanup;
	struct setting *set;
} setting_cleanup_t;

struct list setting_cleaners = LIST(NULL, setting_cleanup_t, 0);

void cleanup_string_setting(struct setting *);

static int request_id = 0x02;

static char game_name[16] = { 0 };
static char game_pass[16] = { 0 };
static int game_diff;

int game_count = 0;

enum {
	DIFF_HELL = 0x2000, DIFF_NIGHTMARE = 0x1000, DIFF_NORMAL = 0x0000
};

bool mcp_responsed = FALSE;
bool game_created = FALSE;
bool game_joined = FALSE;
bool char_logon = FALSE;
bool char_logon_signaled = FALSE;

bool mcp_cleanup = FALSE;

typedef struct keyset {
	char expansion[27];
	char classic[27];
} keyset_t;

#define keyset_new() (keyset_t) { 0 }

static struct keyset *keyset;
static int n_keyset;
static int i_keyset;

static bool rotated = FALSE;

static time_t start_t;
static int n_gph = 0;

typedef struct game {
	dword index;
	byte players;
	char name[15];
	char desc[100];
	bool joined;
	bool active;
} game_t;

static struct list public_games = LIST(NULL, game_t, 0);

static pthread_cond_t pub_cv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t pub_m = PTHREAD_MUTEX_INITIALIZER;

/* statistics */
int n_created = 0;
int n_joined = 0;
int ftj = 0;

pthread_cond_t game_created_cond_v = PTHREAD_COND_INITIALIZER;
pthread_cond_t game_joined_cond_v = PTHREAD_COND_INITIALIZER;
pthread_cond_t mcp_char_logon_cond_v = PTHREAD_COND_INITIALIZER;
pthread_cond_t d2gs_engine_shutdown_cond_v = PTHREAD_COND_INITIALIZER;

pthread_mutex_t game_created_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t game_joined_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mcp_char_logon_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t d2gs_engine_shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mcp_cleanup_m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t mcp_cleanup_cv = PTHREAD_COND_INITIALIZER;

int mcp_charlogon_handler(void *p);
int mcp_creategame_handler(void *p);
int mcp_joingame_handler(void *p);
int on_d2gs_shutdown(void *p);

int on_mcp_cleanup(void *p);

int mcp_gamelist_handler(void *p);

_export const char * module_get_title() {
	return "mcp game";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "creates and joins games";
}

_export int module_get_license() {
	return LICENSE_GPL_V3;
}

_export module_type_t module_get_type() {
	return (module_type_t) { MODULE_MCP, MODULE_ACTIVE };
}

_export bool module_load_config(struct setting_section *s) {
	int i;
	
	if (!strcmp(s->name, "KeySet")) {
		n_keyset++;
		keyset = realloc(keyset, n_keyset * sizeof(keyset_t));
		keyset[n_keyset - 1] = keyset_new();

		for (i = 0; i < s->entries; i++) {
			if (!strcmp(s->settings[i].name, "Expansion")) {
				strncpy(keyset[n_keyset - 1].expansion, s->settings[i].s_var, sizeof(((keyset_t *)0)->expansion));
			}
			if (!strcmp(s->settings[i].name, "Classic")) {
				strncpy(keyset[n_keyset - 1].classic, s->settings[i].s_var, sizeof(((keyset_t *)0)->classic));
			}
		}
	}

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
					if (set->s_var) {
						setting_cleanup_t sc = { cleanup_string_setting, set };
						list_add(&setting_cleaners, &sc);
					}
				} else if (set->type == INTEGER) {
					sscanf(s->settings[i].s_var, "%li", &set->i_var);
				}
			}
		}
	}

	return TRUE;
}

/*static void load_keyset_config(struct setting_section *s) {
	if (strcmp(s->name, "KeySet")) {
		return;
	}

	n_keyset++;
	keyset = realloc(keyset, n_keyset * sizeof(keyset_t));
	keyset[n_keyset - 1] = keyset_new();

	int i;
	for (i = 0; i < s->entries; i++) {
		if (!strcmp(s->settings[i].name, "Expansion")) {
			plugin_print("mcp game", "expansion: %s (%i)\n", s->settings[i].s_var, sizeof(((keyset_t *)0)->expansion));
			strncpy(keyset[n_keyset - 1].expansion, s->settings[i].s_var, sizeof(((keyset_t *)0)->expansion));
		}
		if (!strcmp(s->settings[i].name, "Classic")) {
			plugin_print("mcp game", "classic: %s\n", s->settings[i].s_var);
			strncpy(keyset[n_keyset - 1].classic, s->settings[i].s_var, sizeof(((keyset_t *)0)->classic));
		}
	}
}*/

_export bool module_init() {
	register_packet_handler(MCP_RECEIVED, 0x07, mcp_charlogon_handler);
	register_packet_handler(MCP_RECEIVED, 0x03, mcp_creategame_handler);
	register_packet_handler(MCP_RECEIVED, 0x04, mcp_joingame_handler);
	register_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, on_d2gs_shutdown);

	register_packet_handler(INTERNAL, MCP_ENGINE_MESSAGE, on_mcp_cleanup);

	register_packet_handler(MCP_RECEIVED, 0x05, mcp_gamelist_handler);

	if (!strcmp(string_to_lower_case(module_setting("Difficulty")->s_var), "hell")) {
		game_diff = DIFF_HELL;
	} else if (!strcmp(string_to_lower_case(module_setting("Difficulty")->s_var), "nightmare")) {
		game_diff = DIFF_NIGHTMARE;
	} else {
		game_diff = DIFF_NORMAL;
	}


	/*if (module_setting("RotateCDKeys")->b_var) {
		if (config_load_settings(module_setting("KeySetFile")->s_var, load_keyset_config)) {
			plugin_print("mcp game", "%i key sets from file %s loaded\n", n_keyset, module_setting("KeySetFile")->s_var);
		} else {
			plugin_error("mcp game", "failed to load key sets from file %s\n", module_setting("KeySetFile")->s_var);
		}
	}*/
	if (n_keyset) plugin_print("mcp game", "loaded %i key sets\n", n_keyset);

	start_t = time(NULL);

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(MCP_RECEIVED, 0x07, mcp_charlogon_handler);
	unregister_packet_handler(MCP_RECEIVED, 0x03, mcp_creategame_handler);
	unregister_packet_handler(MCP_RECEIVED, 0x04, mcp_joingame_handler);
	unregister_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, on_d2gs_shutdown);

	unregister_packet_handler(INTERNAL, MCP_ENGINE_MESSAGE, on_mcp_cleanup);

	unregister_packet_handler(MCP_RECEIVED, 0x05, mcp_gamelist_handler);

	struct iterator it = list_iterator(&setting_cleaners);
	setting_cleanup_t *sc;
	while ((sc = iterator_next(&it))) {
		sc->cleanup(sc->set);
	}

	list_clear(&setting_cleaners);

	if (keyset) {
		free(keyset);
		keyset = NULL;
		n_keyset = 0;
		i_keyset = 0;
		rotated = FALSE;
	}

	pthread_cond_destroy(&game_created_cond_v);
	pthread_cond_destroy(&game_joined_cond_v);
	pthread_cond_destroy(&mcp_char_logon_cond_v);
	pthread_cond_destroy(&d2gs_engine_shutdown_cond_v);

	pthread_mutex_destroy(&game_created_mutex);
	pthread_mutex_destroy(&game_joined_mutex);
	pthread_mutex_destroy(&mcp_char_logon_mutex);
	pthread_mutex_destroy(&d2gs_engine_shutdown_mutex);

	pthread_mutex_destroy(&mcp_cleanup_m);
	pthread_cond_destroy(&mcp_cleanup_cv);

	pthread_mutex_destroy(&pub_m);
	pthread_cond_destroy(&pub_cv);

	list_clear(&public_games);

	ui_add_statistics_plugin("mcp game", "games created: %i\n", n_created);
	if (module_setting("JoinPublicGames")->b_var) ui_add_statistics_plugin("mcp game", "public games joined: %i\n", n_joined);
	ui_add_statistics_plugin("mcp game", "failed to join: %i (%i%%)\n", ftj, PERCENT(n_created, ftj));

	return TRUE;
}

static void dump_game_list(struct list *l) {
	ui_console_lock();
	plugin_print("mcp game", "public games available (%i):\n\n", list_size(&public_games));

	game_t *g;
	struct iterator it = list_iterator(l);
	while ((g = iterator_next(&it))) {
		print("%s%s (%i player%s)%s%s\n", g->joined ? "* " : "", g->name, g->players, g->players != 1 ? "s" : "", strlen(g->desc) ? ": " : "", strlen(g->desc) ? g->desc : "");
	}
	if (list_empty(l)) print("(none)\n");
	print("\n");

	ui_console_unlock();
}

static void request_game_list(const char *s) {
	game_t *g;

	struct iterator it = list_iterator(&public_games);
	while ((g = iterator_next(&it))) {
		g->active = FALSE;
	}

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 2;

	pthread_mutex_lock(&pub_m);

	mcp_send(0x05, "%w 00 00 00 00 %s 00", request_id, s);
	request_id++;

	pthread_cond_timedwait(&pub_cv, &pub_m, &ts);

	it = list_iterator(&public_games);
	while ((g = iterator_next(&it))) {
		if (!g->active) {
			iterator_remove(&it);
			plugin_debug("mcp game", "removing inactive game\n");
		}
	}

	dump_game_list(&public_games);

	pthread_mutex_unlock(&pub_m);
}

int compare_game_index(game_t *a, game_t *b) {
	return a->index == b->index;
}

int compare_game_name(game_t *a, char *name) {
	return !strcmp(a->name, name);
}

int mcp_gamelist_handler(void *p) {
	mcp_packet_t *packet = MCP_CAST(p);

	game_t g;

	g.index = net_get_data(packet->data, 2, dword);
	g.players  = net_get_data(packet->data, 6, byte);
	net_extract_string(packet->data, g.name, 11);
	net_extract_string(packet->data, g.desc, 11 + strlen(g.name) + 1);

	pthread_mutex_lock(&pub_m);

	game_t *_g;
	if ((_g = list_find(&public_games, (comparator_t) compare_game_index, &g))) {
		_g->players = g.players;
		_g->active = TRUE;
		plugin_debug("mcp game", "refreshing game info\n");
	} else {
		g.joined = FALSE;
		g.active = TRUE;
		
		if (g.index) {
			list_add(&public_games, &g);

			plugin_debug("mcp game", "new game in game list\n");
		} else {
			plugin_debug("mcp game", "end of game list received\n");

			pthread_cond_signal(&pub_cv);
		}
	}

	pthread_mutex_unlock(&pub_m);

	return FORWARD_PACKET;
}

game_t * get_new_public_game() {
	game_t *g;

	struct iterator it = list_iterator(&public_games);
	while ((g = iterator_next(&it))) {
		if (!g->joined) return g;
	}

	return NULL;
}

static void switch_keys() {
	rotated = TRUE;

	if (n_keyset < 1) {
		plugin_print("mcp game", "cannot switch CD key set (no key sets loaded)\n");
		return;
	}

	i_keyset = (i_keyset + 1) % n_keyset;

	plugin_print("mcp game", "switching to CD key set %i\n", i_keyset + 1);

	strcpy(setting("ClassicKey")->s_var, keyset[i_keyset].classic);
	strcpy(setting("ExpansionKey")->s_var, keyset[i_keyset].expansion);
}

_export void * module_thread(void *arg) {

	pthread_mutex_lock(&mcp_char_logon_mutex);

	if (!char_logon_signaled) {
		pthread_cond_wait(&mcp_char_logon_cond_v, &mcp_char_logon_mutex);
	}

	pthread_mutex_unlock(&mcp_char_logon_mutex);

	while (char_logon) {
		if (module_setting("RotateCDKeys")->b_var && n_created && !(n_created % module_setting("RotateAfterRuns")->i_var) && !rotated) {
			switch_keys();

			plugin_print("mcp game", "requesting client restart\n");

			internal_send(INTERNAL_REQUEST, "%d", CLIENT_RESTART);

			pthread_exit(NULL);
		}

		rotated = FALSE;

		plugin_print("mcp game", "sleeping for %i seconds\n", module_setting("LobbyIdleTime")->i_var);

		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += module_setting("LobbyIdleTime")->i_var;

		pthread_mutex_lock(&mcp_cleanup_m);

		if (mcp_cleanup) {
			pthread_mutex_unlock(&mcp_cleanup_m);

			pthread_exit(NULL);
		}

		if (!pthread_cond_timedwait(&mcp_cleanup_cv, &mcp_cleanup_m, &ts)) {
			pthread_mutex_unlock(&mcp_cleanup_m);

			pthread_exit(NULL);
		}

		pthread_mutex_unlock(&mcp_cleanup_m);

		game_created = FALSE;
		game_joined = FALSE;

		if (module_setting("JoinPublicGames")->b_var) {
			request_game_list("");
		}

		if (module_setting("JoinPublicGames")->b_var) {
			pthread_mutex_lock(&pub_m);

			game_t *g = get_new_public_game();
			if (g) {
				strncpy(game_name, g->name, 15);
				strncpy(game_pass, "", 15);
				n_joined++;
			}

			pthread_mutex_unlock(&pub_m);
		} else {
		if (difftime(time(NULL), start_t) < 3600) {
			if (module_setting("GameLimitPerHour")->i_var > 0 && (n_gph > module_setting("GameLimitPerHour")->i_var)) {
				plugin_print("mcp game", "game limit per hour reached (%i)\n", module_setting("GameLimitPerHour")->i_var);
				continue;
			} else {
				n_gph++;
			}
		} else {
			start_t = time(NULL);
			n_gph = 0;
		}

		if (string_compare(module_setting("GameNamePass")->s_var, "random", FALSE) || !strlen(module_setting("GameNamePass")->s_var)) {
			string_random(15, 'a', 26, game_name);
			string_random(15, 'a', 26, game_pass);
		} else {
			if (game_count > 99) game_count = 0;
			game_count++;

			char *c_setting = strdup(module_setting("GameNamePass")->s_var);

			char *tok = strtok(c_setting, "/");	
			snprintf(game_name, 15, "%s-%s%i", tok ? tok : "", game_count < 10 ? "0" : "", game_count);

			tok = strtok(NULL, "/");
			strncpy(game_pass, tok ? tok : "", 15);

			free(c_setting);
		}

		while (!mcp_responsed) {
			pthread_mutex_lock(&game_created_mutex);
			pthread_cleanup_push((pthread_cleanup_handler_t) pthread_mutex_unlock, (void *) &game_created_mutex)

			mcp_send(0x03, "%w %d 01 ff 08 %s 00 %s 00 %s 00", request_id, game_diff, game_name, game_pass, "");

			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 2;

			pthread_cond_timedwait(&game_created_cond_v, &game_created_mutex, &ts);

			pthread_cleanup_pop(1);
		}

		request_id++;
		//mcp_responsed = FALSE;
		n_created++;

		plugin_print("mcp game", "created game %s / %s (%s)\n", game_name, game_pass, module_setting("Difficulty")->s_var);
		}

		mcp_responsed = FALSE;

		pthread_mutex_lock(&d2gs_engine_shutdown_mutex);
		pthread_cleanup_push((pthread_cleanup_handler_t) pthread_mutex_unlock, (void *) &d2gs_engine_shutdown_mutex);

		while (!mcp_responsed) {
			pthread_mutex_lock(&game_joined_mutex);
			pthread_cleanup_push((pthread_cleanup_handler_t) pthread_mutex_unlock, (void *) &game_joined_mutex)

			mcp_send(0x04, "03 00 %s 00 %s 00", game_name, game_pass);

			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 2;

			pthread_cond_timedwait(&game_joined_cond_v, &game_joined_mutex, &ts);

			pthread_cleanup_pop(1);
		}

		mcp_responsed = FALSE;

		if (!game_joined) {
			ftj++;
			goto retry;
		}

		plugin_print("mcp game", "joined game %s%s%s\n", game_name, strlen(game_pass) ? " / " : "", game_pass);

		pthread_cond_wait(&d2gs_engine_shutdown_cond_v, &d2gs_engine_shutdown_mutex);

		retry:
		pthread_cleanup_pop(1);
	}

	pthread_exit(NULL);
}

_export void module_cleanup() {
	mcp_responsed = FALSE;
	game_created = FALSE;
	game_joined = FALSE;
	char_logon_signaled = FALSE;

	mcp_cleanup = FALSE;

	request_id = 0x02;

	game_count = 0;
}

void cleanup_string_setting(struct setting *s) {
	free(s->s_var);
}

int on_d2gs_shutdown(void *p) {
	internal_packet_t ip = *INTERNAL_CAST(p);

	if (*(dword *)(ip.data) == ENGINE_SHUTDOWN) {
		pthread_mutex_lock(&d2gs_engine_shutdown_mutex);

		pthread_cond_signal(&d2gs_engine_shutdown_cond_v);

		pthread_mutex_unlock(&d2gs_engine_shutdown_mutex);
	}

	return FORWARD_PACKET;
}

int on_mcp_cleanup(void *p) {
	internal_packet_t ip = *INTERNAL_CAST(p);

	if (*(dword *)(ip.data) == MODULES_CLEANUP) {
		pthread_mutex_lock(&mcp_cleanup_m);

		mcp_cleanup = TRUE;

		pthread_cond_signal(&mcp_cleanup_cv);

		pthread_mutex_unlock(&mcp_cleanup_m);
	}

	return FORWARD_PACKET;
}

int mcp_charlogon_handler(void *p) {
	mcp_packet_t incoming = *MCP_CAST(p);

	pthread_mutex_lock(&mcp_char_logon_mutex);

	dword status = net_get_data(incoming.data, 0, dword);
	char_logon = status == 0x00 ? TRUE : FALSE;

	pthread_cond_signal(&mcp_char_logon_cond_v);

	char_logon_signaled = TRUE;

	pthread_mutex_unlock(&mcp_char_logon_mutex);

	return FORWARD_PACKET;
}

int mcp_creategame_handler(void *p) {
	mcp_packet_t incoming = *MCP_CAST(p);

	mcp_responsed = TRUE;

	dword status = net_get_data(incoming.data, 6, dword);
	
	switch (status) {

	case 0x00: {
		plugin_print("mcp game", "successfully created game\n");

		game_created = TRUE;
		break;
	}
	
	case 0x1e: {
		plugin_error("mcp game", "error: invalid game name\n");
		break;
	}

	case 0x1f: {
		plugin_error("mcp game", "error: game already exists\n");
		// remove this (only for testing purposes)
		//game_created = TRUE;
		break;
	}

	case 0x20: {
		plugin_error("mcp game", "error: game server is unavailable\n");
		break;
	}

	case 0x6e: {
		plugin_error("mcp game", "error: a dead hardcore character cannot create games\n");
		break;
	}

	}

	pthread_mutex_lock(&game_created_mutex);

	pthread_cond_signal(&game_created_cond_v);

	pthread_mutex_unlock(&game_created_mutex);

	return FORWARD_PACKET;
}

int mcp_joingame_handler(void *p) {
	mcp_packet_t incoming = *MCP_CAST(p);

	mcp_responsed = TRUE;

	dword status = net_get_data(incoming.data, 14, dword);

	switch (status) {

	case 0x00: {
		plugin_print("mcp game", "successfully joined game\n");

		game_joined = TRUE;

		game_t *g;
		if ((g = list_find(&public_games, (comparator_t) compare_game_name, game_name))) g->joined = TRUE;

		break;
	}

	case 0x29: {
		plugin_error("mcp game", "error: incorrect password\n");
		break;
	}

	case 0x2a: {
		plugin_error("mcp game", "error: game does not exist\n");
		break;
	}
	
	case 0x2b: {
		plugin_error("mcp game", "error: game is full\n");
		break;
	}

	case 0x2c: {
		plugin_error("mcp game", "error: the character does not meet the level requirements for this game\n");
		break;
	}

	case 0x6e: {
		plugin_error("mcp game", "error: a dead hardcore character cannot join a game\n");
		break;
	}

	case 0x71: {
		plugin_error("mcp game", "error: a non-hardcore character cannot join a game created by a hardcore character\n");
		break;
	}

	case 0x73: {
		plugin_error("mcp game", "error: unable to join a nightmare game\n");
		break;
	}

	case 0x74: {
		plugin_error("mcp game", "error: unable to join a hell game\n");
		break;
	}

	case 0x78: {
		plugin_error("mcp game", "error: a non-expansion character cannot join a game created by an expansion character\n");
		break;
	}

	case 0x79: {
		plugin_error("mcp game", "error: a expansion character cannot join a game created by a non-expansion character\n");
		break;
	}

	case 0x7d: {
		plugin_error("mcp game", "error: a non-ladder character cannot join a game created by a ladder character\n");
		break;
	}

	}

	pthread_mutex_lock(&game_joined_mutex);

	pthread_cond_signal(&game_joined_cond_v);

	pthread_mutex_unlock(&game_joined_mutex);

	return FORWARD_PACKET;
}
