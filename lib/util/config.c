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

#include <autoopts/options.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/config.h"
#include "util/error.h"
#include "util/string.h"

int compare_setting(struct setting *s, char *name) {
	return !strcmp(s->name, name);
}

struct setting * config_get_setting(struct setting_section *s, char *name) {
	int i;
	for (i = 0; i < s->entries; i++) {
		if (!strcmp(s->settings[i].name, name)) {
			return s->settings + i;
		}
	}
	return NULL;
}

static void process_section(const tOptionValue *set, void (*process_setting_section)(
		struct setting_section *)) {
	tOptionValue *recursive = NULL;
	int n_recursive = 0;

	const tOptionValue *nested, *n_nested;
	struct setting_section s;

	s.name = strdup(set->pzName);
	s.settings = NULL;
	s.entries = 0;
	n_nested = optionGetValue(set, NULL);
	if (n_nested) {
		do {
			nested = n_nested;
			if (nested->valType == OPARG_TYPE_HIERARCHY) {
				recursive = realloc(recursive, ++n_recursive * sizeof(tOptionValue));
				memcpy(recursive + n_recursive - 1, nested, sizeof(tOptionValue));
			} else {
				s.entries++;
				s.settings = realloc(s.settings, s.entries * sizeof(struct setting));
				s.settings[s.entries - 1].name = strdup(nested->pzName);
				if (nested->valType == OPARG_TYPE_STRING) {
					s.settings[s.entries - 1].s_var = strdup(nested->v.strVal);
					s.settings[s.entries - 1].type = STRING;
				} else if (nested->valType == OPARG_TYPE_NUMERIC) {
					s.settings[s.entries - 1].i_var = nested->v.longVal;
					s.settings[s.entries - 1].type = INTEGER;
				} else if (nested->valType == OPARG_TYPE_BOOLEAN) {
					s.settings[s.entries - 1].b_var = nested->v.boolVal;
					s.settings[s.entries - 1].type = BOOLEAN;
				} else if (nested->valType == OPARG_TYPE_ENUMERATION) {
					s.settings[s.entries - 1].e_var = nested->v.enumVal;
					s.settings[s.entries - 1].type = ENUMERATION;
				} else if (nested->valType == OPARG_TYPE_MEMBERSHIP) {
					s.settings[s.entries - 1].m_var = nested->v.setVal;
					s.settings[s.entries - 1].type = MEMBERSHIP;
				}
			}
		} while ((n_nested = optionNextValue(set, nested)));
	}
	if (s.settings) {
		process_setting_section(&s);
	}
	if (s.settings) {
		int i;
		for (i = 0; i < s.entries; i++) {
			if (s.settings[i].name) {
				free(s.settings[i].name);
			}
			if (s.settings[i].type == STRING && s.settings[i].s_var) {
				free(s.settings[i].s_var);
			}
		}
		free(s.settings);
	}
	free(s.name);
	int i;
	for (i = 0; i < n_recursive; i++) {
		process_section(recursive + i, process_setting_section);
	}
	if (recursive) {
		free(recursive);
	}
}

bool config_load_settings(const char *file, void(*process_setting_section)(
		struct setting_section *)) {
	const tOptionValue *pov = configFileLoad(file);
	if (pov) {
		process_section(pov, process_setting_section);
		optionUnloadNested(pov);
		return TRUE;
	} else {
		return FALSE;
	}
}
