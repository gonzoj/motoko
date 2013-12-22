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

// for PTHREAD_MUTEX_RECURSIVE
#define _GNU_SOURCE

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "packet.h"

#include "bncs.h"
#include "d2gs.h"
#include "internal.h"
#include "mcp.h"

#include <util/list.h>
#include <util/net.h>

static struct list packet_handlers[7][0xFF];

static pthread_mutex_t packet_handlers_m;

void init_packet_handler_list() {
	int i, j;
	for (i = 0; i < 0xFF; i++) {
		for (j = BNCS_RECEIVED; j <= INTERNAL; j++) {
			packet_handlers[j][i] = list_new(packet_handler_t);
		}
	}

	//pthread_mutex_init(&packet_handlers_m, NULL);
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&packet_handlers_m, &attr);
	pthread_mutexattr_destroy(&attr);
}

void finit_packet_handler_list() {
	int i, j;
	for (i = 0; i < 0xFF; i++) {
		for (j = BNCS_RECEIVED; j <= INTERNAL; j++) {
			list_clear(packet_handlers[j] + i);
		}
	}

	pthread_mutex_destroy(&packet_handlers_m);
}

_export void add_packet_handler(packet_t type, byte id, packet_handler_t handler) {
	pthread_mutex_lock(&packet_handlers_m);

	list_add(&packet_handlers[type][id], &handler);

	pthread_mutex_unlock(&packet_handlers_m);
}

static int compare_handler(packet_handler_t *a, packet_handler_t *b) {
	return (*a == *b);
}

_export void remove_packet_handler(packet_t type, byte id, packet_handler_t handler) {
	pthread_mutex_lock(&packet_handlers_m);

	void *h = list_find(&packet_handlers[type][id], (comparator_t) compare_handler, &handler);
	if (h) {
		list_remove(&packet_handlers[type][id], h);;
	}

	pthread_mutex_unlock(&packet_handlers_m);
}

int invoke_packet_handlers(packet_t type, void *packet) {
	int forward = FORWARD_PACKET;

	packet_handler_t *handler;
	byte id;

	switch (type) {

	case BNCS_RECEIVED:
	case BNCS_SENT:
		id = BNCS_CAST(packet)->id;
		break;

	case MCP_RECEIVED:
	case MCP_SENT:
		id = MCP_CAST(packet)->id;
		break;

	case D2GS_RECEIVED:
	case D2GS_SENT:
		id = D2GS_CAST(packet)->id;
		break;

	case INTERNAL:
		id = INTERNAL_CAST(packet)->id;
		break;

	default:
		return forward;

	}

	pthread_mutex_lock(&packet_handlers_m);

	struct iterator i = list_iterator(&packet_handlers[type][id]);
	while ((handler = iterator_next(&i))) {
		void *c;
		void *data = NULL;
		switch (type) {

		case BNCS_RECEIVED:
		case BNCS_SENT:
			c = calloc(1, sizeof(bncs_packet_t));
			BNCS_CAST(c)->id = id;
			BNCS_CAST(c)->len = BNCS_CAST(packet)->len;
			if (bncs_has_payload(BNCS_CAST(packet))) {
				BNCS_CAST(c)->data = malloc(BNCS_CAST(packet)->len);
				data = BNCS_CAST(c)->data;
				memcpy(BNCS_CAST(c)->data, BNCS_CAST(packet)->data, BNCS_CAST(packet)->len - BNCS_HEADER_SIZE);
			}
			break;

		case MCP_RECEIVED:
		case MCP_SENT:
			c = calloc(1, sizeof(mcp_packet_t));
			MCP_CAST(c)->id = id;
			MCP_CAST(c)->len = MCP_CAST(packet)->len;
			if (mcp_has_payload(MCP_CAST(packet))) {
				MCP_CAST(c)->data = malloc(MCP_CAST(packet)->len);
				data = MCP_CAST(c)->data;
				memcpy(MCP_CAST(c)->data, MCP_CAST(packet)->data, MCP_CAST(packet)->len - MCP_HEADER_SIZE);
			}
			break;

		case D2GS_RECEIVED:
		case D2GS_SENT:
			c = calloc(1, sizeof(d2gs_packet_t));
			D2GS_CAST(c)->id = id;
			D2GS_CAST(c)->len = D2GS_CAST(packet)->len;
			if (d2gs_has_payload(D2GS_CAST(packet))) {
				D2GS_CAST(c)->data = malloc(D2GS_CAST(packet)->len);
				data = D2GS_CAST(c)->data;
				memcpy(D2GS_CAST(c)->data, D2GS_CAST(packet)->data, D2GS_CAST(packet)->len - D2GS_HEADER_SIZE);
			}
			break;

		case INTERNAL:
			c = calloc(1, sizeof(internal_packet_t));
			INTERNAL_CAST(c)->id = id;
			INTERNAL_CAST(c)->len = INTERNAL_CAST(packet)->len;
			if (internal_has_payload(INTERNAL_CAST(packet))) {
				INTERNAL_CAST(c)->data = malloc(INTERNAL_CAST(packet)->len);
				data = INTERNAL_CAST(c)->data;
				memcpy(INTERNAL_CAST(c)->data, INTERNAL_CAST(packet)->data, INTERNAL_CAST(packet)->len - INTERNAL_HEADER_SIZE);
			}
			break;

		default:
			pthread_mutex_unlock(&packet_handlers_m);

			return forward;

		}

		forward = (*handler)((void *) c);
		free(c);
		if (data) {
			free(data);
		}
		if (forward == HIDE_PACKET || forward == BLOCK_PACKET) {
			iterator_destroy(&i);
			break;
		}
	}

	pthread_mutex_unlock(&packet_handlers_m);

	return forward;
}
