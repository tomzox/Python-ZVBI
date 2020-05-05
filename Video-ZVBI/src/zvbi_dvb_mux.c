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

#include "zvbi_dvb_mux.h"

#if 0
// ---------------------------------------------------------------------------
//  DVB multiplexer
// ---------------------------------------------------------------------------

typedef struct vbi_dvb_mux_obj_struct {
        vbi_dvb_mux *   ctx;
        SV *            mux_cb;
        SV *            mux_user_data;
} VbiDvb_MuxObj;

// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------

VbiDvb_MuxObj *
pes_new(callback=NULL, user_data=NULL)
        CV *            callback
        SV *            user_data
        CODE:
        Newxz(RETVAL, 1, VbiDvb_MuxObj);
        if (callback != NULL) {
                RETVAL->ctx = vbi_dvb_pes_mux_new(zvbi_xs_dvb_mux_handler, RETVAL);
                if (RETVAL->ctx != NULL) {
                        RETVAL->mux_cb = SvREFCNT_inc(callback);
                        RETVAL->mux_user_data = SvREFCNT_inc(user_data);
                }
        } else {
                RETVAL->ctx = vbi_dvb_pes_mux_new(NULL, NULL);
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
                RETVAL->ctx = vbi_dvb_ts_mux_new(pid, zvbi_xs_dvb_mux_handler, RETVAL);
                if (RETVAL->ctx != NULL) {
                        RETVAL->mux_cb = SvREFCNT_inc(callback);
                        RETVAL->mux_user_data = SvREFCNT_inc(user_data);
                }
        } else {
                RETVAL->ctx = vbi_dvb_ts_mux_new(pid, NULL, NULL);
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
        vbi_dvb_mux_delete(mx->ctx);
        Save_SvREFCNT_dec(mx->mux_cb);
        Save_SvREFCNT_dec(mx->mux_user_data);
        Safefree(mx);

void
vbi_dvb_mux_reset(mx)
        VbiDvb_MuxObj * mx
        CODE:
        vbi_dvb_mux_reset(mx->ctx);

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
                          vbi_dvb_mux_cor(mx->ctx,
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
                  vbi_dvb_mux_feed(mx->ctx,
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
        RETVAL = vbi_dvb_mux_get_data_identifier(mx->ctx);
        OUTPUT:
        RETVAL

vbi_bool
vbi_dvb_mux_set_data_identifier(mx, data_identifier)
        VbiDvb_MuxObj * mx
        unsigned int    data_identifier
        CODE:
        RETVAL = vbi_dvb_mux_set_data_identifier(mx->ctx, data_identifier);
        OUTPUT:
        RETVAL

unsigned int
vbi_dvb_mux_get_min_pes_packet_size(mx)
        VbiDvb_MuxObj * mx
        CODE:
        RETVAL = vbi_dvb_mux_get_min_pes_packet_size(mx->ctx);
        OUTPUT:
        RETVAL

unsigned int
vbi_dvb_mux_get_max_pes_packet_size(mx)
        VbiDvb_MuxObj * mx
        CODE:
        RETVAL = vbi_dvb_mux_get_max_pes_packet_size(mx->ctx);
        OUTPUT:
        RETVAL

vbi_bool
vbi_dvb_mux_set_pes_packet_size(mx, min_size, max_size)
        VbiDvb_MuxObj * mx
        unsigned int    min_size
        unsigned int    max_size
        CODE:
        RETVAL = vbi_dvb_mux_set_pes_packet_size(mx->ctx, min_size, max_size);
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
                          vbi_dvb_multiplex_sliced ( &p_buf, &buffer_left,
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
                                  vbi_dvb_multiplex_raw ( &p_buf, &buffer_left,
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

#endif // 0
