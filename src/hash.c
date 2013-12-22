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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hash.h"

#include <util/error.h>
#include <util/file.h>
#include <util/string.h>

#define cat_bytes(b, o) ((b[o * 4] & 0xFF) | (b[o * 4 + 1] & 0xFF) << 8 | (b[o * 4 + 2] & 0xFF) << 16 | (b[o * 4 + 3] & 0xFF) << 24)

#define sha1_circular_shift(bits, word) ((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (0x20 - (bits))))

static void bsha1_process_block(unsigned *digest, unsigned char *buf) {
	const unsigned K[] = {
			0x5A827999,
	        0x6ED9EBA1,
	        0x8F1BBCDC,
	        0xCA62C1D6
	};
	unsigned W[80];
	unsigned A, B, C, D, E;
	int i;
	for (i = 0; i < 16; i++) {
		W[i] = cat_bytes(buf, i);
	}
	for (i = 16; i < 80; i++) {
		W[i] = sha1_circular_shift(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1); /* this is just retarded */
	}
	A = digest[0];
	B = digest[1];
	C = digest[2];
	D = digest[3];
	E = digest[4];
	for (i = 0; i < 20; i++) {
		unsigned tmp = sha1_circular_shift(5, A) + ((B & C) | ((~B) & D)) + E + W[i] + K[0];
		tmp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = sha1_circular_shift(30, B);
		B = A;
		A = tmp;
	}
	for (i = 20; i < 40; i++) {
		unsigned tmp = sha1_circular_shift(5, A) + (B ^ C ^ D) + E + W[i] + K[1];
		tmp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = sha1_circular_shift(30, B);
		B = A;
		A = tmp;
	}
	for (i = 40; i < 60; i++) {
		unsigned tmp = sha1_circular_shift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[i] + K[2];
		tmp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = sha1_circular_shift(30, B);
		B = A;
		A = tmp;
	}
	for (i = 60; i < 80; i++) {
		unsigned tmp = sha1_circular_shift(5, A) + (B ^ C ^ D) + E + W[i] + K[3];
		tmp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = sha1_circular_shift(30, B);
		B = A;
		A = tmp;
	}
	digest[0] = (digest[0] + A) & 0xFFFFFFFF;
	digest[1] = (digest[1] + B) & 0xFFFFFFFF;
	digest[2] = (digest[2] + C) & 0xFFFFFFFF;
	digest[3] = (digest[3] + D) & 0xFFFFFFFF;
	digest[4] = (digest[4] + E) & 0xFFFFFFFF;
}

static void bsha1(const char *message, size_t len, byte *hash) {
	unsigned digest[] = {
		    0x67452301,
		    0xEFCDAB89,
		    0x98BADCFE,
		    0x10325476,
		    0xC3D2E1F0

	};
	unsigned char buf[64];
	int i;
	for (i = 0; i < len; i += 64) {
		memset(buf, 0, 64); /* blizzard seems not to pad the block (0x80 0x00 ... 0x00 64 bit length) */
		int t_len = (len - i < 64) ? len -i : 64;
		memcpy(buf, message + i, t_len);
		bsha1_process_block(digest, buf);
	}
	memcpy(hash, digest, 5 * sizeof(unsigned));
}

static const byte alpha_map[] =
{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x00, 0xff, 0x01, 0xff, 0x02, 0x03,
	0x04, 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
	0x0c, 0xff, 0x0d, 0x0e, 0xff, 0x0f, 0x10, 0xff,
	0x11, 0xff, 0x12, 0xff, 0x13, 0xff, 0x14, 0x15,
	0x16, 0xff, 0x17, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
	0x0c, 0xff, 0x0d, 0x0e, 0xff, 0x0F, 0x10, 0xff,
	0x11, 0xff, 0x12, 0xff, 0x13, 0xff, 0x14, 0x15,
	0x16, 0xff, 0x17, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

#define hex_digit(byte) ((byte) & 0xf) < 10 ? 0x30 + ((byte) & 0xf) : 0x37 + ((byte) & 0xf)

#define hex_byte(digit) (digit >= '0' && digit <= '9' ? digit - 0x30 : digit - 0x37)

#define to_upper_case(c) (c -= (c >= 'a' && c <= 'z') ? 32 : 0)

_export bool hash_cdkey(const char *cdkey, dword client_token, dword server_token, byte *hash, dword *public) {
	dword checksum = 0, valid = 3;
	char mutated_key[strlen(cdkey) + 1];
	strcpy(mutated_key, cdkey);
	int i;
	for (i = 0; i < strlen(cdkey); i++) {
		dword x = ((dword) alpha_map[(int) cdkey[i]]) * 8 * 3 + (dword) alpha_map[(int) cdkey[++i]];
		if (x >= 0x100) {
			x -= 0x100;
			checksum |= 1 << (i >> 1);
		}
		mutated_key[--i] = hex_digit(x >> 4);
		mutated_key[++i] = hex_digit(x);

		valid += hex_byte(mutated_key[i - 1]) ^ (valid * 2);
		valid += hex_byte(mutated_key[i]) ^ (valid * 2);
	}
	if ((valid & 0xff) != checksum) {
		err_set("invalid CD key");
		return FALSE;
	}
	for (i = 15; i >= 0; i--) {
		char c = mutated_key[i];
		int j = ((i > 8) ? (i - 9) : (0xf - (8 - i))) & 0xf;
		mutated_key[i] = mutated_key[j];
		mutated_key[j] = c;
	}
	int v = 0x13ac9741;
	for (i = 15; i >= 0; i--) {
		/* maybe we need to force to upper */
		char c = to_upper_case(mutated_key[i]); // i guess not needed ?
		mutated_key[i] = c;
		if (c <= '7') {
			mutated_key[i] = (((char) (v & 0xff)) & 0x7) ^ c;
			v >>= 3;
		} else if (c < 'A') {
			mutated_key[i] = (((char) i) & 0x1) ^ c;
		}
	}
	dword product, value;
	sscanf(mutated_key, "%2X%6X%8X", &product, public, &value);
	dword hash_data[] = { client_token, server_token, product, *public, 0x00000000, value };
	bsha1((char *) hash_data, 24, hash);
	return TRUE;
}

_export bool hash_passwd(const char *passwd, dword client_token, dword server_token, byte *hash) {
	// TODO: I think we have to convert pass to lower case
	byte pass_hash[20];
	bsha1(passwd, strlen(passwd), pass_hash);
	char hash_data[20 + 2 * sizeof(dword)];
	*(dword *)&hash_data[0] = client_token;
	*(dword *)&hash_data[sizeof(dword)] = server_token;
	memcpy(hash_data + 2 * sizeof(dword), pass_hash, 20);
	bsha1(hash_data, 20 + 2 * sizeof(dword), hash);
	return TRUE;
}

static bool extract_vars(char *vars, dword *a, dword *b, dword *c) {
	int i = 0;
	char *tok = strtok(vars, " ");
	while (tok && i < 3 && strlen(tok) > 2) {
		dword num;
		sscanf(tok + 2, "%u", &num);
		switch (*tok) {
		case 'A':
			*a = num;
			break;
		case 'B':
			*b = num;
			break;
		case 'C':
			*c = num;
			break;
		default: {
			char *e;
			string_new(&e, "unknown variable ", *tok, "");
			err_set(e);
			free(e);
			return FALSE;
		}
		}
		tok = strtok(NULL, " ");
		i++;
	}
	return i == 3 ? TRUE : FALSE;
}

typedef dword (*operator)(dword, dword);

static dword op_add(dword x, dword y) {
	return x + y;
}

static dword op_sub(dword x, dword y) {
	return x - y;
}

static dword op_xor(dword x, dword y) {
	return x ^ y;
}

static bool extract_ops(char *ops, operator *operators) {
	int i = 0;
	char *tok = strtok(ops, " ");
	while (tok && i < 4 && strlen(tok) == 5) {
		switch (tok[3]) {
		case '+':
			operators[i++] = op_add;
			break;
		case '-':
			operators[i++] = op_sub;
			break;
		case '^':
			operators[i++] = op_xor;
			break;
		default: {
			char *e;
			string_new(&e, "unknown operator ", tok[3], "");
			err_set(e);
			free(e);
			return FALSE;
		}
		}
		tok = strtok(NULL, " ");
	}
	return i == 4 ? TRUE : FALSE;
}

static const dword mpq_hash_codes[] =
{
		0xE7F4CB62,
		0xF6A14FFC,
		0xAA5504AF,
		0x871FCDC2,
		0x11BF6A18,
		0xC57292E6,
		0x7927D27E,
		0x2FEC8733
};

_export bool hash_executable(const char *bin_dir, const char *formula, const char *mpq_file, int *checksum) {

	char *files[] = { "Game.exe", "Bnclient.dll", "D2Client.dll" };

	char form_copy[strlen(formula) + 1];
	strcpy(form_copy, formula);
	char file_copy[strlen(mpq_file) + 1];
	strcpy(file_copy, mpq_file);
	operator operators[4];
	dword a = 0, b = 0, c = 0;
	char *vars;
	char *ops;
	char *separator = strstr(form_copy, " 4 ");
	if (!separator) {
		err_set("invalid formula format");
		return FALSE;
	}
	*separator = '\0';
	vars = form_copy;
	ops = separator + 3;
	if (ops > form_copy + strlen(formula)) {
		err_set("invalid formula format");
		return FALSE;
	}
	if (!extract_vars(vars, &a, &b, &c) || !extract_ops(ops, operators)) {
		return FALSE;
	}
	char *mpq_index = strchr(file_copy, '.');
	if (!mpq_index && strlen(file_copy) != 14) {
		char *e;
		string_new(&e, "invalid mpq file ", mpq_file, "");
		err_set(e);
		free(e);
		return FALSE;
	}
	*mpq_index = '\0';
	int index = atoi(mpq_index - 1);
	if (index < 0 || index > 7) {
		err_set("invalid mpq version");
		return FALSE;
	}
	a ^= mpq_hash_codes[index];
	int i;
	for (i = 0; i < 3; i++) {
		char *file = file_get_absolute_path(bin_dir, files[i]);
		size_t len = file_get_size(file);
		if ((int) len < 0) {
			free(file);
			return FALSE;
		}
		byte *buf = malloc(len);
		size_t total = file_read(file, buf, len);
		if ((int) total < 0) {
			free(buf);
			free(file);
			return FALSE;
		}
		int j;
		for (j = 0; j < (int) total; j += sizeof(dword)) {
			dword s = *(dword *)&buf[j];
			a = operators[0](a, s);
			b = operators[1](b, c);
			c = operators[2](c, a);
			a = operators[3](a, b);
		}
		free(buf);
		free(file);
	}
	*checksum = c;
	return TRUE;
}
