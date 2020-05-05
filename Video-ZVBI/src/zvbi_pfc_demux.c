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

#include "zvbi_pfc_demux.h"

#if 0
// ---------------------------------------------------------------------------
// Page Function Clear (ETS 300 708 section 4) demultiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_pfc_demux_obj_struct {
        vbi_pfc_demux * ctx;
        SV *            demux_cb;
        SV *            demux_user_data;
} VbiPfc_DemuxObj;

// ---------------------------------------------------------------------------

vbi_bool
zvbi_xs_demux_pfc_handler( vbi_pfc_demux *        dx,
                           void *                 user_data,
                           const vbi_pfc_block *  block )
{
        VbiPfc_DemuxObj * ctx = user_data;
        I32  count;
        vbi_bool result = FALSE;

        if ((ctx != NULL) && (ctx->demux_cb != NULL)) {
                dSP ;
                ENTER ;
                SAVETMPS ;

                /* push all function parameters on the Perl interpreter stack */
                PUSHMARK(SP) ;
                XPUSHs(sv_2mortal (newSViv (block->pgno)));
                XPUSHs(sv_2mortal (newSViv (block->stream)));
                XPUSHs(sv_2mortal (newSViv (block->application_id)));
                XPUSHs(sv_2mortal (newSVpvn ((char*)block->block, block->block_size)));
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

VbiPfc_DemuxObj *
vbi_pfc_demux_new(pgno, stream, callback, user_data=NULL)
        vbi_pgno               pgno
        unsigned int           stream
        CV *                   callback
        SV *                   user_data
        CODE:
        if (callback != NULL) {
                Newxz(RETVAL, 1, VbiPfc_DemuxObj);
                /* note: libzvbi prior to version 0.2.26 had an incorrect type definition
                 * for the callback, hence the compiler will warn about a type mismatch */
                RETVAL->ctx = vbi_pfc_demux_new(pgno, stream, zvbi_xs_demux_pfc_handler, RETVAL);

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
        VbiPfc_DemuxObj * dx
        CODE:
        vbi_pfc_demux_delete(dx->ctx);
        Save_SvREFCNT_dec(dx->demux_cb);
        Save_SvREFCNT_dec(dx->demux_user_data);
        Safefree(dx);

void
vbi_pfc_demux_reset(dx)
        VbiPfc_DemuxObj * dx
        CODE:
        vbi_pfc_demux_reset(dx->ctx);

vbi_bool
vbi_pfc_demux_feed(dx, sv_buf)
        VbiPfc_DemuxObj * dx
        SV * sv_buf
        PREINIT:
        uint8_t * p_buf;
        STRLEN buf_size;
        CODE:
        if (SvOK(sv_buf)) {
                p_buf = (uint8_t *) SvPV(sv_buf, buf_size);
                if (buf_size >= 42) {
                        RETVAL = vbi_pfc_demux_feed(dx->ctx, p_buf);
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
vbi_pfc_demux_feed_frame(dx, sv_sliced, n_lines)
        VbiPfc_DemuxObj * dx
        SV * sv_sliced
        unsigned int n_lines
        PREINIT:
        vbi_sliced * p_sliced;
        unsigned int max_lines;
        CODE:
        p_sliced = zvbi_xs_sv_to_sliced(sv_sliced, &max_lines);
        if (p_sliced != NULL) {
                if (n_lines <= max_lines) {
                        RETVAL = vbi_pfc_demux_feed_frame(dx->ctx, p_sliced, n_lines);
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
