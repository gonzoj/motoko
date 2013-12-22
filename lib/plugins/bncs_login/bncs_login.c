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
#include <string.h>

#include <module.h>

#include <bncs.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>
#include <hash.h>

#include <util/net.h>
#include <util/system.h>

static struct setting module_settings[] = (struct setting []) {
	SETTING("Account", .s_var = "", STRING),
	SETTING("Password", .s_var = "", STRING),
	SETTING("Realm", .s_var = "", STRING)
};

static struct list module_settings_list = LIST(module_settings, struct setting, 3);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

typedef void (*setting_cleaner_t)(struct setting *);

typedef struct {
	setting_cleaner_t cleanup;
	struct setting *set;
} setting_cleanup_t;

struct list setting_cleaners = LIST(NULL, setting_cleanup_t, 0);

void cleanup_string_setting(struct setting *);

int bncs_auth_check_handler(void *p);
int bncs_getfiletime_handler(void *p);
int bncs_logonresponse_handler(void *p);
int bncs_queryrealms_handler(void *p);
int bncs_logonrealmex_handler(void *p);

_export const char * module_get_title() {
	return "bncs login";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "logs on to battle.net and establishes connection to MCP";
}

_export int module_get_license() {
	return LICENSE_GPL_V3;
}

_export module_type_t module_get_type() {
	return (module_type_t) { MODULE_BNCS, MODULE_PASSIVE };
}

_export bool module_load_config(struct setting_section *s) {
	int i;
	for (i = 0; i < s->entries; i++) {
		struct setting *set = module_setting(s->settings[i].name);
		if (set) {
			if (s->settings[i].type == STRING) {
				set->s_var = strdup(s->settings[i].s_var);
				if (set->s_var) {
					setting_cleanup_t sc = { cleanup_string_setting, set };
					list_add(&setting_cleaners, &sc);
				}
			}
		}
	}
	return TRUE;
}

_export bool module_init() {
	register_packet_handler(BNCS_RECEIVED, 0x51, bncs_auth_check_handler);
	register_packet_handler(BNCS_RECEIVED, 0x33, bncs_getfiletime_handler);
	register_packet_handler(BNCS_RECEIVED, 0x3a, bncs_logonresponse_handler);
	register_packet_handler(BNCS_RECEIVED, 0x40, bncs_queryrealms_handler);
	register_packet_handler(BNCS_RECEIVED, 0x3e, bncs_logonrealmex_handler);

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(BNCS_RECEIVED, 0x51, bncs_auth_check_handler);
	unregister_packet_handler(BNCS_RECEIVED, 0x33, bncs_getfiletime_handler);
	unregister_packet_handler(BNCS_RECEIVED, 0x3a, bncs_logonresponse_handler);
	unregister_packet_handler(BNCS_RECEIVED, 0x40, bncs_queryrealms_handler);
	unregister_packet_handler(BNCS_RECEIVED, 0x3e, bncs_logonrealmex_handler);
	
	struct iterator it = list_iterator(&setting_cleaners);
	setting_cleanup_t *sc;

	while ((sc = iterator_next(&it))) {
		sc->cleanup(sc->set);
	}

	list_clear(&setting_cleaners);

	return TRUE;
}

_export void * module_thread(void *arg) {
	return NULL;
}

_export void module_cleanup() {
	return;
}

void cleanup_string_setting(struct setting *s) {
	free(s->s_var);
}

int bncs_auth_check_handler(void *p) {
	bncs_packet_t incoming = *BNCS_CAST(p);

	dword response = net_get_data(incoming.data, 0, dword);

	switch (response & ~0x010) {

	case 0x000: {
		plugin_print("bncs login", "successfully logged on to battle.net\n");
		break;
	}

	case 0x100: {
		char info[512];
		net_extract_string(incoming.data, info, 4);

		plugin_error("bncs login", "error: outdated game version (%s)\n", info);

		return FORWARD_PACKET;
	}

	case 0x101: {
		plugin_error("bncs login", "error: invalid version\n");

		return FORWARD_PACKET;
	}

	case 0x102: {
		char info[512];
		net_extract_string(incoming.data, info, 4);
		
		plugin_error("bncs login", "error: game version must be downgraded (%s)\n", info);

		return FORWARD_PACKET;
	}

	case 0x200: {
		plugin_error("bncs login", "error: invalid CD key\n");

		return FORWARD_PACKET;
	}

	case 0x201: {
		char info[512];
		net_extract_string(incoming.data, info, 4);

		plugin_error("bncs login", "error: CD key already in use by %s\n", info);

		plugin_print("bncs login", "requesting client restart\n");

		internal_send(INTERNAL_REQUEST, "%d", CLIENT_RESTART);

		return FORWARD_PACKET;
	}

	case 0x202: {
		plugin_error("bncs login", "error: CD key disabled\n");

		return FORWARD_PACKET;
	}

	case 0x203: {
		plugin_error("bncs login", "error: invalid product\n");

		return FORWARD_PACKET;
	}

	default: {
		plugin_error("bncs login", "error: failed to log on to battle.net (%i)\n", response);

		return FORWARD_PACKET;
	}

	}

	plugin_print("bncs login", "request bnserver-D2DV.ini file time\n");

	bncs_send(0x33, "04 00 00 80 00 00 00 00 %s 00", "bnserver-D2DV.ini");
	return FORWARD_PACKET;
}

int bncs_getfiletime_handler(void *p) {
	dword client_token = (dword) system_get_clock_ticks();

	byte pass_hash[20];	
	hash_passwd(module_setting("Password")->s_var, client_token, bncs_get_server_token(), pass_hash);

	plugin_print("bncs login", "log on to account %s\n", module_setting("Account")->s_var);

	bncs_send(0x3a, "%d %d %h %s 00", client_token, bncs_get_server_token(), pass_hash, module_setting("Account")->s_var);

	return FORWARD_PACKET;
}

int bncs_logonresponse_handler(void *p) {
	bncs_packet_t incoming = *BNCS_CAST(p);

	dword response = net_get_data(incoming.data, 0, dword);

	switch (response) {

	case 0x00: {
		plugin_print("bncs login", "successfully logged on to account\n");
		plugin_print("bncs login", "request realm listing\n");

		bncs_send(0x40, "");
		break;
	}

	case 0x01: {
		plugin_error("bncs login", "error: account does not exist\n");
		break;
	}

	case 0x02: {
		plugin_error("bncs login", "error: invalid password\n");
		break;
	}

	case 0x06: {
		plugin_error("bncs login", "error: account closed\n");
		break;
	}

	default: {
		plugin_error("bncs login", "error: failed to log on to account (%i)\n", response);
		break;
	}

	}

	return FORWARD_PACKET;
}

int bncs_queryrealms_handler(void *p) {
	bncs_packet_t incoming = *BNCS_CAST(p);

	ui_console_lock();

	plugin_print("bncs login", "realm list:\n");

	dword n = net_get_data(incoming.data, 4, dword);
	int offset = 8;
	int i;

	for (i = 0; i < n; i++) {
		offset += 4;

		char realm_title[64], realm_description[512];

		net_extract_string(incoming.data, realm_title, offset);

		offset += strlen(realm_title) + 1;

		net_extract_string(incoming.data, realm_description, offset);

		offset += strlen(realm_description) + 1;

		print(" - %s: %s\n", realm_title, realm_description);
	}

	ui_console_unlock();

	dword client_token = 1;

	byte pass_hash[20];
	hash_passwd("password", client_token, bncs_get_server_token(), pass_hash);

	plugin_print("bncs login", "log on to realm %s:\n", module_setting("Realm")->s_var);

	bncs_send(0x3e, "%d %h %s 00", client_token, pass_hash, module_setting("Realm")->s_var);

	return FORWARD_PACKET;
}

int bncs_logonrealmex_handler(void *p) {
	bncs_packet_t incoming = *BNCS_CAST(p);

	if (incoming.len <= 12) {
		plugin_error("bncs login", "error: failed to log on realm\n");
	}

	return FORWARD_PACKET;
}
