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

#ifndef WUTIL_H_
#define WUTIL_H_

#include <stdio.h>

#include "wtypes.h"

void logmessage(const char *, ...);

/* converts to upper case until the first non-alphabetic character */
char * strtoupper(char *s);

/* used to concatenate paths, inserts '/' if necessary */
char * strconcat(char *dir, char *file);

/* converts a module name to a string and appends .mod */
char * strhexname(byte *name);

/* reads until NL / EOF, yeah! */
char * readline(FILE *file);

#endif /* WUTIL_H_ */
