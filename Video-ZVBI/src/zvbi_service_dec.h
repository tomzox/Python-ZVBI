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
#if !defined (_PY_ZVBI_SRV_DEC_H)
#define _PY_ZVBI_SRV_DEC_H

extern PyTypeObject ZvbiServiceDecTypeDef;
vbi_decoder * ZvbiServiceDec_GetBuf(PyObject * obj);

int PyInit_ServiceDec(PyObject * module, PyObject * error_base);

#endif  /* _PY_ZVBI_SRV_DEC_H */
