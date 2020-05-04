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

#include "zvbi_capture.h"
#include "zvbi_proxy.h"
#include "zvbi_rawdec.h"
#include "zvbi_capture_buf.h"
#include "zvbi_service_dec.h"
#include "zvbi_page.h"
#include "zvbi_export.h"
#include "zvbi_search.h"

/* Version of library that contains all used interfaces
 * (which was released 2007, so we do not bother supporting older ones) */
#define ZVBI_XS_MIN_MICRO 26

/* macro to check for a minimum libzvbi header file version number */
#define LIBZVBI_H_VERSION(A,B,C) \
        ((VBI_VERSION_MAJOR>(A)) || \
        ((VBI_VERSION_MAJOR==(A)) && (VBI_VERSION_MINOR>(B))) || \
        ((VBI_VERSION_MAJOR==(A)) && (VBI_VERSION_MINOR==(B)) && (VBI_VERSION_MICRO>=(C))))

#if !(LIBZVBI_H_VERSION(0,2,ZVBI_XS_MIN_MICRO))
#error "Minimum version for libzvbi is 0.2." #ZVBI_XS_MIN_MICRO
#endif

/*
 * Basic types
 */
//typedef _Bool vbi_bool;
typedef int32_t vbi_pgno;
typedef int32_t vbi_subno;
typedef uint32_t vbi_nuid;
//typedef int32_t vbi_pixfmt;
//typedef int32_t VBI_CHN_PRIO;
//typedef int32_t VBI_CAPTURE_FD_FLAGS;
typedef uint32_t vbi_service_set;
//typedef uint32_t vbi_videostd_set;
//TODO vbi_page_type T_ENUM

#if 0

typedef struct vbi_dvb_mux_obj_struct {
        vbi_dvb_mux *   ctx;
        SV *            mux_cb;
        SV *            mux_user_data;
} VbiDvb_MuxObj;

typedef struct vbi_dvb_demux_obj_struct {
        vbi_dvb_demux * ctx;
        SV *            demux_cb;
        SV *            demux_user_data;
        SV *            log_cb;
        SV *            log_user_data;
} VbiDvb_DemuxObj;

typedef struct vbi_idl_demux_obj_struct {
        vbi_idl_demux * ctx;
        SV *            demux_cb;
        SV *            demux_user_data;
} VbiIdl_DemuxObj;

typedef struct vbi_pfc_demux_obj_struct {
        vbi_pfc_demux * ctx;
        SV *            demux_cb;
        SV *            demux_user_data;
} VbiPfc_DemuxObj;

typedef struct vbi_xds_demux_obj_struct {
        vbi_xds_demux * ctx;
        SV *            demux_cb;
        SV *            demux_user_data;
} VbiXds_DemuxObj;

#define zvbi_(NAME) vbi_ ## NAME


/*
 * Invoke callback in DVB PES and TS multiplexer to process generated
 * packets. Callback can return FALSE to discard remaining data.
 */
vbi_bool
zvbi_xs_dvb_mux_handler( vbi_dvb_mux *          mx,
                         void *                 user_data,
                         const uint8_t *        packet,
                         unsigned int           packet_size )
{
        VbiDvb_MuxObj * ctx = user_data;
        I32  count;
        vbi_bool result = FALSE; /* defaults to "failure" result */

        if ((ctx != NULL) && (ctx->mux_cb != NULL)) {
                dSP ;
                ENTER ;
                SAVETMPS ;

                /* push the function parameters on the Perl interpreter stack */
                PUSHMARK(SP) ;
                XPUSHs(sv_2mortal (newSVpvn ((char*)packet, packet_size)));
                if (ctx->mux_user_data != NULL) {
                        XPUSHs(ctx->mux_user_data);
                }
                PUTBACK ;

                /* invoke the Perl subroutine */
                count = call_sv(ctx->mux_cb, G_SCALAR) ;

                SPAGAIN ;

                if (count == 1) {
                        result = !! POPi;
                }

                FREETMPS ;
                LEAVE ;
        }
        return result;
}

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

/*
 * Invoke callback for log messages.
 */
static void
zvbi_xs_log_callback( vbi_log_mask           level,
                      const char *           context,
                      const char *           message,
                      void *                 user_data)
{
        dMY_CXT;
        SV * perl_cb;
        unsigned cb_idx = PVOID2UINT(user_data);

        if ( (cb_idx < ZVBI_MAX_CB_COUNT) &&
             ((perl_cb = MY_CXT.log[cb_idx].p_cb) != NULL) ) {

                dSP ;
                ENTER ;
                SAVETMPS ;

                /* push the function parameters on the Perl interpreter stack */
                PUSHMARK(SP) ;
                XPUSHs(sv_2mortal (newSViv (level)));
                mXPUSHp(context, strlen(context));
                mXPUSHp(message, strlen(message));
                if (MY_CXT.log[cb_idx].p_data != NULL) {
                        XPUSHs(MY_CXT.log[cb_idx].p_data);
                }
                PUTBACK ;

                /* invoke the Perl subroutine */
                call_sv(perl_cb, G_VOID | G_DISCARD) ;

                FREETMPS ;
                LEAVE ;
        }
}

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

/*
 * Get slicer buffer from a blessed vbi_capture_buffer struct or a plain scalar
 */
static vbi_sliced *
zvbi_xs_sv_to_sliced( SV * sv_sliced, unsigned int * max_lines )
{
        vbi_sliced * p_sliced;

        if (sv_derived_from(sv_sliced, "VbiSlicedBufferPtr")) {
                IV tmp = SvIV((SV*)SvRV(sv_sliced));
                vbi_capture_buffer * p_sliced_buf = INT2PTR(vbi_capture_buffer *,tmp);
                *max_lines = p_sliced_buf->size / sizeof(vbi_sliced);
                p_sliced = p_sliced_buf->data;

        } else if (SvOK(sv_sliced)) {
                size_t buf_size;
                p_sliced = (void *) SvPV(sv_sliced, buf_size);
                *max_lines = buf_size / sizeof(vbi_sliced);

        } else {
                croak("Input raw buffer is undefined or not a scalar");
                p_sliced = NULL;
                *max_lines = 0;
        }

        return p_sliced;
}

/*
 * Grow the given scalar to exactly the requested size for use as an output buffer
 */
static void *
zvbi_xs_sv_buffer_prep( SV * sv_buf, STRLEN buf_size )
{
        STRLEN l;

        if (!SvPOK(sv_buf))  {
                sv_setpv(sv_buf, "");
        }
        SvGROW(sv_buf, buf_size + 1);
        SvCUR_set(sv_buf, buf_size);
        return SvPV_force(sv_buf, l);
}

// ---------------------------------------------------------------------------
//  DVB multiplexer
// ---------------------------------------------------------------------------

//MODULE = Video::ZVBI  PACKAGE = Video::ZVBI::dvb_mux  PREFIX = vbi_dvb_mux_

VbiDvb_MuxObj *
pes_new(callback=NULL, user_data=NULL)
        CV *            callback
        SV *            user_data
        CODE:
        Newxz(RETVAL, 1, VbiDvb_MuxObj);
        if (callback != NULL) {
                RETVAL->ctx = zvbi_(dvb_pes_mux_new)(zvbi_xs_dvb_mux_handler, RETVAL);
                if (RETVAL->ctx != NULL) {
                        RETVAL->mux_cb = SvREFCNT_inc(callback);
                        RETVAL->mux_user_data = SvREFCNT_inc(user_data);
                }
        } else {
                RETVAL->ctx = zvbi_(dvb_pes_mux_new)(NULL, NULL);
        }
        if (RETVAL->ctx == NULL) {
                Safefree(RETVAL);
                RETVAL = NULL;
        }
        OUTPUT:
        RETVAL

VbiDvb_MuxObj *
ts_new(pid, callback=NULL, user_data=NULL)
        unsigned int    pid
        CV *            callback
        SV *            user_data
        CODE:
        Newxz(RETVAL, 1, VbiDvb_MuxObj);
        if (callback != NULL) {
                RETVAL->ctx = zvbi_(dvb_ts_mux_new)(pid, zvbi_xs_dvb_mux_handler, RETVAL);
                if (RETVAL->ctx != NULL) {
                        RETVAL->mux_cb = SvREFCNT_inc(callback);
                        RETVAL->mux_user_data = SvREFCNT_inc(user_data);
                }
        } else {
                RETVAL->ctx = zvbi_(dvb_ts_mux_new)(pid, NULL, NULL);
        }
        if (RETVAL->ctx == NULL) {
                Safefree(RETVAL);
                RETVAL = NULL;
        }
        OUTPUT:
        RETVAL

void
DESTROY(mx)
        VbiDvb_MuxObj * mx
        CODE:
        zvbi_(dvb_mux_delete)(mx->ctx);
        Save_SvREFCNT_dec(mx->mux_cb);
        Save_SvREFCNT_dec(mx->mux_user_data);
        Safefree(mx);

void
vbi_dvb_mux_reset(mx)
        VbiDvb_MuxObj * mx
        CODE:
        zvbi_(dvb_mux_reset)(mx->ctx);

vbi_bool
vbi_dvb_mux_cor(mx, sv_buf, buffer_left, sv_sliced, sliced_left, service_mask, pts, sv_raw=NULL, hv_raw_par=NULL)
        VbiDvb_MuxObj *         mx
        SV *                    sv_buf
        unsigned int            &buffer_left
        SV  *                   sv_sliced
        unsigned int            &sliced_left
        vbi_service_set         service_mask
        int64_t                 pts
        SV  *                   sv_raw
        HV  *                   hv_raw_par
        PREINIT:
        uint8_t *               p_buf;
        STRLEN                  buf_size;
        const vbi_sliced *      p_sliced;
        unsigned int            max_lines;
        const uint8_t *         p_raw = NULL;
        STRLEN                  raw_size;
        vbi_raw_decoder         rd;
        CODE:
        if (sv_raw != NULL) {
                Zero(&rd, 1, vbi_raw_decoder);
                if (hv_raw_par != NULL) {
                        zvbi_xs_hv_to_dec_params(hv_raw_par, &rd);
                } else {
                        croak("Sampling parameters must be present when a raw buffer is passed");
                }
                if (SvOK(sv_raw)) {
                        p_raw = (void *) SvPV(sv_raw, raw_size);
                        if (raw_size < (rd.count[0] + rd.count[1]) * rd.bytes_per_line) {
                                croak("Input raw buffer is smaller than required for "
                                      "VBI geometry (%d+%d lines with %d bytes)",
                                      rd.count[0], rd.count[1], rd.bytes_per_line);
                                p_raw = NULL;
                        }
                } else {
                        croak("Raw buffer is undefined or not a scalar");
                }
        }
        if (SvPOK(sv_buf)) {
                p_buf = (void *) SvPV(sv_buf, buf_size);
        } else {
                p_buf = zvbi_xs_sv_buffer_prep(sv_buf, buffer_left);
                buf_size = buffer_left;
        }
        if (buffer_left <= buf_size) {
                p_sliced = zvbi_xs_sv_to_sliced(sv_sliced, &max_lines);
                if ((p_sliced != NULL) && (sliced_left <= max_lines)) {
                        p_buf += buf_size - buffer_left;
                        p_sliced += max_lines - sliced_left;

                        RETVAL =
                          zvbi_(dvb_mux_cor)(mx->ctx,
                                             &p_buf, &buffer_left,
                                             &p_sliced, &sliced_left,
                                             service_mask,
                                             p_raw, ((p_raw != NULL) ? &rd : NULL),
                                             pts);
                } else if (p_sliced != NULL) {
                        croak("Invalid sliced left count %d for buffer size (max. %d lines)", sliced_left, max_lines);
                        RETVAL = FALSE;
                } else {
                        RETVAL = FALSE;
                }
        } else {
                croak("Output buffer size %d is less than left count %d", (int)buf_size, (int)buffer_left);
                RETVAL = FALSE;
        }
        OUTPUT:
        buffer_left
        sliced_left
        RETVAL

vbi_bool
vbi_dvb_mux_feed(mx, sv_sliced, sliced_lines, service_mask, pts, sv_raw=NULL, hv_raw_par=NULL)
        VbiDvb_MuxObj *         mx
        SV  *                   sv_sliced
        unsigned int            sliced_lines
        vbi_service_set         service_mask
        int64_t                 pts
        SV  *                   sv_raw
        HV  *                   hv_raw_par
        PREINIT:
        const vbi_sliced *      p_sliced;
        unsigned int            max_lines;
        const uint8_t *         p_raw = NULL;
        STRLEN                  raw_size;
        vbi_raw_decoder         rd;
        CODE:
        if (mx->mux_cb == NULL) {
                croak("Use of the feed method is not possible in dvb_mux contexts without handler function");
        }
        if (sv_raw != NULL) {
                Zero(&rd, 1, vbi_raw_decoder);
                if (hv_raw_par != NULL) {
                        zvbi_xs_hv_to_dec_params(hv_raw_par, &rd);
                } else {
                        croak("Sampling parameters must be present when a raw buffer is passed");
                }
                if (SvOK(sv_raw)) {
                        p_raw = (void *) SvPV(sv_raw, raw_size);
                        if (raw_size < (rd.count[0] + rd.count[1]) * rd.bytes_per_line) {
                                croak("Input raw buffer is smaller than required for "
                                      "VBI geometry (%d+%d lines with %d bytes)",
                                      rd.count[0], rd.count[1], rd.bytes_per_line);
                                p_raw = NULL;
                        }
                } else {
                        croak("Raw buffer is undefined or not a scalar");
                }
        }
        p_sliced = zvbi_xs_sv_to_sliced(sv_sliced, &max_lines);
        if ((p_sliced != NULL) && (sliced_lines <= max_lines)) {
                RETVAL =
                  zvbi_(dvb_mux_feed)(mx->ctx,
                                      p_sliced + max_lines - sliced_lines, sliced_lines,
                                      service_mask,
                                      p_raw, ((p_raw != NULL) ? &rd : NULL),
                                      pts);
        } else if (p_sliced != NULL) {
                croak("Invalid sliced line count %d for buffer size (max. %d lines)", sliced_lines, max_lines);
                RETVAL = FALSE;
        } else {
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

unsigned int
vbi_dvb_mux_get_data_identifier(mx)
        VbiDvb_MuxObj * mx
        CODE:
        RETVAL = zvbi_(dvb_mux_get_data_identifier)(mx->ctx);
        OUTPUT:
        RETVAL

vbi_bool
vbi_dvb_mux_set_data_identifier(mx, data_identifier)
        VbiDvb_MuxObj * mx
        unsigned int    data_identifier
        CODE:
        RETVAL = zvbi_(dvb_mux_set_data_identifier)(mx->ctx, data_identifier);
        OUTPUT:
        RETVAL

unsigned int
vbi_dvb_mux_get_min_pes_packet_size(mx)
        VbiDvb_MuxObj * mx
        CODE:
        RETVAL = zvbi_(dvb_mux_get_min_pes_packet_size)(mx->ctx);
        OUTPUT:
        RETVAL

unsigned int
vbi_dvb_mux_get_max_pes_packet_size(mx)
        VbiDvb_MuxObj * mx
        CODE:
        RETVAL = zvbi_(dvb_mux_get_max_pes_packet_size)(mx->ctx);
        OUTPUT:
        RETVAL

vbi_bool
vbi_dvb_mux_set_pes_packet_size(mx, min_size, max_size)
        VbiDvb_MuxObj * mx
        unsigned int    min_size
        unsigned int    max_size
        CODE:
        RETVAL = zvbi_(dvb_mux_set_pes_packet_size)(mx->ctx, min_size, max_size);
        OUTPUT:
        RETVAL

//MODULE = Video::ZVBI  PACKAGE = Video::ZVBI

vbi_bool
dvb_multiplex_sliced(sv_buf, buffer_left, sv_sliced, sliced_left, service_mask, data_identifier, stuffing)
        SV *                    sv_buf
        unsigned int            &buffer_left
        SV *                    sv_sliced
        unsigned int            &sliced_left
        vbi_service_set         service_mask
        unsigned int            data_identifier
        vbi_bool                stuffing
        PREINIT:
        uint8_t *               p_buf;
        STRLEN                  buf_size;
        const vbi_sliced *      p_sliced;
        unsigned int            max_lines;
        CODE:
        if (SvPOK(sv_buf)) {
                p_buf = (void *) SvPV(sv_buf, buf_size);
        } else {
                p_buf = zvbi_xs_sv_buffer_prep(sv_buf, buffer_left);
                buf_size = buffer_left;
        }
        p_buf = (void *) SvPV(sv_buf, buf_size);
        if (buffer_left <= buf_size) {
                p_sliced = zvbi_xs_sv_to_sliced(sv_sliced, &max_lines);
                if ((p_sliced != NULL) && (sliced_left <= max_lines)) {
                        p_buf += buf_size - buffer_left;
                        p_sliced += max_lines - sliced_left;

                        RETVAL =
                          zvbi_(dvb_multiplex_sliced) ( &p_buf, &buffer_left,
                                                        &p_sliced, &sliced_left,
                                                        service_mask, data_identifier,
                                                        stuffing );
                } else if (p_sliced != NULL) {
                        croak("Invalid sliced left count %d for buffer size (max. %d lines)", sliced_left, max_lines);
                        RETVAL = FALSE;
                } else {
                        RETVAL = FALSE;
                }
        } else {
                croak("Output buffer size %d is less than left count %d", (int)buf_size, (int)buffer_left);
                RETVAL = FALSE;
        }
        OUTPUT:
        buffer_left
        sliced_left
        RETVAL

vbi_bool
dvb_multiplex_raw(sv_buf, buffer_left, sv_raw, raw_left, data_identifier, videostd_set, line, first_pixel_position, n_pixels_total, stuffing)
        SV *                    sv_buf
        unsigned int            &buffer_left
        SV *                    sv_raw
        unsigned int            &raw_left
        unsigned int            data_identifier
        vbi_videostd_set        videostd_set
        unsigned int            line
        unsigned int            first_pixel_position
        unsigned int            n_pixels_total
        vbi_bool                stuffing
        PREINIT:
        uint8_t *               p_buf;
        STRLEN                  buf_size;
        const uint8_t *         p_raw;
        STRLEN                  raw_size;
        CODE:
        if (SvPOK(sv_buf)) {
                p_buf = (void *) SvPV(sv_buf, buf_size);
        } else {
                p_buf = zvbi_xs_sv_buffer_prep(sv_buf, buffer_left);
                buf_size = buffer_left;
        }
        p_buf = (void *) SvPV(sv_buf, buf_size);
        if (buffer_left <= buf_size) {
                if (SvOK(sv_raw)) {
                        p_raw = (void *) SvPV(sv_raw, raw_size);
                        if (raw_left <= raw_size) {
                                p_buf += buf_size - buffer_left;
                                p_raw += raw_size - raw_left;

                                RETVAL =
                                  zvbi_(dvb_multiplex_raw) ( &p_buf, &buffer_left,
                                                             &p_raw, &raw_left,
                                                             data_identifier, videostd_set,
                                                             line, first_pixel_position,
                                                             n_pixels_total, stuffing);
                        } else {
                                croak("Output buffer size %d is less than left count %d", (int)buf_size, buffer_left);
                                RETVAL = FALSE;
                        }
                } else {
                        croak("Raw input buffer is undefined or not a scalar");
                        RETVAL = FALSE;
                }
        } else {
                croak("Output buffer size %d is less than left count %d", (int)buf_size, (int)buffer_left);
                RETVAL = FALSE;
        }
        OUTPUT:
        buffer_left
        raw_left
        RETVAL

// ---------------------------------------------------------------------------
//  DVB demultiplexer
// ---------------------------------------------------------------------------

//MODULE = Video::ZVBI  PACKAGE = Video::ZVBI::dvb_demux        PREFIX = vbi_dvb_demux_

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
                zvbi_(dvb_demux_set_log_fn)(dx->ctx, mask, zvbi_xs_dvb_log_handler, dx);
        } else {
                dx->log_cb = NULL;
                dx->log_user_data = NULL;
                zvbi_(dvb_demux_set_log_fn)(dx->ctx, mask, NULL, NULL);
        }

// ---------------------------------------------------------------------------
// IDL Demux
// ---------------------------------------------------------------------------

//MODULE = Video::ZVBI  PACKAGE = Video::ZVBI::idl_demux        PREFIX = vbi_idl_demux_

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
                        RETVAL = zvbi_(idl_demux_feed_frame)(dx->ctx, p_sliced, n_lines);
                } else {
                        croak("Invalid line count %d for buffer size (max. %d lines)", n_lines, max_lines);
                        RETVAL = FALSE;
                }
        } else {
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

// ---------------------------------------------------------------------------
// PFC (Page Format Clear Demultiplexer ETS 300 708)
// ---------------------------------------------------------------------------

//MODULE = Video::ZVBI  PACKAGE = Video::ZVBI::pfc_demux        PREFIX = vbi_pfc_demux_

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
                        RETVAL = zvbi_(pfc_demux_feed_frame)(dx->ctx, p_sliced, n_lines);
                } else {
                        croak("Invalid line count %d for buffer size (max. %d lines)", n_lines, max_lines);
                        RETVAL = FALSE;
                }
        } else {
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

// ---------------------------------------------------------------------------
// XDS Demux
// ---------------------------------------------------------------------------

//MODULE = Video::ZVBI  PACKAGE = Video::ZVBI::xds_demux        PREFIX = vbi_xds_demux_

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
                        RETVAL = zvbi_(xds_demux_feed_frame)(xd->ctx, p_sliced, n_lines);
                } else {
                        croak("Invalid line count %d for buffer size (max. %d lines)", n_lines, max_lines);
                        RETVAL = FALSE;
                }
        } else {
                RETVAL = FALSE;
        }
        OUTPUT:
        RETVAL

// ---------------------------------------------------------------------------
//  Parity and Hamming decoding and encoding
// ---------------------------------------------------------------------------

//MODULE = Video::ZVBI  PACKAGE = Video::ZVBI   PREFIX = vbi_

unsigned int
vbi_par8(val)
        unsigned int val

int
vbi_unpar8(val)
        unsigned int val

void
par_str(data)
        SV * data
        PREINIT:
        uint8_t *p;
        STRLEN len;
        CODE:
        p = (uint8_t *)SvPV (data, len);
        vbi_par(p, len);
        OUTPUT:
        data
#endif // 0

//
// Note this function actually does not use libzvbi "unpar()" internally,
// as that function modifies the string in place which does not work with
// python "bytes" and "string" types being immutable; So we use the macro
// unpar8() instead and combine decoding with conversion to Unicode.
//
static PyObject *
Zvbi_unpar_str(PyObject *self, PyObject *args)
{
    PyObject * obj = NULL;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "O!", &PyBytes_Type, &obj))
    {
        char * in_buf;
        Py_ssize_t in_len;
        if (PyBytes_AsStringAndSize(obj, &in_buf, &in_len) == 0) {
            RETVAL = PyUnicode_New(in_len, 0x7f);
            if (RETVAL != NULL) {
                int kind = PyUnicode_KIND(RETVAL);
                void * out_buf = PyUnicode_DATA(RETVAL);
                for (int idx = 0; idx < in_len; ++idx) {
                    int c = vbi_unpar8(in_buf[idx]);
                    PyUnicode_WRITE(kind, out_buf, idx, ((c >= 0) ? c : ' '));
                }
            }
        }
    }
    return RETVAL;
}

#if 0

unsigned int
vbi_rev8(val)
        unsigned int val

unsigned int
vbi_rev16(val)
        unsigned int val

unsigned int
rev16p(data, offset=0)
        SV *    data
        int     offset
        PREINIT:
        uint8_t *p;
        STRLEN len;
        CODE:
        p = (uint8_t *)SvPV (data, len);
        if (len < offset + 2) {
                croak ("rev16p: input data length must greater than offset by at least 2");
        }
        RETVAL = vbi_rev16p(p + offset);
        OUTPUT:
        RETVAL

unsigned int
vbi_ham8(val)
        unsigned int val

int
vbi_unham8(val)
        unsigned int val

int
unham16p(data, offset=0)
        SV *    data
        int     offset
        PREINIT:
        unsigned char *p;
        STRLEN len;
        CODE:
        p = (unsigned char *)SvPV (data, len);
        if (len < offset + 2) {
                croak ("unham16p: input data length must greater than offset by at least 2");
        }
        RETVAL = vbi_unham16p(p + offset);
        OUTPUT:
        RETVAL

int
unham24p(data, offset=0)
        SV *    data
        int     offset
        PREINIT:
        unsigned char *p;
        STRLEN len;
        CODE:
        p = (unsigned char *)SvPV (data, len);
        if (len < offset + 3) {
                croak ("unham24p: input data length must greater than offset by at least 3");
        }
        RETVAL = vbi_unham24p(p + offset);
        OUTPUT:
        RETVAL

#endif

// ---------------------------------------------------------------------------
//  BCD arithmetic
// ---------------------------------------------------------------------------

static PyObject *
Zvbi_dec2bcd(PyObject *self, PyObject *args)
{
    unsigned int dec;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "I", &dec)) {
        RETVAL = PyLong_FromLong(vbi_dec2bcd(dec));
    }
    return RETVAL;
}

static PyObject *
Zvbi_bcd2dec(PyObject *self, PyObject *args)
{
    unsigned int bcd;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "I", &bcd)) {
        RETVAL = PyLong_FromLong(vbi_bcd2dec(bcd));
    }
    return RETVAL;
}

static PyObject *
Zvbi_add_bcd(PyObject *self, PyObject *args)
{
    unsigned int bcd1;
    unsigned int bcd2;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "II", &bcd1, &bcd2)) {
        RETVAL = PyLong_FromLong(vbi_add_bcd(bcd1, bcd2));
    }
    return RETVAL;
}

static PyObject *
Zvbi_is_bcd(PyObject *self, PyObject *args)
{
    unsigned int bcd;
    PyObject * RETVAL = NULL;

    if (PyArg_ParseTuple(args, "I", &bcd)) {
        RETVAL = PyBool_FromLong(vbi_is_bcd(bcd));
    }
    return RETVAL;
}

// ---------------------------------------------------------------------------
//  Miscellaneous
// ---------------------------------------------------------------------------

static PyObject *
Zvbi_lib_version(PyObject *self, PyObject *args)  // METH_NOARGS
{
    unsigned int major;
    unsigned int minor;
    unsigned int micro;

    vbi_version(&major, &minor, &micro);

    PyObject * tuple = PyTuple_New(3);
    PyTuple_SetItem(tuple, 0, PyLong_FromLong(major));
    PyTuple_SetItem(tuple, 1, PyLong_FromLong(minor));
    PyTuple_SetItem(tuple, 2, PyLong_FromLong(micro));

    return tuple;
}

static PyObject *
Zvbi_check_lib_version(PyObject *self, PyObject *args)
{
    unsigned int need_major = 0;
    unsigned int need_minor = 0;
    unsigned int need_micro = 0;
    unsigned int lib_major;
    unsigned int lib_minor;
    unsigned int lib_micro;

    if (!PyArg_ParseTuple(args, "I|II", &need_major, &need_minor, &need_micro))
        return NULL;

    vbi_version(&lib_major, &lib_minor, &lib_micro);

    vbi_bool ok = (lib_major > need_major) ||
                  ((lib_major == need_major) && (lib_minor > need_minor)) ||
                  ((lib_major == need_major) && (lib_minor == need_minor) && (lib_micro >= need_micro));

    return PyBool_FromLong(ok);
}

#if 0
void
set_log_fn(mask, log_fn=NULL, user_data=NULL)
        unsigned int mask
        CV * log_fn
        SV * user_data
        PREINIT:
        dMY_CXT;
        unsigned cb_idx;
        CODE:
        zvbi_xs_free_callback_by_obj(MY_CXT.log, NULL);
        if (log_fn != NULL) {
                cb_idx = zvbi_xs_alloc_callback(MY_CXT.log, (SV*)log_fn, user_data, NULL);
                if (cb_idx < ZVBI_MAX_CB_COUNT) {
                        zvbi_(set_log_fn)(mask, zvbi_xs_log_callback, UINT2PVOID(cb_idx));
                } else {
                        zvbi_(set_log_fn)(mask, NULL, NULL);
                        croak ("Max. log callback count exceeded");
                }
        } else {
                zvbi_(set_log_fn)(mask, NULL, NULL);
        }

void
set_log_on_stderr(mask)
        unsigned int mask
        PREINIT:
        dMY_CXT;
        CODE:
        zvbi_xs_free_callback_by_obj(MY_CXT.log, NULL);
        zvbi_(set_log_fn)(mask, zvbi_(log_on_stderr), NULL);

void
decode_vps_cni(data)
        SV * data
        PREINIT:
        unsigned int cni;
        unsigned char *p;
        STRLEN len;
        PPCODE:
        p = (unsigned char *)SvPV (data, len);
        if (len >= 13) {
                if (zvbi_(decode_vps_cni)(&cni, p)) {
                        EXTEND(sp,1);
                        PUSHs (sv_2mortal (newSVuv (cni)));
                }
        } else {
                croak ("decode_vps_cni: input buffer must have at least 13 bytes");
        }

void
encode_vps_cni(cni)
        unsigned int cni
        PREINIT:
        uint8_t buffer[13];
        PPCODE:
        if (zvbi_(encode_vps_cni)(buffer, cni)) {
                EXTEND(sp,1);
                PUSHs (sv_2mortal (newSVpvn ((char*)buffer, 13)));
        }

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

void
iconv_caption(sv_src, repl_char=0)
        SV * sv_src
        int repl_char
        PREINIT:
        char * p_src;
        char * p_buf;
        STRLEN src_len;
        SV * sv;
        PPCODE:
        p_src = (void *) SvPV(sv_src, src_len);
        p_buf = zvbi_(strndup_iconv_caption)("UTF-8", p_src, src_len, '?');
        if (p_buf != NULL) {
                sv = newSV(0);
                sv_usepvn(sv, p_buf, strlen(p_buf));
                /* now the pointer is managed by perl -> no free() */
                SvUTF8_on(sv);
                EXTEND(sp, 1);
                PUSHs (sv_2mortal (sv));
        }

void
caption_unicode(c, to_upper=0)
        unsigned int c
        vbi_bool to_upper
        PREINIT:
        UV ucs;
        U8 buf[10];
        U8 * p;
        SV * sv;
        PPCODE:
        ucs = zvbi_(caption_unicode)(c, to_upper);
        if (ucs != 0) {
                p = uvuni_to_utf8(buf, ucs);
                sv = sv_2mortal(newSVpvn(buf, p - buf));
                SvUTF8_on(sv);
        } else {
                sv = sv_2mortal(newSVpvn("", 0));
        }
        EXTEND(sp, 1);
        PUSHs (sv);

#endif  // if 0

// ---------------------------------------------------------------------------

static PyMethodDef Zvbi_Methods[] =
{
    {"unpar_str",         Zvbi_unpar_str,         METH_VARARGS, PyDoc_STR("Decode parity and convert to string")},

    {"dec2bcd",           Zvbi_dec2bcd,           METH_VARARGS, NULL},
    {"bcd2dec",           Zvbi_bcd2dec,           METH_VARARGS, NULL},
    {"add_bcd",           Zvbi_add_bcd,           METH_VARARGS, NULL},
    {"is_bcd",            Zvbi_is_bcd,            METH_VARARGS, NULL},

    {"lib_version",       Zvbi_lib_version,       METH_NOARGS,  PyDoc_STR("Return tuple with library version")},
    {"check_lib_version", Zvbi_check_lib_version, METH_VARARGS, PyDoc_STR("Check if library version is equal or newer than the given")},

    {NULL}       // sentinel
};

static struct PyModuleDef Zvbi_module =
{
    PyModuleDef_HEAD_INIT,
    .m_name = "Zvbi",
    .m_doc = PyDoc_STR("Interface to the Zapping VBI decoder library (for teletext & closed-caption)"),
    .m_size = -1,
    .m_methods = Zvbi_Methods
};

PyObject * ZvbiError;

PyMODINIT_FUNC
PyInit_Zvbi(void)
{
    PyObject * module = PyModule_Create(&Zvbi_module);
    if (module == NULL) {
        return NULL;
    }

    // create exception base class "Zvbi.error", derived from "Exception" base
    ZvbiError = PyErr_NewException("Zvbi.Error", PyExc_Exception, NULL);
    Py_XINCREF(ZvbiError);
    if (PyModule_AddObject(module, "Error", ZvbiError) < 0) {
        Py_XDECREF(ZvbiError);
        Py_CLEAR(ZvbiError);
        Py_DECREF(module);
        return NULL;
    }

    if ((PyInit_Capture(module, ZvbiError) < 0) ||
        (PyInit_Proxy(module, ZvbiError) < 0) ||
        (PyInit_RawDec(module, ZvbiError) < 0) ||
        (PyInit_CaptureBuf(module, ZvbiError) < 0) ||
        (PyInit_ServiceDec(module, ZvbiError) < 0) ||
        (PyInit_Page(module, ZvbiError) < 0) ||
        (PyInit_Export(module, ZvbiError) < 0) ||
        (PyInit_Search(module, ZvbiError) < 0))
    {
        Py_DECREF(module);
        return NULL;
    }

#define EXPORT_CONST(NAME) \
    {   if (PyModule_AddIntConstant(module, #NAME, NAME) < 0) { \
            Py_DECREF(module); \
            return NULL; \
    }   }

    /* capture interface */
    EXPORT_CONST( VBI_SLICED_NONE );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B_L10_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B_L25_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B );
    EXPORT_CONST( VBI_SLICED_VPS );
    EXPORT_CONST( VBI_SLICED_CAPTION_625_F1 );
    EXPORT_CONST( VBI_SLICED_CAPTION_625_F2 );
    EXPORT_CONST( VBI_SLICED_CAPTION_625 );
    EXPORT_CONST( VBI_SLICED_WSS_625 );
    EXPORT_CONST( VBI_SLICED_CAPTION_525_F1 );
    EXPORT_CONST( VBI_SLICED_CAPTION_525_F2 );
    EXPORT_CONST( VBI_SLICED_CAPTION_525 );
    EXPORT_CONST( VBI_SLICED_2xCAPTION_525 );
    EXPORT_CONST( VBI_SLICED_NABTS );
    EXPORT_CONST( VBI_SLICED_TELETEXT_BD_525 );
    EXPORT_CONST( VBI_SLICED_WSS_CPR1204 );
    EXPORT_CONST( VBI_SLICED_VBI_625 );
    EXPORT_CONST( VBI_SLICED_VBI_525 );
    EXPORT_CONST( VBI_SLICED_UNKNOWN );
    EXPORT_CONST( VBI_SLICED_ANTIOPE );
    EXPORT_CONST( VBI_SLICED_VPS_F2 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_A );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_C_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_D_625 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_B_525 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_C_525 );
    EXPORT_CONST( VBI_SLICED_TELETEXT_D_525 );

    /* VBI_CAPTURE_FD_FLAGS */
    EXPORT_CONST( VBI_FD_HAS_SELECT );
    EXPORT_CONST( VBI_FD_HAS_MMAP );
    EXPORT_CONST( VBI_FD_IS_DEVICE );

    /* proxy interface */
    EXPORT_CONST( VBI_PROXY_CLIENT_NO_TIMEOUTS );
    EXPORT_CONST( VBI_PROXY_CLIENT_NO_STATUS_IND );

    EXPORT_CONST( VBI_CHN_PRIO_BACKGROUND );
    EXPORT_CONST( VBI_CHN_PRIO_INTERACTIVE );
    EXPORT_CONST( VBI_CHN_PRIO_DEFAULT );
    EXPORT_CONST( VBI_CHN_PRIO_RECORD );

    EXPORT_CONST( VBI_CHN_SUBPRIO_MINIMAL );
    EXPORT_CONST( VBI_CHN_SUBPRIO_CHECK );
    EXPORT_CONST( VBI_CHN_SUBPRIO_UPDATE );
    EXPORT_CONST( VBI_CHN_SUBPRIO_INITIAL );
    EXPORT_CONST( VBI_CHN_SUBPRIO_VPS_PDC );

    EXPORT_CONST( VBI_PROXY_CHN_RELEASE );
    EXPORT_CONST( VBI_PROXY_CHN_TOKEN );
    EXPORT_CONST( VBI_PROXY_CHN_FLUSH );
    EXPORT_CONST( VBI_PROXY_CHN_NORM );
    EXPORT_CONST( VBI_PROXY_CHN_FAIL );
    EXPORT_CONST( VBI_PROXY_CHN_NONE );

    EXPORT_CONST( VBI_API_UNKNOWN );
    EXPORT_CONST( VBI_API_V4L1 );
    EXPORT_CONST( VBI_API_V4L2 );
    EXPORT_CONST( VBI_API_BKTR );

    EXPORT_CONST( VBI_PROXY_EV_CHN_GRANTED );
    EXPORT_CONST( VBI_PROXY_EV_CHN_CHANGED );
    EXPORT_CONST( VBI_PROXY_EV_NORM_CHANGED );
    EXPORT_CONST( VBI_PROXY_EV_CHN_RECLAIMED );
    EXPORT_CONST( VBI_PROXY_EV_NONE );

    /* demux */
    EXPORT_CONST( VBI_IDL_DATA_LOST );
    EXPORT_CONST( VBI_IDL_DEPENDENT );

    /* vt object */
    EXPORT_CONST( VBI_EVENT_NONE );
    EXPORT_CONST( VBI_EVENT_CLOSE );
    EXPORT_CONST( VBI_EVENT_TTX_PAGE );
    EXPORT_CONST( VBI_EVENT_CAPTION );
    EXPORT_CONST( VBI_EVENT_NETWORK );
    EXPORT_CONST( VBI_EVENT_TRIGGER );
    EXPORT_CONST( VBI_EVENT_ASPECT );
    EXPORT_CONST( VBI_EVENT_PROG_INFO );
    EXPORT_CONST( VBI_EVENT_NETWORK_ID );

    EXPORT_CONST( VBI_WST_LEVEL_1 );
    EXPORT_CONST( VBI_WST_LEVEL_1p5 );
    EXPORT_CONST( VBI_WST_LEVEL_2p5 );
    EXPORT_CONST( VBI_WST_LEVEL_3p5 );

    /* VT pages */
    EXPORT_CONST( VBI_LINK_NONE );
    EXPORT_CONST( VBI_LINK_MESSAGE );
    EXPORT_CONST( VBI_LINK_PAGE );
    EXPORT_CONST( VBI_LINK_SUBPAGE );
    EXPORT_CONST( VBI_LINK_HTTP );
    EXPORT_CONST( VBI_LINK_FTP );
    EXPORT_CONST( VBI_LINK_EMAIL );
    EXPORT_CONST( VBI_LINK_LID );
    EXPORT_CONST( VBI_LINK_TELEWEB );

    EXPORT_CONST( VBI_WEBLINK_UNKNOWN );
    EXPORT_CONST( VBI_WEBLINK_PROGRAM_RELATED );
    EXPORT_CONST( VBI_WEBLINK_NETWORK_RELATED );
    EXPORT_CONST( VBI_WEBLINK_STATION_RELATED );
    EXPORT_CONST( VBI_WEBLINK_SPONSOR_MESSAGE );
    EXPORT_CONST( VBI_WEBLINK_OPERATOR );

    EXPORT_CONST( VBI_SUBT_NONE );
    EXPORT_CONST( VBI_SUBT_ACTIVE );
    EXPORT_CONST( VBI_SUBT_MATTE );
    EXPORT_CONST( VBI_SUBT_UNKNOWN );

    EXPORT_CONST( VBI_BLACK );
    EXPORT_CONST( VBI_RED );
    EXPORT_CONST( VBI_GREEN );
    EXPORT_CONST( VBI_YELLOW );
    EXPORT_CONST( VBI_BLUE );
    EXPORT_CONST( VBI_MAGENTA );
    EXPORT_CONST( VBI_CYAN );
    EXPORT_CONST( VBI_WHITE );

    EXPORT_CONST( VBI_TRANSPARENT_SPACE );
    EXPORT_CONST( VBI_TRANSPARENT_FULL );
    EXPORT_CONST( VBI_SEMI_TRANSPARENT );
    EXPORT_CONST( VBI_OPAQUE );

    EXPORT_CONST( VBI_NORMAL_SIZE );
    EXPORT_CONST( VBI_DOUBLE_WIDTH );
    EXPORT_CONST( VBI_DOUBLE_HEIGHT );
    EXPORT_CONST( VBI_DOUBLE_SIZE );
    EXPORT_CONST( VBI_OVER_TOP );
    EXPORT_CONST( VBI_OVER_BOTTOM );
    EXPORT_CONST( VBI_DOUBLE_HEIGHT2 );
    EXPORT_CONST( VBI_DOUBLE_SIZE2 );

    EXPORT_CONST( VBI_NO_PAGE );
    EXPORT_CONST( VBI_NORMAL_PAGE );
    EXPORT_CONST( VBI_SUBTITLE_PAGE );
    EXPORT_CONST( VBI_SUBTITLE_INDEX );
    EXPORT_CONST( VBI_NONSTD_SUBPAGES );
    EXPORT_CONST( VBI_PROGR_WARNING );
    EXPORT_CONST( VBI_CURRENT_PROGR );
    EXPORT_CONST( VBI_NOW_AND_NEXT );
    EXPORT_CONST( VBI_PROGR_INDEX );
    EXPORT_CONST( VBI_PROGR_SCHEDULE );
    EXPORT_CONST( VBI_UNKNOWN_PAGE );

    /* search */
    EXPORT_CONST( VBI_ANY_SUBNO );
    EXPORT_CONST( VBI_SEARCH_ERROR );
    EXPORT_CONST( VBI_SEARCH_CACHE_EMPTY );
    EXPORT_CONST( VBI_SEARCH_CANCELED );
    EXPORT_CONST( VBI_SEARCH_NOT_FOUND );
    EXPORT_CONST( VBI_SEARCH_SUCCESS );

    /* export */
    EXPORT_CONST( VBI_PIXFMT_RGBA32_LE );
    EXPORT_CONST( VBI_PIXFMT_YUV420 );
    EXPORT_CONST( VBI_PIXFMT_PAL8 );

    EXPORT_CONST( VBI_OPTION_BOOL );
    EXPORT_CONST( VBI_OPTION_INT );
    EXPORT_CONST( VBI_OPTION_REAL );
    EXPORT_CONST( VBI_OPTION_STRING );
    EXPORT_CONST( VBI_OPTION_MENU );

    /* logging */
    EXPORT_CONST( VBI_LOG_ERROR );
    EXPORT_CONST( VBI_LOG_WARNING );
    EXPORT_CONST( VBI_LOG_NOTICE );
    EXPORT_CONST( VBI_LOG_INFO );
    EXPORT_CONST( VBI_LOG_DEBUG );
    EXPORT_CONST( VBI_LOG_DRIVER );
    EXPORT_CONST( VBI_LOG_DEBUG2 );
    EXPORT_CONST( VBI_LOG_DEBUG3 );

    return module;
}
