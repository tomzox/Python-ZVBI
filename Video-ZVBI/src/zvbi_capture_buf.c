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

#if defined (NAMED_TUPLE_GC_BUG)
static PyTypeObject ZvbiCaptureSlicedLineTypeBuf;
static PyTypeObject * const ZvbiCaptureSlicedLineType = &ZvbiCaptureSlicedLineTypeBuf;
#else
static PyTypeObject * ZvbiCaptureSlicedLineType = NULL;
#endif

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

static PyObject *
ZvbiCaptureBufGetTimestamp(ZvbiCaptureBufObj * self, void * closure)
{
    return PyFloat_FromDouble(self->buf->timestamp);
}

// ---------------------------------------------------------------------------
// Raw buffer interfaces

static int
ZvbiCaptureRawBuf_GetBuffer(ZvbiCaptureBufObj * self, Py_buffer * view, int flags)
{
    if ((self->buf != NULL) && ((flags & PyBUF_WRITABLE) == 0)) {
        view->obj = (PyObject*) self;
        view->buf = self->buf->data;
        view->len = self->buf->size;
        view->itemsize = 1;
        view->ndim = 1;
        view->shape = &view->len;
        view->strides = &view->itemsize;
        view->suboffsets = NULL;
        view->format = NULL;
        view->readonly = TRUE;

        Py_INCREF(self);
        return 0;
    }
    else {
        PyErr_SetNone(PyExc_BufferError);
        view->obj = NULL;
        return -1;
    }
}

/*
 * Implmentation of the len() operator
 */
Py_ssize_t ZvbiCaptureRawBuf_MappingLength(ZvbiCaptureBufObj * self)
{
    return ((self->buf != NULL) ? self->buf->size : -1);
}

/*
 * Implementation of sub-script look-up (as alternative to iterator)
 */
PyObject * ZvbiCaptureRawBuf_MappingSubscript(ZvbiCaptureBufObj * self, PyObject * key)
{
    PyObject * RETVAL = NULL;

    // FIXME support argement of type slice
    long idx = PyLong_AsLong(key);
    if (idx >= 0) {
        if ((self->buf != NULL) && (idx < self->buf->size)) {
            uint8_t * item = (uint8_t*)self->buf->data + idx;
            RETVAL = PyLong_FromLong((unsigned long)*item);
        }
        else {
            PyErr_SetNone(PyExc_IndexError);
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
// Sliced buffer interfaces

/*
 * Implementation of the standard "__iter__" function
 */
static PyObject *
ZvbiCaptureSlicedBuf_Iter(ZvbiCaptureBufObj *self)
{
    Py_INCREF(self);  // Note corresponding DECREF is done by caller after end of iteration
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

        RETVAL = PyStructSequence_New(ZvbiCaptureSlicedLineType);
        if (RETVAL != NULL) {
            PyStructSequence_SetItem(RETVAL, 0, PyBytes_FromStringAndSize((char*)p_sliced->data,
                                                                          sizeof(p_sliced->data)));
            PyStructSequence_SetItem(RETVAL, 1, PyLong_FromLong(p_sliced->id));
            PyStructSequence_SetItem(RETVAL, 2, PyLong_FromLong(p_sliced->line));

            self->iter_idx += 1;
        }
    }
    else {
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
        // note "size" element was calculated from "n_lines" slicer result
        // (i.e. not the allocated buffer size, which may be larger)
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

    // FIXME support argement of type slice
    long idx = PyLong_AsLong(key);
    if (idx >= 0) {
        vbi_capture_buffer * p_sliced_buf = self->buf;
        if (p_sliced_buf != NULL) {
            // note "size" element was calculated from "n_lines" slicer result
            // (i.e. not the allocated buffer size, which may be larger)
            max_lines = p_sliced_buf->size / sizeof(vbi_sliced);
            p_sliced = p_sliced_buf->data;
        }
        if ((p_sliced != NULL) && (idx < max_lines)) {
            p_sliced += idx;

            RETVAL = PyStructSequence_New(ZvbiCaptureSlicedLineType);
            if (RETVAL != NULL) {
                PyStructSequence_SetItem(RETVAL, 0, PyBytes_FromStringAndSize((char*)p_sliced->data,
                                                                              sizeof(p_sliced->data)));
                PyStructSequence_SetItem(RETVAL, 1, PyLong_FromLong(p_sliced->id));
                PyStructSequence_SetItem(RETVAL, 2, PyLong_FromLong(p_sliced->line));
            }
        }
        else {
            PyErr_SetNone(PyExc_IndexError);
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
// Type definitions

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

// Dynamically calculated attributes
static PyGetSetDef ZvbiCaptureBufGetSetDef[] =
{
    { .name = "timestamp",
      .get = (getter) ZvbiCaptureBufGetTimestamp,
      .set = NULL,
      .doc = PyDoc_STR("Timestamp indicating when the data was captured; the value is of type float, "
                       "representing the number of seconds and fractions since 1970-01-01 00:00"),
    },
    {NULL}
};

// Implementing the "buffer protocol", i.e. access to encapsulated data via Py_buffer
static PyBufferProcs ZvbiCaptureRawBufAsBufferDef =
{
    .bf_getbuffer = (getbufferproc) ZvbiCaptureRawBuf_GetBuffer,
    .bf_releasebuffer = NULL
};

// Implementing the "mapping protocol", i.e. access via array sub-script
static PyMappingMethods ZvbiCaptureRawBufMappingDef =
{
    .mp_length = (lenfunc) ZvbiCaptureRawBuf_MappingLength,
    .mp_subscript = (binaryfunc) ZvbiCaptureRawBuf_MappingSubscript,
    .mp_ass_subscript = NULL  // assignment not allowed
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
    .tp_getset = ZvbiCaptureBufGetSetDef,
    .tp_as_buffer = &ZvbiCaptureRawBufAsBufferDef,
    .tp_as_mapping = &ZvbiCaptureRawBufMappingDef,
};

// Implementing the "mapping protocol", i.e. access via array sub-script
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
    .tp_getset = ZvbiCaptureBufGetSetDef,
    .tp_iter = (getiterfunc) ZvbiCaptureSlicedBuf_Iter,
    .tp_iternext = (iternextfunc) ZvbiCaptureSlicedBuf_IterNext,
    .tp_as_mapping = &ZvbiCaptureSlicedBufMappingDef,
};

static PyStructSequence_Field ZvbiCaptureSlicedLineDefMembers[] =
{
    { "data", PyDoc_STR("The actual payload data in form of a bytes object.") },
    { "ident", PyDoc_STR("One or more 'VBI_SLICED_*' symbols (bit-wise OR), identifying the type of data service. Multiple identifiers may occur e.g. for VBI_SLICED_TELETEXT_B.") },
    { "line_no", PyDoc_STR("Source line number according to the ITU-R line numbering scheme, or 0 if the exact line number is unknown. This number is required by the service decoder.") },
    { NULL, NULL }
};

static PyStructSequence_Desc ZvbiCaptureSlicedLineDef =
{
    "Zvbi.CaptureSlicedLine",
    PyDoc_STR("Named tuple type containing one line of sliced data"),
    ZvbiCaptureSlicedLineDefMembers,
    3
};

// ---------------------------------------------------------------------------
// External interface for instantiation

vbi_capture_buffer * ZvbiCaptureBuf_GetBuf(PyObject * self)
{
    assert(PyObject_IsInstance(self, (PyObject*)&ZvbiCaptureBufTypeDef) == 1);
    return ((ZvbiCaptureBufObj*)self)->buf;
}

PyObject * ZvbiCaptureRawBuf_FromPtr(vbi_capture_buffer * ptr)
{
    PyObject * self = ZvbiCaptureBuf_new(&ZvbiCaptureRawBufTypeDef, NULL, NULL);
    if (self != NULL) {
        ((ZvbiCaptureBufObj*)self)->buf = ptr;
        ((ZvbiCaptureBufObj*)self)->need_free = FALSE;
    }
    return self;
}

PyObject * ZvbiCaptureRawBuf_FromData(char * data, int size, double timestamp)
{
    ZvbiCaptureBufObj * self = (ZvbiCaptureBufObj*) ZvbiCaptureBuf_new(&ZvbiCaptureRawBufTypeDef, NULL, NULL);
    if (self != NULL) {
        self->buf = PyMem_RawMalloc(sizeof(vbi_capture_buffer));
        self->buf->data = data;
        self->buf->size = size;
        self->buf->timestamp = timestamp;
        self->need_free = TRUE;
    }
    return (PyObject*) self;
}

PyObject * ZvbiCaptureSlicedBuf_FromPtr(vbi_capture_buffer * ptr)
{
    PyObject * self = ZvbiCaptureBuf_new(&ZvbiCaptureSlicedBufTypeDef, NULL, NULL);
    if (self != NULL) {
        ((ZvbiCaptureBufObj*)self)->buf = ptr;
        ((ZvbiCaptureBufObj*)self)->need_free = FALSE;
    }
    return self;
}

PyObject * ZvbiCaptureSlicedBuf_FromData(vbi_sliced * data, int n_lines, double timestamp)
{
    ZvbiCaptureBufObj * self = (ZvbiCaptureBufObj*) ZvbiCaptureBuf_new(&ZvbiCaptureSlicedBufTypeDef, NULL, NULL);
    if (self != NULL) {
        self->buf = PyMem_RawMalloc(sizeof(vbi_capture_buffer));
        self->buf->data = data;
        self->buf->size = n_lines * sizeof(vbi_sliced);
        self->buf->timestamp = timestamp;
        self->need_free = TRUE;
    }
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

    // create sliced line container class type object
#if defined (NAMED_TUPLE_GC_BUG)
    if (PyStructSequence_InitType2(&ZvbiCaptureSlicedLineTypeBuf, &ZvbiCaptureSlicedLineDef) != 0)
#else
    ZvbiCaptureSlicedLineType = PyStructSequence_NewType(&ZvbiCaptureSlicedLineDef);
    if (ZvbiCaptureSlicedLineType == NULL)
#endif
    {
        Py_DECREF(&ZvbiCaptureSlicedLineType);
        Py_DECREF(&ZvbiCaptureSlicedBufTypeDef);
        Py_DECREF(&ZvbiCaptureRawBufTypeDef);
        Py_DECREF(&ZvbiCaptureBufTypeDef);
        return -1;
    }

    return 0;
}
