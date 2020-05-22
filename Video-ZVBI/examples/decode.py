#!/usr/bin/python3
#
#  zvbi-decode -- decode sliced VBI data using low-level
#                  libzvbi functions
#
#  Copyright (C) 2004, 2006 Michael H. Schimek
#  Perl Port: Copyright (C) 2007 Tom Zoerner
#  Python Port: Copyright (C) 2020 Tom Zoerner
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#/

# Description:
#
#   Example for the use of class Zvbi.ServiceDec. Decodes sliced VBI data
#   on stdin for different data services. Example for decoding teletext:
#
#     ./capture.py --sliced | ./decode.py --ttx
#
#   Call with option --help for a list of options.
#   (This is a direct translation of test/decode.c in the libzvbi package.)

import sys
import argparse
import struct
import re
import Zvbi

# Demultiplexers.
pfc = None
dvb = None
idl = None
xds = None

# Reader for old test/capture --sliced output.
infile = None
outfile = None
read_elapsed = False

# ------- slicer.c -----------------------------------------------------------
#
# Read one frame's worth of sliced data (written by decode.pl)
# from a file or pipe (not used in demo mode)
#
def read_sliced():
    global read_elapsed

    # Read timestamp: newline-terminated string
    buf = ""
    while True:
        b = infile.read(1)
        if len(b) == 0:  # EOF
            return None
        c = "%c" % b[0]
        if c == '\n':
            break
        buf += c

    # Time in seconds since last frame.
    try:
        dt = float(buf)
        if (dt < 0.0):
            dt = -dt
    except:
        print("invalid timestamp in input: " % dt, file=sys.stderr)
        sys.exit(1)

    timestamp = read_elapsed
    read_elapsed += dt

    # Read number of sliced lines
    n_lines = infile.read(1)[0]
    assert(n_lines >= 0) # "invalid line count in input"

    # Read sliced data: each record consists of:
    # - type ID (0..7)
    # - line index (LSB)
    # - line index (MSB)
    # - data x N: where N is implied by type ID
    sliced = []
    for n in range(0, n_lines):
        index = infile.read(1)[0]
        assert(index >= 0) # "invalid index"

        line = (infile.read(1)[0]
                   + 256 * infile.read(1)[0]) & 0xFFF

        if index == 0:
            slc_id = Zvbi.VBI_SLICED_TELETEXT_B
            data = infile.read(42)

        elif index == 1:
            slc_id = Zvbi.VBI_SLICED_CAPTION_625
            data = infile.read(2)

        elif index == 2:
            slc_id = Zvbi.VBI_SLICED_VPS
            data = infile.read(13)

        elif index == 3:
            slc_id = Zvbi.VBI_SLICED_WSS_625
            data = infile.read(2)

        elif index == 4:
            slc_id = Zvbi.VBI_SLICED_WSS_CPR1204
            data = infile.read(3)

        elif index == 7:
            slc_id = Zvbi.VBI_SLICED_CAPTION_525
            data = infile.read(2)

        else:
            print("Oops! Unknown data type index in sliced VBI file", file=sys.stderr)
            exit(1)

        sliced.append([data, slc_id, line])

    return (n_lines, timestamp, sliced)


def _vbi_pfc_block_dump(pgno, stream, app_id, block, binary):
    print("PFC pgno=%x stream=%u id=%u size=%u" %
          (pgno, stream, app_id, length(block)))

    if binary:
        # cannot mix binary write with text print within same stream in Python
        #sys.stdout.write(block)
        print(block)
    else:
        block = Zvbi.unpar_str(block, '.')
        block = re.sub(r'[\x00-\x1F\x7F]', '.', block.decode('ISO-8859-1'))
        # missing: insert \n every 75 chars

        print(block)


def put_cc_char(c1, c2):
    # All caption characters are representable in UTF-8
    c = ((c1 << 8) + c2) & 0x777F
    ucs2_str = Zvbi.caption_unicode(c)  # !to_upper

    print(ucs2_str)


def caption_command(line, c1, c2):
    print("CC line=%3u cmd 0x%02x 0x%02x " % (line, c1, c2), end='')

    if 0 == c1:
        print("null")
        return
    elif c2 < 0x20:
        print("invalid")
        return

    # Some common bit groups.

    ch = (c1 >> 3) & 1  # channel
    a7 = c1 & 7
    f = c1 & 1  # field
    b7 = (c2 >> 1) & 7
    u = c2 & 1  # underline

    if (c2 >= 0x40):
        row = (
               10,                # 0     # 0x1040
               -1,                # 1     # unassigned
               0, 1, 2, 3,        # 2     # 0x1140 ... 0x1260
               11, 12, 13, 14,    # 6     # 0x1340 ... 0x1460
               4, 5, 6, 7, 8, 9   # 10    # 0x1540 ... 0x1760
              )

        # Preamble Address Codes -- 001 crrr  1ri bbbu

        rrrr = a7 * 2 + ((c2 >> 5) & 1)

        if c2 & 0x1:
            print("PAC ch=%u row=%u column=%u u=%u" %
                    (ch, row[rrrr], b7 * 4, u))
        else:
            print("PAC ch=%u row=%u color=%u u=%u" %
                    (ch, row[rrrr], b7, u))
        return

    # Control codes -- 001 caaa  01x bbbu

    if a7 == 0:
        if (c2 < 0x30):
            mnemo_1 = (
                        "BWO", "BWS", "BGO", "BGS",
                        "BBO", "BBS", "BCO", "BCS",
                        "BRO", "BRS", "BYO", "BYS",
                        "BMO", "BMS", "BAO", "BAS"
                      )

            print("%s ch=%u" % (mnemo_1[c2 & 0xF], ch))
            return

    elif a7 == 1:
            if (c2 < 0x30):
                print("mid-row ch=%u color=%u u=%u" % (ch, b7, u), end='')
            else:
                print("special character ch=%u 0x%02x%02x='" % (ch, c1, c2), end='')
                put_cc_char(c1, c2)
                print("'")
            return

    elif a7 in (2,3):  # first & second group
        print("extended character ch=%u 0x%02x%02x='" % (ch, c1, c2), end='')
        put_cc_char(c1, c2)
        print("'")
        return

    elif a7 in (4,5): # f=0,1
        if (c2 < 0x30):
            mnemo_2 = (
                        "RCL", "BS",  "AOF", "AON",
                        "DER", "RU2", "RU3", "RU4",
                        "FON", "RDC", "TR",  "RTD",
                        "EDM", "CR",  "ENM", "EOC"
                      )

            print("%s ch=%u f=%u" % (mnemo_2[c2 & 0xF], ch, f))
            return

    elif a7 == 6: # reserved
        pass

    elif a7 == 7:
        if c2 == 0x21 or c2 == 0x22 or c2 == 0x23:
            print("TO%u ch=%u" % (c2 - 0x20, ch))
            return

        elif c2 == 0x2D:
            print("BT ch=%u" % ch)
            return

        elif c2 == 0x2E:
            print("FA ch=%u" % ch)
            return

        elif c2 == 0x2F:
            print("FAU ch=%u" % ch)
            return

    print("unknown")


def xds_cb(xds_class, xds_subclass, buf, user_data=None):
    #_vbi_xds_packet_dump(xp, stdout)
    print("XDS packet:", xds_class, xds_subclass, buf)

    return True  # no errors


def caption(inbuf, line):
    buffer = inbuf[0 : 1]

    if (opt.decode_xds and 284 == line):
        try:
            xds.feed(inbuf)
        except Zvbi.XdsDemuxError:
            print("Parity error in XDS data.")

    if (opt.decode_caption
        and (21 == line or 284 == line # NTSC
            or 22 == line)):

        c1 = Zvbi.unpar8(buffer[0])
        c2 = Zvbi.unpar8(buffer[1])

        if ((c1 | c2) < 0):
            print("Parity error in CC line=%u %s0x%02x %s0x%02x." %
                    (line,
                     (">" if (c1 < 0) else ""), buffer[0] & 0xFF,
                     (">" if (c2 < 0) else ""), buffer[1] & 0xFF))

        elif (c1 >= 0x20):
            print("CC line=%3u text 0x%02x 0x%02x '" %
                    (line, c1, c2), end='')

            # All caption characters are representable
            # in UTF-8, but not necessarily in ASCII.
            text = struct.pack("2B", c1, c2)

            # Error ignored.
            utf = Zvbi.iconv_caption(text, ord('?'))
            # suppress warnings about wide characters
            #utf = encode("ISO-8859-1", utf, Encode::FB_DEFAULT)

            print(utf + "'")

        elif (0 == c1 or c1 >= 0x10):
            caption_command(line, c1, c2)

        elif opt.decode_xds:
            print("CC line=%3u cmd 0x%02x 0x%02x " % (line, c1, c2), end='')
            if (0x0F == c1):
                print("XDS packet end", end='')
            else:
                print("XDS packet start/continue", end='')


#if 3 == VBI_VERSION_MINOR # XXX port me back
#
#static void
#dump_cni                        (vbi_cni_type           type,
#                                 unsigned int           cni)
#{
#        vbi_network nk
#        vbi_bool success
#
#        if (!opt.dump_network)
#                return
#
#        success = vbi_network_init (&nk)
#        if (!success)
#                no_mem_exit ()
#
#        success = vbi_network_set_cni (&nk, type, cni)
#        if (!success)
#                no_mem_exit ()
#
#        _vbi_network_dump (&nk, stdout)
#        putchar ('\n')
#
#        vbi_network_destroy (&nk)
#}
#
#endif # 3 == VBI_VERSION_MINOR

def dump_bytes(buffer, n_bytes):
    if opt.dump_bin:
        outfile.write(buffer, n_bytes)
        return

    if opt.dump_hex:
        hl = (("%02x" % x) for x in buffer)
        print(" ".join(hl))

    # For Teletext: Not all characters are representable
    # in ASCII or even UTF-8, but at this stage we don't
    # know the Teletext code page for a proper conversion.
    astr = Zvbi.unpar_str(buffer, '.')
    astr = re.sub(r'[\x00-\x1F\x7F]', '.', astr.decode('ISO-8859-1'))

    print(">"+ astr[0 : n_bytes] +"<")

#if 3 == VBI_VERSION_MINOR # XXX port me back
#
#static void
#packet_8301                     (const uint8_t          buffer[42],
#                                 unsigned int           designation)
#{
#        unsigned int cni
#        time_t time
#        int gmtoff
#        struct tm tm
#
#        if (!opt.decode_8301)
#                return
#
#        if (!vbi_decode_teletext_8301_cni (&cni, buffer)) {
#                printf (_("Error in Teletext "
#                          "packet 8/30 format 1 CNI.\n"))
#                return
#        }
#
#        if (!vbi_decode_teletext_8301_local_time (&time, &gmtoff, buffer)) {
#                printf (_("Error in Teletext "
#                          "packet 8/30 format 1 local time.\n"))
#                return
#        }
#
#        printf ("Teletext packet 8/30/%u cni=%x time=%u gmtoff=%d ",
#                designation, cni, (unsigned int) time, gmtoff)
#
#        gmtime_r (&time, &tm)
#
#        printf ("(%4u-%02u-%02u %02u:%02u:%02u UTC)\n",
#                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
#                tm.tm_hour, tm.tm_min, tm.tm_sec)
#
#        if (0 != cni)
#                dump_cni (VBI_CNI_TYPE_8301, cni)
#}
#
#static void
#packet_8302                     (const uint8_t          buffer[42],
#                                 unsigned int           designation)
#{
#        unsigned int cni
#        vbi_program_id pi
#
#        if (!opt.decode_8302)
#                return
#
#        if (!vbi_decode_teletext_8302_cni (&cni, buffer)) {
#                printf (_("Error in Teletext "
#                          "packet 8/30 format 2 CNI.\n"))
#                return
#        }
#
#        if (!vbi_decode_teletext_8302_pdc (&pi, buffer)) {
#                printf (_("Error in Teletext "
#                          "packet 8/30 format 2 PDC data.\n"))
#                return
#        }
#
#        printf ("Teletext packet 8/30/%u cni=%x ", designation, cni)
#
#        _vbi_program_id_dump (&pi, stdout)
#
#        putchar ('\n')
#
#        if (0 != pi.cni)
#                dump_cni (pi.cni_type, pi.cni)
#}
#
#endif # 3 == VBI_VERSION_MINOR

def page_function_clear_cb(pgno, stream, app_id, block, user_data=None):
    _vbi_pfc_block_dump(pgno, stream, app_id, block, opt.dump_bin)
    return True


def  idl_format_a_cb(buffer, flags, user_data=None):
    if not opt.dump_bin:
        print("IDL-A%s%s " %
                (" <data lost>" if (flags & Zvbi.VBI_IDL_DATA_LOST) else ""),
                (" <dependent>" if (flags & Zvbi.VBI_IDL_DEPENDENT) else ""), end='')

    dump_bytes(buffer, len(buffer))

    return True


def calc_spa(spa_length, ord_buf):
    spa = 0

    for i in range(0, spa_length):
        h = Zvbi.unham8(ord_buf[4 + i])
        spa |= (h << (4 * i))

    return spa


def packet_idl(buffer, channel):
    ord_buf = buffer[0 : 10]

    print("IDL ch=%u " % channel, end='')

    if channel == 0:
        print("IDL: unexpected channel 0")
        exit(1)

    elif (channel >= 4 and channel <= 12):
        print("(Low bit rate audio) ", end='')

        dump_bytes(buffer, 42)

    elif channel in (5, 6, 13, 14):
        pa = Zvbi.unham8(ord_buf[3])
        pa |= Zvbi.unham8(ord_buf[4]) << 4
        pa |= Zvbi.unham8(ord_buf[5]) << 8

        if (pa < 0):
            print("Hamming error in Datavideo packet-address byte.")
            return

        print("(Datavideo) pa=0x%x " % pa, end='')

        dump_bytes(buffer, 42)

    elif channel in (8, 9, 10, 11, 15):
        # ft: format type
        ft = Zvbi.unham8(ord_buf[2])
        if (ft < 0):
            print("Hamming error in IDL format " +
                  "A or B format-type byte.")
            return

        if (0 == (ft & 1)):
            # ial: interpretation and address length
            # spa: service packet address

            ial = Zvbi.unham8(ord_buf[3])
            if (ial < 0):
                print("Hamming error in IDL format " +
                          "A interpretation-and-address-" +
                          "length byte.")
                return

            spa_length = ial & 7
            if (7 == spa_length):
                print("(Format A?) ", end='')
                dump_bytes(buffer, 42)
                return

            spa = calc_spa(spa_length, ord_buf)

            if (spa < 0):
                print("Hamming error in IDL format" +
                        "A service-packet-address byte.")
                return

            print("(Format A) spa=0x%x " % spa, end='')

        elif (1 == (ft & 3)):
            # an: application number
            # ai: application identifier

            an = (ft >> 2)

            ai = Zvbi.unham8(ord_buf[3])
            if (ai < 0):
                print("Hamming error in IDL format " +
                      "B application-number byte.")
                return

            print("(Format B) an=%d ai=%d " % (an, ai), end='')

        dump_bytes(buffer, 42)

    else:
        dump_bytes(buffer, 42)


def teletext(buffer, line):
    ord_buf = buffer[0 : 42]

    if not pfc is None:
        try:
            pfc.feed(buffer)
        except Zvbi.PfcDemuxError:
            print("Error in Teletext PFC packet.")
            return

    if not (opt.decode_ttx or
            opt.decode_8301 or
            opt.decode_8302 or
            opt.decode_idl):
        return

    pmag = Zvbi.unham16p(buffer)
    if pmag < 0:
        print("Hamming error in Teletext packet number.")
        return

    magazine = pmag & 7
    if (0 == magazine):
        magazine = 8
    packet = pmag >> 3

    if (8 == magazine and 30 == packet):
        designation = Zvbi.unham8(ord_buf[2])
        if (designation < 0 ):
            print("Hamming error in Teletext packet 8/30 designation byte.")
            return

        if (designation >= 0 and designation <= 1):
#if 3 == VBI_VERSION_MINOR # XXX port me back
            #packet_8301(buffer, designation)
#endif
            return

        if (designation >= 2 and designation <= 3):
#if 3 == VBI_VERSION_MINOR # XXX port me back
            #packet_8302(buffer, designation)
#endif
            return

    if (30 == packet or 31 == packet):
        if opt.decode_idl:
            packet_idl(buffer, pmag & 15)
            #print("Teletext IDL packet %u/%2u " %(magazine, packet), end='')
            #dump_bytes(buffer, 42)
            return

    if opt.decode_ttx:
            print("Teletext line=%3u %x/%2u " %
                    (line, magazine, packet), end='')
            dump_bytes(buffer, 42)
            return

vps_pr_label = ["", ""]
vps_label = [bytearray([ord(' ')] * 16), bytearray([ord(' ')] * 16)]
vps_label_off = [0, 0]

def vps(inbuf, line):
    global vps_pr_label
    global vps_label
    global vps_label_off

    if opt.decode_vps:
        if opt.dump_bin:
            print("VPS line=%3u " % line, end='')
            outfile.write(inbuf, 13)
            return

        try:
            cni = Zvbi.decode_vps_cni(inbuf)
        except ZvbiError:
            print("Error in VPS packet CNI.")
            return

#if 3 == VBI_VERSION_MINOR
#           if (!vbi_decode_vps_pdc(&pi, buffer)) {
#                printf "Error in VPS packet PDC data.\n"
#                return
#
#           printf "VPS line=%3u ", line
#
#           _vbi_program_id_dump(&pi, stdout)
#
#           putchar ('\n')
#
#           if (0 != pi.cni)
#               dump_cni(pi.cni_type, pi.cni)
#else
        print("VPS line=%3u CNI=%x" % (line, cni))
#endif

    if opt.decode_vps_other:
        l = (line != 16)

        c = Zvbi.rev8(inbuf[1])

        if (c & 0x80):
            vps_pr_label[l] = vps_label[l][0 : vps_label_off[l]].decode('ascii')
            vps_label_off[l] = 0

        cp = c & 0x7F
        if (cp < 0x20 or cp >= 0x7f):
            cp = ord('.')

        vps_label[l][vps_label_off[l]] = cp

        vps_label_off[l] = (vps_label_off[l] + 1) % 16

        print(("VPS line=%3u bytes 3-10: " +
                "%02x %02x (%02x='%c') %02x %02x " +
                "%02x %02x %02x %02x (\"%s\")") %
               (line,
                inbuf[0], inbuf[1],
                c, cp,
                inbuf[2], inbuf[3],
                inbuf[4], inbuf[5], inbuf[6], inbuf[7],
                vps_pr_label[l]))


#if 3 == VBI_VERSION_MINOR # XXX port me back
#
#static void
#wss_625                         (const uint8_t          buffer[2])
#{
#        if (opt.decode_wss) {
#                vbi_aspect_ratio ar
#
#                if (!vbi_decode_wss_625(&ar, buffer)) {
#                        printf (_("Error in WSS packet.\n"))
#                        return
#                }
#
#                fputs ("WSS ", stdout)
#
#                _vbi_aspect_ratio_dump(&ar, stdout)
#
#                putchar ('\n')
#        }
#}
#
#endif # 3 == VBI_VERSION_MINOR

last_sample_time = 0.0
last_stream_time = 0

def decode_vbi(sliced, n_lines, sample_time, stream_time):
    global last_sample_time
    global last_stream_time

    if opt.dump_time:
        # Sample time: When we captured the data, in
        #              seconds since 1970-01-01 (gettimeofday()).
        # Stream time: For ATSC/DVB the Presentation TimeStamp.
        #              For analog the frame number multiplied by
        #              the nominal frame period (1/25 or
        #              1001/30000 s). Both given in 90000 kHz units.
        # Note this isn't fully implemented yet. */
        print("ST %f (%+f) PTS %lld (%+lld)" %
                (sample_time, sample_time - last_sample_time,
                 stream_time, stream_time - last_stream_time))

        last_sample_time = sample_time
        last_stream_time = stream_time

    for i in range(n_lines):
        (data, slc_id, line) = sliced[i]

        if ( (slc_id == Zvbi.VBI_SLICED_TELETEXT_B_L10_625) or
             (slc_id == Zvbi.VBI_SLICED_TELETEXT_B_L25_625) or
             (slc_id == Zvbi.VBI_SLICED_TELETEXT_B_625) ):
            teletext(data, line)

        elif ( (slc_id == Zvbi.VBI_SLICED_VPS) or
               (slc_id == Zvbi.VBI_SLICED_VPS_F2) ):
            vps(data, line)

        elif ( (slc_id == Zvbi.VBI_SLICED_CAPTION_625_F1) or
               (slc_id == Zvbi.VBI_SLICED_CAPTION_625_F2) or
               (slc_id == Zvbi.VBI_SLICED_CAPTION_625) or
               (slc_id == Zvbi.VBI_SLICED_CAPTION_525_F1) or
               (slc_id == Zvbi.VBI_SLICED_CAPTION_525_F2) or
               (slc_id == Zvbi.VBI_SLICED_CAPTION_525) ):
            caption(data, line)

        elif (slc_id == Zvbi.VBI_SLICED_WSS_625):
            #3 wss_625(data)
            pass

        elif (slc_id == Zvbi.VBI_SLICED_WSS_CPR1204):
            pass


def dvb_feed_cb(sliced_buf, n_lines, pts):
    if n_lines > 0:
        # pull all data lines out of the packed slicer buffer
        # since we want to process them by Python code
        # (something we'd normally like to avoid, as it's slow)
        # (see export.pl for an efficient use case)
        sliced = list(sliced_buf)

        decode_vbi(sliced, n_lines,
                   0,    # sample_time
                   pts)  # stream_time

    # return TRUE in case we're invoked as callback via feed()
    return True


def pes_mainloop():
    while True:
        buf = infile.read(2048)
        if len(buf) == 0:
            break

        dvb.feed(buf)
        for sliced_buf in dvb:
            dvb_feed_cb(sliced_buf, len(sliced_buf), sliced_buf.timestamp)

    print("\rEnd of stream", file=sys.stderr)


def old_mainloop():
    # one one frame's worth of sliced data from the input stream or file
    while True:
        sl = read_sliced()
        if sl:
            (n_lines, timestamp, sliced) = sl

            # pass the full frame's data to the decoder
            decode_vbi(sliced, n_lines, timestamp, 0)
        else:
            break

    print("\rEnd of stream", file=sys.stderr)


def ttx_pgno(val_str):
    val = int(val_str, 16)
    if (val < 0x100) or (val > 0x8FF):
        raise ValueError("Teletext page not in range 100-8FF")
    return val


def main_func():
    global pfc
    global idl
    global xds
    global dvb
    global infile
    global outfile

    if opt.all:
        opt.decode_ttx = True
        opt.decode_8301 = True
        opt.decode_8302 = True
        opt.decode_caption = True
        opt.decode_vps = True
        opt.decode_wss = True
        opt.decode_idl = True
        opt.decode_xds = True
        opt.pfc_pgno = 0x1DF

    if sys.stdin.isatty():
        print("No VBI data on standard input.", file=sys.stderr)
        sys.exit(1)

    if opt.pfc_pgno:
        pfc = Zvbi.PfcDemux(opt.pfc_pgno,
                            opt.pfc_stream,
                            page_function_clear_cb)

    if opt.idl_channel:
        idl = Zvbi.IdlDemux(opt.idl_channel,
                            opt.idl_address,
                            idl_format_a_cb)

    if opt.decode_xds:
        xds = Zvbi.XdsDemux(xds_cb)

    if opt.verbose:
        Zvbi.set_log_on_stderr(-1)

    outfile = open(sys.stdout.fileno(), "wb")
    infile = open(sys.stdin.fileno(), "rb")

    #c = ord(infile.read(1) || 1)
    #infile.fseek(0, 0)
    c=1

    if (0 == c or opt.source_is_pes):
        #dvb = Zvbi.DvbDemux(dvb_feed_cb)
        dvb = Zvbi.DvbDemux()
        if opt.verbose:
            dvb.set_log_fn
            dvb.set_log_fn(Zvbi.VBI_LOG_DEBUG, lambda l,c,m: print("DEMUX LOG", l, c, m, file=sys.stderr))
        pes_mainloop()
    else:
#if 2 == VBI_VERSION_MINOR # XXX port me
        #open_sliced_read(infile)
#endif
        old_mainloop()


def ParseCmdOptions():
    global opt
    desc = "ZVBI decoding examples\n" + \
           "Copyright (C) 2004, 2006 Michael H. Schimek\n" + \
           "This program is licensed under GPL 2 or later. NO WARRANTIES."
    parser = argparse.ArgumentParser(description=desc)
    # Input options:
    parser.add_argument("--pes", action='store_true', default=False, dest="source_is_pes", help="Source is a DVB PES stream [auto-detected]") # ATSC/DVB
    # Decoding options:
    parser.add_argument("--8301", action='store_true', default=False, dest="decode_8301", help="Teletext packet 8/30 format 1 (local time)")
    parser.add_argument("--8302", action='store_true', default=False, dest="decode_8302", help="Teletext packet 8/30 format 2 (PDC)")
    parser.add_argument("--all", action='store_true', default=False, dest="all", help="Enable ttx,8301,8302,cc,idl,vps,wssxds,pfc")
    parser.add_argument("--cc", action='store_true', default=False, dest="decode_caption", help="Closed Caption")
    parser.add_argument("--idl", action='store_true', default=False, dest="decode_idl", help="Any Teletext IDL packets (M/30, M/31)")
    parser.add_argument("--idl-ch", type=int, default=0, dest="idl_channel", help="Decode Teletext IDL format A data from channel N")
    parser.add_argument("--idl-addr", type=int, default=0, dest="idl_address", help="Decode Teletext IDL format A data from address NNN")
    parser.add_argument("--pfc-pgno", type=ttx_pgno, default=0, dest="pfc_pgno", help="Decode Teletext Page Function Clear data from page NNN (e.g. 1DF)")
    parser.add_argument("--pfc-stream", type=int, default=0, dest="pfc_stream", help="Decode Teletext Page Function Clear data from stream NN")
    parser.add_argument("--vps", action='store_true', default=False, dest="decode_vps", help="Decode VPS data unrelated to PDC")
    parser.add_argument("--vps-other", action='store_true', default=False, dest="decode_vps_other", help="Decode VPS data unrelated to PDC")
    parser.add_argument("--ttx", action='store_true', default=False, dest="decode_ttx", help="Decode any Teletext packet")
    parser.add_argument("--wss", action='store_true', default=False, dest="decode_wss", help="Wide Screen Signalling")
    parser.add_argument("--xds", action='store_true', default=False, dest="decode_xds", help="Decode eXtended Data Service (NTSC line 284)")
    # Modifying options:
    parser.add_argument("--hex", action='store_true', default=False, dest="dump_hex", help="With -t dump packets in hex and ASCII, otherwise only ASCII")
    parser.add_argument("--network", action='store_true', default=False, dest="dump_network", help="With -1, -2, -v decode CNI and print available information about the network")
    parser.add_argument("--bin", action='store_true', default=False, dest="dump_bin", help="With -t, -p, -v dump data in binary format instead of ASCII")
    parser.add_argument("--time", action='store_true', default=False, dest="dump_time", help="Dump capture timestamps")
    # misc
    parser.add_argument("--verbose", action='store_true', default=False, help="Enable trace in VBI library")
    parser.add_argument("--version", action='store_true', default=False, dest="version", help="Print the program version and exit")
    opt = parser.parse_args()



try:
    ParseCmdOptions()
    main_func()
except KeyboardInterrupt:
    pass
