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

#ifndef CONFIG_H_
#define CONFIG_H_

#include "util/types.h"

struct setting {
	char *name;
	union {
		long i_var;
		char *s_var;
		bool  b_var;
		unsigned int e_var;
		unsigned long m_var;
	};
	enum {
		INTEGER, STRING, BOOLEAN, ENUMERATION, MEMBERSHIP
	} type;
};

#define SETTING(n, v, t) { n, { v }, t }

struct setting_section {
	char *name;
	struct setting *settings;
	int entries;
};

int compare_setting(struct setting *, char *);

struct setting * config_get_setting(struct setting_section *, char *);

bool config_load_settings(const char *, void(*process_setting_section)(struct setting_section *));

#endif /* CONFIG_H_ */
