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

#include "wcrypt.h"

/* Warden's SHA-1 Implementation */

typedef struct {
	int bitlen[2];
	int state[32];
} wsha1_ctx;

#define reverse_endianess(dword) (((((unsigned) (dword)) & 0x000000FF) << 24) | ((((unsigned) (dword)) & 0x0000FF00) << 8) | ((((unsigned) (dword)) & 0x00FF0000) >> 8) | ((((unsigned) (dword)) & 0xFF000000) >> 24))

#define rotate_left(dword, n) ((((unsigned) (dword)) << (n)) | (((unsigned) (dword)) >> (32 - (n))))

#define wsha1_tweedle(dword, rot, add, b1, b2, b3) dword = dword + rotate_left((b3), 5) + ((~rot & (b2)) | (rot & (b1))) + add + 0x5A827999; add = 0; rot = rotate_left(rot, 30);

#define wsha1_twitter(dword, rot1, rot2, rot3, b1, b2) dword = dword + ((((b1) | (b2)) & rot1) | ((b2) & (b1))) + rotate_left((rot2), 5) + rot3 - 0x70E44324; rot3 = 0; rot1 = rotate_left(rot1, 30);

void wsha1_init(wsha1_ctx *ctx) {
	ctx->bitlen[0] = 0;
	ctx->bitlen[1] = 0;
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
}

void wsha1_transform(int *data, int *state) {
	unsigned W[80];
	unsigned A, B, C, D, E, F, G, H, M, N;
	int i;

	for (i = 0; i < 16; i++) {
		data[i] = reverse_endianess(data[i]);
	}

	memcpy(W, data, 64);

	for (i = 0; i < 64; i++) {
		W[i + 16] = rotate_left(W[i + 13] ^ W[i + 8] ^ W[i + 0] ^ W[i + 2], 1);
	}

	M = state[0];
	B = state[1];
	C = state[2];
	N = state[3];
	E = state[4];

	for (i = 0; i < 20; i += 5) {
		wsha1_tweedle(E, B, W[i + 0], C, N, M)
		wsha1_tweedle(N, M, W[i + 1], B, C, E)
		wsha1_tweedle(C, E, W[i + 2], M, B, N)
		wsha1_tweedle(B, N, W[i + 3], E, M, C)
		wsha1_tweedle(M, C, W[i + 4], N, E, B)
	}

	F = M;
	D = N;

	for (i = 20; i < 40; i += 5) {
		G = W[i] + rotate_left(F, 5) + (D ^ C ^ B);
		D = D + rotate_left(G + E + 0x6ED9EBA1, 5) + (C ^ rotate_left(B, 30) ^ F) + W[i + 1] + 0x6ED9EBA1;
		C = C + rotate_left(D, 5) + ((G + E + 0x6ED9EBA1) ^ rotate_left(B, 30) ^ rotate_left(F, 30)) + W[i + 2] + 0x6ED9EBA1;
		E = rotate_left(G + E + 0x6ED9EBA1, 30);
		B = rotate_left(B, 30) + rotate_left(C, 5) + (E ^ D ^ rotate_left(F, 30)) + W[i + 3] + 0x6ED9EBA1;
		D = rotate_left(D, 30);
		F = rotate_left(F, 30) + rotate_left(B, 5) + (E ^ D ^ C) + W[i + 4] + 0x6ED9EBA1;
		C = rotate_left(C, 30);

		// according to the skullsecurity wiki we should memset(W, 0, 20) here...
	}

	M = F;
	N = D;

	for (i = 40; i < 60; i += 5) {
		wsha1_twitter(E, B, M, W[i + 0], N, C)
		wsha1_twitter(N, M, E, W[i + 1], C, B)
		wsha1_twitter(C, E, N, W[i + 2], B, M)
		wsha1_twitter(B, N, C, W[i + 3], M, E)
		wsha1_twitter(M, C, B, W[i + 4], E, N)
	}

	F = M;
	A = M;
	D = N;

	for (i = 60; i < 80; i += 5) {
		G = rotate_left(A, 5) + (D ^ C ^ B) + W[i + 0] + E - 0x359D3E2A;
		B = rotate_left(B, 30);
		E = G;
		D = (C ^ B ^ A) + W[i + 1] + D + rotate_left(G, 5) - 0x359D3E2A;
		A = rotate_left(A, 30);
		G = rotate_left(D, 5);
		G = (E ^ B ^ A) + W[i + 2] + C + G - 0x359D3E2A;
		E = rotate_left(E, 30);
		C = G;
		G = rotate_left(G, 5) + (E ^ D ^ A) + W[i + 3] + B - 0x359D3E2A;
		D = rotate_left(D, 30);
		H = (E ^ D ^ C) + W[i + 4];
		B = G;
		G = rotate_left(G, 5);
		C = rotate_left(C, 30);
		A = H + A + G - 0x359D3E2A;

		// this should be unnecessary

		/*
		W[i + 0] = 0;
		W[i + 1] = 0;
		W[i + 2] = 0;
		W[i + 3] = 0;
		W[i + 4] = 0;
		*/
	}

	state[0] += A;
	state[1] += B;
	state[2] += C;
	state[3] += D;
	state[4] += E;
}

void wsha1_update(wsha1_ctx *ctx, byte *data, int len) {
	int A, B, C;
	int i;
	byte *state = (byte *) ctx->state;

	if (len >= 64) {
		for (i = 0; i < len; i += 63) {
			wsha1_update(ctx, data + i, ((len - i < 63) ? len - i : 63));
		}
	} else {
		C = len >> 29;
		B = len << 3;
		A = (ctx->bitlen[0] / 8) & 0x3F;

		if (ctx->bitlen[0] + B < ctx->bitlen[0] || ctx->bitlen[0] + B < B) {
			ctx->bitlen[1]++;
		}
		ctx->bitlen[0] += B;
		ctx->bitlen[1] += C;

		len += A;
		data -= A;

		if (len >= 64) {
			if (A) {
				while (A < 64) {
					state[20 + A] = data[A];
					A++;
				}

				wsha1_transform((int *) (state + 20), (int *) state);
				len -= 64;
				data += 64;
				A = 0;
			}

			if (len >= 64) {
				B = len;
				for (i = 0; i < B / 64; i++) {
					wsha1_transform((int *) data, (int *) state);
					len -= 64;
					data += 64;
				}
			}
		}

		for (; A < len; A++) {
			state[A + 0x1C - 8] = data[A];
		}
	}
}

void wsha1_final(wsha1_ctx *ctx, int *hash) {
	byte padding[] = {
			0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00
	};
	int var[2];
	int len;
	int i;

	var[0] = reverse_endianess(ctx->bitlen[1]);
	var[1] = reverse_endianess(ctx->bitlen[0]);

	len = ((-9 - (ctx->bitlen[0] >> 3)) & 0x3F) + 1;

	wsha1_update(ctx, padding, len);
	wsha1_update(ctx, (byte *) var, 8);

	for (i = 0; i < 5; i++) {
		hash[i] = reverse_endianess(ctx->state[i]);
	}
}

void wsha1(byte *data, size_t len, int *hash) {
	wsha1_ctx ctx;

	wsha1_init(&ctx);
	wsha1_update(&ctx, data, len);
	wsha1_final(&ctx, hash);
}

/* Warden's code for generating a pool of random data (used for RC4 keys) */

typedef struct {
	int pos;
	byte data[0x14];
	byte source1[0x14];
	byte source2[0x14];
} wrandom_t;

void wrandom_update(wrandom_t *source) {
	wsha1_ctx ctx;

	wsha1_init(&ctx);
	wsha1_update(&ctx, source->source1, 0x14);
	wsha1_update(&ctx, source->data, 0x14);
	wsha1_update(&ctx, source->source2, 0x14);
	wsha1_final(&ctx, (int *) source->data);
}

void wrandom_init(wrandom_t *source, byte *seed, int len) {
	memset(source, 0, sizeof(wrandom_t));

	wsha1(seed, len >> 1, (int *) source->source1);
	wsha1(seed + (len >> 1), (len - (len >> 1)), (int *) source->source2);
	wrandom_update(source);

	source->pos = 0;
}

void wrandom_generate(wrandom_t *source, size_t len, byte *buf) {
	int i;

	for (i = 0; i < len; i++) {
		buf[i] = source->data[source->pos++];
		if (source->pos >= 0x14) {
			source->pos = 0;
			wrandom_update(source);
		}
	}
}

/* Warden's XOR encryption (RC4) */

#define swap(x, y) if ((x) != (y)) { (x) ^= (y); (y) ^= (x); (x) ^= (y); }

void wrc4_generate_key(byte *base, size_t len, byte *key) {
	byte val = 0;
	int pos = 0;
	int i;

	for (i = 0; i < 0x100; i++) {
		key[i] = i;
	}
	key[0x100] = 0;
	key[0x101] = 0;

	for (i = 1; i <= 64; i++) {
		val += key[(i * 4) - 4] + base[pos++ % len];
		swap(key[(i * 4) - 4], key[val & 0x0FF]);

		val += key[(i * 4) - 3] + base[pos++ % len];
		swap(key[(i * 4) - 3], key[val & 0x0FF]);

		val += key[(i * 4) - 2] + base[pos++ % len];
		swap(key[(i * 4) - 2], key[val & 0x0FF]);

		val += key[(i * 4) - 1] + base[pos++ % len];
		swap(key[(i * 4) - 1], key[val & 0x0FF]);
	}
}

void wrc4_apply(byte *key, size_t key_len, byte *data, size_t len) {
	int i;
	for (i = 0; i < len; i++) {
		key[key_len]++;
		key[key_len + 1] += key[key[key_len]];
		swap(key[key[key_len + 1]], key[key[key_len]]);

		data[i] ^= key[(key[key[key_len + 1]] + key[key[key_len]]) & 0x0FF];
	}
}

wrc4_key wkey;

void wcrypt_keys(byte *seed, size_t len) {
	wrandom_t source;
	byte base[0x10];

	wrandom_init(&source, seed, len);

	wrandom_generate(&source, 0x10, base);
	wrc4_generate_key(base, 0x10, wkey.out);

	wrandom_generate(&source, 0x10, base);
	wrc4_generate_key(base, 0x10, wkey.in);
}
