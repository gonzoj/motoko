/*
 * copyright (c) 2011 gonzoj
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
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include <module.h>

#include <d2gs.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>

#include <moduleman.h>

#include <util/net.h>
#include <util/list.h>
#include <util/system.h>
#include <util/string.h>
#include <util/file.h>

static struct setting module_settings[] = (struct setting []) {
	SETTING("CastDelay", INTEGER, 250)
};

static struct list module_settings_list = LIST(module_settings, struct setting, 1);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

typedef struct {
	word x;
	word y;
} point_t;

typedef struct {
	dword id;
	point_t location;
} object_t;

#define SQUARE(x) ((x) * (x))

#define DISTANCE(a, b) ((int) sqrt((double) (SQUARE((a).x - (b).x) + SQUARE((a).y - (b).y))))

static object_t wp;

static object_t bot;

int d2gs_char_location_update(void *);
int process_incoming_packet(void *);

_export const char * module_get_title() {
	return "meph";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "supposed to to mephisto runs";
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
			/*if (s->settings[i].type == STRING) {
				set->s_var = strdup(s->settings[i].s_var);
				if (set->s_var) {
					setting_cleanup_t sc = { cleanup_string_setting, set };
					list_add(&setting_cleaners, &sc);
				}
			}*/
		}
	}
	return TRUE;
}

_export bool module_init() {
	register_packet_handler(D2GS_RECEIVED, 0x51, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x59, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x5a, process_incoming_packet);

	register_packet_handler(D2GS_RECEIVED, 0x15, d2gs_char_location_update);
	register_packet_handler(D2GS_RECEIVED, 0x95, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x01, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x03, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x0c, d2gs_char_location_update);

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(D2GS_RECEIVED, 0x51, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x59, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x5a, process_incoming_packet);

	unregister_packet_handler(D2GS_RECEIVED, 0x15, d2gs_char_location_update);
	unregister_packet_handler(D2GS_RECEIVED, 0x95, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x01, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x03, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x0c, d2gs_char_location_update);

	return TRUE;
}

void waypoint(dword area) {
	d2gs_send(0x13, "02 00 00 00 %d", wp.id);

	msleep(300);

	d2gs_send(0x49, "%d %d", wp.id, area);

	/*if (merc.id) {
		msleep(300);

		d2gs_send(0x4b, "01 00 00 00 %d", merc.id);
	}*/

	msleep(500);
	
	plugin_print("meph", "took waypoint %02X\n", area);
}

void moveto(int x, int y) {
	point_t p = { x, y };

	plugin_print("meph", "moving to %i/%i\n", (word) p.x, (word) p.y);

	int t = DISTANCE(bot.location, p) * 80;

	d2gs_send(0x03, "%w %w", (word) p.x, (word) p.y);

	plugin_debug("pes", "sleeping for %ims\n", t);
	
	msleep(t > 3000 ? 3000 : t);
}

int d2gs_char_location_update(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch (packet->id) {

		case 0x15: { // received packet
			if (bot.id == net_get_data(packet->data, 1, dword)) {
				bot.location.x = net_get_data(packet->data, 5, word);
				bot.location.y = net_get_data(packet->data, 7, word);
			}
		}
		break;

		case 0x95: { // received packet
			bot.location.x = net_extract_bits(packet->data, 45, 15);
			bot.location.y = net_extract_bits(packet->data, 61, 15);
		}
		break;

		case 0x01:
		case 0x03: { // sent packets
			bot.location.x = net_get_data(packet->data, 0, word);
			bot.location.y = net_get_data(packet->data, 2, word);
		}
		break;

		/*case 0x0c: { // sent packet
			//if (cur_rskill == 0x36) {
				bot.location.x = net_get_data(packet->data, 0, word);
				bot.location.y = net_get_data(packet->data, 2, word);
			//}
		}*/
		break;

	}

	return FORWARD_PACKET;
}

int process_incoming_packet(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch(packet->id) {

	case 0x51: {
		if (net_get_data(packet->data, 0, byte) == 0x02 && net_get_data(packet->data, 5, word) == 0x00ed) {
			wp.id = net_get_data(packet->data, 1, dword);
			wp.location.x = net_get_data(packet->data, 7, word);
			wp.location.y = net_get_data(packet->data, 9, word);
		}
		break;
	}

	case 0x59: {
		word x = net_get_data(packet->data, 21, word);
		word y = net_get_data(packet->data, 23, word);
		if (!x && !y) {
			bot.id = net_get_data(packet->data, 0, dword);
		} else if (bot.id == net_get_data(packet->data, 0, dword)) {
			bot.location.x = x;
			bot.location.y = y;
		}
		break;
	}

	case 0x5a: { // auto-accept party
		if (net_get_data(packet->data, 0, byte) == 0x07) {
			if (net_get_data(packet->data, 1, byte) == 0x02) {
				dword id = net_get_data(packet->data, 2, dword);

				if (net_get_data(packet->data, 6, byte) == 0x05) {
					d2gs_send(0x5e, "08 %d", id);
				}
			}
		}
		break;
	}

	}

	return FORWARD_PACKET;
}

_export void * module_thread(void *arg) {
	time_t run_start;
	time(&run_start);
	moveto(5132, 5165);
	moveto(5133, 5145);
	moveto(5133, 5121);
	moveto(5131, 5095);
	moveto(5148, 5090);
	moveto(5149, 5067);
	moveto(5160, 5053);
	waypoint(0x65);
	sleep(1);
	extension("find_level_exit")->call("meph", "Durance of Hate Level 2", "Mephisto Down");
	sleep(1);
	d2gs_send(0x69, "");
	time_t cur;
	int runtime = (int) difftime(time(&cur), run_start);
	char *s_runtime = string_format_time(runtime);
	plugin_print("meph", "run took %s\n", s_runtime);
	free(s_runtime);
	pthread_exit(NULL);
}

_export void module_cleanup() {
	memset(&wp, 0, sizeof(object_t));
	memset(&bot, 0, sizeof(object_t));
}
