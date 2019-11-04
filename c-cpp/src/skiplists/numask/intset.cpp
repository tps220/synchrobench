/*
 * File:
 *   intset.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip list integer set operations
 *
 * Copyright (c) 2009-2010.
 *
 * intset.c is part of Synchrobench
 *
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "search.h"
#include "intset.h"
#include "nohotspot_ops.h"
#include "gc.h"

#define MAXLEVEL    32
extern gc_t* gc;

int sl_contains_old(search_layer *sl, unsigned int key, int transactional)
{
		gc_crit_enter(gc);
        int retval = sl_contains(sl, (sl_key_t) key);
        gc_crit_exit(gc);
        return retval;
}

int sl_add_old(search_layer *sl, unsigned int key, int transactional)
{
		gc_crit_enter(gc);
        int retval = sl_insert(sl, (sl_key_t) key, (val_t) ((long)key));
        gc_crit_exit(gc);
        return retval;
}

int sl_remove_old(search_layer *sl, unsigned int key, int transactional)
{
	gc_crit_enter(gc);
	int retval = sl_delete(sl, (sl_key_t) key);
	gc_crit_exit(gc);
	return retval;
}
