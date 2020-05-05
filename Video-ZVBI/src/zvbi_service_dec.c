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

#include "zvbi_service_dec.h"
#include "zvbi_page.h"
#include "zvbi_capture_buf.h"
#include "zvbi_callbacks.h"

// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_decoder * ctx;
    PyObject * old_ev_cb;
    PyObject * old_ev_user_data;
} ZvbiServiceDecObj;

PyObject * ZvbiServiceDecError;

// ---------------------------------------------------------------------------

vbi_decoder *
ZvbiServiceDec_GetBuf(PyObject * self)
{
    assert(PyObject_IsInstance(self, (PyObject*)&ZvbiServiceDecTypeDef) == 1);
    return ((ZvbiServiceDecObj*) self)->ctx;
}

static PyObject *
ZvbiServiceDec_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiServiceDec_dealloc(ZvbiServiceDecObj *self)
{
    if (self->ctx) {
        vbi_decoder_delete(self->ctx);
    }
    if (self->old_ev_cb) {
        Py_DECREF(self->old_ev_cb);
    }
    if (self->old_ev_user_data) {
        Py_DECREF(self->old_ev_user_data);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiServiceDec_init(ZvbiServiceDecObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {NULL};
    int RETVAL = -1;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_decoder_delete(self->ctx);
        self->ctx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {
        self->ctx = vbi_decoder_new();

        if (self->ctx != NULL) {
            RETVAL = 0;
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "failed to create teletext decoder");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  Teletext Page De-Multiplexing & Caching
// ---------------------------------------------------------------------------

static PyObject *
ZvbiServiceDec_decode(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * sliced_obj;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "O!", &ZvbiCaptureSlicedBufTypeDef, &sliced_obj)) {
        vbi_capture_buffer * sliced_buffer = ZvbiCaptureBuf_GetBuf(sliced_obj);
        if ((sliced_buffer != NULL) && (sliced_buffer->data != NULL)) {
            vbi_sliced * p_sliced = sliced_buffer->data;
            int n_lines = sliced_buffer->size / sizeof(vbi_sliced);

            vbi_decode(self->ctx, p_sliced, n_lines, sliced_buffer->timestamp);
            RETVAL = Py_None;
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "Sliced capture buffer contains no data");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_channel_switched(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    unsigned nuid;

    if (PyArg_ParseTuple(args, "|I", &nuid)) {
        vbi_channel_switched(self->ctx, nuid);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_classify_page(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int pgno = 0;

    if (PyArg_ParseTuple(args, "i", &pgno)) {
        vbi_subno subno = 0;
        char * language = NULL;
        vbi_page_type type = vbi_classify_page(self->ctx, pgno, &subno, &language);

        RETVAL = PyTuple_New(3);
        if (RETVAL != NULL)
        {
            PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(type));
            PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(subno));
            if (language != NULL) {
                PyTuple_SetItem(RETVAL, 2, PyUnicode_DecodeLatin1(language, strlen(language), NULL));
            }
            else {
                PyTuple_SetItem(RETVAL, 2, Py_None);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_set_brightness(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int brightness = 0;

    if (PyArg_ParseTuple(args, "i", &brightness)) {
        vbi_set_brightness(self->ctx, brightness);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_set_contrast(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int contrast = 0;

    if (PyArg_ParseTuple(args, "i", &contrast)) {
        vbi_set_contrast(self->ctx, contrast);
        RETVAL = Py_None;
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  Teletext Page Caching
// ---------------------------------------------------------------------------

static PyObject *
ZvbiServiceDec_teletext_set_default_region(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int default_region = 0;

    if (PyArg_ParseTuple(args, "i", &default_region)) {
        vbi_teletext_set_default_region(self->ctx, default_region);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_teletext_set_level(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int level = 0;

    if (PyArg_ParseTuple(args, "i", &level)) {
        vbi_teletext_set_level(self->ctx, level);
        RETVAL = Py_None;
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_fetch_vt_page(ZvbiServiceDecObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"pgno", "subno", "max_level",
                              "display_rows", "navigation",
                              NULL};
    PyObject * RETVAL = NULL;
    int pgno = 0;
    int subno = VBI_ANY_SUBNO;
    int max_level = VBI_WST_LEVEL_3p5;
    int display_rows = 25;
    int navigation = 1;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "i|i$iii", kwlist,
                                    &pgno, &subno, &max_level,
                                    &display_rows, &navigation))
    {
        vbi_page * page = PyMem_RawMalloc(sizeof(vbi_page));
        if (page != NULL) {
            if (vbi_fetch_vt_page(self->ctx, page, pgno, subno,
                                  max_level, display_rows, navigation))
            {
                RETVAL = ZvbiPage_New(page, TRUE);
            }
            else {
                PyErr_SetString(ZvbiServiceDecError, "Failed to fetch page");
                PyMem_RawFree(page);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_fetch_cc_page(ZvbiServiceDecObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"pgno", "reset", NULL};
    PyObject * RETVAL = NULL;
    int pgno = 0;
    int reset = TRUE;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "i|p", kwlist, &pgno, &reset)) {
        vbi_page * page = PyMem_RawMalloc(sizeof(vbi_page));
        if (page != NULL) {
            if (vbi_fetch_cc_page(self->ctx, page, pgno, reset)) {
                RETVAL = ZvbiPage_New(page, TRUE);
            }
            else {
                PyErr_SetString(ZvbiServiceDecError, "Failed to fetch page");
                PyMem_RawFree(page);
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_page_title(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int pgno = 0;
    int subno = VBI_ANY_SUBNO;

    if (PyArg_ParseTuple(args, "i|i", &pgno, subno)) {
        char buf[42];
        if (vbi_page_title(self->ctx, pgno, subno, buf)) {
            RETVAL = PyUnicode_DecodeLatin1(buf, strlen(buf), NULL);
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "Failed to determine a page title");
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  Event Handling
// ---------------------------------------------------------------------------

/*
 * Convert event description structs into Perl hashes
 */
#if 0 // TODO
static void
zvbi_xs_aspect_ratio_to_hv( HV * hv, vbi_aspect_ratio * p_asp )
{
        hv_store_iv(hv, first_line, p_asp->first_line);
        hv_store_iv(hv, last_line, p_asp->last_line);
        hv_store_nv(hv, ratio, p_asp->ratio);
        hv_store_iv(hv, film_mode, p_asp->film_mode);
        hv_store_iv(hv, open_subtitles, p_asp->open_subtitles);
}

static void
zvbi_xs_prog_info_to_hv( HV * hv, vbi_program_info * p_pi )
{
        hv_store_iv(hv, future, p_pi->future);
        if (p_pi->month != -1) {
                hv_store_iv(hv, month, p_pi->month);
                hv_store_iv(hv, day, p_pi->day);
                hv_store_iv(hv, hour, p_pi->hour);
                hv_store_iv(hv, min, p_pi->min);
        }
        hv_store_iv(hv, tape_delayed, p_pi->tape_delayed);
        if (p_pi->length_hour != -1) {
                hv_store_iv(hv, length_hour, p_pi->length_hour);
                hv_store_iv(hv, length_min, p_pi->length_min);
        }
        if (p_pi->elapsed_hour != -1) {
                hv_store_iv(hv, elapsed_hour, p_pi->elapsed_hour);
                hv_store_iv(hv, elapsed_min, p_pi->elapsed_min);
                hv_store_iv(hv, elapsed_sec, p_pi->elapsed_sec);
        }
        if (p_pi->title[0] != 0) {
                hv_store_pv(hv, title, (char*)p_pi->title);
        }
        if (p_pi->type_classf != VBI_PROG_CLASSF_NONE) {
                hv_store_iv(hv, type_classf, p_pi->type_classf);
        }
        if (p_pi->type_classf == VBI_PROG_CLASSF_EIA_608) {
                AV * av = newAV();
                int idx;
                for (idx = 0; (idx < 33) && (p_pi->type_id[idx] != 0); idx++) {
                        av_push(av, newSViv(p_pi->type_id[idx]));
                }
                hv_store_rv(hv, type_id, (SV*)av);
        }
        if (p_pi->rating_auth != VBI_RATING_AUTH_NONE) {
                hv_store_iv(hv, rating_auth, p_pi->rating_auth);
                hv_store_iv(hv, rating_id, p_pi->rating_id);
        }
        if (p_pi->rating_auth == VBI_RATING_AUTH_TV_US) {
                hv_store_iv(hv, rating_dlsv, p_pi->rating_dlsv);
        }
        if (p_pi->audio[0].mode != VBI_AUDIO_MODE_UNKNOWN) {
                hv_store_iv(hv, mode_a, p_pi->audio[0].mode);
                if (p_pi->audio[0].language != NULL) {
                        hv_store_pv(hv, language_a, (char*)p_pi->audio[0].language);
                }
        }
        if (p_pi->audio[1].mode != VBI_AUDIO_MODE_UNKNOWN) {
                hv_store_iv(hv, mode_b, p_pi->audio[1].mode);
                if (p_pi->audio[1].language != NULL) {
                        hv_store_pv(hv, language_b, (char*)p_pi->audio[1].language);
                }
        }
        if (p_pi->caption_services != -1) {
                AV * av = newAV();
                int idx;
                hv_store_iv(hv, caption_services, p_pi->caption_services);
                for (idx = 0; idx < 8; idx++) {
                        av_push(av, newSVpv((char*)p_pi->caption_language[idx], 0));
                }
                hv_store_rv(hv, caption_language, (SV*)av);
        }
        if (p_pi->cgms_a != -1) {
                hv_store_iv(hv, cgms_a, p_pi->cgms_a);
        }
        if (p_pi->aspect.first_line != -1) {
                HV * hv = newHV();
                zvbi_xs_aspect_ratio_to_hv(hv, &p_pi->aspect);
                hv_store_rv(hv, aspect, (SV*)hv);
        }
        if (p_pi->description[0][0] != 0) {
                AV * av = newAV();
                int idx;
                for (idx = 0; idx < 8; idx++) {
                        av_push(av, newSVpv((char*)p_pi->description[idx], 0));
                }
                hv_store_rv(hv, description, (SV*)av);
        }
}
#endif // 0

PyObject *
ZvbiServiceDec_Event2Dict( vbi_event * ev )
{
    PyObject * dict = PyDict_New();

    if (dict != NULL) {
        vbi_bool ok = TRUE;

        if (ev->type == VBI_EVENT_TTX_PAGE) {
            ok = (PyDict_SetItemString(dict, "pgno", PyLong_FromLong(ev->ev.ttx_page.pgno)) == 0) &&
                 (PyDict_SetItemString(dict, "subno", PyLong_FromLong(ev->ev.ttx_page.subno)) == 0) &&
                 (PyDict_SetItemString(dict, "pn_offset", PyLong_FromLong(ev->ev.ttx_page.pn_offset)) == 0) &&
                 (PyDict_SetItemString(dict, "raw_header", PyBytes_FromStringAndSize((char*)ev->ev.ttx_page.raw_header, 40)) == 0) &&
                 (PyDict_SetItemString(dict, "roll_header", PyLong_FromLong(ev->ev.ttx_page.roll_header)) == 0) &&
                 (PyDict_SetItemString(dict, "header_update", PyLong_FromLong(ev->ev.ttx_page.header_update)) == 0) &&
                 (PyDict_SetItemString(dict, "clock_update", PyLong_FromLong(ev->ev.ttx_page.clock_update)) == 0);
        }
        else if (ev->type == VBI_EVENT_CAPTION) {
            ok = (PyDict_SetItemString(dict, "pgno", PyLong_FromLong(ev->ev.ttx_page.pgno)) == 0);
        }
        else if (   (ev->type == VBI_EVENT_NETWORK)
                 || (ev->type == VBI_EVENT_NETWORK_ID) )
        {
            ok = (PyDict_SetItemString(dict, "nuid", PyLong_FromLong(ev->ev.network.nuid)) == 0) &&
                 (PyDict_SetItemString(dict, "tape_delay", PyLong_FromLong(ev->ev.network.tape_delay)) == 0) &&
                 (PyDict_SetItemString(dict, "cni_vps", PyLong_FromLong(ev->ev.network.cni_vps)) == 0) &&
                 (PyDict_SetItemString(dict, "cni_8301", PyLong_FromLong(ev->ev.network.cni_8301)) == 0) &&
                 (PyDict_SetItemString(dict, "cni_8302", PyLong_FromLong(ev->ev.network.cni_8302)) == 0) &&
                 (PyDict_SetItemString(dict, "cycle", PyLong_FromLong(ev->ev.network.cycle)) == 0);

            if (ok && (ev->ev.network.name[0] != 0)) {
                PyObject * str = PyUnicode_DecodeLatin1((char*)ev->ev.network.name, strlen((char*)ev->ev.network.name), NULL);
                if (str != NULL) {
                    ok = (PyDict_SetItemString(dict, "name", str) == 0);
                }
            }
            if (ok && (ev->ev.network.call[0] != 0)) {
                PyObject * str = PyUnicode_DecodeLatin1((char*)ev->ev.network.call, strlen((char*)ev->ev.network.call), NULL);
                if (str != NULL) {
                    ok = (PyDict_SetItemString(dict, "call", str) == 0);
                }
            }
        }
        else if (ev->type == VBI_EVENT_TRIGGER) {
            Py_DECREF(dict);  // FIXME
            dict = zvbi_xs_page_link_to_hv(ev->ev.trigger);
        }
        else if (ev->type == VBI_EVENT_ASPECT) {
            //TODO zvbi_xs_aspect_ratio_to_hv(hv, &ev->ev.aspect);
        }
        else if (ev->type == VBI_EVENT_PROG_INFO) {
            //TODO zvbi_xs_prog_info_to_hv(hv, ev->ev.prog_info);
        }

        if (!ok) {
            Py_DECREF(dict);
            dict = NULL;
        }
    }
    return dict;
}

/*
 * Invoke callback for an event generated by the VT decoder
 */
static void
zvbi_xs_vt_event_handler( vbi_event * event, void * user_data )
{
    PyObject * cb_obj;
    unsigned cb_idx = PVOID2UINT(user_data);

    if ( (cb_idx < ZVBI_MAX_CB_COUNT) &&
         ((cb_obj = ZvbiCallbacks.event[cb_idx].p_cb) != NULL) )
    {
        PyObject * dict = ZvbiServiceDec_Event2Dict(event);

        // invoke the Python subroutine
        if (ZvbiCallbacks.event[cb_idx].p_data != NULL) {
            PyObject_CallFunction(cb_obj, "iOO", event->type, dict, ZvbiCallbacks.event[cb_idx].p_data);
        }
        else {
            PyObject_CallFunction(cb_obj, "iO", event->type, dict);
        }
        Py_DECREF(dict);

        // clear exceptions as we cannot handle them here
        if (PyErr_Occurred() != NULL) {
            PyErr_Print();
        }
    }
}


static PyObject *
ZvbiServiceDec_event_handler_register(ZvbiServiceDecObj *self, PyObject *args)
{
    int event_mask;
    PyObject * handler_obj = NULL;
    PyObject * user_data_obj = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "iO|O", &event_mask, &handler_obj, &user_data_obj)) {
        ZvbiCallbacks_free_by_ptr(ZvbiCallbacks.event, self, handler_obj, user_data_obj, TRUE);
        unsigned cb_idx = ZvbiCallbacks_alloc(ZvbiCallbacks.event, handler_obj, user_data_obj, self);
        if (cb_idx < ZVBI_MAX_CB_COUNT) {
            if (vbi_event_handler_register(self->ctx, event_mask,
                                           zvbi_xs_vt_event_handler,
                                           UINT2PVOID(cb_idx)))
            {
                RETVAL = Py_None;
            }
            else {
                PyErr_SetString(ZvbiServiceDecError, "Registration failed");
                ZvbiCallbacks_free_by_idx(ZvbiCallbacks.event, cb_idx);
            }
        }
        else {
            PyErr_SetString(ZvbiServiceDecError, "Overflow of callback table");
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiServiceDec_event_handler_unregister(ZvbiServiceDecObj *self, PyObject *args)
{
    PyObject * handler_obj = NULL;
    PyObject * user_data_obj = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "O|O", &handler_obj, &user_data_obj)) {
        unsigned cb_idx = ZvbiCallbacks_free_by_ptr(ZvbiCallbacks.event, self, handler_obj, user_data_obj, TRUE);
        vbi_event_handler_unregister(self->ctx, zvbi_xs_vt_event_handler, UINT2PVOID(cb_idx));
        RETVAL = Py_None;
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiServiceDec_MethodsDef[] =
{
    {"decode",           (PyCFunction) ZvbiServiceDec_decode,           METH_VARARGS, NULL },
    {"channel_switched", (PyCFunction) ZvbiServiceDec_channel_switched, METH_VARARGS, NULL },
    {"classify_page",    (PyCFunction) ZvbiServiceDec_classify_page,    METH_VARARGS, NULL },
    {"set_brightness",   (PyCFunction) ZvbiServiceDec_set_brightness,   METH_VARARGS, NULL },
    {"set_contrast",     (PyCFunction) ZvbiServiceDec_set_contrast,     METH_VARARGS, NULL },

    {"teletext_set_default_region", (PyCFunction) ZvbiServiceDec_teletext_set_default_region, METH_VARARGS, NULL },
    {"teletext_set_level", (PyCFunction) ZvbiServiceDec_teletext_set_level, METH_VARARGS, NULL },
    {"fetch_vt_page",    (PyCFunction) ZvbiServiceDec_fetch_vt_page,    METH_VARARGS | METH_KEYWORDS, NULL },
    {"fetch_cc_page",    (PyCFunction) ZvbiServiceDec_fetch_cc_page,    METH_VARARGS | METH_KEYWORDS, NULL },
    {"page_title",       (PyCFunction) ZvbiServiceDec_page_title,       METH_VARARGS, NULL },

    // event_handler_add, event_handler_remove: omitted b/c deprecated
    {"event_handler_register",   (PyCFunction) ZvbiServiceDec_event_handler_register,   METH_VARARGS, NULL },
    {"event_handler_unregister", (PyCFunction) ZvbiServiceDec_event_handler_unregister, METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiServiceDecTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.ServiceDec",
    .tp_doc = PyDoc_STR("Class for decoding data services (i.e. Teletext page de-multiplexing & caching)"),
    .tp_basicsize = sizeof(ZvbiServiceDecObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiServiceDec_new,
    .tp_init = (initproc) ZvbiServiceDec_init,
    .tp_dealloc = (destructor) ZvbiServiceDec_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiServiceDec_Repr,
    .tp_methods = ZvbiServiceDec_MethodsDef,
    //.tp_members = ZvbiServiceDec_Members,
};

int PyInit_ServiceDec(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiServiceDecTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiServiceDecError = PyErr_NewException("Zvbi.ServiceDecError", error_base, NULL);
    Py_XINCREF(ZvbiServiceDecError);
    if (PyModule_AddObject(module, "ServiceDecError", ZvbiServiceDecError) < 0) {
        Py_XDECREF(ZvbiServiceDecError);
        Py_CLEAR(ZvbiServiceDecError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiServiceDecTypeDef);
    if (PyModule_AddObject(module, "ServiceDec", (PyObject *) &ZvbiServiceDecTypeDef) < 0) {
        Py_DECREF(&ZvbiServiceDecTypeDef);
        Py_XDECREF(ZvbiServiceDecError);
        Py_CLEAR(ZvbiServiceDecError);
        return -1;
    }

    return 0;
}
