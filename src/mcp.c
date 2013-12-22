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
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "internal.h"
#include "bncs.h"
#include "settings.h"
#include "d2gs.h"
#include "mcp.h"
#include "moduleman.h"
#include "packet.h"
#include "gui.h"
#include <util/net.h>

static int mcp_socket;

static client_status_t mcp_client_status = CLIENT_DISCONNECTED;

/*static*/ bool mcp_engine_shutdown;

static pthread_mutex_t socket_m;

static bool socket_shutdown = FALSE;

mcp_character_t mcp_characters[8] = { { { 0 }, 0 } };

int mcp_character_index = 0;

static void mcp_dump_packet(mcp_packet_t packet) {
	byte buf[packet.len];

	net_set_data(buf, packet.len, 0, word);
	net_set_data(buf, packet.id, sizeof(word), byte);
	memcpy(buf + sizeof(word) + sizeof(byte), packet.data, packet.len
			- MCP_HEADER_SIZE);

	net_dump_data(buf, packet.len, UI_WHITE, ui_print);
}

static size_t mcp_send_packet(mcp_packet_t packet) {
	if (invoke_packet_handlers(MCP_SENT, &packet) == BLOCK_PACKET) {
		return packet.len; // pretend we sent it
	}

	if (setting("Verbose")->b_var) {
		ui_console_lock();

		print("[MCP] sent:\n\n");

		mcp_dump_packet(packet);

		print("\n");

		ui_console_unlock();
	}

	byte buf[packet.len];

	net_set_data(buf, packet.len, 0, word);
	net_set_data(buf, packet.id, sizeof(word), byte);
	memcpy(buf + sizeof(word) + sizeof(byte), packet.data, packet.len
			- MCP_HEADER_SIZE);

	size_t sent = net_send(mcp_socket, buf, packet.len);

	/*if (sent == packet.len) {
		invoke_packet_handlers(MCP_SENT, &packet);
	}*/

	return sent;
}

static size_t mcp_receive_packet(mcp_packet_t *packet) {
	byte buf[3]; // three bytes to obtain packet size + ID

	size_t len = net_receive(mcp_socket, buf, 3);

	if (len == 3) {
		packet->len = *(word *)buf;
		packet->id = net_get_data(buf, sizeof(word), byte);

		size_t data_len = packet->len - MCP_HEADER_SIZE;
		if (data_len) {
			packet->data = (byte *) malloc(data_len);
			len += net_receive(mcp_socket, packet->data, data_len);

			if (len != packet->len) {
				free(packet->data);
				return -1;
			}
		}

		if (setting("Verbose")->b_var) {
			ui_console_lock();

			print("[MCP] received:\n\n");

			mcp_dump_packet(*packet);

			print("\n");

			ui_console_unlock();
		}

		return packet->len;
	} else {
		return -1;
	}
}

static void mcp_create_packet(mcp_packet_t *packet, byte id, char *format,
		va_list args) {
	packet->id = id;
	packet->len = MCP_HEADER_SIZE;
	packet->len += net_build_data((void **) &packet->data, format, args);
}

_export size_t mcp_send(byte id, char *format, ...) {
	module_wait(MODULE_MCP);

	mcp_packet_t request = mcp_new_packet();

	va_list args;
	va_start(args, format);
	mcp_create_packet(&request, id, format, args);
	va_end(args);

	size_t sent = mcp_send_packet(request);

	if (request.data) {
		free(request.data);
	}

	if (is_module_thread() && (int) sent <= 0) {
		pthread_exit(NULL);
	}

	return sent;
}

_export client_status_t mcp_get_client_status() {
	return mcp_client_status;
}

static bool mcp_connect(mcp_con_info_t *info) {
	char addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &info->addr, addr, INET_ADDRSTRLEN);
	int port = ntohs(info->port);

	ui_console_lock();
	print("[MCP] connecting (%s:%i)... ", addr, port);

	if (!net_connect(addr, port, &mcp_socket)) {
		if (mcp_has_payload(&info->startup)) {
			free(info->startup.data);
		}
		free(info);

		error("failed\n");
		ui_console_unlock();

		return FALSE;
	}

	print("done\n");
	ui_console_unlock();

	mcp_client_status = CLIENT_CONNECTED;

	if (setting("Verbose")->b_var) {
		ui_console_lock();

		print("[MCP] sent:\n\n");

		net_dump_data((byte[]) {0x01}, 1, UI_WHITE, ui_print);

		print("\n");

		ui_console_unlock();
	}
	net_send(mcp_socket, (byte[]) {0x01}, 1);

	mcp_send_packet(info->startup);

	if (mcp_has_payload(&info->startup)) {
		free(info->startup.data);
	}
	free(info);

	return TRUE;
}

void mcp_shutdown() {
	mcp_engine_shutdown = TRUE;

	if (mcp_client_status == CLIENT_DISCONNECTED) {
		return;
	}

	pthread_mutex_lock(&socket_m);

	if (!socket_shutdown) {
		net_shutdown(mcp_socket);
		socket_shutdown = TRUE;

		print("[MCP] shutdown connection\n");
	}

	pthread_mutex_unlock(&socket_m);
}

static void mcp_disconnect() {
	net_disconnect(mcp_socket);

	print("[MCP] disconnected\n");

	mcp_client_status = CLIENT_DISCONNECTED;
}

static int mcp_on_char_logon(mcp_packet_t *p) {
	int i;
	for (i = 0; i < 8; i++) {
		if (!strncmp((char *) p->data, mcp_characters[i].name, 0x100)) {
			mcp_character_index = i;
			break;
		}
	}
	return FORWARD_PACKET;
}

static int on_d2gs_engine_connect(internal_packet_t *p) {
	if (*(int *)p->data == ENGINE_CONNECTED) {
		if (setting("MCPDisconnect")->b_var) {
			mcp_shutdown();
		}
	}
	return FORWARD_PACKET;
}

void * mcp_client_engine(mcp_con_info_t *info) {
	pthread_mutex_init(&socket_m, NULL);

	socket_shutdown = FALSE;

	internal_send(MCP_ENGINE_MESSAGE, "%d", ENGINE_STARTUP);

	if (!mcp_connect(info)) {

		internal_send(MCP_ENGINE_MESSAGE, "%d", ENGINE_SHUTDOWN);

		pthread_mutex_destroy(&socket_m);

		pthread_exit(NULL);
	}

	internal_send(MCP_ENGINE_MESSAGE, "%d", ENGINE_CONNECTED);

	// mcp_engine_shutdown = FALSE;

	register_packet_handler(INTERNAL, D2GS_CLIENT_ENGINE, (packet_handler_t) on_d2gs_engine_connect);
	register_packet_handler(MCP_SENT, 0x07, (packet_handler_t) mcp_on_char_logon);

	pthread_mutex_lock(&socket_m);

	while (!mcp_engine_shutdown) {
		mcp_packet_t incoming = mcp_new_packet();

		pthread_mutex_unlock(&socket_m);

		if ((int) mcp_receive_packet(&incoming) < 0) {

			print("[MCP] connection closed\n");

			pthread_mutex_lock(&socket_m);

			break;
		}

		pthread_mutex_lock(&socket_m);

		switch(incoming.id) {

		case 0x01: {
			dword status = net_get_data(incoming.data, 0, dword);
			if (status == 0x00) {

				internal_send(MCP_ENGINE_MESSAGE, "%d", MODULES_STARTUP);

				start_modules(MODULE_MCP);

			}
			break;
		}

		case 0x04: {
			dword status = net_get_data(incoming.data, 14, dword);
			if (status == 0x00) {
				dword gs_addr = net_get_data(incoming.data, 6, dword);
				dword gs_hash = net_get_data(incoming.data, 10, dword);
				word gs_token = net_get_data(incoming.data, 2, word);

				d2gs_con_info_t *info = malloc(sizeof(d2gs_con_info_t));
				*info = (d2gs_con_info_t) { gs_addr, gs_hash, gs_token };

				start_client_engine(D2GS_CLIENT_ENGINE, info);
			}
			break;
		}

		case 0x19: {
			word count = net_get_data(incoming.data, 6, word);
			int offset = 12;
			int i;
			for (i = 0; i < ((count < 8) ? count : 8); i++) {
				strncpy(mcp_characters[i].name, (char *) incoming.data + offset, 0x100);

				offset += strlen(mcp_characters[i].name) + 1;

				net_extract_data(incoming.data, &mcp_characters[i].class, offset + 13, sizeof(byte));

				offset += 34 + 4; /* 34 bytes of stats */
			}
			break;
		}

		}

		invoke_packet_handlers(MCP_RECEIVED, &incoming);

		if (incoming.data) {
			free(incoming.data);
		}
	}

	if (!socket_shutdown) {
		net_shutdown(mcp_socket);
		socket_shutdown = TRUE;
	}

	pthread_mutex_unlock(&socket_m);

	internal_send(MCP_ENGINE_MESSAGE, "%d", MODULES_CLEANUP);

	cleanup_modules(MODULE_MCP);

	clear_module_schedule(MODULE_MCP);

	mcp_disconnect();

	internal_send(MCP_ENGINE_MESSAGE, "%d", ENGINE_DISCONNECTED);

	unregister_packet_handler(MCP_SENT, 0x07, (packet_handler_t) mcp_on_char_logon);
	unregister_packet_handler(INTERNAL, D2GS_CLIENT_ENGINE, (packet_handler_t) on_d2gs_engine_connect);

	internal_send(MCP_ENGINE_MESSAGE, "%d", ENGINE_SHUTDOWN);

	pthread_mutex_destroy(&socket_m);

	pthread_exit(NULL);
}
