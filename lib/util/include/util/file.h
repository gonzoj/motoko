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

#ifndef FILE_H_
#define FILE_H_

#include <unistd.h>

#include <limits.h>
#ifndef MAX_PATH
	#define MAX_PATH 4096
#endif

#include "util/types.h"

size_t file_get_size(const char *);

size_t file_read(const char *, byte *, size_t);

size_t file_read(const char *, byte *, size_t);

size_t file_dump(const char *file, char *format, ...);

size_t file_copy(const char *, const char *);

char * file_get_absolute_path(const char *, const char *);

#endif /* FILE_H_ */
