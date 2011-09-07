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

#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <module.h>

#include <d2gs.h>
#include <internal.h>
#include <moduleman.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>
#include <data/item_codes.h>

#include <util/net.h>
#include <util/list.h>
#include <util/string.h>
#include <util/system.h>
#include <util/types.h>

typedef void (*pthread_cleanup_handler_t)(void *);

#define PERCENT(a, b) ((a) != 0 ? (int) (((double) (b) / (double) (a)) * 100) : 0)

typedef struct {
	word x;
	word y;
} point_t;

#define SQUARE(x) ((x) * (x))

#define DISTANCE(a, b) ((int) sqrt((double) (SQUARE((a).x - (b).x) + SQUARE((a).y - (b).y))))

point_t location; // test pickit

typedef struct {
	byte action;
	byte destination;
	byte container;
	byte quality;
	char code[4];
	point_t location;
	byte level;
	dword id;
	bool ethereal;
	int distance; // test pickit
} item_t;

#define UNSPECIFIED 0xff

struct list items;
struct list valuable = LIST(NULL, item_t, 0); // must init here because load_config is called before init

pthread_mutex_t items_m;

bool routine_scheduled = FALSE;

/* statistics */
int n_attempts = 0;
int n_picked = 0;

char *qualities[] = {
	"other", // 0x00
	"", // 0x01
	"normal", // 0x02
	"superior", // 0x03
	"magic", // 0x04
	"set", // 0x05
	"rare", // 0x06
	"unique", // 0x07
};

void item_dump(item_t *i) {
	char *quality = i->quality == UNSPECIFIED ? "UNSPECIFIED" : qualities[i->quality];
	char *code = "UNSPECIFIED";
	if ((*i->code & UNSPECIFIED) != UNSPECIFIED) {
		code = i->code;
	}
	char level[] = "UNSPECIFIED";
	if (i->level != UNSPECIFIED) {
		sprintf(level, "%i", i->level);
	}
	char *ethereal = (i->ethereal & UNSPECIFIED) == UNSPECIFIED ? "UNSPECIFIED" : (i->ethereal ? "yes" : "no");

	ui_console_lock();

	plugin_print("pickit", "\n");
	plugin_print("pickit", "quality:     %s\n", quality);
	plugin_print("pickit", "code:        %s\n", code);
	plugin_print("pickit", "level:       %s\n", level);
	plugin_print("pickit", "ethereal:    %s\n", ethereal);
	plugin_print("pickit", "\n");

	ui_console_unlock();
}

const char * lookup_item(item_t *i) {
	int j;

	for (j = 0; j < n_item_codes; j++) {
		if (!strcmp(i->code, item_codes[j][1])) {
			return item_codes[j][0];
		}
	}

	return "";
}

void logitem(const char *format, ...) {
	static bool newline = TRUE;

	char *log;
	string_new(&log, profile, ".pickit.log", "");

	FILE *f = fopen(log, "a");

	free(log);

	if (f) {

		if (newline) {
			time_t ltime = time(NULL);
			char *date = asctime(localtime(&ltime));
			char *nl = strchr(date, '\n');
			if (nl) {
				*nl = '\0';
			}
			fprintf(f, "[%s] ", date);
			newline = FALSE;
		}

		if(strchr(format, '\n')) {
			newline = TRUE;
		}
		
		va_list args;
		va_start(args, format);

		vfprintf(f, format, args);

		va_end(args);

		fclose(f);

	}
}

int d2gs_item_action(void *);
int d2gs_char_location_update(void *);
//int internal_trigger_pickit(void *);
void pickit_routine();

_export const char * module_get_title() {
	return "pickit";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "picks up user specified items";
}

_export int module_get_license() {
	return LICENSE_GPL_V3;
}

_export module_type_t module_get_type() {
	return (module_type_t) { MODULE_D2GS, MODULE_PASSIVE };
}

_export bool module_load_config(struct setting_section *s) {
	if (strcmp(s->name, "Item")) {
		return TRUE;
	}

	item_t item;
	memset(&item, UNSPECIFIED, sizeof(item_t));

	int i;

	for (i = 0; i < s->entries; i++) {
		if (!strcmp(s->settings[i].name, "Code")) {
			strncpy(item.code, string_to_lower_case(s->settings[i].s_var), 4);
		}

		if (!strcmp(s->settings[i].name, "Quality")) {
			char *quality = string_to_lower_case(s->settings[i].s_var);

			if (!strcmp(quality, "set")) {
				item.quality = 0x05;
			}
			if (!strcmp(quality, "unique")) {
				item.quality = 0x07;
			}
			if (!strcmp(quality, "other")) {
				item.quality = 0x00;
			}
			if (!strcmp(quality, "normal")) {
				item.quality = 0x02;
			}
			if (!strcmp(quality, "magic")) {
				item.quality = 0x04;
			}
			if (!strcmp(quality, "rare")) {
				item.quality = 0x06;
			}
			if (!strcmp(quality, "superior")) {
				item.quality = 0x03;
			}
		}

		if (!strcmp(s->settings[i].name, "Ethereal")) {
			if (!strcmp(string_to_lower_case(s->settings[i].s_var), "yes")) {
				item.ethereal = TRUE;
			}
			if (!strcmp(string_to_lower_case(s->settings[i].s_var), "no")) {
				item.ethereal = FALSE;
			}
		}

		if (!strcmp(s->settings[i].name, "Level")) {
			sscanf(s->settings[i].s_var, "%c", &item.level);
		}
	}

	list_add(&valuable, &item);

	return TRUE;
}

_export bool module_init() {
	register_packet_handler(D2GS_RECEIVED, 0x9c, d2gs_item_action);

	register_packet_handler(D2GS_RECEIVED, 0x15, d2gs_char_location_update);
	register_packet_handler(D2GS_RECEIVED, 0x95, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x0c, d2gs_char_location_update);

	//register_packet_handler(INTERNAL, 0x9c, internal_trigger_pickit);

	items  = list_new(item_t);

	pthread_mutex_init(&items_m, NULL);

	logitem("  --- NEW SESSION ---\n");

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(D2GS_RECEIVED, 0x9c, d2gs_item_action);
	
	unregister_packet_handler(D2GS_RECEIVED, 0x15, d2gs_char_location_update);
	unregister_packet_handler(D2GS_RECEIVED, 0x95, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x0c, d2gs_char_location_update);

	//unregister_packet_handler(INTERNAL, 0x9c, internal_trigger_pickit);
	
	list_clear(&items);
	list_clear(&valuable);

	pthread_mutex_destroy(&items_m);

	ui_add_statistics_plugin("pickit", "attempts to pick items: %i\n", n_attempts);
	ui_add_statistics_plugin("pickit", "picked items: %i (%i%%)\n", n_picked, PERCENT(n_attempts, n_picked));

	return TRUE;
}

_export void * module_thread(void *arg) {
	return NULL;
}

_export void module_cleanup() {

	struct iterator it = list_iterator(&items);
	item_t *i;
	
	while ((i = iterator_next(&it))) {

		logitem("failed to pick ");
		if (i->ethereal) logitem("ethereal ");
		if (i->quality != 0x00) logitem("%s ", qualities[i->quality]);
		logitem("%s", lookup_item(i));
		if (i->quality != 0x00) logitem(" (%i)", i->level);
		if (setting("Debug")->b_var) logitem(" distance: %i", i->distance); // test pickit
		logitem("\n");

	}

	list_clear(&items);

	routine_scheduled = FALSE;
}

item_t * item_new(d2gs_packet_t *packet, item_t *new) {

	new->action = net_extract_bits(packet->data, 0, 8);

	new->id = net_extract_bits(packet->data, 24, 32);

	new->ethereal = net_extract_bits(packet->data, 78, 1);

	new->destination = net_extract_bits(packet->data, 98, 3);
	if (new->destination == 0x03) {
		new->location.x = net_extract_bits(packet->data, 101, 16);
		new->location.y = net_extract_bits(packet->data, 117, 16);
		new->container = UNSPECIFIED;

		memset(new->code, 0, 4);
		dword bits = net_extract_bits(packet->data, 133, 32);
		memcpy(new->code, &bits, 3);

	} else {
		new->location.x = net_extract_bits(packet->data, 105, 4);
		new->location.y = net_extract_bits(packet->data, 109, 3);
		new->container = net_extract_bits(packet->data, 112, 4);

		memset(new->code, 0, 4);
		dword bits = net_extract_bits(packet->data, 116, 32);
		memcpy(new->code, &bits, 3);

	}
	
	if (net_extract_bits(packet->data, 77, 1)) { 
		new->quality = 0x00;
	} else {
		if (strcmp(new->code, "gld")) {
			if (new->destination == 0x03) {
				new->quality = net_extract_bits(packet->data, 175, 4);
			} else {
				new->quality = net_extract_bits(packet->data, 158, 4);
			}
		}
	}
	
	if (strcmp(new->code, "gld")) { //new->level = net_extract_bits(packet->data, 168, 7);
		if (new->destination == 0x03) {
			new->level = net_extract_bits(packet->data, 168, 7);
		} else {
			new->level = net_extract_bits(packet->data, 151, 7);
		}
	}

	new->distance = DISTANCE(new->location, location);

	return new;
}

bool item_is_valuable(item_t *i, item_t *j) {
	if (j->quality != UNSPECIFIED && i->quality != j->quality) {
		return FALSE;
	}

	if ((*j->code & UNSPECIFIED) != UNSPECIFIED && strcmp(i->code, j->code)) {
		return FALSE;
	}

	if (j->level != UNSPECIFIED && i->level != j->level) {
		return FALSE;
	}

	if ((j->ethereal & UNSPECIFIED) != UNSPECIFIED && i->ethereal != j->ethereal) {
		return FALSE;
	}

	return TRUE;
}

bool item_evaluate(item_t *i) {
	struct iterator it = list_iterator(&valuable);
	item_t *j;

	while ((j = iterator_next(&it))) {
		if (item_is_valuable(i, j)) {

			pthread_mutex_lock(&items_m);

			// schedule pickit in case pickit without tele doesn't work
			if (!routine_scheduled) {
				schedule_module_routine(MODULE_D2GS, pickit_routine);

				routine_scheduled = TRUE;
			}

			plugin_print("pickit", "attempt to pick up %s\n", lookup_item(i));

			list_add(&items, i);

			pthread_mutex_unlock(&items_m);

			n_attempts++;

			return TRUE;
		}
	}

	return FALSE;
}

int item_compare(item_t *i, item_t *j) {
	return (i->id == j->id);
}

int d2gs_item_action(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	item_t i;
	item_new(packet, &i);

	if (i.action == 0x00) { // new item dropped to ground

		ui_console_lock();
		
		plugin_print("pickit", "%s%s%s%s ", i.ethereal ? "ethereal " : "", i.quality == 0x00 ? "" : qualities[i.quality], i.quality == 0x00 ? "" : " ", lookup_item(&i));
		if (i.quality != 0x00) print("(%i) ", i.level);
		print("dropped\n");

		if (item_evaluate(&i)) {
			ui_console_unlock();

			d2gs_send(0x16, "04 00 00 00 %d 00 00 00 00", i.id);
		} else {
			ui_console_unlock();
		}

	} else if (i.action == 0x04 && i.container == 0x02) { // item added to inventory

		item_t *j = list_find(&items, (comparator_t) item_compare, &i);
		
		if (j) {

			n_picked++;

			logitem("picked ");
			if (j->ethereal) logitem("ethereal ");
			if (j->quality != 0x00) logitem("%s ", qualities[j->quality]);
			logitem("%s", lookup_item(j));
			if (j->quality != 0x00) logitem(" (%i)", j->level);
			if (setting("Debug")->b_var) logitem(" distance: %i", j->distance); // test pickit
			logitem("\n");

			pthread_mutex_lock(&items_m);

			list_remove(&items, j);

			pthread_mutex_unlock(&items_m);

		}
	}

	return FORWARD_PACKET;
}

int d2gs_char_location_update(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch (packet->id) {

		case 0x15: {

		location.x = net_get_data(packet->data, 5, word);
		location.y = net_get_data(packet->data, 7, word);

		}
		break;

		case 0x95: {

			location.x = net_extract_bits(packet->data, 45, 15);
			location.y = net_extract_bits(packet->data, 61, 15);

		}
		break;

		case 0x0c: { // sent packet

			location.x = net_get_data(packet->data, 0, word);
			location.y = net_get_data(packet->data, 2, word);

		}
		break;

	}

	return FORWARD_PACKET;
}

/*int internal_trigger_pickit(void *p) {
	internal_packet_t *packet = INTERNAL_CAST(p);

	if (strcmp(packet->data, "pickit")) {
		return FORWARD_PACKET;
	}

	struct iterator it = list_iterator(&items);
	item_t *i;

	while ((i = iterator_next(&it))) {

		d2gs_send(0x0c, "%w %w", i->location.x, i->location.y);

		msleep(500);

		d2gs_send(0x16,"04 00 00 00 %d 00 00 00 00", i->id);

		msleep(250);

	}

	return FORWARD_PACKET;

}*/

void pickit_routine() {
	routine_scheduled = FALSE;

	plugin_print("pickit", "pickit routine started\n");

	d2gs_send(0x3c, "%w 00 00 ff ff ff ff", 0x36);
	msleep(300);

	pthread_mutex_lock(&items_m);
	pthread_cleanup_push((pthread_cleanup_handler_t) pthread_mutex_unlock, (void *) &items_m);

	struct iterator it = list_iterator(&items);
	item_t *i;

	while ((i = iterator_next(&it))) {

		plugin_print("pickit", "pick up %s\n", lookup_item(i));

		d2gs_send(0x0c, "%w %w", i->location.x, i->location.y);

		msleep(500);

		d2gs_send(0x16,"04 00 00 00 %d 00 00 00 00", i->id);

		msleep(250);

	}

	pthread_cleanup_pop(1);

}
