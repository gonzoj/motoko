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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "internal.h"
#include "bncs.h"
#include "settings.h"
#include "mcp.h"
#include "moduleman.h"
#include "packet.h"
#include "gui.h"

#include <util/net.h>
#include <util/system.h>

#include <wardenc.h>

#include "d2gs.h"

static int d2gs_socket;

static client_status_t d2gs_client_status = CLIENT_DISCONNECTED;

/*static*/ bool d2gs_engine_shutdown;

static pthread_t d2gs_ping_tid;

static dword d2gs_hash;

#define D2GS_DECOMPRESS_BUFFER_SIZE 0x6000

static const unsigned int index_table[] =
{
	0x0247, 0x0236, 0x0225, 0x0214, 0x0203, 0x01F2, 0x01E1, 0x01D0,
	0x01BF, 0x01AE, 0x019D, 0x018C, 0x017B, 0x016A, 0x0161, 0x0158,
	0x014F, 0x0146, 0x013D, 0x0134, 0x012B, 0x0122, 0x0119, 0x0110,
	0x0107, 0x00FE, 0x00F5, 0x00EC, 0x00E3, 0x00DA, 0x00D1, 0x00C8,
	0x00BF, 0x00B6, 0x00AD, 0x00A8, 0x00A3, 0x009E, 0x0099, 0x0094,
	0x008F, 0x008A, 0x0085, 0x0080, 0x007B, 0x0076, 0x0071, 0x006C,
	0x0069, 0x0066, 0x0063, 0x0060, 0x005D, 0x005A, 0x0057, 0x0054,
	0x0051, 0x004E, 0x004B, 0x0048, 0x0045, 0x0042, 0x003F, 0x003F,
	0x003C, 0x003C, 0x0039, 0x0039, 0x0036, 0x0036, 0x0033, 0x0033,
	0x0030, 0x0030, 0x002D, 0x002D, 0x002A, 0x002A, 0x0027, 0x0027,
	0x0024, 0x0024, 0x0021, 0x0021, 0x001E, 0x001E, 0x001B, 0x001B,
	0x0018, 0x0018, 0x0015, 0x0015, 0x0012, 0x0012, 0x0012, 0x0012,
	0x000F, 0x000F, 0x000F, 0x000F, 0x000C, 0x000C, 0x000C, 0x000C,
	0x0009, 0x0009, 0x0009, 0x0009, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};


static const unsigned char character_table[] =
{
	0x00, 0x00, 0x01, 0x00, 0x01, 0x04, 0x00, 0xFF, 0x06, 0x00, 0x14, 0x06,
	0x00, 0x13, 0x06, 0x00, 0x05, 0x06, 0x00, 0x02, 0x06, 0x00, 0x80, 0x07,
	0x00, 0x6D, 0x07, 0x00, 0x69, 0x07, 0x00, 0x68, 0x07, 0x00, 0x67, 0x07,
	0x00, 0x1E, 0x07, 0x00, 0x15, 0x07, 0x00, 0x12, 0x07, 0x00, 0x0D, 0x07,
	0x00, 0x0A, 0x07, 0x00, 0x08, 0x07, 0x00, 0x07, 0x07, 0x00, 0x06, 0x07,
	0x00, 0x04, 0x07, 0x00, 0x03, 0x07, 0x00, 0x6C, 0x08, 0x00, 0x51, 0x08,
	0x00, 0x20, 0x08, 0x00, 0x1F, 0x08, 0x00, 0x1D, 0x08, 0x00, 0x18, 0x08,
	0x00, 0x17, 0x08, 0x00, 0x16, 0x08, 0x00, 0x11, 0x08, 0x00, 0x10, 0x08,
	0x00, 0x0F, 0x08, 0x00, 0x0C, 0x08, 0x00, 0x0B, 0x08, 0x00, 0x09, 0x08,
	0x01, 0x96, 0x09, 0x97, 0x09, 0x01, 0x90, 0x09, 0x95, 0x09, 0x01, 0x64,
	0x09, 0x6B, 0x09, 0x01, 0x62, 0x09, 0x63, 0x09, 0x01, 0x56, 0x09, 0x58,
	0x09, 0x01, 0x52, 0x09, 0x55, 0x09, 0x01, 0x4D, 0x09, 0x50, 0x09, 0x01,
	0x45, 0x09, 0x4C, 0x09, 0x01, 0x40, 0x09, 0x43, 0x09, 0x01, 0x31, 0x09,
	0x3B, 0x09, 0x01, 0x28, 0x09, 0x30, 0x09, 0x01, 0x1A, 0x09, 0x25, 0x09,
	0x01, 0x0E, 0x09, 0x19, 0x09, 0x02, 0xE2, 0x0A, 0xE8, 0x0A, 0xF0, 0x0A,
	0xF8, 0x0A, 0x02, 0xC0, 0x0A, 0xC2, 0x0A, 0xCE, 0x0A, 0xE0, 0x0A, 0x02,
	0xA0, 0x0A, 0xA2, 0x0A, 0xB0, 0x0A, 0xB8, 0x0A, 0x02, 0x8A, 0x0A, 0x8F,
	0x0A, 0x93, 0x0A, 0x98, 0x0A, 0x02, 0x81, 0x0A, 0x82, 0x0A, 0x83, 0x0A,
	0x89, 0x0A, 0x02, 0x7C, 0x0A, 0x7D, 0x0A, 0x7E, 0x0A, 0x7F, 0x0A, 0x02,
	0x77, 0x0A, 0x78, 0x0A, 0x79, 0x0A, 0x7A, 0x0A, 0x02, 0x73, 0x0A, 0x74,
	0x0A, 0x75, 0x0A, 0x76, 0x0A, 0x02, 0x6E, 0x0A, 0x6F, 0x0A, 0x70, 0x0A,
	0x72, 0x0A, 0x02, 0x61, 0x0A, 0x65, 0x0A, 0x66, 0x0A, 0x6A, 0x0A, 0x02,
	0x5D, 0x0A, 0x5E, 0x0A, 0x5F, 0x0A, 0x60, 0x0A, 0x02, 0x57, 0x0A, 0x59,
	0x0A, 0x5A, 0x0A, 0x5B, 0x0A, 0x02, 0x4A, 0x0A, 0x4B, 0x0A, 0x4E, 0x0A,
	0x53, 0x0A, 0x02, 0x46, 0x0A, 0x47, 0x0A, 0x48, 0x0A, 0x49, 0x0A, 0x02,
	0x3F, 0x0A, 0x41, 0x0A, 0x42, 0x0A, 0x44, 0x0A, 0x02, 0x3A, 0x0A, 0x3C,
	0x0A, 0x3D, 0x0A, 0x3E, 0x0A, 0x02, 0x36, 0x0A, 0x37, 0x0A, 0x38, 0x0A,
	0x39, 0x0A, 0x02, 0x32, 0x0A, 0x33, 0x0A, 0x34, 0x0A, 0x35, 0x0A, 0x02,
	0x2B, 0x0A, 0x2C, 0x0A, 0x2D, 0x0A, 0x2E, 0x0A, 0x02, 0x26, 0x0A, 0x27,
	0x0A, 0x29, 0x0A, 0x2A, 0x0A, 0x02, 0x21, 0x0A, 0x22, 0x0A, 0x23, 0x0A,
	0x24, 0x0A, 0x03, 0xFB, 0x0B, 0xFC, 0x0B, 0xFD, 0x0B, 0xFE, 0x0B, 0x1B,
	0x0A, 0x1B, 0x0A, 0x1C, 0x0A, 0x1C, 0x0A, 0x03, 0xF2, 0x0B, 0xF3, 0x0B,
	0xF4, 0x0B, 0xF5, 0x0B, 0xF6, 0x0B, 0xF7, 0x0B, 0xF9, 0x0B, 0xFA, 0x0B,
	0x03, 0xE9, 0x0B, 0xEA, 0x0B, 0xEB, 0x0B, 0xEC, 0x0B, 0xED, 0x0B, 0xEE,
	0x0B, 0xEF, 0x0B, 0xF1, 0x0B, 0x03, 0xDE, 0x0B, 0xDF, 0x0B, 0xE1, 0x0B,
	0xE3, 0x0B, 0xE4, 0x0B, 0xE5, 0x0B, 0xE6, 0x0B, 0xE7, 0x0B, 0x03, 0xD6,
	0x0B, 0xD7, 0x0B, 0xD8, 0x0B, 0xD9, 0x0B, 0xDA, 0x0B, 0xDB, 0x0B, 0xDC,
	0x0B, 0xDD, 0x0B, 0x03, 0xCD, 0x0B, 0xCF, 0x0B, 0xD0, 0x0B, 0xD1, 0x0B,
	0xD2, 0x0B, 0xD3, 0x0B, 0xD4, 0x0B, 0xD5, 0x0B, 0x03, 0xC5, 0x0B, 0xC6,
	0x0B, 0xC7, 0x0B, 0xC8, 0x0B, 0xC9, 0x0B, 0xCA, 0x0B, 0xCB, 0x0B, 0xCC,
	0x0B, 0x03, 0xBB, 0x0B, 0xBC, 0x0B, 0xBD, 0x0B, 0xBE, 0x0B, 0xBF, 0x0B,
	0xC1, 0x0B, 0xC3, 0x0B, 0xC4, 0x0B, 0x03, 0xB2, 0x0B, 0xB3, 0x0B, 0xB4,
	0x0B, 0xB5, 0x0B, 0xB6, 0x0B, 0xB7, 0x0B, 0xB9, 0x0B, 0xBA, 0x0B, 0x03,
	0xA9, 0x0B, 0xAA, 0x0B, 0xAB, 0x0B, 0xAC, 0x0B, 0xAD, 0x0B, 0xAE, 0x0B,
	0xAF, 0x0B, 0xB1, 0x0B, 0x03, 0x9F, 0x0B, 0xA1, 0x0B, 0xA3, 0x0B, 0xA4,
	0x0B, 0xA5, 0x0B, 0xA6, 0x0B, 0xA7, 0x0B, 0xA8, 0x0B, 0x03, 0x92, 0x0B,
	0x94, 0x0B, 0x99, 0x0B, 0x9A, 0x0B, 0x9B, 0x0B, 0x9C, 0x0B, 0x9D, 0x0B,
	0x9E, 0x0B, 0x03, 0x86, 0x0B, 0x87, 0x0B, 0x88, 0x0B, 0x8B, 0x0B, 0x8C,
	0x0B, 0x8D, 0x0B, 0x8E, 0x0B, 0x91, 0x0B, 0x03, 0x2F, 0x0B, 0x4F, 0x0B,
	0x54, 0x0B, 0x5C, 0x0B, 0x71, 0x0B, 0x7B, 0x0B, 0x84, 0x0B, 0x85, 0x0B
};

static const unsigned int bit_masks[] =
{
	0x0000, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F,
	0x00FF, 0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF
};

static size_t d2gs_get_compressed_packet_size(byte *input, size_t *header_size) {
	if (*input < 0xF0) {
		*header_size = 1;
		return *input - 1;
	} else {
		*header_size = 2;
		return ((*input & 0xF) << 8) + *++input - 2;
	}
}

static bool d2gs_decompress(byte *input, size_t input_size, byte *output, size_t output_size, size_t *len) {
	unsigned int a, b = 0, c, d;
	unsigned int count = 0x20;
	size_t max_count = output_size;
	while (TRUE) {
		while (input_size > 0 && count >= 8) {
			count -= 8;
			input_size--;
			a = *input++ << count;
			b |= a;
		}
		int index = index_table[b >> 0x18];
		a = character_table[index];
		d = (b >> (0x18 - a)) & bit_masks[a];
		c = character_table[index + 2 * d + 2];
		count += c;
		if (count > 0x20) {
			*len = output_size - max_count;
			return TRUE;
		}
		max_count--;
		if (max_count == 0) {
			return FALSE;
		}
		a = character_table[index + 2 * d + 1];
		*output++ = (byte) a;
		b <<= (c & 0xFF);
	}
}

static const unsigned int compression_table[] =
{
	0x80010000, 0x70040000, 0x5C060000, 0x3E070000, 0x40070000, 0x60060000, 0x42070000, 0x44070000,
	0x46070000, 0x30080000, 0x48070000, 0x31080000, 0x32080000, 0x4A070000, 0x23080100, 0x33080000,
	0x34080000, 0x35080000, 0x4C070000, 0x64060000, 0x68060000, 0x4E070000, 0x36080000, 0x37080000,
	0x38080000, 0x23080101, 0x24080100, 0x0D070306, 0x0D070307, 0x39080000, 0x50070000, 0x3A080000,
	0x3B080000, 0x0E080200, 0x0E080201, 0x0E080202, 0x0E080203, 0x24080101, 0x0F080200, 0x0F080201,
	0x25080100, 0x0F080202, 0x0F080203, 0x10080200, 0x10080201, 0x10080202, 0x10080203, 0x00080300,
	0x25080101, 0x26080100, 0x11080200, 0x11080201, 0x11080202, 0x11080203, 0x12080200, 0x12080201,
	0x12080202, 0x12080203, 0x13080200, 0x26080101, 0x13080201, 0x13080202, 0x13080203, 0x14080200,
	0x27080100, 0x14080201, 0x14080202, 0x27080101, 0x14080203, 0x28080100, 0x15080200, 0x15080201,
	0x15080202, 0x15080203, 0x16080200, 0x16080201, 0x28080101, 0x29080100, 0x16080202, 0x00080301,
	0x29080101, 0x3C080000, 0x2A080100, 0x16080203, 0x00080302, 0x2A080101, 0x2B080100, 0x17080200,
	0x2B080101, 0x17080201, 0x17080202, 0x17080203, 0x00080303, 0x18080200, 0x18080201, 0x18080202,
	0x18080203, 0x19080200, 0x2C080100, 0x2C080101, 0x2D080100, 0x19080201, 0x19080202, 0x52070000,
	0x54070000, 0x56070000, 0x19080203, 0x2D080101, 0x3D080000, 0x58070000, 0x1A080200, 0x1A080201,
	0x1A080202, 0x00080304, 0x1A080203, 0x1B080200, 0x1B080201, 0x1B080202, 0x1B080203, 0x1C080200,
	0x1C080201, 0x1C080202, 0x1C080203, 0x00080305, 0x1D080200, 0x1D080201, 0x1D080202, 0x1D080203,
	0x5A070000, 0x1E080200, 0x1E080201, 0x1E080202, 0x00080306, 0x00080307, 0x01080300, 0x01080301,
	0x01080302, 0x1E080203, 0x1F080200, 0x01080303, 0x01080304, 0x01080305, 0x01080306, 0x1F080201,
	0x2E080100, 0x01080307, 0x02080300, 0x1F080202, 0x02080301, 0x2E080101, 0x2F080100, 0x2F080101,
	0x1F080203, 0x02080302, 0x02080303, 0x02080304, 0x02080305, 0x02080306, 0x02080307, 0x03080300,
	0x20080200, 0x03080301, 0x20080201, 0x03080302, 0x03080303, 0x03080304, 0x03080305, 0x03080306,
	0x03080307, 0x04080300, 0x04080301, 0x04080302, 0x04080303, 0x04080304, 0x04080305, 0x04080306,
	0x20080202, 0x04080307, 0x05080300, 0x05080301, 0x05080302, 0x05080303, 0x05080304, 0x05080305,
	0x20080203, 0x05080306, 0x05080307, 0x06080300, 0x06080301, 0x06080302, 0x06080303, 0x06080304,
	0x21080200, 0x06080305, 0x21080201, 0x06080306, 0x06080307, 0x07080300, 0x07080301, 0x07080302,
	0x07080303, 0x07080304, 0x07080305, 0x07080306, 0x07080307, 0x08080300, 0x21080202, 0x08080301,
	0x08080302, 0x08080303, 0x08080304, 0x08080305, 0x08080306, 0x08080307, 0x09080300, 0x09080301,
	0x09080302, 0x09080303, 0x09080304, 0x09080305, 0x09080306, 0x09080307, 0x0A080300, 0x0A080301,
	0x21080203, 0x0A080302, 0x22080200, 0x0A080303, 0x0A080304, 0x0A080305, 0x0A080306, 0x0A080307,
	0x22080201, 0x0B080300, 0x0B080301, 0x0B080302, 0x0B080303, 0x0B080304, 0x0B080305, 0x0B080306,
	0x22080202, 0x0B080307, 0x0C080300, 0x0C080301, 0x0C080302, 0x0C080303, 0x0C080304, 0x0C080305,
	0x22080203, 0x0C080306, 0x0C080307, 0x0D080300, 0x0D080301, 0x0D080302, 0x0D080303, 0x6C060000,
};

static size_t d2gs_create_packet_header(size_t size, byte *output) {
	if (size > 238) {
		size += 2;
		size |= 0xF000;
		*output++ = (byte) (size >> 8);
		*output = (byte) (size & 0xFF);
		return 2;
	} else {
		*output = (byte) (size + 1);
		return 1;
	}
}


static size_t d2gs_compress(byte *input, size_t input_size, byte *output) {
	unsigned int a, e, buf = 0;
	unsigned int count = 0;
	byte *output_base = output;
	while (input_size > 0) {
		input_size--;
		a = compression_table[*input++];
		e = (a & 0xFF00) >> 8;
		buf |= (a >> 24) << (24 - count);
		count += (a & 0xFF0000) >> 16;
		if (e) {
			buf |= ((a & 0xFF) << (8 - e)) << (24 - count);
		}
		while (count > 8) {
			*output++ = buf >> 24;
			count -= 8;
			buf <<= 8;
		}
	}
	while (count > 0) {
		*output++ = buf >> 24;
		buf <<= 8;
		count -= 8;
	}
	return (size_t) (output - output_base);
}

#define D2GS_NUM_PACKET_SIZES 177

static const size_t packet_sizes[] =
{
	1, 9/*8*/, 1, 12, 1, 1, 1, 6, 6, 11, 6, 6, 9, 13, 12, 16,
	16, 8, 26, 14, 18, 11, 0, 0, 15, 2, 2, 3, 5, 3, 4, 6,
	10, 12, 12, 13, 90, 90, 0, 40, 103,97, 15, 0, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 34, 8,
	13, 0, 6, 0, 0, 13, 0, 11, 11, 0, 0, 0, 16, 17, 7, 1,
	15, 14, 42, 10, 3, 0, 0, 14, 7, 26, 40, 0, 5, 6, 38, 5,
	7, 2, 7, 21, 0, 7, 7, 16, 21, 12, 12, 16, 16, 10, 1, 1,
	1, 1, 1, 32, 10, 13, 6, 2, 21, 6, 13, 8, 6, 18, 5, 10,
	4, 20, 29, 0, 0, 0, 0, 0, 0, 2, 6, 6, 11, 7, 10, 33,
	13, 26, 6, 8, 0, 13, 9, 1, 7, 16, 17, 7, 0, 0, 7, 8,
	10, 7, 8, 24, 3, 8, 0, 7, 0, 7, 0, 7, 0, 0, 0, 0,
	1
};

static bool d2gs_get_chat_packet_size(char *packet, size_t packet_size, size_t *size) {
	size_t name_len, message_len;
	*size = 10;
	if ((name_len = strnlen(packet + *size, packet_size - *size)) == packet_size - *size) {
		return FALSE;
	}
	*size += name_len + 1;
	if ((message_len = strnlen(packet + *size, packet_size - *size)) == packet_size - *size) {
		return FALSE;
	}
	*size += message_len + 1;
	return TRUE;
}

static bool d2gs_get_packet_size(byte *packet, size_t packet_size, size_t *size) {
	byte id = net_get_data(packet, 0, byte);
	switch (id) {
	case 0x26: {
		if (packet_size >= 12) {
			if (d2gs_get_chat_packet_size((char *) packet, packet_size, size)) {
				return TRUE;
			}
		}
		break;
	}
	case 0x5b: {
		if (packet_size >= 3) {
			*size = (size_t) net_get_data(packet, 1, word);
			return TRUE;
		}
		break;
	}
	case 0x94: {
		if (packet_size >= 2) {
			*size = (size_t) net_get_data(packet, 1, byte) * 3 + 6;
			return TRUE;
		}
		break;
	}
	case 0xa8:
	case 0xaa: {
		if (packet_size >= 7) {
			*size = (size_t) net_get_data(packet, 6, byte);
			return TRUE;
		}
		break;
	}
	case 0xac: {
		if (packet_size >= 13) {
			*size = (size_t) net_get_data(packet, 12, byte);
			return TRUE;
		}
		break;
	}
	case 0xae: {
		if (packet_size >= 3) {
			*size = (size_t) net_get_data(packet, 1, word) + 3;
			return TRUE;
		}
		break;
	}
	case 0x9c:
	case 0x9d: {
		if (packet_size >= 3) {
			*size = (size_t) net_get_data(packet, 2, byte);
			return TRUE;
		}
		break;
	}
	default: {
		if (id < D2GS_NUM_PACKET_SIZES) {
			*size = packet_sizes[id];
			return *size ? TRUE : FALSE;
		}
		break;
	}
	}
	return FALSE;
}

static size_t d2gs_receive(byte **packet, size_t *header_size, size_t *data_size) {
	static byte *buffer = NULL;
	static size_t len = 0;

	size_t total = 0, packet_size = 0;
	while (TRUE) {
		if (!len || len < 2  || (len < 3 && *buffer >= 0xf0) || packet_size > len) {
			byte buf[524];
			size_t received = net_receive(d2gs_socket, buf, 524);

			if ((int) received <= 0) {
				if (buffer) {
					free(buffer);
					buffer = NULL;
					len = 0;
				}

				d2gs_engine_shutdown = TRUE;
				return received;
			}

			buffer = (byte *) realloc(buffer, len + received);
			memcpy(buffer + len, buf, received);
			len += received;

			total += received;
		}

		/* handle special case 0xaf 0x01 packet which comes uncompressed */
		if (len == 2 && *(word *)buffer == 0x01AF) {
			*packet = (byte *) malloc(len);
			memcpy(*packet, buffer, len);
			free(buffer);
			buffer = NULL;
			len = 0;
			return 2;
		}

		if (len < 2 || (len < 3 && *buffer >= 0xf0)) {
			continue;
		}

		if (!packet_size) {
			*data_size = d2gs_get_compressed_packet_size(buffer, header_size);
			packet_size = *data_size + *header_size;
		}
		if (packet_size > len) {
			continue;
		} else {
			*packet = (byte *) malloc(packet_size);
			memcpy(*packet, buffer, packet_size);
			len -= packet_size;
			if (len) {
				byte tmp[len];
				memcpy(tmp, buffer + packet_size, len);
				free(buffer);
				buffer = (byte *) malloc(len);
				memcpy(buffer, tmp, len);
			} else {
				free(buffer);
				buffer = NULL;
			}
			break;
		}
	}

	return packet_size;
}

static bool d2gs_process_packet(byte **buf, size_t *len) {
	static byte **packets = NULL;
	static int *packet_sizes = NULL;
	static int index = 0;
	static int n_packets = 0;

	if (!n_packets) {

		int i;
		for (i = 0; i < index; i++) {
			free(packets[i]);
		}
		free(packets);
		packets = NULL;
		free(packet_sizes);
		packet_sizes = NULL;

		index = 0;

		byte packet[D2GS_DECOMPRESS_BUFFER_SIZE]; // TODO: is this enough? RedVex says so.
		byte *buffer = NULL;
		size_t data_len, packet_len, header_len, size;
		data_len = d2gs_receive(&buffer, &header_len, &packet_len);
		if ((int) data_len <= 0) {
			return FALSE;
		}
		if (data_len == 2 && *(word *)buffer == 0x01AF) {
			// just hand it down to d2gs_receive_packet without the static buffer
			*len = data_len;
			*buf = malloc(data_len);
			memcpy(*buf, buffer, data_len);

			free(buffer);

			return TRUE;
		}

		if (!d2gs_decompress(buffer + header_len, packet_len, packet, D2GS_DECOMPRESS_BUFFER_SIZE, &size)) {
			ui_console_lock();

			error("[D2GS] error: could not decompress packet:\n\n");

			net_dump_data(buffer, data_len, UI_WHITE, ui_print);

			print("\n");

			ui_console_unlock();

			free(buffer);

			return FALSE;
		}

		size_t total = 0;
		while (total < size) {
			size_t packet_size;
			if (!d2gs_get_packet_size(packet + total, size - total, &packet_size)) {
				ui_console_lock();

				error("[D2GS] error: could not determine packet size:\n\n");

				net_dump_data(packet + total, size - total, UI_WHITE, ui_print);

				print("\n");

				ui_console_unlock();

				free(buffer);

				if (n_packets) {
					for (i = 0; i < n_packets; i++) {
						free(packets[i]);
					}
					free(packets);
					packets = NULL;
					free(packet_sizes);
					packet_sizes = NULL;

					index = 0;

					n_packets = 0; // should fix segfault when receiving bad packets
				}

				return FALSE;
			}
			packets = (byte **) realloc(packets, (n_packets + 1) * sizeof(byte *));
			packets[n_packets] = (byte *) malloc(packet_size * sizeof(byte));
			packet_sizes = (int *) realloc(packet_sizes, (n_packets + 1) * sizeof(int));
			memcpy(packets[n_packets], packet + total, packet_size);

			packet_sizes[n_packets] = packet_size;
			total += packet_size;
			n_packets++;
		}

		// free the buffer
		free(buffer);

	}

	n_packets--;
	*buf = malloc(packet_sizes[index]);
	memcpy(*buf, packets[index], packet_sizes[index]);
	*len = packet_sizes[index];
	index++;
	return TRUE;
}

static void d2gs_dump_packet(d2gs_packet_t packet) {
	byte buf[packet.len];
	buf[0] = packet.id;
	memcpy(buf + sizeof(byte), packet.data, packet.len - D2GS_HEADER_SIZE);

	net_dump_data(buf, packet.len, UI_WHITE, ui_print);
}

static size_t d2gs_send_packet(d2gs_packet_t packet) {
	if (invoke_packet_handlers(D2GS_SENT, &packet) == BLOCK_PACKET) {
		return packet.len; // pretend we sent it
	}

	if (setting("Verbose")->b_var) {
		ui_console_lock();

		print("[D2GS] sent:\n\n");

		d2gs_dump_packet(packet);

		print("\n");

		ui_console_unlock();
	}

	byte buf[packet.len];
	buf[0] = packet.id;
	memcpy(buf + sizeof(byte), packet.data, packet.len - D2GS_HEADER_SIZE);

	size_t sent = net_send(d2gs_socket, buf, packet.len);

	/*if (sent == packet.len) {
		invoke_packet_handlers(D2GS_SENT, &packet);
	}*/

	return sent;
}

#define WARDEN_REQUEST_ID 0xae

static size_t d2gs_receive_packet(d2gs_packet_t *packet) {
	byte *buf;
	size_t len = -1;

	if (d2gs_process_packet(&buf, &len)) {
		packet->len = len;
		packet->id = *buf;
		if (d2gs_has_payload(packet)) {
			packet->data = malloc(len - D2GS_HEADER_SIZE);
			memcpy(packet->data, buf + sizeof(byte), len - D2GS_HEADER_SIZE);
		}
		free(buf);

		if (setting("Verbose")->b_var && packet->id != WARDEN_REQUEST_ID) {
			ui_console_lock();

			print("[D2GS] received:\n\n");

			d2gs_dump_packet(*packet);

			print("\n");

			ui_console_unlock();
		}
	}

	return len;
}

static void d2gs_create_packet(d2gs_packet_t *packet, byte id, char *format, va_list args) {
	packet->id = id;
	packet->len = D2GS_HEADER_SIZE;
	packet->len += net_build_data((void **) &packet->data, format, args);
}

_export size_t d2gs_send(byte id, char *format, ...) {
	module_wait(MODULE_D2GS);

	d2gs_packet_t request = d2gs_new_packet();

	va_list args;
	va_start(args, format);
	d2gs_create_packet(&request, id, format, args);
	va_end(args);

	size_t sent = d2gs_send_packet(request);

	if (request.data) {
		free(request.data);
	}

	if (is_module_thread() && (int) sent <= 0) {
		pthread_exit(NULL);
	}

	return sent;
}

void d2gs_send_raw(byte *packet, size_t len) {
	net_send(d2gs_socket, packet, len);
}

_export client_status_t d2gs_get_client_status() {
	return d2gs_client_status;
}

_export dword d2gs_get_hash() {
	return d2gs_hash;
}

static void * d2gs_ping_thread(void *arg) {
	time_t start;
	time(&start);

	while (!d2gs_engine_shutdown) {
		time_t cur;

		if (difftime(time(&cur), start) >= 5) {
			dword ticks = (dword) system_get_clock_ticks();

			if ((int) d2gs_send(0x6d, "%d %d 00 00 00 00", ticks, (ticks + 0x01) & 0xff) <= 0) {
				break;
			}

			time(&start);
		} else {
			usleep(100000);
		}
	}

	pthread_exit(NULL);
}


static bool d2gs_connect(d2gs_con_info_t *info) {
	char addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &info->addr, addr, INET_ADDRSTRLEN);

	ui_console_lock();

	print("[D2GS] connecting (%s:%i)... ", addr, setting("D2GSPort")->i_var);

	if (!net_connect(addr, setting("D2GSPort")->i_var, &d2gs_socket)) {

		error("failed\n");
		ui_console_unlock();

		return FALSE;
	}

	print("done\n");
	ui_console_unlock();

	d2gs_client_status = CLIENT_CONNECTED;

	return TRUE;
}


void d2gs_shutdown() {
	d2gs_engine_shutdown = TRUE;

	if (d2gs_client_status == CLIENT_DISCONNECTED) {
		return;
	}

	net_shutdown(d2gs_socket);

	print("[D2GS] shutdown connection\n");
}

static void d2gs_disconnect() {

	pthread_join(d2gs_ping_tid, NULL);

	net_disconnect(d2gs_socket);

	print("[D2GS] disconnected\n");

	d2gs_client_status = CLIENT_DISCONNECTED;
}

void * d2gs_client_engine(d2gs_con_info_t *info) {

	internal_send(D2GS_ENGINE_MESSAGE, "%d", ENGINE_STARTUP);

	if (!d2gs_connect(info)) {

		internal_send(D2GS_ENGINE_MESSAGE, "%d", ENGINE_SHUTDOWN);

		pthread_exit(NULL);
	}

	internal_send(D2GS_ENGINE_MESSAGE, "%d", ENGINE_CONNECTED);

	d2gs_hash = info->hash;

	// d2gs_engine_shutdown = FALSE;

	int errors = 0;

	while (!d2gs_engine_shutdown) {
		d2gs_packet_t incoming = d2gs_new_packet();

		if ((int) d2gs_receive_packet(&incoming) <= 0) { // TODO: sometimes the connection seems to hang up but we don't get unblocked
			if (!d2gs_engine_shutdown) { // we might have to use non-blocking sockets and select :-/
				error("[D2GS] error: failed to process packet\n");
				errors++;
			}
			continue;
		}

		switch (incoming.id) {

		case 0xaf: {
			char format[512] = "%d %w %b %b 00 00 00 50 cc 5d ed b6 19 a5 91 00 %s 00 ";
			if (strlen(mcp_characters[mcp_character_index].name) < 15) {
				char padding[] = "00 00 00 00 00 02 00 4a 29 af 6f 4b 00 00 00 00";
				strcat(format, padding + (strlen(mcp_characters[mcp_character_index].name) + 1) * 3);
			}
			d2gs_send(0x68, format, info->hash, info->token, mcp_characters[mcp_character_index].class, setting("VersionByte")->i_var, mcp_characters[mcp_character_index].name);
			break;
		}

		case 0x02: {
			byte b = 0x6b;
			net_send(d2gs_socket, &b, 1);

			if (setting("Verbose")->b_var) {
				ui_console_lock();

				print("[D2GS] sent:\n\n");

				net_dump_data(&b, 1, UI_WHITE, ui_print);

				print("\n");

				ui_console_unlock();
			}
			break;
		}

		case 0x01: {
			d2gs_send(0x6d, "%d 00 00 00 00 00 00 00 00", (dword) system_get_clock_ticks);
			pthread_create(&d2gs_ping_tid, NULL, d2gs_ping_thread, NULL);
			break;
		}

		case 0x04: {

			internal_send(D2GS_ENGINE_MESSAGE, "%d", MODULES_STARTUP);

			start_modules(MODULE_D2GS);

			break;
		}

		case 0x06:
		case 0xb4:
		case 0xb0: {
			net_shutdown(d2gs_socket);
			break;
		}

		/* the Warden */
		case WARDEN_REQUEST_ID: {
			ui_console_lock();

			if (setting("Verbose")->b_var) {
				print("[D2GS] warden packet received:\n\n");
			}
			if (setting("ResponseWarden")->b_var) {
				byte *p = (byte *) malloc(incoming.len);
				p[0] = incoming.id;
				memcpy(p + 1, incoming.data, incoming.len - 1);

				wardenc_engine(p, incoming.len);

				free(p);
			}
			if (setting("Verbose")->b_var) {
				print("\n");
			}

			ui_console_unlock();
			break;
		}
		}

		invoke_packet_handlers(D2GS_RECEIVED, (void *) &incoming);

		if (incoming.data) {
			free(incoming.data);
		}
	}

	net_shutdown(d2gs_socket); // TODO: we might want to ensure not to call shutdown twice

	internal_send(D2GS_ENGINE_MESSAGE, "%d", MODULES_CLEANUP);

	cleanup_modules(MODULE_D2GS);

	clear_module_schedule(MODULE_D2GS);

	pthread_join(d2gs_ping_tid, NULL);

	d2gs_disconnect();

	internal_send(D2GS_ENGINE_MESSAGE, "%d", ENGINE_DISCONNECTED);

	if (errors) {
		error("[D2GS] %i errors while processing packets\n", errors);
	}

	free(info);

	internal_send(D2GS_ENGINE_MESSAGE, "%d", ENGINE_SHUTDOWN);

	pthread_exit(NULL);
}
