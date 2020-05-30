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

#include "zvbi_dvb_mux.h"
#include "zvbi_capture_buf.h"
#include "zvbi_raw_params.h"
#include "zvbi_callbacks.h"

// ---------------------------------------------------------------------------
//  DVB multiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_dvb_mux_obj_struct {
    PyObject_HEAD
    vbi_dvb_mux *   ctx;
    PyObject *      raw_params;

    // members for use in callback mode
    PyObject *      mux_cb;
    PyObject *      mux_user_data;

    // members for use in iterator mode (called "coroutine" in libzvbi)
    PyObject *      raw_buf_obj;
    PyObject *      sliced_buf_obj;
    const vbi_sliced * p_sliced_buf;
    unsigned        sliced_left;
    PyObject *      buffer_obj;
    unsigned        feed_service_mask;
    int64_t         feed_pts;
} ZvbiDvbMuxObj;

static PyObject * ZvbiDvbMuxError;

// ---------------------------------------------------------------------------

/*
 * Invoke callback in DVB PES and TS multiplexer to process generated
 * packets. Callback can return FALSE to discard remaining data.
 */
static vbi_bool
zvbi_xs_dvb_mux_handler( vbi_dvb_mux *          mx,
                         void *                 user_data,
                         const uint8_t *        packet,
                         unsigned int           packet_size )
{
    ZvbiDvbMuxObj * self = user_data;
    vbi_bool result = FALSE; /* defaults to "failure" result */

    if ((self != NULL) && (self->mux_cb != NULL)) {
        // generate parameter object
        PyObject * pkg_obj = PyBytes_FromStringAndSize((char*)packet, packet_size);
        if (pkg_obj) {
            // invoke the Python subroutine
            PyObject * cb_rslt =
                PyObject_CallFunctionObjArgs(self->mux_cb, pkg_obj, self->mux_user_data);

            // evaluate the result returned by the function
            if (cb_rslt) {
                result = (PyObject_IsTrue(cb_rslt) == 1);
            }
            Py_DECREF(pkg_obj);
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
ZvbiDvbMux_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiDvbMux_dealloc(ZvbiDvbMuxObj *self)
{
    if (self->ctx) {
        vbi_dvb_mux_delete(self->ctx);

        if (self->mux_cb != NULL) {
            Py_DECREF(self->mux_cb);
        }
        if (self->mux_user_data != NULL) {
            Py_DECREF(self->mux_user_data);
        }
        if (self->raw_params != NULL) {
            Py_DECREF(self->raw_params);
        }
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiDvbMux_init(ZvbiDvbMuxObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"pes", "ts_pid", "callback", "user_data",
                              "raw_par", NULL};
    int pes = FALSE;
    int ts_pid = 0;
    PyObject * callback = NULL;
    PyObject * user_data = NULL;
    PyObject * raw_par = NULL;
    int RETVAL = -1;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_dvb_mux_delete(self->ctx);

        if (self->mux_cb != NULL) {
            Py_DECREF(self->mux_cb);
            self->mux_cb = NULL;
        }
        if (self->mux_user_data != NULL) {
            Py_DECREF(self->mux_user_data);
            self->mux_user_data = NULL;
        }
        if (self->raw_params != NULL) {
            Py_DECREF(self->raw_params);
            self->raw_params = NULL;
        }
        self->ctx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|$pIOOO!", kwlist,
                                    &pes, &ts_pid, &callback, &user_data,
                                    &ZvbiRawParamsTypeDef, &raw_par) &&
        ZvbiCallbacks_CheckObj(callback))
    {
        if (((pes == FALSE) ^ (ts_pid == 0)) != 0) {  // error if both or neither are present
            if (pes) {
                if (callback != NULL) {
                    self->ctx = vbi_dvb_pes_mux_new(zvbi_xs_dvb_mux_handler, self);
                }
                else {
                    self->ctx = vbi_dvb_pes_mux_new(NULL, NULL);
                }
            }
            else {  // ts
                if (callback != NULL) {
                    self->ctx = vbi_dvb_ts_mux_new(ts_pid, zvbi_xs_dvb_mux_handler, self);
                }
                else {
                    self->ctx = vbi_dvb_ts_mux_new(ts_pid, NULL, NULL);
                }
            }

            if (self->ctx != NULL) {
                if (callback != NULL) {
                    self->mux_cb = callback;
                    Py_INCREF(callback);
                }
                if (user_data != NULL) {
                    self->mux_user_data = user_data;
                    Py_INCREF(user_data);
                }
                if (raw_par != NULL) {
                    self->raw_params = raw_par;
                    Py_INCREF(raw_par);
                }
                RETVAL = 0;
            }
            else {
                PyErr_SetString(ZvbiDvbMuxError, "Initialization failed");
            }
        }
        else {
            PyErr_SetString(PyExc_ValueError, "Exactly one of parameters pes or ts_pid have to be specified");
        }

    }
    return RETVAL;
}

static PyObject *
ZvbiDvbMux_reset(ZvbiDvbMuxObj *self, PyObject *args)
{
    vbi_dvb_mux_reset(self->ctx);

    if (self->sliced_buf_obj != NULL) {
        Py_DECREF(self->sliced_buf_obj);
        self->sliced_buf_obj = NULL;
        self->p_sliced_buf = NULL;
    }
    if (self->raw_buf_obj != NULL) {
        Py_DECREF(self->raw_buf_obj);
        self->raw_buf_obj = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
ZvbiDvbMux_feed(ZvbiDvbMuxObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"service_mask", "sliced_buf", "raw_buf", "pts", NULL};
    unsigned service_mask = VBI_SLICED_TELETEXT_B |
                            VBI_SLICED_VPS |
                            VBI_SLICED_CAPTION_625 |
                            VBI_SLICED_WSS_625;
    PyObject * sliced_obj = NULL;
    PyObject * raw_obj = NULL;
    long long pts = 0LL;
    PyObject * RETVAL = NULL;

    if ((self->mux_cb != NULL) || (self->sliced_buf_obj == NULL)) {
        if (PyArg_ParseTupleAndKeywords(args, kwds, "|I$O!O!L", kwlist,
                                        &service_mask,
                                        &ZvbiCaptureSlicedBufTypeDef, &sliced_obj,
                                        &ZvbiCaptureRawBufTypeDef, &raw_obj,
                                        &pts))
        {
            if (sliced_obj != NULL) {
                vbi_capture_buffer * p_raw_buf = NULL;
                vbi_raw_decoder * p_raw_par = NULL;

                if (raw_obj != NULL) {
                    if (self->raw_params != NULL) {
                        p_raw_par = ZvbiRawParamsGetStruct(self->raw_params);
                        p_raw_buf = ZvbiCaptureBuf_GetBuf(raw_obj);

                        if (p_raw_buf->size < (p_raw_par->count[0] + p_raw_par->count[1]) * p_raw_par->bytes_per_line) {
                            PyErr_Format(PyExc_TypeError,
                                         "Input raw buffer is smaller than required for "
                                         "VBI geometry (%d+%d lines with %d bytes per line)",
                                         p_raw_par->count[0], p_raw_par->count[1], p_raw_par->bytes_per_line);
                            p_raw_buf = NULL;
                            p_raw_par = NULL;
                        }
                    }
                    else {
                        PyErr_SetString(PyExc_TypeError, "Cannot feed raw data into DvbMux instance created without raw parameters");
                    }
                }
                if ((raw_obj == NULL) || (p_raw_buf != NULL)) {
                    vbi_capture_buffer * p_sliced = ZvbiCaptureBuf_GetBuf(sliced_obj);
                    unsigned sliced_lines = p_sliced->size / sizeof(vbi_sliced);

                    if (self->mux_cb != NULL) {
                        // Callback mode of operation: process all VBI data in one pass
                        if (vbi_dvb_mux_feed(self->ctx,
                                             p_sliced->data, sliced_lines,
                                             service_mask,
                                             (p_raw_buf ? p_raw_buf->data : NULL),
                                             (p_raw_buf ? p_raw_par : NULL),
                                             (int64_t)pts))
                        {
                            RETVAL = Py_None;
                            Py_INCREF(Py_None);
                        }
                        else {
                            PyErr_SetString(ZvbiDvbMuxError, "multiplexing failure");
                        }
                    }
                    else {
                        // Iteration mode: only store reference to the buffer
                        // data will be processed by following iteration
                        Py_INCREF(sliced_obj);
                        self->sliced_buf_obj = sliced_obj;
                        self->p_sliced_buf = p_sliced->data;
                        self->sliced_left = sliced_lines;
                        //self->buffer_obj allocated on demand during iteration

                        if (p_raw_buf != NULL) {
                            Py_INCREF(raw_obj);
                            self->raw_buf_obj = raw_obj;
                        }
                        else {
                            self->raw_buf_obj = NULL;
                        }
                        self->feed_service_mask = service_mask;
                        self->feed_pts = pts;

                        RETVAL = Py_None;
                        Py_INCREF(Py_None);
                    }
                }
            }
            else {
                PyErr_SetString(PyExc_ValueError, "Missing mandatory parameter 'sliced_buf'");
            }
        }
    }
    else {
        PyErr_SetString(PyExc_ValueError, "Previous feed buffer not drained via iteration yet");
    }
    return RETVAL;
}

/*
 * Implementation of the standard "__iter__" function
 */
static ZvbiDvbMuxObj *
ZvbiDvbMux_Iter(ZvbiDvbMuxObj *self)
{
    ZvbiDvbMuxObj * RETVAL = NULL;

    if ((self->mux_cb == NULL) && (self->sliced_buf_obj != NULL)) {
        // Note corresponding DECREF is done by caller after end of iteration
        Py_INCREF(self);
        RETVAL = self;
    }
    else {
        if (self->mux_cb != NULL) {
            PyErr_SetString(PyExc_TypeError, "DvbMux instance is configured for use with callback instead of iteration");
        }
        else {
            PyErr_SetString(PyExc_IndexError, "Feed is empty");
        }
    }
    return RETVAL;
}

/*
 * Implementation of the standard "__next__" function
 */
static PyObject *
ZvbiDvbMux_IterNext(ZvbiDvbMuxObj *self)
{
    PyObject * RETVAL = NULL;
    vbi_bool done = TRUE;

    if (self->sliced_buf_obj != NULL) {
        if (self->sliced_left > 0) {
            vbi_capture_buffer * p_raw_buf = (self->raw_buf_obj
                                    ? ZvbiCaptureBuf_GetBuf(self->raw_buf_obj) : NULL);
            unsigned max_pkg_size = vbi_dvb_mux_get_max_pes_packet_size(self->ctx) + 4;
            if (self->buffer_obj == NULL) {
                self->buffer_obj = PyBytes_FromStringAndSize(NULL, max_pkg_size);
            }
            uint8_t * p_buffer = (uint8_t*) PyBytes_AS_STRING(self->buffer_obj);
            unsigned buffer_left = max_pkg_size;

            if (vbi_dvb_mux_cor(self->ctx,
                                &p_buffer, &buffer_left,
                                &self->p_sliced_buf, &self->sliced_left,
                                self->feed_service_mask,
                                (p_raw_buf ? p_raw_buf->data : NULL),
                                (p_raw_buf ? ZvbiRawParamsGetStruct(self->raw_params) : NULL),
                                self->feed_pts))
            {
                if ((buffer_left == 0) && (self->sliced_left > 0)) {
                    done = FALSE;
                }

                if (buffer_left != max_pkg_size) {
                    _PyBytes_Resize(&self->buffer_obj, (Py_ssize_t)(max_pkg_size - buffer_left));
                    RETVAL = self->buffer_obj;
                    self->buffer_obj = NULL;  // now owned by RETVAL object
                }
            }
            else {
                vbi_capture_buffer * p_sliced = ZvbiCaptureBuf_GetBuf(self->sliced_buf_obj);
                PyErr_Format(ZvbiDvbMuxError, "Encoding failure at sliced line index %d",
                                              self->p_sliced_buf - (vbi_sliced*) p_sliced->data);
            }
        }
        if (done) {
            Py_DECREF(self->sliced_buf_obj);
            self->sliced_buf_obj = NULL;
            self->p_sliced_buf = NULL;

            if (self->raw_buf_obj != NULL) {
                Py_DECREF(self->raw_buf_obj);
                self->raw_buf_obj = NULL;
            }
            // keep possibly still allocated buffer_obj for next iteration
        }
    }
    if (RETVAL == NULL) {
        PyErr_SetNone(PyExc_StopIteration);
    }
    return RETVAL;
}


static PyObject *
ZvbiDvbMux_get_data_identifier(ZvbiDvbMuxObj *self, PyObject *args)
{
    unsigned id = vbi_dvb_mux_get_data_identifier(self->ctx);
    return PyLong_FromLong(id);
}

static PyObject *
ZvbiDvbMux_set_data_identifier(ZvbiDvbMuxObj *self, PyObject *args)
{
    int id = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "I", &id)) {
        if (vbi_dvb_mux_set_data_identifier(self->ctx, id)) {
            RETVAL = Py_None;
            Py_INCREF(Py_None);
        }
        else {
            PyErr_SetString(PyExc_TypeError, "invalid parameter value");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiDvbMux_get_min_pes_packet_size(ZvbiDvbMuxObj *self, PyObject *args)
{
    unsigned sz = vbi_dvb_mux_get_min_pes_packet_size(self->ctx);
    return PyLong_FromLong(sz);
}

static PyObject *
ZvbiDvbMux_get_max_pes_packet_size(ZvbiDvbMuxObj *self, PyObject *args)
{
    unsigned sz = vbi_dvb_mux_get_max_pes_packet_size(self->ctx);
    return PyLong_FromLong(sz);
}

static PyObject *
ZvbiDvbMux_set_pes_packet_size(ZvbiDvbMuxObj *self, PyObject *args)
{
    int min_size = 0;
    int max_size = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "II", &min_size, &max_size)) {
        if (vbi_dvb_mux_set_pes_packet_size(self->ctx, min_size, max_size)) {
            RETVAL = Py_None;
            Py_INCREF(Py_None);
        }
        else {
            PyErr_SetString(ZvbiDvbMuxError, "failed to set packet size (out of memory)");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiDvbMux_multiplex_sliced(ZvbiDvbMuxObj *self, PyObject *args)
{
    PyObject * pkg_buf_obj;
    unsigned   pkg_left;
    PyObject * sliced_obj;
    unsigned   sliced_left;
    unsigned   service_mask = VBI_SLICED_TELETEXT_B |
                              VBI_SLICED_VPS |
                              VBI_SLICED_CAPTION_625 |
                              VBI_SLICED_WSS_625;
    unsigned   data_identifier = 0x10;
    int        stuffing = FALSE;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "YIO!I|IIp",
                               &pkg_buf_obj, &pkg_left,
                               &ZvbiCaptureSlicedBufTypeDef, &sliced_obj, &sliced_left,
                               &service_mask, &data_identifier, &stuffing)) {
        if (PyByteArray_Size(pkg_buf_obj) >= pkg_left) {
            vbi_capture_buffer * p_sliced_buf = ZvbiCaptureBuf_GetBuf(sliced_obj);
            unsigned sliced_cnt = p_sliced_buf->size / sizeof(vbi_sliced);
            const vbi_sliced * p_sliced = p_sliced_buf->data;

            if (sliced_left <= sliced_cnt) {
                uint8_t * p_pkg = (uint8_t*) PyByteArray_AsString(pkg_buf_obj);

                p_pkg += PyByteArray_Size(pkg_buf_obj) - pkg_left;
                p_sliced += sliced_cnt - sliced_left;

                if (vbi_dvb_multiplex_sliced(&p_pkg, &pkg_left,
                                             &p_sliced, &sliced_left,
                                             service_mask, data_identifier, stuffing))
                {
                    RETVAL = PyTuple_New(2);
                    if (RETVAL != NULL) {
                        PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(pkg_left));
                        PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(sliced_left));
                    }
                }
                else {
                    // FIXME return pkg_left, sliced_left
                    PyErr_SetString(ZvbiDvbMuxError, "encoding failed");
                }
            }
            else {
                PyErr_SetString(PyExc_ValueError, "sliced buffer has fewer lines than value of sliced_left");
            }
        }
        else {
            PyErr_SetString(PyExc_ValueError, "packet buffer is smaller than value of packet_left");
        }
    }
    return RETVAL;
}


static PyObject *
ZvbiDvbMux_multiplex_raw(ZvbiDvbMuxObj *self, PyObject *args)
{
    PyObject * pkg_buf_obj;
    unsigned   pkg_left;
    PyObject * raw_obj;
    unsigned   raw_left;
    unsigned   data_identifier;
    unsigned   videostd_set;
    unsigned   itu_line;
    unsigned   first_pixel_position;
    unsigned   n_pixels_total;
    int        stuffing = FALSE;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "YIO!IIIIII|p",
                               &pkg_buf_obj, &pkg_left,
                               &ZvbiCaptureRawBufTypeDef, &raw_obj, &raw_left,
                               &data_identifier, &videostd_set, &itu_line,
                               &first_pixel_position, &n_pixels_total, &stuffing)) {
        if (PyByteArray_Size(pkg_buf_obj) >= pkg_left) {
            vbi_capture_buffer * p_raw_buf = ZvbiCaptureBuf_GetBuf(raw_obj);
            if (raw_left <= p_raw_buf->size) {
                uint8_t * p_pkg = (uint8_t*) PyByteArray_AsString(pkg_buf_obj);

                p_pkg += PyByteArray_Size(pkg_buf_obj) - pkg_left;
                const uint8_t * p_raw = (uint8_t*)p_raw_buf->data + p_raw_buf->size - raw_left;

                if (vbi_dvb_multiplex_raw(&p_pkg, &pkg_left,
                                          &p_raw, &raw_left,
                                          data_identifier, videostd_set,
                                          itu_line, first_pixel_position,
                                          n_pixels_total, stuffing))
                {
                    RETVAL = PyTuple_New(2);
                    if (RETVAL != NULL) {
                        PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(pkg_left));
                        PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(raw_left));
                    }
                }
                else {
                    PyErr_SetString(ZvbiDvbMuxError, "encoding failed");
                }
            }
            else {
                PyErr_SetString(PyExc_ValueError, "raw buffer has fewer lines than value of raw_left");
            }
        }
        else {
            PyErr_SetString(PyExc_ValueError, "packet buffer is smaller than value of packet_left");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiDvbMux_MethodsDef[] =
{
    {"reset",                   (PyCFunction) ZvbiDvbMux_reset,                   METH_NOARGS, NULL },
    {"feed",                    (PyCFunction) ZvbiDvbMux_feed,                    METH_VARARGS | METH_KEYWORDS, NULL },
    {"get_data_identifier",     (PyCFunction) ZvbiDvbMux_get_data_identifier,     METH_NOARGS, NULL },
    {"set_data_identifier",     (PyCFunction) ZvbiDvbMux_set_data_identifier,     METH_VARARGS, NULL },
    {"get_min_pes_packet_size", (PyCFunction) ZvbiDvbMux_get_min_pes_packet_size, METH_NOARGS, NULL },
    {"get_max_pes_packet_size", (PyCFunction) ZvbiDvbMux_get_max_pes_packet_size, METH_NOARGS, NULL },
    {"set_pes_packet_size",     (PyCFunction) ZvbiDvbMux_set_pes_packet_size,     METH_VARARGS, NULL },

    // static methods
    {"multiplex_sliced",        (PyCFunction) ZvbiDvbMux_multiplex_sliced,        METH_VARARGS | METH_STATIC, NULL },
    {"multiplex_raw",           (PyCFunction) ZvbiDvbMux_multiplex_raw,           METH_VARARGS | METH_STATIC, NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiDvbMuxTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.DvbMux",
    .tp_doc = PyDoc_STR("Class for generating a DVB PES or TS stream from VBI sliced or raw data"),
    .tp_basicsize = sizeof(ZvbiDvbMuxObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiDvbMux_new,
    .tp_init = (initproc) ZvbiDvbMux_init,
    .tp_dealloc = (destructor) ZvbiDvbMux_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiDvbMux_Repr,
    .tp_methods = ZvbiDvbMux_MethodsDef,
    //.tp_members = ZvbiDvbMux_Members,
    .tp_iter = (getiterfunc) ZvbiDvbMux_Iter,
    .tp_iternext = (iternextfunc) ZvbiDvbMux_IterNext,
};

int PyInit_DvbMux(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiDvbMuxTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiDvbMuxError = PyErr_NewException("Zvbi.DvbMuxError", error_base, NULL);
    Py_XINCREF(ZvbiDvbMuxError);
    if (PyModule_AddObject(module, "DvbMuxError", ZvbiDvbMuxError) < 0) {
        Py_XDECREF(ZvbiDvbMuxError);
        Py_CLEAR(ZvbiDvbMuxError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiDvbMuxTypeDef);
    if (PyModule_AddObject(module, "DvbMux", (PyObject *) &ZvbiDvbMuxTypeDef) < 0) {
        Py_DECREF(&ZvbiDvbMuxTypeDef);
        Py_XDECREF(ZvbiDvbMuxError);
        Py_CLEAR(ZvbiDvbMuxError);
        return -1;
    }

    return 0;
}
