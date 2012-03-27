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

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "replay.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#ifndef NAME_MAX
#define NAME_MAX 256
#endif

unsigned long replay_get_tick_count() {
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
		return 0;
	} else {
		return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	}
}

replay_t * replay_load_file(const char *file, replay_t *replay) {
	memset(replay, 0, sizeof(replay_t));

	FILE *f = fopen(file, "rb");
	if (!f) return NULL;

	const char *c = strrchr(file, '/');
	if (c) c++;
	else c = file;

	strncpy(replay->file.name, c, 24);
	
	struct stat st;
	if (!stat(file, &st)) {
		replay->file.size = st.st_size;
	}

	//fseek(f, 56, SEEK_SET);
	
	fread(&replay->toon, 16, 1, f);
	fread(&replay->game, 16, 1, f);
	fread(&replay->time, 20, 1, f);
	fread(&replay->length, 4, 1, f);
	fread(&replay->count, 4, 1, f);

	int i;
	for (i = 0; i < replay->count; i++) {
		r_packet_t *new = (r_packet_t *) malloc(sizeof(r_packet_t));

		fread(&new->size, 4, 1, f);

		fread(&new->interval, 4, 1, f);

		new->data = (unsigned char *) malloc(new->size);
		fread(new->data, new->size, 1, f);

		new->next = NULL;

		if (replay->head == NULL) {
			replay->head = new;
		} else {
			r_packet_t *p;
			for (p = replay->head; p; p = p->next) {
				if (p->next == NULL) {
					p->next = new;
					break;
				}
			}
		}
	}

	replay->next = NULL;

	fclose(f);

	return  replay;
}

replay_t * replay_load_all(const char *dir, replay_t *replay, int *count) {
	DIR *d = opendir(dir);
	if (!d) return NULL;

	if (count) *count = 0;

	int first = 1;

	struct dirent *f;
	while ((f = readdir(d))) {
		if (strstr(f->d_name, ".gpl")) {
			if (first) {
				first = replay_load_file(f->d_name, replay) ? 0 : 1;
				if (count && !first) (*count)++;
				continue;
			}

			replay_t *r = (replay_t *) malloc(sizeof(replay_t));
			if (replay_load_file(f->d_name, r)) {
				replay_t *i;
				for (i = replay; i; i = i->next) {
					if (i->next == NULL) {
						i->next = r;
						if (count) (*count)++;
						break;
					}
				}
			} else {
				free(r);
			}
		}
	}

	closedir(d);

	//return first ? NULL : replay;
	return replay;
}

void replay_add_file(replay_t *replay, replay_t *new, int *count) {
	replay_t *ins = NULL;

	if (!(*count)) {
		memcpy(replay, new, sizeof(replay_t));

		ins = replay;
		ins->head = NULL;

		(*count)++;
	} else {
		replay_t *r;
		for (r = replay; r; r = r->next) {
			if (r->next == NULL) {
				replay_t *n = (replay_t *) malloc(sizeof(replay_t));
				memcpy(n, new, sizeof(replay_t));

				r->next = n;

				ins = n;
				ins->head = NULL;

				(*count)++;
				break;
			}
		}
	}

	if (!ins) return;

	r_packet_t *q = new->head;

	int i;
	for (i = 0; i < replay->count && q; i++) {
		r_packet_t *packet = (r_packet_t *) malloc(sizeof(r_packet_t));

		packet->size = q->size;

		packet->interval = q->interval;

		packet->data = (unsigned char *) malloc(packet->size);
		memcpy(packet->data, q->data, q->size);

		packet->next = NULL;

		if (ins->head == NULL) {
			ins->head = packet;
		} else {
			r_packet_t *p;
			for (p = ins->head; p; p = p->next) {
				if (p->next == NULL) {
					p->next = packet;
					break;
				}
			}
		}

		q = q->next;
	}
}

int replay_save_file(const char *path, replay_t *replay, char *output) {
	time_t t;
	t = time(NULL);
	struct tm *tm = gmtime(&t); // localtime() ?
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	char s_time[24] = "";
	sprintf(s_time, "%.2d%.2d%.2d%.2d%.2d%.2d%.3hd", tm->tm_mon + 1, tm->tm_mday, 1900 + tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec, (short) ((ts.tv_sec * 1000 + ts.tv_nsec / 1000000) % 1000)); // 22 bytes

	// remove trailing '/'
	char s_path[UNIX_PATH_MAX];
	strncpy(s_path, path, UNIX_PATH_MAX);
	char *eos = strrchr(s_path, '/');
	if (eos && eos[1] == '\0') *eos = '\0';

	char s_file[UNIX_PATH_MAX + NAME_MAX] = "";
	snprintf(s_file, UNIX_PATH_MAX + NAME_MAX, "%s/%s.gpl", s_path, s_time);

	FILE *f = fopen(s_file, "wb");

	if (output) strcpy(output, s_file);

	if (!f) return 1;

	fwrite(replay->toon, 16, 1, f);
	fwrite(replay->game, 16, 1, f);
	sprintf(s_time, "%.2d/%.2d/%.2d %.2d:%.2d:%.2d", tm->tm_mon + 1, tm->tm_mday, 1900 + tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec); // 20 byte
	fwrite(s_time, 20, 1, f);
	unsigned long length = replay_get_tick_count() - replay->tick;
	fwrite(&length, 4, 1, f);
	fwrite(&replay->count, 4, 1, f);

	r_packet_t *p;
	for (p = replay->head; p; p = p->next) {
		fwrite(&p->size, 4, 1, f);
		fwrite(&p->interval, 4, 1, f);
		fwrite(p->data, p->size, 1, f);
	}

	fclose(f);

	return 0;
}

replay_t * replay_new(replay_t *replay) {
	memset(replay, 0, sizeof(replay_t));
	return replay;
}

void replay_free(replay_t *replay) {
	replay_t *u, *v;
	int first = 1;

	for (u = replay; u; u = v) {
		v = u->next;

		r_packet_t *i, *j;

		for (i = u->head; i; i = j) {
			j = i->next;
			if (i->data) free(i->data);
			free(i);
		}

		if (first) {
			first = 0;
			continue;
		}

		free(u);
	}
}

void replay_add_packet(replay_t *replay, unsigned char *packet, unsigned long size) {
	if (!replay->start) replay->start = replay_get_tick_count();

	if (size > 3000) return;

	r_packet_t *new = (r_packet_t *) malloc(sizeof(r_packet_t));
	new->data = (unsigned char *) malloc(size);

	memcpy(new->data, packet, size);

	new->size = size;
	new->interval = replay->head ? replay_get_tick_count() - replay->start : 0;

	new->next = NULL;

	if (!replay->head) {
		replay->head = new;
	} else {
		r_packet_t *p;
		for (p = replay->head; p; p = p->next) {
			if (p->next == NULL) {
				p->next = new;
				break;
			}
		}
	}

	replay->count++;

	replay->start = replay_get_tick_count();
}
