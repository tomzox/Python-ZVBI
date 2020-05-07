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

#include "zvbi_xds_demux.h"

#if 0
// ---------------------------------------------------------------------------
// Extended Data Service (EIA 608) demultiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_xds_demux_obj_struct {
        vbi_xds_demux * ctx;
        SV *            demux_cb;
        SV *            demux_user_data;
} VbiXds_DemuxObj;

// ---------------------------------------------------------------------------

vbi_bool
zvbi_xs_demux_xds_handler( vbi_xds_demux *        xd,
                           const vbi_xds_packet * xp,
                           void *                 user_data)
{
        VbiXds_DemuxObj * ctx = user_data;
        I32  count;
        vbi_bool result = FALSE;

        if ((ctx != NULL) && (ctx->demux_cb != NULL)) {
                dSP ;
                ENTER ;
                SAVETMPS ;

                /* push all function parameters on the Perl interpreter stack */
                PUSHMARK(SP) ;
                XPUSHs(sv_2mortal (newSViv (xp->xds_class)));
                XPUSHs(sv_2mortal (newSViv (xp->xds_subclass)));
                XPUSHs(sv_2mortal (newSVpvn ((char*)xp->buffer, xp->buffer_size)));
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

VbiXds_DemuxObj *
vbi_xds_demux_new(callback, user_data=NULL)
        CV * callback
        SV * user_data
        CODE:
        if (callback != NULL) {
                Newxz(RETVAL, 1, VbiXds_DemuxObj);
                RETVAL->ctx = vbi_xds_demux_new(zvbi_xs_demux_xds_handler, RETVAL);

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
DESTROY(xd)
        VbiXds_DemuxObj * xd
        CODE:
        vbi_xds_demux_delete(xd->ctx);
        Save_SvREFCNT_dec(xd->demux_cb);
        Save_SvREFCNT_dec(xd->demux_user_data);
        Safefree(xd);

void
vbi_xds_demux_reset(xd)
        VbiXds_DemuxObj * xd
        CODE:
        vbi_xds_demux_reset(xd->ctx);

vbi_bool
vbi_xds_demux_feed(xd, sv_buf)
        VbiXds_DemuxObj * xd
        SV * sv_buf
        PREINIT:
        uint8_t * p_buf;
        STRLEN buf_size;
        CODE:
        if (SvOK(sv_buf)) {
                p_buf = (uint8_t *) SvPV(sv_buf, buf_size);
                if (buf_size >= 2) {
                        RETVAL = vbi_xds_demux_feed(xd->ctx, p_buf);
                } else {
                        croak("Input buffer has less than 2 bytes");
                        RETVAL = FALSE;
                }
        } else {
                croak("Input buffer is undefined or not a scalar");
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

vbi_bool
vbi_xds_demux_feed_frame(xd, sv_sliced, n_lines)
        VbiXds_DemuxObj * xd
        SV * sv_sliced
        unsigned int n_lines
        PREINIT:
        vbi_sliced * p_sliced;
        unsigned int max_lines;
        CODE:
        p_sliced = zvbi_xs_sv_to_sliced(sv_sliced, &max_lines);
        if (p_sliced != NULL) {
                if (n_lines <= max_lines) {
                        RETVAL = vbi_xds_demux_feed_frame(xd->ctx, p_sliced, n_lines);
                } else {
                        croak("Invalid line count %d for buffer size (max. %d lines)", n_lines, max_lines);
                        RETVAL = FALSE;
                }
        } else {
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

// static methods

void
rating_string(auth, id)
        int auth
        int id
        PREINIT:
        const char * p = vbi_rating_string(auth, id);
        PPCODE:
        if (p != NULL) {
                EXTEND(sp, 1);
                PUSHs (sv_2mortal(newSVpv(p, strlen(p))));
        }

void
prog_type_string(classf, id)
        int classf
        int id
        PREINIT:
        const char * p = vbi_prog_type_string(classf, id);
        PPCODE:
        if (p != NULL) {
                EXTEND(sp, 1);
                PUSHs (sv_2mortal(newSVpv(p, strlen(p))));
        }

#endif // 0
