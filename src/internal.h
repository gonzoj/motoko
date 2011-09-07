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

#ifndef INTERNAL_H_
#define INTERNAL_H_

#include <unistd.h>

#include "packet.h"

#include <util/types.h>

typedef struct {
	byte id;
	size_t len;
	byte *data;
} internal_packet_t;

#define internal_new_packet() (internal_packet_t) { .data = NULL }

#define INTERNAL_HEADER_SIZE (sizeof(byte) + sizeof(size_t))

#define internal_has_payload(p) ((p)->len > INTERNAL_HEADER_SIZE)

#define INTERNAL_CAST(p) ((internal_packet_t *) p)

#define internal_send_packet(p) invoke_packet_handlers(INTERNAL, &p)

// exported
size_t internal_send(byte, char *, ...);

#endif /* INTERNAL_H_ */
