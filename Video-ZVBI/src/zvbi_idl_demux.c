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

#include "zvbi_idl_demux.h"

#if 0
// ---------------------------------------------------------------------------
// Independent Data Line format A (EN 300 708 section 6.5) demultiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_idl_demux_obj_struct {
        vbi_idl_demux * ctx;
        SV *            demux_cb;
        SV *            demux_user_data;
} VbiIdl_DemuxObj;

// ---------------------------------------------------------------------------

vbi_bool
zvbi_xs_demux_idl_handler( vbi_idl_demux *        dx,
                           const uint8_t *        buffer,
                           unsigned int           n_bytes,
                           unsigned int           flags,
                           void *                 user_data)
{
        VbiIdl_DemuxObj * ctx = user_data;
        I32  count;
        vbi_bool result = FALSE;

        if ((ctx != NULL) && (ctx->demux_cb != NULL)) {
                dSP ;
                ENTER ;
                SAVETMPS ;

                /* push all function parameters on the Perl interpreter stack */
                PUSHMARK(SP) ;
                XPUSHs(sv_2mortal (newSVpvn ((char*)buffer, n_bytes)));
                XPUSHs(sv_2mortal (newSViv (flags)));
                XPUSHs(ctx->demux_user_data);
                PUTBACK ;

                /* invoke the Perl subroutine */
                count = call_sv(ctx->demux_cb, G_SCALAR) ;

                SPAGAIN ;

                if (count == 1) {
                        result = POPi;
                }

                FREETMPS ;
                LEAVE ;
        }
        return result;
}

// ---------------------------------------------------------------------------

VbiIdl_DemuxObj *
new(channel, address, callback, user_data=NULL)
        unsigned int           channel
        unsigned int           address
        CV *                   callback
        SV *                   user_data
        CODE:
        if (callback != NULL) {
                Newxz(RETVAL, 1, VbiIdl_DemuxObj);
                RETVAL->ctx = vbi_idl_a_demux_new(channel, address,
                                                  zvbi_xs_demux_idl_handler, RETVAL);
                if (RETVAL->ctx != NULL) {
                        RETVAL->demux_cb = SvREFCNT_inc(callback);
                        RETVAL->demux_user_data = SvREFCNT_inc(user_data);
                } else {
                        Safefree(RETVAL);
                        RETVAL = NULL;
                }
        } else {
                croak("Callback must be defined");
                RETVAL = NULL;
        }
        OUTPUT:
        RETVAL

void
DESTROY(dx)
        VbiIdl_DemuxObj * dx
        CODE:
        vbi_idl_demux_delete(dx->ctx);
        Save_SvREFCNT_dec(dx->demux_cb);
        Save_SvREFCNT_dec(dx->demux_user_data);
        Safefree(dx);

void
vbi_idl_demux_reset(dx)
        VbiIdl_DemuxObj * dx
        CODE:
        vbi_idl_demux_reset(dx->ctx);

vbi_bool
vbi_idl_demux_feed(dx, sv_buf)
        VbiIdl_DemuxObj * dx
        SV * sv_buf
        PREINIT:
        uint8_t * p_buf;
        STRLEN buf_size;
        CODE:
        if (SvOK(sv_buf)) {
                p_buf = (uint8_t *) SvPV(sv_buf, buf_size);
                if (buf_size >= 42) {
                        RETVAL = vbi_idl_demux_feed(dx->ctx, p_buf);
                } else {
                        croak("Input buffer has less than 42 bytes");
                        RETVAL = FALSE;
                }
        } else {
                croak("Input buffer is undefined or not a scalar");
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

vbi_bool
vbi_idl_demux_feed_frame(dx, sv_sliced, n_lines)
        VbiIdl_DemuxObj * dx
        SV * sv_sliced
        unsigned int n_lines
        PREINIT:
        vbi_sliced * p_sliced;
        unsigned int max_lines;
        CODE:
        p_sliced = zvbi_xs_sv_to_sliced(sv_sliced, &max_lines);
        if (p_sliced != NULL) {
                if (n_lines <= max_lines) {
                        RETVAL = vbi_idl_demux_feed_frame(dx->ctx, p_sliced, n_lines);
                } else {
                        croak("Invalid line count %d for buffer size (max. %d lines)", n_lines, max_lines);
                        RETVAL = FALSE;
                }
        } else {
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

#endif // 0
