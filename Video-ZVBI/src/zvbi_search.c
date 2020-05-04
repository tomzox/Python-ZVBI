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
#include "zvbi_page.h"

// ---------------------------------------------------------------------------
//  Search
// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_search * ctx;
} ZvbiSearchObj;

PyObject * ZvbiSearchError;

// ---------------------------------------------------------------------------

#if 0
/*
 * Invoke callback for the search in the teletext cache
 * Callback can return FALSE to abort the search.
 */
static int
zvbi_xs_search_progress( vbi_page * p_pg, unsigned cb_idx )
{
        dMY_CXT;
        SV * perl_cb;
        SV * sv;
        VbiPageObj * pg_obj;

        I32  count;
        int  result = TRUE;

        if ( (cb_idx < ZVBI_MAX_CB_COUNT) &&
             ((perl_cb = ZvbiCallbacks.search[cb_idx].p_cb) != NULL) ) {
                dSP ;
                ENTER ;
                SAVETMPS ;

                Newxz(pg_obj, 1, VbiPageObj);
                pg_obj->do_free_pg = FALSE;
                pg_obj->p_pg = p_pg;

                sv = newSV(0);
                sv_setref_pv(sv, "Video::ZVBI::page", (void*)pg_obj);

                /* push the function parameters on the Perl interpreter stack */
                PUSHMARK(SP) ;
                XPUSHs(sv_2mortal (sv));
                if (ZvbiCallbacks.search[cb_idx].p_data != NULL) {
                        XPUSHs(ZvbiCallbacks.search[cb_idx].p_data);
                }
                PUTBACK ;

                /* invoke the Perl subroutine */
                count = call_sv(perl_cb, G_SCALAR) ;

                SPAGAIN ;

                if (count == 1) {
                        result = POPi;
                }

                FREETMPS ;
                LEAVE ;
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

#endif  // 0

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
#if 0
    static char * kwlist[] = {NULL};
    vbi_decoder * vbi
    vbi_pgno pgno
    vbi_subno subno
    SV * sv_pattern
    vbi_bool casefold
    vbi_bool regexp
    CV * progress
    SV * user_data

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_search_delete(self->ctx);
        self->ctx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {
        uint16_t * p_ucs;
        uint16_t * p;
        char * p_utf;
        const char * p_utf_end;
        STRLEN len;
        int rest;
        unsigned cb_idx;

        /* convert pattern string from Perl's utf8 into UCS-2 */
        p_utf = SvPVutf8_force(sv_pattern, len);
        p_utf_end = p_utf + len;
        Newx(p_ucs, len * 2 + 2, uint16_t);
        p = p_ucs;
        rest = len;
        while (rest > 0) {
            *(p++) = utf8_to_uvchr_buf((U8*)p_utf, p_utf_end, &len);
            if (len > 0) {
                p_utf += len;
                rest -= len;
            }
            else {
                break;
            }
        }
        *p = 0;
        if (progress == NULL) {
            self->ctx = vbi_search_new(self->ctx, pgno, subno, p_ucs, casefold, regexp, NULL);
        }
        else {
            cb_idx = ZvbiCallbacks_alloc(ZvbiCallbacks.search, (SV*)progress, user_data, NULL);
            if (cb_idx < ZVBI_MAX_CB_COUNT) {
                self->ctx = vbi_search_new(self->ctx, pgno, subno, p_ucs, casefold, regexp,
                                           zvbi_xs_search_cb_list[cb_idx]);

                if (RETVAL != NULL) {
                        ZvbiCallbacks.search[cb_idx].p_obj = RETVAL;
                } else {
                        ZvbiCallbacks_free_by_idx(ZvbiCallbacks.search, cb_idx);
                }
            }
            else {
                PyErr_SetString(ZvbiSearchError, "Max. search callback count exceeded");
            }
        }
        Safefree(p_ucs);

        if (self->ctx != NULL) {
            RETVAL = 0;
        }
        else {
            PyErr_SetString(ZvbiSearchError, "failed to create search object");
        }
    }
#endif
    return RETVAL;
}

static PyObject *
ZvbiSearch_next(ZvbiSearchObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int direction = 1;  // -1 backward, +1 forward

    if (PyArg_ParseTuple(args, "|i", &direction)) {
        vbi_page * page = NULL;
        int st = vbi_search_next(self->ctx, &page, direction);

        if ((st == VBI_SEARCH_SUCCESS) && (page != NULL)) {
            RETVAL = ZvbiPage_New(page, FALSE);
        }
        else {
            const char * str;
            switch (st)
            {
                default:
                case VBI_SEARCH_ERROR:       str = "error during search"; break;
                case VBI_SEARCH_CACHE_EMPTY: str = "cache empty"; break;
                case VBI_SEARCH_CANCELED:    str = "cancelled"; break;
                case VBI_SEARCH_NOT_FOUND:   str = "no page found"; break;
                case VBI_SEARCH_SUCCESS:     str = "success"; break;
            }
            PyErr_SetString(ZvbiSearchError, str);
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiSearch_MethodsDef[] =
{
    {"next",           (PyCFunction) ZvbiSearch_next,           METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

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
    .tp_methods = ZvbiSearch_MethodsDef,
    //.tp_members = ZvbiSearch_Members,
};

// TODO iterator

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
