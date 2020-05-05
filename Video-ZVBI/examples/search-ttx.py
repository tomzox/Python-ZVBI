#!/usr/bin/python3
#
#  libzvbi test of teletext search
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
# #Perl-Id: search-ttx.pl,v 1.3 2020/04/01 07:34:29 tom Exp tom #

import sys
import select
import argparse
import Zvbi

cr = '\r' if sys.stdout.isatty() else "\n"
last_pgno = -1
last_subno = -1

def pg_handler(pgtype, ev, user_data=None):
    print("%sPage %03x.%02x "
            % (cr, ev['pgno'], ev['subno'] & 0xFF),
          file=sys.stderr, end='')


def progress(pg, pat):
    (page, sub) = pg.get_page_no()
    print("%sSearching %03x.%02X for \"%s\" "
            % (cr, page, sub, pat),
          file=sys.stderr, end='')
    return True  # let search continue


def search(vtdec):
    global last_pgno
    global last_subno

    pat = input("\nEnter search pattern: ")
    if pat:
        #rand = [int(rand(1000))]
        #print "Search rand user data rand->[0]\n"
        srch = Zvbi.Search(vtdec, pat, casefold=False, regexp=False,
                           progress=progress, userdata=pat)
  
        try:
            while True:
                pg = srch.next(True)

                # match found
                (pgno, subno) = pg.get_page_no()
                if (last_pgno < 0) or (pgno != last_pgno) or (subno != last_subno):
                    print("Found match: %03X.%04X\n" % (pgno, subno))
                    print(pg.print_page(fmt='ascii').decode('utf-8'))
                    last_pgno = pgno
                    last_subno = subno
        except StopIteration:
            pass

        print("")


def main_func():
    global cap
    global vtdec

    if opt.v4l2:
        opt_buf_count = 5
        opt_strict = 0
        cap = Zvbi.Capture(opt.device, services=services,
                           buffers=opt_buf_count, strict=opt_strict, trace=opt.verbose)
    else:
        cap = Zvbi.Capture(opt.device, dvb_pid=opt.pid, trace=opt.verbose)

    vtdec = Zvbi.ServiceDec()
    vtdec.event_handler_register(Zvbi.VBI_EVENT_TTX_PAGE, pg_handler)

    print("Press RETURN to stop capture and enter a search pattern", file=sys.stderr)

    while True:
        ret, foo1, foo2 = select.select([sys.stdin], [], [], 0)
        if ret:
            input()
            search(vtdec)

        try:
            cap_data = cap.pull_sliced(10)
            vtdec.decode(cap_data.sliced_buffer)
        except Zvbi.CaptureError as e:
            if "timeout" not in str(e):
                print("Capture error: %s" % e)


def ParseCmdOptions():
    parser = argparse.ArgumentParser(description='Search teletext for an entered string')
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0")
    parser.add_argument("--pid", type=int, default=104)
    parser.add_argument("--v4l2", action='store_true', default=False)
    parser.add_argument("--verbose", "-v", action='store_true', default=False)
    return parser.parse_args()

opt = ParseCmdOptions()
main_func()
