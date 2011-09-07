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

#include <openssl/rc4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wardenc.h"
#include "wcrypt.h"
#include "winapi.h"
#include "wmodule.h"
#include "wtypes.h"
#include "wutil.h"

wardenc_callbacks *callbacks;

char *wardenc_dir;

/* the built-in method for analysing warden responses  */

char **wchecks = NULL;
int n_wchecks = 0;

void wardenc_sniff_request(char *check) {
	int i;
	for (i = 0; i < n_wchecks; i++) {
		if (!strcmp(strchr(check, ':'), strchr(wchecks[i], ':'))) {
			return;
		}
	}
	wchecks = (char **) realloc(wchecks, ++n_wchecks * sizeof(char *));
	wchecks[n_wchecks - 1] = (char *) malloc(strlen(check) + 1);
	strcpy(wchecks[n_wchecks - 1], check);
}

void wardenc_parse_response(byte *packet, size_t size) {
	// dump request + resonse
	
	static int j = 0;

	char *file = strconcat(wardenc_dir, "wrequest.dump");
	FILE *f = fopen(file, "a");

	while (j < n_wchecks) {
		int i = 7;
		char dump[1024] = { 0 };
		strcat(dump, strchr(wchecks[j], ':') + 1);

		int pos;
		char wcheck[512] = { 0 };
		memcpy(wcheck, wchecks[j], strchr(wchecks[j], ':') - wchecks[j]);
		sscanf(wcheck, "%i", &pos);

		char response[512] = { 0 };
		if (strstr(wchecks[j], "mem")) {
			char tmp[strlen(wchecks[j]) + 1];
			strcpy(tmp, wchecks[j]);
			int bytes = 0;
			char *t1 = strrchr(tmp, ':') + 1;
			char *t2 = strtok(t1, " ");
			sscanf(t2, "%i", &bytes);
			sprintf(response, "%02X", packet[i++ + pos]);
			int m;
			for (m = 0; m < bytes; m++) {
				sprintf(response + 2 + m * 2, "%02X", packet[i++ + pos]);
			}
		} else if (strstr(wchecks[j], "mpq")) {
			sprintf(response, "%02X", packet[i++ + pos]);
			int k;
			for (k = 0; k < 20; k++) {
				sprintf(response + 2 + k * 2, "%02X", packet[i++ + pos]);
			}
		} else if (strstr(wchecks[j], "page")) {
			sprintf(response, "%02X", packet[i++ + pos]);
		}

		strcat(dump, ":");
		strcat(dump, response);
		fprintf(f, "%s\n", dump);

		j++;
	}

	fclose(f);
}

/* functions for handling 0x02 requests */

static char **wsignatures = NULL;
static int n_wsignatures = 0;

void wardenc_load_signatures(char *dir) {
	char *file = strconcat(dir, "wrequest.db");
	FILE *f = fopen(file, "r");
	if (!f) {
		free(file);
		return;
	}
	char *line;
	while ((line = readline(f))) {
		wsignatures = (char **) realloc(wsignatures, ++n_wsignatures * sizeof(char *));
		wsignatures[n_wsignatures - 1] = (char *) malloc(strlen(line) + 1);
		strcpy(wsignatures[n_wsignatures - 1], line);
		free(line);
	}
	fclose(f);
	free(file);
}

char * wardenc_match_signature(char *sig) {
	int i;
	for (i = 0; i < n_wsignatures; i++) {
		if (strstr(wsignatures[i], sig)) {
			return strrchr(wsignatures[i], ':') + 1;
		}
	}
	return NULL;
}

/* the built-in method only supports three different checks for now */

/* memory scan */
char * wardenc_check_mem(byte *packet, size_t rem, char **libs, int n_libs, byte k, bool sniff, int *pos) {
	if (rem >= 7) {
		if (packet[1] < n_libs && packet[1] > 0) {
			if (strstr(libs[packet[1]], ".dll") && DWORD(packet, 2) < 0x00100000 && packet[6] < 0x40) {
				
				if (sniff) {
					logmessage("%02X ", packet[0] ^ k);
					logmessage("memcheck: %s + 0x%X (%i bytes)\n", libs[packet[1]], DWORD(packet, 2), packet[6]);

					char check[512];
					sprintf(check, "%i:mem:%s:%X:%i", *pos, libs[packet[1]], DWORD(packet, 2), packet[6]);
					*pos += packet[6] + 1;
					wardenc_sniff_request(check);
					return "";
				}

				char cmp[512] = { 0 };
				sprintf(cmp, "mem:%s:%X:%i", libs[packet[1]], DWORD(packet, 2), packet[6]);
				return wardenc_match_signature(cmp);
			}
		}
	}
	return NULL;
}

/* page check */
char * wardenc_check_page(byte *packet, size_t rem, byte k, bool sniff, int *pos) {
	if (rem >= 30) {
		if (packet[28] == 0 && packet[27] <= 0x40 && packet[29] <= 0x80) {
				
			if (sniff) {
				logmessage("%02X ", packet[0] ^ k);
				logmessage("pagecheck: seed: %08X address: 0x%08X (%i bytes)\n", DWORD(packet, 1), DWORD(packet, 25), packet[29]);

				char check[512];
				sprintf(check, "%i:page:%08X:%i", *pos, DWORD(packet, 25), packet[29]);
				*pos += 1;
				wardenc_sniff_request(check);
				return "";
			}

			char cmp[512] = { 0 };
			sprintf(cmp, "page:%08X:%i", DWORD(packet, 25), packet[29]);
			return wardenc_match_signature(cmp);
		}
	}
	return NULL;
}

/* MPQ file check (SHA-1) */
char * wardenc_check_mpq(byte *packet, size_t rem, char **libs, int n_libs, byte k, bool sniff, int *pos) {
	if (rem >= 2) {
		if (packet[1] < n_libs && packet[1] > 0) {
			if (strstr(libs[packet[1]], ".txt") || strstr(libs[packet[1]], ".D2")) {
				
				if (sniff)  {
					logmessage("%02X ", packet[0] ^ k);
					logmessage("mpqcheck: %s\n", libs[packet[1]]);

					char check[512];
					sprintf(check, "%i:mpq:%s", *pos, libs[packet[1]]);
					*pos += 21;
					wardenc_sniff_request(check);
					return "";
				}

				char cmp[512] = { 0 };
				sprintf(cmp, "mpq:%s", libs[packet[1]]);
				return wardenc_match_signature(cmp);
			}
		}
	}
	return NULL;
}

/* the built-in function to handle 0x02 request, a simple signature lookup basically. */
byte * wardenc_parse_request(byte *packet, size_t len, size_t *n_response, bool sniff) {
	byte *response = NULL;
	*n_response = 0;

	int i = 1, j;
	char **libs = (char **) malloc(sizeof(char *));
	int n_libs = 1;
	byte k = packet[len - 1];

	while (packet[i++] != 0) {
		libs = (char **) realloc(libs, ++n_libs * sizeof(char *));
		libs[n_libs - 1] = (char *) malloc(packet[i - 1] + 1);
		memset(libs[n_libs - 1], 0, packet[i - 1] + 1);
		memcpy(libs[n_libs - 1], packet + i, packet[i - 1]);
		i += packet[i - 1];
	}

	if (sniff) {
		for (j = 1; j < n_libs; j++) {
			logmessage("%s\n", libs[j]);
		}
		logmessage("\n");
	}

	int r = 0;
	int err = 0,suc = i;
	while (i < len - 1) {
		char *ret;
		if ((ret = wardenc_check_mem(packet + i, len - i, libs, n_libs, k, sniff, &r))) {
			i += 7;
			suc += 7;

			response = (byte *) realloc(response, *n_response + strlen(ret) / 2);
			int m;
			for (m = 0; m < strlen(ret) / 2; m++) {
				unsigned b1;
				sscanf(ret + m * 2, "%02X", (dword *) (&b1));
				response[*n_response + m] = b1;
			}
			*n_response += strlen(ret) / 2;

		} else if ((ret = wardenc_check_mpq(packet + i, len - i, libs, n_libs, k, sniff, &r))) {
			i += 2;
			suc += 2;

			response = (byte *) realloc(response, *n_response + strlen(ret) / 2);
			int m;
			for (m = 0; m < strlen(ret) / 2; m++) {
				unsigned b1;
				sscanf(ret + m * 2, "%02X", (dword *) (&b1));
				response[*n_response + m] = b1;
			}
			*n_response += strlen(ret) / 2;

		} else if ((ret = wardenc_check_page(packet + i, len - i, k, sniff, &r))) {
			i += 30;
			suc += 30;

			response = (byte *) realloc(response, *n_response + strlen(ret) / 2);
			int m;
			for (m = 0; m < strlen(ret) / 2; m++) {
				unsigned b1;
				sscanf(ret + m * 2, "%02X", (dword *) (&b1));
				response[*n_response + m] = b1;
			}
			*n_response += strlen(ret) / 2;

		} else {
			err++;
			i++;
		}
	}
	suc++;
	if (sniff) {
		logmessage("\nfailed to parse %i bytes\n", err);
		logmessage("successfully parsed %i / %i bytes\n\n", suc, len);
	}

	for (j = 1; j < n_libs; j++) {
		free(libs[j]);
	}
	free(libs);

	if (!err) {
		int hash[5];
		wsha1(response, *n_response, hash);
		response = (byte *) realloc(response, *n_response + 7);
		byte tmp11[512];
		memcpy(tmp11, response, *n_response);
		memcpy(response + 7, tmp11, *n_response);
		response[0] = 0x02;
		*(word *)&response[1] = (*n_response) & 0xFFFF;
		*(dword *)&response[3] = hash[0] ^ hash[1] ^ hash[2] ^ hash[3] ^ hash[4];
		*n_response += 7;
	} else {
		free(response);
		response = NULL;
		*n_response = 0;
	}

	return response;
}

byte * wardenc_engine_parse_request(byte *packet, size_t len, size_t *n_response) {
	return wardenc_parse_request(packet, len, n_response, FALSE);
}

byte * wardenc_sniff_parse_request(byte *packet, size_t len, size_t *n_response) {
	return wardenc_parse_request(packet, len, n_response, TRUE);
}

/* the actual Warden client */

typedef struct {
	byte name[0x10];
	byte key[0x10];
	struct {
		size_t total;
		size_t recvd;
	} size;
	byte *raw;
	byte *mod;
} wmodule;

static wmodule wmod;

#define wardenc_dump_packet(p, l) if (callbacks->dump_packet) callbacks->dump_packet(p, l)

_export void wardenc_init(wardenc_callbacks *cb, char *dir) {
	callbacks = cb;
	wardenc_dir = dir;

	logmessage("\n--- new session ---\n");

	if (cb->get_d2gs_hash == NULL) {
		logmessage("error: passed callback to NULL for get_d2gs_hash\n");
	}
	/*if (cb->parse_request == NULL) { // fall-back to built-in method
		callbacks->parse_request = (parse_request_t) wardenc_parse_request;
	}*/
	if (cb->parse_response == NULL) {
		callbacks->parse_response = (parse_response_t) wardenc_parse_response;
	}
	if (cb->get_proc_address == NULL) {
		callbacks->get_proc_address = (get_proc_address_t) winapi_get_proc;
	}
	wardenc_load_signatures(dir);
}

/* current D2GS hash received in [MCP] S -> C 0x04 */
static dword d2gs_hash;

/*static char byte_to_char(byte b) {
	return (b >= 127 || b < 32) ? '.' : (char) b;
}

void wardenc_dump_packet(void *packet, size_t len) {
	int i;
	for (i = 0; i < len; i += 8) {
		int end = (len - i < 8) ? len - i : 8;
		int j;
		for (j = i; j < i + end; j++) {
			wardenc_print("%02x ", ((byte *) packet)[j]);
		}
		for (j = 8; j > end; j--) {
			wardenc_print("   ");
		}
		wardenc_print("   ");
		for (j = i; j < i + end; j++) {
			wardenc_print("%c", byte_to_char(((byte *) packet)[j]));
		}
		wardenc_print("\n");
	}
}*/

/* strips the D2GS packet header off */
byte * strip_header(byte *packet, size_t *len) {
	*len = *(word *)&packet[1];
	byte *p = (byte *) malloc(*len);
	memcpy(p, packet + 3, *len);
	return p;
}

/* inserts D2GS header (packet ID, size of payload) */
byte * append_header(byte id, byte *payload, size_t len) {
	byte *packet = malloc(len + 3);
	packet[0] = id;
	*(word *)&packet[1] = len & 0xFFFF;
	memcpy(packet + 3, payload, len);
	return packet;
}

void wardenc_send(byte *payload, size_t len) {
	wcrypt_out(payload, len);
	byte *packet = append_header(0x66, payload, len);
	if (callbacks->send_packet) callbacks->send_packet(packet, len + 3);
	else logmessage("error: passed callback to NULL for send_packet\n");
	free(packet);
}

void wardenc_on_received(byte *incoming, size_t len, bool sniff) {
	if (callbacks->get_d2gs_hash == NULL) {
		return; // stupid user hasn't specified get_d2gs_hash
	}

	if (callbacks->get_d2gs_hash() != d2gs_hash) {

		/* generate a new pair of RC4 keys if the D2GS hash changed */
		d2gs_hash = callbacks->get_d2gs_hash();
		wcrypt_keys((byte *) &d2gs_hash, sizeof(dword));
	}

	size_t size;
	byte *packet;
	packet = strip_header(incoming, &size);

	// wardenc_dump_packet(incoming, len);

	wcrypt_in(packet, size);

	switch (packet[0]) {

	case 0x00: {

		byte *p = append_header(incoming[0], packet, size);
		wardenc_dump_packet(p, len);
		free(p);

		wfunc = NULL;

		memcpy(wmod.name, packet + 1, 0x10);
		memcpy(wmod.key, packet + 17, 0x10);
		memcpy(&wmod.size.total, packet + 33, 4);

		/* load the decprypted and decompressed module from disk if possible, otherwise... */
		if (wmodule_check_diskcache(wardenc_dir, wmod.name)) {

			wmod.raw = wmodule_load_from_disk(wardenc_dir, wmod.name, &wmod.size.total);

			/* decompress the decrypted module */
			size_t size_unc;
			byte *mod_unc;
			if ((mod_unc = wmodule_decompress(wmod.raw + 4, wmod.size.total - 0x108, &size_unc))) {
				free(wmod.raw);
			} else {

				/* module seems to be corrupted */
				char *m = strhexname(wmod.name);
				logmessage("error: failed to decompress cached module %s\n", m);
				free(m);
				free(wmod.raw);
				break;
			}

			wmod.mod = wmodule_load(mod_unc);
			if (!wmod.mod) {

				/* failed to load module */
				char *m = strhexname(wmod.name);
				logmessage("error: failed to load cached module %s\n", m);
				free(m);
				free(mod_unc);
				break;
			}

			free(mod_unc);

			wfunc = wmodule_init(wmod.mod);
			if (!wfunc) {
				char *m = strhexname(wmod.name);
				logmessage("error: failed to initialize cached module %m\n", m);
				free(m);
			}

			if (!sniff) {
				/* signal the server we have the module cached on disk */
				wardenc_send((byte[]) { 0x01 }, 1);
			}

			break;
		}

		wmod.raw = (byte *) malloc(wmod.size.total);
		wmod.size.recvd = 0;

		if (!sniff) {
			/* the server has to send us the module */
			wardenc_send((byte[]) { 0x00 }, 1);
		}

		break;
	}

	case 0x01: {

		byte *p = append_header(incoming[0], packet, size);
		wardenc_dump_packet(p, len);
		free(p);

		/* append to buffer for the raw module */
		size_t len = 0;
		memcpy(&len, packet + 1, 2);
		memcpy(wmod.raw + wmod.size.recvd, packet + 3, len);
		wmod.size.recvd += len;

		/* the transmission is complete */
		if (wmod.size.recvd == wmod.size.total) {

			/* validate the received module */
			if (!wmodule_validate_encrypted(wmod.raw, wmod.size.total, wmod.name)) {
				char *m = strhexname(wmod.name);
				logmessage("error: invalid encrypted module %s received\n");
				free(m);

				if (!sniff) {
					/* request a new transmission */
					wardenc_send((byte[]) { 0x00 }, 1);
				}

				free(wmod.raw);

				break;
			} else {
				if (!sniff) {
					/* everything OK */
					wardenc_send((byte[]) { 0x01 }, 1);
				}
			}

			/* decrypt the received module */
			RC4_KEY rc4key;
			RC4_set_key(&rc4key, 0x10, wmod.key);
			RC4(&rc4key, wmod.size.total, wmod.raw, wmod.raw);

			/* validate the decrypted module */
			if (!wmodule_validate_decrypted(wmod.raw, wmod.size.total, "MAIEV.MOD")) {

				/* we shouldn't get here */
				char *m = strhexname(wmod.name);
				logmessage("error: invalid decrypted module %s received\n");
				free(m);
				free(wmod.raw);
				break;
			}

			/* save the decrypted but still compressed module to disk */
			wmodule_save_to_disk(wardenc_dir, wmod.raw, wmod.name, wmod.size.total);

			/* decompress the decrypted module */
			size_t size_unc;
			byte *mod_unc;
			if ((mod_unc = wmodule_decompress(wmod.raw + 4, wmod.size.total - 0x108, &size_unc))) {
				free(wmod.raw);
			} else {

				/* module seems to be corrupted */
				char *m = strhexname(wmod.name);
				logmessage("error: failed to decompress module %s\n", m);
				free(m);
				free(wmod.raw);
				break;
			}

			/* and prepare it */
			wmod.mod = wmodule_load(mod_unc);
			if (!wmod.mod) {

				/* failed to load module */
				char *m = strhexname(wmod.name);
				logmessage("error: failed to load module %s\n", m);
				free(m);
				free(mod_unc);
				break;
			}

			free(mod_unc);

			/* finally initialize the module */
			wfunc = wmodule_init(wmod.mod);

			if (!wfunc) {
				char *m = strhexname(wmod.name);
				logmessage("error: failed to initialize module %s\n", m);
				free(m);
			}
		}
		break;
	}

	case 0x02: {

		/* we can only handle this packet if the module was initialized correctly */
		if (wfunc && *wfunc) {

			byte *p = append_header(incoming[0], packet, size);
			wardenc_dump_packet(p, len);
			free(p);

			if (sniff && new_RC4keys) {
				memcpy(wkey.out, new_RC4keys, 0x102);
				wmodule_unload(wfunc);
				free(wmod.mod);
			}

			size_t n_bytes;
			byte *response = callbacks->parse_request(packet, size, &n_bytes);
			if (response) {
				if (!sniff) {
					wardenc_send(response, n_bytes);
				}

				if (sniff) {
					byte tmpkey[0x102];
					memcpy(tmpkey, wkey.out, 0x102);
					wrc4_apply(tmpkey, 0x100, response, n_bytes);
				}
			} else {
				/* we couldn't generate a response to this request */
				logmessage("error: failed to generate response for following request:\n\n");
				int i;
				for (i = 0; i < size; i++) {
					logmessage("%02x ", packet[i]);
				}
				logmessage("\n");
			}
		}
		break;
	}

	case 0x03: {

		byte *p = append_header(incoming[0], packet, size);
		wardenc_dump_packet(p, len);
		free(p);

		break;
	}

	case 0x05: {
		if (wfunc) {

			byte *p = append_header(incoming[0], packet, size);
			wardenc_dump_packet(p, len);
			free(p);

			wmodule_init_RC4keys(wfunc, d2gs_hash);

			/* save the module's current RC4 keys */
			byte tmp_inkey[0x102];
			memcpy(tmp_inkey, new_RC4keys + 0x102, 0x102);

			byte tmp_outkey[0x102];
			memcpy(tmp_outkey, new_RC4keys, 0x102);

			/* encrypt the packet with the module's RC4 key */
			wrc4_apply(tmp_inkey, 0x100, packet, size);

			/* pass the packet to the module */
			 wmodule_handle_packet(wfunc, packet, size);

			/* decrypt the response with the module's RC4 key */
			wrc4_apply(tmp_outkey, 0x100, wresponse, wresponse_size);

			if (!sniff) {
				wardenc_send(wresponse, wresponse_size);
			}

			/* save the generated RC4 keys */
			memcpy(wkey.in, new_RC4keys + 0x102, 0x102);

			if (!sniff) {
				memcpy(wkey.out, new_RC4keys, 0x102);

				/* unload the module and free shit up */
				wmodule_unload(wfunc);
				free(wmod.mod);
			}
		}
	}
	}

	free(packet);
}

_export void wardenc_engine(byte *packet, size_t len) {
	if (callbacks->parse_request == NULL) { // fall-back to built-in method
		callbacks->parse_request = (parse_request_t) wardenc_engine_parse_request;
	}
	wardenc_on_received(packet, len, FALSE);
}

_export void wardenc_sniff_on_received(byte *packet, size_t len) {
	if (callbacks->parse_request == NULL) { // fall-back to built-in method
		callbacks->parse_request = (parse_request_t) wardenc_sniff_parse_request;
	}
	wardenc_on_received(packet, len, TRUE);
}

_export void wardenc_sniff_on_sent(byte *outgoing, size_t len) {
	size_t size;
	byte *packet = strip_header(outgoing, &size);

	wcrypt_out(packet, size);

	if (!packet[0] == 0x02 || wfunc) {
		byte *p = append_header(outgoing[0], packet, size);
		wardenc_dump_packet(p, len);
		free(p);
	}

	callbacks->parse_response(packet, size);

	free(packet);
}
