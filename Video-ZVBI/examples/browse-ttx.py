#!/usr/bin/python3
#
#  Minimal level 2.5 teletext browser
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

# Description:
#
#   This script is an example for the use of classes Zvbi.Page and Zvbi.Export
#   for rendering teletext pages. The script captures teletext from a given
#   device and renders selected teletext pages in a simple GUI using TkInter.

import sys
import math
import argparse
import tkinter.font as tkf
from tkinter import *
import Zvbi

#
# Global variables
#
pxc = None
cap = None
vtdec = None
pg_disp = -1
pg_sched = 0x100

tk = None
canvas = None
img_xpm = None
redraw = False

#
# This callback is invoked by the teletext decoder for every ompleted page.
# The function updates the page number display on top of the window and
# updates the display if the scheduled page has been captured.
#
def pg_handler(pgtype, ev, user_data=None):
    pg_lab.set("Page %03x.%02x " % (ev.pgno, ev.subno & 0xFF))

    global redraw
    if ev.pgno == pg_sched:
        redraw = True

#
# This function is called every 10ms to capture VBI data.
# VBI frames are sliced and forwarded to the teletext decoder.
#
def cap_frame():
    global tid_polling
    global redraw

    try:
        sliced_buf = cap.pull_sliced(10)
        vtdec.decode(sliced_buf)
    except Zvbi.CaptureError as e:
        print("Capture error:", e, file=sys.stderr)
    except Zvbi.CaptureTimeout:
        pass

    if redraw:
        pg_display()
        redraw = False

    # reschedule the handler
    tid_polling = tk.after(10, cap_frame)

#
# This function is called once during start-up to initialize the
# device capture context and the teletext decoder
#
def cap_init():
    global cap
    global vtdec
    global tid_polling

    opt.verbose = False
    if opt.v4l2 or (opt.pid == 0 and not "dvb" in opt.device):
        opt_buf_count = 5
        opt_services = Zvbi.VBI_SLICED_TELETEXT_B
        opt_scanning = (525 if opt.ntsc else (625 if opt.pal else 0))
        opt_strict = 0
        cap = Zvbi.Capture.Analog(opt.device, services=opt_services, scanning=opt_scanning,
                                  buffers=opt_buf_count, strict=opt_strict, trace=opt.verbose)
    else:
        cap = Zvbi.Capture.Dvb(opt.device, dvb_pid=opt.pid, trace=opt.verbose)

    vtdec = Zvbi.ServiceDec()
    vtdec.event_handler_register(Zvbi.VBI_EVENT_TTX_PAGE, pg_handler)

    # install a Tk event handler for capturing in the background
    #tk.fileevent(cap.get_fd(), 'readable', cap_frame) # not supported in Python [lame]
    tid_polling = tk.after_idle(cap_frame)

#
# Render the teletext page using PPM (image) export
#
def pg_display_xpm():
    try:
        pg = vtdec.fetch_vt_page(pg_sched)
        (h, w) = pg.get_page_size()

        if False:
            # Pixmap class only exists in Perl -> XPM not supported in TkInter
            # suppress all XPM extensions because Pixmap can't handle them
            #ex = Zvbi.Export('xpm')
            #ex.option_set('creator', "")
            #ex.option_set('titled', 0)
            #ex.option_set('transparency', 0)
            #img_obj = Pixmap(data=img_data)
            pass

        elif False:
            # export page in PPM format
            ex = Zvbi.Export('ppm')
            img_data = ex.to_memory(pg)
            img_obj = tk.call("image", "create", "photo", "-data", img_data)

        else:
            # draw page to RGB canvas & convert to PPM
            # conversion of 8-bit palette image format into XPM is faster
            #fmt = Zvbi.VBI_PIXFMT_PAL8
            fmt = Zvbi.VBI_PIXFMT_RGBA32_LE
            img_canvas = pg.draw_vt_page(fmt=fmt)
            img_data = pg.canvas_to_ppm(img_canvas, fmt, aspect=1)
            img_obj = tk.call("image", "create", "photo", "-data", img_data)
            # FIXME PhotoImage() generates blank (black) image
            #img_obj = PhotoImage(data=img_data, name="ttx")

        canvas.delete('all')
        canvas.create_image(0, 0, anchor=NW, image=img_obj)
        #canvas.configure(width=img_obj.width(), height=img_obj.height())
        w = tk.call("image", "width", img_obj)
        h = tk.call("image", "height", img_obj)
        canvas.configure(width=w, height=h)

        global pg_disp
        pg_disp = pg_sched

    except Zvbi.ServiceDecError:
        pass


#
# Return color name for the given 24-bit R/G/B value
#
def vbi_rgba(v):
    return ("#%02X%02X%02X" % (v&0xff, (v>>8)&0xff, (v>>16)&0xff))


#
# Render the teletext page using page text/property query
#
def pg_display_text():
    try:
        pg = vtdec.fetch_vt_page(pg_sched)

        (rows, columns) = pg.get_page_size()
        pal = pg.get_page_color_map()
        text = pg.get_page_text(' ')
        prop = pg.get_page_text_properties()

        fh = tk.call("font", "metrics", font, "-linespace")
        fw = font.measure("0")

        canvas.delete('all')
        i = 0
        for row in range(0, rows):
            for col in range(0, columns):
                pp = prop[i]
                canvas.create_rectangle(col * fw, row * fh,
                                        (col+1) * fw, (row+1) * fh,
                                        outline="",
                                        fill=vbi_rgba(pal[(pp>>8) & 0xFF]))
                canvas.create_text(col * fw, row * fh,
                                   text=text[i], anchor=NW, font=font,
                                   fill=vbi_rgba(pal[pp & 0xFF]))
                i += 1

        canvas.configure(width=(columns * fw), height=(rows * fh))
        global pg_disp
        pg_disp = pg_sched

    except (Zvbi.ServiceDecError, Zvbi.PageError):
        pass


#
# This function is invoked out of the capture event handler when the page
# which is scheduled for display has been captured.
#
def pg_display():
    if mode_xpm.get():
        pg_display_xpm()
    else:
        pg_display_text()

#
# This callback is invoked when the user clicks into the teletext page.
# If there's a page number of FLOF link under the mouse pointer, the
# respective page is scheduled for display.
#
def pg_link(x, y):
    global redraw
    global pg_sched

    if not pg_disp == -1:
        try:
            pg = vtdec.fetch_vt_page(pg_disp)
            if mode_xpm.get():
                # note: char width 12, char height 10*2 due to scaling in XPM conversion
                fh = 20
                fw = 12
            else:
                fh = tk.call("font", "metrics", font, "-linespace")
                fw = font.measure("0")

            link = pg.resolve_link(x // fw, y // fh)
            if link.type == Zvbi.VBI_LINK_PAGE:
                pg_sched = link.pgno
                dec_entry.set("%03X" % pg_sched)
            redraw = True
        except (Zvbi.ServiceDecError, Zvbi.PageError):
            pass

#
# This callback is invoked when the user hits the left/right buttons
# (actually this is redundant to the +/- buttons in the spinbox)
#
def pg_plus_minus(off):
    if off >= 0:
        off = 1
    else:
        off = 0xF9999999

    global pg_sched
    pg_sched = Zvbi.add_bcd(pg_sched, off)
    if pg_sched < 0x100:
        pg_sched = 0x899
    dec_entry.set("%03X" % pg_sched)

    global redraw
    redraw = True

#
# This callback is invoked when the user edits the page number
#
def pg_change():
    global redraw
    global pg_sched
    try:
        v = int(dec_entry.get())
        pg_sched = Zvbi.dec2bcd(v)
        redraw = True
    except ValueError:
        pass

#
# This callback is invoked when the user hits the "TOP" button
# to display the TOP page table
#
def pg_top_index():
    global pg_sched
    global redraw

    pg_sched = 0x900
    dec_entry.set("900")
    redraw = True

#
# This function is called once during start-up to create the GUI.
#
def gui_init():
    global tk
    global canvas
    global dec_entry
    global mode_xpm
    global pg_lab
    global font
    global redraw

    tk = Tk(className="browse-ttx")
    tk.wm_title('Teletext Level 2.5 Demo')
    tk.wm_resizable(0, 0)

    dec_entry = StringVar(tk, '100')
    mode_xpm = BooleanVar(tk, True)
    pg_lab = StringVar(tk, "Page 000.00")
    font = tkf.Font(family='courier', size=12)

    # frame holding control widgets at the top of the window
    wid_f1 = Frame(tk)
    wid_f1_sp = Spinbox(wid_f1, from_=100, to=899, width=5,
                        textvariable=dec_entry, command=pg_change)
    wid_f1_sp.bind('<Return>', lambda e: pg_change())
    wid_f1_sp.pack(side=LEFT, anchor=W)
    wid_f1_lab = Label(wid_f1, textvariable=pg_lab)
    wid_f1_lab.pack(side=LEFT)
    wid_f1.pack(side=TOP, fill=X)
    wid_f1_but1 = Button(wid_f1, text="<<", command=lambda: pg_plus_minus(-1), padx=1)
    wid_f1_but2 = Button(wid_f1, text=">>", command=lambda: pg_plus_minus(1), padx=1)
    wid_f1_but3 = Button(wid_f1, text="TOP", command=lambda: pg_top_index(), padx=1)
    wid_f1_but1.pack(side=LEFT, anchor=E)
    wid_f1_but2.pack(side=LEFT, anchor=E)
    wid_f1_but3.pack(side=LEFT, anchor=E)
    wid_f1_mode = Checkbutton(wid_f1, variable=mode_xpm, text="XPM",
                              command=lambda:pg_change())
    wid_f1_mode.pack(side=LEFT, anchor=E)

    # canvas for displaying the teletext page as image
    canvas = Canvas(tk, borderwidth=0, relief='flat', background='#000000')
    canvas.bindtags([canvas, 'all'])  # remove widget default bindings
    canvas.bind('<Key-q>', lambda e: sys.exit(0))
    canvas.bind('<Button-1>', lambda e: pg_link(e.x, e.y))
    canvas.pack(fill=BOTH)
    canvas.focus_set()

    redraw = False


def main_func():
    # create & display GUI
    gui_init()

    # start capturing teletext
    cap_init()

    # everything from here on is event driven
    tk.mainloop()


def ParseCmdOptions():
    global opt
    parser = argparse.ArgumentParser(description="Plotter of captured raw VBI data")
    parser.add_argument("--device", type=str, default="/dev/dvb/adapter0/demux0", help="Path to video capture device")
    parser.add_argument("--pid", type=int, default=0, help="Teletext channel PID for DVB")
    parser.add_argument("--v4l2", action='store_true', default=False, help="Using analog driver interface")
    parser.add_argument("--pal", action='store_true', default=False, help="Assume PAL video norm (bktr driver only)")
    parser.add_argument("--ntsc", action='store_true', default=False, help="Assume NTSC video norm (bktr driver only)")
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
