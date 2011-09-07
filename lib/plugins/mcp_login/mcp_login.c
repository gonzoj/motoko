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
#include <string.h>

#include <module.h>

#include <mcp.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>

#include <util/list.h>
#include <util/net.h>

static struct setting module_settings[] = (struct setting []) {
	SETTING("Character", .s_var = "", STRING),
};

static struct list module_settings_list = LIST(module_settings, struct setting, 1);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

typedef void (*setting_cleaner_t)(struct setting *);

typedef struct {
	setting_cleaner_t cleanup;
	struct setting *set;
} setting_cleanup_t;

struct list setting_cleaners = LIST(NULL, setting_cleanup_t, 0);

void cleanup_string_setting(struct setting *);

int mcp_startup_handler(void *p);
int mcp_charlist_handler(void *p);
int mcp_charlogon_handler(void *p);

_export const char * module_get_title() {
	return "mcp login";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "logs on to MCP";
}

_export int module_get_license() {
	return LICENSE_GPL_V3;
}

_export module_type_t module_get_type() {
	return (module_type_t) { MODULE_MCP, MODULE_PASSIVE };
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
	register_packet_handler(MCP_RECEIVED, 0x01, mcp_startup_handler);
	register_packet_handler(MCP_RECEIVED, 0x19, mcp_charlist_handler);
	register_packet_handler(MCP_RECEIVED, 0x07, mcp_charlogon_handler);

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(MCP_RECEIVED, 0x01, mcp_startup_handler);
	unregister_packet_handler(MCP_RECEIVED, 0x19, mcp_charlist_handler);
	unregister_packet_handler(MCP_RECEIVED, 0x07, mcp_charlogon_handler);

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

int mcp_startup_handler(void *p) {
	mcp_packet_t incoming = *MCP_CAST(p);

	dword status = net_get_data(incoming.data, 0, dword);

	switch (status) {

	case 0x00: {
		plugin_print("mcp login", "successfully logged on to realm\n");
		break;
	}

	case 0x7e: {
		plugin_error("mcp login", "error: CD key banned from this realm\n");

		return FORWARD_PACKET;
	}

	case 0x7f: {
		plugin_error("mcp login", "error: temporary IP ban (\"your connection has been temporarily restricted from this realm.\")\n");

		return FORWARD_PACKET;
	}

	default: {
		plugin_error("mcp login", "error: realm unavailable (%i)\n", status);

		return FORWARD_PACKET;
	}

	}

	plugin_print("mcp login", "request list of characters\n");

	mcp_send(0x19, "08 00 00 00");

	return FORWARD_PACKET;
}

int mcp_charlist_handler(void *p) {
	mcp_packet_t incoming = *MCP_CAST(p);

	word count = net_get_data(incoming.data, 6, word);
	
	ui_console_lock();

	plugin_print("mcp login", "list of characters (%i total):\n", count);

	int offset = 12;
	int i;

	for (i = 0; i < count; i++) {
		char name[64];
		net_extract_string(incoming.data, name, offset);
		
		print(" - %s\n", name);

		offset += strlen(name) + 1;

		/*
		 * get some other stats ...
		 */
		offset += 34 + 4; /* 34 bytes of stats */
	}

	ui_console_unlock();

	plugin_print("mcp login", "log on with character %s\n", module_setting("Character")->s_var);

	mcp_send(0x07, "%s 00", module_setting("Character")->s_var);

	return FORWARD_PACKET;
}

int mcp_charlogon_handler(void *p) {
	mcp_packet_t incoming = *MCP_CAST(p);

	dword status = net_get_data(incoming.data, 0, dword);

	switch (status) {

	case 0x00: {
		plugin_print("mcp login", "successfully logged on character\n");
		break;
	}

	case 0x46: {
		plugin_error("mcp login", "error: character not found\n");
		break;
	}

	case 0x7a: {
		plugin_error("mcp login", "error: login failed\n");
		break;
	}

	case 0x7b: {
		plugin_error("mcp login", "error: character expired\n");
		break;
	}

	}

	return FORWARD_PACKET;
}
