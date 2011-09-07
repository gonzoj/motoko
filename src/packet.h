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

#ifndef PACKET_HANDLER_H_
#define PACKET_HANDLER_H_

#include <util/types.h>

typedef enum {
	BNCS_RECEIVED = 0, BNCS_SENT = 1, MCP_RECEIVED = 2, MCP_SENT = 3, D2GS_RECEIVED = 4,
	D2GS_SENT = 5, INTERNAL = 6
} packet_t;

enum {
	BNCS_ENGINE_MESSAGE, MCP_ENGINE_MESSAGE, D2GS_ENGINE_MESSAGE, INTERNAL_FATAL_ERROR
};

enum {
	ENGINE_STARTUP, ENGINE_SHUTDOWN, ENGINE_CONNECTED, ENGINE_DISCONNECTED, MODULES_STARTUP, MODULES_CLEANUP
};

enum {
	FORWARD_PACKET, HIDE_PACKET, BLOCK_PACKET
};

typedef int (*packet_handler_t)(void *);

void init_packet_handler_list();

void finit_packet_handler_list();

// exported
void add_packet_handler(packet_t type, byte id, packet_handler_t handler);

// exported
void remove_packet_handler(packet_t type, byte id, packet_handler_t handler);

#define register_packet_handler(type, id, handler) add_packet_handler(type, id, handler)

#define unregister_packet_handler(type, id, handler) remove_packet_handler(type, id, handler)

int invoke_packet_handlers(packet_t, void *);

#endif /* PACKET_HANDLER_H_ */
