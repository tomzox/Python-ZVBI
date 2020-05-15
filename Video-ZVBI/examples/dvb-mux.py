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
    sys.stdout.write(pkg)
    print("wrote %d" % len(pkg))
    # return True, else multiplexing is aborted
    return True


def main_func():
    if opt.v4l2:
        opt_services = VBI_SLICED_TELETEXT_B_625
        #opt_services = VBI_SLICED_TELETEXT_A
        opt_buf_count = 5
        opt_strict = 0
        cap = Zvbi.Capture(opt.device, services=opt_services,
                           buffers=opt_buf_count, strict=opt_strict, trace=opt.verbose)
    else:
        cap = Zvbi.Capture(opt.device, dvb_pid=opt.pid, trace=opt.verbose)

    Zvbi.set_log_on_stderr(-1)

    # create DVB multiplexer
    if opt.use_feed:
        mux = Zvbi.DvbMux(pes=True, callback=feed_cb)
    else:
        mux = Zvbi.DvbMux(pes=True)

    while True:
        # read a sliced VBI frame
        try:
            sliced_buf = cap.pull_sliced(1000)
        except Zvbi.CaptureError as e:
            if "timeout" not in str(e):
                print("Capture error: %s" % e, file=sys.stderr)

        if not opt.use_feed:
            sliced_left = sliced_buffer.n_lines

            # pass sliced data to multiplexer
            buf_size = 2048
            buf_left = buf_size

            while (sliced_left > 0):
                print("timestamp %f <- %d+%f" % (buf_left, n_lines, sliced_left), file=sys.stderr)
                try:
                    # TODO: API
                    mux.cor(buf, buf_left, sliced_buf, sliced_left, opt_services, sliced_buf.timestamp*90000.0)

                except Zvbi.DvbMuxError:
                    # encoding error: dump the offending line
                    print("ERROR in line %d" % (n_lines-sliced_left), file=sys.stderr)

                    sset = sliced_buf[n_lines - sliced_left]
                    data = Zvbi.unpar_str(sset[0])
                    data = re.sub(r'[\x00-\x1F\x7F]', '.', data.decode('ISO-8859-1'))

                    print("MUX ERROR in line idx:%d ID:%d phys:line >%s<" %
                          ((n_lines - sliced_left), slc_id, data[0,42]), file=sys.stderr)

                    if sliced_left > 0:
                        sliced_left -= 1
                    if sliced_left == 0:
                        break

                #die if buf_left == 0   # buffer too small

            if buf:
                sys.stdout.write(buf, buf_size-buf_left)

            print("wrote %d" % (buf_size-buf_left))

        else:
            try:
                # TODO: API
                mux.feed(sliced_data, opt_services, sliced_buf.timestamp*90000.0)
            except Zvbi.DvbMuxError:
                print("ERROR in feed", file=sys.stderr)

    sys.exit(-1)


def ParseCmdOptions():
    parser = argparse.ArgumentParser(description='DVB muxing example')
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0")
    parser.add_argument("--pid", type=int, default=104)
    parser.add_argument("--v4l2", action='store_true', default=False)
    parser.add_argument("--use_feed", action='store_true', default=False)
    parser.add_argument("--verbose", "-v", action='store_true', default=False)
    return parser.parse_args()

opt = ParseCmdOptions()
main_func()
