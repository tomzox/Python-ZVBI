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

#include "zvbi_proxy.h"
#include "zvbi_callbacks.h"

// ---------------------------------------------------------------------------
//  VBI Proxy Client
// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_proxy_client * ctx;
    PyObject * proxy_cb;
    PyObject * proxy_user_data;
} ZvbiProxyObj;

static PyObject * ZvbiProxyError;

// ---------------------------------------------------------------------------

/*
 * Invoke callback for an event generated by the proxy client
 */
static void
zvbi_xs_proxy_callback( void * user_data, VBI_PROXY_EV_TYPE ev_mask )
{
    ZvbiProxyObj * self = user_data;
    PyObject * cb_rslt;

    if ((self != NULL) && (self->proxy_cb != NULL)) {
        // invoke the Python subroutine
        if (self->proxy_user_data != NULL) {
            cb_rslt = PyObject_CallFunction(self->proxy_cb, "iO", ev_mask, self->proxy_user_data);
        }
        else {
            cb_rslt = PyObject_CallFunction(self->proxy_cb, "i", ev_mask);
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
ZvbiProxy_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiProxy_dealloc(ZvbiProxyObj *self)
{
    if (self->ctx) {
        vbi_proxy_client_destroy(self->ctx);

        if (self->proxy_cb != NULL) {
            Py_DECREF(self->proxy_cb);
        }
        if (self->proxy_user_data != NULL) {
            Py_DECREF(self->proxy_user_data);
        }
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiProxy_init(ZvbiProxyObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"dev",
                              "appname",
                              "appflags",
                              "trace",
                              NULL};
    char *dev_name = NULL;
    const char *p_client_name = NULL;
    int client_flags = 0;
    int trace_level = 0;
    char * errorstr = NULL;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_proxy_client_destroy(self->ctx);
        self->ctx = NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|ii", kwlist,
                                     &dev_name, &p_client_name,
                                     &client_flags, &trace_level))
    {
        return -1;
    }

    self->ctx = vbi_proxy_client_create(dev_name, p_client_name, client_flags,
                                        &errorstr, trace_level);

    if (self->ctx == NULL) {
        PyErr_SetString(ZvbiProxyError, errorstr ? errorstr : "unknown error");
        return -1;
    }
    return 0;
}


// This function is currently NOT supported because we must not create a 2nd
// reference to the C object. It's not worth the effort anyway since the
// application has already received the capture reference via
// vbi_capture_proxy_new()
//

#if 0  /* unsupported */
ZvbiCaptureObj *
vbi_proxy_client_get_capture_if(ZvbiProxyObj * vpc)
{
    vbi_capture * cap = vbi_proxy_client_get_capture_if(self->ctx);
}
#endif

static PyObject *
ZvbiProxy_set_callback(ZvbiProxyObj *self, PyObject *args)
{
    PyObject * handler_obj = NULL;
    PyObject * user_data_obj = NULL;

    if (!PyArg_ParseTuple(args, "|OO", &handler_obj, &user_data_obj) ||
        !ZvbiCallbacks_CheckObj(handler_obj))
    {
        return NULL;
    }

    // free old callback objects
    if (self->proxy_cb != NULL) {
        Py_DECREF(self->proxy_cb);
        self->proxy_cb = NULL;
    }
    if (self->proxy_user_data != NULL) {
        Py_DECREF(self->proxy_user_data);
        self->proxy_user_data = NULL;
    }
    if (handler_obj != NULL) {
        // register new callback
        self->proxy_cb = handler_obj;
        Py_INCREF(self->proxy_cb);
        if (user_data_obj != NULL) {
            self->proxy_user_data = user_data_obj;
            Py_INCREF(self->proxy_user_data);
        }
        vbi_proxy_client_set_callback(self->ctx, zvbi_xs_proxy_callback, self);
    }
    else {
        // remove existing callback registration
        vbi_proxy_client_set_callback(self->ctx, NULL, NULL);
    }
    Py_RETURN_NONE;
}


static PyObject *
ZvbiProxy_get_driver_api(ZvbiProxyObj *self, PyObject *args)
{
    int api = vbi_proxy_client_get_driver_api(self->ctx);
    return PyLong_FromLong(api);
}

static PyObject *
ZvbiProxy_channel_request(ZvbiProxyObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"chn_prio", "request_chn", "sub_prio",
                              "allow_suspend", "min_duration", "exp_duration",
                              NULL};
    int chn_prio = 0;  // VBI_CHN_PRIO
    int request_chn = FALSE;
    int sub_prio = -1;
    int allow_suspend = FALSE;
    int min_duration = -1;
    int exp_duration = -1;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "I|$pIpII", kwlist,
                                    &chn_prio, &request_chn, &sub_prio,
                                    &allow_suspend, &min_duration, &exp_duration))
    {
        vbi_channel_profile l_profile;
        memset(&l_profile, 0, sizeof(l_profile));

        if (!request_chn || ((sub_prio >= 0) && (min_duration >= 0) && (exp_duration >= 0))) {
            l_profile.is_valid = request_chn;
            l_profile.sub_prio = sub_prio;
            l_profile.allow_suspend = allow_suspend;
            l_profile.min_duration = min_duration;
            l_profile.exp_duration = exp_duration;

            int st = vbi_proxy_client_channel_request(self->ctx, chn_prio, &l_profile);
            if (st >= 0) {
                RETVAL = PyBool_FromLong(st);
            }
            else {
                PyErr_SetString(ZvbiProxyError, "proxy communication failure");
            }
        }
        else {
            PyErr_SetString(ZvbiProxyError, "Missing parameters sub_prio, min_duration, or exp_duration: required for 'request_chn:=True'");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiProxy_channel_notify(ZvbiProxyObj *self, PyObject *args)
{
    int notify_flags = 0;
    int scanning = 0;

    if (!PyArg_ParseTuple(args, "I|I", &notify_flags, &scanning)) {
        return NULL;
    }
    if (vbi_proxy_client_channel_notify(self->ctx, notify_flags, scanning) != 0) {
        PyErr_SetString(ZvbiProxyError, "proxy communication failure");
    }
    Py_RETURN_NONE;
}

static PyObject *
ZvbiProxy_channel_suspend(ZvbiProxyObj *self, PyObject *args)
{
    int cmd = 0;

    if (!PyArg_ParseTuple(args, "I", &cmd)) {
        return NULL;
    }
    if (vbi_proxy_client_channel_suspend(self->ctx, cmd) != 0) {
        PyErr_SetString(ZvbiProxyError, "proxy communication failure");
    }
    Py_RETURN_NONE;
}

static PyObject *
ZvbiProxy_device_ioctl(ZvbiProxyObj *self, PyObject *args)
{
    int request = -1;
    Py_buffer in_buf;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "Iy*", &request, &in_buf)) {
        // copy input buffer before the call, as it may get modified
        RETVAL = PyBytes_FromStringAndSize(in_buf.buf, in_buf.len);
        if (RETVAL != NULL) {
            char * p_out_buf = PyBytes_AS_STRING(RETVAL);
            if (vbi_proxy_client_device_ioctl(self->ctx, request, p_out_buf) != 0) {
                PyObject * tuple = PyTuple_New(2);
                if (tuple != NULL) {
                    PyTuple_SetItem(tuple, 0, PyLong_FromLong(errno));
                    PyTuple_SetItem(tuple, 1, PyUnicode_DecodeFSDefault(strerror(errno)));
                    PyErr_SetObject(PyExc_OSError, tuple);
                }
                Py_DECREF(RETVAL);
                RETVAL = NULL;
            }
        }
        PyBuffer_Release(&in_buf);
    }
    return RETVAL;
}

static PyObject *
ZvbiProxy_get_channel_desc(ZvbiProxyObj *self, PyObject *args)
{
    unsigned int scanning;
    vbi_bool granted;
    PyObject * RETVAL = NULL;

    if (vbi_proxy_client_get_channel_desc(self->ctx, &scanning, &granted) == 0) {
        RETVAL = PyTuple_New(2);
        if (RETVAL != NULL)
        {
            PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(scanning));
            PyTuple_SetItem(RETVAL, 1, PyBool_FromLong(granted));
        }
    }
    else {
        PyErr_SetString(ZvbiProxyError, "proxy communication failure");
    }
    return RETVAL;
}

static PyObject *
ZvbiProxy_has_channel_control(ZvbiProxyObj *self, PyObject *args)
{
    vbi_bool result = vbi_proxy_client_has_channel_control(self->ctx);
    return PyBool_FromLong(result);
}

// ---------------------------------------------------------------------------

vbi_proxy_client *
ZvbiProxy_GetCtx(PyObject * self)
{
    return ((ZvbiProxyObj*)self)->ctx;
}

static PyMethodDef ZvbiProxy_MethodsDef[] =
{
    {"set_callback",        (PyCFunction) ZvbiProxy_set_callback,        METH_VARARGS, NULL },
    {"get_driver_api",      (PyCFunction) ZvbiProxy_get_driver_api,      METH_NOARGS, NULL },
    {"channel_request",     (PyCFunction) ZvbiProxy_channel_request,     METH_VARARGS | METH_KEYWORDS, NULL },
    {"channel_notify",      (PyCFunction) ZvbiProxy_channel_notify,      METH_VARARGS, NULL },
    {"channel_suspend",     (PyCFunction) ZvbiProxy_channel_suspend,     METH_VARARGS, NULL },
    {"device_ioctl",        (PyCFunction) ZvbiProxy_device_ioctl,        METH_VARARGS, NULL },
    {"get_channel_desc",    (PyCFunction) ZvbiProxy_get_channel_desc,    METH_NOARGS, NULL },
    {"has_channel_control", (PyCFunction) ZvbiProxy_has_channel_control, METH_NOARGS, NULL },
    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiProxyTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.Proxy",
    .tp_doc = PyDoc_STR("Class connecting to proxy daemon for sharing a capture device"),
    .tp_basicsize = sizeof(ZvbiProxyObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiProxy_new,
    .tp_init = (initproc) ZvbiProxy_init,
    .tp_dealloc = (destructor) ZvbiProxy_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiProxy_Repr,
    .tp_methods = ZvbiProxy_MethodsDef,
    //.tp_members = ZvbiProxy_Members,
};

int PyInit_Proxy(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiProxyTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiProxyError = PyErr_NewException("Zvbi.ProxyError", error_base, NULL);
    Py_XINCREF(ZvbiProxyError);
    if (PyModule_AddObject(module, "ProxyError", ZvbiProxyError) < 0) {
        Py_XDECREF(ZvbiProxyError);
        Py_CLEAR(ZvbiProxyError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiProxyTypeDef);
    if (PyModule_AddObject(module, "Proxy", (PyObject *) &ZvbiProxyTypeDef) < 0) {
        Py_DECREF(&ZvbiProxyTypeDef);
        Py_XDECREF(ZvbiProxyError);
        Py_CLEAR(ZvbiProxyError);
        return -1;
    }

    return 0;
}
