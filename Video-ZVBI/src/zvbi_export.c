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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "zvbi_export.h"
#include "zvbi_page.h"

// ---------------------------------------------------------------------------
//  Teletext Page Export
// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_export * ctx;
} ZvbiExportObj;

PyObject * ZvbiExportError;

// ---------------------------------------------------------------------------

static PyObject *
zvbi_xs_export_info_to_hv( vbi_export_info * p_info )
{
    PyObject * dict = PyDict_New();

    if (dict != NULL) {
        vbi_bool ok = TRUE;
        PyObject * obj = PyUnicode_FromString(p_info->keyword);
        if (obj) {
            ok = (PyDict_SetItemString(dict, "keyword", obj) == 0);
        }
        if (ok) {
            obj = PyUnicode_FromString(p_info->label);
            if (obj) {
                ok = (PyDict_SetItemString(dict, "label", obj) == 0);
            }
        }
        if (ok) {
            obj = PyUnicode_FromString(p_info->tooltip);
            if (obj) {
                ok = (PyDict_SetItemString(dict, "tooltip", obj) == 0);
            }
        }
        if (ok) {
            obj = PyUnicode_FromString(p_info->mime_type);
            if (obj) {
                ok = (PyDict_SetItemString(dict, "mime_type", obj) == 0);
            }
        }
        if (ok) {
            obj = PyUnicode_FromString(p_info->extension);
            if (obj) {
                ok = (PyDict_SetItemString(dict, "extension", obj) == 0);
            }
        }

        if (!ok) {
            Py_DECREF(dict);
            dict = NULL;
        }
    }
    return dict;
}

PyObject *
zvbi_xs_export_option_info_to_hv( vbi_option_info * p_opt )
{
    PyObject * dict = PyDict_New();

    if (dict != NULL) {
#if 0
        vbi_bool has_menu;

        hv_store_iv(hv, type, p_opt->type);

        if (p_opt->keyword != NULL) {
                hv_store_pv(hv, keyword, p_opt->keyword);
        }
        if (p_opt->label != NULL) {
                hv_store_pv(hv, label, p_opt->label);
        }
        if (p_opt->tooltip != NULL) {
                hv_store_pv(hv, tooltip, p_opt->tooltip);
        }

        switch (p_opt->type) {
        case VBI_OPTION_BOOL:
        case VBI_OPTION_INT:
        case VBI_OPTION_MENU:
                hv_store_iv(hv, def, p_opt->def.num);
                hv_store_iv(hv, min, p_opt->min.num);
                hv_store_iv(hv, max, p_opt->max.num);
                hv_store_iv(hv, step, p_opt->step.num);
                has_menu = (p_opt->menu.num != NULL);
                break;
        case VBI_OPTION_REAL:
                hv_store_nv(hv, def, p_opt->def.dbl);
                hv_store_nv(hv, min, p_opt->min.dbl);
                hv_store_nv(hv, max, p_opt->max.dbl);
                hv_store_nv(hv, step, p_opt->step.dbl);
                has_menu = (p_opt->menu.dbl != NULL);
                break;
        case VBI_OPTION_STRING:
                if (p_opt->def.str != NULL) {
                        hv_store_pv(hv, def, p_opt->def.str);
                }
                if (p_opt->min.str != NULL) {
                        hv_store_pv(hv, min, p_opt->min.str);
                }
                if (p_opt->max.str != NULL) {
                        hv_store_pv(hv, max, p_opt->max.str);
                }
                if (p_opt->step.str != NULL) {
                        hv_store_pv(hv, step, p_opt->step.str);
                }
                has_menu = (p_opt->menu.str != NULL);
                break;
        default:
                /* error - the caller can detect this case by evaluating the type */
                has_menu = FALSE;
                break;
        }

        if (has_menu && (p_opt->min.num >= 0)) {
                int idx;
                AV * av = newAV();
                av_extend(av, p_opt->max.num);

                for (idx = p_opt->min.num; idx <= p_opt->max.num; idx++) {
                        switch (p_opt->type) {
                        case VBI_OPTION_BOOL:
                        case VBI_OPTION_INT:
                                av_store(av, idx, newSViv(p_opt->menu.num[idx]));
                                break;
                        case VBI_OPTION_REAL:
                                av_store(av, idx, newSVnv(p_opt->menu.dbl[idx]));
                                break;
                        case VBI_OPTION_MENU:
                        case VBI_OPTION_STRING:
                                if (p_opt->menu.str[idx] != NULL) {
                                        av_store(av, idx, newSVpv(p_opt->menu.str[idx], 0));
                                }
                                break;
                        default:
                                break;
                        }
                }
                hv_store_rv(hv, menu, (SV*)av);
        }

#endif // 0
    }
    return dict;
}

// ---------------------------------------------------------------------------

static PyObject *
ZvbiExport_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return type->tp_alloc(type, 0);
}

static void
ZvbiExport_dealloc(ZvbiExportObj *self)
{
    if (self->ctx) {
        vbi_export_delete(self->ctx);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiExport_init(ZvbiExportObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"keyword", NULL};
    char * keyword = "";
    char * errorstr = NULL;
    int RETVAL = -1;

    // reset state in case the module is already initialized
    if (self->ctx) {
        vbi_export_delete(self->ctx);
        self->ctx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &keyword)) {
        self->ctx = vbi_export_new(keyword, &errorstr);
        if (self->ctx != NULL) {
            RETVAL = 0;
        }
        else {
            PyErr_Format(ZvbiExportError, "failed to create export object: %s",
                         (errorstr ? errorstr : "reason unknown"));
        }
        if (errorstr != NULL) {
            free(errorstr);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_info_enum(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int index = 0;

    if (PyArg_ParseTuple(args, "i", &index)) {
        vbi_export_info * p_info = vbi_export_info_enum(index);
        if (p_info != NULL) {
            RETVAL = zvbi_xs_export_info_to_hv(p_info);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_info_keyword(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    char * keyword = NULL;

    if (PyArg_ParseTuple(args, "s", &keyword)) {
        vbi_export_info * p_info = vbi_export_info_keyword(keyword);
        if (p_info != NULL) {
            RETVAL = zvbi_xs_export_info_to_hv(p_info);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_info_export(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    vbi_export_info * p_info = vbi_export_info_export(self->ctx);
    if (p_info != NULL) {
        RETVAL = zvbi_xs_export_info_to_hv(p_info);
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_option_info_enum(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    int index = 0;

    if (PyArg_ParseTuple(args, "i", &index)) {
        vbi_option_info * p_opt = vbi_export_option_info_enum(self->ctx, index);
        if (p_opt != NULL) {
            RETVAL = zvbi_xs_export_option_info_to_hv(p_opt);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_option_info_keyword(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    char * keyword = NULL;

    if (PyArg_ParseTuple(args, "s", &keyword)) {
        vbi_option_info * p_opt = vbi_export_option_info_keyword(self->ctx, keyword);
        if (p_opt != NULL) {
            RETVAL = zvbi_xs_export_option_info_to_hv(p_opt);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_option_set(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    char * keyword = NULL;
    PyObject * obj = NULL;

    if (PyArg_ParseTuple(args, "sO", &keyword, &obj)) {
        vbi_option_info * p_info = vbi_export_option_info_keyword(self->ctx, keyword);
        if (p_info != NULL) {
            vbi_bool export_ok = TRUE;

            switch (p_info->type) {
                case VBI_OPTION_BOOL:
                case VBI_OPTION_INT:
                case VBI_OPTION_MENU:
                {
                    long val = PyLong_AsLong(obj);
                    if ((val != -1) || (PyErr_Occurred() == NULL)) {
                        export_ok = vbi_export_option_set(self->ctx, keyword, val);
                    }
                }
                case VBI_OPTION_REAL:
                {
                    double val = PyFloat_AsDouble(obj);
                    if ((val != -1.0) || (PyErr_Occurred() == NULL)) {
                        export_ok = vbi_export_option_set(self->ctx, keyword, val);
                    }
                }
                case VBI_OPTION_STRING:
                {
                    char * val = PyUnicode_AsUTF8(obj);
                    if (val != NULL) {
                        export_ok = vbi_export_option_set(self->ctx, keyword, val);
                    }
                }
                default:
                {
                    PyErr_Format(ZvbiExportError, "keyword:%s has unsupported type:%d",
                                 keyword, p_info->type);  // internal error
                    break;
                }
            }

            if (export_ok) {
                RETVAL = Py_None;
            }
            else {
                PyErr_SetString(ZvbiExportError, vbi_export_errstr(self->ctx));
            }
        }
        else {
            PyErr_Format(ZvbiExportError, "unsupported keyword: %s", keyword);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_option_get(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    char * keyword = NULL;

    if (PyArg_ParseTuple(args, "s", &keyword)) {
        vbi_option_value opt_val;
        vbi_option_info * p_info = vbi_export_option_info_keyword(self->ctx, keyword);
        if (p_info != NULL) {
            if (vbi_export_option_get(self->ctx, keyword, &opt_val)) {
                switch (p_info->type) {
                    case VBI_OPTION_BOOL:
                    case VBI_OPTION_INT:
                    case VBI_OPTION_MENU:
                        RETVAL = PyLong_FromLong(opt_val.num);
                        break;
                    case VBI_OPTION_REAL:
                        RETVAL = PyFloat_FromDouble(opt_val.dbl);
                        break;
                    case VBI_OPTION_STRING:
                        RETVAL = PyUnicode_FromString(opt_val.str);
                        free(opt_val.str);
                        break;
                    default:
                        PyErr_Format(ZvbiExportError, "keyword:%s has unsupported type:%d",
                                     keyword, p_info->type);  // internal error
                        break;
                }
            }
            else {
                PyErr_SetString(ZvbiExportError, vbi_export_errstr(self->ctx));
            }
        }
        else {
            PyErr_Format(ZvbiExportError, "unsupported keyword: %s", keyword);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_option_menu_set(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    char * keyword = NULL;
    int entry = 0;

    if (PyArg_ParseTuple(args, "si", &keyword, &entry)) {
        if (vbi_export_option_menu_set(self->ctx, keyword, entry)) {
            RETVAL = Py_None;
        }
        else {
            PyErr_SetString(ZvbiExportError, vbi_export_errstr(self->ctx));
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_option_menu_get(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    char * keyword = NULL;

    if (PyArg_ParseTuple(args, "s", &keyword)) {
        int entry = 0;
        if (vbi_export_option_menu_get(self->ctx, keyword, &entry)) {
            RETVAL = PyLong_FromLong(entry);
        }
        else {
            PyErr_SetString(ZvbiExportError, vbi_export_errstr(self->ctx));
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_to_stdio(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    PyObject * pg_obj = NULL;
    int fd;

    if (PyArg_ParseTuple(args, "O!i", &ZvbiPageTypeDef, &pg_obj, &fd)) {
        FILE * fp = fdopen(fd, "w");
        if (fp != NULL) {
            vbi_page * page = ZvbiPage_GetPageBuf(pg_obj);
            if (vbi_export_stdio(self->ctx, fp, page)) {
                RETVAL = Py_None;
            }
            else {
                PyErr_SetString(ZvbiExportError, vbi_export_errstr(self->ctx));
            }
        }
        else {
            PyErr_Format(ZvbiExportError, "failed to initialize stream: %s", strerror(errno));
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_to_file(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    PyObject * pg_obj = NULL;
    char * file_name = NULL;

    if (PyArg_ParseTuple(args, "O!s", &ZvbiPageTypeDef, &pg_obj, &file_name)) {
        vbi_page * page = ZvbiPage_GetPageBuf(pg_obj);
        if (vbi_export_file(self->ctx, file_name, page)) {
            RETVAL = Py_None;
        }
        else {
            PyErr_SetString(ZvbiExportError, vbi_export_errstr(self->ctx));
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiExport_to_memory(ZvbiExportObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;
    PyObject * pg_obj = NULL;

    if (PyArg_ParseTuple(args, "O!", &ZvbiPageTypeDef, &pg_obj)) {
        vbi_page * page = ZvbiPage_GetPageBuf(pg_obj);
        char * p_buf;
        size_t buf_size;
        if (vbi_export_alloc(self->ctx, (void**)&p_buf, &buf_size, page)) {
            RETVAL = PyBytes_FromStringAndSize(p_buf, buf_size);;
            free(p_buf);
        }
        else {
            PyErr_SetString(ZvbiExportError, vbi_export_errstr(self->ctx));
        }
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiExport_MethodsDef[] =
{
    {"info_enum",           (PyCFunction) ZvbiExport_info_enum,           METH_NOARGS | METH_STATIC, NULL },
    {"info_keyword",        (PyCFunction) ZvbiExport_info_keyword,        METH_NOARGS | METH_STATIC, NULL },

    {"info_export",         (PyCFunction) ZvbiExport_info_export,         METH_VARARGS, NULL },
    {"option_info_enum",    (PyCFunction) ZvbiExport_option_info_enum,    METH_VARARGS, NULL },
    {"option_info_keyword", (PyCFunction) ZvbiExport_option_info_keyword, METH_VARARGS, NULL },

    {"option_set",          (PyCFunction) ZvbiExport_option_set,          METH_VARARGS, NULL },
    {"option_get",          (PyCFunction) ZvbiExport_option_get,          METH_VARARGS, NULL },
    {"option_menu_set",     (PyCFunction) ZvbiExport_option_menu_set,     METH_VARARGS, NULL },
    {"option_menu_get",     (PyCFunction) ZvbiExport_option_menu_get,     METH_VARARGS, NULL },

    {"to_stdio",            (PyCFunction) ZvbiExport_to_stdio,            METH_VARARGS, NULL },
    {"to_file",             (PyCFunction) ZvbiExport_to_file,             METH_VARARGS, NULL },
    {"to_memory",           (PyCFunction) ZvbiExport_to_memory,           METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

static PyTypeObject ZvbiExportTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.Export",
    .tp_doc = PyDoc_STR("Class for exporting teletext pages in various formats"),
    .tp_basicsize = sizeof(ZvbiExportObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ZvbiExport_new,
    .tp_init = (initproc) ZvbiExport_init,
    .tp_dealloc = (destructor) ZvbiExport_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiExport_Repr,
    .tp_methods = ZvbiExport_MethodsDef,
    //.tp_members = ZvbiExport_Members,
};

int PyInit_Export(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiExportTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiExportError = PyErr_NewException("Zvbi.ExportError", error_base, NULL);
    Py_XINCREF(ZvbiExportError);
    if (PyModule_AddObject(module, "ExportError", ZvbiExportError) < 0) {
        Py_XDECREF(ZvbiExportError);
        Py_CLEAR(ZvbiExportError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiExportTypeDef);
    if (PyModule_AddObject(module, "Export", (PyObject *) &ZvbiExportTypeDef) < 0) {
        Py_DECREF(&ZvbiExportTypeDef);
        Py_XDECREF(ZvbiExportError);
        Py_CLEAR(ZvbiExportError);
        return -1;
    }

    return 0;
}
