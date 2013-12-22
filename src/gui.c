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

// for PTHREAD_MUTEX_RECURSIVE
#define _GNU_SOURCE

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <util/curses.h>

//#define INCLUDE_NCURSES
#include <util/types.h>
//#undef INCLUDE_NCURSES

#include <util/string.h>
#include <util/system.h>

#include "gui.h"

#include "clientman.h"
#include "bncs.h"
#include "d2gs.h"
#include "mcp.h"

#include "settings.h"

#include "moduleman.h"

static bool ui_shutdown = FALSE;

static bool ui_resized = FALSE;

static pthread_mutex_t console_mutex;

static char *logfile = NULL;

static char *ui_statistics = NULL;

_export void ui_console_lock() {
	pthread_mutex_lock(&console_mutex);
}

_export void ui_console_unlock() {
	pthread_mutex_unlock(&console_mutex);
}

static void ui_wprint(WINDOW *w, int c, char *format, ...) {

	if (has_colors()) {
		wattron(w, COLOR_PAIR(c + 1));
	}

	va_list args;
	va_start(args, format);
	vwprintw(w, format, args);
	va_end(args);

	wrefresh(w);
}

_export void ui_print(int c, char *format, ...) {
	pthread_mutex_lock(&console_mutex);

	va_list args;
	va_start(args, format);

	vprintf(format, args);

	va_end(args);

	// log this shit
	if (setting("Logging")->b_var) {
		FILE *log = fopen(logfile, "a");
		if (log) {
			va_start(args, format);
			vfprintf(log, format, args);
			va_end(args);
			fclose(log);
		}
	}

	/*if (has_colors()) {
		wattron(w, COLOR_PAIR(c + 1));
	}

	va_list args;
	va_start(args, format);

	vwprintw(w, format, args);

	if (setting("Logging")->b_var) {
		FILE *log = fopen(logile, "a");
		if (log) {
			vfprintf(log, format, args);
			fclose(log);
		}
	}

	va_end(args);

	wrefresh(w);*/

	pthread_mutex_unlock(&console_mutex);
}

_export void ui_print_plugin(int c, char *p, char *format, ...) {
	pthread_mutex_lock(&console_mutex);

	printf("[plugin %s] ", p);

	va_list args;
	va_start(args, format);

	vprintf(format, args);

	va_end(args);

	if (setting("Logging")->b_var) {
		FILE *log = fopen(logfile, "a");
		if (log) {
			fprintf(log, "[plugin %s] ", p);
			va_start(args, format);
			vfprintf(log, format, args);
			va_end(args);
			fclose(log);
		}
	}

	pthread_mutex_unlock(&console_mutex);
}

_export void ui_print_debug(int c, char *format, ...) {
	if (!setting("Debug")->b_var) {
		return;
	}

	pthread_mutex_lock(&console_mutex);

	printf("DEBUG: ");

	va_list args;
	va_start(args, format);

	vprintf(format, args);

	va_end(args);

	if (setting("Logging")->b_var) {
		FILE *log = fopen(logfile, "a");
		if (log) {
			fprintf(log, "DEBUG: ");
			va_start(args, format);
			vfprintf(log, format, args);
			va_end(args);
			fclose(log);
		}
	}

	pthread_mutex_unlock(&console_mutex);
}

_export void ui_print_debug_plugin(int c, char *p, char *format, ...) {
	if (!setting("Debug")->b_var) {
		return;
	}

	pthread_mutex_lock(&console_mutex);

	printf("DEBUG: [plugin %s] ", p);

	va_list args;
	va_start(args, format);

	vprintf(format, args);

	va_end(args);

	if (setting("Logging")->b_var) {
		FILE *log = fopen(logfile, "a");
		if (log) {
			fprintf(log, "DEBUG: [plugin %s] ", p);
			va_start(args, format);
			vfprintf(log, format, args);
			va_end(args);
			fclose(log);
		}
	}

	pthread_mutex_unlock(&console_mutex);
}

_export bool ui_register_cmd(char *cmd, ui_cmd_t handler) {
	return FALSE;
}

_export bool ui_unregister_cmd(char *cmd, ui_cmd_t handler) {
	return FALSE;
}

void ui_invoke_cmd(char *input) {
	
}

_export void ui_add_statistics(char *format, ...) {
	bool empty = ui_statistics ? FALSE : TRUE;

	char buf[512];

	va_list args;
	va_start(args, format);

	vsnprintf(buf, 512, format, args);

	va_end(args);

	buf[511] = '\0';

	ui_statistics = (char *) realloc(ui_statistics, (empty ? 0 : strlen(ui_statistics)) + strlen(buf) + 1);

	if (empty) {
		strcpy(ui_statistics, buf);
	} else {
		strcat(ui_statistics, buf);
	}
}

_export void ui_add_statistics_plugin(char *p, char *format, ...) {
	bool empty = ui_statistics ? FALSE : TRUE;

	char buf[512];

	snprintf(buf, 512, "[plugin %s] ", p);

	va_list args;
	va_start(args, format);

	vsnprintf(buf + strlen("[plugin ] ") + strlen(p), 512 - strlen(buf), format, args);

	va_end(args);

	buf[511] = '\0';

	ui_statistics = (char *) realloc(ui_statistics, (empty ? 0 : strlen(ui_statistics)) + strlen(buf) + 1);

	if (empty) {
		strcpy(ui_statistics, buf);
	} else {
		strcat(ui_statistics, buf);
	}
}

void ui_print_statistics() {
	if (ui_statistics) {
		ui_print(UI_WHITE, "%s", ui_statistics);
	}
}

// not needed
static int string_count(char *s, char c) {
	int i = 0;
	char *t = s;
	while ((t = strchr(t, c))) {
		t++;
		i++;
	}
	return i;
}

static char * ui_pad_string(char *str, char c) {
	char *pad = (char *) malloc(((COLS - strlen(str)) / 2));
	memset(pad, c, ((COLS - strlen(str)) / 2) - 1);
	pad[((COLS - strlen(str)) / 2) - 1] = '\0';
	char *s;
	string_new(&s, pad, " ", str, " ", pad, NULL);
	return s;
}

static void resize_handler() {
	ui_resized = TRUE;
}

void ui_init() {
	ui_shutdown = FALSE;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&console_mutex, &attr);
	pthread_mutexattr_destroy(&attr);

	string_new(&logfile, "motoko.", profile, ".log", "");

	return;

	//signal(SIGWINCH, resize_handler);

	initscr();
	clear();

	if (has_colors()) {
		start_color();
		init_pair(1, UI_BLACK, UI_WHITE);
		init_pair(2, UI_RED, UI_BLACK);
		init_pair(3, UI_GREEN, UI_BLACK);
		init_pair(4, UI_YELLOW, UI_BLACK);
		init_pair(5, UI_BLUE, UI_BLACK);
		init_pair(6, UI_MAGENTA, UI_BLACK);
		init_pair(7, UI_CYAN, UI_BLACK);
		init_pair(8, UI_WHITE, UI_BLACK);
		init_pair(9, UI_WHITE, UI_BLUE);
	}

	curs_set(0);
}

void ui_finit() {
	ui_shutdown = TRUE;

	pthread_mutex_destroy(&console_mutex);

	if (logfile) {
		free(logfile);
	}
	if (ui_statistics) {
		free(ui_statistics);
	}

	return;

	endwin();
}

bool ui_get_terminal_size(int *x, int *y) {
#if TIOCGSIZE
	struct ttysize ts;
#elif defined(TIOCGWINSZ)
	struct winsize ts;
#endif
	if(!isatty(STDIN_FILENO)) {
		return FALSE;
	}
#if TIOCGSIZE
	if(ioctl(STDIN_FILENO, TIOCGSIZE, &ts) < 0) {
		return FALSE;
	}
	*y = ts.ts_cols;
	*x = ts.ts_lines;
#elif defined(TIOCGWINSZ)
	if(ioctl(STDIN_FILENO, TIOCGWINSZ, &ts) < 0) {
		return FALSE;
	}
	*y = ts.ws_col;
	*x = ts.ws_row;
#endif
	return TRUE;
}

void ui_handle_resize() {
	ui_resized = FALSE;

}

void ui_start() {
	ui_init();
}

void ui_stop() {
	ui_finit();
}
