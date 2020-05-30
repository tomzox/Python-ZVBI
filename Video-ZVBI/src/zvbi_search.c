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

#include "zvbi_search.h"
#include "zvbi_callbacks.h"
#include "zvbi_service_dec.h"
#include "zvbi_page.h"

// ---------------------------------------------------------------------------
//  Search
// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_search * ctx;
    int direction;
} ZvbiSearchObj;

static PyObject * ZvbiSearchError;

/*
 * This counter is used for limiting the life-time of page objects that refer
 * to static storage in the libzvbi library. The object encapsulates a copy of
 * the counter at the time of duration. The counter is incremented for any
 * operation that invalidates the page content. Access to the object is
 * rejected via exception when the counter no longer matches the object.
 */
static int ZvbiSearch_TempPageSeqNo;

// ---------------------------------------------------------------------------

/*
 * Invoke callback for the search in the teletext cache
 * Callback can return FALSE to abort the search.
 */
static int
zvbi_xs_search_progress( vbi_page * p_pg, unsigned cb_idx )
{
    PyObject * cb_obj;
    int result = FALSE;

    if ( (cb_idx < ZVBI_MAX_CB_COUNT) &&
         ((cb_obj = ZvbiCallbacks.search[cb_idx].p_cb) != NULL) )
    {
        // invalidate wrapper object returned by previous callbacks or search results
        ZvbiSearch_TempPageSeqNo++;

        PyObject * pg_obj = ZvbiPage_NewTemporary(p_pg, &ZvbiSearch_TempPageSeqNo);
        if (pg_obj != NULL) {
            PyObject * user_data = ZvbiCallbacks.search[cb_idx].p_data;

            // invoke the Python subroutine
            PyObject * cb_rslt = PyObject_CallFunctionObjArgs(cb_obj, pg_obj, user_data, NULL);

            // evaluate the result returned by the function
            if (cb_rslt != NULL) {
                result = (PyObject_IsTrue(cb_rslt) == 1);
                Py_DECREF(cb_rslt);
            }
            Py_DECREF(pg_obj);

            // invalidate page wrapper object: life-time of page is duration of callback only
            ZvbiSearch_TempPageSeqNo++;
        }

        // clear exceptions as we cannot handle them here
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
    }
    return result;
}

/*
 * The search callbacks don't support user data, so instead of passing an
 * array index through, the index is resurrected by using different functions.
 */
static int zvbi_xs_search_progress_0( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 0); }
static int zvbi_xs_search_progress_1( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 1); }
static int zvbi_xs_search_progress_2( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 2); }
static int zvbi_xs_search_progress_3( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 3); }
static int zvbi_xs_search_progress_4( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 4); }
static int zvbi_xs_search_progress_5( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 5); }
static int zvbi_xs_search_progress_6( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 6); }
static int zvbi_xs_search_progress_7( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 7); }
static int zvbi_xs_search_progress_8( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 8); }
static int zvbi_xs_search_progress_9( vbi_page * p_pg ) { return zvbi_xs_search_progress(p_pg, 9); }

static int (* const zvbi_xs_search_cb_list[ZVBI_MAX_CB_COUNT])( vbi_page * pg ) =
{
    zvbi_xs_search_progress_0,
    zvbi_xs_search_progress_1,
    zvbi_xs_search_progress_2,
    zvbi_xs_search_progress_3,
    zvbi_xs_search_progress_4,
    zvbi_xs_search_progress_5,
    zvbi_xs_search_progress_6,
    zvbi_xs_search_progress_7,
    zvbi_xs_search_progress_8,
    zvbi_xs_search_progress_9,
};
#if (ZVBI_MAX_CB_COUNT) != 10
#error "Search progress callback function list length mismatch"
#endif

// ---------------------------------------------------------------------------

static PyObject *
ZvbiSearch_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiSearch_dealloc(ZvbiSearchObj *self)
{
    if (self->ctx) {
        vbi_search_delete(self->ctx);
    }
    ZvbiCallbacks_free_by_obj(ZvbiCallbacks.search, self);

    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiSearch_init(ZvbiSearchObj *self, PyObject *args, PyObject *kwds)
{
    int RETVAL = -1;
    static char * kwlist[] = {"decoder", "pattern", "page", "subno",
                              "casefold", "regexp", "direction",
                              "progress", "userdata", NULL};
    PyObject * dec_obj = NULL;
    PyObject * pattern_obj = NULL;
    int pgno = 0x100;
    int subno = VBI_ANY_SUBNO;
    int casefold = FALSE;
    int regexp = FALSE;
    int direction = 1;
    PyObject * progress = NULL;
    PyObject * user_data = NULL;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_search_delete(self->ctx);
        self->ctx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O!U|ii$ppIOO", kwlist,
                                    &ZvbiServiceDecTypeDef, &dec_obj, &pattern_obj,
                                    &pgno, &subno, &casefold, &regexp, &direction,
                                    &progress, &user_data) &&
        ZvbiCallbacks_CheckObj(progress))
    {
        vbi_decoder * dec = ZvbiServiceDec_GetBuf(dec_obj);
        if (dec != NULL) {
            self->direction = direction;

            // ensure "canonical representation" of Unicode object, needed for READ macro
            if (PyUnicode_READY(pattern_obj) == 0) {
                void * ucs4_buf = PyUnicode_DATA(pattern_obj);
                Py_ssize_t ucs4_len = PyUnicode_GET_LENGTH(pattern_obj); // in code points (not bytes)
                int ucs4_kind = PyUnicode_KIND(pattern_obj);
                uint16_t * ucs2 = (uint16_t*) PyMem_RawMalloc((ucs4_len + 1) * 2);
                uint16_t * p = ucs2;

                // convert pattern string from UCS-4 into UCS-2
                for (Py_ssize_t idx = 0; idx < ucs4_len; ++idx) {
                    Py_UCS4 c = PyUnicode_READ(ucs4_kind, ucs4_buf, idx);
                    if (c < 0x10000) {
                        *(p++) = (uint16_t) c;
                    }
                    else {
                        *(p++) = 0x20;
                    }
                }
                *p = 0;

                // invalidate wrapper object returned by previous search results
                ZvbiSearch_TempPageSeqNo++;

                if (progress == NULL) {
                    self->ctx = vbi_search_new(dec, pgno, subno, ucs2, casefold, regexp, NULL);
                    if (self->ctx != NULL) {
                        RETVAL = 0;
                    }
                    else {
                        PyErr_SetString(ZvbiSearchError, "failed to create search object");
                    }
                }
                else {
                    unsigned cb_idx = ZvbiCallbacks_alloc(ZvbiCallbacks.search, progress, user_data, self);
                    if (cb_idx < ZVBI_MAX_CB_COUNT) {
                        self->ctx = vbi_search_new(dec, pgno, subno, ucs2, casefold, regexp,
                                                   zvbi_xs_search_cb_list[cb_idx]);

                        if (self->ctx != NULL) {
                            RETVAL = 0;
                        }
                        else {
                            PyErr_SetString(ZvbiSearchError, "failed to create search object");
                            ZvbiCallbacks_free_by_idx(ZvbiCallbacks.search, cb_idx);
                        }
                    }
                    else {
                        PyErr_SetString(ZvbiSearchError, "Max. search callback count exceeded");
                    }
                }
                PyMem_RawFree(ucs2);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiSearch_Iter(ZvbiSearchObj *self)
{
    Py_INCREF(self);  // Note corresponding DECREF is done by caller after end of iteration
    return (PyObject *) self;
}

/*
 * Implementation of the standard "__next__" function
 */
static PyObject *
ZvbiSearch_IterNext(ZvbiSearchObj *self)
{
    PyObject * RETVAL = NULL;

    // invalidate wrapper object returned by previous search results
    ZvbiSearch_TempPageSeqNo++;

    vbi_page * page = NULL;
    int st = vbi_search_next(self->ctx, &page, self->direction);

    if ((st == VBI_SEARCH_SUCCESS) && (page != NULL)) {
        RETVAL = ZvbiPage_NewTemporary(page, &ZvbiSearch_TempPageSeqNo);
    }
    else if (st == VBI_SEARCH_NOT_FOUND) {
        PyErr_SetString(PyExc_StopIteration, "no page found");
    }
    else if (st == VBI_SEARCH_CANCELED) {
        PyErr_SetString(PyExc_StopIteration, "search cancelled by callback");
    }
    else {
        const char * str;
        switch (st)
        {
            default:
            case VBI_SEARCH_ERROR:       str = "error during search"; break;
            case VBI_SEARCH_CACHE_EMPTY: str = "cache empty"; break;
            case VBI_SEARCH_CANCELED:    str = "cancelled"; break;  // never reached
            case VBI_SEARCH_NOT_FOUND:   str = "no page found"; break;  // never reached
            case VBI_SEARCH_SUCCESS:     str = "success"; break;  // never reached
        }
        PyErr_SetString(ZvbiSearchError, str);
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyTypeObject ZvbiSearchTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.Search",
    .tp_doc = PyDoc_STR("Class for searching within teletext page content"),
    .tp_basicsize = sizeof(ZvbiSearchObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiSearch_new,
    .tp_init = (initproc) ZvbiSearch_init,
    .tp_dealloc = (destructor) ZvbiSearch_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiSearch_Repr,
    //.tp_methods = ZvbiSearch_MethodsDef,
    .tp_iter = (getiterfunc) ZvbiSearch_Iter,
    .tp_iternext = (iternextfunc) ZvbiSearch_IterNext,
};

int PyInit_Search(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiSearchTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiSearchError = PyErr_NewException("Zvbi.SearchError", error_base, NULL);
    Py_XINCREF(ZvbiSearchError);
    if (PyModule_AddObject(module, "SearchError", ZvbiSearchError) < 0) {
        Py_XDECREF(ZvbiSearchError);
        Py_CLEAR(ZvbiSearchError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiSearchTypeDef);
    if (PyModule_AddObject(module, "Search", (PyObject *) &ZvbiSearchTypeDef) < 0) {
        Py_DECREF(&ZvbiSearchTypeDef);
        Py_XDECREF(ZvbiSearchError);
        Py_CLEAR(ZvbiSearchError);
        return -1;
    }

    return 0;
}
