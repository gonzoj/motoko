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

#ifndef CURSES_H_
#define CURSES_H_

#include <ncurses.h>

#define ui_refresh_window(w) wrefresh(w[0])

#define ui_refresh_boxed_window(w) touchwin(w[0]); wrefresh(w[0]); wrefresh(w[1])

typedef enum {
	UI_WINDOW, UI_BOXED_WINDOW
} ui_element_type;

typedef void (*update_t)(WINDOW *w);

typedef struct {
	ui_element_type type;
	bool scrolling;
	int x;
	int y;
	int size_x;
	int size_y;
	int color;
	WINDOW **element;
	update_t update;
	char *name;
	int n_vars;
	char **vars;
} ui_element;

typedef struct {
	int size;
	ui_element *elements;
} ui_layout;

#define ui_init_layout(l) l.size = 0; l.elements = NULL;

typedef enum {
	UI_NORMAL = A_NORMAL,
	UI_UNDERLINE = A_UNDERLINE,
	UI_BOLD = A_BOLD,
} ui_font;

#define UI_BOLD(w, f) wattron(w, UI_BOLD); f; wattroff(w, UI_BOLD);

#define UI_UNDERLINE(w, f) wattron(w, UI_UNDERLINE); f; wattroff(w, UI_UNDERLINE);

#define UI_COLOR(w, c, f) if (has_colors()) wcolor_set(w, c, NULL); f;

void ui_new_element(ui_layout *layout, ui_element_type t, bool s, int x, int y, int s_x, int s_y, int c, update_t u, char *n, int n_v, char *v[]);

void ui_create_layout(ui_layout *layout);

void ui_destroy_layout(ui_layout *layout);

void ui_update_layout(ui_layout *layout, const char *var);

WINDOW * ui_get_window(ui_layout *layout, const char *name);

ui_element * ui_get_element(ui_layout *layout, const char *name);

#endif /* CURSES_H_ */
