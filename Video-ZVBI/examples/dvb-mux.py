#!/usr/bin/python3
#
#  Very simple test of the DVB PES multiplexer:
#  - reading sliced data from an analog capture device
#  - multiplexer output is written to STDOUT
#  - output can be decoded with examples/decode.pl --pes --all
#
#  Copyright (C) 2007,2020 Tom Zoerner
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

# Perl #Id: dvb-mux.pl,v 1.2 2020/04/01 07:31:19 tom Exp tom #

import sys
import argparse
import re
import Zvbi

# callback function invoked by Zvbi.DvbMux.feed()
def feed_cb(pkg):
    outfile.write(pkg)
    print("wrote %d" % len(pkg), file=sys.stderr)
    # return True, else multiplexing is aborted
    return True


def main_func():
    # prepare for writing binary data to STDOUT
    global outfile
    outfile = open(sys.stdout.fileno(), "wb")

    opt_services = (Zvbi.VBI_SLICED_TELETEXT_B |
                    Zvbi.VBI_SLICED_VPS |
                    Zvbi.VBI_SLICED_CAPTION_625 |
                    Zvbi.VBI_SLICED_WSS_625)

    if opt.v4l2:
        opt_buf_count = 5
        opt_strict = 0
        cap = Zvbi.Capture(opt.device, services=opt_services,
                           buffers=opt_buf_count, strict=opt_strict, trace=opt.verbose)
    else:
        cap = Zvbi.Capture(opt.device, dvb_pid=opt.pid, trace=opt.verbose)

    #Zvbi.set_log_on_stderr(-1)

    if not opt.use_static:
        # create DVB multiplexer
        if not opt.raw:
            if opt.use_feed:
                mux = Zvbi.DvbMux(pes=True, callback=feed_cb)
            else:
                mux = Zvbi.DvbMux(pes=True)
        else:
            if opt.use_feed:
                mux = Zvbi.DvbMux(pes=True, callback=feed_cb, raw_par=cap.parameters())
            else:
                mux = Zvbi.DvbMux(pes=True, raw_par=cap.parameters())

    while True:
        # read a sliced VBI frame
        try:
            if not opt.raw:
                sliced_buf = cap.pull_sliced(1000)
            else:
                raw_buf, sliced_buf = cap.pull(1000)
                if raw_buf is None:
                    print("Capture device does not support raw capture", file=sys.stderr)
                    sys.exit(1)

            try:
                if not opt.use_static:
                    if not opt.raw:
                        mux.feed(opt_services, sliced_buf=sliced_buf, pts=sliced_buf.timestamp*90000.0)
                    else:
                        mux.feed(opt_services, sliced_buf=sliced_buf, raw_buf=raw_buf, pts=sliced_buf.timestamp*90000.0)

                    if not opt.use_feed:
                        for pkg in mux:
                            feed_cb(pkg)
                else: # opt.use_static
                    pkg = bytearray(2024) # multiple of 46
                    pkg_left, sliced_left = \
                        Zvbi.DvbMux.multiplex_sliced(pkg, len(pkg),
                                                     sliced_buf, len(sliced_buf),
                                                     opt_services)
                    if sliced_left != 0:
                        print("WARN: not all sliced processed", sliced_left, file=sys.stderr)
                    if pkg_left == 0:
                        print("WARN: packet buffer overflow", file=sys.stderr)
                    # output is discarded (not a valid PES stream)

            except Zvbi.DvbMuxError as e:
                print("ERROR multiplexing:", e, file=sys.stderr)

        except Zvbi.CaptureError as e:
            if "timeout" not in str(e):
                print("Capture error: %s" % e, file=sys.stderr)


def ParseCmdOptions():
    global opt
    parser = argparse.ArgumentParser(description='DVB muxing example')
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0")
    parser.add_argument("--pid", type=int, default=104)
    parser.add_argument("--v4l2", action='store_true', default=False)
    parser.add_argument("--use-feed", action='store_true', default=False)
    parser.add_argument("--use-static", action='store_true', default=False)
    parser.add_argument("--raw", action='store_true', default=False)
    parser.add_argument("--verbose", "-v", action='store_true', default=False)
    opt = parser.parse_args()


# main
try:
    ParseCmdOptions()
    main_func()
except KeyboardInterrupt:
    sys.stdout.flush()
