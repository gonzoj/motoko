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

#ifndef WCRYPT_H_
#define WCRYPT_H_

#include "wtypes.h"

typedef struct {
	/* decryption */
	byte in[0x102];
	/* encryption */
	byte out[0x102];
} wrc4_key;

/* stores the current set of RC4 keys */
extern wrc4_key wkey;

/* Warden's SHA-1 implementation */
void wsha1(byte *data, size_t len, int *hash);

/* generates RC4 keys from seed */
void wcrypt_keys(byte *seed, size_t len);

/* applies Warden's RC4 algorithm */
void wrc4_apply(byte *key, size_t key_len, byte *data, size_t len);

/* decrypts incoming packets with the current key */
#define wcrypt_in(data, len) wrc4_apply(wkey.in, 0x100, data, len)

/* encrypts outgoing packets with the current key */
#define wcrypt_out(data, len) wrc4_apply(wkey.out, 0x100, data, len)

#endif /* WCRYPT_H_ */
