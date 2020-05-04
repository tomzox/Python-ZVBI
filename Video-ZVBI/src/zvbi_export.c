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

#include "zvbi_export.h"

// ---------------------------------------------------------------------------
//  Teletext Page Export
// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_export * ctx;
} ZvbiExportObj;

PyObject * ZvbiExportError;

// ---------------------------------------------------------------------------

#if 0

static HV *
zvbi_xs_export_info_to_hv( vbi_export_info * p_info )
{
        HV * hv = newHV();

        hv_store_pv(hv, keyword, p_info->keyword);
        hv_store_pv(hv, label, p_info->label);
        hv_store_pv(hv, tooltip, p_info->tooltip);
        hv_store_pv(hv, mime_type, p_info->mime_type);
        hv_store_pv(hv, extension, p_info->extension);

        return hv;
}

static HV *
zvbi_xs_export_option_info_to_hv( vbi_option_info * p_opt )
{
        HV * hv = newHV();
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

        return hv;
}

#endif // 0

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

#if 0
void
vbi_export_info_enum(index)
        int index
        PREINIT:
        vbi_export_info * p_info;
        PPCODE:
        p_info = vbi_export_info_enum(index);
        if (p_info != NULL) {
                HV * hv = zvbi_xs_export_info_to_hv(p_info);
                EXTEND(sp,1);
                PUSHs (sv_2mortal (newRV_noinc ((SV*)hv)));
        }

void
vbi_export_info_keyword(keyword)
        const char * keyword
        PREINIT:
        vbi_export_info * p_info;
        PPCODE:
        p_info = vbi_export_info_keyword(keyword);
        if (p_info != NULL) {
                HV * hv = zvbi_xs_export_info_to_hv(p_info);
                EXTEND(sp,1);
                PUSHs (sv_2mortal (newRV_noinc ((SV*)hv)));
        }

void
vbi_export_info_export(exp)
        VbiExportObj * exp
        PREINIT:
        vbi_export_info * p_info;
        PPCODE:
        p_info = vbi_export_info_export(exp);
        if (p_info != NULL) {
                HV * hv = zvbi_xs_export_info_to_hv(p_info);
                EXTEND(sp,1);
                PUSHs (sv_2mortal (newRV_noinc ((SV*)hv)));
        }

void
vbi_export_option_info_enum(exp, index)
        VbiExportObj * exp
        int index
        PREINIT:
        vbi_option_info * p_opt;
        PPCODE:
        p_opt = vbi_export_option_info_enum(exp, index);
        if (p_opt != NULL) {
                HV * hv = zvbi_xs_export_option_info_to_hv(p_opt);
                EXTEND(sp, 1);
                PUSHs (sv_2mortal (newRV_noinc ((SV*)hv)));
        }

void
vbi_export_option_info_keyword(exp, keyword)
        VbiExportObj * exp
        const char *keyword
        PREINIT:
        vbi_option_info * p_opt;
        PPCODE:
        p_opt = vbi_export_option_info_keyword(exp, keyword);
        if (p_opt != NULL) {
                HV * hv = zvbi_xs_export_option_info_to_hv(p_opt);
                EXTEND(sp, 1);
                PUSHs (sv_2mortal (newRV_noinc ((SV*)hv)));
        }

vbi_bool
vbi_export_option_set(exp, keyword, sv)
        VbiExportObj * exp
        const char * keyword
        SV * sv
        PREINIT:
        vbi_option_info * p_info;
        CODE:
        RETVAL = 0;
        p_info = vbi_export_option_info_keyword(exp, keyword);
        if (p_info != NULL) {
                switch (p_info->type) {
                case VBI_OPTION_BOOL:
                case VBI_OPTION_INT:
                case VBI_OPTION_MENU:
                        RETVAL = vbi_export_option_set(exp, keyword, SvIV(sv));
                        break;
                case VBI_OPTION_REAL:
                        RETVAL = vbi_export_option_set(exp, keyword, SvNV(sv));
                        break;
                case VBI_OPTION_STRING:
                        RETVAL = vbi_export_option_set(exp, keyword, SvPV_nolen(sv));
                        break;
                default:
                        break;
                }
        }
        OUTPUT:
        RETVAL

void
vbi_export_option_get(exp, keyword)
        VbiExportObj * exp
        const char * keyword
        PREINIT:
        vbi_option_value opt_val;
        vbi_option_info * p_info;
        PPCODE:
        p_info = vbi_export_option_info_keyword(exp, keyword);
        if (p_info != NULL) {
                if (vbi_export_option_get(exp, keyword, &opt_val)) {
                        switch (p_info->type) {
                        case VBI_OPTION_BOOL:
                        case VBI_OPTION_INT:
                        case VBI_OPTION_MENU:
                                EXTEND(sp, 1);
                                PUSHs (sv_2mortal (newSViv (opt_val.num)));
                                break;
                        case VBI_OPTION_REAL:
                                EXTEND(sp, 1);
                                PUSHs (sv_2mortal (newSVnv (opt_val.dbl)));
                                break;
                        case VBI_OPTION_STRING:
                                EXTEND(sp, 1);
                                PUSHs (sv_2mortal (newSVpv (opt_val.str, 0)));
                                free(opt_val.str);
                                break;
                        default:
                                break;
                        }
                }
        }

vbi_bool
vbi_export_option_menu_set(exp, keyword, entry)
        VbiExportObj * exp
        const char * keyword
        int entry

void
vbi_export_option_menu_get(exp, keyword)
        VbiExportObj * exp
        const char * keyword
        PREINIT:
        int entry;
        PPCODE:
        if (vbi_export_option_menu_get(exp, keyword, &entry)) {
                EXTEND(sp, 1);
                PUSHs (sv_2mortal (newSViv (entry)));
        }

vbi_bool
vbi_export_stdio(exp, fp, pg_obj)
        VbiExportObj * exp
        FILE * fp
        VbiPageObj * pg_obj
        CODE:
        RETVAL = vbi_export_stdio(exp, fp, pg_obj->p_pg);
        OUTPUT:
        RETVAL

vbi_bool
vbi_export_file(exp, name, pg_obj)
        VbiExportObj * exp
        const char * name
        VbiPageObj * pg_obj
        CODE:
        RETVAL = vbi_export_file(exp, name, pg_obj->p_pg);
        OUTPUT:
        RETVAL

int
vbi_export_mem(exp, sv_buf, pg_obj)
        VbiExportObj * exp
        SV * sv_buf
        VbiPageObj * pg_obj
        PREINIT:
        char * p_buf;
        STRLEN buf_size;
        CODE:
        if (SvOK(sv_buf))  {
                p_buf = SvPV_force(sv_buf, buf_size);
                RETVAL = zvbi_(export_mem)(exp, p_buf, buf_size + 1, pg_obj->p_pg);
        } else {
                croak("Input buffer is undefined or not a scalar");
                RETVAL = FALSE;
        }
        OUTPUT:
        sv_buf
        RETVAL

void
vbi_export_alloc(exp, pg_obj)
        VbiExportObj * exp
        VbiPageObj * pg_obj
        PREINIT:
        char * p_buf;
        size_t buf_size;
        SV * sv;
        PPCODE:
        if (zvbi_(export_alloc)(exp, (void**)&p_buf, &buf_size, pg_obj->p_pg)) {
                sv = newSV(0);
                sv_usepvn(sv, p_buf, buf_size);
                /* now the pointer is managed by perl -> no free() */
                EXTEND(sp, 1);
                PUSHs (sv_2mortal (sv));
        }

char *
vbi_export_errstr(exp)
        VbiExportObj * exp

#endif // 0

// ---------------------------------------------------------------------------

static PyMethodDef ZvbiExport_MethodsDef[] =
{
    //{"decode",           (PyCFunction) ZvbiExport_decode,           METH_VARARGS, NULL },

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
