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

#ifndef MODULE_H_
#define MODULE_H_

#include <util/config.h>
#include <util/types.h>

// you need to export the interface in case you set the default visibility to 'hidden' (which you should)
#ifdef _PLUGIN
	#define _export __attribute__((visibility("default")))
#endif

typedef struct {
	enum {
		MODULE_BNCS = 0, MODULE_MCP = 1, MODULE_D2GS = 2
	} engine;
	enum {
		MODULE_ACTIVE, MODULE_PASSIVE
	} type;
} module_type_t;

#define LICENSE_GPL_V3 0x42

/* interface implemented by modules */

extern const char * module_get_title();

extern const char * module_get_version();

extern const char * module_get_author();

extern const char * module_get_description();

// by returning LICENSE_GPL_V3 you confirm that your module is licensed under the GPL v3
extern int module_get_license();

extern module_type_t module_get_type();

extern bool module_load_config(struct setting_section *);

extern bool module_init();

extern bool module_finit();

extern void * module_thread(void *);

extern void module_cleanup();

#endif /* MODULE_H_ */
