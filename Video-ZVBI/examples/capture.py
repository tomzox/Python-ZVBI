#!/usr/bin/python3
#
#  libzvbi test
#
#  Copyright (C) 2000-2006 Michael H. Schimek
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
#

# Perl Id: capture.pl,v 1.1 2007/11/18 18:48:35 tom Exp tom 
# ZVBI #Id: capture.c,v 1.26 2006/10/08 06:19:48 mschimek Exp #

import argparse
import sys
import re
import Zvbi

opt = None

#extern void
#vbi_capture_set_log_fp         (vbi_capture *          capture,
#                                FILE *                 fp)
#extern vbi_bool vbi_capture_force_read_mode

#
#  Dump
#

def PIL(day, mon, hour, minute):
    return ((day << 15) + (mon << 11) + (hour << 6) + (minute << 0))


def dump_pil(pil):
    day = pil >> 15
    mon = (pil >> 11) & 0xF
    hour = (pil >> 6) & 0x1F
    minute = pil & 0x3F

    if pil == PIL(0, 15, 31, 63):
        print(" PDC: Timer-control (no PDC)")
    elif pil == PIL(0, 15, 30, 63):
        print(" PDC: Recording inhibit/terminate")
    elif pil == PIL(0, 15, 29, 63):
        print(" PDC: Interruption")
    elif pil == PIL(0, 15, 28, 63):
        print(" PDC: Continue")
    elif pil == PIL(31, 15, 31, 63):
        print(" PDC: No time")
    else:
        print(" PDC: %05x, 200X-%02d-%02d %02d:%02d" %
              (pil, mon, day, hour, minute))


pr_label = ""
tmp_label = ""

def decode_vps(buf):
    global pr_label
    global tmp_label

    if not opt.dump_vps:
        return

    print("\nVPS:")

    c = Zvbi.rev8(buf[1])

    if (c & 0x80):
        pr_label = tmp_label
        tmp_label = ""

    c &= 0x7F
    if (c >= 0x20) and (c < 0x7f):
        tmp_label += chr(c)
    else:
        tmp_label += '.'

    print(" 3-10: %02x %02x %02x %02x %02x %02x %02x %02x (\"%s\")" %
          (buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], pr_label))

    pcs = buf[2] >> 6

    cni = (  ((buf[10] & 3) << 10)
           + ((buf[11] & 0xC0) << 2)
           + ((buf[8] & 0xC0) << 0)
           + (buf[11] & 0x3F))

    pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2)

    pty = buf[12]

    print(" CNI: %04x PCS: %d PTY: %d " % (cni, pcs, pty), end='')

    dump_pil(pil)


def decode_wss_625(buf):
    formats = (
            "Full format 4:3, 576 lines",
            "Letterbox 14:9 centre, 504 lines",
            "Letterbox 14:9 top, 504 lines",
            "Letterbox 16:9 centre, 430 lines",
            "Letterbox 16:9 top, 430 lines",
            "Letterbox > 16:9 centre",
            "Full format 14:9 centre, 576 lines",
            "Anamorphic 16:9, 576 lines"
    )
    subtitles = (
            "none",
            "in active image area",
            "out of active image area",
            "<invalid>"
    )

    if opt.dump_wss:
        g1 = buf[0] & 15
        parity = g1
        parity ^= parity >> 2
        parity ^= parity >> 1
        g1 &= 7

        print("WSS PAL: ", end='')
        if not (parity & 1):
            print("<parity error> ", end='')

        print(("%s; %s mode; %s colour coding; %s helper; "+
              "reserved b7=%d; %s Teletext subtitles; "+
              "open subtitles: %s; %s surround sound; "+
              "copyright %s; copying %s") %
              ( formats[g1],
               ("film" if (buf[0] & 0x10) else "camera"),
               ("MA/CP" if (buf[0] & 0x20) else "standard"),
               ("modulated" if (buf[0] & 0x40) else "no"),
               (buf[0] & 0x80) != 0,
               ("have" if (buf[1] & 0x01) else "no"),
               subtitles[(buf[1] >> 1) & 3],
               ("have" if (buf[1] & 0x08) else "no"),
               ("asserted" if (buf[1] & 0x10) else "unknown"),
               ("restricted" if (buf[1] & 0x20) else "not restricted")
             ))


def decode_wss_cpr1204(buf):
    if opt.dump_wss:
        poly = (1 << 6) + (1 << 1) + 1
        g = (buf[0] << 12) + (buf[1] << 4) + buf[2]
        crc = g | (((1 << 6) - 1) << (14 + 6))

        for j in range(14 + 6 - 1, -1, -1):
            if (crc & ((1 << 6) << j)):
                crc ^= poly << j

        print("WSS CPR >> g=%08x crc=%08x" % (g, crc), file=sys.stderr)


def decode_sliced(cap_data):
    if opt.dump_sliced:
        print("Sliced time: %f" % cap_data.timestamp)

        for data, slc_id, line in cap_data.sliced_buffer:
            print("%04x %3d > " % (slc_id, line), end='')

            for d in data:
                print("%02x " % d, end='')

            print(" ", end='')

            astr = Zvbi.unpar_str(data, '.')
            astr = re.sub(r'[\x00-\x1F\x7F]', '.', astr.decode('ISO-8859-1'))
            print(astr)

    for data, slc_id, line in cap_data.sliced_buffer:
        if slc_id == 0:
            pass # nop
        elif not (slc_id & Zvbi.VBI_SLICED_VPS) == 0:
          decode_vps(data)
        elif not (slc_id & Zvbi.VBI_SLICED_TELETEXT_B) == 0:
          pass # Use ./decode instead.
        elif not (slc_id & Zvbi.VBI_SLICED_CAPTION_525) == 0:
          pass # Use ./decode instead.
        elif not (slc_id & Zvbi.VBI_SLICED_CAPTION_625) == 0:
          pass # Use ./decode instead.
        elif not (slc_id & Zvbi.VBI_SLICED_WSS_625) == 0:
          decode_wss_625(data)
        elif not (slc_id & Zvbi.VBI_SLICED_WSS_CPR1204) == 0:
            decode_wss_cpr1204(data)
        else:
            print("Oops. Unhandled vbi service %08x\n" % slc_id, file=sys.stderr)

#
#  Sliced, binary
#

# hysterical compatibility
# (syntax note: "&" is required here to avoid auto-quoting of the bareword before "=>")
ServiceWidth = {
        Zvbi.VBI_SLICED_TELETEXT_B:  (42, 0),
        Zvbi.VBI_SLICED_CAPTION_625: (2, 1),
        Zvbi.VBI_SLICED_VPS:         (13, 2),
        Zvbi.VBI_SLICED_WSS_625:     (2, 3),
        Zvbi.VBI_SLICED_WSS_CPR1204: (3, 4),
        Zvbi.VBI_SLICED_CAPTION_525: (2, 7),
}

def binary_sliced(cap_data):
    last = 0.0

    if last > 0.0:
        print("%f\n%c" % (cap_data.timestamp - last, cap_data.sliced_lines))
    else:
        print("%f\n%c", 0.04 % cap_data.sliced_lines)

    for i in range (0, cap_data.sliced_lines):
        (data, slc_id, line) = Zvbi.get_sliced_line(cap_data.sliced_buffer, i)
        if ServiceWidth.get(slc_id) and (ServiceWidth.get(slc_id)[0] > 0):
            print("%c%c%c" % (ServiceWidth.get(slc_id)[1],
                              line & 0xFF,
                              line >> 8), end='')
#X#            outfile->write(data, ServiceWidth.get(slc_id)->[0])
            last = cap_data.timestamp

#X#    outfile->flush()


def binary_ts_pes(user_data, packet, packet_size):
#X#    outfile->write(packet, packet_size)
#X#    outfile->flush()
    return 1


def mainloop(cap):
    dump = (opt.dump_wss or opt.dump_vps or opt.dump_sliced)

    while True:
        if not opt.pull:
            cap_data = cap.read(4000)
        else:
            cap_data = cap.pull(4000)

#X#        if False:
#X#                $| = 1
#X#                outfile->print(".")

        if dump:
            decode_sliced(cap_data)

        if opt.sliced:
            binary_sliced(cap_data)

#X#        if opt.pes or opt.ts:
#X#            # XXX shouldn't use system time
#X#            pts = timestamp * 90000.0
#X#            _vbi_dvb_mux_feed (mx, pts, cap_data.sliced_buffer, cap_data.sliced_lines, -1); # service_set: all


#static const char short_options[] = "123cd:elnpr:stvPT"

def ParseCmdOptions():
    parser = argparse.ArgumentParser(description='ZVBI capturing example')
    parser.add_argument("--desync", action='store_true', default=False)
    parser.add_argument("--device", type=str, default="/dev/vbi")   # dev_name,
    parser.add_argument("--ignore-error", action='store_true', default=False)
    parser.add_argument("--pid", type=int, default=-1)
    parser.add_argument("--dump-ttx", action='store_true', default=False)
    parser.add_argument("--dump-xds", action='store_true', default=False)
    parser.add_argument("--dump-cc", action='store_true', default=False)
    parser.add_argument("--dump-wss", action='store_true', default=False)
    parser.add_argument("--dump-vps", action='store_true', default=False)
    parser.add_argument("--dump-sliced", action='store_true', default=False)
    parser.add_argument("--pes", action='store_true', default=False)   # bin_pes,
    parser.add_argument("--sliced", action='store_true', default=False)   # bin_sliced,
    parser.add_argument("--ts", action='store_true', default=False)   # bin_ts,
    parser.add_argument("--read", action='store_true', default=True)   # do_read,
    parser.add_argument("--pull", action='store_true', default=False)   # do_read,
    parser.add_argument("--strict", type=int, default=0)
    parser.add_argument("--sim", action='store_true', default=False)   # do_sim,
    parser.add_argument("--ntsc", action='store_true', default=False)   # scanning_ntsc,
    parser.add_argument("--pal", action='store_true', default=False)   # scanning_pal,
    parser.add_argument("--v4l", action='store_true', default=False)   # api_v4l,
    parser.add_argument("--v4l2", action='store_true', default=False)   # api_v4l2,
    parser.add_argument("--v4l2-read", action='store_true', default=False)   # api_v4l2, # FIXME
    parser.add_argument("--v4l2-mmap", action='store_true', default=False)   # api_v4l2, # FIXME
    parser.add_argument("--verbose", action='count', default=0)
    return parser.parse_args()

def main_func():
    if opt.pal:
        scanning = 625
    elif opt.ntsc:
        scanning = 525
    else:
        scanning = 0

    if opt.dump_ttx or opt.dump_cc or opt.dump_xds:
        print("Teletext, CC and XDS decoding are no longer supported by this tool.\n" +
              "Run  ./capture --sliced | ./decode --ttx --cc --xds  instead.\n", file=stderr)
        exit(-1)

    services = (Zvbi.VBI_SLICED_VBI_525 |
                Zvbi.VBI_SLICED_VBI_625 |
                Zvbi.VBI_SLICED_TELETEXT_B |
                Zvbi.VBI_SLICED_CAPTION_525 |
                Zvbi.VBI_SLICED_CAPTION_625 |
                Zvbi.VBI_SLICED_VPS |
                Zvbi.VBI_SLICED_WSS_625 |
                Zvbi.VBI_SLICED_WSS_CPR1204)

    if opt.sim:
        #TODO cap = Zvbi.sim_new (scanning, services, 0, !opt.desync)
        #par = cap.parameters()
        pass
    else:
        cap = Zvbi.Capture(opt.device, services=services, scanning=scanning,
                           dvb_pid=opt.pid, strict=opt.strict, trace=opt.verbose,
                           buffers=5)
        par = cap.parameters()

    if opt.verbose > 1:
        #TODO cap.set_log_fp (STDERR)
        pass

    if opt.pid == -1:
        if par["sampling_format"] != Zvbi.VBI_PIXFMT_YUV420:
            print("Unexpected sampling format %d" % par["sampling_format"])
            exit(-1)

#X#    if opt.pes:
#X#        mx = _vbi_dvb_mux_pes_new (0x10, # data_identifier
#X#                                   8 * 184, # packet_size
#X#                                   0, #TODO Zvbi.VBI_VIDEOSTD_SET_625_50,
#X#                                   \&binary_ts_pes)
#X#        die unless defined mx
#X#
#X#    elif opt.ts:
#X#        mx = _vbi_dvb_mux_ts_new (999, # pid
#X#                                  0x10, # data_identifier
#X#                                  8 * 184, # packet_size
#X#                                  0, #TODO Zvbi.VBI_VIDEOSTD_SET_625_50,
#X#                                  \&binary_ts_pes)
#X#        die unless defined mx
#X#
#X#    outfile = new IO::Handle
#X#    outfile->fdopen(fileno(STDOUT), "w")

    mainloop(cap)

# main
try:
    opt = ParseCmdOptions()
    main_func()
except KeyboardInterrupt:
    pass
