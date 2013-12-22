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
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"
#include "bncs.h"
#include "settings.h"
#include "mcp.h"
#include "moduleman.h"
#include "packet.h"
#include "gui.h"
#include "hash.h"
#include <util/file.h>
#include <util/net.h>
#include <util/system.h>
#include <util/string.h>

static int bncs_socket;

static client_status_t bncs_client_status = CLIENT_DISCONNECTED;

/*static*/ bool bncs_engine_shutdown;

static dword bncs_server_token;

static pthread_mutex_t socket_m;

static bool socket_shutdown = FALSE;

static char bncs_auth_info_format[] = "00 00 00 00 36 38 58 49 50 58 32 44 %b 00 00 00 53 55 6e 65 c0 a8 02 8e 00 00 00 00 09 04 00 00 09 04 00 00 55 53 41 00 55 6e 69 74 65 64 20 53 74 61 74 65 73 00";

static void bncs_dump_packet(bncs_packet_t packet) {
	byte buf[packet.len];

	buf[0] = 0xff;
	buf[1] = packet.id;
	*(word *) &buf[2] = packet.len;
	memcpy(buf + BNCS_HEADER_SIZE, packet.data, packet.len - BNCS_HEADER_SIZE);

	net_dump_data(buf, packet.len, UI_WHITE, ui_print);
}

static size_t bncs_send_packet(bncs_packet_t packet) {
	if (invoke_packet_handlers(BNCS_SENT, &packet) == BLOCK_PACKET) {
		return packet.len; // pretend we sent it
	}

	if (setting("Verbose")->b_var) {
		ui_console_lock();

		print("[BNCS] sent:\n\n");

		bncs_dump_packet(packet);

		print("\n");

		ui_console_unlock();
	}

	byte buf[packet.len];

	buf[0] = 0xff;
	buf[1] = packet.id;
	*(word *) &buf[2] = packet.len;
	memcpy(buf + BNCS_HEADER_SIZE, packet.data, packet.len - BNCS_HEADER_SIZE);

	size_t sent = net_send(bncs_socket, buf, packet.len);

	/*if (sent == packet.len) {
		invoke_packet_handlers(BNCS_SENT, &packet);
	}*/

	return sent;
}

static size_t bncs_receive_packet(bncs_packet_t *packet) {
	byte buf[4]; // four bytes to obtain packet size

	size_t len = net_receive(bncs_socket, buf, 4);

	if (len == 4) {
		packet->len = *(word *)&buf[2];
		packet->id = buf[1];

		size_t data_len = packet->len - BNCS_HEADER_SIZE;
		if (data_len) {
			packet->data = (byte *) malloc(data_len);
			len += net_receive(bncs_socket, packet->data, data_len);

			if (len != packet->len) {
				free(packet->data);
				return -1;
			}
		}

		if (setting("Verbose")->b_var) {
			ui_console_lock();

			print("[BNCS] received:\n\n");

			bncs_dump_packet(*packet);

			print("\n");

			ui_console_unlock();
		}

		return packet->len;
	} else {
		return -1;
	}
}

static void bncs_create_packet(bncs_packet_t *packet, byte id, char *format,
		va_list args) {
	packet->id = id;
	packet->len = BNCS_HEADER_SIZE;
	packet->len += net_build_data((void **) &packet->data, format, args);
}

_export size_t bncs_send(byte id, char *format, ...) {
	module_wait(MODULE_BNCS);

	bncs_packet_t request = bncs_new_packet();

	va_list args;
	va_start(args, format);
	bncs_create_packet(&request, id, format, args);
	va_end(args);

	size_t sent = bncs_send_packet(request);

	if (request.data) {
		free(request.data);
	}

	if (is_module_thread() && (int) sent <= 0) {
		pthread_exit(NULL);
	}

	return sent;
}

size_t bnftp_download(dword *mpq_filetime, char *mpq_file) {
	int bnftp_socket;

	ui_console_lock();
	print("[BNFTP] connecting... ");

	if (!net_connect(setting("Hostname")->s_var, setting("BNCSPort")->i_var, &bnftp_socket)) {

		error("failed\n");
		ui_console_unlock();

		return 0;
	}

	print("done\n");
	ui_console_unlock();

	if (setting("Verbose")->b_var) {
		ui_console_lock();

		print("[BNFTP] sent:\n\n");

		net_dump_data((byte[]) {0x02}, 1, UI_WHITE, ui_print);

		print("\n");

		ui_console_unlock();
	}

	net_send(bnftp_socket, (byte[]) {0x02}, 1);

	print("[BNFTP] request MPQ file\n");

	byte packet[47];
	memset(packet, 0, 47);
	string_to_byte(
			"2f 00 00 01 36 38 58 49 56 44 32 44 00 00 00 00 00 00 00 00 00 00 00 00",
			packet);
	net_insert_data(packet, mpq_filetime, 24, 2 * sizeof(dword));
	net_insert_string(packet, mpq_file, 32);

	if (setting("Verbose")->b_var) {
		ui_console_lock();

		print("[BNFTP] sent:\n\n");

		net_dump_data(packet, 47, UI_WHITE, ui_print);

		print("\n");

		ui_console_unlock();
	}

	net_send(bnftp_socket, packet, 47);

	byte response[39];

	if ((int) net_receive(bnftp_socket, response, 39) < 39) {
		net_disconnect(bnftp_socket);

		print("[BNFTP] disconnected\n");

		return 0;
	}

	if (setting("Verbose")->b_var) {
		ui_console_lock();

		print("[BNFTP] received:\n\n");

		net_dump_data(response, 39, UI_WHITE, ui_print);

		print("\n");

		ui_console_unlock();
	}


	size_t file_size = net_get_data(response, sizeof(word), word)
			+ net_get_data(response, sizeof(dword), dword);

	byte mpq_data[file_size];
	size_t total = 0, tmp;

	do {
		tmp = net_receive(bnftp_socket, mpq_data + total, file_size - total);

		if ((int) tmp <= 0) {
			error("[BNFTP] failed to download MPQ file\n");

			total = 0;
			break;
		}

		if (setting("Verbose")->b_var) {
			ui_console_lock();

			print("[BNFTP] received:\n\n");

			net_dump_data(response, tmp, UI_WHITE, ui_print);

			print("\n");

			ui_console_unlock();
		}

		total += tmp;
	} while (total < file_size);

	print("[BNFTP] received MPQ file\n");

	net_disconnect(bnftp_socket);

	print("[BNFTP] disconnected\n");

	return total;
}

void bncs_request_channels(const char *version) {
	 bncs_send(0x0b, "%s", version);
}

void bncs_enter_chat(const char *character, const char *realm) {
	 bncs_send(0x0a, "%s 00 %s %s %s 00", character, realm, ",",
			 character);
}

void bncs_leave_chat() {
	 bncs_send(0x10, "");
}

void bncs_notify_create(int state, const char *game, const char *pass) {
	 bncs_send(0x1c, "%d %d %d %d %d %s 00 %s 00 00",
		 state, 0, 0, 0, 0, game, pass);
}

void bncs_notify_join(const char *version, const char *game, const char *pass) {
	bncs_send(0x22, "%s %b 00 00 00 %s 00 %s 00", version, setting("VersionByte")->i_var, game,
		 pass);
}

void bncs_notify_leave() {
	bncs_send(0x1f, "");
}

_export client_status_t bncs_get_client_status() {
        return bncs_client_status;
}

_export dword bncs_get_server_token() {
	return bncs_server_token;
}

static bool bncs_connect() {
	ui_console_lock();
	print("[BNCS] connecting... ");

	if (!net_connect(setting("Hostname")->s_var, setting("BNCSPort")->i_var, &bncs_socket)) {

		error("failed\n");
		ui_console_unlock();

		return FALSE;
	}

	print("done\n");
	ui_console_unlock();

	bncs_client_status = CLIENT_CONNECTED;

	if (setting("Verbose")->b_var) {
		ui_console_lock();
		print("\n[BNCS] sent:\n\n");
		net_dump_data((byte[]) {0x01}, 1, UI_WHITE, ui_print);
		ui_console_unlock();
	}
	net_send(bncs_socket, (byte[]) {0x01}, 1);

	bncs_send(0x50, bncs_auth_info_format, setting("VersionByte")->i_var);

	return TRUE;
}

void bncs_shutdown() {
	bncs_engine_shutdown = TRUE;

	if (bncs_client_status == CLIENT_DISCONNECTED) {
		return;
	}

	pthread_mutex_lock(&socket_m);

	if (!socket_shutdown) {
		net_shutdown(bncs_socket);
		socket_shutdown = TRUE;

		print("[BNCS] shutdown connection\n");
	}

	pthread_mutex_unlock(&socket_m);
}

static void bncs_disconnect() {
	net_disconnect(bncs_socket);

	print("[BNCS] disconnected\n");

	bncs_client_status = CLIENT_DISCONNECTED;
}

static int on_mcp_engine_connect(internal_packet_t *p) {
	if (*(int *)p->data == ENGINE_CONNECTED) {
		if (setting("BNCSDisconnect")->b_var) {
			bncs_shutdown();
		}
	}
	return FORWARD_PACKET;
}

void * bncs_client_engine(void *arg) {
	pthread_mutex_init(&socket_m, NULL);

	socket_shutdown = FALSE;

	internal_send(BNCS_ENGINE_MESSAGE, "%d", ENGINE_STARTUP);

	if (!bncs_connect()) {

		internal_send(BNCS_ENGINE_MESSAGE, "%d", ENGINE_SHUTDOWN);

		pthread_mutex_destroy(&socket_m);

		pthread_exit(NULL);
	}

	internal_send(BNCS_ENGINE_MESSAGE, "%d", ENGINE_CONNECTED);

	// bncs_engine_shutdown = FALSE;

	register_packet_handler(INTERNAL, MCP_CLIENT_ENGINE, (packet_handler_t) on_mcp_engine_connect);

	pthread_mutex_lock(&socket_m);

	while (!bncs_engine_shutdown) {
		bncs_packet_t incoming = bncs_new_packet();

		pthread_mutex_unlock(&socket_m);

		if ((int) bncs_receive_packet(&incoming) < 0) {

			print("[BNCS] connection closed\n");

			pthread_mutex_lock(&socket_m);

			break;
		}

		pthread_mutex_lock(&socket_m);

		switch(incoming.id) {

		/* BNCS ping */
		case 0x00:
		case 0x25: {

			print("[BNCS] replying to ping\n");

			bncs_send_packet(incoming);
			break;
		}

		case 0x50: {
			bncs_server_token = net_get_data(incoming.data, 4, dword);
			dword mpq_file_time[2];
			char mpq_file[15], formula[strlen((char *) incoming.data + 35)];

			net_extract_data(incoming.data, mpq_file_time, 12, 2 * sizeof(dword));
			net_extract_string(incoming.data, mpq_file, 20);
			net_extract_string(incoming.data, formula, 35);

			ui_console_lock();

			print("[BNCS] MPQ file: %s\n", mpq_file);

			print("[BNCS] formula: %s\n", formula);

			bnftp_download(mpq_file_time, mpq_file);

			int checksum;
			dword client_token = (dword) system_get_clock_ticks();
			dword classic_public, lod_public;
			byte classic_hash[20], lod_hash[20];

			print("[BNCS] generating checksum... ", checksum);

			if (!hash_executable(setting("BinaryDir")->s_var, formula, mpq_file, &checksum)) {

				error("failed to hash executables\n");
				ui_console_unlock();

				internal_send(INTERNAL_FATAL_ERROR, "");

				break;
			}

			print("done\n");

			print("[BNCS] generating classic hash... ");

			if (!hash_cdkey(setting("ClassicKey")->s_var, client_token, bncs_server_token,
					classic_hash, &classic_public)) {

				error("failed to hash classic CD key\n");
				ui_console_unlock();

				internal_send(INTERNAL_FATAL_ERROR, "%s 00", "");

				break;
			}

			print("done\n");

			print("[BNCS] generating expansion hash... ");

			if (!hash_cdkey(setting("ExpansionKey")->s_var, client_token, bncs_server_token, lod_hash,
					&lod_public)) {

				error("failed to hash expansion CD key\n");
				ui_console_lock();

				internal_send(INTERNAL_FATAL_ERROR, "%s 00", "");

				break;
			}

			print("done\n");

			print("[BNCS] sending auth check\n");
			ui_console_unlock();

			bncs_send(
					0x51,
					"%d 00 %b 00 01 %d 02 00 00 00 00 00 00 00 10 00 00 00 06 00 00 00 %d 00 00 00 00 %h 10 00 00 00 0a 00 00 00 %d 00 00 00 00 %h %s 00 %s 00",
					client_token, setting("VersionByte")->i_var, checksum, classic_public, classic_hash,
					lod_public, lod_hash, setting("VersionInfo")->s_var, setting("Owner")->s_var);
			break;
		}

		/* log on to battle.net successful */
		case 0x51: {
			dword response = net_get_data(incoming.data, 0, dword);
			if ((response & ~0x010) == 0x000) {

				internal_send(BNCS_ENGINE_MESSAGE, "%d", MODULES_STARTUP);

				start_modules(MODULE_BNCS);

			}
			break;
		}

		/* log on to realm successful */
		case 0x3e: {
			if (incoming.len > 12) {
				dword addr = net_get_data(incoming.data, 16, dword);
				dword port = net_get_data(incoming.data, 20, dword);

				mcp_con_info_t *info = malloc(sizeof(mcp_con_info_t));
				info->addr = addr;
				info->port = port;
				info->startup.id = 0x01;
				info->startup.len = incoming.len - (sizeof(byte) + 2 * sizeof(dword));

				if (mcp_has_payload(&info->startup)) {
					info->startup.data = (byte *) malloc(info->startup.len - MCP_HEADER_SIZE);
					net_extract_data(incoming.data, info->startup.data, 0, 16);
					net_extract_data(incoming.data, info->startup.data + 16, 24, info->startup.len - 16 - MCP_HEADER_SIZE);
				}

				start_client_engine(MCP_CLIENT_ENGINE, info);
			}
			break;
		}
		}

		invoke_packet_handlers(BNCS_RECEIVED, &incoming);

		if (incoming.data) {
			free(incoming.data);
		}
	}

	if (!socket_shutdown) {
		net_shutdown(bncs_socket);
		socket_shutdown = TRUE;
	}

	pthread_mutex_unlock(&socket_m);

	internal_send(BNCS_ENGINE_MESSAGE, "%d", MODULES_CLEANUP);

	cleanup_modules(MODULE_BNCS);

	clear_module_schedule(MODULE_BNCS);

	bncs_disconnect();

	internal_send(BNCS_ENGINE_MESSAGE, "%d", ENGINE_DISCONNECTED);

	unregister_packet_handler(INTERNAL, MCP_CLIENT_ENGINE, (packet_handler_t) on_mcp_engine_connect);

	internal_send(BNCS_ENGINE_MESSAGE, "%d", ENGINE_SHUTDOWN);

	pthread_mutex_destroy(&socket_m);

	pthread_exit(NULL);
}
