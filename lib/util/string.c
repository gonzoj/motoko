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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util/string.h"

void string_random(unsigned len, char base, unsigned range, char *seq) {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	srand(t.tv_nsec);
	int i;
	for (i = 0; i < len; i++) {
		unsigned offset = (unsigned) ((double) rand() / (((double) RAND_MAX + (double) 1)
				/ (double) range));
		seq[i] = base + offset;
	}
	seq[len] = '\0';
}

size_t string_to_byte(const char *string, byte *bytes) {
	char copy[strlen(string) + 1];
	strcpy(copy, string);
	int i = 0;
	char *tok = strtok(copy, " ");
	while (tok != NULL) {
		int b;
		sscanf(tok, "%x", &b);
		bytes[i++] = (byte) (b & 0xff);
		tok = strtok(NULL, " ");
	}
	return (size_t) i;
}

char * string_to_lower_case(char *s) {
	int i;
	for (i = 0; i < strlen(s); i++) {
		s[i] += (s[i] >= 'A' && s[i] <= 'Z') ? 32 : 0;
	}
	return s;
}

char * string_to_upper_case(char *s) {
	int i;
	for (i = 0; i < strlen(s); i++) {
		s[i] -= (s[i] >= 'a' && s[i] <= 'z') ? 32 : 0;
	}
	return s;
}

void string_split_lines(char *s, int *n) {
	if (!s) {
		return;
	}
	*n = 1;
	while ((s = strchr(s, '\n'))) {
		*s++ = '\0';
		*n += 1;
	}
}

char * string_new(char **str, ...) {
	*str = NULL;
	int len = 0;


	va_list l;
	va_start(l, str);

	while (TRUE) {
		char *s = va_arg(l, char *);
		if (s == NULL) {
			break;
		}
		if (strlen(s) == 0) {
			break;
		}

		*str = realloc(*str, len + strlen(s) + 1);
		strcpy(*str + len, s);
		len += strlen(s);
	}

	va_end(l);

	return *str;
}

bool string_compare(char *a, char *b, bool case_sensitive) {
	if (case_sensitive) {
		return !strcmp(a, b);
	} else if (strlen(a) == strlen(b)) {
		int i;
		for (i = 0; i < strlen(a); i++) {
			if (a[i] >= 'a' && a[i] <= 'z') {
				if (a[i] != b[i] && a[i] != b[i] + 0x20) {
					return FALSE;
				}
			} else if (a[i] >= 'A' && a[i] <= 'Z') {
				if (a[i] != b[i] && a[i] != b[i] - 0x20) {
					return FALSE;
				}
			} else if (a[i] != b[i]) {
				return FALSE;
			}
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

bool string_is_numeric(char *s) {
	int i;
	for (i = 0; i < strlen(s); i++) {
		if (s[i] < '0' || s[i] > '9') {
			return FALSE;
		}
	}

	return TRUE;
}

char * string_format_time(int seconds) {
	char buf[512];

	int h, m, s;

	h = seconds / 3600;
	m = (seconds - (h * 3600)) / 60;
	s = seconds - (h * 3600) - (m * 60);

	if (h > 0) {
		snprintf(buf, 512, "%02ih %02im %02is", h, m, s);
	} else if (m > 0) {
		snprintf(buf, 512, "%02im %02is", m, s);
	} else {
		snprintf(buf, 512, "%02is", s);
	}

	return strdup(buf);
}

