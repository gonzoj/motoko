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

#ifndef GS_CLIENT_H_
#define GS_CLIENT_H_

#include <unistd.h>

#include "clientman.h"
#include <util/types.h>

extern bool d2gs_engine_shutdown;

typedef struct {
	byte id;
	size_t len;
	byte *data;
} d2gs_packet_t;

#define d2gs_new_packet() { .data = NULL }

#define D2GS_HEADER_SIZE (sizeof(byte))

#define d2gs_has_payload(p) ((p)->len > D2GS_HEADER_SIZE)

#define D2GS_CAST(p) ((d2gs_packet_t *) p)

// exported
size_t d2gs_send(byte, char *, ...);

void d2gs_send_raw(byte *, size_t);

// exported
client_status_t d2gs_get_client_status();

// exported
dword d2gs_get_hash();

void d2gs_shutdown();

typedef struct {
	dword addr;
	dword hash;
	word token;
} d2gs_con_info_t;

void * d2gs_client_engine(d2gs_con_info_t *);

#endif /* GS_CLIENT_H_ */
