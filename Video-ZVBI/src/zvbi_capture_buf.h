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
#if !defined (_PY_ZVBI_RAWBUF_H)
#define _PY_ZVBI_RAWBUF_H

vbi_capture_buffer * ZvbiCaptureBuf_GetBuf(PyObject * obj);

PyObject * ZvbiCaptureRawBuf_FromPtr(vbi_capture_buffer * ptr);
PyObject * ZvbiCaptureRawBuf_FromData(char * data, int size, double timestamp);
PyObject * ZvbiCaptureSlicedBuf_FromPtr(vbi_capture_buffer * ptr);
PyObject * ZvbiCaptureSlicedBuf_FromData(vbi_sliced * data, int n_lines, double timestamp);

extern PyTypeObject ZvbiCaptureRawBufTypeDef;
extern PyTypeObject ZvbiCaptureSlicedBufTypeDef;

int PyInit_CaptureBuf(PyObject * module, PyObject * error_base);

#endif  /* _PY_ZVBI_RAWBUF_H */
