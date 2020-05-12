===================================================
Zvbi - VBI decoding (teletext, closed caption, ...)
===================================================

SYNOPSIS
========

::

  import Zvbi

  cap = Zvbi.Capture("/dev/dvb/adapter0/demux0", dvb_pid=104)

  dec = Zvbi.ServiceDec()
  dec.event_handler_register(Zvbi.VBI_EVENT_TTX_PAGE, pg_handler)

  while True:
      cap_data = cap.pull_sliced(2000)
      vtdec.decode(cap_data.sliced_buffer)

  def pg_handler(pgtype, ev, user_data=None):
      pg = dec.fetch_vt_page(ev['pgno'])
      pg.get_page_text()


DESCRIPTION
===========

This module provides a Python interface to **libzvbi**.
The ZVBI library allows accessing television broadcast data services such
as teletext or closed captions via analog or DVB video capture devices.

Official library description:
"The ZVBI library provides routines to access raw VBI sampling devices
(currently the Linux DVB & V4L2 APIs and the FreeBSD, OpenBSD,
NetBSD and BSDi bktr driver API are supported), a versatile raw VBI
bit slicer, decoders for various data services and basic search, render
and export functions for text pages. The library was written for the
Zapping TV viewer and Zapzilla Teletext browser."

The *Zvbi* Python module covers all exported libzvbi functions. Most of
the libary functions and parameters are exposed equivalently, with
adaptions to render the interface *Pythonic*.

Note: This manual page does not reproduce the full documentation which is
available along with libzvbi: Specifically, methods descriptions are fully
included, but members of data structures are not fully documented here.
Hence it's recommended that you use the libzvbi documentation generated
via doxygen during a libzvbi build in parallel for such details, if needed.

Class Hierarchy
===============

The following is an overview how functionality provided by *libzvbi* is
structured into classes, and how the classes are connected:

`Zvbi.Capture`_
    This class allows receiving data through a video device. For
    digital television transmissions (DVB) this mostly involves demultiplexing
    of the data service sub-stream from the overall transmission stream.
    For analog television transmission this involves capturing selected
    lines within the Vertical Blanking Interval (VBI) and "demodulating"
    the digitized wave form to raw data.
`Zvbi.CaptureBuf`_
    This is a container class, used for efficiently transferring captured
    data to class *ServiceDec* or other processing. The buffer is used
    both for raw and sliced data, which are distinguished by use of
    sub-classes *CaptureRawBuf* and *CaptureSlicedBuf*.
`Zvbi.RawDec`_
    This class can optionally be used for manual processing raw data (i.e.
    direct output of the analog-to-digital conversion of the video signal)
    optionally returned by the *Capture* class via *pull_raw()* methods
    et.al. For most cases the processing (so-called "slicing") done under
    control of the *Capture* class using *pull_sliced()* should be
    sufficient, so this class is usually not needed. This class is not
    applicable for DVB.
`Zvbi.Proxy`_
    This class allows accessing VBI devices via a proxy daemon. An instance
    of this class would be provided to the *Capture* class constructor.
    Using the proxy instead of capturing directly from a VBI device allows
    multiple applications to capture concurrently (e.g. for decoding multiple
    data services). Not applicable to DVB, as DVB drivers usually allow
    capturing by multiple applications concurrently, so the proxy does not
    support DVB.
`Zvbi.ServiceDec`_
    Class performing high level decoding of all supported services and storing
    the data in an internal cache. The class takes input ("sliced data") from
    the *Capture* class. The class supports callback functions for
    notifications about received data; various interfaces allow extracting
    the data in form of instances of the *Page* class, or as an image in
    PPM or XPM formats.
`Zvbi.Search`_
    This class allows searching the cache maintained by *ServiceDec* for
    pages with text matching a pattern. The search returns instances of
    class *Page*.
`Zvbi.Page`_
    Instances of this class are produced by the *ServiceDec* query functions
    or *Search*. Each instance represents a teletext (sub-)page or
    Closed Caption page. The class has various interfaces for extracting
    the text and properties of the page content.
`Zvbi.Export`_
    This class allows rendering an instance of *Page* in specific formats,
    for example as plain-text or image. The result can be returned within
    a Python string or *bytes* object respectively, or written directly to
    a file.

In summary: For receiving live data, you'll need at least an instance of
the *Capture* class. Your application is responsible for calling the capture
interface at 25 or 30 Hz intervals (ideally using a separate thread that
blocks on the capture function, or using a timer). For standard services
such as Teletext, Closed Caption, WSS or network identification you'll need
to feeding the captured data into an instance of the *ServiceDec* class.
For other services you may used the de-multiplexers listed below, or
process the data within your application directly. After that it depends
on your application which interfaces/classes you want to use for further
processing decoded data.

Classes with interfaces supported in libzvbi, but not yet implemented
in the Python module:

`Zvbi.IdlDemux`_
    This class allows decoding data transmissions within a Teletext
    packet stream using **Independent Data Line** protocol (EN 300 708 section 6),
    i.e. data transmissions based on packet 8/30. This service is most likely
    obsolete today (replaced by mobile Internet).
`Zvbi.PfcDemux`_
    Class for separating data transmitted in **Page Function Clear** teletext
    packets (ETS 300 708 section 4), i.e. using regular packets on a dedicated
    teletext page. Historically this protocol was used for **Nextview EPG**,
    an EPG for analog television. This service is most likely obsolete today.
`Zvbi.XdsDemux`_
    Class for separating "Extended Data Service" from a Closed Caption stream
    (EIA 608).
`Zvbi.DvbDemux`_
    Used internally by the Capture class for de-multiplexing
    transport streams received from a DVB driver, i.e. separating "VBI" data
    from a DVB PES stream (EN 301 472, EN 301 775).
`Zvbi.DvbMux`_
    This class converts raw and/or sliced VBI data to a DVB Packetized
    Elementary Stream or Transport Stream as defined in EN 300 472 "Digital
    Video Broadcasting (DVB); Specification for conveying ITU-R System B
    Teletext in DVB bit-streams" and EN 301 775 "Digital Video Broadcasting
    (DVB); Specification for the carriage of Vertical Blanking Information
    (VBI) data in DVB bit-streams".

.. _Zvbi.Capture:

Class Zvbi.Capture
==================

This class is used for opening a DVB or analog "VBI" device and start
receiving data from it.  The class does not support tuning of a channel.

The constructor creates a capture context with the given parameters.
Afterward, one of the *read* or *pull* methods (see below for hints which
one to use) have to be called periodically for retrieving the data.
Usually this is done within a quasi-infinite "while" loop (possibly in a
separate thread), but most devices will support "select()" and thus allow
asynchronous I/O via event handlers. If everything else fails, you can
also use polling in fixed intervals slightly lower than the (interlaced)
video frame rate (e.g. 2*30 Hz for NTSC, 2*25 Hz for PAL)

The context is automatically deleted and the device closed when the object
is destroyed.

Upon failure, the constructor and all member functions raise exception
*Zvbi.CaptureError*, containing a string describing the cause. (Additional
exception types may be used for specific error cases.)

There are two different types of capture functions: The functions named
*read* copy captured data into a bytes object (where the copying is
usually done at device driver level). In contrast the functions named
*pull* leave the data in internal buffers inside the capture context
and just return a reference to this buffer. Usually this allows the device
driver to avoid any copying, however not all devices support this (e.g.
the Linux DVB driver does not support, i.e. there is no difference in
performance between *read* and *pull*).  When you need to access the
captured data directly via Python, choose the read functions. When you use
functions of this module for further decoding, you should use the pull
functions since these are usually more efficient.

If you do not need "raw" data (i.e. if you do not use the `Zvbi.RawDec`_
class, you should use *read_sliced()* or *pull_sliced()* to avoid the
overhead of returning raw data (which has high bandwidth). DVB devices
will not return raw data regardless of the chosen interface.


Constructor Zvbi.Capture()
--------------------------

There is a single constructor for the capture class that covers all
supported device drivers. The constructor "auto-detects" the type of the
given device by sequentially trying to access the device as DVB, "V4l2"
(i.e. analog Linux video capture device), "bktr" (i.e. FreeBSD analog BSD
video capture device), in this order.

The following shows the complete signature of the constructor:

::

    cap = Zvbi.Capture(dev, dvb_pid=0, proxy=None,
                       services=0, strict=0, buffers=5, scanning=0,
                       trace=False)

The device parameter is mandatory, all others are optional and
keyword-only. The parameters have the following meaning:

:dev:
    Path of the device to open (for Linux usually
    `/dev/dvb/adapter0/demux0` or `/dev/vbi0`)
:proxy:
    When present, this has to be a reference to an instance of class
    `Zvbi.Proxy`_. The constructor will request start of capturing via the
    VBI proxy daemon instead of accessing the device directly. The
    following parameters are still applicable, but are passed to the
    daemon. The proxy does not support DVB devices. If the connection
    fails, the constructor will not attempt direct device access; this
    means the call shuld be repeated without the proxy parameter.
:dvb_pid:
    Specifies the number (PID) of a stream which contains VBI data, when
    the device is a DVB capture card. Else the parameter has no effect.
    If you omit this value, you need to configure it afterwards using
    `Zvbi.Capture.dvb_filter()`_, otherwise there will be no reception.
:buffers:
    Number of device buffers for raw VBI data if the driver supports
    streaming. Use higher values if you cannot guarantee there is no
    latency on reading capture data (e.g. if your GUI runs in the same
    thread). Otherwise one bounce buffer is allocated for
    *Zvbi.Capture.pull()*. Not applicable to DVB.
:scanning:
    Indicates the current norm: 625 for PAL and 525 for NTSC; set to 0 if
    you don't know (you should not attempt to query the device for the
    norm, as this parameter is only required for old v4l (i.e. API v1)
    drivers which don't support video standard query ioctls.)
:services:
    Is a bit-wise OR
    of `VBI_SLICED_*` symbols describing the data services to be decoded.
    See `Zvbi.RawDec.add_services()`_ for details.  If you want to capture
    raw data only, set to `VBI_SLICED_VBI_525`, `VBI_SLICED_VBI_625` or
    both.  If this parameter is omitted, no services will be installed.
    You can do so later with *Zvbi.Capture.update_services()* (Note in this
    case the *reset* parameter to that function will have to be set to
    True.). Not applicable to DVB.
:strict:
    The value can be 0, 1, or 2 for determining which services to allow
    for raw decoding. For details see `Zvbi.RawDec.add_services()`_. Not
    applicable to DVB.
:trace:
    If True, enables output of progress messages on `sys.stderr`.

As noted, not all parameters are applicable to each driver. Therefore it
is not fully transparent to the application which driver is used. A
portable application should support the following use-cases:

**Capturing from a DVB driver**:
Note the PID value can usually be derived from the PID for video in
`channels.conf` by adding 3. ::

    opt_device = "/dev/dvb/adapter0/demux0"
    opt_pid = 104
    opt_verbose = False

    cap = Zvbi.Capture(opt_device, dvb_pid=opt_dvb, trace=opt_verbose)

**Capturing from an analog capture card**: ::

    opt_device = "/dev/vbi0"
    opt_services = Zvbi.VBI_SLICED_TELETEXT_B
    opt_strict = 0
    opt_buf_count = 5
    opt_verbose = False

    cap = Zvbi.Capture(opt_device, services=opt_services, strict=opt_strict,
                       buffers=opt_buf_count, trace=opt_verbose)

**Capturing from an analog capture card via proxy**:
Whenever possible, the proxy should be used instead of opening analog
devices directly, since it allows the user to start multiple VBI clients
concurrently. When this function fails (usually because the user hasn't
started the proxy daemon) applications should automatically fall back to
opening the device directly. ::

    opt_device = "/dev/vbi0"
    opt_services = Zvbi.VBI_SLICED_TELETEXT_B
    opt_strict = 0
    opt_buf_count = 5
    opt_verbose = False
    try:
        proxy = Zvbi.Proxy(opt_device, appname="...", appflags=0, trace=opt_verbose)

        cap = Zvbi.Capture(opt_device, proxy=proxy,
                           services=opt_services, strict=opt_strict,
                           buffers=opt_buf_count, trace=opt_verbose)
    except Zvbi.ProxyError, Zvbi.CaptureError:
        # try again without proxy
        cap = Zvbi.Capture(opt_device,
                           services=opt_services, strict=opt_strict,
                           buffers=opt_buf_count, trace=opt_verbose)

The first call of Zvbi.Capture() in the example establishes a new
connection to a VBI proxy to open a VBI or DVB device for capturing.  On
side of the proxy daemon, the given device is opened and initialized,
equivalently as it would be done locally.  If the creation succeeds, and
any of the requested services are available, capturing is started and all
captured data is forwarded transparently to the client. See
`Zvbi.Proxy`_ for details.

Zvbi.Capture.read_raw()
-----------------------

::

    cap_data = cap.read_raw(timeout_ms)

Read a raw VBI frame from the capture device and return it within an
object of type *ZvbiCapture.Result*, which is a named tuple with the
following elements:

0. *timestamp*: Timestamp indicating when the data was captured; the
   timestamp is the number of seconds and fractions since 1970-01-01 00:00
   of type *float*
1. *sliced_lines*: Always set to zero here.
2. *raw_buffer*: Bytes object consecutively containing raw data of all
   captured VBI lines. (Length of a line can be queried via method
   `Zvbi.Capture.parameters()`_: attribute *bytes_per_line*.)
3. *sliced_buffer*: Always set to *None* here.

Parameter *timeout_ms* gives the limit for waiting for data in
milliseconds; if no data arrives within the timeout, the function
raises exception *Zvbi.CaptureError* with text indicating timeout.
The same exception is raised upon error indications from the device.
Note the function may fail if the device does not support
reading data in raw format.

Zvbi.Capture.read_sliced()
--------------------------

::

    cap_data = cap.read_sliced(timeout_ms)

Read a sliced VBI frame from the capture context and return it within an
object of type *ZvbiCapture.Result*, which is a named tuple with the
following elements:

0. *timestamp*: Timestamp indicating when the data was captured; the
   timestamp is the number of seconds and fractions since 1970-01-01 00:00
   of type *float*
1. *sliced_lines*: Number of valid lines in the sliced buffer
2. *raw_buffer*: Always set o *None* here
3. *sliced_buffer*: Object of type *CaptureSlicedBuf*, containing data
   data of sliced lines. For efficiency the data is stored internally
   within a C structure, but can be accessed by Python either by using an
   iterator, or by sub-scripting with indices in range
   `[0 : cap_data.sliced_lines]`. However usually one just transparetnly
   forwards the *sliced_buffer* to `Zvbi.ServiceDec.decode()`_.

Parameter *timeout_ms* gives the limit for waiting for data in
milliseconds; if no data arrives within the timeout, the function
raises exception *Zvbi.CaptureError* with text indicating timeout.
The same exception is raised upon error indications from the device.
Note the function may fail if the device does not support
reading data in raw format.

Note: it's generally more efficient to use *pull_sliced()*
instead, as that one may avoid having to copy sliced data into the
given buffer (e.g. for the VBI proxy)

Zvbi.Capture.read()
-------------------

::

    cap_data = cap.read(timeout_ms)

This function is a combination of *read_raw()* and *read_sliced()*, i.e.
reads a VBI frame from the capture context and returns both the raw data
and the results of "slicing" the raw data. The results are stored in an
object of type *ZvbiCapture.Result*, which is a named tuple with the
following elements:

0. *timestamp*: Timestamp indicating when the data was captured; the
   timestamp is the number of seconds and fractions since 1970-01-01 00:00
   of type *float*
1. *sliced_lines*: Number of valid lines in the sliced buffer
2. *raw_buffer*: Bytes object consecutively containing raw data of all
   captured VBI lines. (Length of a line can be queried via method
   `Zvbi.Capture.parameters()`_: attribute *bytes_per_line*.)
   The element may also be *None* if the driver does not support raw data
   (e.g. DVB devices)
3. *sliced_buffer*: Object of type *CaptureSlicedBuf*, containing data
   data of sliced lines. For efficiency the data is stored internally
   within a C structure, but can be accessed by Python either by using an
   iterator, or by sub-scripting with indices in range
   `[0 : cap_data.sliced_lines]`. However usually one just transparetnly
   forwards the *sliced_buffer* to `Zvbi.ServiceDec.decode()`_.

Note: Depending on the driver, captured raw data may have to be copied
from the capture buffer into the given buffer (e.g. for v4l2 streams which
use memory mapped buffers.)  It's generally more efficient to use one of
the following "pull" interfaces. Also, if you don't require raw data it's
even more efficient to use *pull_sliced()* or *read_sliced()*.

Zvbi.Capture.pull_raw()
-----------------------

::

    cap_data = cap.pull_raw(timeout_ms)

Read a raw VBI frame from the capture context, which is returned in
form of an object of type *ZvbiCapture.Result*. **Note**: The returned
*raw_buffer* remains valid only until the next call to this or any other
*pull* function.

The result of type *ZvbiCapture.Result* is a named tuple with the
following elements:

0. *timestamp*: Timestamp indicating when the data was captured; the
   timestamp is the number of seconds and fractions since 1970-01-01 00:00
   of type *float*
1. *sliced_lines*: Always set to zero here.
2. *raw_buffer*: Object of type *Zvbi.CaptureRawBuf*, encapsulating a
   reference to an internal buffer containing captured raw data.
3. *sliced_buffer*: Always set to *None* here.

The *raw_buffer* can be passed to `Zvbi.RawDec.decode()`_.  If you
need to process the data by Python code, use `Zvbi.Capture.read_raw()`_
instead.  (When processing raw data, *read_raw()* is more efficient as it
may avoid copying the data out of the internal buffer into a Python
object.)

Parameter *timeout_ms* gives the limit for waiting for data in
milliseconds; if no data arrives within the timeout, the function
raises exception *Zvbi.CaptureError* with text indicating timeout.
The same exception is raised upon error indications from the device.
Note the function may fail if the device does not support
reading data in raw format.


Zvbi.Capture.pull_sliced()
--------------------------

::

    cap_data = cap.pull_sliced(timeout_ms)

Read a sliced VBI frame from the capture context, which is returned in
form of an object of type *ZvbiCapture.Result*. **Note**: The returned
*sliced_buffer* remains valid only until the next call to this or any
other *pull* function.

Read a sliced VBI frame from the capture context, which is returned in
*ref* in form of a blessed reference to an internal buffer.

The result of type *ZvbiCapture.Result* is a named tuple with the
following elements:

0. *timestamp*: Timestamp indicating when the data was captured; the
   timestamp is the number of seconds and fractions since 1970-01-01 00:00
   of type *float*
1. *sliced_lines*: Number of valid lines in the sliced buffer
2. *raw_buffer*: Always set to *None* here.
3. *sliced_buffer*: Object of type *CaptureSlicedBuf*, encapsulating a
   reference to an internal buffer containing sliced data.
   The data can be accessed by Python either by using an
   iterator, or by sub-scripting with indices in range
   `[0 : cap_data.sliced_lines]`. However usually one just transparetnly
   forwards the *sliced_buffer* to `Zvbi.ServiceDec.decode()`_.

Parameter *timeout_ms* gives the limit for waiting for data in
milliseconds; if no data arrives within the timeout, the function
raises exception *Zvbi.CaptureError* with text indicating timeout.
The same exception is raised upon error indications from the device.
Note the function may fail if the device does not support
reading data in raw format.

Zvbi.Capture.pull()
-------------------

::

    cap_data = cap.pull(timeout_ms)

This function is a combination of *pull_raw()* and *pull_sliced()*, i.e.
reads a VBI frame from the capture context and returns both the raw data
and the results of "slicing" the raw data.  **Note**: The returned
*raw_buffer* and *sliced_buffer* remain valid only until the next call to
this or any other *pull* function.

The function returns an object of type *ZvbiCapture.Result*, which is a
named tuple with the following elements:

0. *timestamp*: Timestamp indicating when the data was captured; the
   timestamp is the number of seconds and fractions since 1970-01-01 00:00
   of type *float*
1. *sliced_lines*: Number of valid lines in the sliced buffer
2. *raw_buffer*: Object of type *Zvbi.CaptureRawBuf*, encapsulating a
   reference to an internal buffer containing captured raw data.
   The element may also be *None* if the driver does not support raw data
   (e.g. DVB devices)
3. *sliced_buffer*: Object of type *CaptureSlicedBuf*, encapsulating a
   reference to an internal buffer containing sliced data.
   The data can be accessed by Python either by using an
   iterator, or by sub-scripting with indices in range
   `[0 : cap_data.sliced_lines]`. However usually one just transparetnly
   forwards the *sliced_buffer* to `Zvbi.ServiceDec.decode()`_.

Parameter *timeout_ms* gives the limit for waiting for data in
milliseconds; if no data arrives within the timeout, the function
raises exception *Zvbi.CaptureError* with text indicating timeout.
The same exception is raised upon error indications from the device.
Note the function may fail if the device does not support
reading data in raw format.

Zvbi.Capture.parameters()
-------------------------

::

    params = cap.parameters()

Returns an instance of class `Zvbi.RawParams`_ describing the physical
parameters of the VBI source. See the description of that class for a
description of attributes.

Modifying the attributes of the returned object has no effect on the
*Capture* instance. To control raw decoding, pass the returned (and
possibly modified) parameters when instantiating class `Zvbi.RawDec`_ and
then use that class for decoding instead of the *sliced_buffer* output of
the *Capture* member functions.

**Note**: For DVB devices this function only returns dummy parameters, as
no "raw decoding" is performed in this case. In particular the sampling
format will be zero, which is an invalid value, so this can be used for
detecting this case.


Zvbi.Capture.update_services()
------------------------------

::

    services = cap.update_services(services, reset=False, commit=False, strict=0)

Not applicable to DVB:
Adds and/or removes one or more services to an already initialized capture
context.  Can be used to dynamically change the set of active services.

Internally the function will restart parameter negotiation with the
VBI device driver and then call *add_services()* on the internal raw
decoder context.  You may set *reset* to rebuild your service mask from
scratch.  Note that the number of VBI lines may change with this call
even if the function fails and raises an exception.

Result: The function returns a bit-mask of supported services among those
requested (not including previously added services), 0 upon errors.

:services:
    An integer consisting of a bit-wise OR of one or more `VBI_SLICED_*`
    constants describing the data services to be decoded.

:reset:
    When this optional parameter is set True, the method clears all
    previous services before adding new ones (by invoking
    `Zvbi.RawDec.reset()`_ at the appropriate time.)

:commit:
    When this optional parameter is set True, the method applies all
    previously added services to the device; when doing subsequent calls
    of this function, commit should be set only for the last call.
    Reading data cannot continue before changes were committed (because
    capturing has to be suspended to allow resizing the VBI image.)  Note
    this flag is ignored when using the VBI proxy.

:strict:
    The meaning of this optional parameter is as described for
    `Zvbi.RawDec.add_services()`_, as that function is used internally by
    libzvbi. The parameter defaults to 0.

The function returns an integer value with bit-wise OR of `VBI_SLICED_*`
services actually decodable.

Zvbi.Capture.fd()
-----------------

::

    cap.fd()

This function returns the file descriptor used to read from the
capture context's device.  Note when using the proxy this will not
be the actual device, but a socket instead.  Some devices may also
return -1 if they don't have anything similar, or upon internal errors.

The descriptor is intended be used in a *select(2)* syscall. The
application especially must not read or write from it and must never
close the handle (instead destroy the capture context to free the
device.) In other words, the file handle is intended to allow capturing
asynchronously in the background; The handle will become readable
when new data is available.

Zvbi.Capture.get_scanning()
---------------------------

::

    scanning = cap.get_scanning()

This function is intended to allow the application to check for
asynchronous norm changes, i.e. by a different application using the
same device.  The function queries the capture device for the current
norm and returns value 625 for PAL/SECAM norms, 525 for NTSC;
0 if unknown, -1 on error.

Zvbi.Capture.flush()
--------------------

::

    cap.flush()

After a channel change this function should be used to discard all
VBI data in intermediate buffers which may still originate from the
previous TV channel. The function returns `None`.

Zvbi.Capture.get_fd_flags()
---------------------------

::

    flags = cap.get_fd_flags()

Returns properties of the capture context's device. The result is an
integer value containing a bit-wise OR of one or more of the following
constants:

VBI_FD_HAS_SELECT:
    Is set when *select(2)* can be used on the file handle returned by
    *cap.fd()* to wait for new data on the capture device file handle.

VBI_FD_HAS_MMAP:
    Is set when the capture device supports "user-space DMA".  In this case
    it's more efficient to use one of the "pull" functions to read raw data
    because otherwise the data has to be copied once more into the passed buffer.

VBI_FD_IS_DEVICE:
    Is not set when the capture device file handle is not the actual device.
    In this case it can only be used for select(2) and not for ioctl(2)

Zvbi.Capture.dvb_filter()
-------------------------

::

    cap.dvb_filter(pid)

Programs the DVB device transport stream demultiplexer to filter
out PES packets with the given *pid*. The meaning of the parameter is
equivalent to the *pid* parameter to the constructor.

Zvbi.Capture.dvb_last_pts()
---------------------------

::

    cap.dvb_last_pts()

Returns the presentation time stamp (33 bits) associated with the data
last read from the capture context. The PTS refers to the first sliced
VBI line, not the last packet containing data of that frame.

Note timestamps returned by VBI capture read functions contain
the sampling time of the data, that is the time at which the
packet containing the first sliced line arrived.

.. _Zvbi.CaptureBuf:

Class Zvbi.CaptureBuf
=====================

For reasons of efficiency the data is not immediately converted into
Python structures. Functions of the "read" variety return a
bytes object which contains data of all VBI lines.
Functions of the "pull" variety return a binary reference
(i.e. a C pointer) which cannot be used by Python for other purposes
than passing it to further processing functions.  To process either
read or pulled data by Python code, use iteration:

::

    cap_data = cap.pull(2000)
    for data, slc_id, line in cap_data.sliced_buffer:
        ...

The function takes a buffer which was filled by one of the slicer
or capture & slice functions and a line index. The index must be lower
than the line count returned by the slicer.  The function returns
a list of three elements: sliced data from the respective line in
the buffer, slicer type (`VBI_SLICED_...`) and physical line number.

The structure of the data returned in the first element depends on
the kind of data in the VBI line (e.g. for teletext it's 42 bytes,
partly hamming 8/4 and parity encoded; the content in the scalar
after the 42 bytes is undefined.)

.. _Zvbi.RawDec:

Class Zvbi.RawDec
=================

The functions in this section allow converting raw VBI samples (i.e. a
digitized image of the transmitted analog waveform) to payload data bytes.
This class is not applicable to DVB.

These functions are used internally by libzvbi if you use the slicer
functions of the capture object (e.g. *pull_sliced()*). This class
is useful only when capturing raw data only (e.g. *pull_raw()*),
allowing your application to take full control of slicing raw data.

After constructing an image and configuring parameters, the actual work is
done by `Zvbi.RawDec.decode()`_, which you'd call on the data of each
captured VBI frame.

Constructor Zvbi.RawDec()
-------------------------

::

    cap = Zvbi.Capture("/dev/vbi", services=VBI_SLICED_CAPTION_525)
    rd = Zvbi.RawDec(cap)

Creates and initializes a new raw decoder context. Parameter *ref*
specifies the physical parameters of the raw VBI image, such as the
sampling rate, number of VBI lines etc.  The parameter can be either
a reference to a capture context (`Zvbi.Capture`_)
or raw capture parameters of type `Zvbi.RawParams`_. (See description of
that class for a list of attributes.)

A properly initialized instance of *Zvbi.RawParams* can be obtained either
via method `Zvbi.Capture.parameters()`_ or `Zvbi.RawDec.parameters()`_.
In case an instance of `Zvbi.Capture`_ is used as parameter to the
constructor, decoder parameters are retrieved internally using
`Zvbi.Capture.parameters()` for convenience.

Zvbi.RawDec.parameters()
------------------------

::

    services, max_rate, par = Zvbi.RawDec.parameters(services, scanning)

This is a **static** member function. The function calculates the sampling
parameters required to receive and decode the requested data services.
This function can be used to initialize hardware parameters prior to
calling `Zvbi.RawDec.add_services()`_.  The returned sampling format is fixed to
`VBI_PIXFMT_YUV420`, and attribute *bytes_per_line* is set to a reasonable
minimum.

Input parameters:

:services:
    This integer value contains a bit-wise OR of `VBI_SLICED_*` constants.
    Here (and only here) you can add `VBI_SLICED_VBI_625` or
    `VBI_SLICED_VBI_525` to include all VBI scan lines in the calculated
    sampling parameters.
:scanning:
    If *scanning* is set to 525 only NTSC services are accepted; if set to
    625 only PAL/SECAM services are accepted. When scanning is 0, the norm
    is determined from the requested services; an ambiguous set will
    result in undefined behavior.

The function returns a tuple containing the following three results:

0. An integer value containing a bit-wise OR of a sub-set of
   `VBI_SLICED_*` constants describing the data services covered by the
   calculated sampling parameters returned in *href*. This excludes services
   the libzvbi raw decoder cannot decode assuming the specified physical
   parameters.

1. Calculated maximum rate, which is to the highest data bit rate
   in **Hz** of all services requested (The sampling rate should be at least
   twice as high; attribute `sampling_rate` will be set by libzvbi to a more
   reasonable value of 27 MHz derived from ITU-R Rec. 601.)

2. An instance of class `Zvbi.RawParams`_ which contains the calculated
   sampling parameters. The content is described as for function
   `Zvbi.Capture.parameters()`_

Zvbi.RawDec.reset()
-------------------

::

    rd.reset()

Resets the raw decoder context. This removes all previously added services
to be decoded (if any) but does not touch the sampling parameters. You
are free to change the sampling parameters after calling this.

Zvbi.RawDec.add_services()
--------------------------

::

    services = rd.add_services(services, strict)

After you initialized the sampling parameters in raw decoder context
(according to the abilities of your VBI device), this function adds one
or more data services to be decoded. The libzvbi raw VBI decoder can
decode up to eight data services in parallel. You can call this function
while already decoding, it does not change sampling parameters and you
must not change them either after calling this.

Input parameters:

:services:
    This integer value contains a bit-wise OR of `VBI_SLICED_*` constants.
    (see also description of the *parameters* function above.)

:strict:
    The parameter can be set to 0, 1 or 2 for requesting requests loose,
    reliable or strict matching of sampling parameters respectively. For
    example if the data service requires knowledge of line numbers while
    they are not known, value 0 will accept the service (which may work if
    the scan lines are populated in a non-confusing way) but values 1 or 2
    will not. If the data service may use more lines than are sampled,
    value 1 will still accept but value 2 will not. If unsure, set to 1.

The function returns an integer value containing a bit-wise OR of
`VBI_SLICED_*` constants describing the data services that actually can be
decoded. This excludes those services not decodable given sampling
parameters of the raw decoder context.

Zvbi.RawDec.check_services()
----------------------------

::

    services = rd.check_services(services, strict=0)

Check and return which of the given services can be decoded with
current physical parameters at a given strictness level.

See `Zvbi.RawDec.add_services()`_ for details on parameter semantics.

Zvbi.RawDec.remove_services()
-----------------------------

::

    services = rd.remove_services(services)

Removes one or more data services given in input parameter *services*
to be decoded from the raw decoder context.  This function can be called
at any time and does not touch sampling parameters stored in the context.

Returns a set of `VBI_SLICED_*` constants describing the remaining
data services that will be decoded.

Zvbi.RawDec.resize()
--------------------

::

    rd.resize(start_a, count_a, start_b, count_b)

Grows or shrinks the internal state arrays for VBI geometry changes.
Returns `None`.

Zvbi.RawDec.decode()
--------------------

::

    n_lines, buf = rd.decode(ref)

This is the main service offered by the raw decoder: Decodes a raw VBI
image given in *ref*, consisting of several scan lines of raw VBI data,
into sliced VBI lines in *buf*. The output is sorted by line number.

The input *ref* can either be a bytes object filled by one of the
`Zvbi.Capture.read()`_ kind of capture functions (or any bytes object
filled with a byte sequence with the correct number of samples for the
current geometry), or an object of type *Zvbi.CaptureRawBuf* (sub-class
of `Zvbi.CaptureBuf`_) as returned by the "pull" kind of capture functions.

Return value is the non returns a tuple with two elements: The number of
sliced lines, and a buffer containing the sliced output data. The format
of the output buffer is the same as described for
`Zvbi.Capture.read_sliced()`_.
Upon errors the function raises exception *Zvbi.RawDecError*.

Note this function attempts to learn which lines carry which data
service, or none, to speed up decoding.  Hence you must use different
raw decoder contexts for different devices.


.. _Zvbi.RawParams:

Class Zvbi.RawParams
====================

This is a simple parameter container, encapsulating parameters of raw
captured data (i.e. *raw_buffer* result produced by methods
*Zvbi.Capture.read_raw()* et.al.), or for instantiating a raw decoder
of class `Zvbi.RawDec`_.

The class has the following attributes:

scanning:
    Either 525 (M/NTSC, M/PAL) or 625 (PAL, SECAM), describing the scan
    line system all line numbers refer to.

sampling_format:
    Format of the raw VBI data (one of the `VBI_PIXFMT_*` constants,
    e.g. `VBI_PIXFMT_YUV420`; see enum *vbi_pixfmt*)

sampling_rate:
    Sampling rate in Hz (i.e. the number of samples or pixels captured
    per second.)

bytes_per_line:
    Number of samples or pixels captured per scan line, in bytes. This
    determines the raw VBI image width and you want it large enough to
    cover all data transmitted in the line (with headroom).

offset:
    The distance from 0H (leading edge hsync, half amplitude point) to
    the first sample (pixel) captured, in samples (pixels). You want an
    offset small enough not to miss the start of the data transmitted.

start_a, start_b:
    First scan line to be captured in the first and second half-frame
    respectively. Numbering is according to the ITU-R line numbering
    scheme (see *vbi_sliced*). Set to zero if the exact line number isn't
    known.

count_a, count_b:
    Number of scan lines captured in the first and second half-frame
    respectively.  This can be zero if only data from one field is
    required. The sum `count_a + count_b` determines the raw VBI image
    height.

interlaced:
    In the raw vbi image, normally all lines of the second field are
    supposed to follow all lines of the first field. When this flag is
    set, the scan lines of first and second field will be interleaved in
    memory. This implies count_a and count_b are equal.

synchronous:
    Fields must be stored in temporal order, i. e. as the lines have been
    captured. It is assumed that the first field is also stored first in
    memory, however if the hardware cannot reliable distinguish fields this
    flag shall be cleared, which disables decoding of data services
    depending on the field number.



.. _Zvbi.Proxy:

Class Zvbi.Proxy
================

This class is used for receiving sliced or raw data from VBI proxy daemon.
Using the daemon instead of capturing directly from a VBI device allows
multiple applications to capture concurrently, e.g. to decode multiple data
services.

Constructor Zvbi.Proxy
----------------------

::

    proxy = Zvbi.Proxy(dev, appname, appflags=0, trace=False)

    cap = Zvbi.Capture( ..., proxy=proxy )

Creates and returns a new proxy context, or raises exception *Zvbi.ProxyError*
upon error.  (Note in reality this call will always succeed, since a connection
to the proxy daemon isn't established until you actually open a capture context
when instantiating `Zvbi.Capture`_ with a reference to `Zvbi.Proxy`_.)

Parameters:

:dev:
    Specifies the name of the device to open, usually one of `/dev/vbi0` and up.
    The device name has to match that used by the deamon, else the daemon will
    refuse the connection, so that `Zvbi.Capture`_ calls back to direct access
    to the device.

:client_name:
    Names the client application, typically identical to ``sys.argv[0]``
    (without the path though). Can be used by the proxy daemon for fine-tuning
    scheduling, or for presenting the user with a list of currently connected
    applications.

:flags:
    Contains zero or a bit-wise OR of `VBI_PROXY_CLIENT_*` flags.

:trace:
    If True, enables output of progress messages on ``sys.stderr``.

Proxy.set_callback()
--------------------

::

    proxy.set_callback(callback, user_data=None)

Installs or removes a callback function for asynchronous messages (e.g.
channel change notifications.)  The callback function is typically invoked
while processing a read from the capture device.

Input parameters are a callable object *callback* and an optional object
*user_data* which is passed through to the callback function unchanged.
Call without arguments to remove the callback again.

The callback function will receive the event mask (i.e. one of the
constants `VBI_PROXY_EV_*` in the following list) and, if provided,
*user_data* as parameters.

* *VBI_PROXY_EV_CHN_GRANTED*:
  The channel control token was granted, so that the client may now
  change the channel.  Note: the client should return the token after
  the channel change was completed (the channel will still remain
  reserved for the requested time.)

* *VBI_PROXY_EV_CHN_CHANGED*:
  The channel (e.g. TV tuner frequency) was changed by another proxy
  client.

* *VBI_PROXY_EV_NORM_CHANGED*:
  The TV norm was changed by another client (in a way which affects VBI,
  e.g. changes between PAL/SECAM are ignored.)  The client must update
  its services, else no data will be forwarded by the proxy until the
  norm is changed back.

* *VBI_PROXY_EV_CHN_RECLAIMED*:
  The proxy daemon requests to return the channel control token.  The
  client is no longer allowed to switch the channel and must immediately
  reply with a channel notification with flag `VBI_PROXY_CHN_TOKEN`

* *VBI_PROXY_EV_NONE*:
  No news.

Proxy.get_driver_api()
----------------------

This method can be used for querying which driver is behind the
device which is currently opened by the VBI proxy daemon.
Applications which only use libzvbi's capture API need not
care about this.  The information is relevant to applications
which need to switch TV channels or norms.

Returns an identifier describing which API is used on server side,
i.e. one of the symbols
`VBI_API_V4L1`,
`VBI_API_V4L2`,
`VBI_API_BKTR` or
`VBI_API_UNKNOWN` upon error.
The function will fail if the client is currently not connected to
the proxy daemon, i.e. VBI capture has to be started first.

Proxy.channel_request
---------------------

::

    Proxy.channel_request(chn_prio [, profile])

This method is used to request permission to switch channels or norm.
Since the VBI device can be shared with other proxy clients, clients should
wait for permission, so that the proxy daemon can fairly schedule channel
requests.

Scheduling differs at the 3 priority levels. For available priority levels
for *chn_prio* see constants `VBI_CHN_PRIO_*`.  At background level channel
changes are coordinated by introduction of a virtual token: only the
one client which holds the token is allowed to switch channels. The daemon
will wait for the token to be returned before it's granted to another
client.  This way conflicting channel changes are avoided.  At the upper
levels the latest request always wins.  To avoid interference, the
application still might wait until it gets indicated that the token
has been returned to the daemon.

The token may be granted right away or at a later time, e.g. when it has
to be reclaimed from another client first, or if there are other clients
with higher priority.  If a callback has been registered, the respective
function will be invoked when the token arrives; otherwise
*proxy.has_channel_control()*> can be used to poll for it.

To set the priority level to "background" only without requesting a channel,
omit the *profile* parameter. Else, this parameter must be a
dict with the following members: "sub_prio", "allow_suspend",
"min_duration" and "exp_duration".

Zvbi.Proxy.channel_notify()
---------------------------

::

    proxy.channel_notify(notify_flags [, scanning])

Sends channel control request to proxy daemon. Parameter
*notify_flags* is an OR of one or more of the following constants:

* *VBI_PROXY_CHN_RELEASE*:
  Revoke a previous channel request and return the channel switch
  token to the daemon.

* *VBI_PROXY_CHN_TOKEN*:
  Return the channel token to the daemon without releasing the
  channel; This should always be done when the channel switch has
  been completed to allow faster scheduling in the daemon (i.e. the
  daemon can grant the token to a different client without having
  to reclaim it first.)

* *VBI_PROXY_CHN_FLUSH*:
  Indicate that the channel was changed and VBI buffer queue
  must be flushed; Should be called as fast as possible after
  the channel and/or norm was changed.  Note this affects other
  clients' capturing too, so use with care.  Other clients will
  be informed about this change by a channel change indication.
* *VBI_PROXY_CHN_NORM*:

  Indicate a norm change.  The new norm should be supplied in
  the scanning parameter in case the daemon is not able to
  determine it from the device directly.

* *VBI_PROXY_CHN_FAIL*:
  Indicate that the client failed to switch the channel because
  the device was busy. Used to notify the channel scheduler that
  the current time slice cannot be used by the client.  If the
  client isn't able to schedule periodic re-attempts it should
  also return the token.

Proxy.channel_suspend()
-----------------------

::

    proxy.channel_suspend(cmd)

Request to temporarily suspend capturing (if *cmd* is
`VBI_PROXY_SUSPEND_START`) or revoke a suspension (if *cmd*
equals `VBI_PROXY_SUSPEND_STOP`.)

Zvbi.Proxy.device_ioctl()
-------------------------

::

    proxy.device_ioctl(request, arg)

This method allows manipulating parameters of the underlying
VBI device.  Not all ioctls are allowed here.  It's mainly intended
to be used for channel enumeration and channel/norm changes.
The request codes and parameters are the same as for the actual device.
The caller has to query the driver API via *proxy.get_driver_api()*>
first and use the respective ioctl codes, same as if the device would
be used directly.

Parameters and results are equivalent to the called **ioctl** operation,
i.e. *request* is an IO code and *arg* is a packed binary structure.
After the call *arg* may be modified for operations which return data.
You must make sure the result buffer is large enough for the returned data.
Use Perl's *pack* to build the argument buffer. Example:

::

  # get current config of the selected channel
  vchan = struct.pack("ix32iLss", channel, 0, 0, 0, norm);
  proxy.device_ioctl(VIDIOCGCHAN, vchan);

The result is 0 upon success, else and `!` set appropriately.  The function
also will fail with error code `EBUSY` if the client doesn't have permission
to control the channel.

Proxy.get_channel_desc()
------------------------

Retrieve info sent by the proxy daemon in a channel change indication.
The function returns a list with two members: scanning value (625, 525 or 0)
and a boolean indicator if the change request was granted.

Proxy.has_channel_control()
---------------------------

Returns True if client is currently allowed to switch channels, else False.

See **examples/proxy-test.pl** for examples how to use these functions.


.. _Zvbi.ServiceDec:

Class Zvbi.ServiceDec
=====================

This class is used for high level decoding of sliced data received from
an instance of the *Capture* class or the raw decoder (`Zvbi.RawDec`_).
Decoded data is stored in caches for each service. The application can
be notified via callbacks about various events. Various interfaces allow
extracting decoded data from the caches.

Constructor Zvbi.ServiceDec()
-----------------------------

::

  vt = Zvbi.ServiceDec()
  vt.event_handler_register(Zvbi.VBI_EVENT_TTX_PAGE, pg_handler)

Creates and returns a new data service decoder instance. The constructor
does not take any parameters. **However**: The type of data services to
be decoded is determined by the type of installed callbacks. Hence for
the class to do any actual decoding, you must install at least one
callback using `Zvbi.ServiceDec.event_handler_register()`_ after
construction.

Zvbi.ServiceDec.decode()
------------------------

::

  vt.decode(sliced_buf)

This is the main service offered by the data service decoder: The method
decodes sliced VBI data from a video frame, updates the decoder state and
invokes callback functions for registered events. Note this function has
to be called for each received frame, even if it did not contain any
sliced data, because the decoder otherwise assumes a frame was lost and
may reset decoding state.

Input parameter *sliced_buf* has to be an instance of class
*CaptureSlicedBuf* returned by *read* and *pull* capture functions of
`Zvbi.Capture`_ class. The function always returns *None*. As a
side-effect, registered callbacks are invoked.

Zvbi.ServiceDec.decode_bytes()
------------------------------

::

  vt.decode_bytes(data, n_lines, timestamp)

This method is an alternate interface to *decode()*, allowing to insert
data from external sources, such as sliced data stored in a file.  Thus
the discrete method parameters replace attributes otherwise stored in
*CaptureSlicedBuf*:

:data:
    Is a bytes-like object containing concatenated sliced data lines. Each
    line is a binary packed format "=LL56c", containing the service ID
    `VBI_SLICED_*`, the number of the (analog) line from where the line
    was captured, followed by 56 bytes slicer output data.

:n_lines:
    Gives the number of valid lines in the sliced data buffer. The value
    must be between 0 and len(data) / (2*4+56) (i.e. the maximum number of
    records in the given data buffer)

:timestamp:
    This should be a copy of the *timestamp* value returned by the *read*
    and *pull* capture functions of `Zvbi.Capture`_ class.  The timestamps
    are expected to advance by 1/30 to 1/25 seconds for each call to this
    function. Different steps will be interpreted as dropped frames, which
    starts a re-synchronization cycle, eventually a channel switch may be
    assumed which resets even more decoder state. So this function must be
    called even if a frame did not contain any useful data (i.e. with
    parameter *n_lines* equal 0)

Zvbi.ServiceDec.channel_switched()
----------------------------------

::

    vt.channel_switched( [nuid] )

Call this after switching away from the channel (RF channel, video input
line, ... - i.e. after switching the network) from which this context
used to receive VBI data, to reset the decoding context accordingly.
This includes deletion of all cached Teletext and Closed Caption pages
from the cache.  Optional parameter *nuid* is currently unused by
libzvbi and defaults to zero.

The decoder attempts to detect channel switches automatically, but this
does not work reliably, especially when not receiving and decoding Teletext
or VPS (since only these usually transmit network identifiers frequently
enough.)

Note the reset is not executed until the next frame is about to be
decoded, so you may still receive "old" events after calling this. You
may also receive blank events (e. g. unknown network, unknown aspect
ratio) revoking a previously sent event, until new information becomes
available.

Zvbi.ServiceDec.classify_page()
-------------------------------

::

    (type, subno, lang) = vt.classify_page(pgno)

This function queries information about the named page. The return value
is a tuple consisting of three scalars: page number, sub-page number,
and language  Their meaning depends on the data service to which the
given page belongs:

For Closed Caption pages (*pgno* value in range 1 ... 8) *subno* will
always be zero, *language* set or an empty string. *type* will be
`VBI_SUBTITLE_PAGE` for page 1 ... 4 (Closed Caption channel 1 ... 4),
`VBI_NORMAL_PAGE` for page 5 ... 8 (Text channel 1 ... 4), or
`VBI_NO_PAGE` if no data is currently transmitted on the channel.

For Teletext pages (*pgno* in range hex 0x100 ... 0x8FF) *subno*
returns the highest sub-page number used. Note this number can be larger
(but not smaller) than the number of sub-pages actually received and
cached. Still there is no guarantee the advertised sub-pages will ever
appear or stay in cache. Special value 0 means the given page is a
"single page" without alternate sub-pages. (Hence value 1 will never
be used.) *language* currently returns the language of subtitle pages,
or an empty string if unknown or the page is not classified as
`VBI_SUBTITLE_PAGE`.

Note: The information returned by this function is volatile: When more
information becomes available, or when pages are modified (e. g. activation
of subtitles, news updates, program related pages) sub-page numbers can
increase or page types and languages can change.

Zvbi.ServiceDec.set_brightness()
--------------------------------

::

    vt.set_brightness(brightness)

Change brightness of text pages, this affects the color palette of pages
fetched with *fetch_vt_page()* and *fetch_cc_page()*.
Parameter *brightness* is in range 0 ... 255, where 0 is darkest,
255 brightest. Brightness value 128 is default.

Zvbi.ServiceDec.set_contrast()
------------------------------

::

    vt.set_contrast(contrast)

Change contrast of text pages, this affects the color palette of pages
fetched with *vt.fetch_vt_page()* and *vt.fetch_cc_page()*.
Parameter *contrast* is in range -128 to 127, where -128 is inverse,
127 maximum. Contrast value 64 is default.

Zvbi.ServiceDec.teletext_set_default_region()
---------------------------------------------

::

    vt.teletext_set_default_region(default_region)

The original Teletext specification distinguished between
eight national character sets. When more countries started
to broadcast Teletext the three bit character set id was
locally redefined and later extended to seven bits grouping
the regional variants. Since some stations still transmit
only the legacy three bit id and we don't ship regional variants
of this decoder as TV manufacturers do, this function can be used to
set a default for the extended bits. The "factory default" is 16.

Parameter *default_region* is a value between 0 ... 80, index into
the Teletext character set table according to ETS 300 706,
Section 15 (or libzvbi source file lang.c). The three last
significant bits will be replaced.

Zvbi.ServiceDec.fetch_vt_page()
-------------------------------

::

    pg = vt.fetch_vt_page(pgno, [subno],
                          max_level=Zvbi.VBI_WST_LEVEL_3p5,
                          display_rows=25,
                          navigation=True)

Fetches a Teletext page designated by parameters *pgno* and optionally *subno*
from the cache, formats and returns it as an instance of `Zvbi.Page`_.  The
object can then be used to extract page content, or be passed to the
various libzvbi methods working on page objects, such as the export
functions.

The function raises exception *ServiceDecError* if the page is not cached
or could not be formatted for other reasons, for instance is a data page
not intended for display. Level 2.5/3.5 pages which could not be formatted
e. g.  due to referencing data pages not in cache are formatted at a lower
level.

Input parameters:

:page:
    Teletext page number. Not the number is hexadecimal, which means to
    retrieve text page "100", pass number 0x100. Teletext also allows
    hexadecimal page numbers (sometimes used for transmitting hidden
    data), so allowed is the full range of 0x100 to 0x8FF.

:subno:
    Defaults to `VBI_ANY_SUBNO`, which means the newest sub-page of the
    given page is returned. Else this is a sub-page number in range
    0 to 0x3F7E.

:max_level:
    Is one of the `VBI_WST_LEVEL_*` constants and specifies
    the Teletext implementation level to use for formatting.

:display_rows:
    Limits rendering to the given number of rows
    (i.e. row 0 ... *display_rows* - 1)  In practice, useful
    values are 1 (format the page header row only) or 25 (complete page).

:navigation:
    This boolean parameter can be used to skip parsing the page
    for navigation links to save formatting time.

Although safe to do, this function is not supposed to be called from
an event handler since rendering may block decoding for extended
periods of time.

The returned object must be deleted to release resources which are
locked internally in the library during the fetch.

Zvbi.ServiceDec.fetch_cc_page()
-------------------------------

::

    pg = vt.fetch_cc_page(pgno, reset=False)

Fetches a Closed Caption page designated by *pgno* from the cache,
formats and returns it and as an object of type `Zvbi.Page`_.
The function raises exception *ServiceDecError* upon errors.

Closed Caption pages are transmitted basically in two modes: at once
and character by character ("roll-up" mode).  Either way you get a
snapshot of the page as it should appear on screen at the present time.

With `Zvbi.ServiceDec.event_handler_register()`_ you can request a
`VBI_EVENT_CAPTION` event to be notified about pending changes (in case of
"roll-up" mode that is with each new word received) and the "dirty"
attribute provided by `Zvbi.Page.get_page_dirty_range()`_ will mark the
lines actually in need of updates, for speeding-up rendering.

If the *reset* parameter is omitted or set to *True*, the page dirty flags
in the cached paged are reset after fetching. Pass *False* only if you
plan to call this function again to update other displays.

Although safe to do, this function is not supposed to be called from an
event handler, since rendering may block decoding for extended periods of
time.

The returned object must be deleted to release resources which are
locked internally in the library during the fetch.

Zvbi.ServiceDec.page_title()
----------------------------

::

    title = vt.page_title(pgno, [subno])

The function makes an effort to deduce a page title to be used in
bookmarks or similar purposes for the page specified by parameters
*pgno* and *subno*.  The title is mainly derived from navigation data
on the given page.

As usual, parameter *subno* defaults to `VBI_ANY_SUBNO`, which means the
newest sub-page of the given page is used.  The function raises exception
*ServiceDecError* upon errors.

.. _Zvbi.ServiceDec event handling:

Event handling
--------------

Typically the transmission of VBI data elements like a Teletext or Closed Caption
page spans several VBI lines or even video frames. So internally the data
service decoder maintains caches accumulating data. When a page or other
object is complete it calls the respective event handler to notify the
application.

Clients can register any number of handlers needed, also different handlers
for the same event. They will be called by the `Zvbi.ServiceDec.decode()`_
function in the order in which they were registered.  Since decoding is
stopped while in the callback, the handlers should return as soon as
possible.

The handler function receives two parameters: First is the event type
(i.e. one of the `VBI_EVENT_*` constants), second a named tuple
describing the event. The type and contents of the second parameter
depends on the event type. The following event types are defined:

*VBI_EVENT_NONE*:
    No event. Second callback parameter is *None*.

*VBI_EVENT_CLOSE*:
    The vbi decoding context is about to be closed. This event is
    sent when the decoder object is destroyed and can be used to
    clean up event handlers. Second callback parameter is *None*.

*VBI_EVENT_TTX_PAGE*:
    The vbi decoder received and cached another Teletext page. For this
    type the second callback function parameter has type
    *Zvbi.EventTtx* with the following elements:

    The received page is designated by *ev.pgno* and *ev.subno*.

    *ev.roll_header* flags the page header as suitable for rolling page
    numbers, e. g. excluding pages transmitted out of order.  The
    *ev.header_update* flag is set when the header, excluding the page
    number and real time clock, changed since the last
    `VBI_EVENT_TTX_PAGE` evemt. Note this may happen at midnight when the
    date string changes. The *ev.clock_update* flag is set when the real
    time clock changed since the last `VBI_EVENT_TTX_PAGE` (that is at
    most once per second). They are both set at the first
    `VBI_EVENT_TTX_PAGE` sent and unset while the received header or clock
    field is corrupted.

    If any of the roll_header, header_update or clock_update flags
    are set, *ev.raw_header* contains the raw header data (40 bytes).
    *ev.pn_offset* will be the offset (0 ... 37) of the three-digit page
    number in the raw or formatted header. Always call
    *vt.fetch_vt_page()* for proper translation of national characters and
    character attributes; the raw header is only provided here as a means
    to quickly detect changes.

*VBI_EVENT_CAPTION*:
    A Closed Caption page has changed and needs visual update.
    For this type the second callback function parameter has type
    *Zvbi.EventCaption* with a single element *ev.pgno*, which
    indicates the "CC channel" of the received page.

    When the client is monitoring this page, the expected action is
    to call *vt.fetch_cc_page()*. To speed up rendering, more detailed
    update information can be queried via
    `Zvbi.Page.get_page_dirty_range()`_.
    (Note the vbi_page will be a snapshot of the status at fetch time
    and not event time, i.e. the "dirty" flags accumulate all changes
    since the last fetch.)

*VBI_EVENT_NETWORK*:
    Some station/network identifier has been received or is no longer
    transmitted (in the latter case all values are zero, e.g. after a
    channel switch).  The event will not repeat until a different identifier
    has been received and confirmed.  (Note: VPS/TTX and XDS will not combine
    in real life, feeding the decoder with artificial data can confuse
    the logic.)

    For this type the second callback function parameter has type
    *Zvbi.EventNetwork* with the following elements:

    0. *nuid*: Network identifier
    1. *name*: Name of the network from XDS or from a table lookup of CNIs in Teletext packet 8/30 or VPS
    2. *call*: Network call letters, from XDS (i.e. closed-caption, US only), else empty
    3. *tape_delay*: Tape delay in minutes, from XDS; 0 outside of US
    4. *cni_vps*: Network ID received from VPS, or zero if unknown
    5. *cni_8301*: Network ID received from teletext packet 8/30/1, or zero if unknown
    6. *cni_8302*: Network ID received from teletext packet 8/30/2, or zero if unknown

    Minimum times for identifying a network, when data service is
    transmitted: VPS (DE/AT/CH only): 0.08 seconds; Teletext PDC or 8/30:
    2 seconds; XDS (US only): unknown, between 0.1x to 10x seconds.

*VBI_EVENT_NETWORK_ID*:
    Like *VBI_EVENT_NETWORK*, but this event will also be sent when the
    decoder cannot determine a network name.  For this type the second
    callback function parameter has type *Zvbi.EventNetwork* with same
    contents as described above.

*VBI_EVENT_TRIGGER*:
    Triggers are sent by broadcasters to start some action on the
    user interface of modern TVs. Until libzvbi implements all of
    WebTV and SuperTeletext the information available are program
    related (or unrelated) URLs, short messages and Teletext
    page links.

    This event is sent when a trigger has fired. The second callback
    function parameter is of type *Zvbi.PageLink* and has the following
    elements:

    0. *type*: Link type: One of VBI_LINK* constants
    1. *eacem*: Link received via EACEM or ATVEF transport method
    2. *name*: Some descriptive text or empty
    3. *url*: URL
    4. *script*: A piece of ECMA script (Javascript), this may be used on
       WebTV or SuperTeletext pages to trigger some action. Usually empty.
    5. *nuid*: Network ID for linking to pages on other channels
    6. *pgno*: Teletext page number
    7. *subno*: Teletext sub-page number
    8. *expires*: The time in seconds and fractions since 1970-01-01 00:00
       when the link should no longer be offered to the user, similar to a
       HTTP cache expiration date
    9. *itv_type*: One of VBI_WEBLINK_* constants; only applicable to ATVEF triggers; else UNKNOWN
    10. *priority*: Trigger priority (0=EMERGENCY, should never be
        blocked, 1..2=HIGH, 3..5=MEDIUM, 6..9=LOW) for ordering and filtering
    11. *autoload*: Open the target without user confirmation

*VBI_EVENT_ASPECT*:
    The vbi decoder received new information (potentially from PAL WSS,
    NTSC XDS or EIA-J CPR-1204) about the program aspect ratio.

    The second callback function parameter is of type *Zvbi.AspectRatio*
    and has the following elements:

    0. *first_line*: Describe start of active video (inclusive), i.e.
       without the black bars in letterbox mode
    1. *last_line*: Describes enf of active video (inclusive)
    2. *ratio*: The picture aspect ratio in anamorphic mode, 16/9 for
       example. Normal or letterboxed video has aspect ratio 1/1
    3. *film_mode*: TRUE when the source is known to be film transferred
       to video, as opposed to interlaced video from a video camera.
    4. *open_subtitles*: Describes how subtitles are inserted into the
       picture: None, or overlay in picture, or in letterbox bars, or
       unknown.

*VBI_EVENT_PROG_INFO*:
    We have new information about the current or next program.

    The second callback function parameter is of type *Zvbi.ProgInfo*
    and has the following elements:

    0. *current_or_next*: Indicates if entry refers to the current or next program
    1. *start_month*: Month of the start date
    2. *start_day*: Day-of-month of the start date
    3. *start_hour*: Hour of the start time
    4. *start_min*: Minute of the start time
    5. *tape_delayed*: Indicates if a program is routinely tape delayed for
       Western US time zones.
    6. *length_hour*: Duration in hours
    7. *length_min*: Duration remainder in minutes
    8. *elapsed_hour*: Already elapsed duration
    9. *elapsed_min*: Already elapsed duration
    10. *elapsed_sec*: Already elapsed duration
    11. *title*: Program title text (ASCII)
    12. *type_classf*: Scheme used for program type classification:
        One of the *VBI_PROG_CLASSF* constants. Use
        `Zvbi.prog_type_string()`_ for obtaining a string from this
        value and each of the following type identifiers.
    13. *type_id_0*: Program type classifier #1 according to scheme
    14. *type_id_1*: Program type classifier #2
    15. *type_id_2*: Program type classifier #3
    16. *type_id_3*: Program type classifier #4
    17. *rating_auth*: Scheme used for rating: One of VBI_RATING_AUTH*
        constants. Use `Zvbi.rating_string()`_ for obtaining a string from
        this value and the following *rating_id*.
    18. *rating_id*: Rating classification
    19. *rating_dlsv*: Additional rating for scheme in case of
        scheme *VBI_RATING_TV_US*
    20. *audio_mode_a*: Audio mode: One of VBI_AUDIO_MODE* constants
    21. *audio_language_a*: Audio language (audio channel A)
    22. *audio_mode_b*: Audio mode (channel B)
    23. *audio_language_b*: Audio language (audio channel B)
    24. *caption_services*: Active caption pages: bits 0-7 correspond to caption pages 1-8
    25. *caption_languages*: Tuple with caption language on all 8 CC pages
    26. *aspect_ratio*: Aspect ratio description, an instance of class *Zvbi.AspectRatio*
    27. *description*: Program content description text: Up to 8 lines
        of ASCII text spearated by newline character.

Zvbi.ServiceDec.event_handler_register()
----------------------------------------

::

    vt.event_handler_register(event_mask, function, [user_data])

Registers a new event handler. *event_mask* can be a but-wise 'OR' of
`VBI_EVENT_*` constants. When the handler *function* with same *user_data*
is already registered, its event_mask will be changed. Any number of
handlers can be registered, also different handlers for the same event
which will be called in registration order.

The registered handler function with two or three parameters, depending
on the presence of parameter *user_data*:

1. Event type (i.e. one of the `VBI_EVENT_*` constants).
2. A named tuple type describing the event. The class type depends on the
   type of event indicated as first parameter.
3. A copy of the *user_data* object specified during registration. The
   parameter is omitted when no *user_data* was passed during
   registration.

See section `Zvbi.ServiceDec event handling`_ above for a detailed
descripion of the callback parameters and information types.

Apart of adding handlers, this function also enables and disables decoding
of data services depending on the presence of at least one handler for the
respective data. A `VBI_EVENT_TTX_PAGE` handler for example enables
Teletext decoding.

This function can be safely called at any time, even from inside of a handler.
Note only 10 event callback functions can be registered in a script at the
same time.  Callbacks are automatically unregistered when the decoder object
is destroyed.

Zvbi.ServiceDec.event_handler_unregister()
------------------------------------------

::

    vt.event_handler_unregister(function, [user_data])

De-registers the event handler *handler* with optional parameter
*user_data*, if such a handler was previously registered with the same
user data parameter.

Apart from removing a handler, this function also disables decoding of
associated data services when no handler is registered to consume the
respective data. For example, removing the last handler for event type
`VBI_EVENT_TTX_PAGE` disables Teletext decoding.

This function can be safely called at any time, even from inside of a
handler removing itself or another handler, and regardless if the handler
has been successfully registered.


.. _Zvbi.Search:

Class Zvbi.Search
=================

The functions in this section allow searching across one or more
Teletext pages in the cache for a given sub-string or a regular
expression.

Constructor Zvbi.Search()
-------------------------

::

    search = Zvbi.Search(decoder=vt, pattern="",
                         page=0x100, subno=Zvbi.VBI_ANY_SUBNO,
                         casefold=False, regexp=False,
                         progress=None, user_data=None)

Create a search context and prepare for searching the Teletext page
cache with the given sub-string or regular expression.

Input Parameters:

:pattern:
    Contains the search pattern (libzvbi expects the string in UTF-8
    encoding; the conversion from Unicode used by Python strings is done
    automatically).

:page:
    Teletext page number. Not the number is hexadecimal, which means to
    retrieve text page "100", pass number 0x100. Teletext also allows
    hexadecimal page numbers (sometimes used for transmitting hidden
    data), so allowed is the full range of 0x100 to 0x8FF.

:subno:
    Defaults to `Zvbi.VBI_ANY_SUBNO`, which means the newest sub-page of
    the given page is returned. Else this is a sub-page number in range 0
    to 0x3F7E.

:regexp:
    This boolean must be set to True when the search pattern is a regular
    expression; default is False, which means sub-string search. (Note
    libzvbi internally converts the sub-string to regular expression
    simply be escaping all special characters - so there is no performance
    gain by using sub-string search.)

:casefold:
    This boolean can be set to True to make the search case insensitive;
    default is False.

:progress:
    If present, the parameter has to be callable. The function will be
    called for each scanned page. When the function returns False, the
    search is aborted.

    The callback function receives as first parameter a reference to the
    search page (i.e. an instance of `Zvbi.Page`_), plus optionally the
    object specified as *user_data*. Note due to internal limitations only
    10 search callback functions can be registered in a script at the same
    time.  Callbacks are automatically unregistered when the search object
    is destroyed.

:user_data:
    If present, the parameter is passed through as second parameter to each
    call of the function specified by *progress*. When not specified, the
    callback is invoked with a single parameter.

**Note:** The page object is only valid while inside of the
callback function (i.e. you must not assign the object to a
variable outside of the scope of the handler function.)

**Note:**
In a multi-threaded application the data service decoder may receive
and cache new pages during a search session. When these page numbers
have been visited already the pages are not searched. At a channel
switch (and in future at any time) pages can be removed from cache.
All this has yet to be addressed.

Regular expression searching supports the standard set of operators and
constants, with these extensions:

`\\x....` or `\\X....`
    Hexadecimal number of up to 4 digits

`\\u....` or `\\U....`
    Hexadecimal number of up to 4 digits

`:title:`
    Unicode specific character class

`:gfx:`
    Teletext G1 or G3 graphic

`:drcs:`
    Teletext DRCS

`\\pN1,N2,...,Nn`
    Character properties class

`\\PN1,N2,...,Nn`
    Negated character properties class

Property definitions:

1.  alphanumeric
2.  alpha
3.  control
4.  digit
5.  graphical
6.  lowercase
7.  printable
8.  punctuation
9.  space
10. uppercase
11. hex digit
12. title
13. defined
14. wide
15. nonspacing
16. Teletext G1 or G3 graphics
17. Teletext DRCS

Character classes can contain literals, constants, and character
property classes. Example: `[abc\U10A\p1,3,4]`. Note double height
and size characters will match twice, on the upper and lower row,
and double width and size characters count as one (reducing the
line width) so one can find combinations of normal and enlarged
characters.

Zvbi.Search.next()
------------------

::

    status = search.next([dir=1])

The function starts or continues the search on a previously created search
context.  Parameter *dir* specifies the direction: 1 for forward, or -1
for backward search.

When a matching page is found, the function returns a reference to it in
form of an instance of `Zvbi.Page`_. The matching range of text is
highlighted in the page.

If no matching page is found, the function raises exception
*StopIteration*. Upon other errors the function raises exception
*Zvbi.SearchError* which contains a string describing the cause, which can
be one of the following:

VBI_SEARCH_ERROR:
    Pattern not found. Another call of `Zvbi.Search.next()`_
    will restart from the starting point given in the constructor.

VBI_SEARCH_CACHE_EMPTY:
    No pages in the cache.

VBI_SEARCH_CANCELED:
    The search has been canceled by the progress function.
    Another call of *search.next()* continues from the last searched page.


.. _Zvbi.Page:

Class Zvbi.Page
===============

These are functions to render Teletext and Closed Caption pages directly
into memory, essentially a more direct interface to the functions of some
important export modules described in `Zvbi.Export`_.

All of the functions in this section work on page objects as returned
by the page cache's "fetch" functions (see `Zvbi.ServiceDec`_)
or the page search function (see `Zvbi.Search`_)

Zvbi.Page.draw_vt_page()
------------------------

::

    canvas = pg.draw_vt_page(column, row, width, height,
                             fmt=Zvbi.VBI_PIXFMT_RGBA32_LE,
                             reveal=False, flash_on=False,
                             img_pix_width, col_pix_off, row_pix_off)

Draws a complete Teletext page or a sub-section thereof into a raw image
canvas and returns it in form of a bytes object. Each teletext character
occupies 12 x 10 pixels (i.e. a character is 12 pixels wide and each line
is 10 pixels high. Note that this aspect ratio is not optimal for display,
so pixel lines should be doubled. This is done automatically by the PPM
and XPM conversion functions.)

The image is returned in form of a bytes object.  When
using format `Zvbi.VBI_PIXFMT_RGBA32_LE`, each pixel consists of 4 subsequent
bytes in the string (RGBA). Hence the string is
`4 * 12 * pg_columns * 10 * pg_rows` bytes long, where
`pg_columns` and `pg_rows` are the page width and height in
teletext characters respectively.  When using format `Zvbi.VBI_PIXFMT_PAL8`
each pixel uses one byte. In this case each pixel value is an index into
the color palette as delivered by `Zvbi.Page.get_page_color_map()`_.

Input parameters:

:column:
    Start column in the page to render at the first pixel column, defaults
    to 0.  Note this and the following three values are given as numbers
    of teletext characters (not pixels.)

:row:
    Start row in the page to render at the first pixel column, defaults to 0.

:width:
    Number of columns to render. The sum of parameters *column* plus
    *width* shall be less or equal the page width. When omitted, the
    value defaults to the page width minus the start row offset.

:height:
    Number of rows to render. The sum of parameters *row* plus
    *height* shall be less or equal the page height. When omitted, the
    value defaults to the page height minus the start column offset.

:fmt:
    Specifies the output format. Supported is `Zvbi.VBI_PIXFMT_RGBA32_LE`
    (i.e. each pixel uses 4 subsequent bytes for R,G,B,A) and
    `Zvbi.VBI_PIXFMT_PAL8` (i.e. each pixel uses one byte, which is an
    index into the color palette)

:img_pix_width:
    Is the distance between canvas pixel lines in pixels.  When omitted or
    set to 0, the image width is automatically set to the width of the
    selected region (i.e. the number of columns times 12) plus
    *col_pix_off*, if present. If specified, the value has to be equal
    or larger than the default; extraneous pixels are left zero in the
    returned image.

:col_pix_off:
    Offset to the left in pixels defining where in the canvas to draw
    the page section. By using this value combined with *img_pix_width*
    you can achieve a black border around the image.

:row_pix_off:
    Offset to the top in pixels defining where in the canvas to draw
    the page section.

:reveal:
    When omitted or set to False, characters flagged as "concealed" are
    rendered space (U+0020). When set to True the characters are rendered.

:flash_on:
    Set to True to draw characters flagged "blink" (properties) as space
    (U+0020). To implement blinking you'll have to draw the page
    repeatedly with this parameter alternating between 0 and 1.

Zvbi.Page.draw_cc_page()
------------------------

::

    canvas = pg.draw_cc_page(column, row, width, height,
                             fmt=Zvbi.VBI_PIXFMT_RGBA32_LE,
                             img_pix_width, col_pix_off, row_pix_off)

Draw a complete or sub-section of a Closed Caption page. Each character
occupies 16 x 26 pixels (i.e. a character is 16 pixels wide and each line
is 26 pixels high.)

The image is returned in a byte object.  Each
pixel uses 4 subsequent bytes in the string (RGBA). Hence the string
is `4 * 16 * pg_columns * 26 * pg_rows` bytes long, where
`pg_columns` and `pg_rows` are the page width and height in
Closed Caption characters respectively.

For details on parameters please see the previous function.

Zvbi.Page.canvas_to_ppm()
-------------------------

::

    ppm = pg.canvas_to_ppm(canvas, fmt=Zvbi.VBI_PIXFMT_RGBA32_LE,
                           aspect=True, img_pix_width=0)

This is a helper function which converts the image given in *canvas* from
a raw bytes object generated by *draw_vt_page()* or *draw_cc_page()* into
PPM format (specifically "P6" with 256 colors per dimensions, which means
there is a small ASCII header, followed by the image bitmap consisting of
3 bytes (RGB) per pixel.)

:fmt:
    The is the format of the input canvas. If must be the same value as
    passed to *draw_vt_page()* or *draw_cc_page()*.

:aspect:
    This optional boolean parameter when set to False, disables the aspect
    ratio correction (i.e. on teletext pages all lines are doubled by
    default; closed caption output ration is already correct.) Default is
    True.

:img_pix_width:
    The is the pixel width of the input canvas. It must be the same
    value as passed to *draw_vt_page()* or *draw_cc_page()*. When omitted
    or zero, the value is calculated in the same way as described for these
    methods.

Zvbi.Page.canvas_to_xpm()
-------------------------

::

    xpm = pg.canvas_to_xpm(canvas, fmt=Zvbi.VBI_PIXFMT_RGBA32_LE,
                           aspect=True, img_pix_width=0)

This is a helper function which converts the image given in *canvas* from
a raw bytes object generated by *draw_vt_page()* or *draw_cc_page()* into
XPM format. Due to the way XPM is specified, the output is an ASCII text
string (suitable for including in C source code), however returned within
a bytes object.

:fmt:
    The is the format of the input canvas. If must be the same value as
    passed to *draw_vt_page()* or *draw_cc_page()*.

:aspect:
    This optional boolean parameter when set to False, disables the aspect
    ratio correction (i.e. on teletext pages all lines are doubled by
    default; closed caption output ration is already correct.) Default is
    True.

:img_pix_width:
    The is the pixel width of the input canvas. It must be the same
    value as passed to *draw_vt_page()* or *draw_cc_page()*. When omitted
    or zero, the value is calculated in the same way as described for these
    methods.

Zvbi.Page.print_page()
----------------------

::

    txt = pg.print_page(column, row, width, height,
                        fmt='UTF-8', table=True)

Print and return the referenced Teletext or Closed Caption page
in form of a bytes object. Rows are separated by line-feed characters ("\n").
All character attributes and colors will be lost. Graphics characters,
DRCS and all characters not representable in UTF-8 will be replaced by
spaces.

:column:
    Start column in the page to render at the first output column.
    Defaults to 0.

:row:
    Start row in the page to render at the first output row.
    Defaults to 0.

:width:
    Number of columns to render. The sum of parameters *column* plus
    *width* shall be less or equal the page width (use
    *pg.get_page_size()* to determine the dimensions.) When omitted, the
    value defaults to the page width minus the start row offset.

:height:
    Number of rows to render. The sum of parameters *row* plus
    *height* shall be less or equal the page height. When omitted, the
    value defaults to the page height minus the start column offset.

:format:
    Encoding to be used in the output. Default is 'UTF-8'. Use the
    equivalent format specification when decoding the bytes into a Python
    string.

:table:
    When optional parameter *table* is set to 1, the page is scanned in
    table mode, printing all characters within the source rectangle
    including runs of spaces at the start and end of rows. This is the
    default. When set to False, sequences of spaces at the start and end
    of rows are collapsed into single spaces and blank lines are
    suppressed.


Zvbi.Page.get_page_no()
-----------------------

::

    (pgno, subno) = pg.get_page_no()

This function returns a tuple containing the page and sub-page number of
the page instance.

Teletext page numbers are hexadecimal numbers in the range 0x100 .. 0x8FF,
Closed Caption page numbers are in the range 1 .. 8.  Sub-page numbers
are used for teletext only. These are hexadecimal numbers in range
0x0001 .. 0x3F7F, i.e. the 2nd and 4th digit count from 0..F, the
1st and 3rd only from 0..3 and 0..7 respectively. A sub-page number
zero means the page has no sub-pages.

Zvbi.Page.get_page_size()
-------------------------

::

    (rows, columns) = pg.get_page_size()

This function returns a tuple containing the dimensions (i.e. row and
column count) of the page instance.

Zvbi.Page.get_page_dirty_range()
--------------------------------

::

    (y0, y1, roll) = pg.get_page_dirty_range()

To speed up rendering these variables mark the rows
which actually changed since the page has been last fetched
from cache. *y0* ... *y1* are the first to last row changed,
inclusive. *roll* indicates the
page has been vertically scrolled this number of rows,
negative numbers up (towards lower row numbers), positive
numbers down. For example -1 means row `y0 + 1 ... y1`
moved to `y0 ... y1 - 1`, erasing row *y1* to all spaces.

Practically this is only used in Closed Caption roll-up
mode, otherwise all rows are always marked dirty. Clients
are free to ignore this information.

Zvbi.Page.get_page_color_map()
------------------------------

::

    map = pg.get_page_color_map()

The function returns a tuple of length 40 which
contains the page's color palette. Each entry is a 24-bit RGB value
(i.e. three 8-bit values for red, green, blue, with red in the
lowest bits)  To convert this into the usual "`#RRGGBB`" syntax use:

::

    print("#%02X%02X%02X" %
             (rgb&0xFF, (rgb>>8)&0xFF, (rgb>>16)&0xFF))

Zvbi.Page.get_page_text_properties()
------------------------------------

::

    av = pg.get_page_text_properties()

The function returns tuple which contains the properties of all characters
on the given page, starting with those of the first row left to right,
directly followed by the next row etc. (use *pg.get_page_size()* for
unpacking). Each entry is a bit-field. The members are (in
ascending order, width in bits given behind the colon):

* foreground color:8
* background color:8
* opacity:4
* size:4
* underline:1
* bold:1
* italic:1
* flash:1
* conceal:1
* proportional:1
* link:1

The color values are indices into the page color map.

Zvbi.Page.get_page_text()
-------------------------

::

    txt = pg.get_page_text( [all_chars] )

The function returns the complete page text in form of a string (i.e.
Unicode).  This function is very similar to *pg.print_page()*,
but does not insert or remove any characters so that it's guaranteed
that characters in the returned string correlate exactly with the
array returned by *pg.get_page_text_properties()*.

When the optional parameter *all_chars* is set to 1, even
characters on the private Unicode code pages are included.
Otherwise these are replaced with blanks. Note use of these
characters will cause errors when passing the string to
transcoder functions (such as Pythons's *decode()*.)

Zvbi.Page.vbi_resolve_link()
----------------------------

::

    href = pg.vbi_resolve_link(column, row)

The page instance *pg* (in practice only Teletext pages) may contain
hyperlinks such as HTTP URLs, e-mail addresses or links to other
pages. Characters being part of a hyperlink have their "link" flag
set in the character properties (see *pg.get_page_text_properties()*),
this function returns a dict with a more verbose
description of the link.

The returned hash contains the following elements (depending on the
type of the link not all elements may be present):

* "type"
* "eacem"
* "name"
* "url"
* "script"
* "nuid"
* "pgno"
* "subno"
* "expires"
* "itv_type"
* "priority"
* "autoload"

Zvbi.Page.vbi_resolve_home()
----------------------------

::

    href = pg.vbi_resolve_home()

All Teletext pages have a built-in home link, by default
page 100, but can also be the magazine intro page or another
page selected by the editor.  This function returns a dict
with the same elements as *pg.vbi_resolve_link()*.


.. _Zvbi.Export:

Class Zvbi.Export
=================

Once libzvbi received, decoded and formatted a Teletext or Closed Caption
page you will want to render it on screen, print it as text or store it
in various formats.  libzvbi provides export modules converting a page
object into the desired format or rendering directly into an image.

Currently the following export formats are supported:

* Text
* HTML
* PNG (image with lossless compression)
* PPM (image without compression)
* XPM (image without compression)

All the formats support boolean option "reveal"; all the image formats
support boolean option "aspect". The meaning of the options is the same as
for `Zvbi.Page.draw_vt_page()`_.

Constructor Zvbi.Export()
-------------------------

::

    exp = Zvbi.Export(keyword)

Creates a new object for exporting a `Zvbi.Page`_ object in
the format implied by parameter *keyword*. As a special service you can
initialize options by appending to the *keyword* parameter like this:
`keyword = "keyword; quality=75.5, comment=\"example text\"";`

Note: A quick overview of all export formats and options can be
obtained by running the demo script *examples/explist.pl* in the
ZVBI package.

Zvbi.Export.info_enum()
-----------------------

::

    href = Zvbi.Export.info_enum(index)

This is a **static** member function.
The function enumerates all available export modules. You should start
with *index* 0, incrementing until the function raises exception
*StopIteration*.
Some modules may depend on machine features or the presence of certain
libraries, thus the list can vary from session to session.

The function returns a dict with the following elements:

* "keyword"
* "label"
* "tooltip"
* "mime_type"
* "extension"

Zvbi.Export.info_keyword(keyword)
---------------------------------

::

    href = Zvbi.Export.info_keyword(keyword)

This is a **static** member function.
Similar to the above function *info_enum()*, this function returns info
about available modules, although this one searches for an export module
which matches the given *keyword*. If no match is found the function
raises exception *Zvbi.ExportError*, else a dict as described above.

Zvbi.Export.info_export()
-------------------------

::

    href = exp.info_export()

Returns the export module info for the export instance in form of a dict.
The contents are as described for the previous two functions.

Zvbi.Export.option_info_enum()
------------------------------

::

    href = exp.option_info_enum(index)

This member function enumerates the options available for the given
export instance.
You should start at *index* 0, incrementing until the function
raises exception *StopIteration*.  On success, the function returns a
dict with the following elements:

* "type"
* "keyword"
* "label"
* "min"
* "max"
* "step"
* "def"
* "menu"
* "tooltip"

The content format of min, max, step and def depends on the type,
i.e. it may be an integer, double or string.

If present, the value of "menu" is a tuple.  Elements in the tuple are of
the same type as min, max, etc.  If no label or tooltip are available for
the option, these elements are undefined.

Zvbi.Export.option_info_keyword()
---------------------------------

::

    href = exp.option_info_keyword(keyword)

Similar to the above function *exp.option_info_enum()* this
function returns info about available options, although this one
identifies options based on the given *keyword*.

Zvbi.Export.option_set()
------------------------

::

    exp.option_set(keyword, opt)

Sets the value of the option named by *keword* to *opt*.
Raises exception *Zvbi.ExportError* on failure.  Example: ::

    exp.option_set('quality', 75.5);

Note the expected type of the option value depends on the keyword.
The ZVBI interface module automatically converts the option into
type expected by the libzvbi library.

Mind that options of type `VBI_OPTION_MENU` must be set by menu
entry number (integer), all other options by value. If necessary
it will be replaced by the closest value possible. Use function
*exp.option_menu_set()* to set options with menu by menu entry.

Zvbi.Export.option_get()
------------------------

::

    opt = exp.option_get(keyword)

This function queries and returns the current value of the option
named by *keyword*.

Zvbi.Export.option_menu_set()
-----------------------------

::

    exp.option_menu_set(keyword, entry)

Similar to *exp.option_set()* this function sets the value of
the option named by *keyword* to *entry*, however it does so
by number of the corresponding menu entry. Naturally this must
be an option with menu.

Zvbi.Export.option_menu_get()
-----------------------------

::

    entry = exp.option_menu_get(keyword)

Similar to *exp.option_get()* this function queries the current
value of the option named by *keyword*, but returns this value as
number of the corresponding menu entry. Naturally this must be an
option with menu.

Zvbi.Export.to_stdio()
----------------------

::

    exp.to_stdio(pg, fd)

This function writes contents of the `Zvbi.Page`_ instance given in *pg*,
converted to the respective export module format, to a stream created from
*fd* using fdopen(3). This means *fd* has to be a value as returned by
*fileno()* on a file-like object.

The function raises exception *Zvbi.ExportError* upon errors.
Note this function may write incomplete files when an error occurs.

You can call this function as many times as you want, it does not
change state of the export or page objects.

Zvbi.Export.to_file()
---------------------

::

    exp.to_file(pg, file_name)

This function writes contents of the `Zvbi.Page`_ instance given in *pg*,
converted to the respective export module format, into a new file specified
by *file_name*. When an error occurs the file will be deleted.
The function raises exception *Zvbi.ExportError* upon errors.

You can call this function as many times as you want, it does not
change state of the export or page objects.

Zvbi.Export.to_memory()
-----------------------

::

    data = exp.to_memory(pg)

This function writes contents of the `Zvbi.Page`_ instance given in *pg*,
converted to the respective export module format, into a bytes object.

The function raises exception *Zvbi.ExportError* upon errors.


.. _Zvbi.DvbMux:

Class Zvbi.DvbMux
=================

These functions convert raw and/or sliced VBI data to a DVB Packetized
Elementary Stream or Transport Stream as defined in EN 300 472 "Digital
Video Broadcasting (DVB); Specification for conveying ITU-R System B
Teletext in DVB bit-streams" and EN 301 775 "Digital Video Broadcasting
(DVB); Specification for the carriage of Vertical Blanking Information
(VBI) data in DVB bit-streams".

Note EN 300 468 "Digital Video Broadcasting (DVB); Specification for
Service Information (SI) in DVB systems" defines another method to
transmit VPS data in DVB streams. Libzvbi does not provide functions
to generate SI tables but the *encode_dvb_pdc_descriptor()* function
is available to convert a VPS PIL to a PDC descriptor (since version 0.3.0)

Constructor Zvbi.DvbMux()
-------------------------

There are two separate semantics:

::

    mx = Zvbi.DvbMux(pes=1 [callback, user_data] )

Creates a new DVB VBI multiplexer converting raw and/or sliced VBI data
to MPEG-2 Packetized Elementary Stream (PES) packets as defined in the
standards EN 300 472 and EN 301 775.  Returns `undef` upon error.

:callback:
    Specifies a handler which is called by *mx.feed()* when a new packet is
    available. Must be omitted if *mx.cor()* is used.  For further callback
    parameters see the description of the *feed* function.

:user_data:
    Passed through to the *callback*.

::

    mx = Zvbi.DvbMux(pes=0, pid=pid [callback, user_data] )

Allocates a new DVB VBI multiplexer converting raw and/or sliced VBI data
to MPEG-2 Transport Stream (TS) packets as defined in the standards
EN 300 472 and EN 301 775. Returns `undef` upon error.

Parameter *pid* is a program ID that will be stored in the header of the
generated TS packets. The value must be in range 0x0010 to 0x1FFE inclusive.

Parameter *callback* specifies a handler which is called by
*mx.feed()* when a new packet is available. Must be omitted if
*mx.cor()* is used.  The *user_data* is passed through to
the handler.  For further callback parameters see the description
of the *feed* function.

Zvbi.DvbMux.mux_reset()
-----------------------

::

    mx.mux_reset()

This function clears the internal buffers of the DVB VBI multiplexer.

After a reset call the *mx.cor()* function will encode a new
PES packet, discarding any data of the previous packet which has not
been consumed by the application.

Zvbi.DvbMux.cor()
-----------------

::

    mx.cor(buf, buffer_left, sliced, sliced_left, service_mask, pts [, raw, sp])

This function converts raw and/or sliced VBI data to one DVB VBI PES
packet or one or more TS packets as defined in EN 300 472 and
EN 301 775, and stores them in the output buffer.

If the returned *buffer_left* value is zero and the returned
*sliced_left* value is greater than zero another call will be
necessary to convert the remaining data.

After a *reset()* call the *cor()* function will encode a new
PES packet, discarding any data of the previous packet which has
not been consumed by the application.

Parameters:
*buffer* will be used as output buffer for converted data. This scalar
may be undefined; else it should have the length given in *buffer_left*.
*buffer_left* the number of bytes available in *buffer*,
and will be decremented by number of bytes stored there.
*sliced* contains the sliced VBI data to be converted. All data
must belong to the same video frame.  *sliced* is either a blessed
reference to a sliced buffer, or a scalar with a byte string consisting
of sliced data (i.e. the same formats are accepted as by *vt.decode()*.
*sliced_left* must contain the number of sliced VBI lines in the
input buffer *sliced*. It will be decremented by the number of
successfully converted structures.  On failure it will point at
the offending line index (relative to the end of the sliced array.)
*service_mask* Only data services in this set will be
encoded. Other data services in the sliced input buffer will be
discarded without further checks. Create a set by ORing
`VBI_SLICED_*` constants.
*pts* contains the presentation time stamp which will be encoded
into the PES packet. Bits 33 ... 63 are discarded.

*raw* shall contain a raw VBI frame of (*sp.count_a*
+ *sp.count_b*) lines times *sp.bytes_per_line*.
The function encodes only those lines which have been selected by sliced
lines in the *sliced* array with id `VBI_SLICED_VBI_625`
The data field of these structures is ignored. When the sliced input
buffer does not contain such structures *raw* can be omitted.
*sp* Describes the data in the raw buffer unless raw is omitted.
Else it must be valid, with the constraints described for *feed()*
below.

The function returns 0 on failures, which may occur under the
following circumstances:

* The maximum PES packet size, or the value selected with
  *mx.set_pes_packet_size()*, is too small to contain all
  the sliced and raw VBI data.

* The sliced array is not sorted by ascending line number,
  except for elements with line number 0 (undefined).

* Only the following data services can be encoded:
  (1) `VBI_SLICED_TELETEXT_B` on lines 7 to 22 and 320 to 335
  inclusive, or with line number 0 (undefined). All Teletext
  lines will be encoded with data_unit_id 0x02 ("EBU Teletext
  non-subtitle data").
  (2) `VBI_SLICED_VPS` on line 16.
  (3) `VBI_SLICED_CAPTION_625` on line 22.
  (4) `VBI_SLICED_WSS_625` on line 23.
  (5) Raw VBI data with id `VBI_SLICED_VBI_625` can be encoded
  on lines 7 to 23 and 320 to 336 inclusive. Note for compliance
  with the Teletext buffer model defined in EN 300 472,
  EN 301 775 recommends to encode at most one raw and one
  sliced, or two raw VBI lines per frame.

* A vbi_sliced structure contains a line number outside the
  valid range specified above.

* parameter *raw* is undefined although the sliced array contains
  a structure with id `VBI_SLICED_VBI_625`.

* One or more members of the *sp* structure are invalid.

* A vbi_sliced structure with id `VBI_SLICED_VBI_625`
  contains a line number outside the ranges defined by *sp*.

On all errors *sliced_left* will refer to the offending sliced
line in the index buffer (i.e. relative to the end of the buffer)
and the output buffer remains unchanged.

Zvbi.DvbMux.feed()
------------------

::

    mx.feed(sliced, sliced_lines, service_mask, pts [, raw, sp])

This function converts raw and/or sliced VBI data to one DVB VBI PES
packet or one or more TS packets as defined in EN 300 472 and
EN 301 775. To deliver output, the callback function passed to
*pes_new()* or *ts_new()* is called once for each PES or TS packet.

Parameters:
*sliced* contains the sliced VBI data to be converted. All data
must belong to the same video frame.  *sliced* is either a blessed
reference to a sliced buffer, or a scalar with a byte string consisting
of sliced data (i.e. the same formats are accepted as by *vt.decode()*.
*sliced_lines* number of valid lines in the *sliced* input buffer.
*service_mask* Only data services in this set will be
encoded. Other data services in the sliced buffer will be
discarded without further checks. Create a set by ORing
`VBI_SLICED_*` constants.
*pts* This Presentation Time Stamp will be encoded into the
PES packet. Bits 33 ... 63 are discarded.

*raw* shall contain a raw VBI frame of (*sp.count_a*
+ *sp.count_b*) lines times *sp.bytes_per_line*.
The function encodes only those lines which have been selected by sliced
lines in the *sliced* array with id `VBI_SLICED_VBI_625`
The data field of these structures is ignored. When the sliced input
buffer does not contain such structures *raw* can be omitted.

*sp* describes the data in the raw buffer unless raw is omitted.
Else it must be valid, with the following additional constraints:
* videostd_set must contain one or more bits from the
`VBI_VIDEOSTD_SET_625_50`.
* scanning must be 625 (libzvbi 0.2.x only)
* sampling_format must be `VBI_PIXFMT_Y8` or
`VBI_PIXFMT_YUV420`. Chrominance samples are ignored.
* sampling_rate must be 13500000.
* offset must be >= 132.
* samples_per_line (in libzvbi 0.2.x bytes_per_line) must be >= 1.
* offset + samples_per_line must be <= 132 + 720.
* synchronous must be set.

The function returns 0 on failures. For a description of failure
conditions see *cor()* above.

Zvbi.DvbMux.get_data_identifier()
---------------------------------

::

    mx.get_data_identifier()

Returns the data_identifier the multiplexer encodes into PES packets.

Zvbi.DvbMux.set_data_identifier()
---------------------------------

::

    ok = mx.set_data_identifier(data_identifier)

This function can be used to determine the *data_identifier* byte
to be stored in PES packets.
For compatibility with decoders compliant to EN 300 472 this should
be a value in the range 0x10 to 0x1F inclusive. The values 0x99
to 0x9B inclusive as defined in EN 301 775 are also permitted.
The default data_identifier is 0x10.

Returns 0 if *data_identifier* is outside the valid range.

Zvbi.DvbMux.get_min_pes_packet_size()
-------------------------------------

::

    size = mx.get_min_pes_packet_size()

Returns the maximum size of PES packets the multiplexer generates.

Zvbi.DvbMux.get_max_pes_packet_size()
-------------------------------------

::

    size = mx.get_max_pes_packet_size()

Returns the minimum size of PES packets the multiplexer generates.

Zvbi.DvbMux.set_pes_packet_size()
---------------------------------

::

    ok = mx.set_pes_packet_size(min_size, max_size)

Determines the minimum and maximum total size of PES packets
generated by the multiplexer, including all header bytes. When
the data to be stored in a packet is smaller than the minimum size,
the multiplexer will fill the packet up with stuffing bytes. When
the data is larger than the maximum size the *feed()* and
*cor()* functions will fail.

The PES packet size must be a multiple of 184 bytes, in the range 184
to 65504 bytes inclusive, and this function will round *min_size* up
and *max_size* down accordingly. If after rounding the maximum size is
lower than the minimum, it will be set to the same value as the
minimum size.

The default minimum size is 184, the default maximum 65504 bytes. For
compatibility with decoders compliant to the Teletext buffer model
defined in EN 300 472 the maximum should not exceed 1472 bytes.

Returns 0 on failure (out of memory)

The next functions provide similar functionality as described above, but
are special as they work without a *dvb_mux* object.
Meaning and use of parameters is the same as described above.

Zvbi.DvbMux.dvb_multiplex_sliced()
----------------------------------

::

    Zvbi.DvbMux.dvb_multiplex_sliced(buf, buffer_left, sliced, sliced_left, service_mask, data_identifier, stuffing)

Converts the sliced VBI data in the *sliced* buffer to VBI data
units as defined in EN 300 472 and EN 301 775 and stores them
in *buf* as output buffer.

Zvbi.DvbMux.dvb_multiplex_raw()
-------------------------------

::

    Zvbi.DvbMux.dvb_multiplex_raw(buf, buffer_left, raw, raw_left, data_identifier, videostd_set, line, first_pixel_position, n_pixels_total, stuffing)

Converts one line of raw VBI samples in *raw* to one or more "monochrome
4:2:2 samples" data units as defined in EN 301 775, and stores
them in the *buf* output buffer.

Parameters:
*line* The ITU-R line number to be encoded in the data units.
It must not change until all samples have been encoded.
*first_pixel_position* The horizontal offset where decoders
shall insert the first sample in the VBI, counting samples from
the start of the digital active line as defined in ITU-R BT.601.
Usually this value is zero and *n_pixels_total* is 720.
*first_pixel_position* + *n_pixels_total* must not be greater
than 720. This parameter must not change until all samples have
been encoded.
*n_pixels_total* Total size of the raw input buffer in bytes,
and the total number of samples to be encoded. Initially this
value must be equal to *raw_left*, and it must not change until
all samples have been encoded.
Remaining parameters are the same as described above.

**Note:**
According to EN 301 775 all lines stored in one PES packet must
belong to the same video frame (but the data of one frame may be
transmitted in several successive PES packets). They must be encoded
in the same order as they would be transmitted in the VBI, no line more
than once. Samples may have to be split into multiple segments and they
must be contiguously encoded into adjacent data units. The function
cannot enforce this if multiple calls are necessary to encode all
samples.


.. _Zvbi.DvbDemux:

Class Zvbi.DvbDemux
===================

Separating VBI data from a DVB PES stream (EN 301 472, EN 301 775).

Constructor Zvbi.DvbDemux
-------------------------

::

    dvb = Zvbi.DvbDemux( [callback [, user_data]] )

Creates a new DVB VBI demultiplexer context taking a PES stream as input.
Returns a reference to the newly allocated DVB demux context.

The optional callback parameters should only be present if decoding will
occur via the *dvb>feed()* method.  The function referenced by
*callback* will be called inside of *dvb.feed()* whenever
new sliced data is available. Optional parameter *user_data* is
appended to the callback parameters. See *dvb>feed()* for
additional details.

Zvbi.DvbDemux.reset()
---------------------

::

    dvb.reset()

Resets the DVB demux to the initial state as after creation.
Intended to be used after channel changes.

Zvbi.DvbDemux.cor()
-------------------

::

    n_lines = dvb.cor(sliced, sliced_lines, pts, buf, buf_left)

This function takes an arbitrary number of DVB PES data bytes in *buf*,
filters out *PRIVATE_STREAM_1* packets, filters out valid VBI data units,
converts them to sliced buffer format and stores the data at *sliced*.
Usually the function will be called in a loop:

::

  left = len(buffer)
  while left > 0:
    n_lines = dvb.cor(sliced, 64, pts, buffer, left)
    if n_lines > 0:
      vt.decode(sliced, n_lines, pts_conv(pts))

Input parameters: *buf* contains data read from a DVB device (needs
not align with packet boundaries.)  Note you must not modify the buffer
until all data is processed as indicated by *buf_left* being zero
(unless you remove processed data and reset the left count to zero.)
*buffer_left* specifies the number of unprocessed bytes (at the end
of the buffer.)  This value is decremented in each call by the number
of processed bytes. Note the packet filter works faster with larger
buffers. *sliced_lines* specifies the maximum number of sliced lines
expected as result.

Returns the number of sliced lines stored in *sliced*. May be zero
if more data is needed or the data contains errors. Demultiplexed sliced
data is stored in *sliced*.  You must not change the contents until
a frame is complete (i.e. the function returns a non-value.)
*pts* returns the Presentation Time Stamp associated with the
first line of the demultiplexed frame.

Note: Demultiplexing of raw VBI data is not supported yet,
raw data will be discarded.

Zvbi.DvbDemux.feed()
--------------------

::

    ok = dvb.feed(buf)

This function takes an arbitrary number of DVB PES data bytes in *buf*,
filters out *PRIVATE_STREAM_1* packets, filters out valid VBI data units,
converts them to vbi_sliced format and calls the callback function given
during creation of the context. Returns 0 if the data contained errors.

The function is similar to *dvb.cor()*, but uses an internal
buffer for sliced data.  Since this function does not return sliced
data, it's only useful if you have installed a handler. Do not mix
calls to this function with *dvb.cor()*.

The callback function is called with the following parameters:

  ok = &callback(sliced_buf, n_lines, pts, user_data);

*sliced* is a reference to a buffer holding sliced data; the reference
has the same type as returned by capture functions. *n_lines* specifies
the number of valid lines in the buffer. *pts* is the timestamp.
The last parameter is *user_data*, if given during creation.
The handler should return 1 on success, 0 on failure.

Note: Demultiplexing of raw VBI data is not supported yet,
raw data will be discarded.

Zvbi.DvbDemux.set_log_fn()
--------------------------

::

    dvb.set_log_fn(mask [, log_fn [, user_data]])

The DVB demultiplexer supports the logging of errors in the PES stream and
information useful to debug the demultiplexer.
With this function you can redirect log messages generated by this module
from general log function `Zvbi.set_log_fn()`_ to a
different function or enable logging only in the DVB demultiplexer.
The callback can be removed by omitting the handler name.

Input parameters: *mask* specifies which kind of information to log;
may be zero. *log_fn* is a reference to the handler function.
Optional *user_data* is passed through to the handler.

The handler is called with the following parameters: *level*,
*context*, *message* and, if given, *user_data*.

Note: Kind and contents of log messages may change in the future.


.. _Zvbi.IdlDemux:

Class Zvbi.IdlDemux
===================

The functions in this section decode data transmissions in
Teletext **Independent Data Line** packets (EN 300 708 section 6),
i.e. data transmissions based on packet 8/30.

Constructor Zvbi.IdlDemux()
---------------------------

::

    idl = Zvbi.IdlDemux(channel, address [, callback, user_data] )

Creates and returns a new Independent Data Line format A
(EN 300 708 section 6.5) demultiplexer.

*channel* filter out packets of this channel.
*address* filter out packets with this service data address.
Optional: *callback* is a handler to be called by *idl.feed()*
when new data is available.  If present, *user_data* is passed through
to the handler function.

Zvbi.IdlDemux.reset()
---------------------

::

    idl.reset(dx)

Resets the IDL demux context, useful for example after a channel change.

Zvbi.IdlDemux.feed()
--------------------

::

    ok = idl.feed(buf)

This function takes a stream of Teletext packets, filters out packets
of the desired data channel and address and calls the handler
given context creation when new user data is available.

Parameter *buf* is a scalar containing a teletext packet's data
(at last 42 bytes, i. e. without clock run-in and framing code),
as returned by the slicer functions.  The function returns 0 if
the packet contained incorrectable errors.

Parameters to the handler are: *buffer*, *flags*, *user_data*.

Zvbi.IdlDemux.feed_frame()
--------------------------

::

    ok = idl.feed_frame(sliced_buf, n_lines)

This function works like *idl.feed()* but takes a sliced
buffer (i.e. a full frame's worth of sliced data) and automatically
filters out all teletext lines.  This can be used to "short-circuit"
the capture output with the demultiplexer.


.. _Zvbi.PfcDemux:

Class Zvbi.PfcDemux
===================

Separating data transmitted in Page Function Clear Teletext packets
(ETS 300 708 section 4), i.e. using regular packets on a dedicated
teletext page.

Constructor Zvbi.PfcDemux()
---------------------------

::

    pfc = Zvbi.PfcDemux(pgno, stream [, callback, user_data] )

Creates and returns a new demultiplexer context.

Parameters: *page* specifies the teletext page on which the data is
transmitted.  *stream* is the stream number to be demultiplexed.

Optional parameter *callback* is a reference to a handler to be
called by *pfc.feed()* when a new data block is available.
Is present, *user_data* is passed through to the handler.

Zvbi.PfcDemux.reset()
---------------------

::

    pfc.reset()

Resets the PFC demux context, useful for example after a channel change.

Zvbi.PfcDemux.feed()
--------------------

::

    pfc.feed(buf)

This function takes a raw stream of Teletext packets, filters out the
requested page and stream and assembles the data transmitted in this
page in an internal buffer. When a data block is complete it calls
the handler given during creation.

The handler is called with the following parameters:
*pgno* is the page number given during creation;
*stream* is the stream in which the block was received;
*application_id* is the application ID of the block;
*block* is a scalar holding the block's data;
optional *user_data* is passed through from the creation.

Zvbi.PfcDemux.feed_frame()
--------------------------

::

    ok = pfc.feed_frame(sliced_buf, n_lines)

This function works like *pfc.feed()* but takes a sliced
buffer (i.e. a full frame's worth of sliced data) and automatically
filters out all teletext lines.  This can be used to "short-circuit"
the capture output with the demultiplexer.


.. _Zvbi.XdsDemux:

Class Zvbi.XdsDemux
===================

Separating "Extended Data Service" (XDS) from a Closed Caption stream (EIA
608).

Constructor Zvbi.XdsDemux()
---------------------------

::

    xds = Zvbi.XdsDemux( [callback, user_data] )

Creates and returns a new Extended Data Service (EIA 608) demultiplexer.

The optional parameters *callback* and *user_data* specify
a handler and passed-through parameter which is called when
a new packet is available.

Zvbi.XdsDemux.reset()
---------------------

::

    xds.reset()

Resets the XDS demux, useful for example after a channel change.

Zvbi.XdsDemux.feed()
--------------------

::

    xds.feed(buf)

This function takes two successive bytes of a raw Closed Caption
stream, filters out XDS data and calls the handler function given
during context creation when a new packet is complete.

Parameter *buf* is a scalar holding data from NTSC line 284
(as returned by the slicer functions.)  Only the first two bytes
in the buffer hold valid data.

Returns 0 if the buffer contained parity errors.

The handler is called with the following parameters:
*xds_class* is the XDS packet class, i.e. one of the `VBI_XDS_CLASS_*`
constants.
*xds_subclass* holds the subclass; meaning depends on the main class.
*buffer* is a scalar holding the packet data (already parity decoded.)
optional *user_data* is passed through from the creation.

Zvbi.XdsDemux.feed_frame()
--------------------------

::

    ok = xds.feed_frame(sliced_buf, n_lines)

This function works like *xds.feed()* but takes a sliced
buffer (i.e. a full frame's worth of sliced data) and automatically
filters out all teletext lines.  This can be used to "short-circuit"
the capture output with the demultiplexer.


Miscellaneous (Zvbi)
====================

Zvbi.lib_version()
------------------

::

    major, minor, micro = Zvbi.lib_version()

Returns the major, minor and micro versions of the ZVBI library in
form of a tuple.

Zvbi.check_lib_version()
------------------------

::

    Zvbi.check_lib_version(major, minor, micro)

Returns True if the library version is at least the given version.
The last two parameters are optional and default to zero. Example: ::

    if not Zvbi.check_lib_version(0, 2, 35):
        print("Library version is outdated")


Zvbi.set_log_fn()
-----------------

::

    Zvbi.set_log_fn(mask, log_fn=None, user_data=None)

Various functions can print warnings, errors and information useful to
debug the library or application. This function allows enabling these
messages by specifying a function for printing them. (Note: The kind and
contents of messages logged by particular functions may change in the
future.)

Parameters:

:mask:
    An integer value specifying which kind of information to log.
    If zero, logging is disabled.
    Else this is a bit-wise OR of zero or more of the constants
    `VBI_LOG_ERROR`,
    `VBI_LOG_WARNING`,
    `VBI_LOG_NOTICE`,
    `VBI_LOG_INFO`,
    `VBI_LOG_DEBUG`,
    `VBI_LOG_DRIVER`,
    `VBI_LOG_DEBUG2`,
    `VBI_LOG_DEBUG3`.

:log_fn:
    Callable object to be called for log messages. Omit this parameter to
    disable logging.

:user_data:
    If present, the parameter is passed through as last parameter to each
    call of the function specified by *log_fn*. When not specified, the
    callback is invoked with one less parameter.

The handler function is called with the following parameters:

1. *level*: Is one of the `VBI_LOG_*` constants enumerated above.
2. *context*: Is a text string describing the module where the event
   occurred.
3. *message*: The actual error message.
4. *user_data*: The object passed as *user_data* parameter to *set_log_fn()*.
   The parameter is omitted from the call when omitted as parameter to
   *set_log_fn()*.

Note only 10 event log functions can be registered in a script
at the same time.

Zvbi.set_log_on_stderr()
------------------------

::

    Zvbi.set_log_on_stderr(mask)

This function enables error logging just like *set_log_fn()*,
but uses the library's internal log function which prints
all messages to *stderr*, i.e. on the terminal.

*mask* is a bit-wise OR of zero or more of the `VBI_LOG_*`
constants. The mask specifies which kind of information to log.
To disable logging, pass a zero mask value.

Zvbi.par8()
-----------

::

    par_val = par8(val)

This function encodes the given 7-bit value with Parity. The
result is an 8-bit value in the range 0..255.

Zvbi.unpar8()
-------------

::

    val = Zvbi.unpar8(par_val)

This function decodes the given Parity encoded 8-bit value. The result
is a 7-bit value in the range 0...127 or a negative value when a
parity error is detected.  (Note: to decode parity while ignoring
errors, simply mask out the highest bit, i.e. val &= 0x7F)

Zvbi.par_str()
--------------

::

    byte_str = Zvbi.par_str(data)

This function encodes a string with parity and returns the result
within a bytes object.

Zvbi.unpar_str()
----------------

::

    byte_str = Zvbi.unpar_str(data)

This function decodes a Parity encoded string and returns the result
within a bytes object. (Note despite the name the characters cannot
by returned as Python Unicode string, as the encoding is not known.)

Zvbi.rev8()
-----------

::

    val = Zvbi.rev8(val)

This function reverses the order of all bits of the given 8-bit value
and returns the result. This conversion is required for decoding certain
teletext elements which are transmitted MSB first instead of the usual
LSB first (the teletext VBI slicer already inverts the bit order so that
LSB are in bit #0)

Zvbi.rev16()
------------

::

    val = Zvbi.rev16(val)

This function reverses the order of all bits of the given 16-bit value
and returns the result.

Zvbi.rev16p()
-------------

::

    val = Zvbi.rev16p(data, offset=0)

This function reverses 2 bytes from the given bytes object starting at the
given offset and returns them as a numerical 16-bit value.

Zvbi.ham8()
-----------

::

    ham_val = Zvbi.ham8(val)

This function encodes the given 4-bit value (i.e. range 0..15) with
Hamming-8/4.  The result is an 8-bit value in the range 0..255.

Zvbi.unham8()
-------------

::

    val = Zvbi.unham8(ham_val)

This function decodes the given Hamming-8/4 encoded value. The result
is a 4-bit value, or -1 when there are uncorrectable errors.

Zvbi.unham16p()
---------------

::

    val = Zvbi.unham16p(data, offset=0)

This function decodes 2 Hamming-8/4 encoded bytes (taken from the bytes
object *data* at the given *offset*). The result is an 8-bit value,
or -1 when there are uncorrectable errors.

Zvbi.unham24p()
---------------

::

    val = Zvbi.unham24p(data, offset=0)

This function decodes 3 Hamming-24/18 encoded bytes (taken from the bytes
object *data* at the given *offset*). The result is an 8-bit value,
or -1 when there are uncorrectable errors.

Zvbi.dec2bcd()
--------------

::

    bcd = Zvbi.dec2bcd(dec)

Converts a two's complement binary in range 0 ... 999 into a
packed BCD number (binary coded decimal) in range  0x000 ... 0x999.
Extra digits in the input are discarded.

Zvbi.bcd2dec()
--------------

::

    dec = Zvbi.bcd2dec(bcd)

Converts a packed BCD number in range 0x000 ... 0xFFF into a two's
complement binary in range 0 ... 999. Extra digits in the input
will be discarded.

Zvbi.add_bcd()
--------------

::

    bcd_sum = Zvbi.add_bcd(bcd1, bcd2)

Adds two packed BCD numbers, returning a packed BCD sum. Arguments
and result are in range 0xF0000000 ... 0x09999999, that
is -10**7 ... +10**7 - 1 in decimal notation. To subtract you can
add the 10's complement, e. g. -1 = 0xF9999999.

The return value is a packed BCD number. The result is undefined when
any of the arguments contain hex digits 0xA ... 0xF.

Zvbi.is_bcd()
-------------

::

    yes_no = Zvbi.is_bcd(bcd)

Tests if *bcd* forms a valid BCD number. The argument must be
in range 0x00000000 ... 0x09999999. Return value is 0 if *bcd*
contains hex digits 0xA ... 0xF.

Zvbi.vbi_decode_vps_cni()
-------------------------

::

    cni = Zvbi.decode_vps_cni(data)

This function receives a sliced VPS line and returns a 16-bit CNI value,
or undef in case of errors.

Zvbi.vbi_encode_vps_cni()
-------------------------

::

    byte_str = Zvbi.encode_vps_cni(cni)

This function receives a 16-bit CNI value and returns a VPS line,
or raises exception *ZvbiError* in case of an invalid CNI value that
cannot be encoded (e.g. out of range)

Zvbi.rating_string()
--------------------

::

    rating = Zvbi.rating_string(auth, id)

Translate a program rating code given by *auth* and *id* into a
string, native language.  Raises exception *Zvbi.Error* if this
code is undefined. The input parameters will usually originate from
elements *ev.rating_auth* and *ev.rating_id*, provided within an instance
of type *Zvbi.ProgInfo* for an event of type `VBI_EVENT_PROG_INFO` raised
by `Zvbi.ServiceDec event handling`_.

Zvbi.prog_type_string()
-----------------------

::

    prog_type = Zvbi.prog_type_string(classf, id)

Translate a vbi_program_info program type code into string, currently
English only. Raises exception *Zvbi.Error* if this code is undefined.

The input parameters will usually originate from elements *ev.type_classf*
and *ev.type_id_0* et.al., provided within an instance of type
*Zvbi.ProgInfo* for an event of type `VBI_EVENT_PROG_INFO` raised by
`Zvbi.ServiceDec event handling`_.

Zvbi.iconv_caption()
--------------------

::

    str = Zvbi.iconv_caption(src, repl_char='?')

Converts a string of EIA 608 Closed Caption characters to UTF-8.
The function ignores parity bits and the bytes 0x00 ... 0x1F,
except for two-byte special and extended characters (e.g. music
note 0x11 0x37)  See also *caption_unicode()*.

Returns the converted string *src*, or `undef` when the source
buffer contains invalid two byte characters, or when the conversion
fails, when it runs out of memory.

Optional parameter *repl_char* when present specifies an UCS-2
replacement for characters which are not representable in UTF-8
(i.e. a 16-bit value - use Python's *ord()* to obtain a character's
code value.) When omitted or zero, the function will fail if the
source buffer contains unrepresentable characters.

Zvbi.caption_unicode()
----------------------

::

    str = Zvbi.caption_unicode(c, to_upper=False)

Converts a single Closed Caption character code into a Unicode string.
Codes in range 0x1130 to 0x1B3F are special and extended characters
(e.g. caption command 11 37).

Input character codes in *c* are in ranges:

* 0x0020 ... 0x007F
* 0x1130 ... 0x113F
* 0x1930 ... 0x193F
* 0x1220 ... 0x123F
* 0x1A20 ... 0x1A3F
* 0x1320 ... 0x133F
* 0x1B20 ... 0x1B3F

When True is passed as optional second parameter, the character is converted
into upper case. (Often programs are captioned in all upper case, but
except for one character the basic and special CC character sets contain
only lower case accented characters.)


Examples
========

The `examples` sub-directory in the **Zvbi** package
contains a number of scripts used to test the various interface
functions. You can also use them as examples for your code:

:capture.py:
    Example for the use of class `Zvbi.Capture`_.
    The script captures sliced VBI data from a device.  Output can be
    written to a file or passed via stdout into one of the following
    example scripts.  Call with option `--help` for a list of options.
    (This is a translation of `test/capture.c` in the libzvbi package.)

:decode.py:
    Example for the use of class `Zvbi.ServiceDec`_.
    Decodes sliced VBI data on stdin, e. g. ::

      ./capture --sliced | ./decode --ttx

    Call with option `--help` for a list of options.
    (This is a direct translation of `test/decode.c` in the libzvbi package.)

:caption.py:
    Example for the use of class `Zvbi.ServiceDec`_, type *Zvbi.VBI_EVENT_CAPTION*.
    When called without an input stream, the application displays some
    sample messages (character sets etc.) for debugging the decoder.
    When the input stream is the output of `capture.py --sliced`
    (see above), the applications displays the live CC stream received
    from a VBI device.  The buttons on top switch between Closed Caption
    channels 1-4 and Text channels 1-4.
    (This is a translation of `test/caption.c` in the libzvbi package,
    albeit based on TkInter here.)

:export.py:
    Example for the use of export actions in class `Zvbi.Export`_.
    The script captures from `/dev/vbi0` until the page specified on the
    command line is found and then exports the page in a requested format.
    (This is a direct translation of `test/export.c` in the libzvbi package.)

:explist.py:
    Example for the use of export option management in class `Zvbi.Export`_.
    Test of page export options and menu interfaces.  The script lists
    all available export modules (i.e. formats) and options.
    (This is a direct translation of `test/explist.c` in the libzvbi package.)

:hamm.py:
    Automated test of the odd parity and Hamming encoder and decoder functions.
    Note this test runs for a long time.
    (This is a direct translation of `test/hamm.c` in the libzvbi package.)

:network.py:
    Example for the use of class `Zvbi.ServiceDec`_, type *Zvbi.VBI_EVENT_NETWORK*.
    The script captures from `/dev/vbi0` until the currently tuned channel is
    identified by means of VPS, PDC et.al.
    (This is a direct translation of `examples/network.c` in the libzvbi package.)

:proxy-test.py:
    Example for the use of class `Zvbi.Proxy`_.
    The script can capture either from a proxy daemon or a local device and
    dumps captured data on the terminal. Also allows changing services and
    channels during capturing (e.g. by entering "+ttx" or "-ttx" on stdin.)
    Start with option `-help` for a list of supported command line options.
    (This is a direct translation of `test/proxy-test.c` in the libzvbi package.)

:test-vps.py:
    This script contains tests for encoding and decoding the VPS data service on
    randomly generated data.
    (This is a direct translation of `test/test-vps.c` in the libzvbi package.)

:search-ttx.py:
    Example for the use of class `Zvbi.Search`_.
    The script captures and caches teletext pages until the RETURN key is
    pressed, then prompts for a search string.  A search across teletext pages
    is started, and the content of matching pages is printed on the terminal.

:browse-ttx.py:
    Example for the use of classes `Zvbi.Page`_ and `Zvbi.Export`_ for
    rendering teletext pages. The script captures teletext from a given
    device and renders selected teletext pages in a simple GUI using
    TkInter.

:osc.py:
    Example for the use of class `Zvbi.RawDec`_.
    The script continuously captures raw VBI data and displays the data as
    an animated gray-scale image. Below this, the analog wave line of one
    selected video line is plotted (i.e. essentially simulating an
    oscilloscope). For the selected line, the resulting data from slicing
    is also shown if decoding is successful.
    (This script is loosely based on `test/osc.c` in the libzvbi package.)

:dvb-mux.py:
    Example for the use of class `Zvbi.DvbMux`_.
    This script is a small example for use of the DVD multiplexer functions
    The scripts captures teletext from an analog VBI device and generates
    a PES or TS stream on STDOUT.  Output can be decoded with ::

        ./decode.py --pes --all < dvb_mux.out

Authors
=======

The ZVBI Perl interface module was written by T. Zoerner <tomzo@sourceforge.net>
starting March 2006 for the Teletext EPG grabber accompanying nxtvepg
http://nxtvepg.sourceforge.net/. The Perl module was ported to Python
in April 2020.

The module is based on the libzvbi library, mainly written and maintained
by Michael H. Schimek (2000-2007) and Inaki Garcia Etxebarria (2000-2001),
which in turn is based on AleVT 1.5.1 by Edgar Toernig (1998-1999).
See also http://zapping.sourceforge.net/

License
=======

Copyright (C) 2006-2020 T. Zoerner.

Parts of the descriptions in this man page are copied from the
"libzvbi" documentation, licensed under the GNU General Public
License version 2 or later. The respective copyright is by the following:

* Copyright (C) 2000-2007 Michael H. Schimek,
* Copyright (C) 2000-2001 Inaki Garcia Etxebarria,
* Copyright (C) 2003-2004 Tom Zoerner.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but **without any warranty**; without even the implied warranty of
**merchantability** or **fitness for a particular purpose**.  See the
*GNU General Public License* for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
