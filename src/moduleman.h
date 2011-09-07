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

#ifndef MODULE_MANAGER_H_
#define MODULE_MANAGER_H_

#include "module.h"

#include <util/types.h>

#include <pthread.h>

typedef void (*extension_interface_t)(char *, ...);

typedef struct {
	char *name;
	extension_interface_t call;
} extension_t;

// exported
pthread_mutex_t schedule_update_m;

// exported
pthread_cond_t schedule_update_cond_v;

typedef void (*module_routine_t)();

void init_module_manager();

void finit_module_manager();

void load_modules(const char *);

void unload_modules();

void start_modules(int);

void cleanup_modules(int);

bool is_module_thread();

// exported
void block_modules(int);

// exported
void unblock_modules(int);

void module_wait(int);

// exported
void schedule_module_routine(int, module_routine_t);

// exported
void execute_module_schedule(int);

// exported
void clear_module_schedule(int);

// exported
void register_extension(char *, extension_interface_t);

// exported
extension_t * extension(char *);

#endif /* MODULE_MANAGER_H_ */
