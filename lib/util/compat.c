/*
 * Copyright (C) 2013 gonzoj
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

#include <config.h>

#ifndef HAVE_CLOCK_GETTIME

#include <sys/time.h>
#include <time.h>

#ifndef HAVE_CLOCKID_T
#include "util/compat.h"
#endif

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
	struct timeval tv;
	int r = gettimeofday(&tv, NULL);
	if (r == 0) {
		tp->tv_sec = tv.tv_sec;
		tp->tv_nsec = tv.tv_usec * 1000;
	}
	return r;
}

#endif
