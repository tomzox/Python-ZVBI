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

#include "zvbi_capture.h"
#include "zvbi_proxy.h"
#include "zvbi_raw_dec.h"
#include "zvbi_capture_buf.h"

// ---------------------------------------------------------------------------
//  VBI Capturing & Slicing
// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_capture * ctx;
    unsigned services;
} ZvbiCaptureObj;

#if defined (NAMED_TUPLE_GC_BUG)
static PyTypeObject ZvbiCapture_ResultTypeBuf;
static PyTypeObject * const ZvbiCapture_ResultType = &ZvbiCapture_ResultTypeBuf;
#else
static PyTypeObject * ZvbiCapture_ResultType = NULL;
#endif

PyObject * ZvbiCaptureError;

// ---------------------------------------------------------------------------

static PyObject *
ZvbiCapture_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiCapture_dealloc(ZvbiCaptureObj *self)
{
    if (self->ctx) {
        vbi_capture_delete(self->ctx);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiCapture_init(ZvbiCaptureObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"dev",
                              "services",   // analog only
                              "dvb_pid",
                              "buffers",    // v4l2 only
                              "scanning",   // v4l1+bkr only
                              //"dev_fd",   // sidecar only
                              "proxy",
                              "strict",
                              "trace",
                              NULL};
    char * dev_name = NULL;
    unsigned services = 0;
    unsigned dvb_pid = 0;
    int buffers = 5;
    int scanning = 0;
    //int dev_fd = -1;
    PyObject * proxy = NULL;
    int strict = 0;
    int trace = FALSE;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_capture_delete(self->ctx);
        self->ctx = NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|$IIiiO!Ip", kwlist,
                                     &dev_name, &services, &dvb_pid,
                                     &buffers, &scanning, //&dev_fd,
                                     &ZvbiProxyTypeDef, &proxy,
                                     &strict, &trace))
    {
        return -1;
    }

    // parameter may be modified by constructor
    self->services = services;
    char * errorstr = NULL;
    int RETVAL = 0;

    if (proxy != NULL) {
        // FIXME does not work for DVB
        self->ctx = vbi_capture_proxy_new(ZvbiProxy_GetCtx(proxy), buffers, scanning, &self->services, strict, &errorstr);
    }
#if 0  /* obsolete */
    else if (dev_fd != -1) {
        if (dvb_pid == 0) {
            self->ctx = vbi_capture_v4l_sidecar_new(dev_name, dev_fd, &self->services, strict, &errorstr, trace);
        }
        else {
            errorstr = strdup("Invalid combination of option dvb_pid with dev_fd");
        }
    }
#endif
    else {
        // FIXME V4L2 must be done first as DVB open also works on V4L2 device but not vice-versa
        self->ctx = vbi_capture_v4l2_new(dev_name, buffers, &self->services, strict, &errorstr, trace);

#if 0  /* obsolete */
        if (self->ctx == NULL) {
            self->ctx = vbi_capture_v4l_new(dev_name, scanning, &self->services, strict, &errorstr, trace);
        }
#endif
        if (self->ctx == NULL) {
            // FIXME free errorstr if not NULL, or concatenate
            self->ctx = vbi_capture_bktr_new(dev_name, scanning, &self->services, strict, &errorstr, trace);
        }
        if (self->ctx == NULL) {
            // note also works with default value 0 for PID (can be set later using dvb_filter)
            // FIXME free errorstr if not NULL, or concatenate
            self->ctx = vbi_capture_dvb_new2(dev_name, dvb_pid, &errorstr, trace);
        }

    }
    if (self->ctx == NULL) {
        PyErr_SetString(ZvbiCaptureError, errorstr ? errorstr : "unknown error");
        RETVAL = -1;
    }
    if (errorstr != NULL) {
        free(errorstr);
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_dvb_filter(ZvbiCaptureObj *self, PyObject *args)
{
    int pid = 0;

    if (!PyArg_ParseTuple(args, "I", &pid)) {
        return NULL;
    }
    if (vbi_capture_dvb_filter(self->ctx, pid) < 0) {
        PyErr_Format(ZvbiCaptureError, "Failed to set PID:%d (%s)", strerror(errno));
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
ZvbiCapture_dvb_last_pts(ZvbiCaptureObj *self, PyObject *args)
{
    int64_t RETVAL = vbi_capture_dvb_last_pts(self->ctx);
    return PyLong_FromLongLong(RETVAL);
}

static PyObject *
ZvbiCapture_read_raw(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    if (!PyArg_ParseTuple(args, "i", &timeout_ms)) {
        return NULL;
    }
    PyObject * RETVAL = NULL;

    vbi_raw_decoder * p_par = vbi_capture_parameters(self->ctx);
    if (p_par != NULL) {
	size_t size_raw = (p_par->count[0] + p_par->count[1]) * p_par->bytes_per_line;
        PyObject * raw_obj = PyBytes_FromStringAndSize(NULL, size_raw);
        void * raw_buffer = PyBytes_AS_STRING(raw_obj);

        double timestamp = 0;
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int st = vbi_capture_read_raw(self->ctx, raw_buffer, &timestamp, &tv);
        if (st > 0) {
            RETVAL = PyStructSequence_New(ZvbiCapture_ResultType);
            if (RETVAL) {
                PyStructSequence_SetItem(RETVAL, 0, PyFloat_FromDouble(timestamp));
                PyStructSequence_SetItem(RETVAL, 1, PyLong_FromLong(0));
                PyStructSequence_SetItem(RETVAL, 2, raw_obj);
                PyStructSequence_SetItem(RETVAL, 3, Py_None);
                Py_INCREF(Py_None);
            }
        }
        else {
            if (st < 0) {
                PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
            }
            else {
                // FIXME use different exception
                PyErr_SetString(ZvbiCaptureError, "timeout");
            }
        }
    }
    else {
        PyErr_Format(ZvbiCaptureError, "capture error: invalid parameters");
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_read_sliced(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    if (!PyArg_ParseTuple(args, "i", &timeout_ms)) {
        return NULL;
    }
    PyObject * RETVAL = NULL;

    vbi_raw_decoder * p_par = vbi_capture_parameters(self->ctx);
    if (p_par != NULL) {
	size_t size_sliced = (p_par->count[0] + p_par->count[1]) * sizeof(vbi_sliced);
        vbi_sliced * p_sliced = (vbi_sliced*) PyMem_Malloc(size_sliced);

        int n_lines = 0;
        double timestamp = 0;
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int st = vbi_capture_read_sliced(self->ctx, p_sliced, &n_lines, &timestamp, &tv);
        if (st > 0) {
            RETVAL = PyStructSequence_New(ZvbiCapture_ResultType);
            if (RETVAL) {
                PyStructSequence_SetItem(RETVAL, 0, PyFloat_FromDouble(timestamp));
                PyStructSequence_SetItem(RETVAL, 1, PyLong_FromLong(n_lines));
                PyStructSequence_SetItem(RETVAL, 2, Py_None);
                PyStructSequence_SetItem(RETVAL, 3, ZvbiCaptureSlicedBuf_FromData(p_sliced, n_lines, timestamp));
                Py_INCREF(Py_None);
            }
            else {
                PyMem_Free(p_sliced);
            }
        }
        else {
            if (st < 0) {
                PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
            }
            else {
                // FIXME use different exception
                PyErr_SetString(ZvbiCaptureError, "timeout");
            }
            PyMem_Free(p_sliced);
        }
    }
    else {
        PyErr_Format(ZvbiCaptureError, "capture error: invalid parameters");
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_read(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    if (!PyArg_ParseTuple(args, "i", &timeout_ms)) {
        return NULL;
    }
    PyObject * RETVAL = NULL;

    vbi_raw_decoder * p_par = vbi_capture_parameters(self->ctx);
    if (p_par != NULL) {
	size_t size_sliced = (p_par->count[0] + p_par->count[1]) * sizeof(vbi_sliced);
	size_t size_raw = (p_par->count[0] + p_par->count[1]) * p_par->bytes_per_line;

        PyObject * raw_obj = PyBytes_FromStringAndSize(NULL, size_raw);
        void * raw_buffer = PyBytes_AS_STRING(raw_obj);

        vbi_sliced * p_sliced = (vbi_sliced*) PyMem_Malloc(size_sliced);

        int n_lines = 0;
        double timestamp = 0;
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int st = vbi_capture_read(self->ctx, raw_buffer, p_sliced, &n_lines, &timestamp, &tv);
        if (st > 0) {
            RETVAL = PyStructSequence_New(ZvbiCapture_ResultType);
            if (RETVAL) {
                PyStructSequence_SetItem(RETVAL, 0, PyFloat_FromDouble(timestamp));
                PyStructSequence_SetItem(RETVAL, 1, PyLong_FromLong(n_lines));
                PyStructSequence_SetItem(RETVAL, 2, raw_obj);
                PyStructSequence_SetItem(RETVAL, 3, ZvbiCaptureSlicedBuf_FromData(p_sliced, n_lines, timestamp));
            }
            else {
                PyMem_Free(p_sliced);
            }
        }
        else {
            if (st < 0) {
                PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
            }
            else {
                // FIXME use different exception
                PyErr_SetString(ZvbiCaptureError, "timeout");
            }
            PyMem_Free(p_sliced);
        }
    }
    else {
        PyErr_Format(ZvbiCaptureError, "capture error: invalid parameters");
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_pull_raw(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    if (!PyArg_ParseTuple(args, "i", &timeout_ms)) {
        return NULL;
    }

    PyObject * RETVAL = NULL;
    vbi_capture_buffer * raw_buffer = NULL;

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int st = vbi_capture_pull_raw(self->ctx, &raw_buffer, &tv);
    if (st > 0) {
        double timestamp = raw_buffer->timestamp;

        RETVAL = PyStructSequence_New(ZvbiCapture_ResultType);
        if (RETVAL) {
            PyStructSequence_SetItem(RETVAL, 0, PyFloat_FromDouble(timestamp));
            PyStructSequence_SetItem(RETVAL, 1, PyLong_FromLong(0));
            PyStructSequence_SetItem(RETVAL, 2, ZvbiCaptureRawBuf_FromPtr(raw_buffer));
            PyStructSequence_SetItem(RETVAL, 3, Py_None);
            Py_INCREF(Py_None);
        }
    }
    else {
        if (st < 0) {
            PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
        }
        else {
            // FIXME use different exception
            PyErr_SetString(ZvbiCaptureError, "timeout");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_pull_sliced(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    if (!PyArg_ParseTuple(args, "i", &timeout_ms)) {
        return NULL;
    }

    PyObject * RETVAL = NULL;
    vbi_capture_buffer * sliced_buffer = NULL;

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int st = vbi_capture_pull_sliced(self->ctx, &sliced_buffer, &tv);
    if (st > 0) {
        double timestamp = sliced_buffer->timestamp;
        int sliced_lines = sliced_buffer->size / sizeof(vbi_sliced);

        RETVAL = PyStructSequence_New(ZvbiCapture_ResultType);
        if (RETVAL) {
            PyStructSequence_SetItem(RETVAL, 0, PyFloat_FromDouble(timestamp));
            PyStructSequence_SetItem(RETVAL, 1, PyLong_FromLong(sliced_lines));
            PyStructSequence_SetItem(RETVAL, 2, Py_None);
            PyStructSequence_SetItem(RETVAL, 3, ZvbiCaptureSlicedBuf_FromPtr(sliced_buffer));
            Py_INCREF(Py_None);
        }
    }
    else {
        if (st < 0) {
            PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
        }
        else {
            // FIXME use different exception
            PyErr_SetString(ZvbiCaptureError, "timeout");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_pull(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    if (!PyArg_ParseTuple(args, "i", &timeout_ms)) {
        return NULL;
    }

    PyObject * RETVAL = NULL;
    vbi_capture_buffer * raw_buffer = NULL;
    vbi_capture_buffer * sliced_buffer = NULL;

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int st = vbi_capture_pull(self->ctx, &raw_buffer, &sliced_buffer, &tv);
    if (st > 0) {
        double timestamp = sliced_buffer->timestamp;
        int sliced_lines = sliced_buffer->size / sizeof(vbi_sliced);

        RETVAL = PyStructSequence_New(ZvbiCapture_ResultType);
        if (RETVAL) {
            PyStructSequence_SetItem(RETVAL, 0, PyFloat_FromDouble(timestamp));
            PyStructSequence_SetItem(RETVAL, 1, PyLong_FromLong(sliced_lines));
            if (raw_buffer != NULL) {  // DVB devices may not return raw data
                PyStructSequence_SetItem(RETVAL, 2, ZvbiCaptureRawBuf_FromPtr(raw_buffer));
            }
            else {
                PyStructSequence_SetItem(RETVAL, 2, Py_None);
                Py_INCREF(Py_None);
            }
            PyStructSequence_SetItem(RETVAL, 3, ZvbiCaptureSlicedBuf_FromPtr(sliced_buffer));
        }
    }
    else {
        if (st < 0) {
            PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
        }
        else {
            // FIXME use different exception
            PyErr_SetString(ZvbiCaptureError, "timeout");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_parameters(ZvbiCaptureObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    vbi_raw_decoder * p_rd = vbi_capture_parameters(self->ctx);
    if (p_rd != NULL) {
        RETVAL = ZvbiRawDec_Par2Dict(p_rd);
    }
    else {
        PyErr_SetString(ZvbiCaptureError, "failed to retrieve parameters");
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_get_fd(ZvbiCaptureObj *self, PyObject *args)
{
    int RETVAL = vbi_capture_fd(self->ctx);
    return PyLong_FromLong(RETVAL);
}

static PyObject *
ZvbiCapture_update_services(ZvbiCaptureObj *self, PyObject *args)
{
    unsigned int services = 0;
    int reset = FALSE;
    int commit = FALSE;
    int strict = 0;
    char * errorstr = NULL;
    PyObject * RETVAL;

    if (!PyArg_ParseTuple(args, "I|$ppI", &services, &reset, &commit, &strict)) {
        return NULL;
    }
    services = vbi_capture_update_services(self->ctx, reset, commit, services, strict, &errorstr);
    if (services != 0) {
        RETVAL = PyLong_FromLong(services);
    }
    else {
        PyErr_SetString(ZvbiCaptureError, errorstr ? errorstr : "unknown error");
        RETVAL = NULL;
    }
    if (errorstr != NULL) {
        free(errorstr);
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_get_scanning(ZvbiCaptureObj *self, PyObject *args)
{
    int RETVAL = vbi_capture_get_scanning(self->ctx);
    return PyLong_FromLong(RETVAL);
}

static PyObject *
ZvbiCapture_flush(ZvbiCaptureObj *self, PyObject *args)
{
    vbi_capture_flush(self->ctx);
    Py_RETURN_NONE;
}

static PyObject *
ZvbiCapture_get_fd_flags(ZvbiCaptureObj *self, PyObject *args)
{
    VBI_CAPTURE_FD_FLAGS RETVAL = vbi_capture_get_fd_flags(self->ctx);
    return PyLong_FromLong(RETVAL);
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiCapture_MethodsDef[] =
{
    {"dvb_filter",      (PyCFunction) ZvbiCapture_dvb_filter,      METH_VARARGS, NULL },
    {"dvb_last_pts",    (PyCFunction) ZvbiCapture_dvb_last_pts,    METH_NOARGS,  NULL },

    {"read_raw",        (PyCFunction) ZvbiCapture_read_raw,        METH_VARARGS, NULL },
    {"read_sliced",     (PyCFunction) ZvbiCapture_read_sliced,     METH_VARARGS, NULL },
    {"read",            (PyCFunction) ZvbiCapture_read,            METH_VARARGS, NULL },

    {"pull_raw",        (PyCFunction) ZvbiCapture_pull_raw,        METH_VARARGS, NULL },
    {"pull_sliced",     (PyCFunction) ZvbiCapture_pull_sliced,     METH_VARARGS, NULL },
    {"pull",            (PyCFunction) ZvbiCapture_pull,            METH_VARARGS, NULL },

    {"parameters",      (PyCFunction) ZvbiCapture_parameters,      METH_NOARGS,  NULL },
    {"get_fd",          (PyCFunction) ZvbiCapture_get_fd,          METH_NOARGS,  NULL },
    {"update_services", (PyCFunction) ZvbiCapture_update_services, METH_VARARGS, NULL },
    {"get_scanning",    (PyCFunction) ZvbiCapture_get_scanning,    METH_NOARGS,  NULL },
    {"flush",           (PyCFunction) ZvbiCapture_flush,           METH_NOARGS,  NULL },
    {"get_fd_flags",    (PyCFunction) ZvbiCapture_get_fd_flags,    METH_NOARGS,  NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiCaptureTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.Capture",
    .tp_doc = PyDoc_STR("Class controlling VBI data capturing"),
    .tp_basicsize = sizeof(ZvbiCaptureObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiCapture_new,
    .tp_init = (initproc) ZvbiCapture_init,
    .tp_dealloc = (destructor) ZvbiCapture_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiCapture_Repr,
    .tp_methods = ZvbiCapture_MethodsDef,
    //.tp_members = ZvbiCapture_Members,
};

static PyStructSequence_Field ZvbiCapture_ResultDefMembers[] =
{
    { "timestamp", PyDoc_STR("Timestamp indicating when the data was captured") },
    { "sliced_lines", PyDoc_STR("Number of valid lines in the sliced buffer") },
    { "raw_buffer", PyDoc_STR("Container for captured raw data, or None if not requested") },
    { "sliced_buffer", PyDoc_STR("Container for data of sliced lines, or None if not requested") },
    { NULL, NULL }
};

static PyStructSequence_Desc ZvbiCapture_ResultDef =
{
    "ZvbiCapture.Result",
    PyDoc_STR("Named tuple type returned by capturing, containing raw and sliced data"),
    ZvbiCapture_ResultDefMembers,
    4
};

vbi_capture *
ZvbiCapture_GetCtx(PyObject * self)
{
    return ((ZvbiCaptureObj*)self)->ctx;
}

int PyInit_Capture(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiCaptureTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiCaptureError = PyErr_NewException("Zvbi.CaptureError", error_base, NULL);
    Py_XINCREF(ZvbiCaptureError);
    if (PyModule_AddObject(module, "CaptureError", ZvbiCaptureError) < 0) {
        Py_XDECREF(ZvbiCaptureError);
        Py_CLEAR(ZvbiCaptureError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiCaptureTypeDef);
    if (PyModule_AddObject(module, "Capture", (PyObject *) &ZvbiCaptureTypeDef) < 0) {
        Py_DECREF(&ZvbiCaptureTypeDef);
        Py_XDECREF(ZvbiCaptureError);
        Py_CLEAR(ZvbiCaptureError);
        return -1;
    }

    // create result container class type object
#if defined (NAMED_TUPLE_GC_BUG)
    if (PyStructSequence_InitType2(&ZvbiCapture_ResultTypeBuf, &ZvbiCapture_ResultDef) != 0)
#else
    ZvbiCapture_ResultType = PyStructSequence_NewType(&ZvbiCapture_ResultDef);
    if (ZvbiCapture_ResultType == NULL)
#endif
    {
        Py_DECREF(&ZvbiCapture_ResultType);
        Py_DECREF(&ZvbiCaptureTypeDef);
        Py_DECREF(ZvbiCaptureError);
        return -1;
    }

    return 0;
}
