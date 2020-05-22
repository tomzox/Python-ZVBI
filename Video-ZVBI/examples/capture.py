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

# Description:
#
#   Example for the use of class Zvbi.Capture. The script captures VBI
#   data from a device and slices it. The result can be dumped for the
#   various data services in form of hex-string plus roughly decoded text
#   (where applicable) for inspection of incoming data. Altnernatively,
#   output can be written to STDOUT for further processing (decoding) by
#   piping the data into one of the following example scripts. Call with
#   option --help for a list of options.
#
#   (This is a translation of test/capture.c in the libzvbi package.)

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
        print(" PDC: %05x, YYYY-%02d-%02d %02d:%02d" %
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


def decode_sliced(sliced_buf):
    if opt.dump_sliced:
        print("Sliced time: %f" % sliced_buf.timestamp)

        for data, slc_id, line in sliced_buf:
            print("%04x %3d > " % (slc_id, line), end='')

            for d in data:
                print("%02x " % d, end='')

            print(" ", end='')

            astr = Zvbi.unpar_str(data, '.')
            astr = re.sub(r'[\x00-\x1F\x7F]', '.', astr.decode('ISO-8859-1'))
            print(astr)

    for data, slc_id, line in sliced_buf:
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
ServiceWidth = {
        Zvbi.VBI_SLICED_TELETEXT_B:  (42, 0),
        Zvbi.VBI_SLICED_CAPTION_625: (2, 1),
        Zvbi.VBI_SLICED_VPS:         (13, 2),
        Zvbi.VBI_SLICED_WSS_625:     (2, 3),
        Zvbi.VBI_SLICED_WSS_CPR1204: (3, 4),
        Zvbi.VBI_SLICED_CAPTION_525: (2, 7),
}

last = 0.0

def binary_sliced(sliced_buf):
    global last

    ts = sliced_buf.timestamp if (last > 0.0) else 0.04
    outfile.write(bytes("%f\n" % sliced_buf.timestamp, 'ascii'))

    outfile.write(bytes([len(sliced_buf)]))

    for data, slc_id, line in sliced_buf:
        if ServiceWidth.get(slc_id) and (ServiceWidth.get(slc_id)[0] > 0):
            outfile.write(bytes([ServiceWidth.get(slc_id)[1],
                                 line & 0xFF,
                                 line >> 8]))
            data_len = ServiceWidth.get(slc_id)[0]
            outfile.write(data[0 : data_len])
            last = sliced_buf.timestamp

    outfile.flush()


def binary_ts_pes(user_data, packet, packet_size):
    outfile.write(packet[0 : packet_size])
    outfile.flush()
    return 1


def mainloop(cap):
    dump = (opt.dump_wss or opt.dump_vps or opt.dump_sliced)
    err_cnt = 0

    while True:
        try:
            if not opt.pull:
                raw_buf, sliced_buf = cap.read(1000)
            else:
                raw_buf, sliced_buf = cap.pull(1000)
            err_cnt = 0
        except Zvbi.CaptureError as e:
            print("Capture error:", e, file=sys.stderr)
            err_cnt += 1  # ignore occasional singular errors
            if err_cnt >= 2:
                break
        except Zvbi.CaptureTimeout:
            print("Capture timeout", file=sys.stderr)
            continue

        if False:
            print(".", file=outfile)
            outfile.flush()

        if dump:
            decode_sliced(sliced_buf)

        if opt.sliced:
            binary_sliced(sliced_buf)

#X#        if opt.pes or opt.ts:
#X#            # XXX shouldn't use system time
#X#            pts = timestamp * 90000.0
#X#            _vbi_dvb_mux_feed (mx, pts, sliced_buf, len(sliced_buf), -1); # service_set: all


#static const char short_options[] = "123cd:elnpr:stvPT"

def ParseCmdOptions():
    global opt
    parser = argparse.ArgumentParser(description='ZVBI capturing example')
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0", help="Path to video capture device")  # dev_name,
    parser.add_argument("--pid", type=int, default=0, help="Teletext channel PID for DVB")
    parser.add_argument("--ignore-error", action='store_true', default=False, help="Suppress warnings about device errors")
    parser.add_argument("--dump-ttx", action='store_true', default=False, help="Capture and dump teletext packets")
    parser.add_argument("--dump-xds", action='store_true', default=False, help="Capture and dump CC XDS packets")
    parser.add_argument("--dump-cc", action='store_true', default=False, help="Capture and dump CC packets")
    parser.add_argument("--dump-wss", action='store_true', default=False, help="Capture and dump WSS")
    parser.add_argument("--dump-vps", action='store_true', default=False, help="Capture and dump VPS data")
    parser.add_argument("--dump-sliced", action='store_true', default=False, help="Capture and all VBI services")
    #parser.add_argument("--pes", action='store_true', default=False)   # bin_pes,
    #parser.add_argument("--ts", action='store_true', default=False)   # bin_ts,
    parser.add_argument("--sliced", action='store_true', default=False, help="Write binary output, for piping into decode.py")   # bin_sliced,
    parser.add_argument("--read", action='store_true', default=True, help="Use read methods of Zvbi.Capture class")   # do_read,
    parser.add_argument("--pull", action='store_true', default=False, help="Use pull methods of Zvbi.Capture class")   # do_read,
    parser.add_argument("--strict", type=int, default=0, help="Use strict mode 0,1,2 for adding VBI services")
    #parser.add_argument("--desync", action='store_true', default=False)
    #parser.add_argument("--sim", action='store_true', default=False)   # do_sim,
    parser.add_argument("--pal", action='store_true', default=False, help="Assume PAL video norm (bktr driver only)")   # scanning_pal,
    parser.add_argument("--ntsc", action='store_true', default=False, help="Assume NTSC video norm (bktr driver only)")   # scanning_ntsc,
    #parser.add_argument("--v4l", action='store_true', default=False)   # api_v4l,
    parser.add_argument("--v4l2", action='store_true', default=False, help="Using analog driver interface")
    #parser.add_argument("--v4l2-read", action='store_true', default=False)   # api_v4l2, # FIXME
    #parser.add_argument("--v4l2-mmap", action='store_true', default=False)   # api_v4l2, # FIXME
    parser.add_argument("--verbose", action='store_true', default=False, help="Enable trace output in the library")
    opt = parser.parse_args()

    if opt.v4l2 and (opt.pid != 0):
        print("Options --v4l2 and --pid are multually exclusive", file=sys.stderr)
        sys.exit(1)
    if not opt.v4l2 and (opt.pid == 0) and ("dvb" in opt.device):
        print("WARNING: DVB devices require --pid parameter", file=sys.stderr)


def main_func():
    if opt.pal:
        scanning = 625
    elif opt.ntsc:
        scanning = 525
    else:
        scanning = 0

    if opt.dump_ttx or opt.dump_cc or opt.dump_xds:
        print("Teletext, CC and XDS decoding are no longer supported by this tool.\n" +
              "Run  ./capture --sliced | ./decode --ttx --cc --xds  instead.\n", file=sys.stderr)
        exit(-1)

    services = (Zvbi.VBI_SLICED_VBI_525 |
                Zvbi.VBI_SLICED_VBI_625 |
                Zvbi.VBI_SLICED_TELETEXT_B |
                Zvbi.VBI_SLICED_CAPTION_525 |
                Zvbi.VBI_SLICED_CAPTION_625 |
                Zvbi.VBI_SLICED_VPS |
                Zvbi.VBI_SLICED_WSS_625 |
                Zvbi.VBI_SLICED_WSS_CPR1204)

    if False: #opt.sim:
        #TODO cap = Zvbi.sim_new (scanning, services, 0, !opt.desync)
        #par = cap.parameters()
        pass
    elif opt.v4l2 or (opt.pid == 0 and not "dvb" in opt.device):
        cap = Zvbi.Capture.Analog(opt.device, services=services, scanning=scanning,
                                  dvb_pid=opt.pid, strict=opt.strict, trace=opt.verbose,
                                  buffers=5)
        par = cap.parameters()
    else:
        cap = Zvbi.Capture.Dvb(opt.device, dvb_pid=opt.pid, trace=opt.verbose)
        par = cap.parameters()

    if opt.verbose > 1:
        Zvbi.set_log_on_stderr(Zvbi.VBI_LOG_ERROR |
                               Zvbi.VBI_LOG_WARNING |
                               Zvbi.VBI_LOG_INFO)

    if opt.pid == -1:
        if par.sampling_format != Zvbi.VBI_PIXFMT_YUV420:
            print("Unexpected sampling format:%d in capture parameters"
                    % par.sampling_format, file=sys.stderr)
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

    global outfile
    outfile = open(sys.stdout.fileno(), "wb")

    mainloop(cap)

# main
try:
    ParseCmdOptions()
    main_func()
except (KeyboardInterrupt, BrokenPipeError):
    pass
