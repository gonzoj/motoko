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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "util/error.h"
#include "util/net.h"
#include "util/string.h"

bool net_is_hostname(const char *s) {
	int i;
	for (i = 0; i < strlen(s); i++) {
		if ((s[i] < '0' || s[i] > '9') && s[i] != '.') {
			return TRUE;
		}
	}
	return FALSE;
}

static bool init_sockaddr_host(struct sockaddr_in *sockaddr, const char *hostname, int port) {
	memset(sockaddr, 0, sizeof(struct sockaddr_in));
	sockaddr->sin_family = AF_INET;
	struct hostent *server = gethostbyname(hostname);
	if (server == NULL) {
		static char *e;
		string_new(&e, "failed to resolve hostname ", hostname, " (", strerror(errno), ")", "");
		err_set(e);
		free(e);
		return FALSE;
	}
	memcpy(&sockaddr->sin_addr.s_addr, server->h_addr, server->h_length);
	sockaddr->sin_port = htons(port);
	return TRUE;
}

static bool init_sockaddr_addr(struct sockaddr_in *sockaddr, const char *addr, int port) {
	memset(sockaddr, 0, sizeof(struct sockaddr_in));
	sockaddr->sin_family = AF_INET;
	sockaddr->sin_addr.s_addr = inet_addr(addr);
	sockaddr->sin_port = htons(port);
	return TRUE;
}

bool net_connect(const char *host, int port, int *sockfd) {
	char *e;

	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		string_new(&e, "failed to create socket (", strerror(errno), ")", "");
		err_set(e);
		free(e);
		return FALSE;
	}
	struct sockaddr_in addr;
	bool (*init_sockaddr)(struct sockaddr_in *, const char *, int);
	init_sockaddr = net_is_hostname(host) ? init_sockaddr_host : init_sockaddr_addr;
	if (!init_sockaddr(&addr, host, port)) {
		return FALSE;
	}
	if (connect(*sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
		char s_port[64];
		sprintf(s_port, "%i", port);
		string_new(&e, "failed to connect socket ", host, ":", s_port, " (", strerror(errno), ")", "");
		err_set(e);
		free(e);
		return FALSE;
	}
	return TRUE;
}

bool net_shutdown(int socket) {
	return shutdown(socket, SHUT_RDWR) ? FALSE : TRUE;
}

bool net_disconnect(int socket) {
	return close(socket) ? FALSE : TRUE;
}

void net_unblock(int socket) {
	int flags = fcntl(socket, F_GETFL);
	fcntl(socket, F_SETFL, (flags & ~O_NONBLOCK));
}

void net_block(int socket) {
	int flags = fcntl(socket, F_GETFL);
	fcntl(socket, F_SETFL, (flags | O_NONBLOCK));
}

int net_poll(int socket, int timeout, int event) {
	struct pollfd fds;
	fds.fd = socket;
	fds.events = event; // POLLIN | POLLOUT;
	return poll(&fds, 1, timeout);
}

int net_select(int socket, int timeout, int mode) {
	struct timeval tv = { 0, timeout * 1000 }; // micro- to milliseconds
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(socket, &fds);
	if (mode == POLLIN) {
		return select(socket + 1, &fds, NULL, NULL, &tv); // read
	}
	if (mode == POLLOUT) {
		return select(socket + 1, NULL, &fds, NULL, &tv); // write
	}
	return -1;
}

size_t net_send(int sockfd, void *data, size_t len) {
	size_t total;
	int tmp;
	for (total = 0; total < len; total += tmp) {
		tmp = send(sockfd, data + total, len - total, 0);
		if (tmp < 0) {
			char *e;
			string_new(&e, "failed to send data to socket (", strerror(errno), ")", "");
			err_set(e);
			free(e);
			return -1;
		}
		//printf("sent %i byte(s)\n", tmp);
	}
	return total;
}

size_t net_receive(int socket, void *data, size_t len) {
	size_t bytes = 0;
	bytes = recv(socket, data, len, 0);
	if (bytes < 0) {
		char *e;
		string_new(&e, "failed to receive data from socket (", strerror(errno), ")", "");
		err_set(e);
		free(e);
		return -1;
	}
	//printf("received %i byte(s)\n", bytes);
	return bytes;
}

size_t net_extract_string(void *data, char *string, int offset) {
	int i;
	for (i = offset; ((byte *) data)[i] != 0x00; i++) {
		string[i - offset] = ((byte *) data)[i];
	}
	string[i - offset] = '\0';
	return (size_t) i - offset;
}

size_t net_insert_string(void *data, const char *string, int offset) {
	int i;
	for (i = offset; i < offset + strlen(string); i++) {
		((byte *) data)[i] = string[i - offset];
	}
	return (size_t) i - offset;
}

dword net_extract_bits(byte *packet, int pos, int len) {
	dword bits = 0;
	int i;
	for (i = 0; i < len; i++) {
		bits |= BIT(packet, pos + i) << i;
    }
	return bits;
}

/*size_t net_build_data(void *data, int chunks, ...) {
	size_t copied = 0;
	va_list args;
	va_start(args, chunks);
	int i;
	for (i = 0; i < chunks; i++) {
		int offset = va_arg(args, int);
		char *chunk = va_arg(args, char *);
		char string[strlen(chunk) + 1];
		strcpy(string, chunk);
		byte bytes[strlen(chunk) / 2];
		size_t len = string_to_byte(string, bytes);
		memcpy(data + offset, bytes, len);
		copied += len;
	}
	va_end(args);
	return copied;
}*/

size_t net_build_data(void **data, char *format, va_list args) {
	char copy[strlen(format) + 1];
	strcpy(copy, format);
	size_t n_bytes = 0;
	int offset = 0;
	char *chunk = strtok(copy, " ");
	while (chunk != NULL) {
		byte *bytes;
		size_t len;
		if (strcmp(chunk, "%b") == 0) {
			byte b = va_arg(args, dword) & 0xff;
			bytes = &b;
			len = sizeof(byte);
		} else if (strcmp(chunk, "%w") == 0) {
			byte b[sizeof(word)];
			*(word *)&b = va_arg(args, dword) & 0xffff;
			bytes = b;
			len = sizeof(word);
		} else if (strcmp(chunk, "%d") == 0) {
			byte b[sizeof(dword)];
			*(dword *)&b = va_arg(args, dword);
			bytes = b;
			len = sizeof(dword);
		} else if (strcmp(chunk, "%s") == 0) {
			bytes = va_arg(args, byte *);
			len = strlen((char *) bytes);
		} else if (strcmp(chunk, "%h") == 0) {
			bytes = va_arg(args, byte *);
			len = sizeof(byte) * 20;
		} else {
			byte b;
			int tmp;
			//string_to_byte(chunk, &b);
			sscanf(chunk, "%x", &tmp);
			b = tmp & 0xff;
			bytes = &b;
			len = sizeof(byte);
		}
		n_bytes += len;
		*data = realloc(*data, n_bytes);
		memcpy(*data + offset, bytes, len);
		offset += len;
		chunk = strtok(NULL, " ");
	}
	return n_bytes;
}

static char byte_to_char(byte b) {
	return (b >= 127 || b < 32) ? '.' : (char) b;
}

void net_dump_data(void *packet, size_t len, int c, void (*print)(int, char *, ...)) {
	int i;
	for (i = 0; i < len; i += 8) {
		int end = (len - i < 8) ? len - i : 8;
		int j;
		for (j = i; j < i + end; j++) {
			print(c, "%02x ", ((byte *) packet)[j]);
		}
		for (j = 8; j > end; j--) {
			print(c, "   ");
		}
		print(c, "   ");
		for (j = i; j < i + end; j++) {
			print(c, "%c", byte_to_char(((byte *) packet)[j]));
		}
		print(c, "\n");
	}
}
