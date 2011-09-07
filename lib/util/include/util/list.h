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

#ifndef LIST_H_
#define LIST_H_

//#include <pthread.h>
#include <unistd.h>

#include "util/types.h"

/* generic arraylist implementation */

struct list {
	void *elements;
	size_t size;
	unsigned len;
	//pthread_mutex_t mutex;
};

typedef int (*comparator_t)(const void *, const void *);

#define LIST(e, s, l) (struct list) { (void *) e, sizeof(s), l }

struct list list_create(size_t);

#define list_new(t) list_create(sizeof(t)) // creates an empty list for objects of type t

bool list_empty(struct list *); // returns TRUE if list is empty, FALSE otherwise

unsigned list_size(struct list *); // returns the number of objects stored in list

// synchronized
bool list_add(struct list *, void *); // adds a new object to list

// synchronized; when removing objects while iterating use iterator_remove() instead
bool list_remove(struct list *, void *); // removes an object already stored from list (based on address)

// caller must guarantee that no synchronized function operates simultaneously on the same list
bool list_clear(struct list *); // releases resources occupied by list; list remains ready for use

// synchronized
void list_sort(struct list *, int (*comparator)(const void *, const void *)); // sorts list in the way defined by comparator

// synchronized
struct list * list_clone(struct list *, struct list *); // creates a copy of list

// shouldn't be used; use list_iterator() instead
void * list_iterate(struct list *);

// synchronized; caller has to free the returned element
void * list_element(struct list *, int); // returns the element at the specified index in list

// synchronized; caller has to free the returned element
void * list_find(struct list *, int (*comparator)(const void *, const void *), void *); // returns the first element that matches the given element

struct iterator {
	struct list *list;
	int index;
	void *element;
};

struct iterator list_iterator(struct list *); // returns an iterator for list

void * iterator_next(struct iterator *); // returns the next element of list or NULL if the end is reached

void iterator_remove(struct iterator *); // removes the current element from list

// must be called when iteration was canceled before iterator_next() returned null
void iterator_destroy(struct iterator *);

#endif /* LIST_H_ */
