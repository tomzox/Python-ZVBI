#!/usr/bin/python3
#
#  libzvbi test
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

# Perl Id: export.pl,v 1.1 2007/11/18 18:48:35 tom Exp tom 
# ZVBI #Id: export.c,v 1.13 2005/10/04 10:06:11 mschimek Exp #

import sys
import re
import Zvbi

vtdec = None
ex = None
extension = "dat"
export_pgno = -1
quit = False
cr = '\r' if sys.stdout.isatty() else "\n"


def ev_handler(pg_type, ev, user_data=None):
    global quit

    print("%cPage %03x.%02x " % (cr, ev.pgno, ev.subno & 0xFF), end='', file=sys.stderr)

    if not export_pgno == -1 and not ev.pgno == export_pgno:
        return

    if sys.stderr.isatty():
        print("", file=sys.stderr)
    print("Saving page %03X..." % export_pgno, file=sys.stderr)

    # Fetching & exporting here is a bad idea,
    # but this is only a test.
    page = vtdec.fetch_vt_page(ev.pgno, ev.subno)

    try:
        if export_pgno == -1:
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

    if not export_pgno == -1:
        print("done", file=sys.stderr)
        quit = 1


def pes_mainloop(dx):
    while read(STDIN, buf, 2048):  # TODO
        buf_left = len(buf)

        while buf_left > 0:
            lines = dx.cor(sliced, 64, pts, buf, buf_left)
            if lines > 0:
                vtdec.decode(sliced, lines, pts / 90000.0)

            if quit:
                return

    print("\rEnd of stream, page %03x not found" % export_pgno, file=sys.stderr)


def dev_mainloop():
    global cap
    global vtdec

    if False:
        opt_device = "/dev/vbi0"
        opt_buf_count = 5
        opt_services = Zvbi.VBI_SLICED_TELETEXT_B
        opt_strict = 0
        opt_debug_level = 0

        cap = Zvbi.Capture(opt_device, services=opt_services, scanning=scanning,
                           buffers=opt_buf_count, strict=0, trace=opt_debug_level)

    else:
        opt_device = "/dev/dvb/adapter0/demux0"
        opt_pid = 104
        opt_debug_level = 0

        cap = Zvbi.Capture(opt_device, dvb_pid=opt_pid, trace=opt_debug_level)

    vtdec = Zvbi.ServiceDec()
    vtdec.event_handler_register(Zvbi.VBI_EVENT_TTX_PAGE, ev_handler)

    while not quit:
        try:
            cap_data = cap.pull_sliced(10)
            vtdec.decode(cap_data.sliced_buffer)
        except Zvbi.CaptureError as e:
            if "timeout" not in str(e):
                print("Capture error: %s" % e, file=sys.stderr)

    if not quit:
        print("\rEnd of stream, page %03x not found\n" % export_pgno, file=sys.stderr)


def main_func():
    global module
    global export_pgno
    global extension
    global ex

    if not len(sys.argv) == 3:
        print("Usage: %s \"module[;option=value]\" pgno <vbi_data >file" % sys.argv[0], file=sys.stderr)
        print("module eg. \"text\" or \"ppm\", pgno eg. 100 (hex) or \"all\"", file=sys.stderr)
        print("Run explist.py for a list of modules and options", file=sys.stderr)
        sys.exit(-1)

    #if sys.stdout.isatty():
    #    print("No vbi data on stdin", file=sys.stderr)
    #    sys.exit(-1)

    module = sys.argv[1]
    try:
        export_pgno = int(sys.argv[2], 16)
        if (export_pgno < 0x100) or (export_pgno > 0x8FF):
            print("Page number not in valid range 100-899:", sys.argv[1], file=sys.stderr)
            sys.exit(1)
    except ValueError:
        if sys.argv[2] == "all":
            export_pgno = -1
        else:
            print("Page number not numeric (or hexadecimal):", sys.argv[1], file=sys.stderr)
            sys.exit(1)

    ex = Zvbi.Export(module)

    xi = ex.info_export()
    extension = xi['extension']
    re.sub(r',.*', '', extension)
    xi = None

    #my infile = new IO::Handle
    #infile->fdopen(fileno(STDIN), "r")
    #my c = ord(infile->getc() || 1)
    #infile->ungetc(c)

    if False:
        dx = Zvbi.DvbDemux.pes_new()
        pes_mainloop(dx)
    else:
        dev_mainloop()


main_func()
