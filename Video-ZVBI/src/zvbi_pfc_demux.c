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

#include "zvbi_pfc_demux.h"
#include "zvbi_capture_buf.h"

// ---------------------------------------------------------------------------
// Page Function Clear (ETS 300 708 section 4) demultiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_pfc_demux_obj_struct {
    PyObject_HEAD
    vbi_pfc_demux * ctx;
    PyObject *      demux_cb;
    PyObject *      demux_user_data;
} ZvbiPfcDemuxObj;

static PyObject * ZvbiPfcDemuxError;

// ---------------------------------------------------------------------------

static vbi_bool
zvbi_xs_demux_pfc_handler( vbi_pfc_demux *        dx,
                           void *                 user_data,
                           const vbi_pfc_block *  block )
{
    ZvbiPfcDemuxObj * self = user_data;
    PyObject * cb_rslt;
    vbi_bool result = FALSE;

    if ((self != NULL) && (self->demux_cb != NULL)) {
        // invoke the Python subroutine with parameters
        if (self->demux_user_data != NULL) {
            cb_rslt = PyObject_CallFunction(self->demux_cb, "IIIy#O",
                                            block->pgno, block->stream,
                                            block->application_id,
                                            block->block, block->block_size,
                                            self->demux_user_data);
        }
        else {
            cb_rslt = PyObject_CallFunction(self->demux_cb, "IIIy#",
                                            block->pgno, block->stream,
                                            block->application_id,
                                            block->block, block->block_size);
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
ZvbiPfcDemux_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiPfcDemux_dealloc(ZvbiPfcDemuxObj *self)
{
    if (self->ctx) {
        vbi_pfc_demux_delete(self->ctx);
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
ZvbiPfcDemux_init(ZvbiPfcDemuxObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"pgno", "stream", "callback", "user_data", NULL};
    unsigned pgno;  // vbi_pgno
    unsigned stream;
    PyObject * callback = NULL;
    PyObject * user_data = NULL;
    int RETVAL = -1;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_pfc_demux_delete(self->ctx);

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
                                    &pgno, &stream, &callback, &user_data))
    {
        /* note: libzvbi prior to version 0.2.26 had an incorrect type definition
         * for the callback, hence the compiler will warn about a type mismatch */
        self->ctx = vbi_pfc_demux_new(pgno, stream, zvbi_xs_demux_pfc_handler, self);

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
            PyErr_SetString(ZvbiPfcDemuxError, "Initialization failed");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPfcDemux_reset(ZvbiPfcDemuxObj *self, PyObject *args)
{
    vbi_pfc_demux_reset(self->ctx);
    Py_RETURN_NONE;
}

static PyObject *
ZvbiPfcDemux_feed(ZvbiPfcDemuxObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer feed_buf;

    if (PyArg_ParseTuple(args, "y*", &feed_buf)) {
        if (feed_buf.len >= 42) {
            if (vbi_pfc_demux_feed(self->ctx, feed_buf.buf)) {
                RETVAL = Py_None;
                Py_INCREF(Py_None);
            }
            else {
                PyErr_SetString(ZvbiPfcDemuxError, "packet contains incorrectable errors");
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
ZvbiPfcDemux_feed_frame(ZvbiPfcDemuxObj *self, PyObject *args)
{
    PyObject * sliced_obj = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "O!", &ZvbiCaptureSlicedBufTypeDef, &sliced_obj)) {
        vbi_capture_buffer * p_sliced = ZvbiCaptureBuf_GetBuf(sliced_obj);
        unsigned n_lines = p_sliced->size / sizeof(vbi_sliced);

        if (vbi_pfc_demux_feed_frame(self->ctx, p_sliced->data, n_lines)) {
            RETVAL = Py_None;
            Py_INCREF(Py_None);
        }
        else {
            PyErr_SetString(ZvbiPfcDemuxError, "packet contains incorrectable errors");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiPfcDemux_MethodsDef[] =
{
    {"reset",      (PyCFunction) ZvbiPfcDemux_reset,      METH_NOARGS, NULL },
    {"feed",       (PyCFunction) ZvbiPfcDemux_feed,       METH_VARARGS, NULL },
    {"feed_frame", (PyCFunction) ZvbiPfcDemux_feed_frame, METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiPfcDemuxTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.PfcDemux",
    .tp_doc = PyDoc_STR("Page Function Clear (ETS 300 708 section 4) demultiplexer"),
    .tp_basicsize = sizeof(ZvbiPfcDemuxObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiPfcDemux_new,
    .tp_init = (initproc) ZvbiPfcDemux_init,
    .tp_dealloc = (destructor) ZvbiPfcDemux_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiPfcDemux_Repr,
    .tp_methods = ZvbiPfcDemux_MethodsDef,
};

int PyInit_PfcDemux(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiPfcDemuxTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiPfcDemuxError = PyErr_NewException("Zvbi.PfcDemuxError", error_base, NULL);
    Py_XINCREF(ZvbiPfcDemuxError);
    if (PyModule_AddObject(module, "PfcDemuxError", ZvbiPfcDemuxError) < 0) {
        Py_XDECREF(ZvbiPfcDemuxError);
        Py_CLEAR(ZvbiPfcDemuxError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiPfcDemuxTypeDef);
    if (PyModule_AddObject(module, "PfcDemux", (PyObject *) &ZvbiPfcDemuxTypeDef) < 0) {
        Py_DECREF(&ZvbiPfcDemuxTypeDef);
        Py_XDECREF(ZvbiPfcDemuxError);
        Py_CLEAR(ZvbiPfcDemuxError);
        return -1;
    }

    return 0;
}
