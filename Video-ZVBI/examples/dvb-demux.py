#!/usr/bin/python3
#
#  Copyright (C) 2020 T. Zoerner
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
#   Very simple test of the DVB PES de-multiplexer:
#   - reading stream from a DVB demux device
#   - calling de-multiplexer for each chunk of data
#   - forward sliced data
#
#   NOTE: Instead of reading from the device directly (and performing
#   device ioctl with hard-coded constants, which is prone to breakage)
#   you should use the Zvbi.Capture class which does that work for you.
#   Using the DvbDemux class directly only is useful when receiving a
#   stream from other sources (e.g. via socket from proxy on a remote
#   host).

import sys
import fcntl
import struct
import argparse
import Zvbi

#
# Callback invoked by DvbDemux.feed()
#
def sliced_handler(sliced_buf, pts, user_data=None):
    if opt.dump_sliced:
        ttx_lines = vps_lines = wss_lines = cc_lines = 0
        for (data, slc_id, line) in sliced_buf:
            if (slc_id & Zvbi.VBI_SLICED_TELETEXT_B):
                ttx_lines += 1
            elif (slc_id & Zvbi.VBI_SLICED_VPS):
                vps_lines += 1
            elif (slc_id & Zvbi.VBI_SLICED_WSS_625):
                wss_lines += 1
            elif (slc_id & (Zvbi.VBI_SLICED_CAPTION_625 | Zvbi.VBI_SLICED_CAPTION_525)):
                cc_lines += 1

        print("Received frame %.2f with %d lines: %d TTX, %d VPS, %d WSS, %d CC"
                % (pts, len(sliced_buf),
                   ttx_lines, vps_lines, wss_lines, cc_lines),
                file=sys.stderr)

    if opt.sliced:
        binary_sliced(sliced_buf)

    # return True so that feed function does not abort
    return True


#
# Dump packets in binary format equivalent examples/capture.py,
# thus suitable for reading by examples/decode.py
#
ServiceWidth = {
        Zvbi.VBI_SLICED_TELETEXT_B:  (42, 0),
        Zvbi.VBI_SLICED_CAPTION_625: (2, 1),
        Zvbi.VBI_SLICED_VPS:         (13, 2),
        Zvbi.VBI_SLICED_WSS_625:     (2, 3),
        Zvbi.VBI_SLICED_WSS_CPR1204: (3, 4),
        Zvbi.VBI_SLICED_CAPTION_525: (2, 7),
}

def binary_sliced(sliced_buf):
    outfile.write(bytes("%f\n" % sliced_buf.timestamp, 'ascii'))

    outfile.write(bytes([len(sliced_buf)]))

    for data, slc_id, line in sliced_buf:
        if ServiceWidth.get(slc_id) and (ServiceWidth.get(slc_id)[0] > 0):
            outfile.write(bytes([ServiceWidth.get(slc_id)[1],
                                 line & 0xFF,
                                 line >> 8]))
            data_len = ServiceWidth.get(slc_id)[0]
            outfile.write(data[0 : data_len])

    outfile.flush()


def main_func():
    if opt.use_callback:
        demux = Zvbi.DvbDemux(callback=sliced_handler)
    else:
        demux = Zvbi.DvbDemux()

    # open the DVB demux device
    try:
        dev = open(opt.device, "rb")
    except OSError as e:
        print("Failed to open device:", e, file=sys.stderr)
        sys.exit(1)

    # Configuring the PID in the DVB driver (in a very dirty way)
    # The following ioctl() call is equivalent this C code (Linux kernel 4.15.0):
    #   struct dmx_pes_filter_params filter
    #   filter.pid      = pid;
    #   filter.input    = DMX_IN_FRONTEND;
    #   filter.output   = DMX_OUT_TAP;
    #   filter.pes_type = DMX_PES_OTHER;
    #   filter.flags    = DMX_IMMEDIATE_START;
    #   ioctl (fd, DMX_SET_PES_FILTER, &filter))
    try:
        darg = struct.pack("@H2xIIII", opt.pid, 0, 1, 20, 4)
        fcntl.ioctl(dev.fileno(), 0x40146F2C, darg)
    except OSError as e:
        print("Failed to configure device via ioctl DMX_SET_PES_FILTER:", e, file=sys.stderr)
        sys.exit(1)

    # prepare for writing binary data to STDOUT
    if opt.sliced:
        global outfile
        outfile = open(sys.stdout.fileno(), "wb")

    # start reading data from the DVB device and demultiplex it
    # note the read function blocks until data is available
    while True:
        buf = dev.read(1024 * 8)
        if len(buf) == 0: break

        demux.feed(buf)

        if not opt.use_callback:
            for sliced_buf in demux:
                sliced_handler(sliced_buf, sliced_buf.timestamp)


def ParseCmdOptions():
    global opt
    parser = argparse.ArgumentParser(description='DVB de-multiplexer example')
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0", help="Path to video capture device")  # dev_name,
    parser.add_argument("--pid", type=int, default=0, help="Teletext channel PID for DVB")
    parser.add_argument("--use-callback", action='store_true', default=False, help="Use feed/callback API of DvbDemux class")
    parser.add_argument("--dump-sliced", action='store_true', default=False, help="Capture and all VBI services")
    parser.add_argument("--sliced", action='store_true', default=False, help="Write binary output, for piping into decode.py")   # bin_sliced,
    parser.add_argument("--verbose", action='store_true', default=False, help="Enable trace output in the library")
    opt = parser.parse_args()

    if opt.pid <= 0:
        print("Warning: without valid --pid parameter no reception is possible", file=sys.stderr)


# main
try:
    ParseCmdOptions()
    main_func()
except (KeyboardInterrupt, BrokenPipeError):
    pass
