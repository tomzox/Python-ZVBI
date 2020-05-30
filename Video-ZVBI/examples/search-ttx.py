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

# Description:
#
#   Example for the use of class Zvbi.Search. The script captures and
#   caches teletext pages until the RETURN key is pressed, then prompts
#   for a search string. A search across teletext pages is started, and
#   the content of matching pages is printed on the terminal.

import os
import sys
import select
import argparse
import threading
import Zvbi

cr = '\r' if sys.stdout.isatty() else "\n"
last_pgno = -1
last_subno = -1
page_cache = dict()

def progress(pg, search_cnt):
    (page, sub) = pg.get_page_no()
    print("%sSearching page %03x.%02X " % (cr, page, sub),
          file=sys.stderr, end='')
    search_cnt[0] += 1
    return True  # let search continue


def search(vtdec):
    global last_pgno
    global last_subno

    match_cnt = 0
    search_cnt = [0]

    pat = input("\nEnter search pattern: ")
    if pat:
        #rand = [int(rand(1000))]
        #print "Search rand user data rand->[0]\n"
        srch = Zvbi.Search(vtdec, pat, casefold=False, regexp=False,
                           progress=progress, userdata=search_cnt)

        for pg in srch:
            (pgno, subno) = pg.get_page_no()
            if (last_pgno < 0) or (pgno != last_pgno) or (subno != last_subno):
                print("Found match: %03X.%04X\n" % (pgno, subno))
                print(pg.print_page(fmt='ascii').decode('utf-8'))
                last_pgno = pgno
                last_subno = subno
                match_cnt += 1

        print("%sFound %d matches on %d searched pages for \"%s\""
                % (cr, match_cnt, search_cnt[0], pat));


def pg_handler(page, subno, pg_cnt):
    print("%sCached %d pages (last: %03x.%02x) " %
            (cr, pg_cnt, page, subno), file=sys.stderr, end='')


# event handler registered with service decoder - invoked in capture thread
def capture_handler(pgtype, ev):
    global thr_page
    if pgtype == Zvbi.VBI_EVENT_TTX_PAGE:
        pgsub = (ev.pgno << 16) | ev.subno;
        # provide event data to main thread via shared memory
        with thr_lock:
            thr_page = pgsub
            page_cache[pgsub] = True;
        # dummy write to pipe is only used for waking up main thread
        os.write(thr_pipe[1], bytes([0]));


# capture loop, started in separate thread
# (i.e. running concurrently even during search pattern input & search)
def capture_loop():
    while True:
        try:
            sliced_buf = cap.pull_sliced(1000)
            vtdec.decode(sliced_buf)
        except Zvbi.CaptureError as e:
            print("Capture error:", e, file=sys.stderr)
        except Zvbi.CaptureTimeout:
            print("Capture timeout:", file=sys.stderr)


def main_func():
    global cap
    global vtdec
    global thr_inst
    global thr_pipe
    global thr_lock

    if opt.v4l2 or (opt.pid == 0 and not "dvb" in opt.device):
        opt_buf_count = 5
        opt_strict = 0
        cap = Zvbi.Capture.Analog(opt.device, services=services,
                                  buffers=opt_buf_count, strict=opt_strict, trace=opt.verbose)
    else:
        cap = Zvbi.Capture.Dvb(opt.device, dvb_pid=opt.pid, trace=opt.verbose)

    vtdec = Zvbi.ServiceDec()
    vtdec.event_handler_register(Zvbi.VBI_EVENT_TTX_PAGE, capture_handler)

    # instantiate capture thread & IPC
    thr_pipe = os.pipe2(os.O_NONBLOCK)
    thr_lock = threading.Lock()
    thr_inst = threading.Thread(target=capture_loop, daemon=True)
    thr_inst.start()

    print("Press RETURN for entering a search pattern", file=sys.stderr)

    while True:
        # check if STDIN is readable (i.e. RETURN key was pressed)
        ret = select.select([sys.stdin.fileno(), thr_pipe[0]], [], [], 0)[0]
        if sys.stdin.fileno() in ret:
            # discard one line of input (i.e. empty line caused by RETURN key)
            input()
            # ask for search string & perform search
            search(vtdec)
            print("\nPress RETURN for entering a search pattern", file=sys.stderr)

        elif thr_pipe[0] in ret:
            # drain pipe (content is unused)
            os.read(thr_pipe[0], 1000)
            with thr_lock:
                page = thr_page >> 16
                subno = thr_page & 0xFFFF
                pg_cnt = len(page_cache)
            pg_handler(page, subno, pg_cnt)


def ParseCmdOptions():
    global opt
    parser = argparse.ArgumentParser(description='Example for searching through teletext page content')
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0", help="Path to video capture device")
    parser.add_argument("--pid", type=int, default=0, help="Teletext channel PID for DVB")
    parser.add_argument("--v4l2", action='store_true', default=False, help="Using analog driver interface")
    parser.add_argument("--verbose", action='store_true', default=False, help="Enable trace output in the library")
    opt = parser.parse_args()

    if opt.v4l2 and (opt.pid != 0):
        print("Options --v4l2 and --pid are mutually exclusive", file=sys.stderr)
        sys.exit(1)
    if not opt.v4l2 and (opt.pid == 0) and ("dvb" in opt.device):
        print("WARNING: DVB devices require --pid parameter", file=sys.stderr)


try:
    ParseCmdOptions()
    main_func()
except KeyboardInterrupt:
    print("")  # needed due to progress output without trailing NL
