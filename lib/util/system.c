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
#include <sys/times.h>
#include <time.h>

#include "util/system.h"

clock_t system_get_clock_ticks() {
	struct tms buf;
	return times(&buf);
}

char * system_uptime() {
	static time_t start = 0;
	if (!start) {
		start = time(NULL);
	}
	int uptime = (int) difftime(time(NULL), start);
	int h, m, s;
	h = uptime / 3600;
	m = (uptime - (h * 3600)) / 60;
	s = uptime - (h * 3600) - (m * 60);
	char buf[512];
	snprintf(buf, 512, "%02i:%02i:%02i", h, m, s);
	char *str = (char *) malloc(strlen(buf) + 1);
	strcpy(str, buf);
	return str;
}

int system_sh(const char *cmd) {
	if (!cmd || !strlen(cmd)) return -1;
	return system(cmd);
}
