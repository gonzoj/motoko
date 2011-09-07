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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "wutil.h"

extern char *wardenc_dir;

void logmessage(const char *format, ...) {
	char *logfile = strconcat(wardenc_dir, "wardenc.log");
	FILE *f = fopen(logfile, "a");
	if (f) {
		va_list args;
		va_start(args, format);
		vfprintf(f, format, args);
		va_end(args);
		fclose(f);
	}
	free(logfile);
}

char * strtoupper(char *s) {
	int i;
	for (i = 0; i < strlen(s) && (s[i] >= 'a' && s[i] <= 'z'); i++) {
		s[i] -=  32;
	}
	return s;
}

char * strconcat(char *dir, char *file) {
	char *s = (char *) malloc(strlen(dir) + strlen(file) + 1
			+ ((dir[strlen(dir) - 1] == '/') ? 0 : 1));

	strcpy(s, dir);
	if (dir[strlen(dir) - 1] != '/') {
		strcat(s, "/");
	}
	strcat(s, file);

	return s;
}

char * strhexname(byte *name) {
	char *s = (char *) malloc(0x10 * 2 + 5);

	int i;
	for (i = 0; i < 0x10; i++) {
		sprintf(s + i * 2, "%02X", name[i]);
	}
	strcat(s, ".mod");

	return s;
}

char * readline(FILE *file) {
	char *line = NULL;

	int i = 0;
	while (!feof(file)) {
		if (i % 512 == 0) {
			line = (char *) realloc(line, i + 512);
			if (line == NULL) {
				return NULL;
			}
		}

		char chr = fgetc(file);
		if (chr == EOF) {
			line[i] = '\0';
			break;
		}
		if (chr == '\n') {
			line[i] = '\0';
			break;
		}
		line[i++] = chr;
	}

	return line;
}
