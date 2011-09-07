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

#ifndef NET_H_
#define NET_H_

#include <netinet/in.h>
#include <stdarg.h>
#include <string.h>

#include <poll.h>

#include "util/types.h"

bool net_is_hostname(const char *);

// bool init_sockaddr_host(struct sockaddr_in *, const char *, int);

// bool init_sockaddr_addr(struct sockaddr_in *, const char *, int);

bool net_connect(const char *, int, int *);

bool net_shutdown(int);

bool net_disconnect(int);

void net_unblock(int);

void net_block(int);

int net_poll(int, int, int);

int net_select(int, int, int);

size_t net_send(int, void *, size_t);

size_t net_receive(int, void *, size_t);

#define net_extract_data(packet, data, offset, len) memcpy(data, packet + offset, len)

#define net_get_data(packet, offset, type) *(type *)&(packet[offset])

#define net_insert_data(packet, data, offset, len) memcpy(packet + offset, data, len)

#define net_set_data(packet, data, offset, type) *(type *)&(packet[offset]) = data

size_t net_extract_string(void *, char *, int);

size_t net_insert_string(void *, const char *, int);

#define BIT(data, offset) (((data)[(offset) / 8] >> ((offset) % 8)) & 0x1)

dword net_extract_bits(byte *, int, int);

//size_t net_build_data(void *, int, ...);
size_t net_build_data(void **, char *, va_list);

void net_dump_data(void *, size_t, int c, void (*print)(int, char *, ...));

#endif /* NET_H_ */
