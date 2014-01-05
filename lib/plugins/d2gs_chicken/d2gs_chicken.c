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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <util/compat.h>

#include <module.h>

#include <d2gs.h>
#include <internal.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>

#include <util/net.h>
#include <util/list.h>
#include <util/string.h>
#include <util/system.h>
#include <util/types.h>

#define PERCENT(a, b) ((a) != 0 ? (int) (((double) (b) / (double) (a)) * 100) : 0)

static struct setting module_settings[] = (struct setting []) {
	SETTING("HPPotionLimit", 0, INTEGER),
	SETTING("MPPotionLimit", 0, INTEGER),
	SETTING("HPChickenLimit", 0, INTEGER),
	SETTING("MPChickenLimit", 0, INTEGER),
	SETTING("ForceExitDelay", 0, INTEGER),
	SETTING("OnHostile", FALSE, BOOLEAN),
	SETTING("PingChickenLimit", 0, INTEGER)
};

static struct list module_settings_list = LIST(module_settings, struct setting, 7);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

word hp, mp;

int hp_pots_used, total_hp_pots_used, mp_pots_used, total_mp_pots_used, chicken, rip, n_games;

dword npc_id;

bool in_trade;

typedef struct {
	char code[4];
	dword id;
} potion_t;

enum {
	HEALTH_POTION, MANA_POTION
};

struct list belt;

potion_t npc_hp, npc_mp;

static pthread_mutex_t chicken_m;
static pthread_cond_t chicken_cv;

static bool exit_game;
static bool exit_ack;

static struct timespec ping_ts;

int d2gs_char_update(void *);
int d2gs_belt_update(void *);
int d2gs_shop_update(void *);
int d2gs_on_npc_interact(void *);
int d2gs_on_npc_quit(void *);
int d2gs_on_exit_ack(void *);
int internal_on_module_cleanup(internal_packet_t *);
int d2gs_on_event_message(void *);
int d2gs_on_ping(void *);

_export const char * module_get_title() {
	return "chicken";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "keeps your character alive";
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
			if (s->settings[i].type == STRING) {
				if (set->type == BOOLEAN) {
					set->b_var = !strcmp(string_to_lower_case(s->settings[i].s_var), "yes");
				} else if (set->type == INTEGER) {
					sscanf(s->settings[i].s_var, "%li",  &set->i_var);
				}
			}
		}
	}

	return TRUE;
}

_export bool module_init() {
	register_packet_handler(D2GS_RECEIVED, 0x95, d2gs_char_update);
	register_packet_handler(D2GS_RECEIVED, 0x9c, d2gs_belt_update);
	register_packet_handler(D2GS_RECEIVED, 0x9c, d2gs_shop_update);
	register_packet_handler(D2GS_SENT, 0x38, d2gs_on_npc_interact);
	register_packet_handler(D2GS_SENT, 0x30, d2gs_on_npc_quit);
	register_packet_handler(D2GS_RECEIVED, 0x06, d2gs_on_exit_ack);
	register_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, (packet_handler_t) internal_on_module_cleanup);
	register_packet_handler(D2GS_RECEIVED, 0x5a, d2gs_on_event_message);
	register_packet_handler(D2GS_SENT, 0x6d, d2gs_on_ping);
	register_packet_handler(D2GS_RECEIVED, 0x8f, d2gs_on_ping);

	belt = list_new(potion_t);

	in_trade = FALSE;
	npc_id = 0;
	npc_hp.id = 0;
	npc_mp.id = 0;
	hp_pots_used = 0;
	mp_pots_used = 0;
	total_hp_pots_used = 0;
	total_mp_pots_used = 0;
	chicken = 0;
	rip = 0;
	n_games = 0;

	pthread_mutex_init(&chicken_m, NULL);
	pthread_cond_init(&chicken_cv, NULL);

	exit_game = FALSE;
	exit_ack = FALSE;

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(D2GS_RECEIVED, 0x95, d2gs_char_update);
	unregister_packet_handler(D2GS_RECEIVED, 0x9c, d2gs_belt_update);
	unregister_packet_handler(D2GS_RECEIVED, 0x9c, d2gs_shop_update);
	unregister_packet_handler(D2GS_SENT, 0x38, d2gs_on_npc_interact);
	unregister_packet_handler(D2GS_SENT, 0x30, d2gs_on_npc_quit);
	unregister_packet_handler(D2GS_RECEIVED, 0x06, d2gs_on_exit_ack);
	unregister_packet_handler(INTERNAL, D2GS_ENGINE_MESSAGE, (packet_handler_t) internal_on_module_cleanup);
	unregister_packet_handler(D2GS_RECEIVED, 0x5a, d2gs_on_event_message);
	unregister_packet_handler(D2GS_SENT, 0x6d, d2gs_on_ping);
	unregister_packet_handler(D2GS_RECEIVED, 0x8f, d2gs_on_ping);

	list_clear(&belt);

	pthread_mutex_destroy(&chicken_m);
	pthread_cond_destroy(&chicken_cv);

	ui_add_statistics_plugin("chicken", "healing pots used: %i\n", total_hp_pots_used);
	ui_add_statistics_plugin("chicken", "mana pots used: %i\n", total_mp_pots_used);
	ui_add_statistics_plugin("chicken", "chicken: %i (%i%%)\n", chicken, PERCENT(n_games, chicken));
	ui_add_statistics_plugin("chicken", "RIP: %i (%i%%)\n", rip, PERCENT(n_games, rip));

	return TRUE;
}

_export void * module_thread(void *arg) {
	pthread_mutex_lock(&chicken_m);

	if (!exit_ack) {
		pthread_cond_wait(&chicken_cv, &chicken_m);
	}

	if (exit_game) {
		pthread_mutex_unlock(&chicken_m);
		msleep(module_setting("ForceExitDelay")->i_var);
		pthread_mutex_lock(&chicken_m);

		if (!exit_ack) {
			pthread_mutex_unlock(&chicken_m);

			plugin_print("chicken", "%i ms have passed without an exit acknowledgement\n", module_setting("ForceExitDelay")->i_var);
			plugin_print("chicken", "requesting forced disconnect\n");

			internal_send(INTERNAL_REQUEST, "%d", D2GS_ENGINE_SHUTDOWN);
		} else {
			pthread_mutex_unlock(&chicken_m);
		}
	} else {
		pthread_mutex_unlock(&chicken_m);
	}

	exit_game = FALSE;
	exit_ack = FALSE;

	pthread_exit(NULL);
}

int internal_on_module_cleanup(internal_packet_t *p) {
	if (*(int *)p->data == MODULES_CLEANUP) {
		pthread_mutex_lock(&chicken_m);

		exit_ack = TRUE;

		pthread_cond_signal(&chicken_cv);

		pthread_mutex_unlock(&chicken_m);
	}

	return FORWARD_PACKET;
}

_export void module_cleanup() {
	list_clear(&belt);

	in_trade = FALSE;
	npc_id = 0;

	n_games++;
}

void leave_game() {
	d2gs_send(0x69, "");

	chicken++;

	pthread_mutex_lock(&chicken_m);

	exit_game = TRUE;

	pthread_cond_signal(&chicken_cv);

	pthread_mutex_unlock(&chicken_m);
}

int compare_code(const void *pot, const void *code) {
	return (int) strstr(((potion_t *)pot)->code, (char *) code);
}

int compare_id(const void *pot, const void *id) {
	return ((potion_t *) pot)->id == *(dword *)id;
}

bool potion(int type) {
	char *code;

	switch (type) {

	case HEALTH_POTION:
		code = "hp";
		break;
		
	case MANA_POTION:
		code = "mp";
		break;

	default:
		return FALSE;

	}

	potion_t *pot = list_find(&belt, compare_code, (void *) code);
	if (pot) {
		d2gs_send(0x26, "%d 00 00 00 00 00 00 00 00", pot->id);
		return TRUE;
	}

	return FALSE;
}

int d2gs_char_update(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	if (net_extract_bits(packet->data, 45, 15)) {
		word updated_hp = net_extract_bits(packet->data, 0, 15);
		word updated_mp = net_extract_bits(packet->data, 15, 15);

		plugin_debug("chicken", "update hp: %i mp: %i\n", updated_hp, updated_mp);

		if (updated_hp == 0 && updated_hp < hp) {
			d2gs_send(0x69, "");

			plugin_print("chicken", "RIP\n");

			rip++;
		}
		else if (updated_hp < module_setting("HPChickenLimit")->i_var && updated_hp < hp) {
			leave_game();

			plugin_print("chicken", "chicken (%i hp)\n", updated_hp);
		}
		else if (updated_hp < module_setting("HPPotionLimit")->i_var  && updated_hp < hp) {
			if (!potion(HEALTH_POTION)) {
				leave_game();

				plugin_print("chicken", "no healing pot left (%i hp) - exiting game\n", updated_hp);
			} else {
				plugin_print("chicken", "use healing pot (%i hp)\n", updated_hp);
			}
		}
		else if (updated_mp < module_setting("MPChickenLimit")->i_var && updated_mp < mp) {
			leave_game();

			plugin_print("chicken", "chicken (%i mp)\n", updated_mp);
		}
		else if (updated_mp < module_setting("MPPotionLimit")->i_var && updated_mp < mp) {
			if (!potion(MANA_POTION)) {
				leave_game();

				plugin_print("chicken", "no mana pot left (%i mp) - exiting game\n", updated_mp);
			} else {
				plugin_print("chicken", "use mana pot (%i mp)\n", updated_mp);
			}
		}
		
		hp = updated_hp;
		mp = updated_mp;
	}

	return FORWARD_PACKET;
}

int d2gs_belt_update(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	byte action = net_extract_bits(packet->data, 0, 8);
	dword id = net_extract_bits(packet->data, 24, 32);

	if (action == 0x0e) {
		potion_t pot = { .code = { 0 } };
		*(dword *)pot.code = net_extract_bits(packet->data, 116, 24);
		pot.id = id;

		list_add(&belt, &pot);

		plugin_debug("chicken", "add pot %s (%08X) to belt\n", pot.code, id);
	} else if (action == 0x0f) {
		potion_t *pot = (potion_t *) list_find(&belt, compare_id, (void *) &id);
		if (pot) {
			plugin_debug("chicken", "remove pot %s (%08X) from belt\n", pot->code, pot->id);

			if (strstr(pot->code, "hp")) {
				hp_pots_used++;
				total_hp_pots_used++;
			} else if (strstr(pot->code, "mp")) {
				mp_pots_used++;
				total_mp_pots_used++;
			}
			plugin_debug("chicken", "hp pots %i mp pots %i\n", hp_pots_used, mp_pots_used);

			list_remove(&belt, pot);
		}
	}

	return FORWARD_PACKET;
}

int d2gs_shop_update(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	byte action = net_extract_bits(packet->data, 0, 8);
	dword id = net_extract_bits(packet->data, 24, 32);

	if (action == 0x0b) {
		char code[4] = { 0 };
		*(dword *)code = net_extract_bits(packet->data, 116, 24);

		if (strstr((char *) code, "hp")) {
			npc_hp.id = id;

			plugin_debug("chicken", "add pot %s (%08X) to shop\n", code, id);
		} else if (strstr((char *) code, "mp")) {
			npc_mp.id = id;

			plugin_debug("chicken", "add pot %s (%08X) to shop\n", code, id);
		}
	}

	return FORWARD_PACKET;
}

int d2gs_on_npc_interact(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	if (net_get_data(packet->data, 0, dword) == 0x01) {
		in_trade = TRUE;
		npc_id = net_get_data(packet->data, 4, dword);

		plugin_debug("chicken", "in trade with npc %08X\n", npc_id);
	}

	return FORWARD_PACKET;
}

void restock_potions() {
	plugin_debug("chicken", "restocking pots\n");

	int i;

	for (i = 0; i < hp_pots_used && npc_hp.id && npc_id; i++) {
		d2gs_send(0x32, "%d %d 00 00 00 00 64 00 00 00", npc_id, npc_hp.id);

		msleep(200);
	}

	if (i) {
		plugin_print("chicken",  "restocked %i healing pot(s)\n", i);

		hp_pots_used = 0;
	}

	for (i = 0; i < mp_pots_used && npc_mp.id && npc_id; i++) {
		d2gs_send(0x32, "%d %d 00 00 00 00 64 00 00 00", npc_id, npc_mp.id);

		msleep(200);
	}

	if (i) {
		plugin_print("chicken", "restocked %i mana pot(s)\n", i);

		mp_pots_used = 0;
	}
}

int d2gs_on_npc_quit(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	if (net_get_data(packet->data, 4, dword) == npc_id && in_trade) {

		restock_potions();

		in_trade = FALSE;

		npc_id = 0;

		npc_hp.id = 0;
		npc_mp.id = 0;
	}

	return FORWARD_PACKET;
}

int d2gs_on_exit_ack(void *p) {
	pthread_mutex_lock(&chicken_m);

	exit_ack = TRUE;

	pthread_mutex_unlock(&chicken_m);

	return FORWARD_PACKET;
}

int d2gs_on_event_message(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	if (net_get_data(packet->data, 0, byte) == 0x07 && net_get_data(packet->data, 1, byte) == 0x08) {
		plugin_print("chicken", "a player has gone hostile\n");

		if (module_setting("OnHostile")->b_var) {
			leave_game();

			plugin_print("chicken", "chicken (hostile)\n");
		}
	}

	return FORWARD_PACKET;
}

int d2gs_on_ping(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch (packet->id) {
		case 0x6d: {
			clock_gettime(CLOCK_REALTIME, &ping_ts);
			break;
		}
		case 0x8f: {
			struct timespec pong_ts;
			clock_gettime(CLOCK_REALTIME, &pong_ts);

			int latency = (pong_ts.tv_sec - ping_ts.tv_sec) * 1000 + (pong_ts.tv_nsec - ping_ts.tv_nsec) / 1000000;

			if (module_setting("PingChickenLimit")->i_var && (latency > module_setting("PingChickenLimit")->i_var)) {
				leave_game();

				plugin_print("chicken", "chicken (ping: %i ms)\n", latency);
			} else if (latency > 300) {
				plugin_print("chicken", "warning: high latency to game server (%i ms)\n", latency);
			}
			plugin_debug("chicken", "latency: %i ms\n", latency);
			break;
		}
	}

	return FORWARD_PACKET;
}
