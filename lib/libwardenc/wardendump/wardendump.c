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

#include <dlfcn.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wardenc.h"

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

pid_t pid;

int opt_all = 0;
int opt_silent = 0;
char opt_dir[MAX_PATH] = ".";
char opt_module[MAX_PATH] = "";
void *h_module = NULL;
unsigned *opt_filter = NULL;
int n_opt_filter = 0;
char opt_packet_handler[MAX_PATH] = "";
void *h_packet_handler = NULL;

typedef void (*packet_handler_t)(int, unsigned char *, size_t);
packet_handler_t packet_handler = NULL;


#define OFFSET(type, member) ((size_t) &((type *)0)->member)

enum {
	DR0, DR1, DR2, DR3, DR4, DR5, DR6, DR7
};

enum {
	EX = 0, W = 1, RW = 3
};

enum {
	BYTE, WORD, UNDEFINED, DWORD
};

enum {
	STDCALL, FASTCALL
};

long bitmask[] = {
		0xFFF0FFFC,
		0xFF0FFFF3,
		0xF0FFFFCF,
		0x0FFFFF3F
};

#define WARDEN_REQUEST 0xAE
#define WARDEN_RESPONSE 0x66

/* version 1.13c

#define D2CLIENT_RECEIVE_PACKET 0x6FB5CE61
#define D2CLIENT_SEND_PACKET 0x6FABD252
#define D2CLIENT_D2GS_HASH 0x6FBC9924
#define D2WIN_FIRST_CONTROL 0x6F9014A0

*/

/* version 1.13d */

#define D2CLIENT_RECEIVE_PACKET 0x6FB33301
#define D2CLIENT_SEND_PACKET 0x6FABD13C
#define D2CLIENT_D2GS_HASH 0x6FB9B890
#define D2WIN_FIRST_CONTROL 0x6F96DB34

unsigned D2CLIENT_get_hash() {
	return (unsigned) ptrace(PTRACE_PEEKDATA, pid, D2CLIENT_D2GS_HASH, NULL);
}

void * D2WIN_get_first_control() {
	return (void *) ptrace(PTRACE_PEEKDATA, pid, D2WIN_FIRST_CONTROL, NULL);
}

void send_packet_dummy(unsigned char *packet, size_t len) {
	return;
}

#define BYTE_TO_CHAR(b) ((b >= 127 || b < 32) ? '.' : (char) b)

void dump_packet(unsigned char *packet, size_t len) {
	int i;
	for (i = 0; i < len; i += 8) {
		int end = (len - i < 8) ? len - i : 8;
		int j;
		for (j = i; j < i + end; j++) {
			printf("%02x ", (unsigned) (packet[j]));
		}
		for (j = 8; j > end; j--) {
			printf("   ");
		}
		printf("   ");
		for (j = i; j < i + end; j++) {
			printf("%c", BYTE_TO_CHAR((packet)[j]));
		}
		printf("\n");
	}
}

int is_packet_filtered(unsigned char *packet) {
	if (!opt_filter) {
		return 1;
	}
	int i;
	for (i = 0; i < n_opt_filter; i++) {
		if (packet[0] == (opt_filter[i] & 0xFF)) {
			return 1;
		}
	}
	return 0;
}

wardenc_callbacks imports = {
		D2CLIENT_get_hash,
		send_packet_dummy,
		NULL,
		NULL,
		dump_packet,
		NULL
};

int set_hw_breakpoint(unsigned regnum, void *addr, size_t len, unsigned cond) {
	if (regnum > DR3) {
		return -1;
	}

	int status = 0;

	errno = 0;

	long control = ptrace(PTRACE_PEEKUSER, pid, OFFSET(struct user, u_debugreg[DR7]), 0);
	if (errno) {
		return -1;
	}
	control |= 1 << (2 * regnum) | (cond & 0x3) << (16 + 4 * regnum) | (len & 0x3) << (18 + 4 * regnum);

	status |= ptrace(PTRACE_POKEUSER, pid, OFFSET(struct user, u_debugreg[regnum]), addr);
	status |= ptrace(PTRACE_POKEUSER, pid, OFFSET(struct user, u_debugreg[DR7]), control);

	return status;
}

int clear_hw_breakpoint(unsigned regnum) {
	if (regnum > DR3) {
		return -1;
	}

	int status = 0;

	errno = 0;

	long control = ptrace(PTRACE_PEEKUSER, pid, OFFSET(struct user, u_debugreg[DR7]), 0);
	if (errno) {
		return -1;
	}
	control &= bitmask[regnum];

	status |= ptrace(PTRACE_POKEUSER, pid, OFFSET(struct user, u_debugreg[regnum]), 0);
	status |= ptrace(PTRACE_POKEUSER, pid, OFFSET(struct user, u_debugreg[DR7]), control);

	return status;
}

int get_debug_condition() {
	return ptrace(PTRACE_PEEKUSER, pid, OFFSET(struct user, u_debugreg[DR6]), NULL) & 0xF;
}

int clear_debug_condition() {
	return ptrace(PTRACE_POKEUSER, pid, OFFSET(struct user, u_debugreg[DR6]), 0);
}

int read_process_memory(void *addr, size_t len, unsigned char *buf) {
	size_t read, diff;

	diff = len % sizeof(long);

	for (read = 0; read < (len / sizeof(long)) * sizeof(long) + diff; read += sizeof(long)) {
		errno = 0;

		long dword = ptrace(PTRACE_PEEKDATA, pid, addr + read, NULL);
		memcpy(buf + read, &dword, (read > len) ? diff : sizeof(long));

		if (errno) {
			return -1;
		}
	}

	return 0;
}

int retrieve_arguments(int argc, long *argv, int callconv) {
	struct user_regs_struct regs;
	if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) {
		return -1;
	}

	switch (callconv) {

	case STDCALL: {
		int i;
		for (i = 0; i < argc; i++) {
			if (read_process_memory((void *) regs.esp + sizeof(long) + i * sizeof(long), sizeof(long), (unsigned char *) argv + i * sizeof(long)) < 0) {
				return -1;
			}
		}
		break;
	}

	case FASTCALL: {
		int i;
		for (i = 0; i < argc; i++) {
			if (i == 0) {
				argv[i] = regs.ecx;
			} else if (i == 1) {
				argv[i] = regs.edx;
			} else {
				if (read_process_memory((void *) regs.esp + sizeof(long) + (i - 2) * sizeof(long), sizeof(long), (unsigned char *) argv + i * sizeof(long)) < 0) {
					return -1;
				}
			}
		}
		break;
	}

	default:
		return -1;

	}

	return 0;
}

void debug_exception_handler() {
	int event = get_debug_condition();

	switch(event) {

	/* D2CLIENT_RECEIVE_PACKET (DR0) */
	case 1: {

		unsigned char *packet;
		size_t len;

		long args[2];
		if (retrieve_arguments(2, args, FASTCALL)) {
			break;
		}

		len = args[1];
		packet = malloc(len);
		if (read_process_memory((void *) args[0], len, packet)) {
			free(packet);
			break;
		}

		if (packet[0] == WARDEN_REQUEST) {
			if (!is_packet_filtered(packet) || opt_silent) {
				imports.dump_packet = NULL;
			} else {
				imports.dump_packet = dump_packet;
				printf("[S -> C]:\n");
			}

			// call plugin
			wardenc_sniff_on_received(packet, len);

			if (is_packet_filtered(packet) && !opt_silent) {
				printf("\n");
			}

		} else if ((opt_all || (opt_filter && is_packet_filtered(packet))) && !opt_silent) {

			printf("[S -> C]:\n");
			dump_packet(packet, len);
			printf("\n");
		}

		if (packet_handler) packet_handler(event, packet, len);

		free(packet);

		break;
	}

	/* D2CLIENT_SEND_PACKET (DR1) */
	case 2: {

		size_t len;
		unsigned arg;
		unsigned char *packet;

		long args[3];
		if (retrieve_arguments(3, args, STDCALL)) {
			break;
		}

		len = args[0];
		arg = args[1];
		packet = malloc(len);
		if (read_process_memory((void *) args[2], len, packet)) {
			free(packet);
			break;
		}

		if (packet[0] == WARDEN_RESPONSE) {
			if (!is_packet_filtered(packet) || opt_silent) {
				imports.dump_packet = NULL;
			} else {
				imports.dump_packet = dump_packet;
				printf("[C -> S]:\n");
			}

			// call plugin
			wardenc_sniff_on_sent(packet, len);

			if (is_packet_filtered(packet) && !opt_silent) {
				printf("\n");
			}

		} else if ((opt_all || (opt_filter && is_packet_filtered(packet))) && !opt_silent) {

			printf("[C -> S]:\n");
			dump_packet(packet, len);
			printf("\n");
		}

		if (packet_handler) packet_handler(event, packet, len);

		free(packet);

		break;
	}

	default:
		break;

	}

	clear_debug_condition();
}

int parse_arguments(int argc, char **argv) {
	struct option long_options[] = 
	{
		{ "help", no_argument, 0, 'h' },
		{ "all", no_argument, 0, 'a' },
		{ "silent", no_argument, 0, 's' },
		{ "dir", required_argument, 0, 'd' },
		{ "module", required_argument, 0, 'm' },
		{ "filter", required_argument, 0, 'f' },
		{ "packet-handler", required_argument, 0, 'p' },
		{ 0 }
	};

	while (optind < argc) {
		int index = -1;
		int result = getopt_long(argc, argv, "hasd:m:f:p:", long_options, &index);
		if (result == -1) {
			break;
		}

		switch (result) {
			
			case 'h':
			return 0;

			case 'a':
			opt_all = 1;
			break;

			case 's':
			opt_silent = 1;
			break;

			case 'd':
			strncpy(opt_dir, optarg, MAX_PATH);
			break;

			case 'm':
			if (optarg[0] != '/') {
				if (!getcwd(opt_module, MAX_PATH)) {
					break;
				}
			}
			strcat(opt_module, "/");
			strncat(opt_module, optarg, MAX_PATH - strlen(opt_module) - 1);
			void *h_module = dlopen(opt_module, RTLD_LAZY);
			if (h_module) {
				imports.parse_request = dlsym(h_module, "parse_request");
				imports.parse_response = dlsym(h_module, "parse_response");
				imports.get_proc_address = dlsym(h_module, "get_proc_address");
				//dlclose(h);
			} else {
				printf("error: failed to load module %s, using built-in methods instead.\n", opt_module);
			}
			break;

			case 'f': {
			char *t = strtok(optarg, ":");
			while (t) {
				opt_filter = realloc(opt_filter, (n_opt_filter + 1) * sizeof(unsigned));
				sscanf(t, "%x", opt_filter + n_opt_filter++);
				t = strtok(NULL, ":");
			}
			break;
			}

			case 'p': {
			if (optarg[0] != '/') {
				if (!getcwd(opt_packet_handler, MAX_PATH)) {
					break;
				}
			}
			strcat(opt_packet_handler, "/");
			strncat(opt_packet_handler, optarg, MAX_PATH - strlen(opt_packet_handler) - 1);
			void *h_packet_handler = dlopen(opt_packet_handler, RTLD_LAZY);
			if (h_packet_handler) {
				packet_handler = dlsym(h_packet_handler, "packet_handler");
			} else {
				printf("error: failed to load module %s, no packet redirecting.\n", opt_packet_handler);
			}
			break;
			}

			case '?':
			printf("error: unknown parameter.\n");
			return 0;

			case ':':
			printf("error: missing argument.\n");
			return 0;

			default:
			return 0;

		}
	}

	if (argc - optind < 1) {
		return 0;
	} else {
		sscanf(argv[optind], "%i", (int *) &pid);
	}
	return 1;
}

void usage() {
	printf("usage:\n");
	printf("\n");
	printf("wardenump [OPTIONS] PID\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("	-h, --help\n");
	printf("		show this help\n");
	printf("\n");
	printf("	-a, --all\n");
	printf("		dump all packets\n");
	printf("\n");
	printf("        -s, --silent\n");
	printf("		do not dump any packets at all\n");
	printf("\n");
	printf("	-d DIRECTORY, --dir=DIRECTORY\n");
	printf("		init the warden engine with DIRECTORY\n");
	printf("\n");
	printf("	-m MODULE, --module=MODULE\n");
	printf("		init the warden engine with functions from MODULE\n");
	printf("\n");
	printf("	-f EXP, --filter=EXP\n");
	printf("		dump only packets with ID contained in EXP (IDs (hexadecimal) separated with ':')\n");
	printf("\n");
	printf("        -p MODULE, --packet-handler=MODULE\n");
	printf("                redirect packets to the handler function from MODULE\n");
}

int main(int argc, char **argv) {
	if (!parse_arguments(argc, argv)) {
		usage();
		exit(EXIT_FAILURE);
	}

	int status = 0;

	int child;

	if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
		printf("error: failed to attach to process %i. exiting.\n", pid);
		exit(EXIT_FAILURE);
	}

	// init plugin
	wardenc_init(&imports, opt_dir);

	while (!status) {
		status |= wait(&child) < 0 ? -1 : 0;

		status |= set_hw_breakpoint(DR0, (void *) D2CLIENT_RECEIVE_PACKET, BYTE, EX);
		status |= set_hw_breakpoint(DR1, (void *) D2CLIENT_SEND_PACKET, BYTE, EX);

		status |= ptrace(PTRACE_CONT, pid, NULL, NULL);

		status |= wait(&child) < 0 ? -1 : 0;

		debug_exception_handler();

		status |= clear_hw_breakpoint(DR0);
		status |= clear_hw_breakpoint(DR1);

		status |= ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
	}

	ptrace(PTRACE_DETACH, pid, NULL, NULL);

	if (opt_filter) free(opt_filter);

	if (h_module) dlclose(h_module);
	if (h_packet_handler) dlclose(h_packet_handler);

	exit(EXIT_SUCCESS);
}
