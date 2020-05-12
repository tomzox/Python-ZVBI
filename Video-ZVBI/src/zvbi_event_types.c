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

#include "zvbi_event_types.h"

// ---------------------------------------------------------------------------
//  Event type & sub-structures
// ---------------------------------------------------------------------------

#if defined (NAMED_TUPLE_GC_BUG)
static PyTypeObject ZvbiEvent_PageLinkTypeBuf;
static PyTypeObject ZvbiEvent_AspectRatioTypeBuf;
static PyTypeObject ZvbiEvent_ProgInfoTypeBuf;
static PyTypeObject ZvbiEvent_EventTtxTypeBuf;
static PyTypeObject ZvbiEvent_EventCaptionTypeBuf;
static PyTypeObject ZvbiEvent_EventNetworkTypeBuf;
static PyTypeObject * const ZvbiEvent_PageLinkType = &ZvbiEvent_PageLinkTypeBuf;
static PyTypeObject * const ZvbiEvent_AspectRatioType = &ZvbiEvent_AspectRatioTypeBuf;
static PyTypeObject * const ZvbiEvent_ProgInfoType = &ZvbiEvent_ProgInfoTypeBuf;
static PyTypeObject * const ZvbiEvent_EventTtxType = &ZvbiEvent_EventTtxTypeBuf;
static PyTypeObject * const ZvbiEvent_EventCaptionType = &ZvbiEvent_EventCaptionTypeBuf;
static PyTypeObject * const ZvbiEvent_EventNetworkType = &ZvbiEvent_EventNetworkTypeBuf;
#else
static PyTypeObject * ZvbiEvent_PageLinkType = NULL;
static PyTypeObject * ZvbiEvent_AspectRatioType = NULL;
static PyTypeObject * ZvbiEvent_ProgInfoType = NULL;
static PyTypeObject * ZvbiEvent_EventTtxType = NULL;
static PyTypeObject * ZvbiEvent_EventCaptionType = NULL;
static PyTypeObject * ZvbiEvent_EventNetworkType = NULL;
#endif

// ---------------------------------------------------------------------------

static PyObject *
ZvbiEvent_UnicodeFromLatin1(const char * str)
{
    PyObject * obj = PyUnicode_DecodeLatin1(str, strlen(str), "ignore");
    if (obj == NULL) {
        obj = PyUnicode_FromString("");
    }
    return obj;
}

PyObject *
ZvbiEvent_ObjFromPageLink( vbi_link * p_ld )
{
    PyObject * tuple = PyStructSequence_New(ZvbiEvent_PageLinkType);
    if (tuple != NULL) {
        // ATTENTION: indices must match order of definition in ZvbiEvent_PageLinkMembers
        PyStructSequence_SET_ITEM(tuple, 0, PyLong_FromLong(p_ld->type));
        PyStructSequence_SET_ITEM(tuple, 1, PyBool_FromLong(p_ld->eacem));
        PyStructSequence_SET_ITEM(tuple, 2, PyUnicode_FromString((char*)p_ld->name));
        PyStructSequence_SET_ITEM(tuple, 3, PyUnicode_FromString((char*)p_ld->url));
        PyStructSequence_SET_ITEM(tuple, 4, PyUnicode_FromString((char*)p_ld->script));
        PyStructSequence_SET_ITEM(tuple, 5, PyLong_FromLong(p_ld->nuid));
        PyStructSequence_SET_ITEM(tuple, 6, PyLong_FromLong(p_ld->pgno));
        PyStructSequence_SET_ITEM(tuple, 7, PyLong_FromLong(p_ld->subno));
        PyStructSequence_SET_ITEM(tuple, 8, PyLong_FromLong(p_ld->expires));
        PyStructSequence_SET_ITEM(tuple, 9, PyLong_FromLong(p_ld->itv_type));
        PyStructSequence_SET_ITEM(tuple, 10, PyLong_FromLong(p_ld->priority));
        PyStructSequence_SET_ITEM(tuple, 11, PyBool_FromLong(p_ld->autoload));
     }
     return tuple;
}

static PyObject *
ZvbiEvent_ObjFromAspetRatio( vbi_aspect_ratio * p_asp )
{
    PyObject * tuple = PyStructSequence_New(ZvbiEvent_AspectRatioType);
    if (tuple != NULL) {
        // ATTENTION: indices must match order of definition in ZvbiEvent_AspectRatioMembers
        PyStructSequence_SET_ITEM(tuple, 0, PyLong_FromLong(p_asp->first_line));
        PyStructSequence_SET_ITEM(tuple, 1, PyLong_FromLong(p_asp->last_line));
        PyStructSequence_SET_ITEM(tuple, 2, PyFloat_FromDouble(p_asp->ratio));
        PyStructSequence_SET_ITEM(tuple, 3, PyLong_FromLong(p_asp->film_mode));
        PyStructSequence_SET_ITEM(tuple, 4, PyLong_FromLong(p_asp->open_subtitles));
    }
    return tuple;
}

static PyObject *
ZvbiEvent_ObjFromProgramInfo( vbi_program_info * p_pi )
{
    PyObject * tuple = PyStructSequence_New(ZvbiEvent_ProgInfoType);
    if (tuple != NULL) {
        // ATTENTION: indices must match order of definition in ZvbiEvent_ProgInfoMembers
        PyStructSequence_SET_ITEM(tuple, 0, PyBool_FromLong(p_pi->future));

        //if (p_pi->month != -1)
        PyStructSequence_SET_ITEM(tuple, 1, PyLong_FromLong(p_pi->month));
        PyStructSequence_SET_ITEM(tuple, 2, PyLong_FromLong(p_pi->day));
        PyStructSequence_SET_ITEM(tuple, 3, PyLong_FromLong(p_pi->hour));
        PyStructSequence_SET_ITEM(tuple, 4, PyLong_FromLong(p_pi->min));

        PyStructSequence_SET_ITEM(tuple, 5, PyBool_FromLong(p_pi->tape_delayed));

        //if (p_pi->length_hour != -1)
        PyStructSequence_SET_ITEM(tuple, 6, PyLong_FromLong(p_pi->length_hour));
        PyStructSequence_SET_ITEM(tuple, 7, PyLong_FromLong(p_pi->length_min));

        //if (p_pi->elapsed_hour != -1)
        PyStructSequence_SET_ITEM(tuple, 8, PyLong_FromLong(p_pi->elapsed_hour));
        PyStructSequence_SET_ITEM(tuple, 9, PyLong_FromLong(p_pi->elapsed_min));
        PyStructSequence_SET_ITEM(tuple, 10, PyLong_FromLong(p_pi->elapsed_sec));

        //if (p_pi->title[0] != 0)  // ASCII
        PyStructSequence_SET_ITEM(tuple, 11, PyUnicode_FromString((char*)p_pi->title));

        //if (p_pi->type_classf != VBI_PROG_CLASSF_NONE)
        PyStructSequence_SET_ITEM(tuple, 12, PyLong_FromLong(p_pi->type_classf));

        //if (p_pi->type_classf == VBI_PROG_CLASSF_EIA_608)
        PyStructSequence_SET_ITEM(tuple, 13, PyLong_FromLong(p_pi->type_id[0]));
        PyStructSequence_SET_ITEM(tuple, 14, PyLong_FromLong(p_pi->type_id[0]));
        PyStructSequence_SET_ITEM(tuple, 15, PyLong_FromLong(p_pi->type_id[0]));
        PyStructSequence_SET_ITEM(tuple, 16, PyLong_FromLong(p_pi->type_id[0]));

        //if (p_pi->rating_auth != VBI_RATING_AUTH_NONE)
        PyStructSequence_SET_ITEM(tuple, 17, PyLong_FromLong(p_pi->rating_auth));
        PyStructSequence_SET_ITEM(tuple, 18, PyLong_FromLong(p_pi->rating_id));

        //if (p_pi->rating_auth == VBI_RATING_AUTH_TV_US)
        PyStructSequence_SET_ITEM(tuple, 19, PyLong_FromLong(p_pi->rating_dlsv));

        //if (p_pi->audio[0].mode != VBI_AUDIO_MODE_UNKNOWN)  // Latin-1
        PyStructSequence_SET_ITEM(tuple, 20, PyLong_FromLong(p_pi->audio[0].mode));
        PyStructSequence_SET_ITEM(tuple, 21, ZvbiEvent_UnicodeFromLatin1((char*)p_pi->audio[0].language));

        //if (p_pi->audio[1].mode != VBI_AUDIO_MODE_UNKNOWN)
        PyStructSequence_SET_ITEM(tuple, 22, PyLong_FromLong(p_pi->audio[1].mode));
        PyStructSequence_SET_ITEM(tuple, 23, PyUnicode_FromString((char*)p_pi->audio[1].language));

        //if (p_pi->caption_services != -1)  // Latin-1 encoding
        PyStructSequence_SET_ITEM(tuple, 24, PyLong_FromLong(p_pi->caption_services));
        PyObject * cap_tuple = PyTuple_New(8);
        if (cap_tuple != NULL) {
            for (int idx = 0; idx < 8; ++idx) {
                PyTuple_SetItem(tuple, idx,  ZvbiEvent_UnicodeFromLatin1((char*)p_pi->caption_language[idx]));
            }
            PyStructSequence_SET_ITEM(tuple, 25, cap_tuple);
        }
        else {
            PyStructSequence_SET_ITEM(tuple, 25, Py_None);
            Py_INCREF(Py_None);
        }

        //if (p_pi->aspect.first_line != -1)
        PyStructSequence_SET_ITEM(tuple, 26, ZvbiEvent_ObjFromAspetRatio(&p_pi->aspect));

        //if (p_pi->description[0][0] != 0)  // ASCII encoding
        PyStructSequence_SET_ITEM(tuple, 27, PyUnicode_FromFormat(  "%s" "%s%s" "%s%s" "%s%s"
                                                                  "%s%s" "%s%s" "%s%s" "%s%s",
                                                              (char*)p_pi->description[0],
                ((p_pi->description[1][0] != 0) ? "\n" : ""), (char*)p_pi->description[1],
                ((p_pi->description[2][0] != 0) ? "\n" : ""), (char*)p_pi->description[2],
                ((p_pi->description[3][0] != 0) ? "\n" : ""), (char*)p_pi->description[3],
                ((p_pi->description[4][0] != 0) ? "\n" : ""), (char*)p_pi->description[4],
                ((p_pi->description[5][0] != 0) ? "\n" : ""), (char*)p_pi->description[5],
                ((p_pi->description[6][0] != 0) ? "\n" : ""), (char*)p_pi->description[6],
                ((p_pi->description[7][0] != 0) ? "\n" : ""), (char*)p_pi->description[7]));
    }
    return tuple;
}

PyObject *
ZvbiEvent_ObjFromEvent( vbi_event * ev )
{
    PyObject * tuple;

    if (ev->type == VBI_EVENT_TTX_PAGE) {
        tuple = PyStructSequence_New(ZvbiEvent_EventTtxType);
        if (tuple != NULL) {
            // ATTENTION: indices must match order of definition in ZvbiEvent_EventTtxMembers
            PyStructSequence_SET_ITEM(tuple, 0, PyLong_FromLong(ev->ev.ttx_page.pgno));
            PyStructSequence_SET_ITEM(tuple, 1, PyLong_FromLong(ev->ev.ttx_page.subno));
            PyStructSequence_SET_ITEM(tuple, 2, PyBytes_FromStringAndSize((char*)ev->ev.ttx_page.raw_header, 40));
            PyStructSequence_SET_ITEM(tuple, 3, PyLong_FromLong(ev->ev.ttx_page.pn_offset));
            PyStructSequence_SET_ITEM(tuple, 4, PyBool_FromLong(ev->ev.ttx_page.roll_header));
            PyStructSequence_SET_ITEM(tuple, 5, PyBool_FromLong(ev->ev.ttx_page.header_update));
            PyStructSequence_SET_ITEM(tuple, 6, PyBool_FromLong(ev->ev.ttx_page.clock_update));
        }
    }
    else if (ev->type == VBI_EVENT_CAPTION) {
        tuple = PyStructSequence_New(ZvbiEvent_EventCaptionType);
        if (tuple != NULL) {
            // ATTENTION: indices must match order of definition in ZvbiEvent_EventCaptionMembers
            PyStructSequence_SET_ITEM(tuple, 0, PyLong_FromLong(ev->ev.caption.pgno));
        }
    }
    else if (   (ev->type == VBI_EVENT_NETWORK)
             || (ev->type == VBI_EVENT_NETWORK_ID) )
    {
        tuple = PyStructSequence_New(ZvbiEvent_EventNetworkType);
        if (tuple != NULL) {
            // ATTENTION: indices must match order of definition in ZvbiEvent_EventNetworkMembers
            PyStructSequence_SET_ITEM(tuple, 0, PyLong_FromLong(ev->ev.network.nuid));
            PyStructSequence_SET_ITEM(tuple, 1, ZvbiEvent_UnicodeFromLatin1((char*)ev->ev.network.name));
            PyStructSequence_SET_ITEM(tuple, 2, ZvbiEvent_UnicodeFromLatin1((char*)ev->ev.network.call));
            PyStructSequence_SET_ITEM(tuple, 3, PyLong_FromLong(ev->ev.network.tape_delay));
            PyStructSequence_SET_ITEM(tuple, 4, PyLong_FromLong(ev->ev.network.cni_vps));
            PyStructSequence_SET_ITEM(tuple, 5, PyLong_FromLong(ev->ev.network.cni_8301));
            PyStructSequence_SET_ITEM(tuple, 6, PyLong_FromLong(ev->ev.network.cni_8302));
        }
    }
    else if (ev->type == VBI_EVENT_TRIGGER) {
        tuple = ZvbiEvent_ObjFromPageLink(ev->ev.trigger);
    }
    else if (ev->type == VBI_EVENT_ASPECT) {
        tuple = ZvbiEvent_ObjFromAspetRatio(&ev->ev.aspect);
    }
    else if (ev->type == VBI_EVENT_PROG_INFO) {
        tuple = ZvbiEvent_ObjFromProgramInfo(ev->ev.prog_info);
    }
    else {
        tuple = Py_None;
        Py_INCREF(Py_None);
    }
    return tuple;
}

// ---------------------------------------------------------------------------

static PyStructSequence_Field ZvbiEvent_PageLinkMembers[] =
{
    /*  0 */ { "type", PyDoc_STR("Link type: One of VBI_LINK* constants") },  // int
    /*  1 */ { "eacem", PyDoc_STR("Link received via EACEM or ATVEF transport method") },  // bool
    /*  2 */ { "name", PyDoc_STR("Some descriptive text or empty") },  // string
    /*  3 */ { "url", PyDoc_STR("URL") },  // string
    /*  4 */ { "script", PyDoc_STR("A piece of ECMA script (Javascript), this may be used on WebTV or SuperTeletext pages to trigger some action. Usually empty.") },  // string
    /*  5 */ { "nuid", PyDoc_STR("Network ID for linking to pages on other channels") },  // int
    /*  6 */ { "pgno", PyDoc_STR("Teletext page number") },  // int
    /*  7 */ { "subno", PyDoc_STR("Teletext sub-page number") },  // int
    /*  8 */ { "expires", PyDoc_STR("The time in seconds and fractions since 1970-01-01 00:00 when the link should no longer be offered to the user, similar to a HTTP cache expiration date") },  // int
    /*  9 */ { "itv_type", PyDoc_STR("One of VBI_WEBLINK_* constants; only applicable to ATVEF triggers, else UNKNOWN") },  // int
    /* 10 */ { "priority", PyDoc_STR("Trigger priority (0=EMERGENCY, should never be blocked, 1..2=HIGH, 3..5=MEDIUM, 6..9=LOW) for ordering and filtering") },  // int
    /* 11 */ { "autoload", PyDoc_STR("Open the target without user confirmation") },  // bool
             { NULL }
};

static PyStructSequence_Desc ZvbiEvent_PageLinkDef =
{
    "Zvbi.PageLink",
    PyDoc_STR("General purpose link description for ATVEF (ITV, WebTV in the United States) "
              "and EACEM triggers, Teletext TOP and FLOF, "
              "and links guessed from page-like numbers on teletext pages"),
    ZvbiEvent_PageLinkMembers,
    sizeof(ZvbiEvent_PageLinkMembers) / sizeof(ZvbiEvent_PageLinkMembers[0])
};

// ---------------------------------------------------------------------------

static PyStructSequence_Field ZvbiEvent_AspectRatioMembers[] =
{
    /*  0 */ { "first_line", PyDoc_STR("Describe start of active video (inclusive), i.e. without the black bars in letterbox mode") },  // int
    /*  1 */ { "last_line", PyDoc_STR("Describes enf of active video (inclusive)") },  // int
    /*  2 */ { "ratio", PyDoc_STR("The picture aspect ratio in anamorphic mode, 16/9 for example. Normal or letterboxed video has aspect ratio 1/1") },  // double
    /*  3 */ { "film_mode", PyDoc_STR("TRUE when the source is known to be film transferred to video, as opposed to interlaced video from a video camera.") },  // bool
    /*  4 */ { "open_subtitles", PyDoc_STR("Describes how subtitles are inserted into the picture: None, or overlay in picture, or in letterbox bars, or unknown.") },  // vbi_subt: enum[4]
             { NULL }
};

static PyStructSequence_Desc ZvbiEvent_AspectRatioDef =
{
    "Zvbi.AspectRatio",
    PyDoc_STR("Information about the picture aspect ratio and open subtitles."),
    ZvbiEvent_AspectRatioMembers,
    sizeof(ZvbiEvent_AspectRatioMembers) / sizeof(ZvbiEvent_AspectRatioMembers[0])
};

// ---------------------------------------------------------------------------

static PyStructSequence_Field ZvbiEvent_ProgInfoMembers[] =
{
    /*  0 */ { "current_or_next", PyDoc_STR("Indicates if entry refers to the current or next program") },  // bool
    /*  1 */ { "start_month", PyDoc_STR("Month of the start date") },  // int
    /*  2 */ { "start_day", PyDoc_STR("Day-of-month of the start date") },  // int
    /*  3 */ { "start_hour", PyDoc_STR("Hour of the start time") },  // int
    /*  4 */ { "start_min", PyDoc_STR("Minute of the start time") },  // int
    /*  5 */ { "tape_delayed", PyDoc_STR("Indicates if a program is routinely tape delayed for MST and PST") },  // bool
    /*  6 */ { "length_hour", PyDoc_STR("Duration in hours") },  // int
    /*  7 */ { "length_min", PyDoc_STR("Duration remainder in minutes") },  // int
    /*  8 */ { "elapsed_hour", PyDoc_STR("Already elapsed duration") },  // int
    /*  9 */ { "elapsed_min", PyDoc_STR("Already elapsed duration") },  // int
    /* 10 */ { "elapsed_sec", PyDoc_STR("Already elapsed duration") },  // int
    /* 11 */ { "title", PyDoc_STR("Program title text") },  // string
    /* 12 */ { "type_classf", PyDoc_STR("Scheme used for program type classification: One of VBI_PROG_CLASSF* constants") },  // int
    /* 13 */ { "type_id_0", PyDoc_STR("Program type classifier") },  // int
    /* 14 */ { "type_id_1", PyDoc_STR("Program type classifier") },  // int
    /* 15 */ { "type_id_2", PyDoc_STR("Program type classifier") },  // int
    /* 16 */ { "type_id_3", PyDoc_STR("Program type classifier") },  // int
    /* 17 */ { "rating_auth", PyDoc_STR("Scheme used for rating: One of VBI_RATING_AUTH* constants") },  // int
    /* 18 */ { "rating_id", PyDoc_STR("Rating classification") },  // int
    /* 19 */ { "rating_dlsv", PyDoc_STR("Additional rating for scheme VBI_RATING_TV_US") },  // int
    /* 20 */ { "audio_mode_a", PyDoc_STR("Audio mode: One of VBI_AUDIO_MODE* constants") },  // int
    /* 21 */ { "audio_language_a", PyDoc_STR("Audio language (audio channel A)") },  // string
    /* 22 */ { "audio_mode_b", PyDoc_STR("Audio mode (channel B)") },  // int
    /* 23 */ { "audio_language_b", PyDoc_STR("Audio language (audio channel B)") },  // string
    /* 24 */ { "caption_services", PyDoc_STR("Active caption pages: bits 0-7 correspond to caption pages 1-8") },  // int
    /* 25 */ { "caption_languages", PyDoc_STR("Tuple with caption language on all 8 CC pages") },  // string
    /* 26 */ { "aspect_ratio", PyDoc_STR("Aspect ratio, instance of Zvbi.AspectRatio") },  // sub-structure
    /* 27 */ { "description", PyDoc_STR("Program content description text") },  // string
             { NULL }
};

static PyStructSequence_Desc ZvbiEvent_ProgInfoDef =
{
    "Zvbi.ProgInfo",
    PyDoc_STR("Description of the current or next program content."),
    ZvbiEvent_ProgInfoMembers,
    sizeof(ZvbiEvent_ProgInfoMembers) / sizeof(ZvbiEvent_ProgInfoMembers[0])
};

// ---------------------------------------------------------------------------

static PyStructSequence_Field ZvbiEvent_EventTtxMembers[] =
{
    /*  0 */ { "pgno", PyDoc_STR("Teletext page number") },  // int
    /*  1 */ { "subno", PyDoc_STR("Teletext sub-page number") },  // int
    /*  2 */ { "raw_header", PyDoc_STR("Teletext page title text (raw, i.e. including page number, time, etc.)") },  // string
    /*  3 */ { "pn_offset", PyDoc_STR("Offset to the page number within the raw header text") },  // int
    /*  4 */ { "roll_header", PyDoc_STR("Indicates rolling header text") },  // bool
    /*  5 */ { "header_update", PyDoc_STR("Indicates header update") },  // bool
    /*  6 */ { "clock_update", PyDoc_STR("Indicates clock update") },  // bool
             { NULL }
};

static PyStructSequence_Desc ZvbiEvent_EventTtxDef =
{
    "Zvbi.EventTtx",
    PyDoc_STR("Event notification about reception of a teletext page"),
    ZvbiEvent_EventTtxMembers,
    sizeof(ZvbiEvent_EventTtxMembers) / sizeof(ZvbiEvent_EventTtxMembers[0])
};

// ---------------------------------------------------------------------------

static PyStructSequence_Field ZvbiEvent_EventCaptionMembers[] =
{
    /*  0 */ { "pgno", PyDoc_STR("Closed-caption page number") },  // int
             { NULL }
};

static PyStructSequence_Desc ZvbiEvent_EventCaptionDef =
{
    "Zvbi.EventCaption",
    PyDoc_STR("Event notification about reception of a closed-caption page"),
    ZvbiEvent_EventCaptionMembers,
    sizeof(ZvbiEvent_EventCaptionMembers) / sizeof(ZvbiEvent_EventCaptionMembers[0])
};

// ---------------------------------------------------------------------------

static PyStructSequence_Field ZvbiEvent_EventNetworkMembers[] =
{
    /*  0 */ { "nuid", PyDoc_STR("Network identifier") },  // vbi_nuid = unsigned int
    /*  1 */ { "name", PyDoc_STR("Name of the network from XDS or from a table lookup of CNIs in Teletext packet 8/30 or VPS") },  // string
    /*  2 */ { "call", PyDoc_STR("Network call letters, from XDS (i.e. closed-caption, US only), else empty") },  // string
    /*  3 */ { "tape_delay", PyDoc_STR("Tape delay in minutes, from XDS; 0 outside of US") },  // int
    /*  4 */ { "cni_vps", PyDoc_STR("Network ID received from VPS, or zero if unknown") },  // int
    /*  5 */ { "cni_8301", PyDoc_STR("Network ID received from teletext packet 8/30/1, or zero if unknown") },  // int
    /*  6 */ { "cni_8302", PyDoc_STR("Network ID received from teletext packet 8/30/2, or zero if unknown") },  // int
             { NULL }
};

static PyStructSequence_Desc ZvbiEvent_EventNetworkDef =
{
    "Zvbi.EventNetwork",
    PyDoc_STR("Event notification about reception of network identification"),
    ZvbiEvent_EventNetworkMembers,
    sizeof(ZvbiEvent_EventNetworkMembers) / sizeof(ZvbiEvent_EventNetworkMembers[0])
};

// ---------------------------------------------------------------------------

int PyInit_EventTypes(PyObject * module, PyObject * error_base)
{
#if defined (NAMED_TUPLE_GC_BUG)
    if (PyStructSequence_InitType2(&ZvbiEvent_PageLinkTypeBuf, &ZvbiEvent_PageLinkDef) != 0)
#else
    ZvbiEvent_PageLinkType = PyStructSequence_NewType(&ZvbiEvent_PageLinkDef);
    if (ZvbiEvent_PageLinkType == NULL)
#endif
    {
        Py_DECREF(&ZvbiEvent_PageLinkType);
        return -1;
    }
#if defined (NAMED_TUPLE_GC_BUG)
    if (PyStructSequence_InitType2(&ZvbiEvent_AspectRatioTypeBuf, &ZvbiEvent_AspectRatioDef) != 0)
#else
    ZvbiEvent_AspectRatioType = PyStructSequence_NewType(&ZvbiEvent_AspectRatioDef);
    if (ZvbiEvent_AspectRatioType == NULL)
#endif
    {
        Py_DECREF(&ZvbiEvent_AspectRatioType);
        Py_DECREF(&ZvbiEvent_PageLinkType);
        return -1;
    }
#if defined (NAMED_TUPLE_GC_BUG)
    if (PyStructSequence_InitType2(&ZvbiEvent_ProgInfoTypeBuf, &ZvbiEvent_ProgInfoDef) != 0)
#else
    ZvbiEvent_ProgInfoType = PyStructSequence_NewType(&ZvbiEvent_ProgInfoDef);
    if (ZvbiEvent_ProgInfoType == NULL)
#endif
    {
        Py_DECREF(&ZvbiEvent_ProgInfoType);
        Py_DECREF(&ZvbiEvent_AspectRatioType);
        Py_DECREF(&ZvbiEvent_PageLinkType);
        return -1;
    }
#if defined (NAMED_TUPLE_GC_BUG)
    if (PyStructSequence_InitType2(&ZvbiEvent_EventTtxTypeBuf, &ZvbiEvent_EventTtxDef) != 0)
#else
    ZvbiEvent_EventTtxType = PyStructSequence_NewType(&ZvbiEvent_EventTtxDef);
    if (ZvbiEvent_EventTtxType == NULL)
#endif
    {
        Py_DECREF(&ZvbiEvent_EventTtxType);
        Py_DECREF(&ZvbiEvent_ProgInfoType);
        Py_DECREF(&ZvbiEvent_AspectRatioType);
        Py_DECREF(&ZvbiEvent_PageLinkType);
        return -1;
    }
#if defined (NAMED_TUPLE_GC_BUG)
    if (PyStructSequence_InitType2(&ZvbiEvent_EventCaptionTypeBuf, &ZvbiEvent_EventCaptionDef) != 0)
#else
    ZvbiEvent_EventCaptionType = PyStructSequence_NewType(&ZvbiEvent_EventCaptionDef);
    if (ZvbiEvent_EventCaptionType == NULL)
#endif
    {
        Py_DECREF(&ZvbiEvent_EventCaptionType);
        Py_DECREF(&ZvbiEvent_EventTtxType);
        Py_DECREF(&ZvbiEvent_ProgInfoType);
        Py_DECREF(&ZvbiEvent_AspectRatioType);
        Py_DECREF(&ZvbiEvent_PageLinkType);
        return -1;
    }
#if defined (NAMED_TUPLE_GC_BUG)
    if (PyStructSequence_InitType2(&ZvbiEvent_EventNetworkTypeBuf, &ZvbiEvent_EventNetworkDef) != 0)
#else
    ZvbiEvent_EventNetworkType = PyStructSequence_NewType(&ZvbiEvent_EventNetworkDef);
    if (ZvbiEvent_EventNetworkType == NULL)
#endif
    {
        Py_DECREF(&ZvbiEvent_EventNetworkType);
        Py_DECREF(&ZvbiEvent_EventCaptionType);
        Py_DECREF(&ZvbiEvent_EventTtxType);
        Py_DECREF(&ZvbiEvent_ProgInfoType);
        Py_DECREF(&ZvbiEvent_AspectRatioType);
        Py_DECREF(&ZvbiEvent_PageLinkType);
        return -1;
    }

    return 0;
}
