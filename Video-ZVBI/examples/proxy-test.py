#!/usr/bin/python3
#
#  VBI proxy test client
#
#  Copyright (C) 2003,2004,2006,2007,2020 Tom Zoerner
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
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
#  Description:
#
#   Example for the use of class Zvbi.Proxy. The script can capture either
#   from a proxy daemon or a local device and dumps captured data on the
#   terminal. Also allows changing services and channels during capturing
#   (e.g. by entering "+ttx" or "-ttx" on stdin.) Start with option -help
#   for a list of supported command line options.
#
#   (This is a direct translation of test/proxy-test.c in libzvbi.)

import sys
import argparse
import struct
import re
import select
import Zvbi

# constants from linux/videodev2.h
VIDIOC_ENUMINPUT = 0xC050561A
VIDIOC_G_INPUT = 0x80045626
VIDIOC_S_INPUT = 0xC0045627
VIDIOC_G_FREQUENCY = 0xC02C5638
VIDIOC_S_FREQUENCY = 0x402C5639

all_services_625 =   ( Zvbi.VBI_SLICED_TELETEXT_B |
                       Zvbi.VBI_SLICED_VPS |
                       Zvbi.VBI_SLICED_CAPTION_625 |
                       Zvbi.VBI_SLICED_WSS_625 |
                       Zvbi.VBI_SLICED_VBI_625 )
all_services_525 =   ( Zvbi.VBI_SLICED_CAPTION_525 |
                       Zvbi.VBI_SLICED_2xCAPTION_525 |
                       Zvbi.VBI_SLICED_TELETEXT_BD_525 |
                       Zvbi.VBI_SLICED_VBI_525 )

opt_buf_count = 5
opt_scanning = 0
opt_services = 0

update_services = False
proxy = None # for use in callback

# ---------------------------------------------------------------------------
# Switch channel and frequency (Video 4 Linux #2 API)
#
def SwitchTvChannel(proxy, input_idx, freq):
   result = True

   if (input_idx != -1):
      result = False

      # get current selected input
      try:
         vinp = struct.pack("=i", 0)
         vinp = proxy.device_ioctl(VIDIOC_G_INPUT, vinp)
         prev_inp_idx = struct.unpack("=i", vinp)[0]

         # insert requested channel and norm into the struct
         vinp = struct.pack("=i", input_idx)

         # send channel change request
         try:
            proxy.device_ioctl(VIDIOC_S_INPUT, vinp)
            print("Successfully switched video input from %d to %d" % (prev_inp_idx, input_idx))
            result = True
         except OSError as e:
            print("ioctl VIDIOC_S_INPUT:", e, file=sys.stderr)

      except OSError as e:
         print("ioctl VIDIOC_G_INPUT:", e, file=sys.stderr)

   if (freq != -1):
      result = False

      # query current tuner parameters (including frequency)
      try:
         vfreq = struct.pack("=LLL32x", 0, 0, 0)
         vfreq = proxy.device_ioctl(VIDIOC_G_FREQUENCY, vfreq)
         vtuner, vtype, prev_freq = struct.unpack("=LLL32x", vfreq)

         # send frequency change request
         try:
            vfreq = struct.pack("=LLL32x", vtuner, vtype, freq)
            proxy.device_ioctl(VIDIOC_S_FREQUENCY, vfreq)
            print("Successfully switched frequency: from %d to %d (tuner:%d type:%d)"
                    % (prev_freq, freq, vtuner, vtype), file=sys.stderr)
            result = True
         except OSError as e:
            print("ioctl VIDIOC_S_FREQUENCY(%d):" % freq, e, file=sys.stderr)
      except OSError as e:
         print("ioctl VIDIOC_G_FREQUENCY:", e, file=sys.stderr)

   return result


# ----------------------------------------------------------------------------
# Callback for proxy events
#
def ProxyEventCallback(ev_mask):
   global update_services

   if proxy:
      if (ev_mask & Zvbi.VBI_PROXY_EV_CHN_RECLAIMED):
         print("ProxyEventCallback: token was reclaimed", file=sys.stderr)

         proxy.channel_notify(Zvbi.VBI_PROXY_CHN_TOKEN, 0)

      elif (ev_mask & Zvbi.VBI_PROXY_EV_CHN_GRANTED):
         print("ProxyEventCallback: token granted", file=sys.stderr)

         if ((opt.vinput != -1) or (opt.freq != -1)):
            if (SwitchTvChannel(proxy, opt.vinput, opt.freq)):
               flags = (Zvbi.VBI_PROXY_CHN_TOKEN |
                        Zvbi.VBI_PROXY_CHN_FLUSH)
            else:
               flags = (Zvbi.VBI_PROXY_CHN_RELEASE |
                        Zvbi.VBI_PROXY_CHN_FAIL |
                        Zvbi.VBI_PROXY_CHN_FLUSH)

            if (opt_scanning != 0):
               flags |= Zvbi.VBI_PROXY_CHN_NORM

         else:
            flags = Zvbi.VBI_PROXY_CHN_RELEASE

         proxy.channel_notify(flags, opt_scanning)

      if (ev_mask & Zvbi.VBI_PROXY_EV_CHN_CHANGED):
         lfreq = 0
         # query frequency: tuner, type=V4L2_TUNER_ANALOG_TV, frequency
         buf = struct.pack("=LLL32x", 0, 0, 0)
         try:
            buf = proxy.device_ioctl(VIDIOC_G_FREQUENCY, buf)
            lfreq = struct.unpack("=LLL32x", buf)[2]
            print("Proxy granted video freq: %d" % lfreq, file=sys.stderr)
         except OSError as e:
            print("ProxyEventCallback: VIDIOCGFREQ failed:", e, file=sys.stderr)

         print("ProxyEventCallback: TV channel changed: lfreq", file=sys.stderr)

      if (ev_mask & Zvbi.VBI_PROXY_EV_NORM_CHANGED):
         print("ProxyEventCallback: TV norm changed", file=sys.stderr)
         update_services = True


# ---------------------------------------------------------------------------
# Decode a teletext data line
#
def PrintTeletextData(data, line, slc_id):
   mag    =    0xF
   pkgno  =   0xFF
   tmp1 = Zvbi.unham16p(data)
   if (tmp1 >= 0):
      pkgno = (tmp1 >> 3) & 0x1f
      mag   = tmp1 & 7
      if (mag == 0):
         mag = 8

   if (pkgno != 0):
      data = Zvbi.unpar_str(data, ' ')
      data = re.sub(r'[\x00-\x1F\x7F]', ' ', data.decode('ISO-8859-1'))
      print("line %3d id=%d pkg %X.%03X: '%s'" % (line, slc_id, mag, pkgno, data[2 : 40]))
   else:
      # it's a page header: decode page number and sub-page code
      tmp1 = Zvbi.unham16p(data, 2)
      tmp2 = Zvbi.unham16p(data, 4)
      tmp3 = Zvbi.unham16p(data, 6)
      pageNo = tmp1 | (mag << 8)
      sub    = (tmp2 | (tmp3 << 8)) & 0x3f7f

      data = Zvbi.unpar_str(data, ' ')
      data = re.sub(r'[\x00-\x1F]', ' ', data.decode('ISO-8859-1'))
      print("line %3d id=%d page %03X.%04X: '%s'" % (line, slc_id, pageNo, sub, data[2+8 : 40-8]))


# ---------------------------------------------------------------------------
# Decode a VPS data line
# - bit fields are defined in "VPS Richtlinie 8R2" from August 1995
# - called by the VBI decoder for every received VPS line
#
def PrintVpsData(data):
   VPSOFF = -3

   #cni = ((data[VPSOFF+13] & 0x3) << 10) | ((data[VPSOFF+14] & 0xc0) << 2) |
   #       ((data[VPSOFF+11] & 0xc0)) | (data[VPSOFF+14] & 0x3f)
   #if (cni == 0xDC3):
   #   # special case: "ARD/ZDF Gemeinsames Vormittagsprogramm"
   #   cni = (data[VPSOFF+5] & 0x20) ? 0xDC1 : 0xDC2

   cni = Zvbi.decode_vps_cni(data)

   if ((cni != 0) and (cni != 0xfff)):
      # decode VPS PIL
      mday   =  (data[VPSOFF+11] & 0x3e) >> 1
      month  = ((data[VPSOFF+12] & 0xe0) >> 5) | ((data[VPSOFF+11] & 1) << 3)
      hour   =  (data[VPSOFF+12] & 0x1f)
      minute =  (data[VPSOFF+13] >> 2)

      print("VPS %d.%d. %02d:%02d CNI 0x%04X" % (mday, month, hour, minute, cni))


# ---------------------------------------------------------------------------
# Check stdin for services change requests
# - syntax: ["+"|"-"|"="]keyword, e.g. "+vps-ttx" or "=wss"
#
def read_service_string():
   services = opt_services

   buf = input("")
   if buf != "":
      usage = "Expected command syntax: {+|-|=}{ttx|vps|wss|cc|raw}"
      for match in re.finditer(r' *(\S+)', buf):
         substract = 0
         if (len(match.group(1)) > 1):
            ctrl_str = match.group(1)[0]
            srv_str = match.group(1)[1:]

            if (ctrl_str == "="):
               services = 0
            elif (ctrl_str == "-"):
               substract = 1
            elif (ctrl_str == "+"):
               substract = 0
            else:
               print("Invalid control: '%s' in '%s'\n" % (ctrl_str, buf), usage, file=sys.stderr)

            if ( (srv_str == "ttx") or (srv_str == "teletext") ):
               tmp_services = Zvbi.VBI_SLICED_TELETEXT_B | Zvbi.VBI_SLICED_TELETEXT_BD_525
            elif (srv_str == "vps"):
               tmp_services = Zvbi.VBI_SLICED_VPS
            elif (srv_str == "wss"):
               tmp_services = Zvbi.VBI_SLICED_WSS_625 | Zvbi.VBI_SLICED_WSS_CPR1204
            elif ( (srv_str == "cc") or (srv_str == "caption") ):
               tmp_services = Zvbi.VBI_SLICED_CAPTION_625 | Zvbi.VBI_SLICED_CAPTION_525
            elif (srv_str == "raw"):
               tmp_services = Zvbi.VBI_SLICED_VBI_625 | Zvbi.VBI_SLICED_VBI_525
            else:
               print("Invalid service: '%s'\n" % srv_str, usage, file=sys.stderr)
               break

            if (substract == 0):
               services |= tmp_services
            else:
               services &= ~ tmp_services
         else:
            print("Invalid command (too short): '%s'\n" % buf, usage, file=sys.stderr)
            break

   return services


# ----------------------------------------------------------------------------
# Main loop
#
def main_func():
   global proxy
   global cap
   global update_services
   global opt_services

   #TODO fcntl(STDIN, F_SETFL, O_NONBLOCK)

   if ((opt_services != 0) and (opt_scanning == 0)):
      cur_services = opt_services
   else:
      cur_services = None

   proxy = None
   cap = None
   try:
      if opt.api == 'v4l2':
         cap = Zvbi.Capture.Analog(opt.device, services=cur_services, scanning=opt_scanning,
                            buffers=opt_buf_count, strict=opt.strict, trace=opt.debug_level)
      else:
         proxy = Zvbi.Proxy(opt.device, "proxy-test", trace=opt.debug_level)
         cap = Zvbi.Capture.Analog(opt.device, services=opt_services, scanning=opt_scanning,
                                   buffers=opt_buf_count, strict=opt.strict, trace=opt.debug_level,
                                   proxy=proxy)
         proxy.set_callback(ProxyEventCallback)

      last_line_count = -1

      # switch to the requested channel
      if ( (opt.vinput != -1) or (opt.freq != -1) or
           (opt.chnprio != Zvbi.VBI_CHN_PRIO_INTERACTIVE) ):

         proxy.channel_request(opt.chnprio,
                               request_chn   = (opt.vinput != -1) or (opt.freq != -1),
                               sub_prio      = opt.subprio,
                               exp_duration  = 0,
                               min_duration  = 10)

         if (opt.chnprio != Zvbi.VBI_CHN_PRIO_BACKGROUND):
            SwitchTvChannel(proxy, opt.vinput, opt.freq)

      # initialize services for raw capture
      if ((opt_services & (Zvbi.VBI_SLICED_VBI_625 | Zvbi.VBI_SLICED_VBI_525)) != 0):
         #raw = Zvbi.RawDec(cap.parameters())
         raw = Zvbi.RawDec(cap)
         raw.add_services(all_services_525 | all_services_625, 0)

      update_services = (opt_scanning != 0)

      while True:
         vbi_fd = cap.get_fd()
         if vbi_fd == -1:
            break

         ret, foo1, foo2 = select.select([vbi_fd, sys.stdin.fileno()], [], [])

         if sys.stdin.fileno() in ret:
            new_services = read_service_string()
            if (opt_scanning == 625):
               new_services &= all_services_625
            elif (opt_scanning == 525):
               new_services &= all_services_525

            if (new_services != opt_services):
               print("switching service from 0x%X to 0x%X..." % (opt_services, new_services), file=sys.stderr)
               opt_services = new_services
               update_services = True

         if (update_services):
            try:
               cur_services = cap.update_services(opt_services, reset=True, commit=True, strict=opt.strict)
               if ((cur_services != 0) or (opt_services == 0)):
                  print("...got granted services 0x%X." % cur_services, file=sys.stderr)
            except Zvbi.CaptureError as e:
               print("...failed:", e, file=sys.stderr)

            last_line_count = 0
            update_services = False

         if vbi_fd in ret:
            if ((opt_services & (Zvbi.VBI_SLICED_VBI_625 | Zvbi.VBI_SLICED_VBI_525)) == 0):
               try:
                  sliced_buf = cap.pull_sliced(1000)

                  ttx_lines = 0
                  for (data, slc_id, line) in sliced_buf:
                     if (slc_id & Zvbi.VBI_SLICED_TELETEXT_B):
                        PrintTeletextData(data, line, slc_id)
                        ttx_lines += 1

                     elif (slc_id & Zvbi.VBI_SLICED_VPS):
                        PrintVpsData(data)

                     elif (slc_id & Zvbi.VBI_SLICED_WSS_625):
                        (w0, w1, w2) = struct.unpack("=ccc", data)
                        print("WSS 0x%02X%02X%02X" % (w0, w1, w2))

                     elif (slc_id & (Zvbi.VBI_SLICED_CAPTION_625 | Zvbi.VBI_SLICED_CAPTION_525)):
                        print("CC data 0x%02X,%02X" % (Zvbi.unpar8(data[0]), Zvbi.unpar8(data[1])))

                  if (last_line_count != len(sliced_buf)):
                     last_line_count = len(sliced_buf)
                     print("Receiving frames with %d sliced lines" % last_line_count, file=sys.stderr)

               except Zvbi.CaptureError as e:
                  print("VBI read error:", e, file=sys.stderr)
                  break
               except Zvbi.CaptureTimeout:
                  if (opt.debug_level > 0):
                     print("proxy-test: timeout in VBI read", file=sys.stderr)

            else:
               #res = cap.pull_raw(raw_buf, timestamp, 1000)
               try:
                  raw_buf = cap.read_raw(1000)
                  line_count, sliced = raw.decode(raw_buf)

                  if (last_line_count != line_count):
                     print("Receiving frames with %d slicable lines" % line_count, file=sys.stderr)
                     last_line_count = line_count

                  for (data, slc_id, line) in sliced:
                     if (slc_id & Zvbi.VBI_SLICED_TELETEXT_B):
                        PrintTeletextData(data, line, slc_id)

                     elif (slc_id & Zvbi.VBI_SLICED_VPS):
                        PrintVpsData(data)

                     elif (slc_id & Zvbi.VBI_SLICED_WSS_625):
                        (w0, w1, w2) = struct.unpack("=ccc", data)
                        print("WSS 0x%02X%02X%02X" % (w0, w1, w2))

                     elif (slc_id & (Zvbi.VBI_SLICED_CAPTION_625 | Zvbi.VBI_SLICED_CAPTION_525)):
                        print("CC data 0x%02X,%02X" % (Zvbi.unpar8(data[0]), Zvbi.unpar8(data[1])))

               except Zvbi.CaptureError as e:
                  print("VBI read error:", e, file=sys.stderr)
                  break
               except Zvbi.CaptureTimeout:
                  if (opt.debug_level > 0):
                     print("VBI read timeout", file=sys.stderr)

      del cap

   except Zvbi.ProxyError as e:
      print("Error configuring proxy:", e, file=sys.stderr)
   except Zvbi.CaptureError as e:
      print("Error starting acquisition", e, file=sys.stderr)

   if proxy:
      del proxy

# ---------------------------------------------------------------------------
# Parse command line options
#
def ParseCmdOptions():
   global opt
   global opt_scanning
   global opt_services

   usage = (("%s [ Options ] service ...\n" % sys.argv[0]) +
            "Supported services         : ttx | vps | wss | cc | raw | null\n"+
            "Supported options:\n"+
            "       -dev <path>         : device path\n"+
            "       -api <type>         : v4l API: proxy|v4l2|v4l\n"+
            "       -strict <level>     : service strictness level: 0..2\n"+
            "       -norm PAL|NTSC      : specify video norm as PAL or NTSC\n"+
            "       -vinput <index>     : switch video input source\n"+
            "       -freq <kHz * 16>    : switch TV tuner frequency\n"+
            "       -chnprio <1..3>     : channel switch priority\n"+
            "       -subprio <0..4>     : background scheduling priority\n"+
            "       -debug <level>      : enable debug output: 1=warnings, 2=all\n"+
            "       -help               : this message\n"+
            "You can also type service requests to stdin at runtime:\n"+
            "Format: [\"+\"|\"-\"|\"=\"]<service>, e.g. \"+vps -ttx\" or \"=wss\"\n")

   parser = argparse.ArgumentParser(description="Test of proxy client", usage=usage)
   parser.add_argument("--device", type=str, default="/dev/vbi0", help="Path to VBI capture device (analog only)")
   parser.add_argument("--api", type=str, default="proxy", help="interface to use: proxy|v4l2")
   parser.add_argument("--strict", type=int, default=0, help="service strictness level: 0..2")
   parser.add_argument("--norm", type=str, default="", help="specify video norm as PAL or NTSC")
   parser.add_argument("--vinput", type=int, default=-1, help="switch video input source")
   parser.add_argument("--freq", type=int, default=-1, help="switch TV tuner frequency (unit: kHz*16)")
   parser.add_argument("--chnprio", type=int, default=Zvbi.VBI_CHN_PRIO_INTERACTIVE, help="channel switch priority 1..3")
   parser.add_argument("--subprio", type=int, default=0, help="background scheduling priority 0..4")
   parser.add_argument("--debug", type=int, default=0, dest="debug_level", help="control debug output: 0=none, 1=warnings, 2=all")

   parser.add_argument("srv_lbl", nargs='*', help="services: ttx | vps | wss | cc | raw | null")
   opt = parser.parse_args()

   have_service = False
   opt_services = 0
   for srv in opt.srv_lbl:
      if srv == 'ttx' or srv == 'teletext':
         opt_services |= Zvbi.VBI_SLICED_TELETEXT_B | Zvbi.VBI_SLICED_TELETEXT_BD_525
         have_service = True
      elif srv == 'vps':
         opt_services |= Zvbi.VBI_SLICED_VPS
         have_service = True
      elif srv == 'wss':
         opt_services |= Zvbi.VBI_SLICED_WSS_625 | Zvbi.VBI_SLICED_WSS_CPR1204
         have_service = True
      elif srv == 'cc' or srv == 'caption':
         opt_services |= Zvbi.VBI_SLICED_CAPTION_625 | Zvbi.VBI_SLICED_CAPTION_525
         have_service = True
      elif srv == 'raw':
         opt_services |= Zvbi.VBI_SLICED_VBI_625 | Zvbi.VBI_SLICED_VBI_525
         have_service = True
      elif srv == 'null':
         have_service = True

   if not have_service:
      print("no service given - Must specify at least one service (may be 'null')", file=sys.stderr)
      sys.exit(1)

   if not (opt.api == "proxy" or opt.api == "v4l2"):
      print("Unknown API:", opt.api, "\n", usage, file=sys.stderr)
      exit(1)

   if opt.norm == "PAL":
      opt_scanning = 625
   elif opt.norm == "NTSC":
      opt_scanning = 525
   elif not opt.norm == "":
      print("Unknown video norm:", opt.norm, "\n", usage, file=sys.stderr)
      exit(1)

   if opt.strict < 0 or opt.strict > 2:
      print("Invalid strictness level:", opt.strict, "\n", usage, file=sys.stderr)
      exit(1)

   if "dvb" in opt.device:
      print("WARNING: DVB devices are not supported by proxy", file=sys.stderr)


# ---------------------------------------------------------------------------
# main

try:
   ParseCmdOptions()
   main_func()
except KeyboardInterrupt:
   pass
