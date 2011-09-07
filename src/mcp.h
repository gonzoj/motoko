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

#ifndef MCP_CLIENT_H_
#define MCP_CLIENT_H_

#include <unistd.h>

#include "clientman.h"

#include <util/types.h>

extern bool mcp_engine_shutdown;

typedef struct {
	char name[0x100];
	byte class;
} mcp_character_t;

extern mcp_character_t mcp_characters[8];

extern int mcp_character_index;

typedef struct {
	word len;
	byte id;
	byte *data;
} mcp_packet_t;

#define mcp_new_packet() { .data = NULL }

#define MCP_HEADER_SIZE (sizeof(word) + sizeof(byte))

#define mcp_has_payload(p) ((p)->len > MCP_HEADER_SIZE)

#define MCP_CAST(p) ((mcp_packet_t *) p)

// exported
size_t mcp_send(byte, char *, ...);

// exported
client_status_t mcp_get_client_status();

void mcp_shutdown();

typedef struct {
	dword addr;
	dword port;
	mcp_packet_t startup;
} mcp_con_info_t;

void * mcp_client_engine(mcp_con_info_t *);

#endif /* MCP_CLIENT_H_ */
