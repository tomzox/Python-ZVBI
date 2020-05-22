#!/usr/bin/python3
#
#  Rudimentary render code for Closed Caption (CC) test.
#
#  Copyright (C) 2000, 2001 Michael H. Schimek
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

#
# Description:
#
#   Example for the use of class Zvbi.ServiceDec, type Zvbi.VBI_EVENT_CAPTION.
#   When called without an input stream, the application opens a GUI displaying
#   a demo messages sequente (character sets etc.) for debugging the decoder.
#   For displaying live CC streams you can use the following:
#
#      ./capture.py --sliced | ./caption.py
#
#   The buttons on top of the GUI switch between Closed Caption channels 1-4
#   and Text channels 1-4.
#
#   (This script is a translation of test/caption.c in the libzvbi package,
#   albeit based on TkInter here.)

import sys
import argparse
import struct
import tkinter.font as tkf
from tkinter import *
import select
import Zvbi

# declare global variables
vbi = None
pgno = None
dx = None
infile = None
read_elapsed = 0
tk = None
canvas = None

# constants
DISP_WIDTH   = 640
DISP_HEIGHT  = 480
CELL_WIDTH   = 16
CELL_HEIGHT  = 26
DISP_X_OFF   = 40
DISP_Y_OFF   = 45

# hash array to hold IDs of rolling text rows in the canvas
shift = dict()
shift_step = 2

# canvas background color - visible where video would look through
COLORKEY = "#80FF80"

#
#  Remove one row's text content (i.e. all pixmaps in the row's area)
#
def draw_blank(row, col, n_cols):
    for cid in canvas.find( "overlapping",
                            col * CELL_WIDTH + 1 + DISP_X_OFF,
                            row * CELL_HEIGHT + 1 + DISP_Y_OFF,
                            (col + n_cols) * CELL_WIDTH - 2 + DISP_X_OFF,
                            (row + 1) * CELL_HEIGHT - 2 + DISP_X_OFF ):

        img = canvas.itemcget(cid, "image")

        # remove the pixmap from the canvas
        canvas.delete(cid)

        # destroy the image (important to free the image's memory)
        tk.call("image", "delete", img)


def is_transp(v):
    return ((v >> 16) & 0x0F) == Zvbi.VBI_TRANSPARENT_SPACE


#
#  Draw one row of text
#
def draw_row(pg, row):
    (rows, columns) = pg.get_page_size()
    prop = pg.get_page_text_properties()

    # first remove all old text in the row
    draw_blank(row, 0, columns)

    col = 0
    while col < columns:
        # skip transparent characters
        if is_transp(prop[row * columns + col]):
            col += 1
            continue

        # count number of subsequent non-transparent characters
        # (required as optimisation - drawing each char separately is very slow)
        i = col + 1
        while (i < columns) and not is_transp(prop[row * columns + i]):
            i += 1

        # create RGBA image of the character sequence
        fmt = Zvbi.VBI_PIXFMT_PAL8
        vbi_canvas = pg.draw_cc_page(col, row, i - col, 1, fmt=fmt)

        # convert into a pixmap via XPM
        img = tk.call("image", "create", "photo", "-data",
                      pg.canvas_to_ppm(vbi_canvas, fmt, aspect=1))

        # finally, display the pixmap in the canvas
        cid = canvas.create_image(col * CELL_WIDTH + DISP_X_OFF,
                                  row * CELL_HEIGHT + DISP_Y_OFF,
                                  anchor=NW, image=img)
        col = i


#
#  Timer event for rolling text rows
#
def bump(snap):
    renew = False

    for cid in list(shift.keys()):  # copy key list to allow modification during iteration
        d = shift[cid]
        if snap:
            step = d
        else:
            step = (d if (d < shift_step) else shift_step)

        canvas.move(cid, 0, 0 - step)

        shift[cid] -= step
        if shift[cid] <= 0:
            del shift[cid]
        else:
            renew = True

    if renew:
        tk.after(20 * shift_step, lambda: bump(0))


#
#  Scroll a range of rows upwards
#
def roll_up(pg, first_row, last_row):
    if True:  # ---- soft scrolling ----

        # snap possibly still moving rows into their target positions
        bump(1)

        for cid in canvas.find("overlapping",
                               0,
                               first_row * CELL_HEIGHT + 1 + DISP_Y_OFF,
                               DISP_WIDTH,
                               (last_row + 1) * CELL_HEIGHT - 1 + DISP_Y_OFF):
            shift[cid] = CELL_HEIGHT

            # start time to trigger smooth scrolling
            tk.after(20 + 20 * shift_step, lambda: bump(0))

    else: # ---- jumpy scrolling ----
        for cid in canvas.find("overlapping",
                               0,
                               first_row * CELL_HEIGHT + DISP_Y_OFF,
                               DISP_WIDTH,
                               (last_row + 1) * CELL_HEIGHT - 1 + DISP_Y_OFF):

            canvas.move(cid, 0, 0 - CELL_HEIGHT)


#
#  Update a range of text rows
#
def render(pg, y0, y1):
    # snap possibly still moving rows into their target positions
    bump(1)

    for row in range(y0, y1+1):
        draw_row(pg, row)


#
#  Clear all text on-screen
#
def clear():
    for cid in canvas.find("all"):
        img = canvas.itemcget(cid, "image")
        canvas.delete(cid)
        tk.call("image", "delete", img)


#
#  Callback invoked by the VBI decoder when a new CC line is available
#
def cc_handler(type, ev):
    if (pgno.get() != -1) and (ev.pgno != pgno.get()):
        return

    # Fetching & rendering in the handler
    # is a bad idea, but this is only a test

    pg = vbi.fetch_cc_page(ev.pgno)

    (rows, columns) = pg.get_page_size()
    (y0, y1, roll) = pg.get_page_dirty_range()

    if (abs (roll) > rows):
        clear()
    elif (roll == -1):
        #draw_blank(y0, 0, columns)
        roll_up(pg, y0+1, y1)
    else:
        render(pg, y0, y1)


#
#  Callback bound to CC channel changes
#
def reset():
    try:
        pg = vbi.fetch_cc_page(pgno.get())
        (rows, columns) = pg.get_page_size()
        render(pg, 0, rows - 1)
    except Zvbi.PageError:
        clear()


#
#  Create the GUI
#
def init_window():
    global tk
    global canvas
    global pgno

    tk = Tk()
    tk.wm_title('Caption decoder')

    pgno = IntVar(tk, 1)

    # at the top: button array to switch CC channels
    f = Frame(tk)
    b = Label(f, text="Page:")
    b.pack(side=LEFT)
    for i in range(1, 8+1):
        b = Radiobutton(f, text=i, value=i, variable=pgno, command=lambda: reset())
        b.pack(side=LEFT)

    f.pack(side=TOP)

    # canvas to display CC text (as pixmaps)
    canvas = Canvas(tk, borderwidth=1, relief="sunken",
                    background=COLORKEY,
                    height=DISP_HEIGHT, width=DISP_WIDTH)
    canvas.pack(side=TOP)
    canvas.focus_set()


#
#  Feed caption from live stream or file with sample data
#
def pes_mainloop():
    while True:
        buf = infile.read(2048)
        bytes_left = len(buf)
        if bytes_left == 0:
            break

        while (bytes_left > 0):
            n_lines = dx.cor(sliced, 64, pts, buf, bytes_left)
            if (n_lines > 0):
                vbi.decode_bytes(sliced, n_lines, pts / 90000.0)

        tk.after(20, pes_mainloop)
        return

    print("\rEnd of stream", file=sys.stderr)


#
#  Forward sliced data read from STDIN to service decoder
#
def old_mainloop():
    # avoid blocking in read when no data available: keep Tk mainloop alive
    if select.select([infile], [], [], 0)[0]:
        # one one frame's worth of sliced data from the input stream or file
        sl = read_sliced()
        if sl:
            (n_lines, timestamp, sliced) = sl

            # pack the read data into the normal slicer output format
            # (i.e. the format delivered by the librarie's internal slicer)
            REC_LEN = 4+4+56
            buf = bytearray(n_lines * REC_LEN)
            line_idx = 0
            for slc_id, line, data in sliced:
                struct.pack_into("=LL", buf, line_idx*REC_LEN, slc_id, line)
                d_idx = 0
                for d in data:
                    buf[line_idx*REC_LEN + 4+4 + d_idx] = d
                    d_idx += 1
                line_idx += 1

            # pass the full frame's data to the decoder
            vbi.decode_bytes(buf, n_lines, timestamp)

            tk.after(20, old_mainloop)

        else:
            print("\rEnd of stream", file=sys.stderr)
    else:
        tk.after(20, old_mainloop)


# ----------------------------------------------------------------------------
#
#  Generate artificial caption data
#
def cmd(n):
    global sim_buf
    global cmd_time

    sliced = struct.pack("=LLBB54x", Zvbi.VBI_SLICED_CAPTION_525,
                                     21,
                                     Zvbi.par8(n >> 8),
                                     Zvbi.par8(n & 0x7F))

    sim_buf.append(["sliced", sliced, cmd_time])
    #vbi.decode(sliced, 1, cmd_time)

    cmd_time += 1 / 29.97


def printc(v):
    global sim_buf

    cmd(v * 256 + 0x80)
    sim_buf.append(["delay", 1])


def prints(v):
    global sim_buf
    s = bytes(v, 'ascii')

    i = 0
    while i + 1 < len(s):
        cmd(s[i] * 256 + s[i+1])
        i += 2

    if i < len(s):
        cmd(s[i] * 256 + 0x80)

    sim_buf.append(["delay", 1])


# constants
white = 0
green = 1
red = 4
yellow = 5
blue = 2
cyan = 3
magenta = 6
black = 7

mapping_row = (2, 3, 4, 5,  10, 11, 12, 13, 14, 15,  0, 6, 7, 8, 9, -1)

italic = 7
underline = 1
opaque = 0
semi_transp = 1

# global variable
ch = 0

def BACKG(v,x):         cmd(0x2000); \
                        cmd(0x1020 + ((ch & 1) << 11) + (v << 1) + x)
def PREAMBLE(v,x,y): 
                        cmd(0x1040 + ((ch & 1) << 11) + ((mapping_row[v] & 14) << 7)
                             + ((mapping_row[v] & 1) << 5) + (x << 1) + y)
def INDENT(v,x,y): 
                        cmd(0x1050 + ((ch & 1) << 11) + ((mapping_row[v] & 14) << 7)
                                   + ((mapping_row[v] & 1) << 5) + ((x // 4) << 1) + y)
def MIDROW(v,x):        cmd(0x1120 + ((ch & 1) << 11) + (v << 1) + x)
def SPECIAL_CHAR(v):    cmd(0x1130 + ((ch & 1) << 11) + v)

def CCODE(v,x):         return (v + ((x & 1) << 11) + ((x & 2) << 7))
def RESUME_CAPTION():   cmd(CCODE(0x1420, ch))
def BACKSPACE():        cmd(CCODE(0x1421, ch))
def DELETE_EOR(v):      cmd(CCODE(0x1424, ch))
def ROLL_UP(v):         cmd(CCODE(0x1425, ch) + v - 2)
def FLASH_ON():         cmd(CCODE(0x1428, ch))
def RESUME_DIRECT():    cmd(CCODE(0x1429, ch))
def TEXT_RESTART():     cmd(CCODE(0x142A, ch))
def RESUME_TEXT():      cmd(CCODE(0x142B, ch))
def END_OF_CAPTION():   cmd(CCODE(0x142F, ch))
def ERASE_DISPLAY():    cmd(CCODE(0x142C, ch))
def CR():               cmd(CCODE(0x142D, ch))
def ERASE_HIDDEN():     cmd(CCODE(0x142E, ch))
def TAB(v):             cmd(CCODE(0x1720, ch) + v)
def TRANSP():           cmd(0x2000); cmd(0x172D + ((ch & 1) << 11))
def BLACK(v):           cmd(0x2000); cmd(0x172E + ((ch & 1) << 11) + v)

def PAUSE(n_frames):
    sim_buf.append(["delay", n_frames])


def hello_world():
    global ch
    global sim_buf
    global cmd_time

    sim_buf = []
    cmd_time = 0.0
    pgno.set(-1)

    prints(" HELLO WORLD! ")
    PAUSE(30)

    ch = 4
    TEXT_RESTART()
    prints("Character set - Text 1")
    CR(); CR()
    for i in range(32, 127+1):
        printc(i)
        if ((i & 15) == 15):
            CR()
    MIDROW(italic, 0)
    for i in range(32, 127+1):
        printc(i)
        if ((i & 15) == 15):
            CR()
    MIDROW(white, underline)
    for i in range(32, 127+1):
        printc(i)
        if ((i & 15) == 15):
            CR()
    MIDROW(white, 0)
    prints("Special: ")
    for i in range(0, 15+1):
        SPECIAL_CHAR(i)
    CR()
    prints("DONE - Text 1 ")
    PAUSE(50)

    ch = 5
    TEXT_RESTART()
    prints("Styles - Text 2")
    CR(); CR()
    MIDROW(white, 0); prints("WHITE"); CR()
    MIDROW(red, 0); prints("RED"); CR()
    MIDROW(green, 0); prints("GREEN"); CR()
    MIDROW(blue, 0); prints("BLUE"); CR()
    MIDROW(yellow, 0); prints("YELLOW"); CR()
    MIDROW(cyan, 0); prints("CYAN"); CR()
    MIDROW(magenta, 0); prints("MAGENTA"); BLACK (0); CR()
    BACKG(white, opaque); prints("WHITE"); BACKG (black, opaque); CR()
    BACKG(red, opaque); prints("RED"); BACKG (black, opaque); CR()
    BACKG(green, opaque); prints("GREEN"); BACKG (black, opaque); CR()
    BACKG(blue, opaque); prints("BLUE"); BACKG (black, opaque); CR()
    BACKG(yellow, opaque); prints("YELLOW"); BACKG (black, opaque); CR()
    BACKG(cyan, opaque); prints("CYAN"); BACKG (black, opaque); CR()
    BACKG(magenta, opaque); prints("MAGENTA"); BACKG (black, opaque); CR()
    PAUSE(200)
    TRANSP()
    prints(" TRANSPARENT BACKGROUND ")
    BACKG(black, opaque); CR()
    MIDROW(white, 0); FLASH_ON()
    prints(" Flashing Text  (if implemented) "); CR()
    MIDROW(white, 0); prints("DONE - Text 2 ")
    PAUSE(50)

    ch = 0
    ROLL_UP(2)
    ERASE_DISPLAY()
    prints(" ROLL-UP TEST "); CR(); PAUSE (20)
    prints("The ZVBI library provides"); CR(); PAUSE (20)
    prints("routines to access raw VBI"); CR(); PAUSE (20)
    prints("sampling devices (currently"); CR(); PAUSE (20)
    prints("the Linux V4L and and V4L2"); CR(); PAUSE (20)
    prints("API and the FreeBSD, OpenBSD,"); CR(); PAUSE (20)
    prints("NetBSD and BSDi bktr driver"); CR(); PAUSE (20)
    prints("API are supported), a versatile"); CR(); PAUSE (20)
    prints("raw VBI bit slicer, decoders"); CR(); PAUSE (20)
    prints("for various data services and"); CR(); PAUSE (20)
    prints("basic search, render and export"); CR(); PAUSE (20)
    prints("functions for text pages. The"); CR(); PAUSE (20)
    prints("library was written for the"); CR(); PAUSE (20)
    prints("Zapping TV viewer and Zapzilla"); CR(); PAUSE (20)
    prints("Teletext browser."); CR(); PAUSE (20)
    CR(); PAUSE(30)
    prints(" DONE - Caption 1 ")
    PAUSE(30)

    ch = 1
    RESUME_DIRECT()
    ERASE_DISPLAY()
    MIDROW(yellow, 0)
    INDENT(2, 10, 0); prints(" FOO "); CR()
    INDENT(3, 10, 0); prints(" MIKE WAS HERE "); CR(); PAUSE (20)
    MIDROW(red, 0)
    INDENT(6, 13, 0); prints(" AND NOW... "); CR()
    INDENT(8, 13, 0); prints(" TOM'S THERE TOO "); CR(); PAUSE (20)
    PREAMBLE(12, cyan, 0)
    prints("01234567890123456789012345678901234567890123456789"); CR()
    MIDROW(white, 0)
    prints(" DONE - Caption 2 "); CR()
    PAUSE(30)


#
#  Play back the buffered (simulated) CC data
#
def play_world():
    while len(sim_buf) > 0:
        a = sim_buf.pop(0)

        if (a[0] == "delay"):
            # delay event -> stop play-back and start timer
            tk.after(25 * a[1], play_world)
            break
        else:
            # pass VBI Data to the decoder context
            # (will trigger display via the CC callback function)
            vbi.decode_bytes(a[1], 1, a[2])


# ------- slicer.c -----------------------------------------------------------
#
# Read one frame's worth of sliced data (written by decode.pl)
# from a file or pipe (not used in demo mode)
#
def read_sliced():
    global read_elapsed

    # Read timestamp: newline-terminated string
    buf = ""
    while True:
        b = infile.read(1)
        if len(b) == 0:  # EOF
            return None
        c = "%c" % b[0]
        if c == '\n':
            break
        buf += c

    # Time in seconds since last frame.
    try:
        dt = float(buf)
        if (dt < 0.0):
            dt = -dt
    except:
        print("invalid timestamp in input: " % dt, file=sys.stderr)
        sys.exit(1)

    timestamp = read_elapsed
    read_elapsed += dt

    # Read number of sliced lines
    n_lines = infile.read(1)[0]
    assert(n_lines >= 0) # "invalid line count in input"

    # Read sliced data: each record consists of:
    # - type ID (0..7)
    # - line index (LSB)
    # - line index (MSB)
    # - data x N: where N is implied by type ID
    sliced = []
    for n in range(0, n_lines):
        index = infile.read(1)[0]
        assert(index >= 0) # "invalid index"

        line = (infile.read(1)[0]
                   + 256 * infile.read(1)[0]) & 0xFFF

        if index == 0:
            slc_id = Zvbi.VBI_SLICED_TELETEXT_B
            data = infile.read(42)

        elif index == 1:
            slc_id = Zvbi.VBI_SLICED_CAPTION_625; 
            data = infile.read(2)

        elif index == 2:
            slc_id = Zvbi.VBI_SLICED_VPS
            data = infile.read(13)

        elif index == 3:
            slc_id = Zvbi.VBI_SLICED_WSS_625; 
            data = infile.read(2)

        elif index == 4:
            slc_id = Zvbi.VBI_SLICED_WSS_CPR1204; 
            data = infile.read(3)

        elif index == 7:
            slc_id = Zvbi.VBI_SLICED_CAPTION_525; 
            data = infile.read(2)

        else:
            print("Oops! Unknown data type index in sliced VBI file", file=sys.stderr)
            exit(1)

        sliced.append([slc_id, line, data])

    return (n_lines, timestamp, sliced)


# ----------------------------------------------------------------------------

def main_func():
    global vbi
    global infile

    # create the GUI
    init_window()

    # create a decoder context and enable Closed Captioning decoding
    vbi = Zvbi.ServiceDec()
    vbi.event_handler_register(Zvbi.VBI_EVENT_CAPTION, cc_handler)

    if sys.stdin.isatty():
        # no file or stream on STDIN -> generate demo data
        print("No input provided via STDIN - playing back demo sequence", file=sys.stderr)
        hello_world()
        # start play back of the demo data (timer-based, to give control to the main loop below)
        play_world()

    else:
        infile = open(sys.stdin.fileno(), "rb")

        #c = ord(infile.read(1) or 1)  # TODO
        #infile.ungetc(c)
        #if (0 == c):
        #    dx = Zvbi.DvbDemux.pes_new()
        #    tk.after(20, pes_mainloop)
        #else:
        if True:
            # install timer to poll for incoming data
            tk.after(20, old_mainloop)

    # everything from here on is event driven
    tk.mainloop()

try:
    main_func()
except KeyboardInterrupt:
    pass
