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

#ifndef BNCS_CLIENT_H_
#define BNCS_CLIENT_H_

#include <unistd.h>

#include "clientman.h"
#include <util/types.h>

extern bool bncs_engine_shutdown;

typedef struct {
	byte id;
	word len;
	byte *data;
} bncs_packet_t;

#define bncs_new_packet() { .data = NULL }

#define BNCS_HEADER_SIZE (sizeof(byte) * 2 + sizeof(word))

#define bncs_has_payload(p) ((p)->len > BNCS_HEADER_SIZE)

#define BNCS_CAST(p) ((bncs_packet_t *) p)

// exported
size_t bncs_send(byte, char *, ...);

/* those should be in a separate file */

void bncs_request_channels(const char *);

void bncs_enter_chat(const char *, const char *);

void bncs_leave_chat();

void bncs_notify_create(int, const char *, const char *);

void bncs_notify_join(const char *, const char *, const char *);

void bncs_notify_leave();

/* ---------------------------------- */

// exported
client_status_t bncs_get_client_status();

// exported
dword bncs_get_server_token();

void bncs_shutdown();

void * bncs_client_engine(void *);

#endif /* BNCS_CLIENT_H_ */
