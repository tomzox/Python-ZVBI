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

#include "zvbi_idl_demux.h"
#include "zvbi_capture_buf.h"
#include "zvbi_callbacks.h"

// ---------------------------------------------------------------------------
// Independent Data Line format A (EN 300 708 section 6.5) demultiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_idl_demux_obj_struct {
    PyObject_HEAD
    vbi_idl_demux * ctx;
    PyObject *      demux_cb;
    PyObject *      demux_user_data;
} ZvbiIdlDemuxObj;

static PyObject * ZvbiIdlDemuxError;

// ---------------------------------------------------------------------------

static vbi_bool
zvbi_xs_demux_idl_handler( vbi_idl_demux *        dx,
                           const uint8_t *        buffer,
                           unsigned int           n_bytes,
                           unsigned int           flags,
                           void *                 user_data )
{
    ZvbiIdlDemuxObj * self = user_data;
    PyObject * cb_rslt;
    vbi_bool result = FALSE;

    if ((self != NULL) && (self->demux_cb != NULL)) {
        // invoke the Python subroutine with parameters
        if (self->demux_user_data != NULL) {
            cb_rslt = PyObject_CallFunction(self->demux_cb, "y#IO",
                                            buffer, n_bytes, flags,
                                            self->demux_user_data);
        }
        else {
            cb_rslt = PyObject_CallFunction(self->demux_cb, "y#I",
                                            buffer, n_bytes, flags);
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
ZvbiIdlDemux_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiIdlDemux_dealloc(ZvbiIdlDemuxObj *self)
{
    if (self->ctx) {
        vbi_idl_demux_delete(self->ctx);
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
ZvbiIdlDemux_init(ZvbiIdlDemuxObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"channel", "address", "callback", "user_data", NULL};
    unsigned channel;
    unsigned address;
    PyObject * callback = NULL;
    PyObject * user_data = NULL;
    int RETVAL = -1;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_idl_demux_delete(self->ctx);

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

    if (PyArg_ParseTupleAndKeywords(args, kwds, "IIO|O", kwlist,
                                    &channel, &address, &callback, &user_data) &&
        ZvbiCallbacks_CheckObj(callback))
    {
        self->ctx = vbi_idl_a_demux_new(channel, address, zvbi_xs_demux_idl_handler, self);

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
            PyErr_SetString(ZvbiIdlDemuxError, "Initialization failed");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiIdlDemux_reset(ZvbiIdlDemuxObj *self, PyObject *args)
{
    vbi_idl_demux_reset(self->ctx);
    Py_RETURN_NONE;
}

static PyObject *
ZvbiIdlDemux_feed(ZvbiIdlDemuxObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer feed_buf;

    if (PyArg_ParseTuple(args, "y*", &feed_buf)) {
        if (feed_buf.len >= 42) {
            if (vbi_idl_demux_feed(self->ctx, feed_buf.buf)) {
                RETVAL = Py_None;
                Py_INCREF(Py_None);
            }
            else {
                PyErr_SetString(ZvbiIdlDemuxError, "packet contains incorrectable errors");
            }
        }
        else {
            PyErr_SetString(PyExc_ValueError, "input buffer has less than 42 bytes");
        }
        PyBuffer_Release(&feed_buf);
    }
    return RETVAL;
}

static PyObject *
ZvbiIdlDemux_feed_frame(ZvbiIdlDemuxObj *self, PyObject *args)
{
    PyObject * sliced_obj = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "O!", &ZvbiCaptureSlicedBufTypeDef, &sliced_obj)) {
        vbi_capture_buffer * p_sliced = ZvbiCaptureBuf_GetBuf(sliced_obj);
        unsigned n_lines = p_sliced->size / sizeof(vbi_sliced);

        if (vbi_idl_demux_feed_frame(self->ctx, p_sliced->data, n_lines)) {
            RETVAL = Py_None;
            Py_INCREF(Py_None);
        }
        else {
            PyErr_SetString(ZvbiIdlDemuxError, "packet contains incorrectable errors");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiIdlDemux_MethodsDef[] =
{
    {"reset",      (PyCFunction) ZvbiIdlDemux_reset,      METH_NOARGS, NULL },
    {"feed",       (PyCFunction) ZvbiIdlDemux_feed,       METH_VARARGS, NULL },
    {"feed_frame", (PyCFunction) ZvbiIdlDemux_feed_frame, METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiIdlDemuxTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.IdlDemux",
    .tp_doc = PyDoc_STR("Independent Data Line format A (EN 300 708 section 6.5) demultiplexer"),
    .tp_basicsize = sizeof(ZvbiIdlDemuxObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiIdlDemux_new,
    .tp_init = (initproc) ZvbiIdlDemux_init,
    .tp_dealloc = (destructor) ZvbiIdlDemux_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiIdlDemux_Repr,
    .tp_methods = ZvbiIdlDemux_MethodsDef,
};

int PyInit_IdlDemux(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiIdlDemuxTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiIdlDemuxError = PyErr_NewException("Zvbi.IdlDemuxError", error_base, NULL);
    Py_XINCREF(ZvbiIdlDemuxError);
    if (PyModule_AddObject(module, "IdlDemuxError", ZvbiIdlDemuxError) < 0) {
        Py_XDECREF(ZvbiIdlDemuxError);
        Py_CLEAR(ZvbiIdlDemuxError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiIdlDemuxTypeDef);
    if (PyModule_AddObject(module, "IdlDemux", (PyObject *) &ZvbiIdlDemuxTypeDef) < 0) {
        Py_DECREF(&ZvbiIdlDemuxTypeDef);
        Py_XDECREF(ZvbiIdlDemuxError);
        Py_CLEAR(ZvbiIdlDemuxError);
        return -1;
    }

    return 0;
}
