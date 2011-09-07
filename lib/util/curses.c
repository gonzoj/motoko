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

#include <stdlib.h>
#include <string.h>

#include "util/curses.h"

static void ui_window(WINDOW **w, int x, int y, int l, int h, int c, bool boxed) {
	w[0] = newwin(h, l, y, x);
	if (has_colors()) {
		wbkgd(w[0], COLOR_PAIR(c));
	}
	if (boxed) {
		box(w[0], 0, 0);

		w[1] = subwin(w[0], h - 2, l - 2, y + 1, x + 1);
		//w[1] = newwin(h - 2, l - 2, y + 1, x + 1);
		if (has_colors()) {
			wbkgd(w[1], COLOR_PAIR(c));
		}
	}
}

void ui_new_element(ui_layout *layout, ui_element_type t, bool s, int x, int y, int s_x, int s_y, int c, update_t u, char *n, int n_v, char *v[]) {
	layout->elements = (ui_element *) realloc(layout->elements, ++(layout->size) * sizeof(ui_element));
	layout->elements[layout->size - 1].type = t;
	layout->elements[layout->size - 1].scrolling = s;
	layout->elements[layout->size - 1].x = x;
	layout->elements[layout->size - 1].y = y;
	layout->elements[layout->size - 1].size_x = s_x;
	layout->elements[layout->size - 1].size_y = s_y;
	layout->elements[layout->size - 1].color = c;
	layout->elements[layout->size - 1].element = NULL;
	layout->elements[layout->size - 1].update = u;
	layout->elements[layout->size - 1].name = n;
	layout->elements[layout->size - 1].n_vars = n_v;
	if (n_v) {
		layout->elements[layout->size - 1].vars = (char **) malloc(sizeof(char *) * n_v);
		memcpy(layout->elements[layout->size - 1].vars, v, sizeof(char *) * n_v);
	}
}

void ui_create_layout(ui_layout *layout) {
	int i;
	for (i = 0; i < layout->size; i++) {
		ui_element *e = &layout->elements[i];
		switch (e->type) {
		case UI_WINDOW: {
			e->element = (WINDOW **) malloc(sizeof(WINDOW *));
			ui_window(e->element, e->x, e->y, e->size_x, e->size_y, e->color, FALSE);
			if (e->update) {
				e->update(e->element[0]);
			}
			if (e->scrolling) {
				scrollok(e->element[0], TRUE);
			}
			ui_refresh_window(e->element);
			break;
		}
		case UI_BOXED_WINDOW: {
			e->element = (WINDOW **) malloc(sizeof(WINDOW *) * 2);
			ui_window(e->element, e->x, e->y, e->size_x, e->size_y, e->color, TRUE);
			if (e->update) {
				e->update(e->element[1]);
			}
			if (e->scrolling) {
				scrollok(e->element[1], TRUE);
			}
			ui_refresh_boxed_window(e->element);
			break;
		}
		}
	}
}

void ui_destroy_layout(ui_layout *layout) {
	int i;
	for (i = 0; i < layout->size; i++) {
		ui_element *e = &layout->elements[i];
		switch (e->type) {
		case UI_WINDOW: {
			delwin(e->element[0]);
			free(e->element);
			if (e->n_vars) {
				free(e->vars);
			}
			break;
		}
		case UI_BOXED_WINDOW: {
			delwin(e->element[1]);
			delwin(e->element[0]);
			free(e->element);
			if (e->n_vars) {
				free(e->vars);
			}
			break;
		}
		}
	}
	if (layout->size) {
		free(layout->elements);
	}
}

void ui_update_layout(ui_layout *layout, const char *var) {
	int i;
	for (i = 0; i < layout->size; i++) {
		ui_element *e = &layout->elements[i];
		int j;
		for (j = 0; j < e->n_vars; j++) {
			if (!strcmp(e->vars[j], var)) {
				switch (e->type) {
				case UI_WINDOW: {
					if(e->update) {
						e->update(e->element[0]);
					}
					ui_refresh_window(e->element);
					break;
				}
				case UI_BOXED_WINDOW: {
					if (e->update) {
						e->update(e->element[1]);
					}
					ui_refresh_boxed_window(e->element);
					break;
				}
				}
			}
		}
	}
}

WINDOW * ui_get_window(ui_layout *layout, const char *name) {
	int i;
	for (i = 0; i < layout->size; i++) {
		if (!strcmp(layout->elements[i].name, name)) {
			switch (layout->elements[i].type) {
			case UI_WINDOW:
				return layout->elements[i].element[0];
			case UI_BOXED_WINDOW:
				return layout->elements[i].element[1];
			}
		}
	}
	return NULL;
}

ui_element * ui_get_element(ui_layout *layout, const char *name) {
	int i;
	for (i = 0; i < layout->size; i++) {
		if (!strcmp(layout->elements[i].name, name)) {
			return (layout->elements + i);
		}
	}
	return NULL;
}
