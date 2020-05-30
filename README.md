# ZVBI library interface module for Python

This repository contains the sources of the "Zvbi" module for Python,
which has its official home at [PyPi](https://pypi.org/project/Zvbi/).
This module is equivalent to the
[Video::ZVBI module for Perl](https://metacpan.org/pod/Video::ZVBI).

This C extension module provides an object-oriented interface to the
[ZVBI library](http://zapping.sourceforge.net/ZVBI/index.html).
The ZVBI library allows accessing television broadcast data services such
as teletext and closed captions via analog or DVB video capture devices.

Official ZVBI library description:

> The ZVBI library provides routines to access raw VBI sampling devices
> (currently the Linux DVB &amp; V4L2 APIs and the FreeBSD, OpenBSD,
> NetBSD and BSDi bktr driver API are supported), a versatile raw VBI
> bit slicer, decoders for various data services and basic search, render
> and export functions for text pages. The library was written for the
> Zapping TV viewer and Zapzilla Teletext browser.

The Zvbi Python module covers all exported libzvbi interfaces.

The Python interface module is largely based on objects which encapsulate
the native C structures of libzvbi. Only when a Python script actually
wants to manipulate captured or decoded data directly (i.e. not via
library functions), the structures' data is converted into Python data
types. Even then, Python methods such as the "buffer protocol" are used to
avoid unnecessary copying. This approach allows for a pretty snappy
performance despite the additional interface layer.

Best starting point to get familiar with the module is having a look at
the *Class Hierarchy*, description of the main classes in the
<A HREF="Video-ZVBI/doc/ZVBI.rst">Python API documentation</A>,
and the example scripts provided in the
<A HREF="Video-ZVBI/examples/">examples/</A> sub-directory, which
demonstrate use of all the main classes.
(The examples are a more or less direct C to Python ports of the respective
programs in the `test/` sub-directory of the libzvbi source tree, plus a
few additions, such as a teletext level 2.5 browser implemented in apx.
300 lines of [Python Tkinter](https://docs.python.org/3/library/tk.html).)
A description of all example scripts can be found in the API documentation
in section "Examples". Additionally you can look at the
[ZVBI home](http://zapping.sourceforge.net/ZVBI/index.html) and the
ZVBI library documentation (unfortunately not available online; built via
doxygen when compiling the library).

## Module information

Module summary:

* development stage python interface: beta release
* development stage libzvbi: mature
* C compiler required for installation
* support of python interface: by developer
* support of libzvbi: no longer maintained (last release 2013)
* object oriented interface
* licensed under GPL-v2 (GNU General Public License, version 2)

Supported operating systems (as determined by ZVBI library):

* Linux
* FreeBSD
* NetBSD
* OpenBSD

## Installation

Pre-requisite to the installation is a C compiler and (obviously) the
[libzvbi library](http://zapping.sourceforge.net/ZVBI/index.html)
(oldest supported version is 0.2.26) which in turn requires the
pthreads and PNG libraries. Once you have installed these,
the module can be installed in the usual steps:

```console
    cd Video-ZVBI
    python3 setup.py test
    python3 setup.py install
```

Note there are no dependencies on other Python extension modules by the
module itself. Some of the provided example scripts however depend on
[Python Tkinter](https://docs.python.org/3/library/tk.html).

## Bug reports

Please submit bug reports relating to the interface module via email
to the author (address listed in module meta data).

If your bug report or request relates to libzvbi rather than the
interface module, please contact the libzvbi authors, preferably
via the [Zapping](http://zapping.sourceforge.net/) mailing list.
In case of capture problems, please make sure to specify which
operating system and hardware you are using, and of course which
version of libzvbi and which version of Python and the ZVBI module
respectively.

## Documentation

For further information please refer to the following files:

* <A HREF="Video-ZVBI/doc/ZVBI.rst">ZVBI.rst</A>: API documentation
* <A HREF="Video-ZVBI/Changelog">Changelog</A>: Release history &amp; change-log
* <A HREF="Video-ZVBI/doc/README.rst">README</A>: same content as on this page
* <A HREF="Video-ZVBI/LICENSE">COPYING</A>: GPL-v2 license

## Copyright

Copyright (C) 2006-2020 T. Zoerner.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but <B>without any warranty</B>; without even the implied warranty of
<B>merchantability or fitness for a particular purpose</B>.  See the
<A HREF="LICENSE">GNU General Public License</A> for more details.
