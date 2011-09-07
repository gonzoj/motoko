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

#ifndef WARDENC_H_
#define WARDENC_H_

#include <unistd.h>

typedef unsigned int (*get_d2gs_hash_t)();
typedef void (*send_packet_t)(unsigned char *, size_t);
typedef unsigned char * (*parse_request_t)(unsigned char *, size_t, size_t *);
typedef void (*parse_response_t)(unsigned char *, size_t);
typedef void (*dump_packet_t)(unsigned char *, size_t);
typedef void * (*get_proc_address_t)(char *lib, char *proc);

typedef struct {
	// may not be NULL
	get_d2gs_hash_t get_d2gs_hash;
	// can be NULL if sniffing
	send_packet_t send_packet;
	// can be NULL (fallback to built-in)
	parse_request_t parse_request;
	// can be NULL (fallback to built-in)
	parse_response_t parse_response;
	// can be NULL if no output is desired
	dump_packet_t dump_packet;
	// can be NULL (fallback to built-in)
	get_proc_address_t get_proc_address;
} wardenc_callbacks;

/* NOTE: arguments won't get copied, don't pass NULL */
void wardenc_init(wardenc_callbacks *, char *);

void wardenc_engine(unsigned char *, size_t);

void wardenc_sniff_on_received(unsigned char *, size_t);

void wardenc_sniff_on_sent(unsigned char *, size_t);

#endif /* WARDENC_H_ */
