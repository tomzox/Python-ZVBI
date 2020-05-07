=============================
ZVBI library interface module
=============================

Name:           Video-ZVBI
Version:        1.0.0
Date:           April 2020

DLSIP-code:     RcdOg
                - stable release
                - C compiler required for installation
                - support by developer
                - object oriented using blessed references
                - licensed under GPL (GNU General Public License)

Supported OS:   Linux, FreeBSD, NetBSD, OpenBSD
Author:         TOMZO (T. Zoerner <tomzo AT users.sourceforge.net>)
URL:            https://github.com/tomzox/Perl-Video-ZVBI


Description
-----------

This Perl module provides an object-oriented interface to the ZVBI
library. The ZVBI library allows accessing broadcast data services such
as teletext or closed caption via analog video or DVB capture devices.

Official ZVBI library description:

  "The ZVBI library provides routines to access raw VBI sampling devices
  (currently the Linux V4L & V4L2 APIs and the FreeBSD, OpenBSD,
  NetBSD and BSDi bktr driver API are supported), a versatile raw VBI
  bit slicer, decoders for various data services and basic search, render
  and export functions for text pages. The library was written for the
  Zapping TV viewer and Zapzilla Teletext browser."

The ZVBI Perl module covers all exported libzvbi functions.

The Perl interface is largely based on blessed references to binary
pointers. Internally the module uses the native C structures of libzvbi
as far as possible.  Only in the last step, when a Perl script wants
to manipulate captured data directly (i.e. not via library functions),
the structures' data is copied into Perl data types.  This approach
allows for a pretty snappy performance.

Best starting point to get familiar with the module having a look at
ZVBI <http://zapping.sourceforge.net/ZVBI/index.html>, the documentation
generated when building the ZVBI package, and the example scripts
provided in the example/ sub-directory: these are more or less direct C
to Perl ports of the respective programs in the test/ sub-directory of
the libzvbi sources with a few additions, such as a teletext level 2.5
browser implemented in apx. 300 lines of Perl::Tk.


Comparison with the Video-Capture-VBI module
---------------------------------------------

You may have noticed that there already is a Perl VBI module which
provides very similar functionality.  The approach is completely
different though: The older VBI module contains an actual implementation
of a teletext and VPS slicers and databases, while ZVBI is an interface
layer to an external C library.

Besides that, the reason for not just contributing code to the
existing module was that libzvbi alone can almost entirely replace
the older VBI module (i.e. it covers the same functionality, except
currently for the high-level "nexTView" EPG decoding.)  So there would
not much to be gained by merging the code. On the other hand, libzvbi
supports much more platforms, drivers (e.g. V4L2, DVB, BSD) and services,
so that it seems warranted to make that available to the Perl universe.


Installation
------------

Pre-requisite to the installation is a C compiler and the libzvbi
library (oldest supported version is 0.2.16, or 0.2.4 when disabling
USE_DL_SYM in Makefile.PL) which in turn requires the pthreads and
PNG libraries. Once you have these, installation is done in the
usual steps:

    perl Makefile.PL
    make
    make install

However cleaner than installing directly is generating a package and
installing that, as this will ensure dependencies are available and
allow for easy removal of installed files. The repository at the
homepage listed above contains a script called create_deb.sh that
automatically creates such a package for Debian (or Ubuntu).

Note there are no dependencies on other Perl modules by the module
itself. Some of provided example scripts however depend on Perl::Tk.

Trouble-shooting note: By default the module compiles against an
internal copy of libzvbi.h and loads symbols added in recent library
versions during module start from the shared library. If your compiled
module doesn't load, try disabling compile switches USE_DL_SYM and
USE_LIBZVBI_INT in Makefile.PL


Bug Reports
-----------

Please submit bug reports relating to the interface module via
CPAN <https://metacpan.org/pod/Video::ZVBI>.

If your bug report or request relates to libzvbi rather than the
interface module, please contact the libzvbi authors, preferably
via the zapping mailing list (<http://zapping.sourceforge.net/>)
In case of capture problems, please make sure to specify which
operating system and hardware you are using, and of course which
version of libzvbi and which version of Perl and the ZVBI module
respectively.


Copyright
---------

Copyright (C) 2006-2020 T. Zoerner.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.