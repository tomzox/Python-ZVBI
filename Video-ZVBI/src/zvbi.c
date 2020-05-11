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
#include "zvbi_capture.h"
#include "zvbi_capture_buf.h"
#include "zvbi_raw_dec.h"
#include "zvbi_service_dec.h"
#include "zvbi_page.h"
#include "zvbi_export.h"
#include "zvbi_search.h"
#include "zvbi_callbacks.h"

#include "zvbi_dvb_mux.h"
#include "zvbi_dvb_demux.h"
#include "zvbi_idl_demux.h"
#include "zvbi_pfc_demux.h"
#include "zvbi_xds_demux.h"

/* Version of library that contains all used interfaces
 * (which was released 2007, so we do not bother supporting older ones) */
#define ZVBI_XS_MIN_MICRO 26

/* macro to check for a minimum libzvbi header file version number */
#define LIBZVBI_H_VERSION(A,B,C) \
        ((VBI_VERSION_MAJOR>(A)) || \
        ((VBI_VERSION_MAJOR==(A)) && (VBI_VERSION_MINOR>(B))) || \
        ((VBI_VERSION_MAJOR==(A)) && (VBI_VERSION_MINOR==(B)) && (VBI_VERSION_MICRO>=(C))))

#if !(LIBZVBI_H_VERSION(0,2,ZVBI_XS_MIN_MICRO))
#error "Minimum version for libzvbi is 0.2." #ZVBI_XS_MIN_MICRO
#endif

PyObject * ZvbiError;

// ---------------------------------------------------------------------------

/*
 * Invoke callback for log messages.
 */
static void
zvbi_xs_log_callback( vbi_log_mask           level,
                      const char *           context,
                      const char *           message,
                      void *                 vp_cb_idx)
{
    unsigned cb_idx = PVOID2UINT(vp_cb_idx);
    PyObject * cb_obj;

    if ( (cb_idx < ZVBI_MAX_CB_COUNT) &&
         ((cb_obj = ZvbiCallbacks.log[cb_idx].p_cb) != NULL) )
    {
        PyObject * user_data = ZvbiCallbacks.log[cb_idx].p_data;  // INCREF done by Py_BuildValue

        // invoke the Python subroutine
        PyObject * cb_rslt = (user_data
            ? PyObject_CallFunction(cb_obj, "IssO", level, context, message, user_data)
            : PyObject_CallFunction(cb_obj, "Iss", level, context, message));

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
//  Parity and Hamming decoding and encoding
// ---------------------------------------------------------------------------

static PyObject *
Zvbi_par8(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned val;

    if (PyArg_ParseTuple(args, "I", &val)) {
        unsigned result = vbi_par8(val);
        RETVAL = PyLong_FromLong(result);
    }
    return RETVAL;
}

static PyObject *
Zvbi_unpar8(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned val;

    if (PyArg_ParseTuple(args, "I", &val)) {
        int result = vbi_unpar8(val);  // -1 on error
        RETVAL = PyLong_FromLong(result);
    }
    return RETVAL;
}

static PyObject *
Zvbi_par_str(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer in_buf;

    if (PyArg_ParseTuple(args, "s*", &in_buf)) {
        RETVAL = PyBytes_FromStringAndSize(in_buf.buf, in_buf.len);
        if (RETVAL) {
            char * out_buf = PyBytes_AsString(RETVAL);
            vbi_par((uint8_t*)out_buf, in_buf.len);
        }
        PyBuffer_Release(&in_buf);
    }
    return RETVAL;
}

static PyObject *
Zvbi_unpar_str(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer in_buf;
    int repl_char = -1;

    if (PyArg_ParseTuple(args, "y*|C", &in_buf, &repl_char)) {
        RETVAL = PyBytes_FromStringAndSize(in_buf.buf, in_buf.len);
        if (RETVAL) {
            char * out_buf = PyBytes_AsString(RETVAL);
            vbi_unpar((uint8_t*)out_buf, in_buf.len);
            if (repl_char >= 0) {
                char * p = out_buf;
                for (int idx = 0; idx < in_buf.len; ++idx, ++p) {
                    if (*p < 0) {
                        *p = repl_char;
                    }
                }
            }
        }
        PyBuffer_Release(&in_buf);
    }
    return RETVAL;
}

static PyObject *
Zvbi_rev8(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned val;

    if (PyArg_ParseTuple(args, "I", &val)) {
        unsigned result = vbi_rev8(val);
        RETVAL = PyLong_FromLong(result);
    }
    return RETVAL;
}

static PyObject *
Zvbi_rev16(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned val;

    if (PyArg_ParseTuple(args, "I", &val)) {
        unsigned result = vbi_rev16(val);
        RETVAL = PyLong_FromLong(result);
    }
    return RETVAL;
}

static PyObject *
Zvbi_rev16p(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer in_buf;
    unsigned offset = 0;

    if (PyArg_ParseTuple(args, "y*|I", &in_buf, offset)) {
        if (offset + 2 <= in_buf.len) {
            unsigned result = vbi_rev16p((uint8_t*)in_buf.buf + offset);
            RETVAL = PyLong_FromLong(result);
        }
        else {
            PyErr_SetString(ZvbiError, "rev16p: input data length must greater than offset by at least 2");
        }
        PyBuffer_Release(&in_buf);
    }
    return RETVAL;
}


static PyObject *
Zvbi_ham8(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned val;

    if (PyArg_ParseTuple(args, "I", &val)) {
        unsigned result = vbi_ham8(val);
        RETVAL = PyLong_FromLong(result);
    }
    return RETVAL;
}

static PyObject *
Zvbi_unham8(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned val;

    if (PyArg_ParseTuple(args, "I", &val)) {
        int result = vbi_unham8(val);  // may be -1
        RETVAL = PyLong_FromLong(result);
    }
    return RETVAL;
}

static PyObject *
Zvbi_unham16p(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer in_buf;
    unsigned offset = 0;

    if (PyArg_ParseTuple(args, "y*|I", &in_buf, offset)) {
        if (offset + 2 <= in_buf.len) {
            int result = vbi_unham16p((uint8_t*)in_buf.buf + offset);
            RETVAL = PyLong_FromLong(result);
        }
        else {
            PyErr_SetString(ZvbiError, "unham16p: input data length must greater than offset by at least 2");
        }
        PyBuffer_Release(&in_buf);
    }
    return RETVAL;
}

static PyObject *
Zvbi_unham24p(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer in_buf;
    unsigned offset = 0;

    if (PyArg_ParseTuple(args, "y*|I", &in_buf, offset)) {
        if (offset + 3 <= in_buf.len) {
            int result = vbi_unham24p((uint8_t*)in_buf.buf + offset);
            RETVAL = PyLong_FromLong(result);
        }
        else {
            PyErr_SetString(ZvbiError, "unham24p: input data length must greater than offset by at least 3");
        }
        PyBuffer_Release(&in_buf);
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  BCD arithmetic
// ---------------------------------------------------------------------------

static PyObject *
Zvbi_dec2bcd(PyObject *self, PyObject *args)
{
    unsigned int dec;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "I", &dec)) {
        RETVAL = PyLong_FromLong(vbi_dec2bcd(dec));
    }
    return RETVAL;
}

static PyObject *
Zvbi_bcd2dec(PyObject *self, PyObject *args)
{
    unsigned int bcd;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "I", &bcd)) {
        RETVAL = PyLong_FromLong(vbi_bcd2dec(bcd));
    }
    return RETVAL;
}

static PyObject *
Zvbi_add_bcd(PyObject *self, PyObject *args)
{
    unsigned int bcd1;
    unsigned int bcd2;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "II", &bcd1, &bcd2)) {
        RETVAL = PyLong_FromLong(vbi_add_bcd(bcd1, bcd2));
    }
    return RETVAL;
}

static PyObject *
Zvbi_is_bcd(PyObject *self, PyObject *args)
{
    unsigned int bcd;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "I", &bcd)) {
        RETVAL = PyBool_FromLong(vbi_is_bcd(bcd));
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  Miscellaneous
// ---------------------------------------------------------------------------

static PyObject *
Zvbi_lib_version(PyObject *self, PyObject *args)  // METH_NOARGS
{
    unsigned int major;
    unsigned int minor;
    unsigned int micro;

    vbi_version(&major, &minor, &micro);

    PyObject * tuple = PyTuple_New(3);
    PyTuple_SetItem(tuple, 0, PyLong_FromLong(major));
    PyTuple_SetItem(tuple, 1, PyLong_FromLong(minor));
    PyTuple_SetItem(tuple, 2, PyLong_FromLong(micro));

    return tuple;
}

static PyObject *
Zvbi_check_lib_version(PyObject *self, PyObject *args)
{
    unsigned int need_major = 0;
    unsigned int need_minor = 0;
    unsigned int need_micro = 0;
    unsigned int lib_major;
    unsigned int lib_minor;
    unsigned int lib_micro;

    if (!PyArg_ParseTuple(args, "I|II", &need_major, &need_minor, &need_micro))
        return NULL;

    vbi_version(&lib_major, &lib_minor, &lib_micro);

    vbi_bool ok = (lib_major > need_major) ||
                  ((lib_major == need_major) && (lib_minor > need_minor)) ||
                  ((lib_major == need_major) && (lib_minor == need_minor) && (lib_micro >= need_micro));

    return PyBool_FromLong(ok);
}

static PyObject *
Zvbi_set_log_fn(PyObject *self, PyObject *args)
{
    unsigned mask = 0;
    PyObject * log_fn = NULL;
    PyObject * user_data = NULL;
    PyObject * RETVAL = NULL;

    if (!PyArg_ParseTuple(args, "IO|O", &mask, &log_fn, &user_data))
        return NULL;

    ZvbiCallbacks_free_by_obj(ZvbiCallbacks.log, NULL);
    if (log_fn != NULL) {
        unsigned cb_idx = ZvbiCallbacks_alloc(ZvbiCallbacks.log, log_fn, user_data, NULL);
        if (cb_idx < ZVBI_MAX_CB_COUNT) {
            vbi_set_log_fn(mask, zvbi_xs_log_callback, UINT2PVOID(cb_idx));
            Py_INCREF(Py_None);
            RETVAL = Py_None;
        }
        else {
            PyErr_SetString(ZvbiError, "Max. log callback count exceeded");
        }
    }
    else {
        vbi_set_log_fn(0, NULL, NULL);
        Py_INCREF(Py_None);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
Zvbi_set_log_on_stderr(PyObject *self, PyObject *args)
{
    unsigned mask = 0;

    if (!PyArg_ParseTuple(args, "I", &mask))
        return NULL;

    ZvbiCallbacks_free_by_obj(ZvbiCallbacks.log, NULL);
    vbi_set_log_fn(mask, vbi_log_on_stderr, NULL);

    Py_RETURN_NONE;
}

static PyObject *
Zvbi_decode_vps_cni(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    PyObject * in_obj = NULL;

    if (PyArg_ParseTuple(args, "O!", &PyBytes_Type, &in_obj)) {
        unsigned int cni;
        char * p_data = NULL;
        Py_ssize_t buf_size = 0;

        if (PyBytes_AsStringAndSize(in_obj, &p_data, &buf_size) == 0) {
            if (buf_size >= 13) {
                vbi_decode_vps_cni(&cni, (uint8_t*)p_data);
                RETVAL = PyLong_FromLong(cni);
            }
            else {
                PyErr_SetString(ZvbiError, "decode_vps_cni: input buffer must have at least 13 bytes");
            }
        }
    }
    return RETVAL;
}

static PyObject *
Zvbi_encode_vps_cni(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned cni;

    if (PyArg_ParseTuple(args, "I", &cni)) {
        RETVAL = PyBytes_FromStringAndSize(NULL, 13);  // alloc uninitialized buffer
        char * p_buf = PyBytes_AsString(RETVAL);
        memset(p_buf, 0, 13);

        if (vbi_encode_vps_cni((uint8_t*)p_buf, cni) == FALSE) {
            PyErr_SetString(ZvbiError, "encode_vps_cni: invalid CNI");
            Py_DECREF(RETVAL);
            RETVAL = NULL;
        }
    }
    return RETVAL;
}

static PyObject *
Zvbi_iconv_caption(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    Py_buffer in_buf;
    int repl_char = '?';

    if (PyArg_ParseTuple(args, "s*|C", &in_buf, &repl_char)) {
        char * p_buf = vbi_strndup_iconv_caption("UTF-8", in_buf.buf, in_buf.len, repl_char);
        if (p_buf != NULL) {
            RETVAL = PyBytes_FromString(p_buf);
        }
        else {
            // source buffer contains invalid two-byte chars (or out of memory)
            PyErr_SetString(ZvbiError, "conversion failed");
        }
        PyBuffer_Release(&in_buf);
    }
    return RETVAL;
}

static PyObject *
Zvbi_caption_unicode(PyObject *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned inp_char = 0;
    int to_upper = FALSE;

    if (PyArg_ParseTuple(args, "I|p", &inp_char, &to_upper)) {
        unsigned ucs = vbi_caption_unicode(inp_char, to_upper);
        if (ucs != 0) {
            RETVAL = PyUnicode_New(1, 1114111);  // UCS-4
            if (RETVAL != NULL) {
                PyUnicode_WRITE(PyUnicode_KIND(RETVAL), PyUnicode_DATA(RETVAL), 0, ucs);
            }
        }
        else {
            PyErr_SetString(ZvbiError, "conversion failed");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef Zvbi_Methods[] =
{
    {"par8",              Zvbi_par8,              METH_VARARGS, NULL},
    {"unpar8",            Zvbi_unpar8,            METH_VARARGS, NULL},
    {"par_str",           Zvbi_par_str,           METH_VARARGS, NULL},
    {"unpar_str",         Zvbi_unpar_str,         METH_VARARGS, PyDoc_STR("Decode parity and convert to string")},
    {"rev8",              Zvbi_rev8,              METH_VARARGS, NULL},
    {"rev16",             Zvbi_rev16,             METH_VARARGS, NULL},
    {"rev16p",            Zvbi_rev16p,            METH_VARARGS, NULL},
    {"ham8",              Zvbi_ham8,              METH_VARARGS, NULL},
    {"unham8",            Zvbi_unham8,            METH_VARARGS, NULL},
    {"unham16p",          Zvbi_unham16p,          METH_VARARGS, NULL},
    {"unham24p",          Zvbi_unham24p,          METH_VARARGS, NULL},

    {"dec2bcd",           Zvbi_dec2bcd,           METH_VARARGS, NULL},
    {"bcd2dec",           Zvbi_bcd2dec,           METH_VARARGS, NULL},
    {"add_bcd",           Zvbi_add_bcd,           METH_VARARGS, NULL},
    {"is_bcd",            Zvbi_is_bcd,            METH_VARARGS, NULL},

    {"lib_version",       Zvbi_lib_version,       METH_NOARGS,  PyDoc_STR("Return tuple with library version")},
    {"check_lib_version", Zvbi_check_lib_version, METH_VARARGS, PyDoc_STR("Check if library version is equal or newer than the given")},
    {"set_log_fn",        Zvbi_set_log_fn,        METH_VARARGS, NULL},
    {"set_log_on_stderr", Zvbi_set_log_on_stderr, METH_VARARGS, NULL},

    {"decode_vps_cni",    Zvbi_decode_vps_cni,    METH_VARARGS, NULL},
    {"encode_vps_cni",    Zvbi_encode_vps_cni,    METH_VARARGS, NULL},

    {"iconv_caption",     Zvbi_iconv_caption,     METH_VARARGS, NULL},
    {"caption_unicode",   Zvbi_caption_unicode,   METH_VARARGS, NULL},

    {NULL}       // sentinel
};

static struct PyModuleDef Zvbi_module =
{
    PyModuleDef_HEAD_INIT,
    .m_name = "Zvbi",
    .m_doc = PyDoc_STR("Interface to the Zapping VBI decoder library (for teletext & closed-caption)"),
    .m_size = -1,
    .m_methods = Zvbi_Methods
};

PyMODINIT_FUNC
PyInit_Zvbi(void)
{
    PyObject * module = PyModule_Create(&Zvbi_module);
    if (module == NULL) {
        return NULL;
    }

    // create exception base class "Zvbi.error", derived from "Exception" base
    ZvbiError = PyErr_NewException("Zvbi.Error", PyExc_Exception, NULL);
    Py_XINCREF(ZvbiError);
    if (PyModule_AddObject(module, "Error", ZvbiError) < 0) {
        Py_XDECREF(ZvbiError);
        Py_CLEAR(ZvbiError);
        Py_DECREF(module);
        return NULL;
    }

    if ((PyInit_Capture(module, ZvbiError) < 0) ||
        (PyInit_Proxy(module, ZvbiError) < 0) ||
        (PyInit_RawDec(module, ZvbiError) < 0) ||
        (PyInit_CaptureBuf(module, ZvbiError) < 0) ||
        (PyInit_ServiceDec(module, ZvbiError) < 0) ||
        (PyInit_Page(module, ZvbiError) < 0) ||
        (PyInit_Export(module, ZvbiError) < 0) ||
        (PyInit_Search(module, ZvbiError) < 0))
    {
        Py_DECREF(module);
        return NULL;
    }

#define EXPORT_CONST(NAME) \
    {   if (PyModule_AddIntConstant(module, #NAME, NAME) < 0) { \
            Py_DECREF(module); \
            return NULL; \
    }   }

    /* capture interface */
    EXPORT_CONST( VBI_SLICED_NONE );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B_L10_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B_L25_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B );
    EXPORT_CONST( VBI_SLICED_VPS );
    EXPORT_CONST( VBI_SLICED_CAPTION_625_F1 );
    EXPORT_CONST( VBI_SLICED_CAPTION_625_F2 );
    EXPORT_CONST( VBI_SLICED_CAPTION_625 );
    EXPORT_CONST( VBI_SLICED_WSS_625 );
    EXPORT_CONST( VBI_SLICED_CAPTION_525_F1 );
    EXPORT_CONST( VBI_SLICED_CAPTION_525_F2 );
    EXPORT_CONST( VBI_SLICED_CAPTION_525 );
    EXPORT_CONST( VBI_SLICED_2xCAPTION_525 );
    EXPORT_CONST( VBI_SLICED_NABTS );
    EXPORT_CONST( VBI_SLICED_TELETEXT_BD_525 );
    EXPORT_CONST( VBI_SLICED_WSS_CPR1204 );
    EXPORT_CONST( VBI_SLICED_VBI_625 );
    EXPORT_CONST( VBI_SLICED_VBI_525 );
    EXPORT_CONST( VBI_SLICED_UNKNOWN );
    EXPORT_CONST( VBI_SLICED_ANTIOPE );
    EXPORT_CONST( VBI_SLICED_VPS_F2 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_A );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_C_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_D_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B_525 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_C_525 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_D_525 );

    /* VBI_CAPTURE_FD_FLAGS */
    EXPORT_CONST( VBI_FD_HAS_SELECT );
    EXPORT_CONST( VBI_FD_HAS_MMAP );
    EXPORT_CONST( VBI_FD_IS_DEVICE );

    /* proxy interface */
    EXPORT_CONST( VBI_PROXY_CLIENT_NO_TIMEOUTS );
    EXPORT_CONST( VBI_PROXY_CLIENT_NO_STATUS_IND );

    EXPORT_CONST( VBI_CHN_PRIO_BACKGROUND );
    EXPORT_CONST( VBI_CHN_PRIO_INTERACTIVE );
    EXPORT_CONST( VBI_CHN_PRIO_DEFAULT );
    EXPORT_CONST( VBI_CHN_PRIO_RECORD );

    EXPORT_CONST( VBI_CHN_SUBPRIO_MINIMAL );
    EXPORT_CONST( VBI_CHN_SUBPRIO_CHECK );
    EXPORT_CONST( VBI_CHN_SUBPRIO_UPDATE );
    EXPORT_CONST( VBI_CHN_SUBPRIO_INITIAL );
    EXPORT_CONST( VBI_CHN_SUBPRIO_VPS_PDC );

    EXPORT_CONST( VBI_PROXY_CHN_RELEASE );
    EXPORT_CONST( VBI_PROXY_CHN_TOKEN );
    EXPORT_CONST( VBI_PROXY_CHN_FLUSH );
    EXPORT_CONST( VBI_PROXY_CHN_NORM );
    EXPORT_CONST( VBI_PROXY_CHN_FAIL );
    EXPORT_CONST( VBI_PROXY_CHN_NONE );

    EXPORT_CONST( VBI_API_UNKNOWN );
    EXPORT_CONST( VBI_API_V4L1 );
    EXPORT_CONST( VBI_API_V4L2 );
    EXPORT_CONST( VBI_API_BKTR );

    EXPORT_CONST( VBI_PROXY_EV_CHN_GRANTED );
    EXPORT_CONST( VBI_PROXY_EV_CHN_CHANGED );
    EXPORT_CONST( VBI_PROXY_EV_NORM_CHANGED );
    EXPORT_CONST( VBI_PROXY_EV_CHN_RECLAIMED );
    EXPORT_CONST( VBI_PROXY_EV_NONE );

    /* demux */
    EXPORT_CONST( VBI_IDL_DATA_LOST );
    EXPORT_CONST( VBI_IDL_DEPENDENT );

    /* vt object */
    EXPORT_CONST( VBI_EVENT_NONE );
    EXPORT_CONST( VBI_EVENT_CLOSE );
    EXPORT_CONST( VBI_EVENT_TTX_PAGE );
    EXPORT_CONST( VBI_EVENT_CAPTION );
    EXPORT_CONST( VBI_EVENT_NETWORK );
    EXPORT_CONST( VBI_EVENT_TRIGGER );
    EXPORT_CONST( VBI_EVENT_ASPECT );
    EXPORT_CONST( VBI_EVENT_PROG_INFO );
    EXPORT_CONST( VBI_EVENT_NETWORK_ID );

    EXPORT_CONST( VBI_WST_LEVEL_1 );
    EXPORT_CONST( VBI_WST_LEVEL_1p5 );
    EXPORT_CONST( VBI_WST_LEVEL_2p5 );
    EXPORT_CONST( VBI_WST_LEVEL_3p5 );

    /* VT pages */
    EXPORT_CONST( VBI_LINK_NONE );
    EXPORT_CONST( VBI_LINK_MESSAGE );
    EXPORT_CONST( VBI_LINK_PAGE );
    EXPORT_CONST( VBI_LINK_SUBPAGE );
    EXPORT_CONST( VBI_LINK_HTTP );
    EXPORT_CONST( VBI_LINK_FTP );
    EXPORT_CONST( VBI_LINK_EMAIL );
    EXPORT_CONST( VBI_LINK_LID );
    EXPORT_CONST( VBI_LINK_TELEWEB );

    EXPORT_CONST( VBI_WEBLINK_UNKNOWN );
    EXPORT_CONST( VBI_WEBLINK_PROGRAM_RELATED );
    EXPORT_CONST( VBI_WEBLINK_NETWORK_RELATED );
    EXPORT_CONST( VBI_WEBLINK_STATION_RELATED );
    EXPORT_CONST( VBI_WEBLINK_SPONSOR_MESSAGE );
    EXPORT_CONST( VBI_WEBLINK_OPERATOR );

    EXPORT_CONST( VBI_SUBT_NONE );
    EXPORT_CONST( VBI_SUBT_ACTIVE );
    EXPORT_CONST( VBI_SUBT_MATTE );
    EXPORT_CONST( VBI_SUBT_UNKNOWN );

    EXPORT_CONST( VBI_BLACK );
    EXPORT_CONST( VBI_RED );
    EXPORT_CONST( VBI_GREEN );
    EXPORT_CONST( VBI_YELLOW );
    EXPORT_CONST( VBI_BLUE );
    EXPORT_CONST( VBI_MAGENTA );
    EXPORT_CONST( VBI_CYAN );
    EXPORT_CONST( VBI_WHITE );

    EXPORT_CONST( VBI_TRANSPARENT_SPACE );
    EXPORT_CONST( VBI_TRANSPARENT_FULL );
    EXPORT_CONST( VBI_SEMI_TRANSPARENT );
    EXPORT_CONST( VBI_OPAQUE );

    EXPORT_CONST( VBI_NORMAL_SIZE );
    EXPORT_CONST( VBI_DOUBLE_WIDTH );
    EXPORT_CONST( VBI_DOUBLE_HEIGHT );
    EXPORT_CONST( VBI_DOUBLE_SIZE );
    EXPORT_CONST( VBI_OVER_TOP );
    EXPORT_CONST( VBI_OVER_BOTTOM );
    EXPORT_CONST( VBI_DOUBLE_HEIGHT2 );
    EXPORT_CONST( VBI_DOUBLE_SIZE2 );

    EXPORT_CONST( VBI_NO_PAGE );
    EXPORT_CONST( VBI_NORMAL_PAGE );
    EXPORT_CONST( VBI_SUBTITLE_PAGE );
    EXPORT_CONST( VBI_SUBTITLE_INDEX );
    EXPORT_CONST( VBI_NONSTD_SUBPAGES );
    EXPORT_CONST( VBI_PROGR_WARNING );
    EXPORT_CONST( VBI_CURRENT_PROGR );
    EXPORT_CONST( VBI_NOW_AND_NEXT );
    EXPORT_CONST( VBI_PROGR_INDEX );
    EXPORT_CONST( VBI_PROGR_SCHEDULE );
    EXPORT_CONST( VBI_UNKNOWN_PAGE );

    /* search */
    EXPORT_CONST( VBI_ANY_SUBNO );
    EXPORT_CONST( VBI_SEARCH_ERROR );
    EXPORT_CONST( VBI_SEARCH_CACHE_EMPTY );
    EXPORT_CONST( VBI_SEARCH_CANCELED );
    EXPORT_CONST( VBI_SEARCH_NOT_FOUND );
    EXPORT_CONST( VBI_SEARCH_SUCCESS );

    /* export */
    EXPORT_CONST( VBI_PIXFMT_RGBA32_LE );
    EXPORT_CONST( VBI_PIXFMT_YUV420 );
    EXPORT_CONST( VBI_PIXFMT_PAL8 );

    EXPORT_CONST( VBI_OPTION_BOOL );
    EXPORT_CONST( VBI_OPTION_INT );
    EXPORT_CONST( VBI_OPTION_REAL );
    EXPORT_CONST( VBI_OPTION_STRING );
    EXPORT_CONST( VBI_OPTION_MENU );

    /* logging */
    EXPORT_CONST( VBI_LOG_ERROR );
    EXPORT_CONST( VBI_LOG_WARNING );
    EXPORT_CONST( VBI_LOG_NOTICE );
    EXPORT_CONST( VBI_LOG_INFO );
    EXPORT_CONST( VBI_LOG_DEBUG );
    EXPORT_CONST( VBI_LOG_DRIVER );
    EXPORT_CONST( VBI_LOG_DEBUG2 );
    EXPORT_CONST( VBI_LOG_DEBUG3 );

    return module;
}
