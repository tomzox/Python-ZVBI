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

#include "zvbi_dvb_demux.h"
#include "zvbi_capture_buf.h"
#include "zvbi_callbacks.h"

// ---------------------------------------------------------------------------
//  DVB demultiplexer
// ---------------------------------------------------------------------------

// MPEG presentation timestamp has resolution of 90 kHz, while VBI timestamps
// use seconds-since 1970-Jan-01. As normally timestamps are only used for
// calculating delta, simply converting timer resolution should suffice.
#define PTS_TO_TIMESTAMP(PTS) ((PTS) * (1 / 90000.0))

// Default for the maximum number of sliced lines per frame (in iterator mode)
#define SLICED_LINE_CNT 64

typedef struct vbi_dvb_demux_obj_struct {
    PyObject_HEAD
    vbi_dvb_demux * ctx;

    PyObject *      log_cb;
    PyObject *      log_user_data;

    // members for use in callback mode
    PyObject *      demux_cb;
    PyObject *      demux_user_data;

    // members for use in iterator mode (called "coroutine" in libzvbi)
    unsigned        max_sliced_lines;
    Py_buffer       feed_buf;
    const uint8_t * p_feed_buf_src;
    unsigned int    feed_buf_left;
    vbi_sliced *    p_sliced_buf;

} ZvbiDvbDemuxObj;

static PyObject * ZvbiDvbDemuxError;

/*
 * This counter is used for limiting the life-time of capture buffer objects
 * produced by the callback to the duration of the callback. Later access to
 * the buffer will result in an exception.
 */
static int ZvbiDvbDemux_CaptureBufSeqNo;

// ---------------------------------------------------------------------------

/*
 * Invoke callback in DVB PES de-multiplexer to process sliced data.
 * Callback can return FALSE to abort decoding of the current buffer
 */
vbi_bool
zvbi_xs_dvb_pes_handler( vbi_dvb_demux *        dx,
                         void *                 user_data,
                         const vbi_sliced *     sliced,
                         unsigned int           sliced_lines,
                         int64_t                pts)
{
    ZvbiDvbDemuxObj * self = user_data;
    vbi_bool result = FALSE; /* defaults to "failure" result */

    if ((self != NULL) && (self->demux_cb != NULL)) {
        // invalidate previously returned capture buffer wrapper objects
        ZvbiDvbDemux_CaptureBufSeqNo++;

        vbi_capture_buffer cap_buf;
        cap_buf.data = (void*)sliced;  /* cast removes "const" */
        cap_buf.size = sizeof(vbi_sliced) * sliced_lines;
        cap_buf.timestamp = PTS_TO_TIMESTAMP(pts);

        PyObject * sliced_obj = ZvbiCaptureSlicedBuf_FromPtr(&cap_buf, &ZvbiDvbDemux_CaptureBufSeqNo);
        if (sliced_obj != NULL) {
            // invoke the Python subroutine
            PyObject * cb_rslt =
                PyObject_CallFunctionObjArgs(self->demux_cb, sliced_obj, self->demux_user_data);

            // evaluate the boolean result
            if (cb_rslt != NULL) {
                result = (PyObject_IsTrue(cb_rslt) == 1);
                Py_DECREF(cb_rslt);
            }
            Py_DECREF(sliced_obj);
        }

        // invalidate page wrapper object: life-time of buffer is duration of callback only
        ZvbiDvbDemux_CaptureBufSeqNo++;

        // clear exceptions as we cannot handle them here
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
    }
    return result;
}

void
zvbi_xs_dvb_log_handler( vbi_log_mask           level,
                         const char *           context,
                         const char *           message,
                         void *                 user_data)
{
    ZvbiDvbDemuxObj * self = user_data;
    PyObject * cb_rslt;

    if ((self != NULL) && (self->log_cb != NULL)) {
        // invoke the Python subroutine with parameters
        if (self->log_user_data != NULL) {
            cb_rslt = PyObject_CallFunction(self->log_cb, "iss",
                                            level, context, message, self->log_user_data);
        }
        else {
            cb_rslt = PyObject_CallFunction(self->log_cb, "iss",
                                            level, context, message);
        }

        // discard result
        if (cb_rslt != NULL) {
            Py_DECREF(cb_rslt);
        }

        // clear exceptions as we cannot handle them here
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
    }
}

// ---------------------------------------------------------------------------

static PyObject *
ZvbiDvbDemux_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiDvbDemux_dealloc(ZvbiDvbDemuxObj *self)
{
    if (self->ctx) {
        vbi_dvb_demux_delete(self->ctx);
    }

    if (self->demux_cb != NULL) {
        Py_DECREF(self->demux_cb);
    }
    if (self->demux_user_data != NULL) {
        Py_DECREF(self->demux_user_data);
    }
    if (self->log_cb != NULL) {
        Py_DECREF(self->log_cb);
    }
    if (self->log_user_data != NULL) {
        Py_DECREF(self->log_user_data);
    }

    if (self->p_sliced_buf != NULL) {
        PyMem_RawFree(self->p_sliced_buf);
    }

    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiDvbDemux_init(ZvbiDvbDemuxObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"callback", "user_data", "max_sliced", NULL};
    PyObject * callback = NULL;
    PyObject * user_data = NULL;
    unsigned max_sliced_lines = SLICED_LINE_CNT;
    int RETVAL = -1;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_dvb_demux_delete(self->ctx);

        if (self->demux_cb != NULL) {
            Py_DECREF(self->demux_cb);
            self->demux_cb = NULL;
        }
        if (self->demux_user_data != NULL) {
            Py_DECREF(self->demux_user_data);
            self->demux_user_data = NULL;
        }
        if (self->log_cb != NULL) {
            Py_DECREF(self->log_cb);
            self->log_cb = NULL;
        }
        if (self->log_user_data != NULL) {
            Py_DECREF(self->log_user_data);
            self->log_user_data = NULL;
        }
        if (self->p_sliced_buf != NULL) {
            PyMem_RawFree(self->p_sliced_buf);
            self->p_sliced_buf = NULL;
        }
        self->ctx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|$OOI", kwlist,
                                    &callback, &user_data, &max_sliced_lines) &&
        ZvbiCallbacks_CheckObj(callback))
    {
        if (callback != NULL) {
            self->ctx = vbi_dvb_pes_demux_new(zvbi_xs_dvb_pes_handler, self);
        }
        else {
            self->ctx = vbi_dvb_pes_demux_new(NULL, NULL);
        }

        if (self->ctx != NULL) {
            if (callback != NULL) {
                self->demux_cb = callback;
                Py_INCREF(callback);
            }
            if (user_data != NULL) {
                self->demux_user_data = user_data;
                Py_INCREF(user_data);
            }
            self->max_sliced_lines = max_sliced_lines;
            RETVAL = 0;
        }
        else {
            PyErr_SetString(ZvbiDvbDemuxError, "Initialization failed");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiDvbDemux_reset(ZvbiDvbDemuxObj *self, PyObject *args)
{
    vbi_dvb_demux_reset(self->ctx);

    if (self->feed_buf_left != 0) {
        PyBuffer_Release(&self->feed_buf);
        self->feed_buf.buf = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
ZvbiDvbDemux_feed(ZvbiDvbDemuxObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    if ((self->demux_cb != NULL) || (self->feed_buf.buf == NULL)) {
        if (PyArg_ParseTuple(args, "y*", &self->feed_buf)) {
            if (self->demux_cb != NULL) {
                // Callback mode of operation: process all VBI data in one pass
                if (vbi_dvb_demux_feed(self->ctx, self->feed_buf.buf, self->feed_buf.len)) {
                    RETVAL = Py_None;
                    Py_INCREF(Py_None);
                }
                else {
                    PyErr_SetString(ZvbiDvbDemuxError, "demux failure");
                }
                PyBuffer_Release(&self->feed_buf);
                self->feed_buf.buf = NULL;
            }
            else {
                // Iteration mode: only store reference to the buffer
                // data will be processed by following iteration
                assert(self->feed_buf.buf != NULL);
                self->feed_buf_left = self->feed_buf.len;
                self->p_feed_buf_src = (const uint8_t*) self->feed_buf.buf;
                //self->p_sliced_buf allocated on demand during iteration

                RETVAL = Py_None;
                Py_INCREF(Py_None);
            }
        }
    }
    else {
        PyErr_SetString(ZvbiDvbDemuxError, "Previous feed buffer not drained via iteration yet");
    }
    return RETVAL;
}

/*
 * Implementation of the standard "__iter__" function
 */
static ZvbiDvbDemuxObj *
ZvbiDvbDemux_Iter(ZvbiDvbDemuxObj *self)
{
    ZvbiDvbDemuxObj * RETVAL = NULL;

    if ((self->demux_cb == NULL) && (self->feed_buf.buf != NULL)) {
        // Note corresponding DECREF is done by caller after end of iteration
        Py_INCREF(self);
        RETVAL = self;
    }
    else {
        if (self->demux_cb != NULL) {
            PyErr_SetString(PyExc_TypeError, "DvbDemux instance is configured for use with callback instead of iteration");
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
ZvbiDvbDemux_IterNext(ZvbiDvbDemuxObj *self)
{
    PyObject * RETVAL = NULL;
    unsigned n_lines = 0;

    if (self->feed_buf.buf != NULL) {
        if (self->feed_buf_left > 0) {
            if (self->p_sliced_buf == NULL) {
                self->p_sliced_buf = PyMem_RawMalloc(sizeof(vbi_sliced) * self->max_sliced_lines);
            }
            int64_t pts;
            n_lines = vbi_dvb_demux_cor(self->ctx,
                                        self->p_sliced_buf, self->max_sliced_lines, &pts,
                                        &self->p_feed_buf_src, &self->feed_buf_left);
            if (n_lines > 0) {
                RETVAL = ZvbiCaptureSlicedBuf_FromData(self->p_sliced_buf, n_lines,
                                                       PTS_TO_TIMESTAMP(pts));
                self->p_sliced_buf = NULL;  // now owned by RETVAL object
            }
            else {
                assert(self->feed_buf_left == 0);
            }
        }
        if (self->feed_buf_left == 0) {
            PyBuffer_Release(&self->feed_buf);
            self->feed_buf.buf = NULL;
            // keep possibly still allocated p_sliced_buf for next iteration
        }
    }
    if (n_lines == 0) {
        PyErr_SetNone(PyExc_StopIteration);
    }
    return RETVAL;
}


static PyObject *
ZvbiDvbDemux_set_log_fn(ZvbiDvbDemuxObj *self, PyObject *args)
{
    int mask = 0;
    PyObject * callback = NULL;
    PyObject * user_data = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "I|OO", &mask, &callback, &user_data) &&
        ZvbiCallbacks_CheckObj(callback))
    {
        if (self->log_cb != NULL) {
            Py_DECREF(self->log_cb);
            self->log_cb = NULL;
        }
        if (self->log_user_data != NULL) {
            Py_DECREF(self->log_user_data);
            self->log_user_data = NULL;
        }

        if ((mask != 0) && (callback != NULL)) {
            if (callback != NULL) {
                self->log_cb = callback;
                Py_INCREF(callback);
            }
            if (self->log_user_data != NULL) {
                self->log_user_data = user_data;
                Py_INCREF(user_data);
            }
            vbi_dvb_demux_set_log_fn(self->ctx, mask, zvbi_xs_dvb_log_handler, self);
        }
        else {
            vbi_dvb_demux_set_log_fn(self->ctx, 0, NULL, NULL);
        }
        RETVAL = Py_None;
        Py_INCREF(Py_None);
    }
    return RETVAL;
}


// ---------------------------------------------------------------------------

static PyMethodDef ZvbiDvbDemux_MethodsDef[] =
{
    {"reset",      (PyCFunction) ZvbiDvbDemux_reset,      METH_NOARGS, NULL },
    {"feed",       (PyCFunction) ZvbiDvbDemux_feed,       METH_VARARGS, NULL },
    {"set_log_fn", (PyCFunction) ZvbiDvbDemux_set_log_fn, METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiDvbDemuxTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.DvbDemux",
    .tp_doc = PyDoc_STR("Class for extracting VBI data from a DVB stream"),
    .tp_basicsize = sizeof(ZvbiDvbDemuxObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiDvbDemux_new,
    .tp_init = (initproc) ZvbiDvbDemux_init,
    .tp_dealloc = (destructor) ZvbiDvbDemux_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiDvbDemux_Repr,
    .tp_methods = ZvbiDvbDemux_MethodsDef,
    //.tp_members = ZvbiDvbDemux_Members,
    .tp_iter = (getiterfunc) ZvbiDvbDemux_Iter,
    .tp_iternext = (iternextfunc) ZvbiDvbDemux_IterNext,
};

int PyInit_DvbDemux(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiDvbDemuxTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiDvbDemuxError = PyErr_NewException("Zvbi.DvbDemuxError", error_base, NULL);
    Py_XINCREF(ZvbiDvbDemuxError);
    if (PyModule_AddObject(module, "DvbDemuxError", ZvbiDvbDemuxError) < 0) {
        Py_XDECREF(ZvbiDvbDemuxError);
        Py_CLEAR(ZvbiDvbDemuxError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiDvbDemuxTypeDef);
    if (PyModule_AddObject(module, "DvbDemux", (PyObject *) &ZvbiDvbDemuxTypeDef) < 0) {
        Py_DECREF(&ZvbiDvbDemuxTypeDef);
        Py_XDECREF(ZvbiDvbDemuxError);
        Py_CLEAR(ZvbiDvbDemuxError);
        return -1;
    }

    return 0;
}
