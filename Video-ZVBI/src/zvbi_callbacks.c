/*
 * Copyright (C) 2006-2020 T. Zoerner.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define PY_SSIZE_T_CLEAN
#include "Python.h"

#include <libzvbi.h>

#include "zvbi_callbacks.h"

// ---------------------------------------------------------------------------

/*
 * Global storage for callback function references
 */
ZvbiCallacks_t ZvbiCallbacks;

// ---------------------------------------------------------------------------

unsigned
ZvbiCallbacks_alloc( ZvbiCallacksEntry_t * p_list,
                     PyObject * p_cb, PyObject * p_data, void * p_obj )
{
    unsigned idx;

    for (idx = 0; idx < ZVBI_MAX_CB_COUNT; idx++) {
        if (p_list[idx].p_cb == NULL) {
            p_list[idx].p_cb = p_cb;
            p_list[idx].p_data = p_data;
            p_list[idx].p_obj = p_obj;

            if (p_cb)
                Py_INCREF(p_cb);
            if (p_data)
                Py_INCREF(p_data);
            break;
        }
    }
    return idx;
}

void
ZvbiCallbacks_free_by_idx( ZvbiCallacksEntry_t * p_list, unsigned idx )
{
    if (p_list[idx].p_cb != NULL) {
        Py_DECREF(p_list[idx].p_cb);
        p_list[idx].p_cb = NULL;
    }
    if (p_list[idx].p_data != NULL) {
        Py_DECREF(p_list[idx].p_data);
        p_list[idx].p_data = NULL;
    }
    p_list[idx].p_obj = NULL;
}

unsigned
ZvbiCallbacks_free_by_ptr( ZvbiCallacksEntry_t * p_list, void * p_obj,
                           PyObject * p_cb, PyObject * p_data, vbi_bool cmp_data )
{
    unsigned match_idx = ZVBI_MAX_CB_COUNT;

    for (unsigned idx = 0; idx < ZVBI_MAX_CB_COUNT; idx++) {
        if ((p_list[idx].p_obj == p_obj) &&
            (p_list[idx].p_cb == p_cb) &&
            (!cmp_data || (p_list[idx].p_data == p_data)))
        {
            ZvbiCallbacks_free_by_idx(p_list, idx);

            match_idx = idx;
        }
    }
    return match_idx;
}

void
ZvbiCallbacks_free_by_obj( ZvbiCallacksEntry_t * p_list, void * p_obj )
{
    for (unsigned idx = 0; idx < ZVBI_MAX_CB_COUNT; idx++) {
        if (p_list[idx].p_obj == p_obj) {
            ZvbiCallbacks_free_by_idx(p_list, idx);
        }
    }
}

vbi_bool
ZvbiCallbacks_CheckObj( PyObject * cb_obj )
{
    if ((cb_obj != NULL) && !PyCallable_Check(cb_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback parameter is not a callable object");
        return FALSE;
    }
    return TRUE;
}
