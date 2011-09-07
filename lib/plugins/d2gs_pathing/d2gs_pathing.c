/*
 * Copyright (c) 2011 gonzoj
 *
 * please check the CREDITS file for further information.
 *
 * this program is free software: you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program.  if not, see <http://www.gnu.org/licenses/>.
 */

#define _PLUGIN

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include <module.h>

#include <d2gs.h>
#include <packet.h>
#include <settings.h>
#include <gui.h>

#include <moduleman.h>

#include <util/net.h>
#include <util/list.h>
#include <util/system.h>
#include <util/string.h>
#include <util/file.h>

#include "levels.h"
#include "exits.h"
#include "mazes.h"

static struct setting module_settings[] = (struct setting []) {
	SETTING("CastDelay", INTEGER, 250)
};

static struct list module_settings_list = LIST(module_settings, struct setting, 1);

#define module_setting(name) ((struct setting *)list_find(&module_settings_list, (comparator_t) compare_setting, name))

#define PERCENT(a, b) ((a) != 0 ? (int) (((double) (b) / (double) (a)) * 100) : 0)

/* statistics */
static int n_runs = 0;
static int n_found = 0;
static int n_stuck = 0;
static int n_tiles_discovered = 0;
static int n_direct_path = 0;
static int t_total = 0;

#define MAX_TELEPORT_DISTANCE 45

#define min(x, y) ((x) < (y) ? (x) : (y))

pthread_mutex_t teleport_m;
pthread_cond_t teleport_cv;

typedef struct {
	word x;
	word y;
} point_t;

typedef struct {
	dword id;
	point_t location;
} object_t;

#define SQUARE(x) ((x) * (x))

#define DISTANCE(a, b) ((int) sqrt((double) (SQUARE((a).x - (b).x) + SQUARE((a).y - (b).y))))

static object_t bot;

typedef struct {
	object_t object;
	byte id;
} exit_t;

static struct list exits;

pthread_mutex_t exits_m;

#define SCALE 5

#define TILE_TO_WORLD(c) ((c) * SCALE)
#define WORLD_TO_TILE(c) ((c) / SCALE)

typedef struct _tile_t {
	word x;
	word y;
	int size;
	bool visited;
	struct {
		struct _tile_t *north;
		struct _tile_t *east;
		struct _tile_t *south;
		struct _tile_t *west;
	} adjacent;
	struct list *objects;
	struct list *npcs;
} tile_t;

static struct list tiles[0xFF];

static struct list *c_tiles;

pthread_mutex_t tiles_m;

pthread_mutex_t objects_m;
pthread_mutex_t npcs_m;

typedef struct _node_t {
	word x;
	word y;
	int size;
	int distance;
	bool visited;
	struct _node_t *previous;
} node_t;

typedef struct {
	int layout[512][512];
	point_t origin;
	int width;
	int height;
	int size;
} room_layout_t;

#define MAX_TILE_SIZE 14

enum {
	NORTH = 0, EAST, SOUTH, WEST, NORTHEAST, NORTHWEST, SOUTHEAST, SOUTHWEST
};

typedef struct {
	unsigned n_events;
	double p_north;
	double p_east;
	double p_south;
	double p_west;
	bool north;
	bool east;
	bool south;
	bool west;
	word objects[MAX_TILE_SIZE][MAX_TILE_SIZE];
	bool walkable[MAX_TILE_SIZE][MAX_TILE_SIZE];
} tile_data_t;

static struct list tiles_data[0xFF];

static struct list *c_tiles_data;

static const unsigned short object_filter[] = {
	31, 101, 102, 283, 285, 286, 287, 358, 359, 400, 403, 408, 409, // braziers
	37, 38, 48, 49, 117, 296, 297, 327, 328, 370, 409, 434, 436, 437, 480, 481, 482, 489, 490, 514, 515, 536, 537, 552, 560, // torches
	13, 14, 15, 16, 23, 24, 25, 27, 47, 62, 63, 64, 74, 75, 91, 92, 98, 99, 129, 153, 229, 230, 290, 291, 292, 293, 294, 295, 508, 547, 564, 571, 572, // doors
	119, 145, 156, 157, 237, 238, 288, 323, 324, 398, 402, 429, 494, 496, 511, 539 // waypoints
};

bool is_valid_object_code(unsigned short code) {
	int i;
	for (i = 0; i < sizeof(object_filter) / sizeof(unsigned short); i++) {
		if (code == object_filter[i]) return TRUE;
	}
	return FALSE;
}

void set_object_mask(tile_data_t *d, tile_t *t, word code, word x, word y) {
	if (is_valid_object_code(code)) {
		d->objects[WORLD_TO_TILE(x) - t->x][WORLD_TO_TILE(y) - t->y] = code;
	}
}

void set_walkable_mask(tile_data_t *d, tile_t *t, word x, word y) {
	d->walkable[WORLD_TO_TILE(x) - t->x][WORLD_TO_TILE(y) - t->y] = TRUE;
}

void update_walkable_mask(tile_data_t *d, tile_t *t) {
	pthread_mutex_lock(&npcs_m);
	struct iterator it = list_iterator(t->npcs);
	object_t *o;
	while ((o = iterator_next(&it))) {
		set_walkable_mask(d, t, o->location.x, o->location.y);
	}
	pthread_mutex_unlock(&npcs_m);
}

void set_adjacent_tiles(tile_data_t *d, tile_t *t) {
	d->north = t->adjacent.north ? TRUE : FALSE;
	d->east = t->adjacent.east ? TRUE : FALSE;
	d->south = t->adjacent.south ? TRUE : FALSE;
	d->west = t->adjacent.west ? TRUE : FALSE;
}

void set_direction_probabilites(tile_data_t *d, tile_t *t, exit_t *exit) {
	if (d->n_events > 0xFFFFFFFE) {
		return; // n_events overflow
	}
	unsigned n_north = d->p_north * d->n_events;
	unsigned n_east = d->p_east * d->n_events;
	unsigned n_south = d->p_south * d->n_events;
	unsigned n_west = d->p_west * d->n_events;
	d->n_events++;
	if (TILE_TO_WORLD(t->x) < exit->object.location.x && TILE_TO_WORLD(t->x + t->size) < exit->object.location.x) {
		n_east++;
	} else if (TILE_TO_WORLD(t->x) > exit->object.location.x) {
		n_west++;
	}
	if (TILE_TO_WORLD(t->y) < exit->object.location.y && TILE_TO_WORLD(t->y + t->size) < exit->object.location.y) {
		n_north++;
	} else if (TILE_TO_WORLD(t->y) > exit->object.location.y) {
		n_south++;
	}
	d->p_north = (double) n_north / d->n_events;
	d->p_east = (double) n_east / d->n_events;
	d->p_south = (double) n_south / d->n_events;
	d->p_west = (double) n_west / d->n_events;
}

bool get_walkable_coords(tile_data_t *d, tile_t *t, point_t *p, point_t *q) {
	bool s = FALSE;
	int i, j;
	for (i = 0; i < MAX_TILE_SIZE; i++) {
		for (j = 0; j < MAX_TILE_SIZE; j++) {
			if (d->walkable[i][j]) {
				if (!s || !q) {
					p->x = TILE_TO_WORLD(t->x + i);
					p->y = TILE_TO_WORLD(t->y + j);
					s = TRUE;
				} else if (q) {
					point_t r;
					r.x = TILE_TO_WORLD(t->x + i);
					r.y = TILE_TO_WORLD(t->y + j);
					if (DISTANCE(*q, r) < DISTANCE(*q, *p)) {
						p->x = r.x;
						p->y = r.y;
					}
				}
				bool clear = TRUE;
				pthread_mutex_lock(&npcs_m);
				struct iterator it = list_iterator(t->npcs);
				object_t *o;
				while ((o = iterator_next(&it))) {
					if (p->x == o->location.x && p->y == o->location.y) {
						clear = FALSE;
					}
				}
				pthread_mutex_unlock(&npcs_m);
				if (clear && !q) {
					return TRUE;
				}
			}
		}
	}
	return s;
}

bool compare_tile_data_to_tile(tile_data_t *d, tile_t *t) {
	bool r = (t->adjacent.north ? d->north : !d->north) && \
	         (t->adjacent.east ? d->east : !d->east) && \
		 (t->adjacent.south ? d->south : !d->south) && \
		 (t->adjacent.west ? d->west : !d->west);
	pthread_mutex_lock(&objects_m);
	struct iterator it = list_iterator(t->objects);
	object_t *o;
	while ((o = iterator_next(&it)) && r) {
		if (is_valid_object_code(o->id)) {
			r &= (d->objects[WORLD_TO_TILE(o->location.x) - t->x][WORLD_TO_TILE(o->location.y) - t->y] == o->id);
		}
	}
	pthread_mutex_unlock(&objects_m);
	return r;
}

tile_data_t * get_corresponding_tile_data(struct list *l, tile_t *t) {
	return list_find(l, (comparator_t) compare_tile_data_to_tile, t);
}

tile_data_t * tile_data_new(tile_data_t *d, tile_t *t) {
	memset(d, 0, sizeof(tile_data_t));
	d->n_events = 0;
	d->p_north = d->p_east = d->p_south = d->p_west = 0;
	set_adjacent_tiles(d, t);
	struct iterator it;
	object_t *o;
	pthread_mutex_lock(&objects_m);
	it = list_iterator(t->objects);
	while ((o = iterator_next(&it))) {
		set_object_mask(d, t, o->id, o->location.x, o->location.y);
	}
	pthread_mutex_unlock(&objects_m);
	pthread_mutex_lock(&npcs_m);
	it = list_iterator(t->npcs);
	while ((o = iterator_next(&it))) {
		set_walkable_mask(d, t, o->location.x, o->location.y);
	}
	pthread_mutex_unlock(&npcs_m);
	return d;
}

void update_tile_data(struct list *data, struct list *tiles, exit_t *exit) {
	pthread_mutex_lock(&exits_m);
	pthread_mutex_lock(&tiles_m);
	plugin_print("pathing", "updating tile database\n");
	struct iterator it = list_iterator(tiles);
	tile_t *t;
	int i = 0;
	while ((t = iterator_next(&it))) {
		if (!t->visited) {
			continue;
		}
		tile_data_t *d;
		if (!(d = get_corresponding_tile_data(data, t))) {
			tile_data_t n;
			tile_data_new(&n, t);
			list_add(data, &n);
			d = list_element(data, list_size(data) - 1);
			i++;
		} else {
			update_walkable_mask(d, t);
		}
		if (exit) {
			set_direction_probabilites(d, t, exit);
		}
	}
	pthread_mutex_unlock(&tiles_m);
	pthread_mutex_unlock(&exits_m);
	n_tiles_discovered += i;
	if (i) plugin_print("pathing", "added %i tile(s) to database\n", i);
}

int level_get_id_from_string(const char *s_level) {
	int i;
	for (i = 0; i < sizeof(levels) / sizeof(level_t); i++) {
		if (string_compare(levels[i].name, (char *) s_level, FALSE)) return i;
	}
	return -1;
}

const char * level_get_string_from_id(int i) {
	return i >= 0 && i < sizeof(levels) / sizeof(level_t) ? levels[i].name : "(null)";
}

void load_tile_data(struct list *data, const char *tilesdir) {
	DIR *tiles = opendir(tilesdir);
	if (!tiles) return;
	struct dirent *tile;
	while ((tile = readdir(tiles))) {
		int ii = 0;
		if (!strstr(tile->d_name, ".tdb")) continue;
		char *s_file = file_get_absolute_path(tilesdir, tile->d_name);
		FILE *f = fopen(s_file, "rb");
		free(s_file);
		if (!f) continue;
		while (!feof(f) && !ferror(f)) {
			tile_data_t tile_data;
			if (!fread(&tile_data, sizeof(tile_data_t), 1, f)) break;
			char *ext = strrchr(tile->d_name, '.');
			*ext = '\0';
			int i = level_get_id_from_string(tile->d_name);
			*ext = '.';
			if (i < 0) break;
			list_add(data + i, &tile_data);
			ii++;
		}
		fclose(f);
	}
	closedir(tiles);
}

void save_tile_data(struct list *data, const char *tilesdir) {
	int i;
	for (i = 0; i < 0xFF; i++) {
		if (list_size(&data[i])) {
			DIR *tiles = opendir(tilesdir);
			if (!tiles) {
				if (mkdir(tilesdir, S_IRWXU)) continue;
			} else {
				closedir(tiles);
			}
			char *s_file;
			string_new(&s_file, level_get_string_from_id(i), ".tdb", NULL);
			char *f_tiles = file_get_absolute_path(tilesdir, s_file);
			FILE *f = fopen(f_tiles, "wb");
			free(s_file);
			free(f_tiles);
			if (!f) continue;
			struct iterator it = list_iterator(&data[i]);
			tile_data_t *tile_data;
			while ((tile_data = iterator_next(&it)) && !ferror(f)) {
				if (!fwrite(tile_data, sizeof(tile_data_t), 1, f)) break;
			}
			fclose(f);
		}
	}
}

void exit_add(dword id, word x, word y, byte exit_id) {
	pthread_mutex_lock(&exits_m);
	exit_t e = { { id, { x, y } }, exit_id };
	list_add(&exits, &e);
	pthread_mutex_unlock(&exits_m);
}

//#define tile_new(x, y, s) (tile_t) { x, y, s, FALSE, .adjacent.north = NULL, .adjacent.east = NULL, .adjacent.south = NULL, .adjacent.west = NULL, .objects = LIST(NULL, object_t, 0), .npcs = LIST(NULL, object_t, 0) }

tile_t tile_new(word x, word y, int s) {
	tile_t t = { x, y, s, FALSE, { NULL, NULL, NULL, NULL }, NULL, NULL };
	t.objects = (struct list *) malloc(sizeof(struct list));
	*t.objects = list_new(object_t);
	t.npcs = (struct list *) malloc(sizeof(struct list));
	*t.npcs = list_new(object_t);
	return t;
}

void tile_destroy(tile_t *t) {
	list_clear(t->objects);
	free(t->objects);
	list_clear(t->npcs);
	free(t->npcs);
}

// y seems to grow to south
#define northof(a, b) (((a)->x == (b)->x) && ((a)->y == (b)->y + (b)->size))
#define eastof(a, b) (((a)->y == (b)->y) && ((a)->x == (b)->x + (b)->size))
#define southof(a, b) (((a)->x == (b)->x) && ((a)->y == (b)->y - (b)->size))
#define westof(a, b) (((a)->y == (b)->y) && ((a)->x == (b)->x - (b)->size))

#define tile_contains(t, o) (((o)->location.x >= (t)->x * SCALE) && ((o)->location.x < ((t)->x + (t)->size) * SCALE) &&\
                             ((o)->location.y >= (t)->y * SCALE) && ((o)->location.y < ((t)->y + (t)->size) * SCALE))

bool tile_check_for_exit(tile_t *t, const char *exit, exit_t *r) {
	bool s = FALSE;
	pthread_mutex_lock(&exits_m);
	struct iterator it = list_iterator(&exits);
	exit_t *e;
	while ((e = iterator_next(&it))) {
		if (tile_contains(t, &e->object)) {
			if (exit) {
				if (string_compare((char *) exit, (char *) s_exits[e->id], FALSE)) {
					s = TRUE;
					if (r) {
						*r = *e;
					}
					break;
				}
			} else {
				s = TRUE;
				break;
			}
		}
	}
	pthread_mutex_unlock(&exits_m);
	return s;
}

int tile_compare(tile_t *a, tile_t *b) {
	return ((a->x == b->x) && (a->y == b->y));
}

void move_layout_x(room_layout_t *layout, int x) {	
	layout->width += x;
	int i, j;
	/*for (j = 0; j < layout->height; j++) {
		int line[layout->width];
		memcpy(line, &(layout->layout[0][j]), layout->width);
		memset(&(layout->layout[0][j]), 0, layout->width);
		for (i = 0; i < layout->width - x; i++) {
			layout->layout[i + x][j] = line[i];
		}
	}*/
	int copy[layout->width - x][layout->height];
	for (i = 0; i < layout->width - x; i++) {
		for (j = 0; j < layout->height; j++) {
			copy[i][j] = layout->layout[i][j];
			layout->layout[i][j] = 0;
		}
	}
	for (i = 0; i < layout->width - x; i++) {
		for (j = 0; j < layout->height; j++) {
			layout->layout[i + x][j] = copy[i][j];
		}
	}
	layout->origin.x -= (layout->size * x);
}

void move_layout_y(room_layout_t *layout, int y) {	
	layout->height += y;
	int i, j;
	/*for (i = 0; i < layout->width; i++) {
		int column[layout->height];
		memcpy(column, &(layout->layout[i][0]), layout->height);
		memset(&(layout->layout[i][0]), 0, layout->height);
		for (j = 0; j < layout->height - y; j++) {
			layout->layout[i][j + y] = column[j];
		}
	}*/
	int copy[layout->width][layout->height - y];
	for (i = 0; i < layout->width; i++) {
		for (j = 0; j < layout->height - y; j++) {
			copy[i][j] = layout->layout[i][j];
			layout->layout[i][j] = 0;
		}
	}
	for (i = 0; i < layout->width; i++) {
		for (j = 0; j < layout->height - y; j++) {
			layout->layout[i][j + y] = copy[i][j];
		}
	}
	layout->origin.y -= (layout->size * y);
}

#define north(l, x, y) ((y) > 0 && (l)->layout[(x)][(y) - 1])
#define west(l, x, y) ((x) > 0 && (l)->layout[(x) - 1][(y)])
#define south(l, x, y) ((l)->layout[(x)][(y) + 1])
#define east(l, x, y) ((l)->layout[(x) + 1][(y)])

void add_room(room_layout_t *layout, point_t *room) {
	if (layout->origin.x == 0 && layout->origin.y == 0) {
		//memcpy(&(layout->origin), &room, sizeof(point_t));
		layout->origin.x = room->x;
		layout->origin.y = room->y;
		layout->layout[0][0] = 1;
		layout->width = 1;
		layout->height = 1;
	} else {
		//if (layout->origin.x - room->x > 10 * 12 || layout->origin.y - room->y > 10 * 12) return;
		//if (room->x - layout->origin.x > 10 * 12 || room->y - layout->origin.y > 10 * 12) return;
		if (room->x < layout->origin.x) {
			move_layout_x(layout, (layout->origin.x - room->x) / layout->size);
		} else if ((room->x - layout->origin.x) / layout->size > layout->width - 1) {
			layout->width += ((room->x - layout->origin.x) / layout->size) + 1 - layout->width;;
		}
		if (room->y < layout->origin.y) {
			move_layout_y(layout, (layout->origin.y - room->y) / layout->size);
		} else if ((room->y - layout->origin.y) / layout->size > layout->height - 1) {
			layout->height += ((room->y - layout->origin.y) / layout->size) + 1 - layout->height;
		}

		layout->layout[(room->x - layout->origin.x) / layout->size][(room->y - layout->origin.y) / layout->size] = 1;
	}
}

void dump_room_layout_simple(room_layout_t *layout, struct list *tiles) {
	int i, j;
	for (j = 0; j < layout->height; j++) {
		for (i = 0; i < layout->width; i++) {
			char *r = "+";
			tile_t t = tile_new(layout->origin.x + i * layout->size, layout->origin.y +  j * layout->size, layout->size);
			tile_t *c = list_find(tiles, (comparator_t) tile_compare, &t);
			if (c) {
				if (c->visited) r = "x";
			}
			if (tile_check_for_exit(&t, NULL, NULL)) r = "e";
			if (tile_contains(&t, &bot)) r = "o";
			tile_destroy(&t);
			print(layout->layout[i][j] ? r : " ");
			if (east(layout, i, j) && layout->layout[i][j]) {
				print("-");
			} else {
				print(" ");
			}
		}
		print("\n");
		if (j < layout->height - 1) {
			int k;
			for (k = 0; k < layout->width; k++) {
				if (south(layout, k, j) && layout->layout[k][j]) {
					print("| ");
				} else {
					print("  ");
				}
			}
			print("\n");
		}
	}
}

void dump_tiles(struct list *tiles) {
	ui_console_lock();
	pthread_mutex_lock(&tiles_m);
	plugin_print("pathing", "tile layout (%i tile(s)):\n", list_size(tiles));
	room_layout_t layout = (room_layout_t) { { { 0 } }, { 0, 0 }, 0, 0, 0 };
	if (setting("Debug")->b_var) print("\n");
	struct iterator it = list_iterator(tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		if (!layout.size) layout.size = t->size;
		point_t p = (point_t) { t->x, t->y };
		add_room(&layout, &p);
		if (setting("Debug")->b_var) {
			print("tile %i/%i ", t->x, t->y);
			if (t->adjacent.south) print("N");
			if (t->adjacent.east) print("E");
			if (t->adjacent.north) print("S");
			if (t->adjacent.west) print("W");
			if (t->visited) print(" visited");
			if (tile_contains(t, &bot)) print(" <---");
			print("\n");
		}
	}
	print("\n");
	dump_room_layout_simple(&layout, tiles);
	print("\n");
	pthread_mutex_unlock(&tiles_m);
	ui_console_unlock();
}

void update_adjacent_tiles(struct list *tiles) {
	struct iterator it = list_iterator(tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		/*if (t != new) {
			if (t->adjacent.north) {
				t->adjacent.north = (tile_t *) (tiles->elements + (t->adjacent.north - base));
			}
			if (t->adjacent.east) {
				t->adjacent.east = (tile_t *) (tiles->elements + (t->adjacent.east - base));
			}
			if (t->adjacent.south) {
				t->adjacent.south = (tile_t *) (tiles->elements + (t->adjacent.south - base));
			}
			if (t->adjacent.west) {
				t->adjacent.west = (tile_t *) (tiles->elements + (t->adjacent.west - base));
			}
		}
		if (northof(new, t)) {
			t->adjacent.north = new;
			new->adjacent.south = t;
		} else if (eastof(new, t)) {
			t->adjacent.east = new;
			new->adjacent.west = t;
		} else if (southof(new, t)) {
			t->adjacent.south = new;
			new->adjacent.north = t;
		} else if (westof(new, t)) {
			t->adjacent.west = new;
			new->adjacent.east = t;
		}*/
		struct iterator jt = list_iterator(tiles);
		tile_t *j;
		while ((j = iterator_next(&jt))) {
			if (northof(j, t)) {
				t->adjacent.north = j;
			} else if (eastof(j, t)) {
				t->adjacent.east = j;
			} else if (southof(j, t)) {
				t->adjacent.south = j;
			} else if (westof(j, t)) {
				t->adjacent.west = j;
			}
		}
	}
}

int get_tile_size_by_area(byte area) {
	if (levels[area].type == 1) { // maze
		if (mazes[area][0] < MAX_TILE_SIZE && mazes[area][1] < MAX_TILE_SIZE) {
			return mazes[area][1]; // fuck, area 28 isn't symmetric (10/14)
		} else {
			return 8; // we assume every room larger than MAX_TILE_SIZE consists of 8x8 tiles
		}
	} else {
		return 8; // we assume tile size is 8 if the level is preset / wilderness
	}
}

void tile_add(word x, word y, byte area) {
	tile_t new = tile_new(x, y, get_tile_size_by_area(area)); // retrieve tile size corresponding to area
	tile_t *t = list_find(&tiles[area], (comparator_t) tile_compare, &new);
	if (!t) {
		plugin_debug("pathing", "new tile at %i/%i (%02X)\n", new.x, new.y, area);
		//tile_t *base = tiles[area].elements;
		pthread_mutex_lock(&tiles_m);
		list_add(&tiles[area], &new);
		//t = list_element(&tiles[area], list_size(&tiles[area]) - 1); // bah, need a function to retrieve last added item or let list_add return it instead
		update_adjacent_tiles(&tiles[area]);
		pthread_mutex_unlock(&tiles_m);
	} else {
		tile_destroy(&new);
	}
}

void tile_add_object(word x, word y, word code) {
	object_t o = { code, { x, y } };
	int i;
	for (i = 0; i < 0xFF; i++) {
		struct iterator it = list_iterator(&tiles[i]);
		tile_t *t;
		while ((t = iterator_next(&it))) {
			if (tile_contains(t, &o)) {
				pthread_mutex_lock(&objects_m);
				list_add(t->objects, &o);
				pthread_mutex_unlock(&objects_m);
				return;
			}
		}
	}
}

void tile_add_npc(word x, word y, dword id) {
	object_t o = { id, { x, y } };
	int i;
	for (i = 0; i < 0xFF; i++) {
		struct iterator it = list_iterator(&tiles[i]);
		tile_t *t;
		while ((t = iterator_next(&it))) {
			if (tile_contains(t, &o)) {
				pthread_mutex_lock(&npcs_m);
				list_add(t->npcs, &o);
				pthread_mutex_unlock(&npcs_m);
				return;
			}
		}
	}
}

tile_t * get_current_tile(tile_t *c) {
	pthread_mutex_lock(&tiles_m);
	struct iterator it = list_iterator(c_tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		if (tile_contains(t, &bot)) {
			if (c) {
				*c = *t;
			}
			pthread_mutex_unlock(&tiles_m);
			return t;
		}
	}
	pthread_mutex_unlock(&tiles_m);
	return NULL;
}

void set_current_tile_visited() {
	pthread_mutex_lock(&tiles_m);
	struct iterator it = list_iterator(c_tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		if (tile_contains(t, &bot)) {
			t->visited = TRUE;
		}
	}
	pthread_mutex_unlock(&tiles_m);
}

bool get_closest_new_tile(tile_t *c, tile_t *r) {
	bool s = FALSE;
	struct iterator it = list_iterator(c_tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		if (tile_compare(t, c)) {
			continue;
		}
		if (!t->visited) {
			if (!s) {
				*r = *t;
				s = TRUE;
			} else if (DISTANCE(*t, *c) < DISTANCE(*r, *c)) {
				*r = *t;
			}
		}
	}
	return s;
}

bool check_tile_direction(tile_t *c, tile_t *t, int direction) {
	switch (direction) {
		case NORTH: return (t->y > c->y && t->x == c->x);
		case EAST: return (t->y == c->y && t->x > c->x);
		case SOUTH: return (t->y < c->y && t->x == c->x);
		case WEST: return (t->y == c->y && t->x < c->x);
		case NORTHEAST: return (t->y > c->y && t->x > c->x);
		case NORTHWEST: return (t->y > c->y && t->x < c->x);
		case SOUTHEAST: return (t->y < c->y && t->x > c->x);
		case SOUTHWEST: return (t->y < c->y && t->x < c->x);
	}
	return FALSE;
}

bool is_better_pick(tile_t *c, tile_t *t, tile_t *r, int *direction) {
	int i;
	for (i = 0; i < 8; i++) {
		if (check_tile_direction(c, t, direction[i]) && check_tile_direction(c, r, direction[i])) {
			return DISTANCE(*t, *c) < DISTANCE(*r, *c);
		} else if (check_tile_direction(c, r, direction[i])) {
			return FALSE;
		} else if (check_tile_direction(c, t, direction[i])) {
			return TRUE;
		}
	}
	return FALSE;
}

bool get_closest_new_tile_in_direction(tile_t *c, tile_t *r, int *direction) {
	bool s = FALSE;
	struct iterator it = list_iterator(c_tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		if (tile_compare(t, c)) {
			continue;
		}
		if (!t->visited) {
			if (!s) {
				*r = *t;
				s = TRUE;
			} else if (is_better_pick(c, t, r, direction)) {
				*r = *t;
			}
		}
	}
	return s;
}

bool get_optimal_direction(tile_t *c, int *direction) {
	double p[8];
	tile_data_t *d = get_corresponding_tile_data(c_tiles_data, c);
	if (d) {
		p[NORTH] = d->p_north;
		p[EAST] = d->p_east;
		p[SOUTH] = d->p_south;
		p[WEST] = d->p_west;
		p[NORTHEAST] = p[NORTH] * p[EAST];
		p[NORTHWEST] = p[NORTH] * p[WEST];
		p[SOUTHEAST] = p[SOUTH] * p[EAST];
		p[SOUTHWEST] = p[SOUTH] * p[WEST];
		tile_t **adjacents = (tile_t **) &c->adjacent;
		int i;
		for (i = 0; i < 4; i++) {
			tile_t *a = adjacents[i];
			if (!a) continue;
			d = get_corresponding_tile_data(c_tiles_data, a);
			if (d) {
				p[NORTH] *= d->p_north;
				p[EAST] *= d->p_east;
				p[SOUTH] *= d->p_south;
				p[WEST] *= d->p_west;
				p[NORTHEAST] = p[NORTH] * p[EAST];
				p[NORTHWEST] = p[NORTH] * p[WEST];
				p[SOUTHEAST] = p[SOUTH] * p[EAST];
				p[SOUTHWEST] = p[SOUTH] * p[WEST];
			}
		}
		int j;
		for (j = 0; j < 8; j++) {
			int dir = NORTH;
			for (i = 1; i < 8; i++) {
				if (p[dir] < p[i] && p[i] >= 0) {
					dir = i;
				}
			}
			if (p[dir] >= 0) direction[j] = dir;
			p[dir] = -1;
		}
		return TRUE;
	}
	return FALSE;
}

bool tile_has_adjacent(tile_t *t) {
	return (t->adjacent.north || t->adjacent.east || t->adjacent.south || t->adjacent.west);
}

bool get_next_tile(tile_t *c, tile_t *r, const char *exit) {
	pthread_mutex_lock(&tiles_m);

	struct iterator it2 = list_iterator(c_tiles);
	tile_t *t1;
	while ((t1 = iterator_next(&it2))) {
		if (tile_contains(t1, &bot)) {
			c = t1;
		}
	}

	struct iterator it = list_iterator(c_tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		if (tile_check_for_exit(t, exit, NULL)) {
			if (tile_has_adjacent(t)) { // warning: we assume that the tile has no adjacent tiles if the current tile isn't connected to it yet
				*r = *t;
				pthread_mutex_unlock(&tiles_m);
				return TRUE;
			}
		}
	}
	int direction[8];
	if (get_optimal_direction(c,  direction)) {
		char *s_dir[] = { "N", "E", "S", "W", "NE", "NW", "SE", "SW" };
		plugin_debug("pathing", "optimal direction: %s %s %s %s %s %s %s %s\n", s_dir[direction[0]], s_dir[direction[1]], s_dir[direction[2]], s_dir[direction[3]], s_dir[direction[4]], s_dir[direction[5]], s_dir[direction[6]], s_dir[direction[7]]);
		if (get_closest_new_tile_in_direction(c, r, direction)) {
			pthread_mutex_unlock(&tiles_m);
			return TRUE;
		}
	}
	plugin_debug("pathing", "current tile not in database, choosing closest new tile\n");
	bool s = get_closest_new_tile(c, r);
	pthread_mutex_unlock(&tiles_m);
	return s;
}

#define list_index(l, e) ((void *) (e) >= (void *) (l)->elements && (void *) (e) < (void *) ((l)->elements + (l)->len * (l)->size) ? ((void *) (e) - (void *) (l)->elements) / (l)->size : -1) // we should inlcude this in list.c/list.h

struct list * get_adjacent_nodes(struct list *nodes, node_t *n, struct list *l) {
	list_clear(l);
	struct iterator it = list_iterator(nodes);
	node_t *m;
	while ((m = iterator_next(&it))) {
		if (northof(m, n) || eastof(m, n) || southof(m, n) || westof(m, n)) {
			list_add(l, &m);
		}
	}
	return l;
}

tile_t * get_corresponding_tile(struct list *tiles, node_t *n) {
	struct iterator it = list_iterator(tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		if (t->x == n->x && t->y == n->y) {
			return t;
		}
	}
	return NULL;
}

bool dijkstra(tile_t *source, tile_t *target, struct list *path) {
	int i, alt;
	struct list nodes = list_new(node_t);
	struct list adjacent = list_new(node_t *);
	struct iterator it;
	tile_t *t;
	node_t *u, *v, **w;
	struct list c_path = list_new(tile_t);
	pthread_mutex_lock(&tiles_m);
	it = list_iterator(c_tiles);
	while ((t = iterator_next(&it))) {
		node_t n;
		n.x = t->x;
		n.y = t->y;
		n.size = t->size;
		if (tile_compare(t, source)) {
			n.distance = 0;
		} else {
			n.distance = 0x7FFFFFFF;
		}
		n.visited = FALSE;
		n.previous = NULL;
		list_add(&nodes, &n);
	}
	pthread_mutex_unlock(&tiles_m);
	i = 0;
	u = NULL;
	while (i < list_size(&nodes)) {
		u = NULL;
		it = list_iterator(&nodes);
		while ((v = iterator_next(&it))) {
			if (v->visited) {
				continue;
			}
			if (!u) {
				u = v;
				continue;
			}
			if (v->distance < u->distance) {
				u = v;
			}
		}
		if (!u) {
			break;
		}
		if (u->distance == 0x7FFFFFFF) {
			break;
		}
		if (u->x == target->x && u->y == target->y) {
			break;
		}
		u->visited = TRUE;
		i++;
		get_adjacent_nodes(&nodes, u, &adjacent);
		it = list_iterator(&adjacent);
		while ((w = iterator_next(&it))) {
			if ((*w)->visited) {
				continue;
			}
			alt = u->distance + u->size;
			if (alt < (*w)->distance) {
				(*w)->distance = alt;
				(*w)->previous = u;
			}
		}
		list_clear(&adjacent);
	}
	list_clear(path);
	*path = list_new(tile_t);
	if (!u) {
		plugin_debug("pathing", "error (dijkstra): u is NULL\n");
		goto end;
	}
	if (u->distance == 0x7FFFFFFF) {
		plugin_debug("pathing", "error (dijkstra): u->distance is max\n");
		goto end;
	}
	pthread_mutex_lock(&tiles_m);
	while ((t = get_corresponding_tile(c_tiles, u))) {
		if (tile_compare(t, source)) {
			break;
		}
		list_add(&c_path, t);
		plugin_debug("pathing", "adding %i/%i to path\n", t->x, t->y);
		u = u->previous;
	}
	pthread_mutex_unlock(&tiles_m);
	for (i = list_size(&c_path) - 1; i >= 0; i--) {
		list_add(path, list_element(&c_path, i));
	}
	list_clear(&c_path);
	end:
	list_clear(&nodes);
	return list_size(path) > 0 ? TRUE : FALSE;
}

void teleport(int x, int y) {
	point_t p = { x, y };
	plugin_print("pathing", "teleporting to %i/%i (%i)\n", x, y, DISTANCE(p, bot.location));

	//swap_right(NULL, 0x36);
	
	pthread_mutex_lock(&teleport_m);

	d2gs_send(0x0c, "%w %w", x, y);
	
	msleep(module_setting("CastDelay")->i_var);

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 2;
	pthread_cond_timedwait(&teleport_cv, &teleport_m, &ts);

	pthread_mutex_unlock(&teleport_m);
}

// having two separate dijkstra functions for different input is pretty ugly, oh well :-/

struct list * get_adjacent_nodes2(struct list *nodes, node_t *n, struct list *l) {
	list_clear(l);
	struct iterator it = list_iterator(nodes);
	node_t *m;
	while ((m = iterator_next(&it))) {
		if (DISTANCE(*m, *n) < MAX_TELEPORT_DISTANCE) {
			list_add(l, &m);
		}
	}
	return l;
}

bool point_compare(point_t *a, point_t *b) {
	return (a->x == b->x && a->y == b->y);
}

bool dijkstra2(struct list *l, point_t *source, point_t *target, struct list *path) {
	int i, alt;
	struct list nodes = list_new(node_t);
	struct list adjacent = list_new(node_t *);
	struct iterator it;
	point_t *t;
	node_t *u, *v, **w;
	struct list c_path = list_new(point_t);
	it = list_iterator(l);
	while ((t = iterator_next(&it))) {
		node_t n;
		n.x = t->x;
		n.y = t->y;
		if (point_compare(t, source)) {
			n.distance = 0;
		} else {
			n.distance = 0x7FFFFFFF;
		}
		n.visited = FALSE;
		n.previous = NULL;
		list_add(&nodes, &n);
	}
	i = 0;
	u = NULL;
	while (i < list_size(&nodes)) {
		u = NULL;
		it = list_iterator(&nodes);
		while ((v = iterator_next(&it))) {
			if (v->visited) {
				continue;
			}
			if (!u) {
				u = v;
				continue;
			}
			if (v->distance < u->distance) {
				u = v;
			}
		}
		if (!u) {
			break;
		}
		if (u->distance == 0x7FFFFFFF) {
			break;
		}
		if (u->x == target->x && u->y == target->y) {
			break;
		}
		u->visited = TRUE;
		i++;
		get_adjacent_nodes2(&nodes, u, &adjacent);
		it = list_iterator(&adjacent);
		while ((w = iterator_next(&it))) {
			if ((*w)->visited) {
				continue;
			}
			alt = u->distance + DISTANCE(*u, **w);
			if (alt < (*w)->distance) {
				(*w)->distance = alt;
				(*w)->previous = u;
			}
		}
		list_clear(&adjacent);
	}
	list_clear(path);
	*path = list_new(point_t);
	if (!u) {
		plugin_debug("pathing", "error (dijkstra): u is NULL\n");
		goto end;
	}
	if (u->distance == 0x7FFFFFFF) {
		plugin_debug("pathing", "error (dijkstra): u->distance is max\n");
		goto end;
	}
	while (!(u->x == source->x && u->y == source->y)) {
		point_t p = { u->x, u->y };
		list_add(&c_path, &p);
		plugin_debug("pathing", "adding %i/%i to path\n", p.x, p.y);
		u = u->previous;
	}
	for (i = list_size(&c_path) - 1; i >= 0; i--) {
		list_add(path, list_element(&c_path, i));
	}
	list_clear(&c_path);
	end:
	list_clear(&nodes);
	return list_size(path) > 0 ? TRUE : FALSE;
}

bool get_valid_teleport_coords(tile_t *t, struct list *l) {
	/*bool s = FALSE;
	point_t p, q;
	tile_data_t *d = get_corresponding_tile_data(c_tiles_data, t);
	if (d) {
		s = get_walkable_coords(d, t, &p, &bot.location);
	}
	pthread_mutex_lock(&npcs_m);
	if (!s && !list_empty(t->npcs)) {
		struct iterator it = list_iterator(t->npcs);
		object_t *o;
		while ((o = iterator_next(&it))) {
			if (!s) {
				p.x = o->location.x;
				p.y = o->location.y;
				s = TRUE;
			} else if (DISTANCE(o->location, bot.location) < DISTANCE(p, bot.location)) {
				p.x = o->location.x;
				p.y = o->location.y;
			}
		}
		pthread_mutex_unlock(&npcs_m);
	}
	pthread_mutex_unlock(&npcs_m);
	tile_t c;
	get_current_tile(&c);
	if (!s) {
		if (northof(t, &c)) {
			p.x = TILE_TO_WORLD(t->x + (t->size / 2));
			p.y = TILE_TO_WORLD(t->y + 1);
			s = TRUE;
		} else if (eastof(t, &c)) {
			p.x = TILE_TO_WORLD(t->x + 1);
			p.y = TILE_TO_WORLD(t->y + (t->size / 2));
			s = TRUE;
		} else if (southof(t, &c)) {
			p.x = TILE_TO_WORLD(t->x + (t->size / 2));
			p.y = TILE_TO_WORLD(t->y + (t->size - 1));
			s = TRUE;
		} else if (westof(t, &c)) {
			p.x = TILE_TO_WORLD(t->x + (t->size - 1));
			p.y = TILE_TO_WORLD(t->y + (t->size / 2));
			s = TRUE;
		}
	}
	if (s && DISTANCE(p, bot.location) > MAX_TELEPORT_DISTANCE) {
		s = FALSE;
		d = get_corresponding_tile_data(c_tiles_data, &c);
		if (d) {
			if (get_walkable_coords(d, &c, &q, &p)) {
				s = (DISTANCE(q, p) <= MAX_TELEPORT_DISTANCE && DISTANCE(q, bot.location) <= MAX_TELEPORT_DISTANCE);
				print("q set here\n");
			}
		}
		pthread_mutex_lock(&npcs_m);
		if (!s && !list_empty(c.npcs)) {
			struct iterator it = list_iterator(c.npcs);
			object_t *o;
			while ((o = iterator_next(&it))) {
				if (!s) {
					q.x = o->location.x;
					q.y = o->location.y;
					s = TRUE;
				} else if (DISTANCE(o->location, p) < DISTANCE(q, p)) {
					q.x = o->location.x;
					q.y = o->location.y;
				}
			}
			s = (DISTANCE(q, p) <= MAX_TELEPORT_DISTANCE && DISTANCE(q, bot.location) <= MAX_TELEPORT_DISTANCE);
		}
		pthread_mutex_unlock(&npcs_m);
		if (!s) {
			print("c: %i/%i t: %i/%i\n", c.x, c.y, t->x, t->y);
			if (northof(t, &c)) {
				q.x = TILE_TO_WORLD(c.x + (c.size / 2));
				q.y = TILE_TO_WORLD(c.y + (c.size - 1));
				//q.y = TILE_TO_WORLD(c.y) + min(MAX_TELEPORT_DISTANCE, TILE_TO_WORLD(c.size - 1));
				if (DISTANCE(q, p) > MAX_TELEPORT_DISTANCE) {
					p.x = TILE_TO_WORLD(t->x + (t->size / 2));
					p.y = TILE_TO_WORLD(t->y + 1);
				}
				s = TRUE;
			} else if (eastof(t, &c)) {
				q.x = TILE_TO_WORLD(c.x + (c.size - 1));
				//q.x = TILE_TO_WORLD(c.x) + min(MAX_TELEPORT_DISTANCE, TILE_TO_WORLD(c.size - 1));
				q.y = TILE_TO_WORLD(c.y + (c.size / 2));
				if (DISTANCE(q, p) > MAX_TELEPORT_DISTANCE) {
					p.x = TILE_TO_WORLD(t->x + 1);
					p.y = TILE_TO_WORLD(t->y + (t->size / 2));
				}
				s = TRUE;
			} else if (southof(t, &c)) {
				q.x = TILE_TO_WORLD(c.x + (c.size / 2));
				q.y = TILE_TO_WORLD(c.y + 1);
				//q.y = TILE_TO_WORLD(c.y) + min(MAX_TELEPORT_DISTANCE, TILE_TO_WORLD(1));
				if (DISTANCE(q, p) > MAX_TELEPORT_DISTANCE) {
					p.x = TILE_TO_WORLD(t->x + (t->size / 2));
					p.y = TILE_TO_WORLD(t->y + (t->size - 1));
				}
				s = TRUE;
			} else if (westof(t, &c)) {
				q.x = TILE_TO_WORLD(c.x + 1);
				//q.y = TILE_TO_WORLD(c.x) + min(MAX_TELEPORT_DISTANCE, TILE_TO_WORLD(1));
				q.y = TILE_TO_WORLD(c.y + (c.size / 2));
				if (DISTANCE(q, p) > MAX_TELEPORT_DISTANCE) {
					p.x = TILE_TO_WORLD(t->x + (t->size - 1));
					p.y = TILE_TO_WORLD(t->y + (t->size / 2));
				}
				s = TRUE;
			}
			if (s) {
				if (DISTANCE(q, bot.location) > MAX_TELEPORT_DISTANCE) {
					word off = (word) ceil((double) (DISTANCE(q, bot.location) - MAX_TELEPORT_DISTANCE) / 2);
					q.x -= off;
					q.y -= off;
				}
			}
		}
		if (s) {
			list_add(l, &q);
		}
	}
	if (s) {
		list_add(l, &p);
	}
	return s;*/

	tile_t c;
	get_current_tile(&c);
	point_t p;
	point_t target = { 0, 0 };
	struct list valid_coords = list_new(point_t);
	list_add(&valid_coords, &bot.location);
	if (c.adjacent.north) {
		p.x = TILE_TO_WORLD(c.x + (c.size / 2));
		p.y = TILE_TO_WORLD(c.y + (c.size - 1));
		list_add(&valid_coords, &p);
	}
	if (t->adjacent.south) {
		p.x = TILE_TO_WORLD(t->x + (t->size / 2));
		p.y = TILE_TO_WORLD(t->y + 1);
		list_add(&valid_coords, &p);
	}
	if (c.adjacent.east) {
		p.x = TILE_TO_WORLD(c.x + (c.size - 1));
		p.y = TILE_TO_WORLD(c.y + (c.size / 2));
		list_add(&valid_coords, &p);
	}
	if (t->adjacent.west) {
		p.x = TILE_TO_WORLD(t->x + 1);
		p.y = TILE_TO_WORLD(t->y + (t->size / 2));
		list_add(&valid_coords, &p);
	}
	if (c.adjacent.south) {
		p.x = TILE_TO_WORLD(c.x + (c.size / 2));
		p.y = TILE_TO_WORLD(c.y + 1);
		list_add(&valid_coords, &p);
	}
	if (t->adjacent.north) {
		p.x = TILE_TO_WORLD(t->x + (t->size / 2));
		p.y = TILE_TO_WORLD(t->y + (t->size - 1));
		list_add(&valid_coords, &p);
	}
	if (c.adjacent.west) {
		p.x = TILE_TO_WORLD(c.x + 1);
		p.y = TILE_TO_WORLD(c.y + (c.size / 2));
		list_add(&valid_coords, &p);
	}
	if (t->adjacent.east) {
		p.x = TILE_TO_WORLD(t->x + (t->size - 1));
		p.y = TILE_TO_WORLD(t->y + (t->size / 2));
		list_add(&valid_coords, &p);
	}
	tile_data_t *d = get_corresponding_tile_data(c_tiles_data, t);
	if (d) {
		int i, j;
		for (i = 0; i < MAX_TILE_SIZE; i++) {
			for (j = 0; j < MAX_TILE_SIZE; j++) {
			if (d->walkable[i][j]) {
				p.x = TILE_TO_WORLD(t->x + i);
				p.y = TILE_TO_WORLD(t->y + j);
				list_add(&valid_coords, &p);
				if (!target.x && !target.y) {
					target.x = p.x;
					target.y = p.y;
				}
			}
			}
		}
	}
	d = get_corresponding_tile_data(c_tiles_data, &c);
	if (d) {
		int i, j;
		for (i = 0; i < MAX_TILE_SIZE; i++) {
			for (j = 0; j < MAX_TILE_SIZE; j++) {
			if (d->walkable[i][j]) {
				p.x = TILE_TO_WORLD(c.x + i);
				p.y = TILE_TO_WORLD(c.y + j);
				list_add(&valid_coords, &p);
			}
			}
		}
	}
	pthread_mutex_lock(&npcs_m);
	struct iterator it = list_iterator(t->npcs);
	object_t *o;
	while ((o = iterator_next(&it))) {
		p.x = o->location.x;
		p.y = o->location.y;
		list_add(&valid_coords, &p);
		if (!target.x && !target.y) {
			target.x = p.x;
			target.y = p.y;
		}
	}
	it = list_iterator(c.npcs);
	while ((o = iterator_next(&it))) {
		p.x = o->location.x;
		p.y = o->location.y;
		list_add(&valid_coords, &p);
	}
	pthread_mutex_unlock(&npcs_m);
	it = list_iterator(&valid_coords);
	point_t *q;
	bool s = FALSE;
	if (!target.x && !target.y) {
	while ((q = iterator_next(&it))) {
		object_t obj;
		obj.location = *q;
		if (tile_contains(t, &obj)) {
			if (!s) {
				target = *q;
				s = TRUE;
			} else if (DISTANCE(*q, bot.location) < DISTANCE(target, bot.location)) {
				target = *q;
			}
		}
	}
	}
	s = dijkstra2(&valid_coords, &bot.location, &target, l);
	if (!s) {
	it = list_iterator(&valid_coords);
	point_t *q;
	while ((q = iterator_next(&it))) {
		plugin_debug("pathing", "valid coord: %i/%i\n", q->x, q->y);
	}
	}
	list_clear(&valid_coords);
	return s;
}

void teleport_path(struct list *path) {
	struct iterator it = list_iterator(path);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		/*point_t a, b;
		tile_t c_tile;
		get_current_tile(&c_tile);
		if (northof(t, &c_tile)) {
			a.x = TILE_TO_WORLD(c_tile.x + (c_tile.size / 2));
			a.y = TILE_TO_WORLD(c_tile.y + (c_tile.size - 1));
			b.x = TILE_TO_WORLD(t->x + (t->size / 2));
			b.y = TILE_TO_WORLD(t->y + 1);
		} else if (eastof(t, &c_tile)) {
			a.x = TILE_TO_WORLD(c_tile.x + (c_tile.size - 1));
			a.y = TILE_TO_WORLD(c_tile.y + (c_tile.size / 2));
			b.x = TILE_TO_WORLD(t->x + 1);
			b.y = TILE_TO_WORLD(t->y + (t->size / 2));
		} else if (southof(t, &c_tile)) {
			a.x = TILE_TO_WORLD(c_tile.x + (c_tile.size / 2));
			a.y = TILE_TO_WORLD(c_tile.y + 1);
			b.x = TILE_TO_WORLD(t->x + (t->size / 2));
			b.y = TILE_TO_WORLD(t->y + (t->size - 1));
		} else if (westof(t, &c_tile)) {
			a.x = TILE_TO_WORLD(c_tile.x + 1);
			a.y = TILE_TO_WORLD(c_tile.y + (c_tile.size / 2));
			b.x = TILE_TO_WORLD(t->x + (t->size - 1));
			b.y = TILE_TO_WORLD(t->y + (t->size / 2));
		} else {
			plugin_print("meph", "oops, somehow the tile is not next to us\n");
			return;
		}
		teleport(a.x, a.y);
		teleport(b.x, b.y);*/
		struct list coords = list_new(point_t);
		if (get_valid_teleport_coords(t, &coords)) {
			struct iterator it = list_iterator(&coords);
			point_t *p;
			while ((p = iterator_next(&it))) {
				teleport(p->x, p->y);
			}
			set_current_tile_visited();
			list_clear(&coords);
		} else {
			plugin_print("pathing", "no valid coordinates to teleport to\n");
			break;
		}
		//sleep(2);
		//sleep(1);
	}
}

int get_number_of_visited_tiles(struct list *tiles) {
	int i = 0;
	pthread_mutex_lock(&tiles_m);
	struct iterator it = list_iterator(tiles);
	tile_t *t;
	while ((t = iterator_next(&it))) {
		if (t->visited) i++;
	}
	pthread_mutex_unlock(&tiles_m);
	return i;
}

void find_level_exit(const char *level, const char *exit) {
	time_t t_start;
	time(&t_start);
	n_runs++;
	plugin_print("pathing", "trying to find exit %s in level %s\n", exit, level);
	int id = level_get_id_from_string(level);
	if (id < 0) {
		plugin_print("pathing", "failed to retrieve level ID\n");
		return;
	}
	c_tiles = &tiles[id];
	c_tiles_data = &tiles_data[id];
	tile_t c, t;
	if (!get_current_tile(&c)) {
		plugin_print("pathing", "failed to locate bot\n");
		return;
	}
	tile_t start = c; // statistic direct path
	set_current_tile_visited();
	dump_tiles(c_tiles);
	int moved_since = 0;
	while (get_next_tile(&c, &t, exit)) {
		plugin_print("pathing", "moving from %i/%i (%i/%i) to %i/%i (%i/%i)\n", c.x, c.y, TILE_TO_WORLD(c.x), TILE_TO_WORLD(c.y), t.x, t.y, TILE_TO_WORLD(t.x), TILE_TO_WORLD(t.y));
		struct list path = list_new(tile_t);
		if (!dijkstra(&c, &t, &path)) {
			plugin_debug("pathing", "dijkstra failed\n");
			return;
		}
		teleport_path(&path);
		list_clear(&path);
		tile_t c_tile;
		if (!get_current_tile(&c_tile)) {
			plugin_print("pathing", "failed to locate bot\n");
			break;
		}
		if (tile_compare(&c, &c_tile)) moved_since++;
		else moved_since = 0;
		c = c_tile;
		//sleep(2);
		dump_tiles(c_tiles);
		exit_t e;
		if (tile_check_for_exit(&c, exit, &e)) {
			n_found++;
			plugin_print("pathing", "found level exit %s\n", exit);
			teleport(e.object.location.x, e.object.location.y);
			msleep(300);
			d2gs_send(0x13, "%d %d", 0x05, e.object.id);
			plugin_print("pathing", "took level exit %s\n", exit);
			update_tile_data(c_tiles_data, c_tiles, &e);

			// statistic direct path
			if (dijkstra(&start, &c, &path)) {
				int n_optimal = list_size(&path) + 1;
				int n_nodes = get_number_of_visited_tiles(c_tiles);
				n_direct_path += PERCENT(n_nodes, n_optimal);
			}
			list_clear(&path);

			time_t t_cur;
			t_total += (int) difftime(time(&t_cur), t_start);

			return;
		}
		// give the server some time to add tiles and objects
		msleep(300);
		if (moved_since > 2) {
			n_stuck++;
			plugin_print("pathing", "bot got stuck - cancel search\n");
			break;
		}
	}
	plugin_error("pathing", "failed to find exit %s in level %s\n", exit, level);
	update_tile_data(c_tiles_data, c_tiles, NULL);
}

void find_level_exit_extension(char *caller, ...) {
	va_list vl;
	va_start(vl, caller);
	const char *level = va_arg(vl, const char *);
	const char *exit = va_arg(vl, const char *);
	va_end(vl);
	find_level_exit(level, exit);
}

int d2gs_char_location_update(void *);
int process_incoming_packet(void *);

_export const char * module_get_title() {
	return "pathing";
}

_export const char * module_get_version() {
	return "0.1.0";
}

_export const char * module_get_author() {
	return "gonzoj";
}

_export const char * module_get_description() {
	return "offers an extension for other plugins that allows pathfinding in dungeons";
}

_export int module_get_license() {
	return LICENSE_GPL_V3;
}

_export module_type_t module_get_type() {
	return (module_type_t) { MODULE_D2GS, MODULE_PASSIVE };
}

_export bool module_load_config(struct setting_section *s) {
	int i;
	for (i = 0; i < s->entries; i++) {
		struct setting *set = module_setting(s->settings[i].name);
		if (set) {
			/*if (s->settings[i].type == STRING) {
				set->s_var = strdup(s->settings[i].s_var);
				if (set->s_var) {
					setting_cleanup_t sc = { cleanup_string_setting, set };
					list_add(&setting_cleaners, &sc);
				}
			}*/
		}
	}
	return TRUE;
}

_export bool module_init() {
	register_packet_handler(D2GS_RECEIVED, 0x51, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x07, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x09, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x5a, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0x59, process_incoming_packet);
	register_packet_handler(D2GS_RECEIVED, 0xac, process_incoming_packet);

	register_packet_handler(D2GS_RECEIVED, 0x15, d2gs_char_location_update);
	register_packet_handler(D2GS_RECEIVED, 0x95, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x01, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x03, d2gs_char_location_update);
	register_packet_handler(D2GS_SENT, 0x0c, d2gs_char_location_update);

	int i;
	for (i = 0; i < 0xFF; i++) {
		tiles[i] = list_new(tile_t);
		tiles_data[i] = list_new(tile_data_t);
	}
	exits = list_new(exit_t);

	pthread_mutex_init(&tiles_m, NULL);
	pthread_mutex_init(&objects_m, NULL);
	pthread_mutex_init(&npcs_m, NULL);
	pthread_mutex_init(&exits_m, NULL);
	pthread_mutex_init(&teleport_m, NULL);
	pthread_cond_init(&teleport_cv, NULL);

	load_tile_data(tiles_data, "tiles/");

	register_extension("find_level_exit", find_level_exit_extension);

	return TRUE;
}

_export bool module_finit() {
	unregister_packet_handler(D2GS_RECEIVED, 0x51, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x07, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x09, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x5a, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0x59, process_incoming_packet);
	unregister_packet_handler(D2GS_RECEIVED, 0xac, process_incoming_packet);

	unregister_packet_handler(D2GS_RECEIVED, 0x15, d2gs_char_location_update);
	unregister_packet_handler(D2GS_RECEIVED, 0x95, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x01, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x03, d2gs_char_location_update);
	unregister_packet_handler(D2GS_SENT, 0x0c, d2gs_char_location_update);

	save_tile_data(tiles_data, "tiles/");

	int i;
	for (i = 0; i < 0xFF; i++) {
		struct iterator it = list_iterator(&tiles[i]);
		tile_t *t;
		while (( t = iterator_next(&it))) {
			tile_destroy(t);
		}
		list_clear(&tiles[i]);
		list_clear(&tiles_data[i]);
	}
	list_clear(&exits);

	pthread_mutex_destroy(&tiles_m);
	pthread_mutex_destroy(&objects_m);
	pthread_mutex_destroy(&npcs_m);
	pthread_mutex_destroy(&exits_m);
	pthread_mutex_destroy(&teleport_m);
	pthread_cond_destroy(&teleport_cv);

	// statistics
	ui_add_statistics_plugin("pathing", "runs: %i\n", n_runs);
	ui_add_statistics_plugin("pathing", "found: %i (%i%%)\n", n_found, PERCENT(n_runs, n_found));
	ui_add_statistics_plugin("pathing", "stuck: %i (%i%%)\n", n_stuck, PERCENT(n_runs, n_stuck));
	ui_add_statistics_plugin("pathing", "tiles discovered: %i\n", n_tiles_discovered);
	ui_add_statistics_plugin("pathing", "direct path: %i%%\n", n_runs ? n_direct_path / n_runs : 0);
	char *s_average;
	s_average = string_format_time(n_found ? t_total / n_found : 0);
	ui_add_statistics_plugin("pathing", "average time to find exit: %s\n", s_average);
	free(s_average);

	return TRUE;
}

int d2gs_char_location_update(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch (packet->id) {

		case 0x15: { // received packet
			if (bot.id == net_get_data(packet->data, 1, dword)) {
				word x = bot.location.x;
				word y = bot.location.y;
				bot.location.x = net_get_data(packet->data, 5, word);
				bot.location.y = net_get_data(packet->data, 7, word);
				if (x != bot.location.x || y != bot.location.y) {
					pthread_mutex_lock(&teleport_m);
					pthread_cond_signal(&teleport_cv);
					pthread_mutex_unlock(&teleport_m);
				}
				// by adding the bot (coordinates possibly corrected by the server) to the tile's npc list
				// we should be able to remember even more valid teleport spots
				tile_add_npc(bot.location.x, bot.location.y, bot.id);
			}
		}
		break;

		case 0x95: { // received packet
			bot.location.x = net_extract_bits(packet->data, 45, 15);
			bot.location.y = net_extract_bits(packet->data, 61, 15);
			tile_add_npc(bot.location.x, bot.location.y, bot.id);
		}
		break;

		case 0x01:
		case 0x03: { // sent packets
			bot.location.x = net_get_data(packet->data, 0, word);
			bot.location.y = net_get_data(packet->data, 2, word);
		}
		break;

		/*case 0x0c: { // sent packet
			//if (cur_rskill == 0x36) {
				bot.location.x = net_get_data(packet->data, 0, word);
				bot.location.y = net_get_data(packet->data, 2, word);
			//}
		}*/
		break;

	}
	
	plugin_debug("pathing", "bot at %i/%i\n", bot.location.x, bot.location.y);

	return FORWARD_PACKET;
}

int process_incoming_packet(void *p) {
	d2gs_packet_t *packet = D2GS_CAST(p);

	switch(packet->id) {

	case 0x51: {
		word code = net_get_data(packet->data, 5, word);
		word x = net_get_data(packet->data, 7, word);
		word y = net_get_data(packet->data, 9, word);
		tile_add_object(x, y, code);
		break;
	}

	case 0xac: {
		word x = net_get_data(packet->data, 6, word);
		word y = net_get_data(packet->data, 8, word);
		dword id = net_get_data(packet->data, 0, dword);
		tile_add_npc(x, y, id);
		break;
	}

	case 0x07: {
		byte area = net_get_data(packet->data, 4, byte);
		word x = net_get_data(packet->data, 0, word);
		word y = net_get_data(packet->data, 2, word);
		tile_add(x, y, area);
		break;

	}

	case 0x09: {
		dword id = net_get_data(packet->data, 1, dword);
		byte side = net_get_data(packet->data, 5, byte);
		word x = net_get_data(packet->data, 6, word);
		word y = net_get_data(packet->data, 8, word);
		exit_add(id, x, y, side);
		break;
	}

	case 0x59: {
		word x = net_get_data(packet->data, 21, word);
		word y = net_get_data(packet->data, 23, word);
		if (!x && !y) {
			bot.id = net_get_data(packet->data, 0, dword);
		} else if (bot.id == net_get_data(packet->data, 0, dword)) {
			bot.location.x = x;
			bot.location.y = y;
			tile_add_npc(bot.location.x, bot.location.y, bot.id);
		}
		break;
	}

	}

	return FORWARD_PACKET;
}

_export void * module_thread(void *arg) {
	return NULL;
}

_export void module_cleanup() {
	memset(&bot, 0, sizeof(object_t));

	int i;
	for (i = 0; i < 0xFF; i++) {
		struct iterator it = list_iterator(&tiles[i]);
		tile_t *t;
		while (( t = iterator_next(&it))) {
			tile_destroy(t);
		}
		list_clear(&(tiles[i]));
	}
	list_clear(&exits);
}
