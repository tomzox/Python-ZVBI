#!/usr/bin/python3
#
#  libzvbi test of page export in different formats
#
#  Copyright (C) 2000, 2001 Michael H. Schimek
#  Perl Port: Copyright (C) 2006, 2007 Tom Zoerner
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
#   Example for the use of export actions in class Zvbi.Export. The script
#   captures from a device until the page specified on the command line is
#   found and then exports the page in a requested format. Examples:
#
#     ./export.py text 100
#     ./export.py "png;reveal=1" 100 > page_100.png
#
#   Use ./explist.py for listing supported export formats (aka "modules")
#   and possible options. Note options are appended to the module name,
#   separated by semicolon as shown in the second example. (This is a
#   direct translation of test/export.c in the libzvbi package.)
#
#   (This is a direct translation of test/export.c in libzvbi.)

import sys
import re
import argparse
import Zvbi

opt = None
vtdec = None
ex = None
extension = "dat"
quit = False
cr = '\r' if sys.stderr.isatty() else "\n"


def ev_handler(pg_type, ev, user_data=None):
    global quit

    print("%cPage %03x.%02x " % (cr, ev.pgno, ev.subno & 0xFF), end='', file=sys.stderr)

    if not opt.page == -1 and not ev.pgno == opt.page:
        return

    if sys.stderr.isatty():
        print("", file=sys.stderr)
    print("Saving page %03X..." % opt.page, file=sys.stderr)

    # Fetching & exporting here is a bad idea,
    # but this is only a test.
    with vtdec.fetch_vt_page(ev.pgno, ev.subno) as page:
        try:
            if opt.page == -1:
                name = ("test-%03x-%02x.%s" % (ev.pgno, ev.subno, extension))
                ex.to_file(page, name)
            else:
                sys.stdout.flush()
                fd = sys.stdout.fileno()
                ex.to_stdio(page, fd)

        except Zvbi.ExportError as e:
            print("export failed:", e, file=sys.stderr)
            sys.exit(-1)

        str = ex.to_memory(page)
        #print(str)
        str = None

        if not opt.page == -1:
            print("done", file=sys.stderr)
            quit = 1


def pes_mainloop():
    infile = open(sys.stdin.fileno(), "rb")
    dvb = Zvbi.DvbDemux()

    while not quit:
        buf = infile.read(2048)
        if len(buf) == 0:
            break

        dvb.feed(buf)
        for sliced_buf in dvb:
            vtdec.decode(sliced_buf)

    if not quit:
        print("\rEnd of stream, page %03x not found" % opt.page, file=sys.stderr)


def dev_mainloop():
    global cap
    global vtdec

    if opt.v4l2 or (opt.pid == 0 and not "dvb" in opt.device):
        opt_services = Zvbi.VBI_SLICED_TELETEXT_B
        opt_buf_count = 5
        opt_strict = 0

        cap = Zvbi.Capture.Analog(opt.device, services=opt_services,
                                  buffers=opt_buf_count, strict=opt_strict,
                                  trace=opt.verbose)
    else:
        cap = Zvbi.Capture.Dvb(opt.device, dvb_pid=opt.pid, trace=opt.verbose)

    vtdec = Zvbi.ServiceDec()
    vtdec.event_handler_register(Zvbi.VBI_EVENT_TTX_PAGE, ev_handler)

    while not quit:
        try:
            sliced_buf = cap.pull_sliced(1000)
            vtdec.decode(sliced_buf)
        except Zvbi.CaptureError as e:
            print("Capture error:", e)
        except Zvbi.CaptureTimeout:
            print("Capture timeout:")

    if not quit:
        print("\rEnd of stream, page %03x not found\n" % opt.page, file=sys.stderr)


def main_func():
    global extension
    global ex

    try:
        ex = Zvbi.Export(opt.module)
    except Zvbi.ExportError as e:
        print("FATAL:", e, file=sys.stderr)
        exit(1)

    xi = ex.info_export()
    extension = xi['extension']
    re.sub(r',.*', '', extension)
    xi = None

    if opt.pes:
        pes_mainloop()
    else:
        dev_mainloop()


def ttx_pgno(val_str):
    val = int(val_str, 16)
    if (val < 0x100) or (val > 0x8FF):
        raise ValueError("Teletext page not in range 100-8FF")
    return val


def ParseCmdOptions():
    global opt

    if len(sys.argv) <= 1:
        print("Usage: %s \"module[;option=value]\" pgno >file" % sys.argv[0],
              "module eg. \"text\" or \"ppm\", pgno eg. 100 (hex) or \"all\"",
              "Run explist.py for a list of modules and options", sep='\n', file=sys.stderr)
        sys.exit(1)

    parser = argparse.ArgumentParser(description='Search teletext for an entered string')
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0", help="Path to video capture device")
    parser.add_argument("--pid", type=int, default=0, help="Teletext channel PID for DVB")
    parser.add_argument("--v4l2", action='store_true', default=False, help="Using analog driver interface")
    parser.add_argument("--pes", action='store_true', default=False, help="Read DVB PES input stream from STDIN")
    parser.add_argument("--verbose", action='store_true', default=False, help="Enable trace output in the library")
    parser.add_argument("module", type=str, action='store', help="Export format, optionally followed by option assignments: FORMAT;OPTION=VALUE")
    parser.add_argument("page", type=ttx_pgno, action='store', help="Page number to export")
    opt = parser.parse_args()

    if not opt.pes:
        if opt.v4l2 and (opt.pid != 0):
            print("Options --v4l2 and --pid are multually exclusive", file=sys.stderr)
            sys.exit(1)
        if not opt.v4l2 and (opt.pid == 0) and ("dvb" in opt.device):
            print("WARNING: DVB devices require --pid parameter", file=sys.stderr)
    else:
        if opt.v4l2 or (opt.pid != 0):
            print("Options -pes, --v4l2 and --pid are multually exclusive", file=sys.stderr)
            sys.exit(1)
        if sys.stdin.isatty():
            print("No PES data on stdin", file=sys.stderr)
            sys.exit(-1)

try:
    ParseCmdOptions()
    main_func()
except (KeyboardInterrupt, BrokenPipeError):
    pass
