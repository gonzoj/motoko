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

//#include <pthread.h>

#include <stdlib.h>
#include <string.h>

#include "util/list.h"

struct list list_create(size_t sizeof_type) {
	struct list l;

	l = (struct list) { NULL, sizeof_type, 0 };
	//pthread_mutex_init(&l.mutex, NULL);

	return l;
}

bool list_empty(struct list *l) {
	return l->len ? FALSE : TRUE;
}

unsigned list_size(struct list *l) {
	return l->len;
}

bool list_add(struct list *l, void *e) {
	//pthread_mutex_lock(&l->mutex);

	l->elements = realloc(l->elements, ++l->len * l->size);
	if (l->elements) {
		memcpy(l->elements + (l->len - 1) * l->size, e, l->size);

		//pthread_mutex_unlock(&l->mutex);

		return TRUE;
	}

	//pthread_mutex_unlock(&l->mutex);

	return FALSE;
}

bool list_remove(struct list *l, void *e) {
	/*unsigned new_len = 0;
	void *i;
	void *tmp = NULL;
	int index;

	pthread_mutex_lock(&l->mutex);

	for (index = 0; index < l->len; index++) {
		i = l->elements + index * l->size;
		if (i != e) {
			tmp = realloc(tmp, l->size * ++new_len);
			if (!tmp) {

				pthread_mutex_unlock(&l->mutex);

				return FALSE;
			}
			memcpy(tmp + (new_len - 1) * l->size, i, l->size);
		}
	}
	free(l->elements);
	l->elements = tmp;
	l->len = new_len;

	pthread_mutex_unlock(&l->mutex);

	return TRUE;*/

	// optimized implementation

	//pthread_mutex_lock(&l->mutex);

	int offset = e - l->elements;

	if ((offset < 0) || (offset >= l->len * l->size) || (offset % l->size != 0))  {

		//pthread_mutex_unlock(&l->mutex);

		return FALSE;
	}

	void *tmp = NULL;
	if (--l->len > 0) {
		tmp = malloc(l->size * l->len);
		if (offset > 0) {
			memcpy(tmp, l->elements, offset);
		}
		if ((l->len * l->size) - offset > 0) {
			memcpy(tmp + offset, e + l->size, (l->len * l->size) - offset);
		}
	}
	free(l->elements);
	l->elements = tmp;

	//pthread_mutex_unlock(&l->mutex);

	return TRUE;
}

bool list_clear(struct list *l) {
	if (l->len) {
		free(l->elements);
		l->elements = NULL;
		l->len = 0;
	}
	//pthread_mutex_destroy(&l->mutex);

	return TRUE;
}

void list_sort(struct list *l, int (*comparator)(const void *, const void *)) {
	//pthread_mutex_lock(&l->mutex);

	qsort(l->elements, (size_t) l->len, l->size, comparator);

	//pthread_mutex_unlock(&l->mutex);
}

struct list * list_clone(struct list *l, struct list *c) {
	//struct list *c = (struct list *) malloc(sizeof(struct list));

	//pthread_mutex_lock(&l->mutex);

	c->len = l->len;
	c->size = l->size;
	c->elements = malloc(c->size * c->len);
	memcpy(c->elements, l->elements, c->size * c->len);

	//pthread_mutex_unlock(&l->mutex);

	//pthread_mutex_init(&c->mutex, NULL);

	return c;
}

// shouldn't be used
void * list_iterate(struct list *l) {
	static int i = 0;
	static bool reset = TRUE;

	if (!l) {
		i = 0;
		reset = TRUE;
		return NULL;
	}
	if (!l->len || ((i &&  !(i % l->len)) && reset)) {
		reset = FALSE;
		return NULL;
	} else {
		reset = TRUE;
		return (l->elements + (i++ % l->len) * l->size);
	}
}

void * list_element(struct list *l, int index) {
	// return (index >= l->len) ? NULL : l->elements + index * l->size;

	void *e;

	//pthread_mutex_lock(&l->mutex);

	if (index >= l->len) {
		e = NULL;
	} else {
		//e = malloc(l->size);
		//memcpy(e, l->elements + index * l->size, l->size);
		e = l->elements + index * l->size;
	}

	//pthread_mutex_unlock(&l->mutex);

	return e;
}

void * list_find(struct list *l, int (*comparator)(const void *, const void *), void *e) {
	int index;

	//pthread_mutex_lock(&l->mutex);

	for (index = 0; index < l->len; index++) {
		void *i = l->elements + index * l->size;
		if (comparator(i, e)) {

			//i = malloc(l->size);
			//memcpy(i, l->elements + index * l->size, l->size);

			//pthread_mutex_unlock(&l->mutex);

			return i;
		}
	}

	//pthread_mutex_unlock(&l->mutex);

	return NULL;
}

struct iterator list_iterator(struct list *l) {
	return (struct iterator) { l, 0, NULL };
}

void * iterator_next(struct iterator *i) {
	/*if (i->element) {
		free(i->element);
	}*/

	i->element = list_element(i->list, i->index++);

	return i->element;
}

void iterator_remove(struct iterator *i) {
	list_remove(i->list, i->element);
	i->index--;
}

void iterator_destroy(struct iterator *i) {
	/*if (i->element) {
		free(i->element);
	}*/
}
