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

#include "zvbi_page.h"
#include "zvbi_event_types.h"

// ---------------------------------------------------------------------------
//  Rendering
// ---------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    vbi_page *      page;
    vbi_bool        do_free_pg;
    const int     * p_validity_src;
    int             validity_id;
} ZvbiPageObj;

static PyObject * ZvbiPageError;

/*
 * Constants for the "draw" functions
 */
#define DRAW_TTX_CELL_WIDTH     12
#define DRAW_TTX_CELL_HEIGHT    10
#define DRAW_CC_CELL_WIDTH      16
#define DRAW_CC_CELL_HEIGHT     26
#define GET_CANVAS_TYPE(FMT)    (((FMT)==VBI_PIXFMT_PAL8) ? sizeof(uint8_t) : sizeof(vbi_rgba))

#if !defined(UTF8_MAXBYTES)
#define UTF8_MAXBYTES 4         /* max length of an UTF-8 encoded Unicode character */
#endif

// ---------------------------------------------------------------------------

PyObject *
zvbi_xs_convert_rgba_to_ppm( ZvbiPageObj * self, const vbi_rgba * p_img,
                             int pix_width, int pix_height, int scale )
{
    switch (scale) {
        case 0: pix_height /= 2; break;
        case 2: pix_height *= 2; break;
        default: break;
    }

    Py_ssize_t img_max_len = 200 + pix_height * pix_width * 3;
    PyObject * img_obj = PyBytes_FromStringAndSize(NULL, img_max_len);
    if (img_obj != NULL) {
        char * p_img_data = PyBytes_AS_STRING(img_obj);
        Py_ssize_t img_off = 0;

        /*
         * Write the image header (including image dimensions)
         */
        img_off += snprintf(p_img_data + img_off, img_max_len - img_off,
                            "P6\n"
                            "%d %d\n"
                            "255\n",
                            pix_width, pix_height);

        /*
         * Write the image data (raw, 3 bytes RGB per pixel)
         */
        for (unsigned row = 0; row < pix_height; row++) {
            for (unsigned col = 0; col < pix_width; col++) {
                if (img_off + 2 < img_max_len) {
                    uint32_t bgr = *(p_img++);
                    p_img_data[img_off++] = bgr & 0xFF;
                    p_img_data[img_off++] = (bgr >>  8) & 0xFF;
                    p_img_data[img_off++] = (bgr >> 16) & 0xFF;
                }
            }

            if (scale == 0) {
                p_img += pix_width;
            }
            else if ((scale == 2) && ((row & 1) == 0)) {
                p_img -= pix_width;
            }
        }
        assert(img_off <= img_max_len);
        _PyBytes_Resize(&img_obj, img_off);
    }
    return img_obj;
}

PyObject *
zvbi_xs_convert_pal8_to_ppm( ZvbiPageObj * self, const uint8_t * p_img,
                             int pix_width, int pix_height, int scale )
{
    switch (scale) {
        case 0: pix_height /= 2; break;
        case 2: pix_height *= 2; break;
        default: break;
    }

    Py_ssize_t img_max_len = 200 + pix_height * pix_width * 3;
    PyObject * img_obj = PyBytes_FromStringAndSize(NULL, img_max_len);
    if (img_obj != NULL) {
        char * p_img_data = PyBytes_AS_STRING(img_obj);
        Py_ssize_t img_off = 0;

        /*
         * Write the image header (including image dimensions)
         */
        img_off += snprintf(p_img_data + img_off, img_max_len - img_off,
                            "P6\n"
                            "%d %d\n"
                            "255\n",
                            pix_width, pix_height);

        /*
         * Write the image data (raw, 3 bytes RGB per pixel)
         */
        for (unsigned row = 0; row < pix_height; row++) {
            for (unsigned col = 0; col < pix_width; col++) {
                uint8_t col_idx = *(p_img++);
                uint32_t bgr = ((col_idx < 40)? self->page->color_map[col_idx] : 0);
                p_img_data[img_off++] = bgr & 0xFF;
                p_img_data[img_off++] = (bgr >> 8) & 0xFF;
                p_img_data[img_off++] = (bgr >> 16) & 0xFF;
            }

            if (scale == 0) {
                p_img += pix_width;
            }
            else if ((scale == 2) && ((row & 1) == 0)) {
                p_img -= pix_width;
            }
        }
        assert(img_off <= img_max_len);
        _PyBytes_Resize(&img_obj, img_off);
    }
    return img_obj;
}

// deprecated, as implementation is slow
PyObject *
zvbi_xs_convert_rgba_to_xpm( ZvbiPageObj * pg_obj, const vbi_rgba * p_img,
                             int pix_width, int pix_height, int scale )
{
    /*
     * Determine the color palette
     */
    PyObject * col_map = PyDict_New();
    unsigned col_idx = 0;
    for (unsigned idx = 0; idx < pix_width * pix_height; idx++) {
        PyObject * key = PyLong_FromLong(p_img[idx] & 0xFFFFFF);
        if (PyDict_Contains(col_map, key) == 0) {
            PyDict_SetItem(col_map, key, PyLong_FromLong(col_idx));
            col_idx += 1;
        }
        else {
            Py_DECREF(key);
        }
    }

    switch (scale) {
        case 0: pix_height /= 2; break;
        case 2: pix_height *= 2; break;
        default: break;
    }

    /*
     * Write the image header (including image dimensions)
     */
    size_t img_max_len = 200 + col_idx * 15 + 13 + pix_height * (pix_width + 4) + 3;
    PyObject * img_obj = PyBytes_FromStringAndSize(NULL, img_max_len + 1);
    if (img_obj != NULL) {
        char * p_img_data = PyBytes_AS_STRING(img_obj);
        size_t img_off = 0;
        img_off += snprintf(p_img_data + img_off, img_max_len - img_off,
                            "/* XPM */\n"
                            "static char *image[] = {\n"
                            "/* width height ncolors chars_per_pixel */\n"
                            "\"%d %d %d %d\",\n"
                            "/* colors */\n",
                            pix_width, pix_height, col_idx, 1);

        /*
         * Write the color palette
         */
        Py_ssize_t map_iter = 0;
        PyObject * col_map_key;
        PyObject * col_map_val;
        while (PyDict_Next(col_map, &map_iter, &col_map_key, &col_map_val)) {
            int cval = PyLong_AsLong(col_map_key);
            int cidx = PyLong_AsLong(col_map_val);
            img_off += snprintf(p_img_data + img_off, img_max_len - img_off,
                                "\"%c c #%02X%02X%02X\",\n",
                                '0' + cidx,
                                cval & 0xFF,
                                (cval >> 8) & 0xFF,
                                (cval >> 16) & 0xFF);
        }

        /*
         * Write the image row by row
         */
        img_off += snprintf(p_img_data + img_off, img_max_len - img_off, "/* pixels */\n");
        for (unsigned row = 0; row < pix_height; row++) {
            img_off += snprintf(p_img_data + img_off, img_max_len - img_off, "\"");
            for (unsigned col = 0; col < pix_width; col++) {
                col_map_key = PyLong_FromLong(*(p_img++) & 0xFFFFFF);
                col_map_val = PyDict_GetItem(col_map, col_map_key);
                if (img_off < img_max_len) {
                    if (col_map_val != NULL) {
                        p_img_data[img_off++] = '0' + PyLong_AsLong(col_map_val);
                    }
                    else {
                        p_img_data[img_off++] = '0'; /* should never happen */
                    }
                }
            }
            img_off += snprintf(p_img_data + img_off, img_max_len - img_off, "\",\n");

            if (scale == 0) {
                p_img += pix_width;
            }
            else if ((scale == 2) && ((row & 1) == 0)) {
                p_img -= pix_width;
            }
        }
        img_off += snprintf(p_img_data + img_off, img_max_len - img_off, "};\n");

        assert(img_off <= img_max_len);
        _PyBytes_Resize(&img_obj, img_off);
    }
    Py_DECREF(col_map);

    return img_obj;
}

static PyObject *
zvbi_xs_convert_pal8_to_xpm( ZvbiPageObj * self, const uint8_t * p_img,
                             int pix_width, int pix_height, int scale )
{
    switch (scale) {
        case 0: pix_height /= 2; break;
        case 2: pix_height *= 2; break;
        default: break;
    }

    Py_ssize_t img_max_len = 400 + 40 * 15 + pix_height * (pix_width + 4);
    PyObject * img_obj = PyBytes_FromStringAndSize(NULL, img_max_len);
    if (img_obj != NULL) {
        char * p_img_data = PyBytes_AS_STRING(img_obj);
        Py_ssize_t img_off = 0;

        /*
         * Write the image header (including image dimensions)
         */
        img_off += snprintf(p_img_data + img_off, img_max_len - img_off,
                            "/* XPM */\n"
                            "static char *image[] = {\n"
                            "/* width height ncolors chars_per_pixel */\n"
                            "\"%d %d %d %d\",\n"
                            "/* colors */\n",
                            pix_width, pix_height, 40, 1);

        /*
         * Write the color palette (always the complete palette, including unused colors)
         */
        static const uint8_t col_codes[40] = " 1234567.BCDEFGHIJKLMNOPabcdefghijklmnop";
        for (unsigned idx = 0; idx < 40; idx++) {
            img_off += snprintf(p_img_data + img_off, img_max_len - img_off,
                                "\"%c c #%02X%02X%02X\",\n",
                                col_codes[idx],
                                self->page->color_map[idx] & 0xFF,
                                (self->page->color_map[idx] >> 8) & 0xFF,
                                (self->page->color_map[idx] >> 16) & 0xFF);
        }

        /*
         * Write the image row by row
         */
        img_off += snprintf(p_img_data + img_off, img_max_len - img_off, "/* pixels */\n");
        for (unsigned row = 0; row < pix_height; row++) {
            p_img_data[img_off++] = '"';
            for (unsigned col = 0; col < pix_width; col++) {
                uint8_t c = *(p_img++);
                if (c < 40) {
                    p_img_data[img_off++] = col_codes[c];
                }
                else {
                    p_img_data[img_off++] = ' ';
                }
            }
            p_img_data[img_off++] = '"';
            p_img_data[img_off++] = ',';
            p_img_data[img_off++] = '\n';

            if (scale == 0) {
                p_img += pix_width;
            }
            else if ((scale == 2) && ((row & 1) == 0)) {
                p_img -= pix_width;
            }
        }
        p_img_data[img_off++] = '}';
        p_img_data[img_off++] = ';';
        p_img_data[img_off++] = '\n';

        assert(img_off <= img_max_len);
        _PyBytes_Resize(&img_obj, img_off);
    }
    return img_obj;
}

// ---------------------------------------------------------------------------

PyObject *
ZvbiPage_New(vbi_page * page)
{
    ZvbiPageObj * self = (ZvbiPageObj *) ZvbiPageTypeDef.tp_alloc(&ZvbiPageTypeDef, 0);
    if (self != NULL) {
        self->page = page;
        self->do_free_pg = TRUE;
    }
    return (PyObject *) self;
}

PyObject *
ZvbiPage_NewTemporary(vbi_page * page, const int * validity_src)
{
    ZvbiPageObj * self = (ZvbiPageObj *) ZvbiPageTypeDef.tp_alloc(&ZvbiPageTypeDef, 0);
    if (self != NULL) {
        self->page = page;
        self->do_free_pg = FALSE;
        self->p_validity_src = validity_src;
        self->validity_id = *validity_src;
    }
    return (PyObject *) self;
}

static void
ZvbiPage_dealloc(ZvbiPageObj *self)
{
    if (self->page && self->do_free_pg) {
        vbi_unref_page(self->page);
        PyMem_RawFree(self->page);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static int
ZvbiPage_CheckValid(ZvbiPageObj * self)
{
    if ((self->page == NULL) ||
        ((self->p_validity_src != NULL) && (*self->p_validity_src != self->validity_id)))
    {
        PyErr_SetString(PyExc_ValueError, "Page content is no longer valid");
        return FALSE;
    }
    return TRUE;
}

vbi_page *
ZvbiPage_GetPageBuf(PyObject * obj)
{
    assert(PyObject_IsInstance(obj, (PyObject*)&ZvbiPageTypeDef) == 1);
    ZvbiPageObj * self = (ZvbiPageObj*) obj;

    if (ZvbiPage_CheckValid(self) == FALSE) {
        PyErr_SetString(PyExc_ValueError, "Page content is no longer valid");
        return NULL;
    }
    return self->page;
}

// ---------------------------------------------------------------------------

#if 0  /* obsolete simplified interface, equivalent to below with default options */
static PyObject *
ZvbiPage_draw_vt_page(ZvbiPageObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"fmt", "reveal", "flash_on", NULL};
    PyObject * RETVAL = NULL;
    int fmt = VBI_PIXFMT_RGBA32_LE;  // vbi_pixfmt
    int reveal = FALSE;
    int flash_on = FALSE;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|i$pp", kwlist, &fmt, &reveal, &flash_on)) {
        int canvas_type = GET_CANVAS_TYPE(fmt);  /* prior to 0.2.26 only RGBA is supported */
        int rowstride = self->page->columns * DRAW_TTX_CELL_WIDTH * canvas_type;
        int canvas_size = rowstride * self->page->rows * DRAW_TTX_CELL_HEIGHT;

        RETVAL = PyBytes_FromStringAndSize(NULL, canvas_size);  // alloc uninitialized buffer
        char * p_buf = PyBytes_AsString(RETVAL);
        memset(p_buf, 0, canvas_size);

        vbi_draw_vt_page_region(self->page, fmt, p_buf, rowstride,
                                0, 0, self->page->columns, self->page->rows,
                                reveal, flash_on);
    }
    return RETVAL;
}
#else
static PyObject *
ZvbiPage_draw_vt_page(ZvbiPageObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"column", "row", "width", "height",
                              "img_pix_width", "col_pix_off", "row_pix_off",
                              "fmt", "reveal", "flash_on", NULL};
    int column = 0;
    int row = 0;
    int width = 0;
    int height = 0;
    int img_pix_width = 0;
    int col_pix_off = 0;
    int row_pix_off = 0;
    int fmt = VBI_PIXFMT_RGBA32_LE;  // vbi_pixfmt
    int reveal = FALSE;
    int flash_on = FALSE;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|iiii$iiiipp", kwlist,
                                    &column, &row, &width, &height,
                                    &img_pix_width, &col_pix_off, &row_pix_off,
                                    &fmt, &reveal, &flash_on))
    {
        if (ZvbiPage_CheckValid(self)) {
            if ((width == 0) && (height == 0) && (column == 0) && (row == 0)) {
                width = self->page->columns;
                height = self->page->rows;
            }
            if (img_pix_width <= 0) {
                img_pix_width = width * DRAW_TTX_CELL_WIDTH;
            }
            if ((width > 0) && (height > 0) &&
                (column + width <= self->page->columns) &&
                (row + height <= self->page->rows) &&
                (col_pix_off >= 0) && (row_pix_off >= 0) &&
                (img_pix_width >= (col_pix_off + (width * DRAW_TTX_CELL_WIDTH))))
            {
                int canvas_type = GET_CANVAS_TYPE(fmt);  /* prior to 0.2.26 only RGBA is supported */
                int rowstride = img_pix_width * canvas_type;
                int canvas_size = rowstride * (row_pix_off + height * DRAW_TTX_CELL_HEIGHT);

                RETVAL = PyBytes_FromStringAndSize(NULL, canvas_size);  // alloc uninitialized buffer
                char * p_buf = PyBytes_AsString(RETVAL);
                memset(p_buf, 0, canvas_size);  // needed in case col_pix_off|row_pix_off > 0

                vbi_draw_vt_page_region(self->page, fmt,
                                        p_buf + (row_pix_off * rowstride) + col_pix_off,
                                        rowstride, column, row, width, height, reveal, flash_on);
            }
            else {
                if ((width == 0) || (height == 0)) {
                    PyErr_SetString(ZvbiPageError, "width and height need to be > 0");
                }
                else if ((column + width > self->page->columns) ||
                         (row + height > self->page->rows)) {
                    PyErr_Format(ZvbiPageError, "invalid col %d + width %d or row %d + height %d for page geometry %dx%d",
                                 column, width, row, height, self->page->columns, self->page->rows);
                }
                else {
                    PyErr_Format(ZvbiPageError, "invalid image pixel width %d for page/region width %d char * %d pixel",
                                 img_pix_width, width, DRAW_TTX_CELL_WIDTH);
                }
            }
        }
    }
    return RETVAL;
}
#endif

static PyObject *
ZvbiPage_draw_cc_page(ZvbiPageObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"column", "row", "width", "height",
                              "img_pix_width", "col_pix_off", "row_pix_off",
                              "fmt", NULL};
    int column = 0;
    int row = 0;
    int width = 0;
    int height = 0;
    int img_pix_width = 0;
    int col_pix_off = 0;
    int row_pix_off = 0;
    int fmt = VBI_PIXFMT_RGBA32_LE;  // vbi_pixfmt
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|iiii$iiii", kwlist,
                                    &column, &row, &width, &height,
                                    &img_pix_width, &col_pix_off, &row_pix_off,
                                    &fmt))
    {
        if (ZvbiPage_CheckValid(self)) {
            if ((width == 0) && (height == 0) && (column == 0) && (row == 0)) {
                width = self->page->columns;
                height = self->page->rows;
            }
            if (img_pix_width <= 0) {
                img_pix_width = self->page->columns * DRAW_CC_CELL_WIDTH;
            }
            if ((width > 0) && (height > 0) &&
                (column + width <= self->page->columns) &&
                (row + height <= self->page->rows) &&
                (col_pix_off >= 0) && (row_pix_off >= 0) &&
                (img_pix_width >= (col_pix_off + (width * DRAW_CC_CELL_WIDTH))))
            {
                int canvas_type = GET_CANVAS_TYPE(fmt);  /* prior to 0.2.26 only RGBA is supported */
                int rowstride = img_pix_width * canvas_type;
                int canvas_size = rowstride * (row_pix_off + height * DRAW_CC_CELL_HEIGHT);

                RETVAL = PyBytes_FromStringAndSize(NULL, canvas_size);  // alloc uninitialized buffer
                char * p_buf = PyBytes_AsString(RETVAL);
                memset(p_buf, 0, canvas_size);  // needed in case col_pix_off|row_pix_off > 0

                vbi_draw_cc_page_region(self->page, fmt,
                                        p_buf + (row_pix_off * rowstride) + col_pix_off,
                                        rowstride, column, row, width, height);
            }
            else {
                if ((width == 0) || (height == 0)) {
                    PyErr_SetString(ZvbiPageError, "width and height need to be > 0");
                }
                else if ((column + width > self->page->columns) ||
                         (row + height > self->page->rows)) {
                    PyErr_Format(ZvbiPageError, "invalid col %d + width %d or row %d + height %d for page geometry %dx%d",
                                 column, width, row, height, self->page->columns, self->page->rows);
                }
                else {
                    PyErr_Format(ZvbiPageError, "invalid image pixel width %d for page/region width %d char * %d pixel",
                                 img_pix_width, width, DRAW_CC_CELL_WIDTH);
                }
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_canvas_to_ppm(ZvbiPageObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"canvas", "fmt", "aspect", "img_pix_width", NULL};
    PyObject * in_obj = NULL;
    int fmt = VBI_PIXFMT_RGBA32_LE;  // vbi_pixfmt
    int aspect = FALSE;
    int img_pix_width = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O!|i$pi", kwlist,
                                    &PyBytes_Type, &in_obj, &fmt, &aspect, &img_pix_width))
    {
        if (ZvbiPage_CheckValid(self)) {
            char * p_img = NULL;
            Py_ssize_t buf_size = 0;

            if (PyBytes_AsStringAndSize(in_obj, &p_img, &buf_size) == 0) {
                int canvas_type;
                int img_pix_height;
                int scale;

                if (img_pix_width <= 0) {
                    if (self->page->pgno <= 8) {
                        img_pix_width = self->page->columns * DRAW_CC_CELL_WIDTH;
                    }
                    else {
                        img_pix_width = self->page->columns * DRAW_TTX_CELL_WIDTH;
                    }
                }
                if (self->page->pgno <= 8) {
                    scale = aspect ? 1 : 0;  /* CC: is already line-doubled */
                }
                else {
                    scale = aspect ? 2 : 1;  /* TTX: correct aspect ratio by doubling lines in Y dimension */
                }
                canvas_type = GET_CANVAS_TYPE(fmt);  /* prior to 0.2.26 only RGBA is supported */
                if (buf_size % (img_pix_width * canvas_type) == 0) {
                    img_pix_height = buf_size / (img_pix_width * canvas_type);
                    if (fmt == VBI_PIXFMT_RGBA32_LE) {
                        RETVAL = zvbi_xs_convert_rgba_to_ppm(self, (void*)p_img, img_pix_width, img_pix_height, scale);
                    }
                    else {
                        RETVAL = zvbi_xs_convert_pal8_to_ppm(self, (void*)p_img, img_pix_width, img_pix_height, scale);
                    }
                }
                else {
                    PyErr_Format(ZvbiPageError, "Input buffer size %d doesn't match img_pix_width %d (pixel size %d)",
                                 (int)buf_size, img_pix_width, canvas_type);
                }
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_canvas_to_xpm(ZvbiPageObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"canvas", "fmt", "aspect", "img_pix_width", NULL};
    PyObject * in_obj = NULL;
    int fmt = VBI_PIXFMT_RGBA32_LE;  // vbi_pixfmt
    int aspect = FALSE;
    int img_pix_width = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O!|i$pi", kwlist,
                                    &PyBytes_Type, &in_obj, &fmt, &aspect, &img_pix_width))
    {
        if (ZvbiPage_CheckValid(self)) {
            char * p_img = NULL;
            Py_ssize_t buf_size = 0;

            if (PyBytes_AsStringAndSize(in_obj, &p_img, &buf_size) == 0) {
                int canvas_type;
                int img_pix_height;
                int scale;

                if (img_pix_width <= 0) {
                    if (self->page->pgno <= 8) {
                        img_pix_width = self->page->columns * DRAW_CC_CELL_WIDTH;
                    }
                    else {
                        img_pix_width = self->page->columns * DRAW_TTX_CELL_WIDTH;
                    }
                }
                if (self->page->pgno <= 8) {
                    scale = aspect ? 1 : 0;  /* CC: is already line-doubled */
                }
                else {
                    scale = aspect ? 2 : 1;  /* TTX: correct aspect ratio by doubling lines in Y dimension */
                }
                canvas_type = GET_CANVAS_TYPE(fmt);  /* prior to 0.2.26 only RGBA is supported */
                if (buf_size % (img_pix_width * canvas_type) == 0) {
                    img_pix_height = buf_size / (img_pix_width * canvas_type);
                    if (fmt == VBI_PIXFMT_RGBA32_LE) {
                        RETVAL = zvbi_xs_convert_rgba_to_xpm(self, (void*)p_img, img_pix_width, img_pix_height, scale);
                    }
                    else {
                        RETVAL = zvbi_xs_convert_pal8_to_xpm(self, (void*)p_img, img_pix_width, img_pix_height, scale);
                    }
                }
                else {
                    PyErr_Format(ZvbiPageError, "Input buffer size %d doesn't match img_pix_width %d (pixel size %d)",
                                 (int)buf_size, img_pix_width, canvas_type);
                }
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_print_page(ZvbiPageObj *self, PyObject *args, PyObject *kwds)
{
    static char * kwlist[] = {"column", "row", "width", "height",
                              "fmt", "table", NULL};
    int column = 0;
    int row = 0;
    int width = 0;
    int height = 0;
    const char * fmt = "UTF-8";
    int table = TRUE;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|iiii$sp", kwlist,
                                    &column, &row, &width, &height,
                                    &fmt, &table))
    {
        if (ZvbiPage_CheckValid(self)) {
            if ((width == 0) && (height == 0) && (column == 0) && (row == 0)) {
                width = self->page->columns;
                height = self->page->rows;
            }
            if ((width > 0) && (height > 0) &&
                (column + width <= self->page->columns) &&
                (row + height <= self->page->rows))
            {
                const int max_size = (self->page->columns + 1) * self->page->rows * UTF8_MAXBYTES;
                RETVAL = PyBytes_FromStringAndSize(NULL, max_size + 1);  // alloc uninitialized buffer
                char * p_buf = PyBytes_AsString(RETVAL);

                int len = vbi_print_page_region(self->page, p_buf, max_size,
                                                fmt, table, 0 /*rtl, unused*/,
                                                column, row, width, height);
                if ((len > 0) && (len <= max_size)) {
                    p_buf[len] = 0;
                    _PyBytes_Resize(&RETVAL, len);
                }
                else {
                    PyErr_SetString(ZvbiPageError, "conversion failed");
                    Py_DECREF(RETVAL);
                    RETVAL = NULL;
                }
            }
            else {
                if ((width == 0) || (height == 0)) {
                    PyErr_SetString(ZvbiPageError, "width and height need to be > 0");
                }
                else {
                    PyErr_Format(ZvbiPageError, "invalid col %d + width %d or row %d + height %d for page geometry %dx%d",
                                 column, width, row, height, self->page->columns, self->page->rows);
                }
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_get_page_no(ZvbiPageObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    if (ZvbiPage_CheckValid(self)) {
        RETVAL = PyTuple_New(2);
        if (RETVAL != NULL) {
            PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(self->page->pgno));
            PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(self->page->subno));
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_get_page_size(ZvbiPageObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    if (ZvbiPage_CheckValid(self)) {
        RETVAL = PyTuple_New(2);
        if (RETVAL != NULL) {
            PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(self->page->rows));
            PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(self->page->columns));
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_get_page_dirty_range(ZvbiPageObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    if (ZvbiPage_CheckValid(self)) {
        RETVAL = PyTuple_New(3);
        if (RETVAL != NULL) {
            PyTuple_SetItem(RETVAL, 0, PyLong_FromLong(self->page->dirty.y0));
            PyTuple_SetItem(RETVAL, 1, PyLong_FromLong(self->page->dirty.y1));
            PyTuple_SetItem(RETVAL, 2, PyLong_FromLong(self->page->dirty.roll));
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_get_page_color_map(ZvbiPageObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    if (ZvbiPage_CheckValid(self)) {
        RETVAL = PyTuple_New(40);
        if (RETVAL != NULL) {
            for (unsigned idx = 0; idx < 40; idx++) {
                PyTuple_SetItem(RETVAL, idx, PyLong_FromLong(self->page->color_map[idx]));
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_get_page_text_properties(ZvbiPageObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    if (ZvbiPage_CheckValid(self)) {
        unsigned size = self->page->rows * self->page->columns;

        RETVAL = PyTuple_New(size);
        if (RETVAL != NULL) {
            for (unsigned idx = 0; idx < size; idx++) {
                vbi_char * p = &self->page->text[idx];
                uint32_t val = (p->foreground << 0) |
                               (p->background << 8) |
                               ((p->opacity & 0x0F) << 16) |
                               ((p->size & 0x0F) << 20) |
                               (p->underline << 24) |
                               (p->bold << 25) |
                               (p->italic << 26) |
                               (p->flash << 27) |
                               (p->conceal << 28) |
                               (p->proportional << 29) |
                               (p->link << 30);
                PyTuple_SetItem(RETVAL, idx, PyLong_FromLong(val));
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_get_page_text(ZvbiPageObj *self, PyObject *args)
{
    int repl_chr = 0;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "|C", &repl_chr)) {
        if (ZvbiPage_CheckValid(self)) {
            unsigned size = self->page->rows * self->page->columns;
            RETVAL = PyUnicode_New(size, 65535);  // source is UCS-2
            if (RETVAL != NULL) {
                int kind = PyUnicode_KIND(RETVAL);
                void * outp = PyUnicode_DATA(RETVAL);
                vbi_char * txt = self->page->text;
                unsigned idx = 0;

                for (unsigned row = 0; row < self->page->rows; row++) {
                    for (unsigned column = 0; column < self->page->columns; column++) {
                        /* replace "private use" charaters with space */
                        unsigned ucs = (txt++)->unicode;
                        if ((ucs > 0xE000) && (ucs <= 0xF8FF) && repl_chr) {
                            ucs = repl_chr;
                        }
                        PyUnicode_WRITE(kind, outp, idx++, ucs);
                    }
                }
            }
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_resolve_link(ZvbiPageObj *self, PyObject *args)
{
    unsigned column;
    unsigned row;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "II", &column, &row)) {
        if (ZvbiPage_CheckValid(self)) {
            vbi_link ld;
            memset(&ld, 0, sizeof(ld));
            vbi_resolve_link(self->page, column, row, &ld);
            RETVAL = ZvbiEvent_ObjFromPageLink(&ld);
        }
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_resolve_home(ZvbiPageObj *self, PyObject *args)
{
    PyObject * RETVAL = NULL;

    if (ZvbiPage_CheckValid(self)) {
        vbi_link ld;
        memset(&ld, 0, sizeof(ld));
        vbi_resolve_home(self->page, &ld);

        RETVAL = ZvbiEvent_ObjFromPageLink(&ld);
    }
    return RETVAL;
}

static PyObject *
ZvbiPage_DunderEnter(ZvbiPageObj *self, PyObject *args)
{
    Py_INCREF(self);
    return (PyObject*) self;
}

static PyObject *
ZvbiPage_DunderExit(ZvbiPageObj *self, PyObject *args)
{
    PyObject * exc_type = NULL;
    PyObject * exc_val = NULL;
    PyObject * exc_tb = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "OOO", &exc_type, &exc_val, &exc_tb)) {
        if (self->page != NULL) {
            if (self->do_free_pg) {
                vbi_unref_page(self->page);
                PyMem_RawFree(self->page);
            }
            self->page = NULL;
        }
        Py_INCREF(Py_None);
        RETVAL = Py_None;
    }
    return RETVAL;
}


// ---------------------------------------------------------------------------

static PyMethodDef ZvbiPage_MethodsDef[] =
{
    {"draw_vt_page",      (PyCFunction) ZvbiPage_draw_vt_page,       METH_VARARGS | METH_KEYWORDS, NULL },
    {"draw_cc_page",      (PyCFunction) ZvbiPage_draw_cc_page,       METH_VARARGS | METH_KEYWORDS, NULL },
    {"canvas_to_ppm",     (PyCFunction) ZvbiPage_canvas_to_ppm,      METH_VARARGS | METH_KEYWORDS, NULL },
    {"canvas_to_xpm",     (PyCFunction) ZvbiPage_canvas_to_xpm,      METH_VARARGS | METH_KEYWORDS, NULL },

    {"print_page",        (PyCFunction) ZvbiPage_print_page,         METH_VARARGS | METH_KEYWORDS, NULL },

    {"get_page_no",       (PyCFunction) ZvbiPage_get_page_no,        METH_NOARGS, NULL },
    {"get_page_size",     (PyCFunction) ZvbiPage_get_page_size,      METH_NOARGS, NULL },
    {"get_page_dirty_range", (PyCFunction) ZvbiPage_get_page_dirty_range, METH_NOARGS, NULL },
    {"get_page_color_map",(PyCFunction) ZvbiPage_get_page_color_map, METH_NOARGS, NULL },
    {"get_page_text_properties", (PyCFunction) ZvbiPage_get_page_text_properties, METH_NOARGS, NULL },
    {"get_page_text",     (PyCFunction) ZvbiPage_get_page_text,      METH_VARARGS, NULL },
    {"resolve_link",      (PyCFunction) ZvbiPage_resolve_link,       METH_VARARGS, NULL },
    {"resolve_home",      (PyCFunction) ZvbiPage_resolve_home,       METH_NOARGS,  NULL },

    {"__enter__",         (PyCFunction) ZvbiPage_DunderEnter,        METH_NOARGS,  NULL },
    {"__exit__",          (PyCFunction) ZvbiPage_DunderExit,         METH_VARARGS, NULL },

    {NULL}  /* Sentinel */
};

PyTypeObject ZvbiPageTypeDef =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Zvbi.Page",
    .tp_doc = PyDoc_STR("Class for rendering teletext and caption pages"),
    .tp_basicsize = sizeof(ZvbiPageObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = NULL,  // cannot be instantiated directly
    //.tp_init = (initproc) ZvbiPage_init,
    .tp_dealloc = (destructor) ZvbiPage_dealloc,
    //.tp_repr = (PyObject * (*)(PyObject*)) ZvbiPage_Repr,
    .tp_methods = ZvbiPage_MethodsDef,
    //.tp_members = ZvbiPage_Members,
};

int PyInit_Page(PyObject * module, PyObject * error_base)
{
    if (PyType_Ready(&ZvbiPageTypeDef) < 0) {
        return -1;
    }

    // create exception class
    ZvbiPageError = PyErr_NewException("Zvbi.PageError", error_base, NULL);
    Py_XINCREF(ZvbiPageError);
    if (PyModule_AddObject(module, "PageError", ZvbiPageError) < 0) {
        Py_XDECREF(ZvbiPageError);
        Py_CLEAR(ZvbiPageError);
        Py_DECREF(module);
        return -1;
    }

    // create class type object
    Py_INCREF(&ZvbiPageTypeDef);
    if (PyModule_AddObject(module, "Page", (PyObject *) &ZvbiPageTypeDef) < 0) {
        Py_DECREF(&ZvbiPageTypeDef);
        Py_XDECREF(ZvbiPageError);
        Py_CLEAR(ZvbiPageError);
        return -1;
    }

    return 0;
}
