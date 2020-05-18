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

#include "zvbi_xds_demux.h"
#include "zvbi_capture_buf.h"

// ---------------------------------------------------------------------------
// Extended Data Service (EIA 608) demultiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_xds_demux_obj_struct {
    PyObject_HEAD
    vbi_xds_demux * ctx;
    PyObject *      demux_cb;
    PyObject *      demux_user_data;
} ZvbiXdsDemuxObj;

static PyObject * ZvbiXdsDemuxError;

// ---------------------------------------------------------------------------

static vbi_bool
zvbi_xs_demux_xds_handler( vbi_xds_demux *        xd,
                           const vbi_xds_packet * xp,
                           void *                 user_data)
{
    ZvbiXdsDemuxObj * self = user_data;
    PyObject * cb_rslt;
    vbi_bool result = FALSE;

    if ((self != NULL) && (self->demux_cb != NULL)) {
        // invoke the Python subroutine with parameters
        if (self->demux_user_data != NULL) {
            cb_rslt = PyObject_CallFunction(self->demux_cb, "IIy#O",
                                            xp->xds_class, xp->xds_subclass,
                                            xp->buffer, xp->buffer_size,
                                            self->demux_user_data);
        }
        else {
            cb_rslt = PyObject_CallFunction(self->demux_cb, "IIy#",
                                            xp->xds_class, xp->xds_subclass,
                                            xp->buffer, xp->buffer_size);
        }

        // evaluate the boolean result
        if (cb_rslt != NULL) {
            result = (PyObject_IsTrue(cb_rslt) == 1);
            Py_DECREF(cb_rslt);
        }

        // clear exceptions as we cannot handle them here
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
    }
    return result;
}

// ---------------------------------------------------------------------------

static PyObject *
ZvbiXdsDemux_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiXdsDemux_dealloc(ZvbiXdsDemuxObj *self)
{
    if (self->ctx) {
        vbi_xds_demux_delete(self->ctx);
    }

    if (self->demux_cb != NULL) {
        Py_DECREF(self->demux_cb);
    }
    if (self->demux_user_data != NULL) {
        Py_DECREF(self->demux_user_data);
    }

    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiXdsDemux_init(ZvbiXdsDemuxObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"callback", "user_data", NULL};
    PyObject * callback = NULL;
    PyObject * user_data = NULL;
    int RETVAL = -1;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_xds_demux_delete(self->ctx);

        if (self->demux_cb != NULL) {
            Py_DECREF(self->demux_cb);
            self->demux_cb = NULL;
        }
        if (self->demux_user_data != NULL) {
            Py_DECREF(self->demux_user_data);
            self->demux_user_data = NULL;
        }
        self->ctx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O|O", kwlist,
                                     &callback, &user_data))
    {
        self->ctx = vbi_xds_demux_new(zvbi_xs_demux_xds_handler, self);

        if (self->ctx != NULL) {
            if (callback != NULL) {
                self->demux_cb = callback;
                Py_INCREF(callback);
            }
            if (user_data != NULL) {
                self->demux_user_data = user_data;
                Py_INCREF(user_data);
            }
            RETVAL = 0;
        }
        else {
            PyErr_SetString(ZvbiXdsDemuxError, "Initialization failed");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiXdsDemux_reset(ZvbiXdsDemuxObj *self, PyObject *args)
{
    vbi_xds_demux_reset(self->ctx);
    Py_RETURN_NONE;
}

static PyObject *
ZvbiXdsDemux_feed(ZvbiXdsDemuxObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer feed_buf;

    if (PyArg_ParseTuple(args, "y*", &feed_buf)) {
        if (feed_buf.len >= 2) {
            if (vbi_xds_demux_feed(self->ctx, feed_buf.buf)) {
                RETVAL = Py_None;
                Py_INCREF(Py_None);
            }
            else {
                PyErr_SetString(ZvbiXdsDemuxError, "parity errors");
            }
        }
        else {
            PyErr_SetString(PyExc_ValueError, "input buffer has less than 2 bytes");
        }
        PyBuffer_Release(&feed_buf);
    }
    return RETVAL;
}

static PyObject *
ZvbiXdsDemux_feed_frame(ZvbiXdsDemuxObj *self, PyObject *args)
{
    PyObject * sliced_obj = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "O!", &ZvbiCaptureSlicedBufTypeDef, &sliced_obj)) {
        vbi_capture_buffer * p_sliced = ZvbiCaptureBuf_GetBuf(sliced_obj);
        unsigned n_lines = p_sliced->size / sizeof(vbi_sliced);

        if (vbi_xds_demux_feed_frame(self->ctx, p_sliced->data, n_lines)) {
            RETVAL = Py_None;
            Py_INCREF(Py_None);
        }
        else {
            PyErr_SetString(ZvbiXdsDemuxError, "parity errors");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiXdsDemux_MethodsDef[] =
{
    {"reset",      (PyCFunction) ZvbiXdsDemux_reset,      METH_NOARGS, NULL },
    {"feed",       (PyCFunction) ZvbiXdsDemux_feed,       METH_VARARGS, NULL },
    {"feed_frame", (PyCFunction) ZvbiXdsDemux_feed_frame, METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiXdsDemuxTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.XdsDemux",
    .tp_doc = PyDoc_STR("Extended Data Service (EIA 608) demultiplexer"),
    .tp_basicsize = sizeof(ZvbiXdsDemuxObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiXdsDemux_new,
    .tp_init = (initproc) ZvbiXdsDemux_init,
    .tp_dealloc = (destructor) ZvbiXdsDemux_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiXdsDemux_Repr,
    .tp_methods = ZvbiXdsDemux_MethodsDef,
};

int PyInit_XdsDemux(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiXdsDemuxTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiXdsDemuxError = PyErr_NewException("Zvbi.XdsDemuxError", error_base, NULL);
    Py_XINCREF(ZvbiXdsDemuxError);
    if (PyModule_AddObject(module, "XdsDemuxError", ZvbiXdsDemuxError) < 0) {
        Py_XDECREF(ZvbiXdsDemuxError);
        Py_CLEAR(ZvbiXdsDemuxError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiXdsDemuxTypeDef);
    if (PyModule_AddObject(module, "XdsDemux", (PyObject *) &ZvbiXdsDemuxTypeDef) < 0) {
        Py_DECREF(&ZvbiXdsDemuxTypeDef);
        Py_XDECREF(ZvbiXdsDemuxError);
        Py_CLEAR(ZvbiXdsDemuxError);
        return -1;
    }

    return 0;
}
