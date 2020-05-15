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

#include "zvbi_raw_dec.h"
#include "zvbi_raw_params.h"
#include "zvbi_capture.h"
#include "zvbi_capture_buf.h"

// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_raw_decoder rd;
} ZvbiRawDecObj;

static PyObject * ZvbiRawDecError;

// ---------------------------------------------------------------------------
//  VBI raw decoder
// ---------------------------------------------------------------------------

static PyObject *
ZvbiRawDec_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    ZvbiRawDecObj * self = (ZvbiRawDecObj *) type->tp_alloc(type, 0);

    vbi_raw_decoder_init(&self->rd);

    return (PyObject *) self;
}

static void
ZvbiRawDec_dealloc(ZvbiRawDecObj *self)
{
    vbi_raw_decoder_destroy(&self->rd);

    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiRawDec_init(ZvbiRawDecObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"par", NULL};
    int RETVAL = -1;
    PyObject * obj = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &obj)) {
        return -1;
    }

    if (PyObject_IsInstance(obj, (PyObject*)&ZvbiCaptureTypeDef) == 1) {
        vbi_capture * p_cap = ZvbiCapture_GetCtx(obj);
        vbi_raw_decoder * p_par = vbi_capture_parameters(p_cap);
        if (p_par != NULL) {
            // copy individual parameters from the given container into "self"
            self->rd.scanning = p_par->scanning;
            self->rd.sampling_format = p_par->sampling_format;
            self->rd.sampling_rate = p_par->sampling_rate;
            self->rd.bytes_per_line = p_par->bytes_per_line;
            self->rd.offset = p_par->offset;
            self->rd.start[0] = p_par->start[0];
            self->rd.start[1] = p_par->start[1];
            self->rd.count[0] = p_par->count[0];
            self->rd.count[1] = p_par->count[1];
            self->rd.interlaced = p_par->interlaced;
            self->rd.synchronous = p_par->synchronous;
            RETVAL = 0;
        }
        else {
            PyErr_SetString(ZvbiRawDecError, "failed to get capture parameters from Capture object");
        }
    }
    else if (PyObject_IsInstance(obj, (PyObject*)&ZvbiRawParamsTypeDef) == 1) {
        vbi_raw_decoder * p_par = ZvbiRawParamsGetStruct(obj);
        self->rd = *p_par;
        RETVAL = 0;
    }
    else {
        // use standard exception TypeError as this error does not come from ZVBI library
        PyErr_SetString(PyExc_TypeError, "Parameter is neither dict nor ZVBI capture reference");
    }

    return RETVAL;
}

static PyObject *
ZvbiRawDec_parameters(ZvbiRawDecObj *self, PyObject *args)
{
    unsigned services;
    int scanning;
    int max_rate;

    if (!PyArg_ParseTuple(args, "Ii", &services, &scanning)) {
        return NULL;
    }
    PyObject * RETVAL = NULL;

    vbi_raw_decoder rd;
    vbi_raw_decoder_init(&rd);
    services = vbi_raw_decoder_parameters(&rd, services, scanning, &max_rate);

    RETVAL = PyTuple_New(3);
    if (RETVAL != NULL) {
        PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(services));
        PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(max_rate));
        PyTuple_SetItem(RETVAL, 2, ZvbiRawParamsFromStruct(&rd));
    }
    vbi_raw_decoder_destroy(&rd);
    return RETVAL;
}

static PyObject *
ZvbiRawDec_reset(ZvbiRawDecObj *self, PyObject *args)
{
    vbi_raw_decoder_reset(&self->rd);
    Py_RETURN_NONE;
}

static PyObject *
ZvbiRawDec_add_services(ZvbiRawDecObj *self, PyObject *args)
{
    unsigned services;
    int strict = 0;

    if (!PyArg_ParseTuple(args, "I|I", &services, &strict)) {
        return NULL;
    }
    services = vbi_raw_decoder_add_services(&self->rd, services, strict);
    return PyLong_FromLong(services);
}

static PyObject *
ZvbiRawDec_check_services(ZvbiRawDecObj *self, PyObject *args)
{
    unsigned services;
    int strict = 0;

    if (!PyArg_ParseTuple(args, "I|I", &services, &strict)) {
        return NULL;
    }
    services = vbi_raw_decoder_check_services(&self->rd, services, strict);
    return PyLong_FromLong(services);
}

static PyObject *
ZvbiRawDec_remove_services(ZvbiRawDecObj *self, PyObject *args)
{
    unsigned services;

    if (!PyArg_ParseTuple(args, "I", &services)) {
        return NULL;
    }
    services = vbi_raw_decoder_remove_services(&self->rd, services);
    return PyLong_FromLong(services);
}

static PyObject *
ZvbiRawDec_resize(ZvbiRawDecObj *self, PyObject *args)
{
    int start[2];
    unsigned int count[2];

    if (!PyArg_ParseTuple(args, "iIiI", &start[0], &count[0], &start[1], &count[1])) {
        return NULL;
    }
    vbi_raw_decoder_resize(&self->rd, start, count);
    Py_RETURN_NONE;
}

static PyObject *
ZvbiRawDec_decode(ZvbiRawDecObj *self, PyObject *args)
{
    PyObject * obj = NULL;
    double timestamp = 0.0;

    if (!PyArg_ParseTuple(args, "O|d", &obj, &timestamp)) {
        return NULL;
    }

    uint8_t * p_raw = NULL;
    size_t raw_buf_size = 0;

    if (PyObject_IsInstance(obj, (PyObject*)&ZvbiCaptureRawBufTypeDef) == 1) {
        vbi_capture_buffer * p_raw_buf = ZvbiCaptureBuf_GetBuf(obj);
        if (p_raw_buf != NULL) {
            raw_buf_size = p_raw_buf->size;
            p_raw = p_raw_buf->data;
        }
        else {
            PyErr_SetString(ZvbiRawDecError, "Raw capture buffer contains no data");
        }
    }
    else if (PyObject_IsInstance(obj, (PyObject*)&PyBytes_Type) == 1) {
        p_raw = (uint8_t*) PyBytes_AsString(obj);
        raw_buf_size = PyBytes_Size(obj);
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Parameter is neither a ZvbiCaptureRawBuf nor bytes object");
    }
    PyObject * RETVAL = NULL;

    if (p_raw != NULL) {
        size_t raw_size = (self->rd.count[0] + self->rd.count[1]) * self->rd.bytes_per_line;
        if (raw_buf_size >= raw_size) {
            size_t size_sliced = (self->rd.count[0] + self->rd.count[1]) * sizeof(vbi_sliced);
            vbi_sliced * p_sliced = (vbi_sliced*) PyMem_Malloc(size_sliced);
            if (p_sliced != NULL) {
                int nof_lines = vbi_raw_decode(&self->rd, p_raw, p_sliced);

                RETVAL = ZvbiCaptureSlicedBuf_FromData(p_sliced, nof_lines, timestamp);
            }
            else {
                PyErr_SetString(ZvbiRawDecError, "Failed to allocate memory for sliced buffer");
            }
        }
        else {
            PyErr_SetString(ZvbiRawDecError, "Input raw buffer is smaller than required for VBI geometry");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiRawDec_MethodsDef[] =
{
    {"parameters",      (PyCFunction) ZvbiRawDec_parameters, METH_VARARGS | METH_STATIC, NULL },

    {"reset",           (PyCFunction) ZvbiRawDec_reset,           METH_NOARGS,  NULL },
    {"add_services",    (PyCFunction) ZvbiRawDec_add_services,    METH_VARARGS, NULL },
    {"check_services",  (PyCFunction) ZvbiRawDec_check_services,  METH_VARARGS, NULL },
    {"remove_services", (PyCFunction) ZvbiRawDec_remove_services, METH_VARARGS, NULL },
    {"resize",          (PyCFunction) ZvbiRawDec_resize,          METH_VARARGS, NULL },
    {"decode",          (PyCFunction) ZvbiRawDec_decode,          METH_VARARGS, NULL },
    {NULL}  /* Sentinel */
};

static PyTypeObject ZvbiRawDecTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.RawDec",
    .tp_doc = PyDoc_STR("Class for decoding raw capture output"),
    .tp_basicsize = sizeof(ZvbiRawDecObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiRawDec_new,
    .tp_init = (initproc) ZvbiRawDec_init,
    .tp_dealloc = (destructor) ZvbiRawDec_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiRawDec_Repr,
    .tp_methods = ZvbiRawDec_MethodsDef,
    //.tp_members = ZvbiRawDec_Members,
};

int PyInit_RawDec(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiRawDecTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiRawDecError = PyErr_NewException("Zvbi.RawDecError", error_base, NULL);
    Py_XINCREF(ZvbiRawDecError);
    if (PyModule_AddObject(module, "RawDecError", ZvbiRawDecError) < 0) {
        Py_XDECREF(ZvbiRawDecError);
        Py_CLEAR(ZvbiRawDecError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiRawDecTypeDef);
    if (PyModule_AddObject(module, "RawDec", (PyObject *) &ZvbiRawDecTypeDef) < 0) {
        Py_DECREF(&ZvbiRawDecTypeDef);
        Py_XDECREF(ZvbiRawDecError);
        Py_CLEAR(ZvbiRawDecError);
        return -1;
    }

    return 0;
}
