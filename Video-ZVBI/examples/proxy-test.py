#!/usr/bin/perl -w
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
#    This is a small demo application for the VBI proxy and libzvbi.
#    It will read VBI data from the device given on the command line
#    and dump requested services' data to standard output.  See below
#    for a list of possible options.
#
#    This Python script has been translated from a Perl script that
#    has been translated from proxy-test.c which is located in the
#    test directory of the libzvbi package.
#
#  Perl #Id: proxy-test.pl,v 1.1 2007/11/18 18:48:35 tom Exp tom #
#

import sys
import argparse
import struct
import re
import select
import Zvbi

# constants
VIDIOCGCAP = 0x803C7601  # from linux/videodev.h
VIDIOCGCHAN = 0xC0307602
VIDIOCSCHAN = 0x40307603
VIDIOCGFREQ = 0x8008760E
VIDIOCSFREQ = 0x4008760F
VIDIOCGTUNER = 0xC0407604
VIDIOCSTUNER = 0x40407605

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
proxy = None # for callback

# ---------------------------------------------------------------------------
# Switch channel and frequency (Video 4 Linux #1 API)
#
def SwitchTvChannel(proxy, channel, freq):
   result = True

   if (channel != -1):
      result = False

      norm = 0
      if (opt_scanning == 625):
         norm = 0
      elif (opt_scanning == 525):
         norm = 1

      vchan = struct.pack("i32xiLhh", channel, 0, 0, 0, norm)

      # get current config of the selected chanel
      try:
         proxy.device_ioctl(VIDIOCGCHAN, vchan)
         (vc_channel, vc_tuners, vc_flags, vc_type, vc_norm) = struct.unpack("i32xiLhh", vchan)

         # insert requested channel and norm into the struct
         vchan = struct.pack("i32xiLhh", channel, vc_tuners, vc_flags, vc_type, norm)

         # send channel change request
         try:
            proxy.device_ioctl(VIDIOCSCHAN, vchan)
            result = True

         except OSError as e:
            print("ioctl VIDIOCSCHAN:", e, file=sys.stderr)

      except OSError as e:
         print("ioctl VIDIOCGCHAN:", e, file=sys.stderr)

   if (freq != -1):
      result = False

      if ( (channel == -1) or ((vc_type & 1) and (vc_flags & 1)) ):
         # send frequency change request
         arg = struct.pack("L", freq)
         try:
            proxy.device_ioctl(VIDIOCSFREQ, arg)
            result = True
         except OSError as e:
            print("ioctl VIDIOCSFREQ:", e, file=sys.stderr)
      else:
         print("cannot tune frequency: channel channel has no tuner", file=sys.stderr)

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

         if ((opt.channel != -1) or (opt.freq != -1)):
            if (SwitchTvChannel(proxy, opt.channel, opt.freq)):
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
         buf = struct.pack("L", lfreq)
         try:
            proxy.device_ioctl(VIDIOCGFREQ, buf)
            lfreq = struct.unpack("L", buf)
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
            "       -channel <index>    : switch video input channel\n"+
            "       -freq <kHz * 16>    : switch TV tuner frequency\n"+
            "       -chnprio <1..3>     : channel switch priority\n"+
            "       -subprio <0..4>     : background scheduling priority\n"+
            "       -debug <level>      : enable debug output: 1=warnings, 2=all\n"+
            "       -help               : this message\n"+
            "You can also type service requests to stdin at runtime:\n"+
            "Format: [\"+\"|\"-\"|\"=\"]<service>, e.g. \"+vps -ttx\" or \"=wss\"\n")

   parser = argparse.ArgumentParser(description="Test of proxy client", usage=usage)
   parser.add_argument("--device", type=str, default="/dev/vbi0", help="device path")
   parser.add_argument("--api", type=str, default="proxy", help="v4l API: proxy|v4l2|v4l")
   parser.add_argument("--strict", type=int, default=0, help="service strictness level: 0..2")
   parser.add_argument("--norm", type=str, default="", help="specify video norm as PAL or NTSC")
   parser.add_argument("--channel", type=int, default=-1, help="switch video input channel")
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


# ----------------------------------------------------------------------------
# Main entry point
#
def main():
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
         cap = Zvbi.Capture(opt.device, services=cur_services, scanning=opt_scanning,
                            buffers=opt_buf_count, strict=opt.strict, trace=opt.debug_level)
      else:
         proxy = Zvbi.Proxy(opt.device, "proxy-test", trace=opt.debug_level)
         cap = Zvbi.Capture(opt.device, services=opt_services, scanning=opt_scanning,
                            buffers=opt_buf_count, strict=opt.strict, trace=opt.debug_level,
                            proxy=proxy)
         proxy.set_callback(ProxyEventCallback)

      last_line_count = -1

      # switch to the requested channel
      if ( (opt.channel != -1) or (opt.freq != -1) or
           (opt.chnprio != Zvbi.VBI_CHN_PRIO_INTERACTIVE) ):

         proxy.channel_request(opt.chnprio,
                               request_chn   = (opt.channel != -1) or (opt.freq != -1),
                               sub_prio      = opt.subprio,
                               exp_duration  = 0,
                               min_duration  = 10)

         if (opt.chnprio != Zvbi.VBI_CHN_PRIO_BACKGROUND):
            SwitchTvChannel(proxy, opt.channel, opt.freq)

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
                  cap_data = cap.pull_sliced(1000)

                  ttx_lines = 0
                  for (data, slc_id, line) in cap_data.sliced_buffer:
                     if (slc_id & Zvbi.VBI_SLICED_TELETEXT_B):
                        PrintTeletextData(data, line, slc_id)
                        ttx_lines += 1

                     elif (slc_id & Zvbi.VBI_SLICED_VPS):
                        PrintVpsData(data)

                     elif (slc_id & Zvbi.VBI_SLICED_WSS_625):
                        (w0, w1, w2) = struct.unpack("ccc", data)
                        print("WSS 0x%02X%02X%02X" % (w0, w1, w2))

                     elif (slc_id & (Zvbi.VBI_SLICED_CAPTION_625 | Zvbi.VBI_SLICED_CAPTION_525)):
                        print("CC data 0x%02X,%02X" % (Zvbi.unpar8(data[0]), Zvbi.unpar8(data[1])))

                  if (last_line_count != cap_data.sliced_lines):
                     last_line_count = cap_data.sliced_lines
                     print("Receiving frames with %d sliced lines" % last_line_count, file=sys.stderr)

               except Zvbi.CaptureError as e:
                  if "timeout" not in str(e):
                     print("VBI read error:", e, file=sys.stderr)
                     break
                  else:
                     print("proxy-test: timeout in VBI read", file=sys.stderr)

            else:
               #res = cap.pull_raw(raw_buf, timestamp, 1000)
               try:
                  cap_data = cap.read_raw(1000)
                  line_count, sliced = raw.decode(cap.data.raw_buffer)

                  if (last_line_count != line_count):
                     print("Receiving frames with %d slicable lines" % line_count, file=sys.stderr)
                     last_line_count = line_count

                  for (data, slc_id, line) in sliced:
                     if (slc_id & Zvbi.VBI_SLICED_TELETEXT_B):
                        PrintTeletextData(data, line, slc_id)

                     elif (slc_id & Zvbi.VBI_SLICED_VPS):
                        PrintVpsData(data)

                     elif (slc_id & Zvbi.VBI_SLICED_WSS_625):
                        (w0, w1, w2) = struct.unpack("ccc", data)
                        print("WSS 0x%02X%02X%02X" % (w0, w1, w2))

                     elif (slc_id & (Zvbi.VBI_SLICED_CAPTION_625 | Zvbi.VBI_SLICED_CAPTION_525)):
                        print("CC data 0x%02X,%02X" % (Zvbi.unpar8(data[0]), Zvbi.unpar8(data[1])))

               except Zvbi.CaptureError as e:
                  if "timeout" not in str(e):
                     print("VBI read error:", e, file=sys.stderr)
                     break
                  elif (opt.debug_level > 0):
                     print("VBI read timeout", file=sys.stderr)

      del cap

   except Zvbi.ProxyError as e:
      print("Error configuring proxy:", e, file=sys.stderr)
   except Zvbi.CaptureError as e:
      print("Error starting acquisition", e, file=sys.stderr)

   if proxy:
      del proxy

try:
   ParseCmdOptions()
   main()
except KeyboardInterrupt:
   pass
