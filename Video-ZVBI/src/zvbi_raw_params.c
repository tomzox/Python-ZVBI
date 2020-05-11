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
#include "structmember.h"  // include is missing in Python.h
#include <stddef.h>  // for offsetof()

#include <libzvbi.h>

#include "zvbi_raw_params.h"

// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    // ATTENTION: struct layout must be in sync with ZvbiRawParams_Members
    vbi_raw_decoder par;
} ZvbiRawParamsObj;

// ---------------------------------------------------------------------------
//  VBI raw parameter struct type
// ---------------------------------------------------------------------------

static PyObject *
ZvbiRawParams_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiRawParams_dealloc(ZvbiRawParamsObj *self)
{
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiRawParams_init(ZvbiRawParamsObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {
        return -1;
    }
    return 0;
}

static PyObject *
ZvbiRawParams_Repr(ZvbiRawParamsObj * self)
{
    return PyUnicode_FromFormat("Zvbi.RawParams("
                                    "scanning=%d, "
                                    "sampling_format=%d, "
                                    "sampling_rate=%d, "
                                    "bytes_per_line=%d, "
                                    "offset=%d, "
                                    "start_a=%d, "
                                    "start_b=%d, "
                                    "count_a=%d, "
                                    "count_b=%d, "
                                    "interlaced=%d, "
                                    "synchronous=%d"
                                ")",
                                self->par.scanning,
                                self->par.sampling_format,
                                self->par.sampling_rate,
                                self->par.bytes_per_line,
                                self->par.offset,
                                self->par.start[0],
                                self->par.start[1],
                                self->par.count[0],
                                self->par.count[1],
                                self->par.interlaced,
                                self->par.synchronous);
}

vbi_raw_decoder * ZvbiRawParamsGetStruct(PyObject * self)
{
    assert(PyObject_IsInstance(self, (PyObject*)&ZvbiRawParamsTypeDef) == 1);
    return &((ZvbiRawParamsObj*)self)->par;
}

PyObject * ZvbiRawParamsFromStruct(const vbi_raw_decoder * par)
{
    PyObject * self = ZvbiRawParams_new(&ZvbiRawParamsTypeDef, NULL, NULL);
    if (self != NULL) {
        ((ZvbiRawParamsObj*)self)->par = *par;
    }
    return self;
}

// ---------------------------------------------------------------------------

static PyMemberDef ZvbiRawParams_Members[] =
{
#define MY_T_VBI_BOOL ((sizeof(vbi_bool) == sizeof(char)) ? T_BYTE : T_INT)
#define MY_T_VBI_PIXFMT ((sizeof(vbi_pixfmt) == sizeof(char)) ? T_BYTE : T_INT)
#define MY_OFFSETOF(EL) (offsetof(vbi_raw_decoder, EL) + offsetof(ZvbiRawParamsObj, par))
    //{ name, type, offset, flags, doc },
    { "scanning",        T_INT, MY_OFFSETOF(scanning), 0, PyDoc_STR("Describing the scan line system all line numbers refer to: 625 for PAL, 525 for NTSC, or 0 if unknown") },
    { "sampling_format", T_INT, MY_OFFSETOF(sampling_format), 0, PyDoc_STR("Format of the raw VBI data: one of constants VBI_PIXFMT_*") },
    { "sampling_rate",   T_INT, MY_OFFSETOF(sampling_rate), 0, PyDoc_STR("Sampling rate in Hz") },
    { "bytes_per_line",  T_INT, MY_OFFSETOF(bytes_per_line), 0, PyDoc_STR("Number of samples or pixels captured per scan line in bytes") },
    { "offset",          T_INT, MY_OFFSETOF(offset), 0, PyDoc_STR("The distance of the first captured sample to the physical start of the scan line") },
    { "start_a",         T_INT, MY_OFFSETOF(start[0]), 0, PyDoc_STR("First scan line to be captured in the first half-frame") },
    { "start_b",         T_INT, MY_OFFSETOF(start[1]), 0, PyDoc_STR("First scan line to be captured in the second half-frame") },
    { "count_a",         T_INT, MY_OFFSETOF(count[0]), 0, PyDoc_STR("Number of scan lines captured in the first half-frame respectively") },
    { "count_b",         T_INT, MY_OFFSETOF(count[1]), 0, PyDoc_STR("Number of scan lines captured in the second half-frame") },
    { "interlaced",      MY_T_VBI_BOOL, MY_OFFSETOF(interlaced), 0, PyDoc_STR("When True, scan lines of first and second half-frames will be interleaved in memory") },
    { "synchronous",     MY_T_VBI_PIXFMT, MY_OFFSETOF(synchronous), 0, PyDoc_STR("When True, half-frames are guaranteed to be stored in temporal order in memory") },
    { NULL }
};

PyTypeObject ZvbiRawParamsTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.RawParams",
    .tp_doc = PyDoc_STR("Data structure containing parameters for raw decoding"),
    .tp_basicsize = sizeof(ZvbiRawParamsObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiRawParams_new,
    .tp_init = (initproc) ZvbiRawParams_init,
    .tp_dealloc = (destructor) ZvbiRawParams_dealloc,
    .tp_repr = (reprfunc) ZvbiRawParams_Repr,
    //.tp_methods = ZvbiRawParams_MethodsDef,
    .tp_members = ZvbiRawParams_Members,
};

int PyInit_RawParams(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiRawParamsTypeDef) < 0) {
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiRawParamsTypeDef);
    if (PyModule_AddObject(module, "RawParams", (PyObject *) &ZvbiRawParamsTypeDef) < 0) {
        Py_DECREF(&ZvbiRawParamsTypeDef);
        return -1;
    }

    return 0;
}
