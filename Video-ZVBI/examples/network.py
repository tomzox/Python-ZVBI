#!/usr/bin/python3
#
#  libzvbi network identification example.
#
#  Copyright (C) 2006 Michael H. Schimek
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
#   Example for the use of class Zvbi.ServiceDec, type
#   Zvbi.VBI_EVENT_NETWORK. This script shows how to identify a network
#   from data transmitted in XDS packets, Teletext packet 8/30 format 1
#   and 2, and VPS packets. The script captures from a device until the
#   currently tuned channel is identified by means of VPS, PDC et.al.
#
#   (This is a direct translation of examples/network.c in libzvbi.)

import sys
import argparse
import Zvbi

cap = None
vtdec = None
quit = False
opt = None

def ev_handler(ev_type, ev, user_data=None):
    # VBI_EVENT_NETWORK_ID is always sent when the decoder
    # receives a CNI. VBI_EVENT_NETWORK only if it can
    # determine a network name.

    if ev_type == Zvbi.VBI_EVENT_NETWORK:
        event_name = "VBI_EVENT_NETWORK"
    elif ev_type == Zvbi.VBI_EVENT_NETWORK_ID:
        event_name = "VBI_EVENT_NETWORK_ID"
    else:
        raise Exception("Unexpected event type: " + str(ev_type))

    if ev.name != "":
        # The network name is an ISO-8859-1 string (the API is quite old...) so
        # we convert it to locale encoding, nowadays usually UTF-8.
        # (Note this is done automatically within the Zvbi module)
        network_name = ev.name
    else:
        network_name = "unknown"

    # ASCII.
    if ev.call != "":
        call_sign = ev.call
    else:
        call_sign = "unknown"

    print(("%s: receiving: \"%s\" call sign: \"%s\" " +
           "CNI VPS: 0x%x 8/30-1: 0x%x 8/30-2: 0x%x") %
           (event_name,
            network_name,
            call_sign,
            ev.cni_vps,
            ev.cni_8301,
            ev.cni_8302))

    global quit
    if (((ev.cni_vps != 0) and opt.vps) or
        ((ev.cni_8301 != 0) and opt.p8301) or
        ((ev.cni_8302 != 0) and opt.p8302)):
        quit = True


def mainloop(services):
    # Should receive a CNI within two seconds, call sign within ten seconds(?).
    if (services & Zvbi.VBI_SLICED_CAPTION_525):
        n_frames = 11 * 30
    else:
        n_frames = 3 * 25

    for foo in range(0, n_frames):
        try:
            sliced_buf = cap.pull_sliced(10)
            vtdec.decode(sliced_buf)
        except Zvbi.CaptureError as e:
            print("Capture error:", e)
        except Zvbi.CaptureTimeout:
            pass

        if quit:
            return

    print("No network ID received or network unknown.")


def main_func():
    global cap
    global vtdec
    global opt

    if not opt.vps and not opt.p8301 and not opt.p8302:
        opt.vps = opt.p8301 = opt.p8302 = True

    services = (Zvbi.VBI_SLICED_TELETEXT_B |
                Zvbi.VBI_SLICED_VPS |
                Zvbi.VBI_SLICED_CAPTION_525)

    if opt.v4l2 or (opt.pid == 0 and not "dvb" in opt.device):
        opt_buf_count = 5
        opt_strict = 0
        cap = Zvbi.Capture.Analog(opt.device, services=services,
                                  buffers=opt_buf_count, strict=opt_strict, trace=opt.verbose)
    else:
        cap = Zvbi.Capture.Dvb(opt.device, dvb_pid=opt.pid, trace=opt.verbose)

    vtdec = Zvbi.ServiceDec()
    vtdec.event_handler_register( (Zvbi.VBI_EVENT_NETWORK |
                                   Zvbi.VBI_EVENT_NETWORK_ID), ev_handler )

    mainloop(services)


def ParseCmdOptions():
    global opt
    parser = argparse.ArgumentParser(description='Capture and print network identification')
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0", help="Path to video capture device")
    parser.add_argument("--pid", type=int, default=0, help="VBI channel PID for DVB")
    parser.add_argument("--v4l2", action='store_true', default=False, help="Using analog driver interface")
    parser.add_argument("--vps", action='store_true', default=False, help="Use VPS only")
    parser.add_argument("--p8301", action='store_true', default=False, help="Use packet 8/30/1 only")
    parser.add_argument("--p8302", action='store_true', default=False, help="Use packet 8/30/2 only")
    parser.add_argument("--verbose", action='store_true', default=False, help="Enable trace output in the library")
    opt = parser.parse_args()

    if opt.v4l2 and (opt.pid != 0):
        print("Options --v4l2 and --pid are multually exclusive", file=sys.stderr)
        sys.exit(1)
    if not opt.v4l2 and (opt.pid == 0) and ("dvb" in opt.device):
        print("WARNING: DVB devices require --pid parameter", file=sys.stderr)


try:
    ParseCmdOptions()
    main_func()
except KeyboardInterrupt:
    pass
