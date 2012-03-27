/*
 * Copyright (C) 2011 gonzoj
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

#ifndef REPLAY_H_
#define REPLAY_H_

typedef struct packet {
	unsigned long size;
	unsigned long interval;
	unsigned char *data;
	struct packet *next;
} r_packet_t;

typedef struct replay {
	struct {
		int size;
		char name[24];
	} file;
	char toon[16];
	char game[16];
	char time[20];
	unsigned long tick;
	unsigned long length;
	unsigned long count;
	unsigned long start;
	r_packet_t *head;
	struct replay *next;
} replay_t;

unsigned long replay_get_tick_count();

replay_t * replay_load_file(const char *, replay_t *);

replay_t * replay_load_all(const char *, replay_t *, int *);

void replay_add_file(replay_t *, replay_t *, int *);

int replay_save_file(const char *, replay_t *, char *);

replay_t * replay_new(replay_t *);

void replay_free(replay_t *);

void replay_add_packet(replay_t *, unsigned char *, unsigned long);

#endif /* REPLAY_H_ */
