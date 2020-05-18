=============================
ZVBI library interface module
=============================

The Python "ZVBI" module provides an object-oriented interface to the
`ZVBI library`_. The ZVBI library allows accessing television broadcast
data services such as teletext or closed captions via analog or DVB
video capture devices.

.. _ZVBI library: http://zapping.sourceforge.net/ZVBI/index.html

Official ZVBI library description:

  The ZVBI library provides routines to access raw VBI sampling devices
  (currently the Linux DVB & V4L2 API and the FreeBSD, OpenBSD,
  NetBSD and BSDi bktr driver API are supported), a versatile raw VBI
  bit slicer, decoders for various data services and basic search, render
  and export functions for text pages. The library was written for the
  Zapping TV viewer and Zapzilla Teletext browser.

The Python ZVBI module covers all exported libzvbi functions.

The Python interface module is largely based on objects which encapsulate
the native C structures of libzvbi. Only in the last step, when a Python
script actually wants to manipulate captured or decoded data directly
(i.e. not via library functions), the structures' data is converted into
Python data types. This approach allows for a pretty snappy performance.

Best starting point to get familiar with the module is having a look at
the `Class Hierarchy`_, description of the main classes, and the example
scripts provided in the "examples/" sub-directory, which demonstrate use
of all the main classes.

The examples are a more or less direct C to Python ports of the respective
programs in the `test/` sub-directory of the libzvbi source tree, plus a
few additions, such as a teletext level 2.5 browser implemented in apx.
300 lines of `TkInter`_. A description of all example scripts can be found
below in section `Examples`_.

.. _Video::ZVBI module for Perl: https://metacpan.org/pod/Video::ZVBI

Supported operating systems (as determined by ZVBI library):

* Linux
* FreeBSD
* NetBSD
* OpenBSD

Pre-requisite to the installation is a C compiler and the
`ZVBI library`_ (oldest supported version is 0.2.26) which in turn
requires the pthreads and PNG libraries.  Note there are no
dependencies on other Python modules by the module itself. Some of
the provided example scripts however depend on `Tkinter`_.

.. _ZVBI library: http://zapping.sourceforge.net/ZVBI/index.html
.. _Tkinter: https://docs.python.org/3/library/tk.html

Finally note there is an equivalent
`Video::ZVBI module for Perl`_ (also at `CPAN`_)

.. _CPAN: https://metacpan.org/pod/Quota
.. _Tkinter: https://docs.python.org/3/library/tk.html

The following is a copy of the API documentation in file doc/FsQuota.rst
