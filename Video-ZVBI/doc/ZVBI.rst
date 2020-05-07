===================================================
Zvbi - VBI decoding (teletext, closed caption, ...)
===================================================

SYNOPSIS
========

  import Zvbi

DESCRIPTION
===========

This module provides a Python interface to **libzvbi**.  The ZVBI library
allows to access broadcast data services such as teletext or
closed caption via analog video or DVB capture devices.

Official library description:
"The ZVBI library provides routines to access raw VBI sampling devices
(currently the Linux V4L & V4L2 APIs and the FreeBSD, OpenBSD,
NetBSD and BSDi bktr driver API are supported), a versatile raw VBI
bit slicer, decoders for various data services and basic search, render
and export functions for text pages. The library was written for the
Zapping TV viewer and Zapzilla Teletext browser."

The *Zvbi* Python module covers all exported libzvbi functions. Most of
the functions and parameters are exposed nearly identical, with
adaptions to render the interface *Pythonic*.

Note: This manual page does not reproduce the full documentation
which is available along with libzvbi. Hence it's recommended that
you use the libzvbi documentation in parallel to this one. It is
included in the libzvbi-dev package in the `doc/html` sub-directory
and online at http://zapping.sourceforge.net/doc/libzvbi/index.html

Class Zvbi.capture
==================

The following functions create and return capture contexts with the
given parameters.  Upon success, the returned context can be passed
to the read, pull and other control functions. The context is automatically
deleted and the device closed when the object is destroyed. The meaning
of the parameters to these function is identical to the ZVBI C library.

Upon failure, these functions return *undef* and an explanatory text
in **errorstr**.

Constructor v4l2_new
--------------------

::

    v4l2_new(dev, buffers, services, strict, errorstr, trace)

Initializes a device using the Video4Linux API version 2.  The function
returns a blessed reference to a capture context. Upon error the function
returns `undef` as result and an error message in *errorstr*

Parameters: *dev* is the path of the device to open, usually one of
`/dev/vbi0` or up. *buffers* is the number of device buffers for
raw VBI data if the driver supports streaming. Otherwise one bounce
buffer is allocated for *cap->pull()*  *services* is a logical OR
of `VBI_SLICED_*` symbols describing the data services to be decoded.
On return the services actually decodable will be stored here.
See *Zvbi.raw_dec::add_services()* for details.  If you want to capture
raw data only, set to `VBI_SLICED_VBI_525`, `VBI_SLICED_VBI_625` or
both.  If this parameter is `undef`, no services will be installed.
You can do so later with *cap->update_services()* (Note in this
case the *reset* parameter to that function will have to be set to 1.)
*strict* is passed internally to *Zvbi.raw_dec::add_services()*.
*errorstr* is used to return an error description.  *trace* can be
used to enable output of progress messages on *stderr*.

Constructor v4l_new
-------------------

::

    cap = v4l_new(dev, scanning, services, strict, errorstr, trace)

Initializes a device using the Video4Linux API version 1. Should only
be used after trying Video4Linux API version 2.  The function returns
a blessed reference to a capture context.  Upon error the function
returns `undef` as result and an error message in *errorstr*

Parameters: *dev* is the path of the device to open, usually one of
`/dev/vbi0` or up. *scanning* can be used to specify the current
TV norm for old drivers which don't support ioctls to query the current
norm.  Allowed values are: 625 for PAL/SECAM family; 525 for NTSC family;
0 if unknown or if you don't care about obsolete drivers. *services*,
*strict*, *errorstr*, *trace*: see function *v4l2_new()* above.

=item cap = v4l_sidecar_new(dev, given_fd, services, strict, errorstr, trace)

Same as *v4l_new()* however working on an already open device.
Parameter *given_fd* must be the numerical file handle, i.e. as
returned by Perl's **fileno**.

Constructor bktr_new
--------------------

::

    cap = bktr_new(dev, scanning, services, strict, errorstr, trace)

Initializes a video device using the BSD driver.
Result and parameters are identical to function *v4l2_new()*

Constructor dvb_new
-------------------

::

    cap = dvb_new(dev, scanning, services, strict, errorstr, trace)

Initializes a DVB video device.  This function is deprecated as it has many
bugs (see libzvbi documentation for details). Use *dvb_new2()* instead.

Constructor dvb_new2
--------------------

::

    dvb_new2(dev, pid, errorstr, trace)

Initializes a DVB video device.  The function returns a blessed reference
to a capture context.  Upon error the function returns `undef` as result
and an error message in *errorstr*

Parameters: *dev* is the path of the DVB device to open.
*pid* specifies the number (PID) of a stream which contains the data.
You can pass 0 here and set or change the PID later with *cap->dvb_filter()*.
*errorstr* is used to return an error descriptions.  *trace* can be
used to enable output of progress messages on *stderr*.

Construction via Zvbi.proxy
---------------------------

::

    cap = proxy->proxy_new(buffers, scanning, services, strict, errorstr)

Open a new connection to a VBI proxy to open a VBI device for the
given services.  On side of the proxy daemon, one of the regular
capture context creation functions (e.g. *v4l2_new()*) is invoked.
If the creation succeeds, and any of the requested services are
available, capturing is started and all captured data is forwarded
transparently to the client.

Whenever possible the proxy should be used instead of opening the device
directly, since it allows the user to start multiple VBI clients in
parallel.  When this function fails (usually because the user hasn't
started the proxy daemon) applications should automatically fall back
to opening the device directly.

Result: The function returns a blessed reference to a capture context.
Upon error the function returns `undef` as result and an error message
in *errorstr*

Parameters: *proxy* is a reference to a previously created proxy
client context (*Zvbi.proxy*). The remaining
parameters have the same meaning as described above, as they are used
by the daemon when opening the device.
*buffers* specifies the number of intermediate buffers on server side
of the proxy socket connection. (Note this is not related to the
device buffer count.)
*scanning* indicates the current norm: 625 for PAL and
525 for NTSC; set to 0 if you don't know (you should not attempt
to query the device for the norm, as this parameter is only required
for old v4l1 drivers which don't support video standard query ioctls.)
*services* is a set of `VBI_SLICED_*` symbols describing the data
services to be decoded. On return *services* contains actually
decodable services.  See *Zvbi.raw_dec::add_services()*
for details.  If you want to capture raw data only, set to
`VBI_SLICED_VBI_525`, `VBI_SLICED_VBI_625` or both.  If this
parameter is `undef`, no services will be installed.  *strict* has
the same meaning as described in the device-specific capture context
creation functions.  *errorstr* is used to return an error message
when the function fails.

Member functions
----------------

The following functions are used to read raw and sliced VBI data from
a previously created capture context *cap* (the reference is implicitly
inserted as first parameter when the functions are invoked as listed
below.) All these functions return a status result code: -1 on error
(and an error indicator in `!`), 0 on timeout (i.e. no data arrived
within *timeout_ms* milliseconds) or 1 on success.

There are two different types of capture functions: The functions
named `read...` copy captured data into the given Perl scalar. In
contrast the functions named `pull...` leave the data in internal
buffers inside the capture context and just return a blessed reference
to this buffer. When you need to access the captured data directly
via Perl, choose the read functions. When you use functions of this
module for further decoding, you should use the pull functions since
these are usually more efficient.

=over 4

=item cap->read_raw(raw_buf, timestamp, timeout_ms)

Read a raw VBI frame from the capture device into scalar *raw_buf*.
The buffer variable is automatically extended to the exact length
required for the frame's data.  On success, the function returns
in *timestamp* the capture instant in seconds and fractions
since 1970-01-01 00:00 in double format.

Parameter *timeout_ms* gives the limit for waiting for data in
milliseconds; if no data arrives within the timeout, the function
returns 0.  Note the function may fail if the device does not support
reading data in raw format.

=item cap->read_sliced(sliced_buf, n_lines, timestamp, timeout_ms)

Read a sliced VBI frame from the capture context into scalar
*sliced_buf*.  The buffer is automatically extended to the length
required for the sliced data.  Parameter *timeout_ms* specifies the
limit for waiting for data (in milliseconds.)

On success, the function returns in *timestamp* the capture instant
in seconds and fractions since 1970-01-01 00:00 in double format and
in *n_lines* the number of sliced lines in the buffer. Note for
efficiency the buffer is an array of vbi_sliced C structures. Use
*get_sliced_line()* to process the contents in Perl, or pass the buffer
directly to class *Zvbi.vt* or other decoder objects.

Note: it's generally more efficient to use *pull_sliced()*
instead, as that one may avoid having to copy sliced data into the
given buffer (e.g. for the VBI proxy)

=item cap->read(raw_buf, sliced_buf, n_lines, timestamp, timeout_ms)

This function is a combination of *read_raw()* and *read_sliced()*, i.e.
reads a raw VBI frame from the capture context into *raw_buf* and
decodes it to sliced data which is returned in *sliced_buf*. For
details on parameters see above.

Note: Depending on the driver, captured raw data may have to be copied
from the capture buffer into the given buffer (e.g. for v4l2 streams which
use memory mapped buffers.)  It's generally more efficient to use one of
the following "pull" interfaces, especially if you don't require access
to raw data at all.

=item cap->pull_raw(ref, timestamp, timeout_ms)

Read a raw VBI frame from the capture context, which is returned in
*ref* in form of a blessed reference to an internal buffer.  The data
remains valid until the next call to this or any other "pull" function.
The reference can be passed to the raw decoder function.  If you need to
process the data in Perl, use *read_raw()* instead.  For all other cases
*read_raw()* is more efficient as it may avoid copying the data.

On success, the function returns in *timestamp* the capture instant in
seconds and fractions since 1970-01-01 00:00 in double format.  Parameter
*timeout_ms* specifies the limit for waiting for data (in milliseconds.)
Note the function may fail if the device does not support reading data
in raw format.

=item cap->pull_sliced(ref, n_lines, timestamp, timeout_ms)

Read a sliced VBI frame from the capture context, which is returned in
*ref* in form of a blessed reference to an internal buffer. The data
remains valid until the next call to this or any other "pull" function.
The reference can be passed to *get_sliced_line()* to process the data in
Perl, or it can be passed to a *Zvbi.vt* decoder object.

On success, the function returns in *timestamp* the capture instant
in seconds and fractions since 1970-01-01 00:00 in double format and
in *n_lines* the number of sliced lines in the buffer.  Parameter
*timeout_ms* specifies the limit for waiting for data (in milliseconds.)

=item cap->pull(raw_ref, sliced_ref, sliced_lines, timestamp, timeout_ms)

This function is a combination of *pull_raw()* and *pull_sliced()*, i.e.
returns blessed references to an internal raw data buffer in *raw_ref*
and to a sliced data buffer in *sliced_ref*. For details on parameters
see above.

=back

For reasons of efficiency the data is not immediately converted into
Perl structures. Functions of the "read" variety return a single
byte-string in the given scalar which contains all VBI lines.
Functions of the "pull" variety just return a binary reference
(i.e. a C pointer) which cannot be used by Perl for other purposes
than passing it to further processing functions.  To process either
read or pulled data by Perl code, use the following function:

=over 4

=item (data, id, line) = cap->get_sliced_line(buffer, line_idx)

The function takes a buffer which was filled by one of the slicer
or capture & slice functions and a line index. The index must be lower
than the line count returned by the slicer.  The function returns
a list of three elements: sliced data from the respective line in
the buffer, slicer type (`VBI_SLICED_...`) and physical line number.

The structure of the data returned in the first element depends on
the kind of data in the VBI line (e.g. for teletext it's 42 bytes,
partly hamming 8/4 and parity encoded; the content in the scalar
after the 42 bytes is undefined.)

=back

The following control functions work as described in the libzvbi
documentation.

=over 4

=item cap->parameters()

Returns a hash reference describing the physical parameters of the
VBI source.  This hash can be used to initialize the raw decoder
context described below.

The hash array has the following members:

=over 8

=item scanning

Either 525 (M/NTSC, M/PAL) or 625 (PAL, SECAM), describing the scan
line system all line numbers refer to.

=item sampling_format

Format of the raw VBI data.

=item sampling_rate

Sampling rate in Hz, the number of samples or pixels captured per second.

=item bytes_per_line

Number of samples or pixels captured per scan line, in bytes. This
determines the raw VBI image width and you want it large enough to
cover all data transmitted in the line (with headroom).

=item offset

The distance from 0H (leading edge hsync, half amplitude point) to
the first sample (pixel) captured, in samples (pixels). You want an
offset small enough not to miss the start of the data transmitted.

=item start_a, start_b

First scan line to be captured, first and second field respectively,
according to the ITU-R line numbering scheme (see vbi_sliced). Set
to zero if the exact line number isn't known.

=item count_a, count_b

Number of scan lines captured, first and second field respectively.
This can be zero if only data from one field is required. The sum
count_a + count_b determines the raw VBI image height.

=item interlaced

In the raw vbi image, normally all lines of the second field are
supposed to follow all lines of the first field. When this flag is
set, the scan lines of first and second field will be interleaved in
memory. This implies count_a and count_b are equal.

=item synchronous

Fields must be stored in temporal order, i. e. as the lines have been
captured. It is assumed that the first field is also stored first in
memory, however if the hardware cannot reliable distinguish fields this
flag shall be cleared, which disables decoding of data services
depending on the field number.

=back

=item services = cap->update_services(reset, commit, services, strict, errorstr)

Adds and/or removes one or more services to an already initialized capture
context.  Can be used to dynamically change the set of active services.
Internally the function will restart parameter negotiation with the
VBI device driver and then call *rd->add_services()* on the internal raw
decoder context.  You may set *reset* to rebuild your service mask from
scratch.  Note that the number of VBI lines may change with this call
even if a negative result is returned.

Result: The function returns a bit-mask of supported services among those
requested (not including previously added services), 0 upon errors.

*reset* when set, clears all previous services before adding new
ones (by invoking *raw_dec->reset()* at the appropriate time.)
*commit* when set, applies all previously added services to the device;
when doing subsequent calls of this function, commit should be set only
for the last call.  Reading data cannot continue before changes were
committed (because capturing has to be suspended to allow resizing the
VBI image.)  Note this flag is ignored when using the VBI proxy.
*services* contains a set of `VBI_SLICED_*` symbols describing the
data services to be decoded. On return the services actually decodable
will be stored here, i.e. the behaviour is identical to *v4l2_new()* etc.
*strict* and *errorstr* are also same as during capture context
creation.

=item cap->fd()

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

=item cap->get_scanning()

This function is intended to allow the application to check for
asynchronous norm changes, i.e. by a different application using the
same device.  The function queries the capture device for the current
norm and returns value 625 for PAL/SECAM norms, 525 for NTSC;
0 if unknown, -1 on error.

=item cap->flush()

After a channel change this function should be used to discard all
VBI data in intermediate buffers which may still originate from the
previous TV channel.

=item cap->set_video_path(dev)

The function sets the path to the video device for TV norm queries.
Parameter *dev* must refer to the same hardware as the VBI device
which is used for capturing (e.g. `/dev/video0` when capturing from
`/dev/vbi0`) Note: only useful for old video4linux drivers which don't
support norm queries through VBI devices.

=item cap->get_fd_flags()

Returns properties of the capture context's device. The result is an OR
of one or more `VBI_FD_*` constants:

=over 8

=item VBI_FD_HAS_SELECT

Is set when *select(2)* can be used on the filehandle returned by
*cap->fd()* to wait for new data on the capture device file handle.

=item VBI_FD_HAS_MMAP

Is set when the capture device supports "user-space DMA".  In this case
it's more efficient to use one of the "pull" functions to read raw data
because otherwise the data has to be copied once more into the passed buffer.

=item VBI_FD_IS_DEVICE

Is not set when the capture device file handle is not the actual device.
In this case it can only be used for select(2) and not for ioctl(2)

=back

=item cap->dvb_filter(pid)

Programs the DVB device transport stream demultiplexer to filter
out PES packets with the given *pid*.  Returns -1 on failure,
0 on success.

=item cap->dvb_last_pts()

Returns the presentation time stamp (33 bits) associated with the data
last read from the context. The PTS refers to the first sliced
VBI line, not the last packet containing data of that frame.

Note timestamps returned by VBI capture read functions contain
the sampling time of the data, that is the time at which the
packet containing the first sliced line arrived.

=back

Class Zvbi.proxy
================

The following functions are used for receiving sliced or raw data from
VBI proxy daemon.  Using the daemon instead of capturing directly from
a VBI device allows multiple applications to capture concurrently,
e.g. to decode multiple data services.

=over 4

=item proxy = create(dev, client_name, flags, errorstr, trace)

Creates and returns a new proxy context, or `undef` upon error.
(Note in reality this call will always succeed, since a connection to
the proxy daemon isn't established until you actually open a capture
context via *proxy->proxy_new()*)

Parameters: *dev* contains the name of the device to open, usually one of
`/dev/vbi0` and up.  Note: should be the same path as used by the proxy
daemon, else the client may not be able to connect.  *client_name*
names the client application, typically identical to *0* (without the
path though)  Can be used by the proxy daemon to fine-tune scheduling or
to present the user with a list of currently connected applications.
*flags* can contain one or more members of `VBI_PROXY_CLIENT_*` flags.
*errorstr* is used to return an error descriptions.  *trace* can be
used to enable output of progress messages on *stderr*.

=item proxy->get_capture_if()

This function is not supported as it does not make sense for the
Perl module.  In libzvbi the function returns a reference to a capture
context created from the proxy context via *proxy->proxy_new()*>.
In Perl, you must keep the reference anyway, because otherwise the
capture context would be automatically closed and destroyed.  So you
can just use the stored reference instead of using this function.

=item proxy->set_callback(\&callback [, user_data])

Installs or removes a callback function for asynchronous messages (e.g.
channel change notifications.)  The callback function is typically invoked
while processing a read from the capture device.

Input parameters are a function reference *callback* and an optional
scalar *user_data* which is passed through to the callback unchanged.
Call without arguments to remove the callback again.

The callback function will receive the event mask (i.e. one of the
constants `VBI_PROXY_EV_*` in the following list) and, if provided,
*user_data* as parameters.

=over 8

=item VBI_PROXY_EV_CHN_GRANTED

The channel control token was granted, so that the client may now change the
channel.  Note: the client should return the token after the channel change
was completed (the channel will still remain reserved for the requested
time.)

=item VBI_PROXY_EV_CHN_CHANGED

The channel (e.g. TV tuner frequency) was changed by another proxy client.

=item VBI_PROXY_EV_NORM_CHANGED

The TV norm was changed by another client (in a way which affects VBI,
e.g. changes between PAL/SECAM are ignored.)  The client must update
its services, else no data will be forwarded by the proxy until
the norm is changed back.

=item VBI_PROXY_EV_CHN_RECLAIMED

The proxy daemon requests to return the channel control token.  The client
is no longer allowed to switch the channel and must immediately reply with
a channel notification with flag `VBI_PROXY_CHN_TOKEN`

=item VBI_PROXY_EV_NONE

No news.

=back

=item proxy->get_driver_api()

This function can be used to query which driver is behind the
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

=item proxy->channel_request(chn_prio [, profile])

This function is used to request permission to switch channels or norm.
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
*proxy->has_channel_control()*> can be used to poll for it.

To set the priority level to "background" only without requesting a channel,
omit the *profile* parameter. Else, this parameter must be a reference
to a hash with the following members: "sub_prio", "allow_suspend",
"min_duration" and "exp_duration".

=item proxy->channel_notify(notify_flags [, scanning])

Sends channel control request to proxy daemon. Parameter
*notify_flags* is an OR of one or more of the following constants:

=over 8

=item VBI_PROXY_CHN_RELEASE

Revoke a previous channel request and return the channel switch
token to the daemon.

=item VBI_PROXY_CHN_TOKEN

Return the channel token to the daemon without releasing the
channel; This should always be done when the channel switch has
been completed to allow faster scheduling in the daemon (i.e. the
daemon can grant the token to a different client without having
to reclaim it first.)

=item VBI_PROXY_CHN_FLUSH

Indicate that the channel was changed and VBI buffer queue
must be flushed; Should be called as fast as possible after
the channel and/or norm was changed.  Note this affects other
clients' capturing too, so use with care.  Other clients will
be informed about this change by a channel change indication.

=item VBI_PROXY_CHN_NORM

Indicate a norm change.  The new norm should be supplied in
the scanning parameter in case the daemon is not able to
determine it from the device directly.

=item VBI_PROXY_CHN_FAIL

Indicate that the client failed to switch the channel because
the device was busy. Used to notify the channel scheduler that
the current time slice cannot be used by the client.  If the
client isn't able to schedule periodic re-attempts it should
also return the token.

=back

=item proxy->channel_suspend(cmd)

Request to temporarily suspend capturing (if *cmd* is
`VBI_PROXY_SUSPEND_START`) or revoke a suspension (if *cmd*
equals `VBI_PROXY_SUSPEND_STOP`.)

=item proxy->device_ioctl(request, arg)

This function allows to manipulate parameters of the underlying
VBI device.  Not all ioctls are allowed here.  It's mainly intended
to be used for channel enumeration and channel/norm changes.
The request codes and parameters are the same as for the actual device.
The caller has to query the driver API via *proxy->get_driver_api()*>
first and use the respective ioctl codes, same as if the device would
be used directly.

Parameters and results are equivalent to the called **ioctl** operation,
i.e. *request* is an IO code and *arg* is a packed binary structure.
After the call *arg* may be modified for operations which return data.
You must make sure the result buffer is large enough for the returned data.
Use Perl's *pack* to build the argument buffer. Example:

  # get current config of the selected channel
  vchan = pack("ix32iLss", channel, 0, 0, 0, norm);
  proxy->device_ioctl(VIDIOCGCHAN, vchan);

The result is 0 upon success, else and `!` set appropriately.  The function
also will fail with error code `EBUSY` if the client doesn't have permission
to control the channel.

=item proxy->get_channel_desc()

Retrieve info sent by the proxy daemon in a channel change indication.
The function returns a list with two members: scanning value (625, 525 or 0)
and a boolean indicator if the change request was granted.

=item proxy->has_channel_control()

Returns 1 if client is currently allowed to switch channels, else 0.

=back

See **examples/proxy-test.pl** for examples how to use these functions.

Class Zvbi.rawdec
=================

The functions in this section allow converting raw VBI samples to
bits and bytes (i.e. analog to digital conversion - even though the
data in a raw VBI buffer is obviously already digital, it's just a
sampled image of the analog wave line.)

These functions are used internally by libzvbi if you use the slicer
functions of the capture object (e.g. *pull_sliced*)

=over 4

=item rd = Zvbi.rawdec::new(ref)

Creates and initializes a new raw decoder context. Parameter *ref*
specifies the physical parameters of the raw VBI image, such as the
sampling rate, number of VBI lines etc.  The parameter can be either
a reference to a capture context (*Zvbi.capture*)
or a reference to a hash. The contents for the hash are as returned
by method *cap->parameters()* on capture contexts, i.e. they
describe the physical parameters of the source.

=item services = Zvbi.rawdec::parameters(href, services, scanning, max_rate)

Calculate the sampling parameters required to receive and decode the
requested data services.  This function can be used to initialize
hardware prior to calling *rd->add_service()*.  The returned sampling
format is fixed to `VBI_PIXFMT_YUV420`, and attribute `href-`bytes_per_line>
is set to a reasonable minimum.

Input parameters: *href* must be a reference to a hash which is filled
with sampling parameters on return (contents see
*Zvbi.capture::parameters()*.)
*services* is a set of `VBI_SLICED_*` constants. Here (and only here)
you can add `VBI_SLICED_VBI_625` or `VBI_SLICED_VBI_525` to include all
VBI scan lines in the calculated sampling parameters.
If *scanning* is set to 525 only NTSC services are accepted; if set to
625 only PAL/SECAM services are accepted. When scanning is 0, the norm is
determined from the requested services; an ambiguous set will result in
undefined behaviour.

The function returns a set of `VBI_SLICED_*` constants describing the
data services covered by the calculated sampling parameters returned in
*href*. This excludes services the libzvbi raw decoder cannot decode
assuming the specified physical parameters.
 On return parameter *max_rate* is set to the highest data bit rate
in **Hz** of all services requested (The sampling rate should be at least
twice as high; attribute `href-`{sampling_rate}> will be set by libzvbi to a more
reasonable value of 27 MHz derived from ITU-R Rec. 601.)

=item rd->reset()

Reset a raw decoder context. This removes all previously added services
to be decoded (if any) but does not touch the sampling parameters. You
are free to change the sampling parameters after calling this.

=item services = rd->add_services(services, strict)

After you initialized the sampling parameters in raw decoder context
(according to the abilities of your VBI device), this function adds one
or more data services to be decoded. The libzvbi raw VBI decoder can
decode up to eight data services in parallel. You can call this function
while already decoding, it does not change sampling parameters and you
must not change them either after calling this.

Input parameters: *services* is a set of `VBI_SLICED_*` constants
(see also description of the *parameters* function above.)
*strict* is value of 0, 1 or 2 and requests loose, reliable or strict
matching of sampling parameters respectively. For example if the data
service requires knowledge of line numbers while they are not known,
value 0 will accept the service (which may work if the scan lines are
populated in a non-confusing way) but values 1 or 2 will not. If the
data service may use more lines than are sampled, value 1 will still
accept but value 2 will not. If unsure, set to 1.

Returns a set of `VBI_SLICED_*` constants describing the data services
that actually can be decoded. This excludes those services not decodable
given sampling parameters of the raw decoder context.

=item services = rd->check_services(services, strict)

Check and return which of the given services can be decoded with
current physical parameters at a given strictness level.
See *add_services* for details on parameter semantics.

=item services = rd->remove_services(services)

Removes one or more data services given in input parameter *services*
to be decoded from the raw decoder context.  This function can be called
at any time and does not touch sampling parameters stored in the context.

Returns a set of `VBI_SLICED_*` constants describing the remaining
data services that will be decoded.

=item rd->resize(start_a, count_a, start_b, count_b)

Grows or shrinks the internal state arrays for VBI geometry changes.
Returns `undef`.

=item n_lines = rd->decode(ref, buf)

This is the main service offered by the raw decoder: Decodes a raw VBI
image given in *ref*, consisting of several scan lines of raw VBI data,
into sliced VBI lines in *buf*. The output is sorted by line number.

The input *ref* can either be a scalar filled by one of the "read" kind of
capture functions (or any scalar filled with a byte string with the correct
number of samples for the current geometry), or a blessed reference to
an internal capture buffer as returned by the "pull" kind of capture
functions. The format of the output buffer is the same as described
for *cap->read_sliced()*.  Return value is the number of lines decoded.

Note this function attempts to learn which lines carry which data
service, or none, to speed up decoding.  Hence you must use different
raw decoder contexts for different devices.

=back

Class Zvbi.dvb_mux
==================

These functions convert raw and/or sliced VBI data to a DVB Packetized
Elementary Stream or Transport Stream as defined in EN 300 472 "Digital
Video Broadcasting (DVB); Specification for conveying ITU-R System B
Teletext in DVB bitstreams" and EN 301 775 "Digital Video Broadcasting
(DVB); Specification for the carriage of Vertical Blanking Information
(VBI) data in DVB bitstreams".

Note EN 300 468 "Digital Video Broadcasting (DVB); Specification for
Service Information (SI) in DVB systems" defines another method to
transmit VPS data in DVB streams. Libzvbi does not provide functions
to generate SI tables but the *encode_dvb_pdc_descriptor()* function
is available to convert a VPS PIL to a PDC descriptor (since version 0.3.0)

**Available:** All of the functions in this section are available
only since libzvbi version 0.2.26

=over 4

=item mx = pes_new( [callback, user_data] )

Creates a new DVB VBI multiplexer converting raw and/or sliced VBI data
to MPEG-2 Packetized Elementary Stream (PES) packets as defined in the
standards EN 300 472 and EN 301 775.  Returns `undef` upon error.

Parameter *callback* specifies a handler which is called by
*mx->feed()* when a new packet is available. must be omitted if
*mx->cor()* is used.  The *user_data* is passed through to
the handler.  For further callback parameters see the description
of the *feed* function.

=item mx = ts_new(pid [, callback, user_data] )

Allocates a new DVB VBI multiplexer converting raw and/or sliced VBI data
to MPEG-2 Transport Stream (TS) packets as defined in the standards
EN 300 472 and EN 301 775. Returns `undef` upon error.

Parameter *pid* is a program ID that will be stored in the header of the
generated TS packets. The value must be in range 0x0010 to 0x1FFE inclusive.

Parameter *callback* specifies a handler which is called by
*mx->feed()* when a new packet is available. Must be omitted if
*mx->cor()* is used.  The *user_data* is passed through to
the handler.  For further callback parameters see the description
of the *feed* function.

=item mx->mux_reset()

This function clears the internal buffers of the DVB VBI multiplexer.

After a reset call the *mx->cor()* function will encode a new
PES packet, discarding any data of the previous packet which has not
been consumed by the application.

=item mx->cor(buf, buffer_left, sliced, sliced_left, service_mask, pts [, raw, sp])

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
reference to a slicer buffer, or a scalar with a byte string consisting
of sliced data (i.e. the same formats are accepted as by *vt->decode()*.
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

*raw* shall contain a raw VBI frame of (*sp->{count_a}*
+ *sp->{count_b}*) lines times *sp->{bytes_per_line}*.
The function encodes only those lines which have been selected by sliced
lines in the *sliced* array with id `VBI_SLICED_VBI_625`
The data field of these structures is ignored. When the sliced input
buffer does not contain such structures *raw* can be omitted.
*sp* Describes the data in the raw buffer unless raw is omitted.
Else it must be valid, with the constraints described for *feed()*
below.

The function returns 0 on failures, which may occur under the
following circumstances:

=over 4

=item *

The maximum PES packet size, or the value selected with
*mx->set_pes_packet_size()*, is too small to contain all
the sliced and raw VBI data.

=item *

The sliced array is not sorted by ascending line number,
except for elements with line number 0 (undefined).

=item *

Only the following data services can be encoded:
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

=item *

A vbi_sliced structure contains a line number outside the
valid range specified above.

=item *

parameter *raw* is undefined although the sliced array contains
a structure with id `VBI_SLICED_VBI_625`.

=item *

One or more members of the *sp* structure are invalid.

=item *

A vbi_sliced structure with id `VBI_SLICED_VBI_625`
contains a line number outside the ranges defined by *sp*.

=back

On all errors *sliced_left* will refer to the offending sliced
line in the index buffer (i.e. relative to the end of the buffer)
and the output buffer remains unchanged.

=item mx->feed(sliced, sliced_lines, service_mask, pts [, raw, sp])

This function converts raw and/or sliced VBI data to one DVB VBI PES
packet or one or more TS packets as defined in EN 300 472 and
EN 301 775. To deliver output, the callback function passed to
*pes_new()* or *ts_new()* is called once for each PES or TS packet.

Parameters:
*sliced* contains the sliced VBI data to be converted. All data
must belong to the same video frame.  *sliced* is either a blessed
reference to a slicer buffer, or a scalar with a byte string consisting
of sliced data (i.e. the same formats are accepted as by *vt->decode()*.
*sliced_lines* number of valid lines in the *sliced* input buffer.
*service_mask* Only data services in this set will be
encoded. Other data services in the sliced buffer will be
discarded without further checks. Create a set by ORing
`VBI_SLICED_*` constants.
*pts* This Presentation Time Stamp will be encoded into the
PES packet. Bits 33 ... 63 are discarded.

*raw* shall contain a raw VBI frame of (*sp->{count_a}*
+ *sp->{count_b}*) lines times *sp->{bytes_per_line}*.
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

=item mx->get_data_identifier()

Returns the data_identifier the multiplexer encodes into PES packets.

=item ok = mx->set_data_identifier(data_identifier)

This function can be used to determine the *data_identifier* byte
to be stored in PES packets.
For compatibility with decoders compliant to EN 300 472 this should
be a value in the range 0x10 to 0x1F inclusive. The values 0x99
to 0x9B inclusive as defined in EN 301 775 are also permitted.
The default data_identifier is 0x10.

Returns 0 if *data_identifier* is outside the valid range.

=item size = mx->get_min_pes_packet_size()

Returns the maximum size of PES packets the multiplexer generates.

=item size = mx->get_max_pes_packet_size()

Returns the minimum size of PES packets the multiplexer generates.

=item ok = mx->set_pes_packet_size(min_size, max_size)

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

=back

The next functions provide similar functionality as described above, but
are special as they work without a *dvb_mux* object.
Meaning and use of parameters is the same as described above.

=over 4

=item Zvbi.dvb_multiplex_sliced(buf, buffer_left, sliced, sliced_left, service_mask, data_identifier, stuffing)

Converts the sliced VBI data in the *sliced* buffer to VBI data
units as defined in EN 300 472 and EN 301 775 and stores them
in *buf* as output buffer.

=item Zvbi.dvb_multiplex_raw(buf, buffer_left, raw, raw_left, data_identifier, videostd_set, line, first_pixel_position, n_pixels_total, stuffing)

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

=back


Class Zvbi.dvb_demux
====================

Separating VBI data from a DVB PES stream (EN 301 472, EN 301 775).

=over 4

=item dvb = Zvbi.dvb_demux::pes_new( [callback [, user_data]] )

Creates a new DVB VBI demultiplexer context taking a PES stream as input.
Returns a reference to the newly allocated DVB demux context.

The optional callback parameters should only be present if decoding will
occur via the *dvb>feed()* method.  The function referenced by
*callback* will be called inside of *dvb->feed()* whenever
new sliced data is available. Optional parameter *user_data* is
appended to the callback parameters. See *dvb>feed()* for
additional details.

=item dvb->reset()

Resets the DVB demux to the initial state as after creation.
Intended to be used after channel changes.

=item n_lines = dvb->cor(sliced, sliced_lines, pts, buf, buf_left)

This function takes an arbitrary number of DVB PES data bytes in *buf*,
filters out *PRIVATE_STREAM_1* packets, filters out valid VBI data units,
converts them to sliced buffer format and stores the data at *sliced*.
Usually the function will be called in a loop:

  left = length(buffer);
  while (left > 0) {
    n_lines = dvb->cor (sliced, 64, pts, buffer, left);
    if (n_lines > 0) {
      vt->decode(sliced, n_lines, pts_conv(pts));
    }
  }

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

=item ok = dvb->feed(buf)

This function takes an arbitrary number of DVB PES data bytes in *buf*,
filters out *PRIVATE_STREAM_1* packets, filters out valid VBI data units,
converts them to vbi_sliced format and calls the callback function given
during creation of the context. Returns 0 if the data contained errors.

The function is similar to *dvb->cor()*, but uses an internal
buffer for sliced data.  Since this function does not return sliced
data, it's only useful if you have installed a handler. Do not mix
calls to this function with *dvb->cor()*.

The callback function is called with the following parameters:

  ok = &callback(sliced_buf, n_lines, pts, user_data);

*sliced* is a reference to a buffer holding sliced data; the reference
has the same type as returned by capture functions. *n_lines* specifies
the number of valid lines in the buffer. *pts* is the timestamp.
The last parameter is *user_data*, if given during creation.
The handler should return 1 on success, 0 on failure.

Note: Demultiplexing of raw VBI data is not supported yet,
raw data will be discarded.

=item dvb->set_log_fn(mask [, log_fn [, user_data]])

The DVB demultiplexer supports the logging of errors in the PES stream and
information useful to debug the demultiplexer.
With this function you can redirect log messages generated by this module
from general log function *Zvbi.set_log_fn()* to a
different function or enable logging only in the DVB demultiplexer.
The callback can be removed by omitting the handler name.

Input parameters: *mask* specifies which kind of information to log;
may be zero. *log_fn* is a reference to the handler function.
Optional *user_data* is passed through to the handler.

The handler is called with the following parameters: *level*,
*context*, *message* and, if given, *user_data*.

Note: Kind and contents of log messages may change in the future.

=back

Class Zvbi.idl_demux
====================

The functions in this section decode data transmissions in
Teletext **Independent Data Line** packets (EN 300 708 section 6),
i.e. data transmissions based on packet 8/30.

=over 4

=item idl = Zvbi.idl_demux::new(channel, address [, callback, user_data] )

Creates and returns a new Independent Data Line format A
(EN 300 708 section 6.5) demultiplexer.

*channel* filter out packets of this channel.
*address* filter out packets with this service data address.
Optional: *callback* is a handler to be called by *idl->feed()*
when new data is available.  If present, *user_data* is passed through
to the handler function.

=item idl->reset(dx)

Resets the IDL demux context, useful for example after a channel change.

=item ok = idl->feed(buf)

This function takes a stream of Teletext packets, filters out packets
of the desired data channel and address and calls the handler
given context creation when new user data is available.

Parameter *buf* is a scalar containing a teletext packet's data
(at last 42 bytes, i. e. without clock run-in and framing code),
as returned by the slicer functions.  The function returns 0 if
the packet contained incorrectable errors.

Parameters to the handler are: *buffer*, *flags*, *user_data*.

=item ok = idl->feed_frame(sliced_buf, n_lines)

This function works like *idl->feed()* but takes a sliced
buffer (i.e. a full frame's worth of sliced data) and automatically
filters out all teletext lines.  This can be used to "short-circuit"
the capture output with the demultiplexer.

**Available:** since libzvbi version 0.2.26

=back

Class Zvbi.pfc_demux
====================

Separating data transmitted in Page Function Clear Teletext packets
(ETS 300 708 section 4), i.e. using regular packets on a dedicated
teletext page.

=over 4

=item pfc = Zvbi.pfc_demux::new(pgno, stream [, callback, user_data] )

Creates and returns a new demultiplexer context.

Parameters: *page* specifies the teletext page on which the data is
transmitted.  *stream* is the stream number to be demultiplexed.

Optional parameter *callback* is a reference to a handler to be
called by *pfc->feed()* when a new data block is available.
Is present, *user_data* is passed through to the handler.

=item pfc->reset()

Resets the PFC demux context, useful for example after a channel change.

=item pfc->feed(buf)

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

=item ok = pfc->feed_frame(sliced_buf, n_lines)

This function works like *pfc->feed()* but takes a sliced
buffer (i.e. a full frame's worth of sliced data) and automatically
filters out all teletext lines.  This can be used to "short-circuit"
the capture output with the demultiplexer.

**Available:** since libzvbi version 0.2.26

=back

Class Zvbi.xds_demux
====================

Separating XDS data from a Closed Caption stream (EIA 608).

=over 4

=item xds = Zvbi.xds_demux::new( [callback, user_data] )

Creates and returns a new Extended Data Service (EIA 608) demultiplexer.

The optional parameters *callback* and *user_data* specify
a handler and passed-through parameter which is called when
a new packet is available.

=item xds->reset()

Resets the XDS demux, useful for example after a channel change.

=item xds->feed(buf)

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

=item ok = xds->feed_frame(sliced_buf, n_lines)

This function works like *xds->feed()* but takes a sliced
buffer (i.e. a full frame's worth of sliced data) and automatically
filters out all teletext lines.  This can be used to "short-circuit"
the capture output with the demultiplexer.

**Available:** since libzvbi version 0.2.26

=back

Class Zvbi.vt
=============

This section describes high level decoding functions.  Input to the
decoder functions in this section is sliced data, as returned from
capture objects (*Zvbi.capture*) or the raw decoder
(*Zvbi.rawdec*)

=over 4

=item vt = Zvbi.vt::decoder_new()

Creates and returns a new data service decoder instance.

Note the type of data services to be decoded is determined by the
type of installed callbacks. Hence you must install at least one
callback using *vt->event_handler_register()*.

=item vt->decode(buf, n_lines, timestamp)

This is the main service offered by the data service decoder:
Decodes zero or more lines of sliced VBI data from the same video
frame, updates the decoder state and invokes callback functions
for registered events.  The function always returns `undef`.

Input parameters: *buf* is either a blessed reference to a slicer
buffer, or a scalar with a byte string consisting of sliced data.
*n_lines* gives the number of valid lines in the sliced data buffer
and should be exactly the value returned by the slicer function.
*timestamp* specifies the capture instant of the input data in seconds
and fractions since 1970-01-01 00:00 in double format. The timestamps
are expected to advance by 1/30 to 1/25 seconds for each call to this
function. Different steps will be interpreted as dropped frames, which
starts a resynchronization cycle, eventually a channel switch may be assumed
which resets even more decoder state. So this function must be called even
if a frame did not contain any useful data (with parameter *n_lines* = 0)

=item vt->channel_switched( [nuid] )

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

=item (type, subno, lang) = vt->classify_page(pgno)

This function queries information about the named page. The return value
is a list consisting of three scalars.  Their content depends on the
data service to which the given page belongs:

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

=item vt->set_brightness(brightness)

Change brightness of text pages, this affects the color palette of pages
fetched with *vt->fetch_vt_page()* and *vt->fetch_cc_page()*.
Parameter *brightness* is in range 0 ... 255, where 0 is darkest,
255 brightest. Brightness value 128 is default.

=item vt->set_contrast(contrast)

Change contrast of text pages, this affects the color palette of pages
fetched with *vt->fetch_vt_page()* and *vt->fetch_cc_page()*.
Parameter *contrast* is in range -128 to 127, where -128 is inverse,
127 maximum. Contrast value 64 is default.

=item vt->teletext_set_default_region(default_region)

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

=item pg = vt->fetch_vt_page(pgno, subno [, max_level, display_rows, navigation])

Fetches a Teletext page designated by parameters *pgno* and *subno*
from the cache, formats and returns it as a blessed reference to a
page object of type *Zvbi.page*.  The reference can
then be passed to the various libzvbi methods working on page objects,
such as the export functions.

The function returns `undef` if the page is not cached or could not
be formatted for other reasons, for instance is a data page not intended
for display. Level 2.5/3.5 pages which could not be formatted e. g.
due to referencing data pages not in cache are formatted at a
lower level.

Further input parameters: If *subno* is `VBI_ANY_SUBNO` then the
newest sub-page of the given page is returned.
*max_level* is one of the `VBI_WST_LEVEL_*` constants and specifies
the Teletext implementation level to use for formatting.
*display_rows* limits rendering to the given number of rows
(i.e. row 0 ... *display_rows* - 1)  In practice, useful
values are 1 (format the page header row only) or 25 (complete page).
Boolean parameter *navigation* can be used to skip parsing the page
for navigation links to save formatting time.  The last three parameters
are optional and default to `VBI_WST_LEVEL_3p5`, 25 and 1 respectively.

Although safe to do, this function is not supposed to be called from
an event handler since rendering may block decoding for extended
periods of time.

The returned reference must be destroyed to release resources which are
locked internally in the library during the fetch.  The destruction is
done automatically when a local variable falls out of scope, or it can
be forced by use of Perl's *undef* operator.

=item pg = vt->fetch_cc_page(pgno, reset)

Fetches a Closed Caption page designated by *pgno* from the cache,
formats and returns it and  as a blessed reference to a page object
of type *Zvbi.page*.
Returns `undef` upon errors.

Closed Caption pages are transmitted basically in two modes: at once
and character by character ("roll-up" mode).  Either way you get a
snapshot of the page as it should appear on screen at the present time.
With *vt->event_handler_register()* you can request a `VBI_EVENT_CAPTION`
event to be notified about pending changes (in case of "roll-up" mode
that is with each new word received) and the vbi_page->dirty fields
will mark the lines actually in need of updates, to speed up rendering.

If the *reset* parameter is set to 1, the page dirty flags in the
cached paged are reset after fetching. Pass 0 only if you plan to call
this function again to update other displays. If omitted, the parameter
defaults to 1.

Although safe to do, this function is not supposed to be
called from an event handler, since rendering may block decoding
for extended periods of time.

=item yes_no = vt->is_cached(pgno, subno)

This function queries if the page specified by parameters
*pgno* and *subno* is currently available in the cache.
The result is 1 if yes, else 0.

This function is deprecated for reasons of forwards compatibility:
At the moment pages can only be added to the cache but not removed
unless the decoder is reset. That will change, making the result
volatile in a multi-threaded environment.

=item subno = vt->cache_hi_subno(pgno)

This function queries the highest cached sub-page of the page
specified by parameter *pgno*.

This function is deprecated for the same reason as *vt->is_cached()*

=item title = vt->page_title(pgno, subno)

The function makes an effort to deduce a page title to be used in
bookmarks or similar purposes for the page specified by parameters
*pgno* and *subno*.  The title is mainly derived from navigation data
on the given page.  The function returns the title or `undef` upon error.

=back

Typically the transmission of VBI data elements like a Teletext or Closed Caption
page spans several VBI lines or even video frames. So internally the data
service decoder maintains caches accumulating data. When a page or other
object is complete it calls the respective event handler to notify the
application.

Clients can register any number of handlers needed, also different handlers
for the same event. They will be called by the *vt->decode()* function in
the order in which they were registered.  Since decoding is stopped while in
the callback, the handlers should return as soon as possible.

The handler function receives two parameters: First is the event type
(i.e. one of the `VBI_EVENT_*` constants), second a hash reference
describing the event.  See libzvbi for a definition of contents.

=over 4

=item vt->event_handler_register(event_mask, handler [, user_data])

Registers a new event handler. *event_mask* can be any 'or' of `VBI_EVENT_*`
constants, -1 for all events and 0 for none. When the *handler* function with
*user_data* is already registered, its event_mask will be changed. Any
number of handlers can be registered, also different handlers for the same
event which will be called in registration order.

Apart of adding handlers this function also enables and disables decoding
of data services depending on the presence of at least one handler for the
respective data. A `VBI_EVENT_TTX_PAGE` handler for example enables
Teletext decoding.

This function can be safely called at any time, even from inside of a handler.
Note only 10 event callback functions can be registered in a script at the
same time.  Callbacks are automatically unregistered when the decoder object
is destroyed.

=item vt->event_handler_unregister(handler [, user_data])

Unregisters the event handler *handler* with parameter *user_data*,
if such a handler was previously registered.

Apart of removing a handler this function also disables decoding
of data services when no handler is registered to consume the
respective data. Removing the last `VBI_EVENT_TTX_PAGE` handler for
example disables Teletext decoding.

This function can be safely called at any time, even from inside of a
handler removing itself or another handler, and regardless if the handler
has been successfully registered.

=item vt->event_handler_add(event_mask, handler [, user_data])

**Deprecated:** Installs *handler* as event callback for the given
events.  When using this function you can install only a single event
handler per decoder (note this is a stronger limitation than the one
in libzvbi for this function.) For this reason the function is
deprecated; use *event_handler_register()* in new code.
The function returns boolean FALSE on failure, else TRUE.

Parameters: *event_mask* is one of the `VBI_EVENT*` constants and
specifies the events the handler is waiting for.
*handler* is a reference to a handler function.
The optional *user_data* is stored internally and passed through
in calls to the event handler function.

=item vt->event_handler_remove(handler)

**Deprecated:**
This function removes an event handler function (if any) which was
previously installed via *vt->event_handler_add()*.
Parameter *handler* is a reference to the event handler which is
to be removed (currently ignored as only one handler can be installed.)

Use *event_handler_register()* and *event_handler_unregister()*
in new code instead.

=back

The following event types are defined:

=over 8

=item VBI_EVENT_NONE

No event.

=item VBI_EVENT_CLOSE

The vbi decoding context is about to be closed. This event is
sent when the decoder object is destroyed and can be used to
clean up event handlers.

=item VBI_EVENT_TTX_PAGE

The vbi decoder received and cached another Teletext page
designated by *ev->{pgno}* and *ev->{subno}*.

*ev->{roll_header}* flags the page header as suitable for
rolling page numbers, e. g. excluding pages transmitted out
of order.

The *ev->{header_update}* flag is set when the header,
excluding the page number and real time clock, changed since the
last `VBI_EVENT_TTX_PAGE`. Note this may happen at midnight when the
date string changes. The *ev->{clock_update}* flag is set when
the real time clock changed since the last `VBI_EVENT_TTX_PAGE`
(that is at most once per second). They are both set at the first
`VBI_EVENT_TTX_PAGE` sent and unset while the received header
or clock field is corrupted.

If any of the roll_header, header_update or clock_update flags
are set *ev->{raw_header}* is a pointer to the raw header data
(40 bytes), which remains valid until the event handler returns.
*ev->{pn_offset}* will be the offset (0 ... 37) of the three
digit page number in the raw or formatted header. Always call
*vt->fetch_vt_page()* for proper translation of national characters
and character attributes, the raw header is only provided here
as a means to quickly detect changes.

=item VBI_EVENT_CAPTION

A Closed Caption page has changed and needs visual update.
The page or "CC channel" is designated by *ev->{pgno}*.

When the client is monitoring this page, the expected action is
to call *vt->fetch_cc_page()*. To speed up rendering, more detailed
update information can be queried via *pg->get_page_dirty_range()*.
(Note the vbi_page will be a snapshot of the status at fetch time
and not event time, i.e. the "dirty" flags accumulate all changes
since the last fetch.)

=item VBI_EVENT_NETWORK

Some station/network identifier has been received or is no longer
transmitted (in the latter case all values are zero, e.g. after a
channel switch).  The event will not repeat until a different identifier
has been received and confirmed.  (Note: VPS/TTX and XDS will not combine
in real life, feeding the decoder with artificial data can confuse
the logic.)

The referenced hash contains the following elements:
nuid,
name,
call,
tape_delay,
cni_vps,
cni_8301,
cni_8302,
cycle.

Minimum time to identify network, when data service is transmitted:
VPS (DE/AT/CH only): 0.08 seconds; Teletext PDC or 8/30: 2 seconds;
XDS (US only): unknown, between 0.1x to 10x seconds.

=item VBI_EVENT_TRIGGER

Triggers are sent by broadcasters to start some action on the
user interface of modern TVs. Until libzvbi implements all of
WebTV and SuperTeletext the information available are program
related (or unrelated) URLs, short messages and Teletext
page links.

This event is sent when a trigger has fired.
The hash parameter contains the following elements:
type,
eacem,
name,
url,
script,
nuid,
pgno,
subno,
expires,
itv_type,
priority,
autoload.

=item VBI_EVENT_ASPECT

The vbi decoder received new information (potentially from
PAL WSS, NTSC XDS or EIA-J CPR-1204) about the program
aspect ratio.

The hash parameter contains the following elements:
first_line,
last_line,
ratio,
film_mode,
open_subtitles.

=item VBI_EVENT_PROG_INFO

We have new information about the current or next program.
(Note this event is preliminary as info from Teletext is not implemented yet.)

The referenced has contains the program description including
many parameters. See libzvbi documentation for details.

=item VBI_EVENT_NETWORK_ID

Like `VBI_EVENT_NETWORK`, but this event will also be sent
when the decoder cannot determine a network name.

**Available:** since libzvbi version 0.2.20

=back

Class Zvbi.page
===============

These are functions to render Teletext and Closed Caption pages directly
into memory, essentially a more direct interface to the functions of some
important export modules described in *Zvbi.export*.

All of the functions in this section work on page objects as returned
by the page cache's "fetch" functions (see *Zvbi.vt*)
or the page search function (see *Zvbi.search*)

=over 4

=item canvas = pg->draw_vt_page(fmt=VBI_PIXFMT_RGBA32_LE, reveal=0, flash_on=0)

Draw a complete Teletext page. Each teletext character occupies
12 x 10 pixels (i.e. a character is 12 pixels wide and each line
is 10 pixels high. Note that this aspect ratio is not optimal
for display, so pixel lines should be doubled. This is done
automatically by the XPM conversion functions.)

The image is returned in a scalar which contains a byte string.  When
using format `VBI_PIXFMT_RGBA32_LE`, each pixel consists of 4 subsequent
bytes in the string (RGBA). Hence the string is
`4 * 12 * pg_columns * 10 * pg_rows` bytes long, where
`pg_columns` and `pg_rows` are the page width and height in
teletext characters respectively.  When using format `VBI_PIXFMT_PAL8`
(only available with libzvbi version 0.2.26 or later) each pixel uses
one byte. In this case each pixel value is an index into the color
palette as delivered by *pg->get_page_color_map()*.

Note this function is just a convenience interface to
*pg->draw_vt_page_region()* which automatically inserts the
page column, row, width and height parameters by querying page dimensions.
The image width is set to the full page width (i.e. same as when passing
value -1 for *img_pix_width*)

See the following function for descriptions of the remaining parameters.

=item pg->draw_vt_page_region(fmt, canvas, img_pix_width, col_pix_off, row_pix_off, column, row, width, height, reveal=0, flash_on=0)

Draw a sub-section of a Teletext page. Each character occupies 12 x 10 pixels
(i.e. a character is 12 pixels wide and each line is 10 pixels high.)

The image is written into *canvas*. If the scalar is undefined or not
large enough to hold the output image, the canvas is initialized as black.
Else it's left as is. This allows to call the draw functions multiple times
to assemble an image. In this case *img_pix_width* must have the same
value in all rendering calls. See also *pg->draw_blank()*.

The image is returned in a scalar which contains a byte string.  With
format `VBI_PIXFMT_RGBA32_LE` each pixel uses 4 subsequent bytes in the
string (RGBA). With format `VBI_PIXFMT_PAL8` (only available with libzvbi
version 0.2.26 or later) each pixel uses one byte (reference into the
color palette.)

Input parameters:
*fmt* is the target format. Currently only `VBI_PIXFMT_RGBA32_LE`
is supported (i.e. each pixel uses 4 subsequent bytes for R,G,B,A.)
*canvas* is a scalar into which the image is written.

*img_pix_width* is the distance between canvas pixel lines in pixels.
When set to -1, the image width is automatically set to the width of
the selected region (i.e. `pg_columns * 12` bytes.)

*col_pix_off* and *row_pix_off* are offsets to the upper left
corner in pixels and define where in the canvas to draw the page
section.

*column* is the first source column (range 0 ... pg->columns - 1);
*row* is the first source row (range 0 ... pg->rows - 1);
*width* is the number of columns to draw, 1 ... pg->columns;
*height* is the number of rows to draw, 1 ... pg->rows;
Note all four values are given as numbers of teletext characters (not pixels.)

Example to draw two pages stacked into one canvas:

  my fmt = Zvbi.VBI_PIXFMT_RGBA32_LE;
  my canvas = pg->draw_blank(fmt, 10 * 25 * 2);
  pg_1->draw_vt_page_region(fmt, canvas,
                             -1, 0, 0, 0, 0, 40, 25);
  pg_2->draw_vt_page_region(fmt, canvas,
                             -1, 0, 10 * 25, 0, 0, 40, 25);

Optional parameter *reveal* can be set to 1 to draw characters flagged
as "concealed" as space (U+0020).
Optional parameter *flash_on* can be set to 1 to draw characters flagged
"blink" (see vbi_char) as space (U+0020). To implement blinking you'll have
to draw the page repeatedly with this parameter alternating between 0 and 1.

=item canvas = pg->draw_cc_page(fmt=VBI_PIXFMT_RGBA32_LE)

Draw a complete Closed Caption page. Each character occupies
16 x 26 pixels (i.e. a character is 16 pixels wide and each line
is 26 pixels high.)

The image is returned in a scalar which contains a byte string.  Each
pixel uses 4 subsequent bytes in the string (RGBA). Hence the string
is `4 * 16 * pg_columns * 26 * pg_rows` bytes long, where
`pg_columns` and `pg_rows` are the page width and height in
Closed Caption characters respectively.

Note this function is just a convenience interface to
*pg->draw_cc_page_region()* which automatically inserts the
page column, row, width and height parameters by querying page dimensions.
The image width is set to the page width (i.e. same as when passing
value -1 for *img_pix_width*)

=item pg->draw_cc_page_region(fmt, canvas, img_pix_width, column, row, width, height)

Draw a sub-section of a Closed Caption page. Please refer to
*pg->draw_cc_page()* and
*pg->draw_vt_page_region()* for details on parameters
and the format of the returned byte string.

=item canvas = pg->draw_blank(fmt, pix_height, img_pix_width)

This function can be used to create a blank canvas onto which several
Teletext or Closed Caption regions can be drawn later.

All input parameters are optional:
*fmt* is the target format. Currently only `VBI_PIXFMT_RGBA32_LE`
is supported (i.e. each pixel uses 4 subsequent bytes for R,G,B,A.)
*img_pix_width* is the distance between canvas pixel lines in pixels.
*pix_height* is the height of the canvas in pixels (note each
Teletext line has 10 pixels and each Closed Caption line 26 pixels
when using the above drawing functions.)
When omitted, the previous two parameters are derived from the
referenced page object.

=item xpm = pg->canvas_to_xpm(canvas [, fmt, aspect, img_pix_width])

This is a helper function which converts the image given in
*canvas* from a raw byte string into XPM format. Due to the way
XPM is specified, the output is a regular text string. (The result
is suitable as input to **Tk::Pixmap** but can also be written into
a file for passing the image to external applications.)

Optional boolean parameter *aspect* when set to 0, disables the
aspect ration correction (i.e. on teletext pages all lines are
doubled by default; closed caption output ration is already correct.)
Optional parameter *img_pix_width* if present, must have the same
value as used when drawing the image. If this parameter is omitted
or set to -1, the referenced page's full width is assumed (which is
suitable for converting images generated by *draw_vt_page()* or
*draw_cc_page()*.)

Note: Since libzvbi 0.2.26, you can also obtain XPM snapshots
via the *Zvbi.export* class.

=item txt = pg->print_page(table=0, rtl=0)

Print and return the referenced Teletext or Closed Caption page
in text form, with rows separated by line-feed characters ("\n").
All character attributes and colors will be lost. Graphics
characters, DRCS and all characters not representable in UTF-8
will be replaced by spaces.

When optional parameter *table* is set to 1, the page is scanned
in table mode, printing all characters within the source rectangle
including runs of spaces at the start and end of rows. Else,
sequences of spaces at the start and end of rows are collapsed
into single spaces and blank lines are suppressed.
Optional parameter *rtl* is currently ignored; defaults to 0.

=item txt = pg->print_page_region(table, rtl, column, row, width, height)

Print and return a sub-section of the referenced Teletext or
Closed Caption page in text form, with rows separated
by line-feed characters ("\n").
All character attributes and colors will be lost. Graphics
characters, DRCS and all characters not representable in UTF-8
will be replaced by spaces.

*table* Scan page in table mode, printing all characters within the
source rectangle including runs of spaces at the start and end of rows.
When 0, scan all characters from position `{column, row}` to
`{column + width - 1, row + height - 1}`
and all intermediate rows to the page's full columns width.
In this mode runs of spaces at the start and end of rows are collapsed
into single spaces, blank lines are suppressed.
Parameter *rtl* is currently ignored and should be set to 0.

The next four parameters specify the page region:
*column*: first source column;
*row*: first source row;
*width*: number of columns to print;
*height*: number of rows to print.
You can use *pg->get_page_size()* below to determine the allowed
ranges for these values, or use *pg->print_page()* to print the
complete page.

=item (pgno, subno) = pg->get_page_no()

This function returns a list of two scalars which contain the
page and sub-page number of the referenced page object.

Teletext page numbers are hexadecimal numbers in the range 0x100 .. 0x8FF,
Closed Caption page numbers are in the range 1 .. 8.  Sub-page numbers
are used for teletext only. These are hexadecimal numbers in range
0x0001 .. 0x3F7F, i.e. the 2nd and 4th digit count from 0..F, the
1st and 3rd only from 0..3 and 0..7 respectively. A sub-page number
zero means the page has no sub-pages.

=item (rows, columns) = pg->get_page_size()

This function returns a list of two scalars which contain the
dimensions (i.e. row and column count) of the referenced page object.

=item (y0, y1, roll) = pg->get_page_dirty_range()

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

=item av = pg->get_page_color_map()

The function returns a reference to an array with 40 entries which
contains the page color palette. Each array entry is a 24-bit RGB value
(i.e. three 8-bit values for red, green, blue, with red in the
lowest bits)  To convert this into the usual #RRGGBB syntax use:

  sprintf "#%02X%02X%02X",
          rgb&0xFF, (rgb>>8)&0xFF, (rgb>>16)&0xFF

=item av = pg->get_page_text_properties()

The function returns a reference to an array which contains
the properties of all characters on the given page. Each element
in the array is a bit-field. The members are (in ascending order,
width in bits given behind the colon):
foreground:8, background:8, opacity:4, size:4,
underline:1, bold:1, italic:1, flash:1, conceal:1, proportional:1, link:1.

=item txt = pg->get_page_text( [all_chars] )

The function returns the complete page text in form of an UTF-8
string.  This function is very similar to *pg->print_page()*,
but does not insert or remove any characters so that it's guaranteed
that characters in the returned string correlate exactly with the
array returned by *pg->get_page_text_properties()*.

Note since UTF-8 is a multi-byte encoding, the length of the string
in bytes may be different from the length in characters. Hence you
should access the variable with string manipulation functions only
(e.g. *substr()*)

When the optional parameter *all_chars* is set to 1, even
characters on the private Unicode code pages are included.
Otherwise these are replaced with blanks. Note use of these
characters will cause warnings when passing the string to
transcoder functions (such as Perl's *encode()* or *print*.)

=item href = pg->vbi_resolve_link(column, row)

The referenced page *pg* (in practice only Teletext pages) may contain
hyperlinks such as HTTP URLs, e-mail addresses or links to other
pages. Characters being part of a hyperlink have their "link" flag
set in the character properties (see *pg->get_page_text_properties()*),
this function returns a reference to a hash with a more verbose
description of the link.

The returned hash contains the following elements (depending on the
type of the link not all elements may be present):
type, eacem, name, url, script, nuid, pgno, subno,
expires, itv_type, priority, autoload.

=item href = pg->vbi_resolve_home()

All Teletext pages have a built-in home link, by default
page 100, but can also be the magazine intro page or another
page selected by the editor.  This function returns a hash
reference with the same elements as *pg->vbi_resolve_link()*.

=item pg->unref_page()

This function can be use to de-reference the given page (see also
*vt->fetch_vt_page()* and *vt->fetch_cc_page()*)
The call is equivalent to using Perl's *undef* operator
on the page reference (i.e. `undef pg;`)

Note use of this operator is deprecated in Perl.  It's recommended
to instead assign page references to local variables (i.e. declared
with `my`) so that the page is automatically destroyed when the
function or block which works on the reference is left.

=back

Class Zvbi.export
=================

Once libzvbi received, decoded and formatted a Teletext or Closed Caption
page you will want to render it on screen, print it as text or store it
in various formats.  libzvbi provides export modules converting a page
object into the desired format or rendering directly into an image.

=over 4

=item exp = Zvbi.export::new(keyword, errstr)

Creates a new export module object to export a VBI page object in
the respective module format. As a special service you can
initialize options by appending to the *keyword* parameter like this:
`keyword = "keyword; quality=75.5, comment=\"example text\"";`

Note: A quick overview of all export formats and options can be
ptained by running the demo script *examples/explist.pl* in the
ZVBI package.

=item href = Zvbi.export::info_enum(index)

Enumerates all available export modules. You should start with
*index* 0, incrementing until the function returns `undef`.
Some modules may depend on machine features or the presence of certain
libraries, thus the list can vary from session to session.

On success the function returns a reference to an hash with the
following elements: keyword, label, tooltip, mime_type, extension.

=item href = Zvbi.export::info_keyword(keyword)

Similar to the above function *info_enum()*, this function returns
info about available modules, although this one searches for an
export module which matches *keyword*. If no match is found the
function returns `undef`, else a hash reference as described
above.

=item href = exp->info_export()

Returns the export module info for the export object referenced
by *exp*.  On success a hash reference as described for the
previous two functions is returned.

=item href = exp->option_info_enum(index)

Enumerates the options available for the referenced export module.
You should start at *index* 0, incrementing until the function
returns `undef`.  On success, the function returns a reference
to a hash with the following elements:
type, keyword, label, min, max, step, def, menu, tooltip.

The content format of min, max, step and def depends on the type,
i.e. it may be an integer, double or string - but usually you don't
have to worry about that in Perl. If present, menu is an array
reference. Elements in the array are of the same type as min, max, etc.
If no label or tooltip are available for the option, these elements
are undefined.

=item href = exp->option_info_keyword(keyword)

Similar to the above function *exp->option_info_enum()* this
function returns info about available options, although this one
identifies options based on the given *keyword*.

=item exp->option_set(keyword, opt)

Sets the value of the option named by *keword* to *opt*.
Returns 0 on failure, 1 on success.  Example:

  exp->option_set('quality', 75.5);

Note the expected type of the option value depends on the keyword.
The ZVBI interface module automatically converts the option into
type expected by the libzvbi library.

Mind that options of type `VBI_OPTION_MENU` must be set by menu
entry number (integer), all other options by value. If necessary
it will be replaced by the closest value possible. Use function
*exp->option_menu_set()* to set options with menu by menu entry.

=item opt = exp->option_get(keyword)

This function queries and returns the current value of the option
named by *keyword*.  Returns `undef` upon error.

=item exp->option_menu_set(keyword, entry)

Similar to *exp->option_set()* this function sets the value of
the option named by *keyword* to *entry*, however it does so
by number of the corresponding menu entry. Naturally this must
be an option with menu.

=item entry = exp->option_menu_get(keyword)

Similar to *exp->option_get()* this function queries the current
value of the option named by *keyword*, but returns this value as
number of the corresponding menu entry. Naturally this must be an
option with menu.

=item exp->stdio(io, pg)

This function writes contents of the page given in *pg*, converted
to the respective export module format, to the stream *io*.
The caller is responsible for opening and closing the stream,
don't forget to check for I/O errors after closing. Note this
function may write incomplete files when an error occurs.
The function returns 1 on success, else 0.

You can call this function as many times as you want, it does not
change state of the export or page objects.

=item exp->file(name, pg)

This function writes contents of the page given in *pg*, converted
to the respective export module format, into a new file specified
by *name*. When an error occurs the file will be deleted.
The function returns 1 on success, else 0.

You can call this function as many times as you want, it does not
change state of the export or page objects.

=item data = exp->alloc(pg)

This functions renders the page *pg* and returns it as a (byte-)string.
Returns undef if the function fails.

**Available:** since libzvbi version 0.2.26

=item size = exp->mem(data, pg)

This functions renders the page *pg* into scalar *data*. The size
of the scalar must be large enough to hold all of the data. The result
is -1 upon internal errors, or the size of the output. You must check
if the size is larger then the length of *data*:

  my sz = ex->mem(img, page);
  die "Export failed: ". exp->errstr() ."\n" if s < 0;
  die "Export failed: Buffer too small.\n" if sz > length(img);

Usually you should get the same performance from the *exp->alloc()*
variant, which has much simpler semantics.  Note you can also use
*exp->alloc()* to determine the size.  For image formats without
compression the output size will usually be the same for all pages
with the same dimensions.

**Available:** since libzvbi version 0.2.26

=item text = exp->errstr()

When an export function failed, this function returns a string
with a more detailed error description.

=back

Class Zvbi.search
=================

The functions in this section allow to search across one or more
Teletext pages in the cache for a given sub-string or a regular
expression.

=over 4

=item search = Zvbi.new(vt, pgno, subno, pattern, casefold=0, regexp=0, progress=NULL, user_data=NULL)

Create a search context and prepare for searching the Teletext page
cache with the given expression.  Regular expression searching supports
the standard set of operators and constants, with these extensions:

Input Parameters:
*pgno* and *subno* specify the number of the first (forward) or
last (backward) page to visit. Optionally `VBI_ANY_SUBNO` can be used
for *subno*.
*pattern* contains the search pattern (encoded in UTF-8, but usually
you won't have to worry about that when using Perl; use Perl's **Encode**
module to search for characters which are not supported in your current locale.)
Boolean *casefold* can be set to 1 to make the search case insensitive;
default is 0.
Boolean *regexp* must be set to 1 when the search pattern is a regular
expression; default is 0.

If present, *progress* can be used to pass a reference to a function
which will be called for each scanned page. When the function returns 0,
the search is aborted.  The callback function receives as only parameter
a reference to the search page.  Use *pg->get_page_no()* to query
the page number for a progress display.  Note due to internal limitations
only 10 search callback functions can be registered in a script at the
same time.  Callbacks are automatically unregistered when the search
object is destroyed.

**Note:** The referenced page is only valid while inside of the
callback function (i.e. you must not assign the reference to a
variable outside of the scope of the handler function.)

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

Note:
In a multi-threaded application the data service decoder may receive
and cache new pages during a search session. When these page numbers
have been visited already the pages are not searched. At a channel
switch (and in future at any time) pages can be removed from cache.
All this has yet to be addressed.

=item status = search->next(pgref, dir)

The function starts the search on a previously created search
context.  Parameter *dir* specifies the direction:
1 for forwards, or -1 for backwards search.

The function returns a status code which is one of the following
constants:

=over 8

=item VBI_SEARCH_ERROR

Pattern not found, pg is invalid. Another vbi_search_next()
will restart from the original starting point.

=item VBI_SEARCH_CACHE_EMPTY

No pages in the cache, pg is invalid.

=item VBI_SEARCH_CANCELED

The search has been canceled by the progress function.
*pg* points to the current page as in success case, except for
the highlighting. Another *search->next()* continues from
this page.

=item VBI_SEARCH_NOT_FOUND

Some error occurred, condition unclear.

=item VBI_SEARCH_SUCCESS

Pattern found. *pgref* points to the page ready for display
with the pattern highlighted.

=back

If and only if the function returns `VBI_SEARCH_SUCCESS`, *pgref* is
set to a reference to the matching page.

=back

Miscellaneous (Zvbi)
====================

=over 4

=item lib_version()

Returns the version of the ZVBI library.

=item set_log_fn(mask [, log_fn [, user_data ]] )

Various functions can print warnings, errors and information useful to
debug the library. With this function you can enable these messages
and determine a function to print them. (Note: The kind and contents
of messages logged by particular functions may change in the future.)

Parameters:
*mask* specifies which kind of information to log.
It's a bit-wise OR of zero or more of the constants
`VBI_LOG_ERROR`,
`VBI_LOG_WARNING`,
`VBI_LOG_NOTICE`,
`VBI_LOG_INFO`,
`VBI_LOG_DEBUG`,
`VBI_LOG_DRIVER`,
`VBI_LOG_DEBUG2`,
`VBI_LOG_DEBUG3`.
*log_fn* is a reference to a function to be called with log
messages. Omit this parameter to disable logging.

The log handler is called with the following parameters: *level*
is one of the `VBI_LOG_*` constants; *context* which is a text
string describing the module where the event occurred; *message*
the actual error message; finally, if passed during callback
definition, a *user_data* parameter.

Note only 10 event log functions can be registered in a script
at the same time.

**Available:** since libzvbi version 0.2.22

=item set_log_on_stderr(mask)

This function enables error logging just like *set_log_fn()*,
but uses the library's internal log function which prints
all messages to *stderr*, i.e. on the terminal.
*mask* is a bit-wise OR of zero or more of the `VBI_LOG_*`
constants. The mask specifies which kind of information to log.

To disable logging call `set_log_fn(0)`, i.e. without passing
a callback function reference.

**Available:** since libzvbi version 0.2.22

=item par8(val)

This function encodes the given 7-bit value with Parity. The
result is an 8-bit value in the range 0..255.

=item unpar8(val)

This function decodes the given Parity encoded 8-bit value. The result
is a 7-bit value in the range 0...127 or a negative value when a
parity error is detected.  (Note: to decode parity while ignoring
errors, simply mask out the highest bit, i.e. val &= 0x7F)

=item par_str(data)

This function encodes a string with parity in place, i.e. the given
string contains the result after the call.

=item unpar_str(data)

This function decodes a Parity encoded string in place, i.e. the
parity bit is removed from all characters in the given string.
The result is negative when a decoding error is detected, else
the result is positive or zero.

=item rev8(val)

This function reverses the order of all bits of the given 8-bit value
and returns the result. This conversion is required for decoding certain
teletext elements which are transmitted MSB first instead of the usual
LSB first (the teletext VBI slicer already inverts the bit order so that
LSB are in bit #0)

=item rev16(val)

This function reverses the order of all bits of the given 16-bit value
and returns the result.

=item rev16p(data, offset=0)

This function reverses 2 bytes from the string representation of the
given scalar at the given offset and returns them as a numerical value.

=item ham8(val)

This function encodes the given 4-bit value (i.e. range 0..15) with
Hamming-8/4.  The result is an 8-bit value in the range 0..255.

=item unham8(val)

This function decodes the given Hamming-8/4 encoded value. The result
is a 4-bit value, or -1 when there are uncorrectable errors.

=item unham16p(data, offset=0)

This function decodes 2 Hamming-8/4 encoded bytes (taken from the string
in parameter "data" at the given offset) The result is an 8-bit value,
or -1 when there are uncorrectable errors.

=item unham24p(data, offset=0)

This function decodes 3 Hamming-24/18 encoded bytes (taken from the string
in parameter "data" at the given offset) The result is an 8-bit value,
or -1 when there are uncorrectable errors.

=item dec2bcd(dec)

Converts a two's complement binary in range 0 ... 999 into a
packed BCD number (binary coded decimal) in range  0x000 ... 0x999.
Extra digits in the input are discarded.

=item dec = bcd2dec(bcd)

Converts a packed BCD number in range 0x000 ... 0xFFF into a two's
complement binary in range 0 ... 999. Extra digits in the input
will be discarded.

=item add_bcd(bcd1, bcd2)

Adds two packed BCD numbers, returning a packed BCD sum. Arguments
and result are in range 0xF0000000 ... 0x09999999, that
is -10**7 ... +10**7 - 1 in decimal notation. To subtract you can
add the 10's complement, e. g. -1 = 0xF9999999.

The return value is a packed BCD number. The result is undefined when
any of the arguments contain hex digits 0xA ... 0xF.

=item is_bcd(bcd)

Tests if *bcd* forms a valid BCD number. The argument must be
in range 0x00000000 ... 0x09999999. Return value is 0 if *bcd*
contains hex digits 0xA ... 0xF.

=item vbi_decode_vps_cni(data)

This function receives a sliced VPS line and returns a 16-bit CNI value,
or undef in case of errors.

**Available:** since libzvbi version 0.2.22

=item vbi_encode_vps_cni(cni)

This function receives a 16-bit CNI value and returns a VPS line,
or undef in case of errors.

**Available:** since libzvbi version 0.2.22

=item rating_string(auth, id)

Translate a program rating code given by *auth* and *id* into a
Latin-1 string, native language.  Returns `undef` if this code is
undefined. The input parameters will usually originate from
*ev->{rating_auth}* and *ev->{rating_id}* in an event struct
passed for a data service decoder event of type `VBI_EVENT_PROG_INFO`.

=item prog_type_string(classf, id)

Translate a vbi_program_info program type code into a Latin-1 string,
currently English only.  Returns `undef` if this code is undefined.
The input parameters will usually originate from *ev->{type_classf}*
and array members *ev->{type_id}* in an event struct
passed for a data service decoder event of type `VBI_EVENT_PROG_INFO`.

=item str = iconv_caption(src [, repl_char] )

Converts a string of EIA 608 Closed Caption characters to UTF-8.
The function ignores parity bits and the bytes 0x00 ... 0x1F,
except for two-byte special and extended characters (e.g. music
note 0x11 0x37)  See also *caption_unicode()*.

Returns the converted string *src*, or `undef` when the source
buffer contains invalid two byte characters, or when the conversion
fails, when it runs out of memory.

Optional parameter *repl_char* when present specifies an UCS-2
replacement for characters which are not representable in UTF-8
(i.e. a 16-bit value - use Perl's *ord()* to obtain a character's
code value.) When omitted or zero, the function will fail if the
source buffer contains unrepresentable characters.

**Available:** since libzvbi version 0.2.23

=item str = caption_unicode(c [, to_upper] )

Converts a single Closed Caption character code into an UTF-8 string.
Codes in range 0x1130 to 0x1B3F are special and extended characters
(e.g. caption command 11 37).

Input character codes in *c* are in range

  0x0020 ... 0x007F,
  0x1130 ... 0x113F, 0x1930 ... 0x193F, 0x1220 ... 0x123F,
  0x1A20 ... 0x1A3F, 0x1320 ... 0x133F, 0x1B20 ... 0x1B3F.

When the optional *to_upper* is set to 1, the character is converted
into upper case. (Often programs are captioned in all upper case, but
except for one character the basic and special CC character sets contain
only lower case accented characters.)

**Available:** since libzvbi version 0.2.23

=back

Examples
========

The `examples` sub-directory in the **Zvbi** package
contains a number of scripts used to test the various interface
functions. You can also use them as examples for your code:

:capture.pl:
    This is a translation of `test/capture.c` in the libzvbi package.
    The script captures sliced VBI data from a device.  Output can be
    written to a file or passed via stdout into one of the following
    example scripts.  Call with option `--help` for a list of options.

:decode.pl:
    This is a direct translation of `test/decode.c` in the libzvbi package.
    Decodes sliced VBI data on stdin, e. g. ::

      ./capture --sliced | ./decode --ttx

    Call with option `--help` for a list of options.

:caption.pl:
    This is a translation of `test/caption.c` in the libzvbi package,
    albeit based on TkInter here.
    When called without an input stream, the application displays some
    sample messages (character sets etc.) for debugging the decoder.
    When the input stream is the output of `capture.pl --sliced`
    (see above), the applications displays the live CC stream received
    from a VBI device.  The buttons on top switch between Closed Caption
    channels 1-4 and Text channels 1-4.

:export.pl:
    This is a direct translation of `test/export.c` in the libzvbi package.
    The script captures from `/dev/vbi0` until the page specified on the
    command line is found and then exports the page in a requested format.

:explist.pl:
    This is a direct translation of `test/explist.c` in the libzvbi package.
    Test of page export options and menu interfaces.  The script lists
    all available export modules (i.e. formats) and options.

:hamm.pl:
    This is a direct translation of `test/hamm.c` in the libzvbi package.
    Automated test of the odd parity and Hamming encoder and decoder functions.

:network.pl:
    This is a direct translation of `examples/network.c` in the libzvbi package.
    The script captures from `/dev/vbi0` until the currently tuned channel is
    identified by means of VPS, PDC et.al.

:proxy-test.pl:
    This is a direct translation of `test/proxy-test.c` in the libzvbi package.
    The script can capture either from a proxy daemon or a local device and
    dumps captured data on the terminal. Also allows changing services and
    channels during capturing (e.g. by entering "+ttx" or "-ttx" on stdin.)
    Start with option `-help` for a list of supported command line options.

:test-vps.pl:
    This is a direct translation of `test/test-vps.c` in the libzvbi package.
    It contains tests for encoding and decoding the VPS data service on
    randomly generated data.

:search-ttx.pl:
    The script is used to test search on teletext pages. The script
    captures from `/dev/vbi0` until the RETURN key is pressed, then prompts
    for a search string.  The content of matching pages is printed on
    the terminal and capturing continues until a new search text is
    entered.

:browse-ttx.pl:
    The script captures from `/dev/vbi0` and displays teletext pages in
    a small GUI using TkInter.

:osc.pl:
    This script is loosely based on `test/osc.c` in the libzvbi package.
    The script captures raw VBI data from a device and displays the data as
    an animated gray-scale image. One selected line is plotted and the decoded
    teletext or VPS Data of that line is shown.

:dvb-mux.pl:
    This script is a small example for use of the DVD multiplexer functions
    (available since libzvbi 0.2.26)  The scripts captures teletext from an
    analog VBI device and generates a PES or TS stream on STDOUT.

Authors
=======

The ZVBI Perl interface module was written by T. Zoerner <tomzo@sourceforge.net>
starting March 2006 for the Teletext EPG grabber accompanying nxtvepg
http://nxtvepg.sourceforge.net/

The module is based on the libzvbi library, mainly written and maintained
by Michael H. Schimek (2000-2007) and &ntilde;aki Garc&iacute;a Etxebarria (2000-2001),
which in turn is based on AleVT 1.5.1 by Edgar Toernig (1998-1999).
See also http://zapping.sourceforge.net/

License
=======

Copyright (C) 2006-2020 T. Zoerner.

Parts of the descriptions in this man page are copied from the
"libzvbi" documentation, licensed under the GNU General Public
License version 2 or later,

* Copyright (C) 2000-2007 Michael H. Schimek,
* Copyright (C) 2000-2001 &ntilde;aki Garc&acute;a Etxebarria,
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
