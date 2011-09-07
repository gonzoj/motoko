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

#include <stdlib.h>

#include "internal.h"
#include "settings.h"
#include "gui.h"

#include <util/net.h>

static void internal_create_packet(internal_packet_t *packet, byte id, char *format, va_list args) {
	packet->id = id;
	packet->len = INTERNAL_HEADER_SIZE;
	packet->len += net_build_data((void **) &packet->data, format, args);
}

static char *message[] = {
	"ENGINE_STARTUP",
	"ENGINE_SHUTDOWN",
	"ENGINE_CONNECTED",
	"ENGINE_DICONNECTED",
	"MODULES_STARTUP",
	"MODULES_CLEANUP"
};

static void internal_dump_packet(internal_packet_t packet) {
	if (packet.id == BNCS_ENGINE_MESSAGE) {
		print("BNCS_ENGINE_MESSAGE: ");
		if (*(dword *)packet.data <= ENGINE_STARTUP && *(dword *)packet.data >= MODULES_CLEANUP) {
			print("%s", message[*(dword *)packet.data]);
		} else {
			print("unkown ID");
		}
		print("\n");
	} else if (packet.id == MCP_ENGINE_MESSAGE) {
		print("MCP_ENGINE_MESSAGE: ");
		if (*(dword *)packet.data <= ENGINE_STARTUP && *(dword *)packet.data >= MODULES_CLEANUP) {
			print("%s", message[*(dword *)packet.data]);
		} else {
			print("unkown ID");
		}
		print("\n");
	} else if (packet.id == D2GS_ENGINE_MESSAGE) {
		print("D2GS_ENGINE_MESSAGE: ");
		if (*(dword *)packet.data <= ENGINE_STARTUP && *(dword *)packet.data >= MODULES_CLEANUP) {
			print("%s", message[*(dword *)packet.data]);
		} else {
			print("unkown ID");
		}
		print("\n");
	} else if (packet.id == INTERNAL_FATAL_ERROR) {
		print("INTERNAL_FATAL_ERROR: ");
		print("\n");
	} else {
		byte buf[packet.len];
		buf[0] = packet.id;
		*(size_t *) &buf[1] = packet.len;
		memcpy(buf + sizeof(byte) + sizeof(size_t), packet.data, packet.len - (sizeof(byte) + sizeof(size_t)));
		net_dump_data(buf, packet.len, UI_WHITE, ui_print);
	}
}

_export size_t internal_send(byte id, char *format, ...) {
	internal_packet_t request = internal_new_packet();
	va_list args;
	va_start(args, format);
	internal_create_packet(&request, id, format, args);
	va_end(args);
	if (setting("Verbose")->b_var) {
		ui_console_lock();

		print("[INTERNAL] sent:\n\n");

		internal_dump_packet(request);

		print("\n");

		ui_console_unlock();
	}
	internal_send_packet(request);
	if (request.data) {
		free(request.data);
	}
	return request.len;
}
