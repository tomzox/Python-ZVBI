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
#if !defined (_PY_ZVBI_CALLBACKS_H)
#define _PY_ZVBI_CALLBACKS_H

#define PVOID2INT(X)    ((int)((long)(X)))
#define PVOID2UINT(X)   ((unsigned int)((unsigned long)(X)))
#define INT2PVOID(X)    ((void *)((long)(X)))
#define UINT2PVOID(X)   ((void *)((unsigned long)(X)))

/*
 * Structure which is used to store callback function references and user data.
 * Required because we need to replace the callback function pointer given to
 * the C library with a wrapper function which invokes the Python interpreter.
 */
typedef struct
{
    PyObject *  p_cb;
    PyObject *  p_data;
    void *      p_obj;
} ZvbiCallacksEntry_t;

#define ZVBI_MAX_CB_COUNT   10

typedef struct
{
    ZvbiCallacksEntry_t    event[ZVBI_MAX_CB_COUNT];
    ZvbiCallacksEntry_t    search[ZVBI_MAX_CB_COUNT];
    ZvbiCallacksEntry_t    log[ZVBI_MAX_CB_COUNT];
} ZvbiCallacks_t;

extern ZvbiCallacks_t ZvbiCallbacks;

unsigned ZvbiCallbacks_alloc( ZvbiCallacksEntry_t * p_list, PyObject * p_cb, PyObject * p_data, void * p_obj );

void ZvbiCallbacks_free_by_idx( ZvbiCallacksEntry_t * p_list, unsigned idx );
unsigned ZvbiCallbacks_free_by_ptr( ZvbiCallacksEntry_t * p_list, void * p_obj, PyObject * p_cb, PyObject * p_data, vbi_bool cmp_data );
void ZvbiCallbacks_free_by_obj( ZvbiCallacksEntry_t * p_list, void * p_obj );

vbi_bool ZvbiCallbacks_CheckObj( PyObject * cb_obj );

#endif  /* _PY_ZVBI_CALLBACKS_H */
