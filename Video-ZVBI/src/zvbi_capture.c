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
#include "zvbi_raw_params.h"
#include "zvbi_capture_buf.h"

// ---------------------------------------------------------------------------
//  VBI Capturing & Slicing
// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_capture * ctx;
    unsigned services;
} ZvbiCaptureObj;

static PyObject * ZvbiCaptureError;
static PyObject * ZvbiCaptureTimeout;

/*
 * This counter is used for limiting the life-time of capture buffer objects
 * that refer to static storage in the libzvbi library. The object encapsulates
 * a copy of the counter at the time of duration. The counter is incremented
 * for any operation that invalidates the capture buffer content. Access to the
 * object is rejected via exception when the counter no longer matches the
 * object.
 */
static int ZvbiCapture_PulledBufferSeqNo;

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

static void
ZvbiCapture_AppendErrorStr(char ** errorstr, const char * src, char * new_error)
{
    if (new_error != NULL) {
        if (*errorstr) {
            char * tmp = malloc(strlen(*errorstr) + strlen(src) + strlen(new_error) + 1+1);
            sprintf(tmp, "%s\n%s%s", *errorstr, src, new_error);
            free(*errorstr);
            free(new_error);
            *errorstr = tmp;
        }
        else {
            char * tmp = malloc(strlen(src) + strlen(new_error) + 1);
            sprintf(tmp, "%s%s", src, new_error);
            free(new_error);
            *errorstr = tmp;
        }
    }
}

static PyObject *
ZvbiCapture_NewDvb(PyObject *null_self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"dev", "dvb_pid", "trace", NULL};
    char * dev_name = NULL;
    unsigned dvb_pid = 0;
    int trace = FALSE;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "s|II", kwlist,
                                    &dev_name, &dvb_pid, &trace))
    {
        // note also works with default value 0 for PID (can be set later using dvb_filter)
        char * errorstr = NULL;
        vbi_capture * ctx = vbi_capture_dvb_new2(dev_name, dvb_pid, &errorstr, trace);

        if (ctx != NULL) {
            RETVAL = ZvbiCapture_new(&ZvbiCaptureTypeDef, NULL, NULL);
            if (RETVAL != NULL) {
                ((ZvbiCaptureObj*)RETVAL)->ctx = ctx;
            }
        }
        else {
            PyErr_SetString(ZvbiCaptureError, errorstr ? errorstr : "unknown error");
        }
        if (errorstr != NULL) {
            free(errorstr);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_NewAnalog(PyObject *null_self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"dev",
                              "services",   // analog only
                              "buffers",    // v4l2 only
                              "scanning",   // v4l1+bkr only
                              //"dev_fd",   // sidecar only
                              "proxy",
                              "strict",
                              "trace",
                              NULL};
    char * dev_name = NULL;
    unsigned services = 0;
    int buffers = 5;
    int scanning = 0;
    //int dev_fd = -1;
    PyObject * proxy = NULL;
    int strict = 0;
    int trace = FALSE;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "sI|$iiO!Ip", kwlist,
                                     &dev_name, &services,
                                     &buffers, &scanning, //&dev_fd,
                                     &ZvbiProxyTypeDef, &proxy,
                                     &strict, &trace))
    {
        char * errorstr = NULL;
        vbi_capture * ctx = NULL;
        // keep a copy as "services" parameter may be modified by constructors below
        unsigned opt_services = services;

        if (proxy != NULL) {
            services = opt_services;
            ctx = vbi_capture_proxy_new(ZvbiProxy_GetCtx(proxy), buffers, scanning,
                                        (services ? &services : NULL),
                                        strict, &errorstr);
        }
#if 0  /* obsolete */
        else if (dev_fd != -1) {
            services = opt_services;
            ctx = vbi_capture_v4l_sidecar_new(dev_name, dev_fd,
                                              (services ? &services : NULL),
                                              strict, &errorstr, trace);
        }
#endif
        else {
            char * tmp_errorstr = NULL;
            services = opt_services;
            ctx = vbi_capture_v4l2_new(dev_name, buffers,
                                       (services ?  &services : NULL),
                                       strict, &tmp_errorstr, trace);
            ZvbiCapture_AppendErrorStr(&errorstr, "V4L2 driver: ", tmp_errorstr);

#if 0  /* obsolete */
            if (ctx == NULL) {
                tmp_errorstr = NULL;
                services = opt_services;
                ctx = vbi_capture_v4l_new(dev_name, scanning,
                                          (services ?  &services : NULL),
                                          strict, &tmp_errorstr, trace);
                ZvbiCapture_AppendErrorStr(&errorstr, "V4L1 driver: ", tmp_errorstr);
            }
#endif
            if (ctx == NULL) {
                tmp_errorstr = NULL;
                services = opt_services;
                if (services != 0) {
                    ctx = vbi_capture_bktr_new(dev_name, scanning, &services, strict, &tmp_errorstr, trace);
                    ZvbiCapture_AppendErrorStr(&errorstr, "BSD bktr driver: ", tmp_errorstr);
                }
                else {
                    ZvbiCapture_AppendErrorStr(&errorstr, "BSD bktr driver: ",
                                               strdup("Zero for parameter service not supported"));
                }
            }
        }
        if (ctx != NULL) {
            RETVAL = ZvbiCapture_new(&ZvbiCaptureTypeDef, NULL, NULL);
            if (RETVAL != NULL) {
                ((ZvbiCaptureObj*)RETVAL)->ctx = ctx;
                ((ZvbiCaptureObj*)RETVAL)->services = services;
            }
        }
        else {
            PyErr_SetString(ZvbiCaptureError, errorstr ? errorstr : "unknown error");
        }
        if (errorstr != NULL) {
            free(errorstr);
        }
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
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "i", &timeout_ms)) {
        vbi_raw_decoder * p_par = vbi_capture_parameters(self->ctx);
        if (p_par != NULL) {
            size_t size_raw = (p_par->count[0] + p_par->count[1]) * p_par->bytes_per_line;
            void * raw_buffer = PyMem_Malloc(size_raw);

            double timestamp = 0;
            struct timeval tv;
            tv.tv_sec  = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            int st = vbi_capture_read_raw(self->ctx, raw_buffer, &timestamp, &tv);
            if (st > 0) {
                RETVAL = ZvbiCaptureRawBuf_FromData(raw_buffer, size_raw, timestamp);
            }
            else {
                if (st < 0) {
                    PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
                }
                else {
                    PyErr_SetNone(ZvbiCaptureTimeout);
                }
                PyMem_Free(raw_buffer);
            }
        }
        else {
            PyErr_Format(ZvbiCaptureError, "internal error: failed to query decoder parameters");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_read_sliced(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "i", &timeout_ms)) {
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
                RETVAL = ZvbiCaptureSlicedBuf_FromData(p_sliced, n_lines, timestamp);
            }
            else {
                if (st < 0) {
                    PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
                }
                else {
                    PyErr_SetNone(ZvbiCaptureTimeout);
                }
                PyMem_Free(p_sliced);
            }
        }
        else {
            PyErr_Format(ZvbiCaptureError, "internal error: failed to query decoder parameters");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_read(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "i", &timeout_ms)) {
        vbi_raw_decoder * p_par = vbi_capture_parameters(self->ctx);
        if (p_par != NULL) {
            size_t size_sliced = (p_par->count[0] + p_par->count[1]) * sizeof(vbi_sliced);
            size_t size_raw = (p_par->count[0] + p_par->count[1]) * p_par->bytes_per_line;
            void * raw_buffer = PyMem_Malloc(size_raw);
            vbi_sliced * p_sliced = (vbi_sliced*) PyMem_Malloc(size_sliced);

            int n_lines = 0;
            double timestamp = 0;
            struct timeval tv;
            tv.tv_sec  = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            int st = vbi_capture_read(self->ctx, raw_buffer, p_sliced, &n_lines, &timestamp, &tv);
            if (st > 0) {
                RETVAL = PyTuple_New(2);
                if (RETVAL) {
                    PyTuple_SetItem(RETVAL, 0, ZvbiCaptureRawBuf_FromData(raw_buffer, size_raw, timestamp));
                    PyTuple_SetItem(RETVAL, 1, ZvbiCaptureSlicedBuf_FromData(p_sliced, n_lines, timestamp));
                }
                else {
                    PyMem_Free(raw_buffer);
                    PyMem_Free(p_sliced);
                }
            }
            else {
                if (st < 0) {
                    PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
                }
                else {
                    PyErr_SetNone(ZvbiCaptureTimeout);
                }
                PyMem_Free(raw_buffer);
                PyMem_Free(p_sliced);
            }
        }
        else {
            PyErr_Format(ZvbiCaptureError, "internal error: failed to query decoder parameters");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_pull_raw(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "i", &timeout_ms)) {
        vbi_capture_buffer * raw_buffer = NULL;

        // invalidate previously returned capture buffer wrapper objects
        ZvbiCapture_PulledBufferSeqNo++;

        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int st = vbi_capture_pull_raw(self->ctx, &raw_buffer, &tv);
        if (st > 0) {
            RETVAL = ZvbiCaptureRawBuf_FromPtr(raw_buffer, &ZvbiCapture_PulledBufferSeqNo);
        }
        else {
            if (st < 0) {
                PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
            }
            else {
                PyErr_SetNone(ZvbiCaptureTimeout);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_pull_sliced(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "i", &timeout_ms)) {
        vbi_capture_buffer * sliced_buffer = NULL;

        // invalidate previously returned capture buffer wrapper objects
        ZvbiCapture_PulledBufferSeqNo++;

        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int st = vbi_capture_pull_sliced(self->ctx, &sliced_buffer, &tv);
        if (st > 0) {
            RETVAL = ZvbiCaptureSlicedBuf_FromPtr(sliced_buffer, &ZvbiCapture_PulledBufferSeqNo);
        }
        else {
            if (st < 0) {
                PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
            }
            else {
                PyErr_SetNone(ZvbiCaptureTimeout);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiCapture_pull(ZvbiCaptureObj *self, PyObject *args)
{
    int timeout_ms = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "i", &timeout_ms)) {
        vbi_capture_buffer * raw_buffer = NULL;
        vbi_capture_buffer * sliced_buffer = NULL;

        // invalidate previously returned capture buffer wrapper objects
        ZvbiCapture_PulledBufferSeqNo++;

        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int st = vbi_capture_pull(self->ctx, &raw_buffer, &sliced_buffer, &tv);
        if (st > 0) {
            RETVAL = PyTuple_New(2);
            if (RETVAL) {
                if (raw_buffer != NULL) {  // DVB devices may not return raw data
                    PyTuple_SetItem(RETVAL, 0, ZvbiCaptureRawBuf_FromPtr(raw_buffer, &ZvbiCapture_PulledBufferSeqNo));
                }
                else {
                    PyTuple_SetItem(RETVAL, 0, Py_None);
                    Py_INCREF(Py_None);
                }
                PyTuple_SetItem(RETVAL, 1, ZvbiCaptureSlicedBuf_FromPtr(sliced_buffer, &ZvbiCapture_PulledBufferSeqNo));
            }
        }
        else {
            if (st < 0) {
                PyErr_Format(ZvbiCaptureError, "capture error (%s)", strerror(errno));
            }
            else {
                PyErr_SetNone(ZvbiCaptureTimeout);
            }
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
        RETVAL = ZvbiRawParamsFromStruct(p_rd);
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
ZvbiCapture_update_services(ZvbiCaptureObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"services", "reset", "commit", "strict", NULL};
    unsigned int services = 0;
    int reset = TRUE;
    int commit = TRUE;
    int strict = 0;
    char * errorstr = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "I|$ppI", kwlist,
                                          &services, &reset, &commit, &strict))
    {
        services = vbi_capture_update_services(self->ctx, reset, commit, services, strict, &errorstr);
        if (services != 0) {
            RETVAL = PyLong_FromLong(services);
        }
        else {
            PyErr_SetString(ZvbiCaptureError, errorstr ? errorstr : "zero compatible services");
            RETVAL = NULL;
        }
        if (errorstr != NULL) {
            free(errorstr);
        }
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
    // static factory methods
    {"Dvb",             (PyCFunction) ZvbiCapture_NewDvb,          METH_VARARGS | METH_KEYWORDS | METH_STATIC, NULL },
    {"Analog",          (PyCFunction) ZvbiCapture_NewAnalog,       METH_VARARGS | METH_KEYWORDS | METH_STATIC, NULL },

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
    {"update_services", (PyCFunction) ZvbiCapture_update_services, METH_VARARGS | METH_KEYWORDS, NULL },
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
    //.tp_new = ZvbiCapture_new,  // instantation via factory methods only
    //.tp_init = (initproc) ZvbiCapture_init,
    .tp_dealloc = (destructor) ZvbiCapture_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiCapture_Repr,
    .tp_methods = ZvbiCapture_MethodsDef,
    //.tp_members = ZvbiCapture_Members,
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

    // create exception classes
    ZvbiCaptureError = PyErr_NewException("Zvbi.CaptureError", error_base, NULL);
    Py_XINCREF(ZvbiCaptureError);
    if (PyModule_AddObject(module, "CaptureError", ZvbiCaptureError) < 0) {
        Py_XDECREF(ZvbiCaptureError);
        Py_CLEAR(ZvbiCaptureError);
        Py_DECREF(module);
        return -1;
    }
    // create exception class
    ZvbiCaptureTimeout = PyErr_NewException("Zvbi.CaptureTimeout", error_base, NULL);
    Py_XINCREF(ZvbiCaptureTimeout);
    if (PyModule_AddObject(module, "CaptureTimeout", ZvbiCaptureTimeout) < 0) {
        Py_XDECREF(ZvbiCaptureTimeout);
        Py_CLEAR(ZvbiCaptureTimeout);
        Py_XDECREF(ZvbiCaptureError);
        Py_CLEAR(ZvbiCaptureError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiCaptureTypeDef);
    if (PyModule_AddObject(module, "Capture", (PyObject *) &ZvbiCaptureTypeDef) < 0) {
        Py_DECREF(&ZvbiCaptureTypeDef);
        Py_XDECREF(ZvbiCaptureTimeout);
        Py_CLEAR(ZvbiCaptureTimeout);
        Py_XDECREF(ZvbiCaptureError);
        Py_CLEAR(ZvbiCaptureError);
        return -1;
    }

    return 0;
}
