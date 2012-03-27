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
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#include <module.h>

#include <d2gs.h>
#include <mcp.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>

#include <util/net.h>
#include <util/list.h>
#include <util/string.h>
#include <util/types.h>

#include "replay/replay.h"

#define D2GS_COMPRESS_BUFFER_SIZE 0x6000

static replay_t replay;

pthread_mutex_t replay_m;

static dword player_id;
static word player_x;
static word player_y;
static byte cur_skill[2];

static bool replaydir_set = FALSE;

/* statistics */
static int n_records = 0;

static const byte on_send_id[] = {
	0x6b, 0x68, 0x06, 0x07, 0x09, 0x0a, 0x0d, 0x0e,
	0x10, 0x11, 0x05, 0x08, 0x0c, 0x0f, 0x01, 0x03,
	0x02, 0x04
};

static struct setting module_settings[] = (struct setting []) {
	SETTING("ReplayDir", .s_var = "replays", STRING)
};

static struct list module_settings_list = LIST(module_settings, struct setting, 1);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

int update_player_variables(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch (packet->id) {

		case 0x59: {
			if (!player_id) {
				player_id = net_get_data(packet->data, 0, dword);
				player_x = net_get_data(packet->data, 21, word);
				player_y = net_get_data(packet->data, 23, word);
			}
		}
		break;

		case 0x15: { // received packet
			player_x = net_get_data(packet->data, 5, word);
			player_y = net_get_data(packet->data, 7, word);
		}
		break;

		case 0x95: { // received packet
			player_x = net_extract_bits(packet->data, 45, 15);
			player_y = net_extract_bits(packet->data, 61, 15);
		}
		break;

		case 0x01:
		case 0x03: { // sent packets
			player_x = net_get_data(packet->data, 0, word);
			player_y = net_get_data(packet->data, 2, word);
		}
		break;

		case 0x0c: { // sent packet
			if (cur_skill[1] == 0x36) {
				player_x = net_get_data(packet->data, 0, word);
				player_y = net_get_data(packet->data, 2, word);
			}
		}
		break;

		case 0x3c: { // sent packet
			cur_skill[net_get_data(packet->data, 2, word) ? 0 : 1] = net_get_data(packet->data, 0, word);
			break;
		}

	}

	return FORWARD_PACKET;
}

int update_game_info(void *p) {
	mcp_packet_t *packet = MCP_CAST(p);

	strcpy(replay.game, (char *) &packet->data[2]);

	return FORWARD_PACKET;
}

int on_d2gs_receive(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch (packet->id) {
		case 0xae:
		case 0xaf:
		case 0x00:
		case 0x02:
		case 0x8f:
		case 0xb0:
		case 0x05:
		case 0x06:
		return FORWARD_PACKET;
	}

	byte new[packet->len];
	byte new_c[D2GS_COMPRESS_BUFFER_SIZE];

	new[0] = packet->id;
	memcpy(new + D2GS_HEADER_SIZE, packet->data, packet->len - D2GS_HEADER_SIZE);

	int size = d2gs_compress(new, packet->len, new_c + 2);
	int header = d2gs_create_packet_header(size, new_c);

	pthread_mutex_lock(&replay_m);
	replay_add_packet(&replay, new_c + (2 - header), size + header);
	pthread_mutex_unlock(&replay_m);

	return FORWARD_PACKET;
}

int on_d2gs_send(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch(packet->id) {
		
		case 0x68: {
			replay.tick = replay_get_tick_count();
			strcpy(replay.toon, (char *) &packet->data[20]);
			break;
		}

		case 0x06:
		case 0x07:
		case 0x09:
		case 0x0A:
		case 0x0D:
		case 0x0E:
		case 0x10:
		case 0x11: {
			// receive cast on unit
			word side;
			if (packet->id < 0x0b) side = 0; // left
			else side = 1; // right

			byte new[16] = { 0 };
			byte new_c[D2GS_COMPRESS_BUFFER_SIZE];
			
			new[0] = 0x4c;
			*(dword *)&new[2] = player_id;
			new[6] = cur_skill[side];
			new[8] = 0x02;
			new[9] = *(dword *)&packet->data[0];
			*(dword *)&new[10] = *(dword *)&packet->data[4];

			//int size = d2gs_compress(new_c + 2, 0x448, new, 16);
			int size = d2gs_compress(new, 16, new_c + 2);
			int header = d2gs_create_packet_header(size, new_c);

			pthread_mutex_lock(&replay_m);
			replay_add_packet(&replay, new_c + (2 - header), size + header);
			pthread_mutex_unlock(&replay_m);
			break;
		}

		case 0x05:
		case 0x08:
		case 0x0C:
		case 0x0F: {
			// reseive cast on pos
			word side;
			if (packet->id < 0x0b) side = 0;
			else side = 1;

			byte new[17] = { 0 };
			byte new_c[D2GS_COMPRESS_BUFFER_SIZE];

			new[0] = 0x4c;
			*(dword *)&new[2] = player_id;
			new[6] = cur_skill[side];
			new[10] = 0x02;
			*(word *)&new[11] = *(word *)&packet->data[0];
			*(word *)&new[13] = *(word *)&packet->data[2];

			//int size = d2gs_compress(new_c + 2, 0x448, new, 17);
			int size = d2gs_compress(new, 17, new_c + 2);
			int header = d2gs_create_packet_header(size, new_c);

			pthread_mutex_lock(&replay_m);
			replay_add_packet(&replay, new_c + (2 - header), size + header);
			pthread_mutex_unlock(&replay_m);
			break;
		}

		case 0x01:
		case 0x03: {
			// receive walk
			byte new[16] = { 0 };
			byte new_c[D2GS_COMPRESS_BUFFER_SIZE];

			new[0] = 0x0f;
			*(dword *)&new[2] = player_id;
			new[6] = packet->id == 0x03 ? 0x17 : 0x01;
			*(word *)&new[7] = *(word *)&packet->data[0];
			*(word *)&new[9] = *(word *)&packet->data[2];
			*(word *)&new[12] = player_x;
			*(word *)&new[14] = player_y;

			//int size = d2gs_compress(new_c + 2, 0x448, new, 16); // 2 byte header, we might wanna limit to 0x446 instead of 0x448
			int size = d2gs_compress(new, 16, new_c + 2);
			int header = d2gs_create_packet_header(size, new_c);

			pthread_mutex_lock(&replay_m);
			replay_add_packet(&replay, new_c + (2 - header), size + header);
			pthread_mutex_unlock(&replay_m);
			break;
		}

		case 0x02:
		case 0x04: {
			// receive walk to unit
			byte new[16] = { 0 };
			byte new_c[D2GS_COMPRESS_BUFFER_SIZE];

			new[0] = 0x10;
			*(dword *)&new[2] = player_id;
			new[6] = packet->id == 0x04 ? 0x18 : 0x00;
			new[7] = (byte) (*(dword *)&packet->data[0]);
			*(dword *)&new[8] = *(dword *)&packet->data[4];
			*(word *)&new[12] = player_x;
			*(word *)&new[14] = player_y;

			//int size = d2gs_compress(new_c + 2, 0x448, new, 16);
			int size = d2gs_compress(new, 16, new_c + 2);
			int header = d2gs_create_packet_header(size, new_c);

			pthread_mutex_lock(&replay_m);
			replay_add_packet(&replay, new_c + (2 - header), size + header);
			pthread_mutex_unlock(&replay_m);
			break;
		}

	}

	return FORWARD_PACKET;
}

_export const char * module_get_title() {
	return "record";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "records games to replay files";
}

_export int module_get_license() {
	return LICENSE_GPL_V3;
}

_export module_type_t module_get_type() {
	return (module_type_t) { MODULE_D2GS, MODULE_PASSIVE };
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
					replaydir_set = TRUE; // ;-)
				} else if (set->type == INTEGER) {
					sscanf(s->settings[i].s_var, "%li", &set->i_var);
				}
			}
		}
	}

	return TRUE;
}

_export bool module_init() {
	replay_new(&replay);

	register_packet_handler(D2GS_RECEIVED, 0x59, update_player_variables);
	register_packet_handler(D2GS_RECEIVED, 0x15, update_player_variables);
	register_packet_handler(D2GS_RECEIVED, 0x95, update_player_variables);
	register_packet_handler(D2GS_SENT, 0x01, update_player_variables);
	register_packet_handler(D2GS_SENT, 0x03, update_player_variables);
	register_packet_handler(D2GS_SENT, 0x3c, update_player_variables);

	register_packet_handler(MCP_SENT, 0x04, update_game_info);

	byte id;
	for (id = 0; id < 0xFF; id++) {
		register_packet_handler(D2GS_RECEIVED, id, on_d2gs_receive);
	}

	int i;
	for (i = 0; i < sizeof(on_send_id); i++) {
		register_packet_handler(D2GS_SENT, on_send_id[i], on_d2gs_send);
	}

	pthread_mutex_init(&replay_m, NULL);

	struct stat st;
	if (stat(module_setting("ReplayDir")->s_var, &st) && mkdir(module_setting("ReplayDir")->s_var, S_IRWXU)) {
		if (replaydir_set) {
			free(module_setting("ReplayDir")->s_var);
			replaydir_set = FALSE;
		}

		if (stat("replays", &st) && mkdir("replays", S_IRWXU)) {
			module_setting("ReplayDir")->s_var = ".";
		} else {
			module_setting("ReplayDir")->s_var = "replays";
		}
	}

	return TRUE;
}

_export bool module_finit() {
	if (replay.count > 0) replay_free(&replay);

	unregister_packet_handler(D2GS_RECEIVED, 0x59, update_player_variables);
	unregister_packet_handler(D2GS_RECEIVED, 0x15, update_player_variables);
	unregister_packet_handler(D2GS_RECEIVED, 0x95, update_player_variables);
	unregister_packet_handler(D2GS_SENT, 0x01, update_player_variables);
	unregister_packet_handler(D2GS_SENT, 0x03, update_player_variables);
	unregister_packet_handler(D2GS_SENT, 0x3c, update_player_variables);

	unregister_packet_handler(MCP_SENT, 0x04, update_game_info);

	byte id;
	for (id = 0; id < 0xFF; id++) {
		unregister_packet_handler(D2GS_RECEIVED, id, on_d2gs_receive);
	}

	int i;
	for (i = 0; i < sizeof(on_send_id); i++) {
		unregister_packet_handler(D2GS_SENT, on_send_id[i], on_d2gs_send);
	}

	pthread_mutex_destroy(&replay_m);

	ui_add_statistics_plugin("record", "recorded %i replay files to %s\n", n_records, module_setting("ReplayDir")->s_var);

	n_records = 0;

	if (replaydir_set) free(module_setting("ReplayDir")->s_var);

	return TRUE;
}

_export void * module_thread(void *arg) {
	return NULL;
}

_export void module_cleanup() {
	if (replay.count > 0) {
		char buf[512];
		if (!replay_save_file(module_setting("ReplayDir")->s_var, &replay, buf)) {

			plugin_print("record", "saving replay file for game %s\n", replay.game);

		} else {

			plugin_print("record", "failed to save replay file %s\n", buf);

		}
		replay_free(&replay);
		replay_new(&replay);
		n_records++;
	}

	player_id = 0;
}
