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

#include "zvbi_capture_buf.h"

// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_capture_buffer * buf;
    vbi_bool need_free;
    int iter_idx;
} ZvbiCaptureBufObj;

// ---------------------------------------------------------------------------

static PyObject *
ZvbiCaptureBuf_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiCaptureBuf_dealloc(ZvbiCaptureBufObj *self)
{
    // when originating from "pull", self->buf is owned by libzvbi
    if (self->buf && self->need_free) {
        if (self->buf->data) {
            PyMem_RawFree(self->buf->data);
        }
        PyMem_RawFree(self->buf);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

/*
 * Implementation of the standard "__iter__" function
 */
static PyObject *
ZvbiCaptureSlicedBuf_Iter(ZvbiCaptureBufObj *self)
{
    Py_INCREF(self);
    self->iter_idx = 0;
    return (PyObject *) self;
}

/*
 * Implementation of the standard "__next__" function
 */
static PyObject *
ZvbiCaptureSlicedBuf_IterNext(ZvbiCaptureBufObj *self)
{
    unsigned max_lines = 0;
    vbi_sliced * p_sliced = NULL;
    PyObject * RETVAL = NULL;

    vbi_capture_buffer * p_sliced_buf = self->buf;
    if (p_sliced_buf != NULL) {
        max_lines = p_sliced_buf->size / sizeof(vbi_sliced);
        p_sliced = p_sliced_buf->data;
    }
    if ((p_sliced != NULL) && (self->iter_idx < max_lines)) {
        p_sliced += self->iter_idx;

        RETVAL = PyTuple_New(3);
        if (RETVAL != NULL) {
            PyTuple_SetItem(RETVAL, 0, PyBytes_FromStringAndSize((char*)p_sliced->data,
                                                                 sizeof(p_sliced->data)));
            PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(p_sliced->id));
            PyTuple_SetItem(RETVAL, 2, PyLong_FromLong(p_sliced->line));

            self->iter_idx += 1;
        }
    }
    else
    {
        PyErr_SetNone(PyExc_StopIteration);
        self->iter_idx = -1;
    }
    return RETVAL;
}

/*
 * Implmentation of the len() operator
 */
Py_ssize_t ZvbiCaptureSlicedBuf_MappingLength(ZvbiCaptureBufObj * self)
{
    Py_ssize_t result = -1;

    vbi_capture_buffer * p_sliced_buf = self->buf;
    if (p_sliced_buf != NULL) {
        // FIXME should use "n_lines"
        result = p_sliced_buf->size / sizeof(vbi_sliced);
    }
    return result;
}

/*
 * Implementation of sub-script look-up (as alternative to iterator)
 */
PyObject * ZvbiCaptureSlicedBuf_MappingSubscript(ZvbiCaptureBufObj * self, PyObject * key)
{
    vbi_sliced * p_sliced = NULL;
    unsigned max_lines = 0;
    PyObject * RETVAL = NULL;

    long idx = PyLong_AsLong(key);
    if (idx >= 0) {
        vbi_capture_buffer * p_sliced_buf = self->buf;
        if (p_sliced_buf != NULL) {
            // FIXME should use "n_lines"
            max_lines = p_sliced_buf->size / sizeof(vbi_sliced);
            p_sliced = p_sliced_buf->data;
        }
        if ((p_sliced != NULL) && (idx < max_lines)) {
            p_sliced += idx;

            RETVAL = PyTuple_New(3);
            if (RETVAL != NULL) {
                PyTuple_SetItem(RETVAL, 0, PyBytes_FromStringAndSize((char*)p_sliced->data,
                                                                     sizeof(p_sliced->data)));
                PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(p_sliced->id));
                PyTuple_SetItem(RETVAL, 2, PyLong_FromLong(p_sliced->line));
            }
        }
        else
        {
            PyErr_SetNone(PyExc_IndexError);
        }
    }
    return RETVAL;
}

static PyTypeObject ZvbiCaptureBufTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.CaptureBuf",
    .tp_doc = PyDoc_STR("Abstract base class for capture data buffers"),
    .tp_basicsize = sizeof(ZvbiCaptureBufObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = NULL,  // abstract base, not to be instantiated
    .tp_dealloc = (destructor) ZvbiCaptureBuf_dealloc,
};

PyTypeObject ZvbiCaptureRawBufTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.CaptureRawBuf",
    .tp_doc = PyDoc_STR("Container for raw capture data"),
    .tp_basicsize = sizeof(ZvbiCaptureBufObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiCaptureBuf_new,
    .tp_dealloc = (destructor) ZvbiCaptureBuf_dealloc,
    .tp_base = &ZvbiCaptureBufTypeDef,
};

static PyMappingMethods ZvbiCaptureSlicedBufMappingDef =
{
    .mp_length = (lenfunc) ZvbiCaptureSlicedBuf_MappingLength,
    .mp_subscript = (binaryfunc) ZvbiCaptureSlicedBuf_MappingSubscript,
    .mp_ass_subscript = NULL  // assignment not allowed
};

PyTypeObject ZvbiCaptureSlicedBufTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.CaptureSlicedBuf",
    .tp_doc = PyDoc_STR("Container for sliced capture data"),
    .tp_basicsize = sizeof(ZvbiCaptureBufObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiCaptureBuf_new,
    .tp_dealloc = (destructor) ZvbiCaptureBuf_dealloc,
    .tp_base = &ZvbiCaptureBufTypeDef,
    .tp_iter = (getiterfunc) ZvbiCaptureSlicedBuf_Iter,
    .tp_iternext = (iternextfunc) ZvbiCaptureSlicedBuf_IterNext,
    .tp_as_mapping = &ZvbiCaptureSlicedBufMappingDef,
};

// ---------------------------------------------------------------------------

vbi_capture_buffer * ZvbiCaptureBuf_GetBuf(PyObject * self)
{
    assert(PyObject_IsInstance(self, (PyObject*)&ZvbiCaptureBufTypeDef) == 1);
    return ((ZvbiCaptureBufObj*)self)->buf;
}

PyObject * ZvbiCaptureRawBuf_FromPtr(vbi_capture_buffer * ptr)
{
    PyObject * self = ZvbiCaptureBuf_new(&ZvbiCaptureRawBufTypeDef, NULL, NULL);
    ((ZvbiCaptureBufObj*)self)->buf = ptr;
    ((ZvbiCaptureBufObj*)self)->need_free = FALSE;
    return self;
}

PyObject * ZvbiCaptureSlicedBuf_FromPtr(vbi_capture_buffer * ptr)
{
    PyObject * self = ZvbiCaptureBuf_new(&ZvbiCaptureSlicedBufTypeDef, NULL, NULL);
    ((ZvbiCaptureBufObj*)self)->buf = ptr;
    ((ZvbiCaptureBufObj*)self)->need_free = FALSE;
    return self;
}

PyObject * ZvbiCaptureSlicedBuf_FromData(vbi_sliced * data, int n_lines, double timestamp)
{
    ZvbiCaptureBufObj * self = (ZvbiCaptureBufObj*) ZvbiCaptureBuf_new(&ZvbiCaptureSlicedBufTypeDef, NULL, NULL);
    self->buf = PyMem_RawMalloc(sizeof(vbi_capture_buffer));
    self->buf->data = data;
    self->buf->size = n_lines * sizeof(vbi_sliced);
    self->buf->timestamp = timestamp;
    self->need_free = TRUE;
    return (PyObject*) self;
}

int PyInit_CaptureBuf(PyObject * module, PyObject * error_base)
{
    if ((PyType_Ready(&ZvbiCaptureBufTypeDef) < 0) ||
        (PyType_Ready(&ZvbiCaptureRawBufTypeDef) < 0) ||
        (PyType_Ready(&ZvbiCaptureSlicedBufTypeDef) < 0))
    {
        return -1;
    }

    // create class type objects
    Py_INCREF(&ZvbiCaptureBufTypeDef);
    if (PyModule_AddObject(module, "CaptureBuf", (PyObject *) &ZvbiCaptureBufTypeDef) < 0) {
        Py_DECREF(&ZvbiCaptureBufTypeDef);
        return -1;
    }
    Py_INCREF(&ZvbiCaptureRawBufTypeDef);
    if (PyModule_AddObject(module, "CaptureRawBuf", (PyObject *) &ZvbiCaptureRawBufTypeDef) < 0) {
        Py_DECREF(&ZvbiCaptureRawBufTypeDef);
        Py_DECREF(&ZvbiCaptureBufTypeDef);
        return -1;
    }
    Py_INCREF(&ZvbiCaptureSlicedBufTypeDef);
    if (PyModule_AddObject(module, "CaptureSlicedBuf", (PyObject *) &ZvbiCaptureSlicedBufTypeDef) < 0) {
        Py_DECREF(&ZvbiCaptureSlicedBufTypeDef);
        Py_DECREF(&ZvbiCaptureRawBufTypeDef);
        Py_DECREF(&ZvbiCaptureBufTypeDef);
        return -1;
    }

    return 0;
}
