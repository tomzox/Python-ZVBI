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
#if !defined (_PY_ZVBI_RAW_PARAMS_H)
#define _PY_ZVBI_RAW_PARAMS_H

extern PyTypeObject ZvbiRawParamsTypeDef;
vbi_raw_decoder * ZvbiRawParamsGetStruct(PyObject * self);
PyObject * ZvbiRawParamsFromStruct(const vbi_raw_decoder * par);

int PyInit_RawParams(PyObject * module, PyObject * error_base);

#endif  /* _PY_ZVBI_RAW_PARAMS_H */
