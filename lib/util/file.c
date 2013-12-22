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

#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/error.h"
#include "util/file.h"
#include "util/string.h"

size_t file_get_size(const char *file) {
	struct stat buf;
	if (stat(file, &buf)) {
		char *e;
		string_new(&e, "failed to get size of file ", file, " (", strerror(
				errno), ")", "");
		err_set(e);
		free(e);
		return -1;
	}
	return (size_t) buf.st_size;
}

size_t file_read(const char *file, byte *buf, size_t len) {
	char *e;

	int fd = open(file, O_RDONLY);
	if (fd < 0) {
		string_new(&e, "failed to open file ", file, " (", strerror(errno), ")", "");
		err_set(e);
		free(e);
		return -1;
	}
	int tmp, total = 0;
	do {
		tmp = read(fd, buf + total, len - total);
		if (tmp < 0) {
			string_new(&e, "failed to read from file ", file, " (", strerror(
					errno), ")", "");
			err_set(e);
			free(e);
			return -1;
		}
		total += tmp;
	} while (tmp > 0);
	if (close(fd)) {
		string_new(&e, "failed to close file ", file, " (", strerror(errno), ")", "");
		err_set(e);
		free(e);
		return -1;
	}
	return total;
}

size_t file_write(const char *file, byte *buf, size_t len) {
	char *e;

	int fd = open(file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		string_new(&e, "failed to open file ", file, " (", strerror(errno), ")", "");
		err_set(e);
		free(e);
		return -1;
	}
	int tmp, total = 0;
	do {
		tmp = write(fd, buf + total, len - total);
		if (tmp < 0) {
			string_new(&e, "failed to write to file ", file, " (", strerror(
					errno), ")", "");
			err_set(e);
			free(e);
			return -1;
		}
		total += tmp;
	} while (tmp > 0);
	if (close(fd)) {
		string_new(&e, "failed to close file ", file, " (", strerror(errno), ")", "");
		err_set(e);
		free(e);
		return -1;
	}
	return total;
}

size_t file_dump(const char *file, char *format, ...) {
	size_t w = 0;
	FILE *f = fopen(file, "r");

	if (f) {
		va_list args;
		va_start(args, format);
		w = vfprintf(f, format, args);
		va_end(args);

		fclose(f);
	}

	return w;
}

size_t file_copy(const char *file, const char *copy) {
	int fd;
	size_t size = file_get_size(file);
	if ((int) size > 0) {
		byte *buf = malloc(size);
		size_t read = file_read(file, buf, size);
		if ((int) read > 0) {
			size = file_write(copy, buf, read);
		} else {
			size = -1;
		}
		free(buf);
	} else if (size == 0) {
		fd = open(file, O_WRONLY);
		close(fd);
	}
	return size;
}

char * file_get_absolute_path(const char *dir, const char *file) {
	char *abs_path;
	
	abs_path = (char *) malloc(MAX_PATH * sizeof(char));

	strncpy(abs_path, dir, MAX_PATH);
	if (abs_path[strlen(dir) - 1] != '/') {
		strcat(abs_path, "/");
	}
	strcat(abs_path, file);

	return abs_path;
}
