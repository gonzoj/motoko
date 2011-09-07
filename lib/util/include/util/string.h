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

#ifndef STRING_H_
#define STRING_H_

#include <unistd.h>

#include "util/types.h"

void string_random(unsigned, char, unsigned, char *);

size_t string_to_byte(const char *, byte *);

char * string_to_lower_case(char *);

char * string_to_upper_case(char *);

void string_split_lines(char *, int *);

char * string_new(char **, ...);

bool string_compare(char *, char *, bool);

bool string_is_numeric(char *);

char * string_format_time(int);

#endif /* STRING_H_ */
