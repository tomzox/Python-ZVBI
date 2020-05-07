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

#include "zvbi_dvb_demux.h"

#if 0
// ---------------------------------------------------------------------------
//  DVB demultiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_dvb_demux_obj_struct {
        vbi_dvb_demux * ctx;
        SV *            demux_cb;
        SV *            demux_user_data;
        SV *            log_cb;
        SV *            log_user_data;
} VbiDvb_DemuxObj;

// ---------------------------------------------------------------------------

/*
 * Invoke callback in DVB PES de-multiplexer to process sliced data.
 * Callback can return FALSE to abort decoding of the current buffer
 */
vbi_bool
zvbi_xs_dvb_pes_handler( vbi_dvb_demux *        dx,
                         void *                 user_data,
                         const vbi_sliced *     sliced,
                         unsigned int           sliced_lines,
                         int64_t                pts)
{
        VbiDvb_DemuxObj * ctx = user_data;
        vbi_capture_buffer buffer;
        SV * sv_sliced;

        I32  count;
        vbi_bool result = FALSE; /* defaults to "failure" result */

        if ((ctx != NULL) && (ctx->demux_cb != NULL)) {
                dSP ;
                ENTER ;
                SAVETMPS ;

                buffer.data = (void*)sliced;  /* cast removes "const" */
                buffer.size = sizeof(vbi_sliced) * sliced_lines;
                buffer.timestamp = pts * 90000.0;

                sv_sliced = newSV(0);
                sv_setref_pv(sv_sliced, "VbiSlicedBufferPtr", (void*)&buffer);

                /* push the function parameters on the Perl interpreter stack */
                PUSHMARK(SP) ;
                XPUSHs(sv_2mortal (sv_sliced));
                XPUSHs(sv_2mortal (newSVuv (sliced_lines)));
                XPUSHs(sv_2mortal (newSViv (pts)));
                if (ctx->demux_user_data != NULL) {
                        XPUSHs(ctx->demux_user_data);
                }
                PUTBACK ;

                /* invoke the Perl subroutine */
                count = call_sv(ctx->demux_cb, G_SCALAR) ;

                SPAGAIN ;

                if (count == 1) {
                        result = !! POPi;
                }

                FREETMPS ;
                LEAVE ;
        }
        return result;
}

void
zvbi_xs_dvb_log_handler( vbi_log_mask           level,
                         const char *           context,
                         const char *           message,
                         void *                 user_data)
{
        VbiDvb_DemuxObj * ctx = user_data;
        I32  count;

        if ((ctx != NULL) && (ctx->log_cb != NULL)) {
                dSP ;
                ENTER ;
                SAVETMPS ;

                /* push all function parameters on the Perl interpreter stack */
                PUSHMARK(SP) ;
                XPUSHs(sv_2mortal (newSViv (level)));
                mXPUSHp(context, strlen(context));
                mXPUSHp(message, strlen(message));
                if (ctx->log_user_data != NULL) {
                        XPUSHs(ctx->log_user_data);
                }
                PUTBACK ;

                /* invoke the Perl subroutine */
                count = call_sv(ctx->log_cb, G_SCALAR) ;

                SPAGAIN ;

                FREETMPS ;
                LEAVE ;
        }
}

// ---------------------------------------------------------------------------

VbiDvb_DemuxObj *
pes_new(callback=NULL, user_data=NULL)
        CV *                   callback
        SV *                   user_data
        CODE:
        Newxz(RETVAL, 1, VbiDvb_DemuxObj);
        if (callback != NULL) {
                RETVAL->ctx = vbi_dvb_pes_demux_new(zvbi_xs_dvb_pes_handler, RETVAL);
                if (RETVAL->ctx != NULL) {
                        RETVAL->demux_cb = SvREFCNT_inc(callback);
                        RETVAL->demux_user_data = SvREFCNT_inc(user_data);
                }
        } else {
                RETVAL->ctx = vbi_dvb_pes_demux_new(NULL, NULL);
        }
        if (RETVAL->ctx == NULL) {
                Safefree(RETVAL);
                RETVAL = NULL;
        }
        OUTPUT:
        RETVAL

void
DESTROY(dx)
        VbiDvb_DemuxObj *       dx
        CODE:
        vbi_dvb_demux_delete(dx->ctx);
        Save_SvREFCNT_dec(dx->demux_cb);
        Save_SvREFCNT_dec(dx->demux_user_data);
        Save_SvREFCNT_dec(dx->log_cb);
        Save_SvREFCNT_dec(dx->log_user_data);
        Safefree(dx);

void
vbi_dvb_demux_reset(dx)
        VbiDvb_DemuxObj *       dx
        CODE:
        vbi_dvb_demux_reset(dx->ctx);

unsigned int
vbi_dvb_demux_cor(dx, sv_sliced, sliced_lines, pts, sv_buf, buf_left)
        VbiDvb_DemuxObj *       dx
        SV *                    sv_sliced
        unsigned int            sliced_lines
        int64_t                 &pts = NO_INIT
        SV *                    sv_buf
        unsigned int            buf_left
        PREINIT:
        STRLEN buf_size;
        const uint8_t * p_buf;
        vbi_sliced * p_sliced;
        size_t size_sliced;
        CODE:
        if (dx->demux_cb != NULL) {
                croak("Use of the cor method is not supported in dvb_demux contexts with handler function");

        } else if (SvOK(sv_buf)) {
                p_buf = (void *) SvPV(sv_buf, buf_size);
                if (buf_left <= buf_size) {
                        p_buf += buf_size - buf_left;

                        size_sliced = sliced_lines * sizeof(vbi_sliced);
                        p_sliced = (void *)zvbi_xs_sv_buffer_prep(sv_sliced, size_sliced);

                        RETVAL = vbi_dvb_demux_cor(dx->ctx, p_sliced, sliced_lines, &pts,
                                                   &p_buf, &buf_left);
                } else {
                        croak("Input buffer size %d is less than left count %d", (int)buf_size, (int)buf_left);
                        RETVAL = 0;
                }
        } else {
                croak("Input buffer is undefined or not a scalar");
                RETVAL = 0;
        }
        OUTPUT:
        sv_sliced
        pts
        buf_left
        RETVAL

vbi_bool
vbi_dvb_demux_feed(dx, sv_buf)
        VbiDvb_DemuxObj *       dx
        SV *                    sv_buf
        PREINIT:
        STRLEN buf_size;
        uint8_t * p_buf;
        CODE:
        if (dx->demux_cb == NULL) {
                croak("Use of the feed method is not possible in dvb_demux contexts without handler function");

        } else if (SvOK(sv_buf)) {
                p_buf = (uint8_t *) SvPV(sv_buf, buf_size);
                RETVAL = vbi_dvb_demux_feed(dx->ctx, p_buf, buf_size);
        } else {
                croak("Input buffer is undefined or not a scalar");
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

void
vbi_dvb_demux_set_log_fn(dx, mask, log_fn=NULL, user_data=NULL)
        VbiDvb_DemuxObj *       dx
        int                     mask
        CV *                    log_fn
        SV *                    user_data
        CODE:
        Save_SvREFCNT_dec(dx->log_cb);
        Save_SvREFCNT_dec(dx->log_user_data);
        if (log_fn != NULL) {
                dx->log_cb = SvREFCNT_inc(log_fn);
                dx->demux_user_data = SvREFCNT_inc(user_data);
                vbi_dvb_demux_set_log_fn(dx->ctx, mask, zvbi_xs_dvb_log_handler, dx);
        } else {
                dx->log_cb = NULL;
                dx->log_user_data = NULL;
                vbi_dvb_demux_set_log_fn(dx->ctx, mask, NULL, NULL);
        }

#endif // 0
