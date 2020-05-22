#!/usr/bin/python3
#
#  libzvbi test of raw data capturing and raw decoding
#
#  Copyright (C) 2000-2002, 2004 Michael H. Schimek
#  Copyright (C) 2003 James Mastros
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
#   Example for the use of class Zvbi.RawDec. The script continuously
#   captures raw VBI data and displays the data as an animated gray-scale
#   image. Below this, the analog wave line of one selected video line is
#   plotted (i.e. essentially simulating an oscilloscope). For the
#   selected line, the resulting data from slicing is also shown if
#   decoding is successful.
#
#   (This script is loosely based on test/osc.c in the libzvbi package.)

import sys
import re
import argparse
from tkinter import *
import Zvbi

cap = None
par = None
rawdec = None
src_w = -1
src_h = -1
dst_h = 64
raw1 = None

tk = None
canvas = None
pgm = None
canvas_pgm = None
canvas_lid = None
dec_text = None

draw_row = None
draw_count = None

# -----------------------------------------------------------------------------
# from capture.c

def PIL(day, mon, hour, minute):
    return ((day << 15) + (mon << 11) + (hour << 6) + (minute << 0))


def decode_ttx(buf):
    packet_address = Zvbi.unham16p(buf)
    if packet_address < 0:  # hamming error
        return ""

    magazine = packet_address & 7
    packet = packet_address >> 3

    text = ("pg %x%02d >" % (magazine, packet))

    astr = Zvbi.unpar_str(buf, '.')
    astr = re.sub(r'[\x00-\x1F\x7F]', '.', astr.decode('ISO-8859-1'))

    return text + astr[0 : 42] + "<"


def dump_pil(pil):
    day = pil >> 15
    mon = (pil >> 11) & 0xF
    hour = (pil >> 6) & 0x1F
    minute = pil & 0x3F

    if pil == PIL(0, 15, 31, 63):
        text = " PDC: Timer-control (no PDC)"
    elif pil == PIL(0, 15, 30, 63):
        text = " PDC: Recording inhibit/terminate"
    elif pil == PIL(0, 15, 29, 63):
        text = " PDC: Interruption"
    elif pil == PIL(0, 15, 28, 63):
        text = " PDC: Continue"
    elif pil == PIL(31, 15, 31, 63):
        text = " PDC: No time"
    else:
        text = (" PDC: %05x, 200X-%02d-%02d %02d:%02d" %
                (pil, mon, day, hour, minute))

    return text


pr_label = ""
tmp_label = ""

def decode_vps(buf):
    global pr_label
    global tmp_label

    if not opt.dump_vps:
        return

    text = "VPS: "

    c = Zvbi.rev8(buf[1])

    if (c & 0x80):
        pr_label = tmp_label
        tmp_label = ""

    c &= 0x7F
    if (c >= 0x20) and (c < 0x7f):
        tmp_label += chr(c)
    else:
        tmp_label += '.'

    text += (" 3-10: %02x %02x %02x %02x %02x %02x %02x %02x (\"%s\")" %
             (buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], pr_label))

    pcs = buf[2] >> 6

    cni = (  ((buf[10] & 3) << 10)
           + ((buf[11] & 0xC0) << 2)
           + ((buf[8] & 0xC0) << 0)
           + (buf[11] & 0x3F))

    pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2)

    pty = buf[12]

    text += (" CNI: %04x PCS: %d PTY: %d " % (cni, pcs, pty))

    text += dump_pil(pil)

    return text

# End from capture.c
# -----------------------------------------------------------------------------

def draw(v=None):
    global raw1

    if v:
        # new frame available - handling freeze & single-stepping counter
        if draw_count.get() == 0:
            return
        if draw_count.get() > 0:
            draw_count.set(draw_count.get() - 1)

        # store a copy of the raw data (to allow navigating during freeze)
        raw1 = v

    if not raw1:
        return

    # display raw data as gray-scale image
    draw_pgm()

    if (draw_row.get() >= 0) and (draw_row.get() < src_h):
        field = (draw_row.get() >= par.count_a)

        if (par.start_a if (field == 0) else par.start_b) < 0:
            nchars = ("Row %d Line ? - " % draw_row.get())
            phys_line = -1

        elif field == 0:
            phys_line = draw_row.get() + par.start_a
            nchars = ("Row %d Line %d - " % (draw_row.get(), phys_line))

        else:
            phys_line = draw_row.get() - par.count_a + par.start_b
            nchars = ("Row %d Line %d - " % (draw_row.get(), phys_line))

        sliced_buf = rawdec.decode(raw1)

        # search the selected physical line in the slicer output
        for sld, slid, slin in sliced_buf:
            if slin == phys_line:
                # display decoder output
                draw_dec(nchars, sld, slid, slin)
                break
        else:
            # print "unkown"
            draw_dec(nchars)

        # plot that line as a wave line
        draw_plot()


def draw_pgm():
    global pgm

    pgm_str = bytearray("P5\n%d %d\n255\n" % (src_w, src_h), 'ascii') + raw1

    # image data is accepted only as bytes object, not bytearray
    pgm_str = bytes(pgm_str)

    new_pgm = tk.call("image", "create", "photo", "-data", pgm_str)

    canvas.itemconfigure(canvas_pgm, image=new_pgm)

    tk.call("image", "delete", pgm)
    pgm = new_pgm


def draw_dec(nchars, sld=None, slid=None, slin=None):
    if sld:
        if (slid & Zvbi.VBI_SLICED_TELETEXT_B):
            nchars += decode_ttx(sld)

        elif (slid & Zvbi.VBI_SLICED_VPS):
            nchars += decode_vps(sld)

        elif (slid & (Zvbi.VBI_SLICED_CAPTION_625 |
                      Zvbi.VBI_SLICED_CAPTION_525)):
            nchars += "Closed Caption"

        else:
            nchars += ("Sliced service 0x%X" % slid)
    else:
        nchars += "Unknown signal"

    dec_text.set(nchars)


def draw_plot():
    start = src_w * draw_row.get()

    Poly = []
    r = src_h + 0 + dst_h
    for i in range(0, src_w):
        y = raw1[start + i]
        Poly.append(i)
        Poly.append(r - y * dst_h//256)

    canvas.coords(canvas_lid, Poly)


def change_row(step):
    row = draw_row.get()
    if (step > 0):
        if (row + 1 < src_h):
            row += 1
    elif (step < 0):
        if (row > 0):
            row -= 1
    draw_row.set(row)

    draw()
    return "break"


def change_count(cnt):
    draw_count.set(cnt)


def resize_window(w, h):
    global canvas_lid
    global dst_h

    dst_h = h - (src_h + 10)
    if dst_h < src_h:
        dst_h = src_h

    # remove old grid
    for cid in canvas.find('overlapping', 0, src_h + 1,
                           src_w, src_h + 1 + dst_h*2):
        canvas.delete(cid)

    # paint grid in the new size
    x = 0
    while x < src_w:
       canvas.create_line(x, src_h + 10, x, src_h + 12+dst_h, fill='#AAAAAA')
       x += 10

    # create plot element
    canvas_lid = canvas.create_line(0, src_h+dst_h, 0, src_h+dst_h, fill='#ffffff')

    if draw_count == 0:
        draw(None)


def init_window():
    global tk
    global canvas
    global pgm
    global canvas_pgm
    global dec_text
    global draw_row
    global draw_count

    tk = Tk(className="osc")
    tk.wm_title('Raw capture & plot')

    dec_text = StringVar(tk, "")
    draw_row = IntVar(tk, 0)
    draw_count = IntVar(tk, -1)

    canvas = Canvas(tk, borderwidth=1, relief=SUNKEN, background='#000000',
                          height=src_h + 10 + dst_h, width=640,
                          scrollregion=[0, 0, src_w, src_h])
    canvas.pack(side=TOP, fill=X, expand=True)
    csb = Scrollbar(tk, orient=HORIZONTAL, takefocus=False, width=10, borderwidth=1,
                        command=canvas.xview)
    canvas.configure(xscrollcommand=csb.set)
    csb.pack(side=TOP, fill=X)
    canvas.pack(side=TOP, fill=BOTH, expand=True)

    canvas.bind('<Configure>', lambda e: resize_window(e.width, e.height))
    canvas.bind('<Key-q>', lambda e: sys.exit(0))
    canvas.bind('<Down>', lambda e: change_row(1))
    canvas.bind('<Up>', lambda e: change_row(-1))
    canvas.bind('<space>', lambda e: change_count(1))  # single-stepping
    canvas.bind('<Return>', lambda e: change_count(-1))  # live capture
    canvas.bindtags([canvas, 'all'])  # remove widget default bindings
    canvas.focus_set()

    label = Entry(tk, textvariable=dec_text, takefocus=0, width=50)
    label.pack(side=TOP, fill=X, anchor=W)

    f = Frame(tk)
    f_c = Checkbutton(f, text='Live capture',
                         offvalue=1, onvalue=-1, variable=draw_count)
    f_l = Label(f, text="Plot row:")
    f_s = Spinbox(f, from_=0, to=src_h - 1, width=5,
                     textvariable=draw_row, command=draw)
    f_c.pack(side=LEFT, padx=10, pady=5)
    f_l.pack(side=LEFT, padx=5)
    f_s.pack(side=LEFT, padx=5, pady=5)
    f.pack(side=TOP, anchor=W)

    pgm = tk.call("image", "create", "photo")
    canvas_pgm = canvas.create_image(0, 0, image=pgm, anchor=NW)


def cap_frame():
    # note: must use "read" and not "pull" since a copy of the data is kept in "raw1"
    try:
        raw_buf = cap.read_raw(10)
        draw(raw_buf)

    except Zvbi.CaptureError as e:
        if not opt.ignore_error:
            print("Capture error:", e, file=sys.stderr)
    except Zvbi.CaptureTimeout:
        # not an error/warning as this occurs regularly due to polling via 10ms timer
        pass

    tk.after(10, cap_frame)


def main_func():
    global cap
    global rawdec
    global par
    global src_w
    global src_h

    if opt.verbose > 1:
        vbi_capture_set_log_fp(cap, stderr)
        Zvbi.set_log_on_stderr(0)

    services = ( Zvbi.VBI_SLICED_VBI_525 | Zvbi.VBI_SLICED_VBI_625 |
                 Zvbi.VBI_SLICED_TELETEXT_B | Zvbi.VBI_SLICED_CAPTION_525 |
                 Zvbi.VBI_SLICED_CAPTION_625 | Zvbi.VBI_SLICED_VPS |
                 Zvbi.VBI_SLICED_WSS_625 | Zvbi.VBI_SLICED_WSS_CPR1204 )
    opt_strict = 0
    opt_buf_count = 5
    opt_scanning = (525 if opt.ntsc else (625 if opt.pal else 0))

    if opt.pid < 0:
        cap = Zvbi.Capture.Analog(opt.device, services=services, scanning=opt_scanning,
                                  buffers=opt_buf_count, strict=opt_strict, trace=opt.verbose)
    else:
        cap = Zvbi.Capture.Dvb(opt.device, dvb_pid=opt.pid, trace=opt.verbose)

    rawdec = Zvbi.RawDec(cap)
    rawdec.add_services(services, opt_strict)

    par = cap.parameters()
    if not par.sampling_format == Zvbi.VBI_PIXFMT_YUV420:
        print(("Unexpected sampling format:%d\n" +
               "In raw decoder parameters: %s\n" +
               "Likely the device does not support capturing raw data")
                % (par.sampling_format, str(par)), file=sys.stderr)
        sys.exit(1)

    src_w = par.bytes_per_line
    src_h = par.count_a + par.count_b

    init_window()

    # install a Tk event handler for capturing in the background
    #io = new IO::Handle
    #io.fdopen(cap.get_fd(), 'r')
    #tk.fileevent(io, 'readable', cap_frame)
    tk.after(10, cap_frame)

    # everything from here on is event driven
    tk.mainloop()


def ParseCmdOptions():
    global opt
    parser = argparse.ArgumentParser(description="Plot raw captured VBI video lines")
    parser.add_argument("--device", type=str, default="/dev/vbi0", help="Path to video capture device")
    parser.add_argument("--pid", type=int, default=0, help="Teletext channel PID for DVB")
    parser.add_argument("--pal", action='store_true', default=False, help="Force PAL video format")
    parser.add_argument("--ntsc", action='store_true', default=False, help="Force NTSC video format")
    parser.add_argument("--ignore-error", action='store_true', default=False, help="Ignore errors silently")
    parser.add_argument("--verbose", action='count', default=0, help="Enable trace in VBI library")
    opt = parser.parse_args()

    if (opt.pid == 0) and ("dvb" in opt.device):
        print("WARNING: DVB devices require --pid parameter", file=sys.stderr)


ParseCmdOptions()
main_func()
