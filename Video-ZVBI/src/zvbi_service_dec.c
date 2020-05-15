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

#include "zvbi_service_dec.h"
#include "zvbi_page.h"
#include "zvbi_event_types.h"
#include "zvbi_capture_buf.h"
#include "zvbi_callbacks.h"

// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_decoder * ctx;
} ZvbiServiceDecObj;

PyObject * ZvbiServiceDecError;

// ---------------------------------------------------------------------------

vbi_decoder *
ZvbiServiceDec_GetBuf(PyObject * self)
{
    assert(PyObject_IsInstance(self, (PyObject*)&ZvbiServiceDecTypeDef) == 1);
    return ((ZvbiServiceDecObj*) self)->ctx;
}

static PyObject *
ZvbiServiceDec_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiServiceDec_dealloc(ZvbiServiceDecObj *self)
{
    if (self->ctx) {
        vbi_decoder_delete(self->ctx);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiServiceDec_init(ZvbiServiceDecObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {NULL};
    int RETVAL = -1;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_decoder_delete(self->ctx);
        self->ctx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {
        self->ctx = vbi_decoder_new();

        if (self->ctx != NULL) {
            RETVAL = 0;
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "failed to create teletext decoder");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  Teletext Page De-Multiplexing & Caching
// ---------------------------------------------------------------------------

static PyObject *
ZvbiServiceDec_decode(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * sliced_obj;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "O!", &ZvbiCaptureSlicedBufTypeDef, &sliced_obj)) {
        vbi_capture_buffer * sliced_buffer = ZvbiCaptureBuf_GetBuf(sliced_obj);
        if ((sliced_buffer != NULL) && (sliced_buffer->data != NULL)) {
            vbi_sliced * p_sliced = sliced_buffer->data;
            int n_lines = sliced_buffer->size / sizeof(vbi_sliced);
            // FIXME should be valid lines only

            vbi_decode(self->ctx, p_sliced, n_lines, sliced_buffer->timestamp);
            Py_INCREF(Py_None);
            RETVAL = Py_None;
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "Sliced capture buffer contains no data");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_decode_bytes(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer in_buf;
    unsigned n_lines;
    double timestamp;

    if (PyArg_ParseTuple(args, "y*If", &in_buf, &n_lines, &timestamp)) {
        if (n_lines <= in_buf.len / sizeof(vbi_sliced)) {
            vbi_decode(self->ctx, (vbi_sliced*)in_buf.buf, n_lines, timestamp);
            Py_INCREF(Py_None);
            RETVAL = Py_None;
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "Buffer too short for given number of lines");
        }
        PyBuffer_Release(&in_buf);
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_channel_switched(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned nuid;

    if (PyArg_ParseTuple(args, "|I", &nuid)) {
        vbi_channel_switched(self->ctx, nuid);
        Py_INCREF(Py_None);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_classify_page(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int pgno = 0;

    if (PyArg_ParseTuple(args, "i", &pgno)) {
        vbi_subno subno = 0;
        char * language = NULL;
        vbi_page_type type = vbi_classify_page(self->ctx, pgno, &subno, &language);

        RETVAL = PyTuple_New(3);
        if (RETVAL != NULL)
        {
            PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(type));
            PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(subno));
            if (language != NULL) {
                PyTuple_SetItem(RETVAL, 2, PyUnicode_DecodeLatin1(language, strlen(language), NULL));
            }
            else {
                Py_INCREF(Py_None);
                PyTuple_SetItem(RETVAL, 2, Py_None);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_set_brightness(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int brightness = 0;

    if (PyArg_ParseTuple(args, "i", &brightness)) {
        vbi_set_brightness(self->ctx, brightness);
        Py_INCREF(Py_None);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_set_contrast(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int contrast = 0;

    if (PyArg_ParseTuple(args, "i", &contrast)) {
        vbi_set_contrast(self->ctx, contrast);
        Py_INCREF(Py_None);
        RETVAL = Py_None;
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  Teletext Page Caching
// ---------------------------------------------------------------------------

static PyObject *
ZvbiServiceDec_teletext_set_default_region(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int default_region = 0;

    if (PyArg_ParseTuple(args, "i", &default_region)) {
        vbi_teletext_set_default_region(self->ctx, default_region);
        Py_INCREF(Py_None);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_teletext_set_level(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int level = 0;

    if (PyArg_ParseTuple(args, "i", &level)) {
        vbi_teletext_set_level(self->ctx, level);
        Py_INCREF(Py_None);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_fetch_vt_page(ZvbiServiceDecObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"pgno", "subno", "max_level",
                              "display_rows", "navigation",
                              NULL};
    PyObject * RETVAL = NULL;
    int pgno = 0;
    int subno = VBI_ANY_SUBNO;
    int max_level = VBI_WST_LEVEL_3p5;
    int display_rows = 25;
    int navigation = 1;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "i|i$iii", kwlist,
                                    &pgno, &subno, &max_level,
                                    &display_rows, &navigation))
    {
        vbi_page * page = PyMem_RawMalloc(sizeof(vbi_page));
        if (page != NULL) {
            if (vbi_fetch_vt_page(self->ctx, page, pgno, subno,
                                  max_level, display_rows, navigation))
            {
                RETVAL = ZvbiPage_New(page, TRUE);
            }
            else {
                PyErr_SetString(ZvbiServiceDecError, "Failed to fetch page");
                PyMem_RawFree(page);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_fetch_cc_page(ZvbiServiceDecObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"pgno", "reset", NULL};
    PyObject * RETVAL = NULL;
    int pgno = 0;
    int reset = TRUE;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "i|p", kwlist, &pgno, &reset)) {
        vbi_page * page = PyMem_RawMalloc(sizeof(vbi_page));
        if (page != NULL) {
            if (vbi_fetch_cc_page(self->ctx, page, pgno, reset)) {
                RETVAL = ZvbiPage_New(page, TRUE);
            }
            else {
                PyErr_SetString(ZvbiServiceDecError, "Failed to fetch page");
                PyMem_RawFree(page);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_page_title(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int pgno = 0;
    int subno = VBI_ANY_SUBNO;

    if (PyArg_ParseTuple(args, "i|i", &pgno, subno)) {
        char buf[42];
        if (vbi_page_title(self->ctx, pgno, subno, buf)) {
            RETVAL = PyUnicode_DecodeLatin1(buf, strlen(buf), NULL);
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "Failed to determine a page title");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  Event Handling
// ---------------------------------------------------------------------------

/*
 * Invoke callback for an event generated by the VT decoder
 */
static void
zvbi_xs_vt_event_handler( vbi_event * event, void * user_data )
{
    PyObject * cb_obj;
    unsigned cb_idx = PVOID2UINT(user_data);

    if ( (cb_idx < ZVBI_MAX_CB_COUNT) &&
         ((cb_obj = ZvbiCallbacks.event[cb_idx].p_cb) != NULL) )
    {
        PyObject * ev_obj = ZvbiEvent_ObjFromEvent(event);
        PyObject * cb_rslt;

        if (ev_obj != NULL) {
            // invoke the Python subroutine
            if (ZvbiCallbacks.event[cb_idx].p_data != NULL) {
                cb_rslt = PyObject_CallFunction(cb_obj, "iOO", event->type, ev_obj, ZvbiCallbacks.event[cb_idx].p_data);
            }
            else {
                cb_rslt = PyObject_CallFunction(cb_obj, "iO", event->type, ev_obj);
            }

            if (cb_rslt != NULL) {
                Py_DECREF(cb_rslt);
            }
            Py_DECREF(ev_obj);
        }

        // clear exceptions as we cannot handle them here
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
    }
}


static PyObject *
ZvbiServiceDec_event_handler_register(ZvbiServiceDecObj *self, PyObject *args)
{
    int event_mask;
    PyObject * handler_obj = NULL;
    PyObject * user_data_obj = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "iO|O", &event_mask, &handler_obj, &user_data_obj)) {
        ZvbiCallbacks_free_by_ptr(ZvbiCallbacks.event, self, handler_obj, user_data_obj, TRUE);
        unsigned cb_idx = ZvbiCallbacks_alloc(ZvbiCallbacks.event, handler_obj, user_data_obj, self);
        if (cb_idx < ZVBI_MAX_CB_COUNT) {
            if (vbi_event_handler_register(self->ctx, event_mask,
                                           zvbi_xs_vt_event_handler,
                                           UINT2PVOID(cb_idx)))
            {
                Py_INCREF(Py_None);
                RETVAL = Py_None;
            }
            else {
                PyErr_SetString(ZvbiServiceDecError, "Registration failed");
                ZvbiCallbacks_free_by_idx(ZvbiCallbacks.event, cb_idx);
            }
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "Overflow of callback table");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_event_handler_unregister(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * handler_obj = NULL;
    PyObject * user_data_obj = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "O|O", &handler_obj, &user_data_obj)) {
        unsigned cb_idx = ZvbiCallbacks_free_by_ptr(ZvbiCallbacks.event, self, handler_obj, user_data_obj, TRUE);
        vbi_event_handler_unregister(self->ctx, zvbi_xs_vt_event_handler, UINT2PVOID(cb_idx));
        Py_INCREF(Py_None);
        RETVAL = Py_None;
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiServiceDec_MethodsDef[] =
{
    {"decode",           (PyCFunction) ZvbiServiceDec_decode,           METH_VARARGS, NULL },
    {"decode_bytes",     (PyCFunction) ZvbiServiceDec_decode_bytes,     METH_VARARGS, NULL },
    {"channel_switched", (PyCFunction) ZvbiServiceDec_channel_switched, METH_VARARGS, NULL },
    {"classify_page",    (PyCFunction) ZvbiServiceDec_classify_page,    METH_VARARGS, NULL },
    {"set_brightness",   (PyCFunction) ZvbiServiceDec_set_brightness,   METH_VARARGS, NULL },
    {"set_contrast",     (PyCFunction) ZvbiServiceDec_set_contrast,     METH_VARARGS, NULL },

    {"teletext_set_default_region", (PyCFunction) ZvbiServiceDec_teletext_set_default_region, METH_VARARGS, NULL },
    {"teletext_set_level", (PyCFunction) ZvbiServiceDec_teletext_set_level, METH_VARARGS, NULL },
    {"fetch_vt_page",    (PyCFunction) ZvbiServiceDec_fetch_vt_page,    METH_VARARGS | METH_KEYWORDS, NULL },
    {"fetch_cc_page",    (PyCFunction) ZvbiServiceDec_fetch_cc_page,    METH_VARARGS | METH_KEYWORDS, NULL },
    {"page_title",       (PyCFunction) ZvbiServiceDec_page_title,       METH_VARARGS, NULL },

    // event_handler_add, event_handler_remove: omitted b/c deprecated
    {"event_handler_register",   (PyCFunction) ZvbiServiceDec_event_handler_register,   METH_VARARGS, NULL },
    {"event_handler_unregister", (PyCFunction) ZvbiServiceDec_event_handler_unregister, METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiServiceDecTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.ServiceDec",
    .tp_doc = PyDoc_STR("Class for decoding data services (i.e. Teletext page de-multiplexing & caching)"),
    .tp_basicsize = sizeof(ZvbiServiceDecObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiServiceDec_new,
    .tp_init = (initproc) ZvbiServiceDec_init,
    .tp_dealloc = (destructor) ZvbiServiceDec_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiServiceDec_Repr,
    .tp_methods = ZvbiServiceDec_MethodsDef,
    //.tp_members = ZvbiServiceDec_Members,
};

int PyInit_ServiceDec(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiServiceDecTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiServiceDecError = PyErr_NewException("Zvbi.ServiceDecError", error_base, NULL);
    Py_XINCREF(ZvbiServiceDecError);
    if (PyModule_AddObject(module, "ServiceDecError", ZvbiServiceDecError) < 0) {
        Py_XDECREF(ZvbiServiceDecError);
        Py_CLEAR(ZvbiServiceDecError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiServiceDecTypeDef);
    if (PyModule_AddObject(module, "ServiceDec", (PyObject *) &ZvbiServiceDecTypeDef) < 0) {
        Py_DECREF(&ZvbiServiceDecTypeDef);
        Py_XDECREF(ZvbiServiceDecError);
        Py_CLEAR(ZvbiServiceDecError);
        return -1;
    }

    return 0;
}
