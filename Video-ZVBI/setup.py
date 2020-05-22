# ----------------------------------------------------------------------------
# Copyright (C) 2007-2009,-2020 T. Zoerner
# ----------------------------------------------------------------------------
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation (see http://www.opensource.org/licenses/)
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# ----------------------------------------------------------------------------

from setuptools import setup, Extension
from distutils.command.install import install

import os
import os.path
import sys
import platform
import shutil
import re

if sys.version_info[0] != 3:
    print("This script requires Python 3")
    exit()

# ----------------------------------------------------------------------------

class MyClean(install):
    cwd = os.path.abspath(os.path.dirname(__file__))
    def rmfile(self, apath):
        p = os.path.join(MyClean.cwd, apath)
        if os.path.isfile(p):
            os.remove(p)
    def rmtree(self, apath):
        p = os.path.join(MyClean.cwd, apath)
        if os.path.isdir(p):
            shutil.rmtree(p)
    def run(self):
        # files created by build stage
        self.rmtree('build')
        # files created by test stage
        self.rmtree('Zvbi.egg-info')
        self.rmtree('__pycache__')
        for name in os.listdir(MyClean.cwd):
            if re.match(r"^.*\.so$", name):
                os.remove(os.path.join(MyClean.cwd, name))
        self.rmfile('core')
        # files created by sdist stage
        self.rmtree('dist')

# ----------------------------------------------------------------------------

this_directory = os.path.abspath(os.path.dirname(__file__))

# platform-specific build parameters
extrasrc = []       # extra source modules to compile
extrainc = []       # include path
extradef = []       # extra copile-switches
extralibdirs = []   # extra library search path
extralibs = []      # extra libraries to link against

# BSD requires listing libraries that libzvbi depends on
if re.match(r'bsd$', platform.system(), flags=re.IGNORECASE):
    extralibs += ['pthread', 'png', 'z']

# ----------------------------------------------------------------------------

# Assemble main README from doc/README.rsr plus all but the header of API doc
with open(os.path.join(this_directory, 'doc/README.rst'), encoding='utf-8') as fh:
    long_description = fh.read()
with open(os.path.join(this_directory, 'doc/ZVBI.rst'), encoding='utf-8') as fh:
    api_doc = fh.read()
    match = re.match(r"[\x00-\xff]*?(?=^SYNOPSIS$)", api_doc, re.MULTILINE)
    if match:
        long_description += "\n\n" + api_doc[match.end():]
    else:
        print("ERROR: Failed to find SYNOPSIS in ZVBI.rst", file=sys.stderr)

# Enable work-around for bugs in PyStructSequence_NewType (i.e. for
# creating named tuples in C module; causing crash in GC:
# "Fatal Python error: type_traverse() called for non-heap type")
# Known issue: https://bugs.python.org/issue28709 - fixed in 3.8
if (sys.version_info[0] == 3) and (sys.version_info[1] < 8):
    extradef += [('NAMED_TUPLE_GC_BUG', 1)]

# Definition of the "Zvbi" extension module
ext = Extension('Zvbi',
                sources       = ['src/zvbi.c',
                                 'src/zvbi_proxy.c',
                                 'src/zvbi_capture.c',
                                 'src/zvbi_capture_buf.c',
                                 'src/zvbi_raw_dec.c',
                                 'src/zvbi_raw_params.c',
                                 'src/zvbi_service_dec.c',
                                 'src/zvbi_page.c',
                                 'src/zvbi_event_types.c',
                                 'src/zvbi_callbacks.c',
                                 'src/zvbi_export.c',
                                 'src/zvbi_search.c',
                                 'src/zvbi_dvb_mux.c',
                                 'src/zvbi_dvb_demux.c',
                                 'src/zvbi_idl_demux.c',
                                 'src/zvbi_pfc_demux.c',
                                 'src/zvbi_xds_demux.c',
                                ] + extrasrc,
                include_dirs  = ['src'] + extrainc,
                define_macros = extradef,
                libraries     = ['zvbi'] + extralibs,
                library_dirs  = extralibdirs,
                #undef_macros  = ["NDEBUG"]   # for debug build only
               )

# Package definition
setup(name='Zvbi',
      version='0.1.0',
      description='Interface to the Zapping VBI decoder library',
      long_description=long_description,
      long_description_content_type="text/x-rst",
      author='T. Zoerner',
      author_email='tomzo@users.sourceforge.net',
      url='https://github.com/tomzox/Python-Video-ZVBI',
      license = "GNU GPLv2",
      classifiers=[
          'Development Status :: 3 - Alpha',
          "Programming Language :: C",
          "Programming Language :: Python :: 3",
          'Topic :: Multimedia :: Video :: Capture',
          'Intended Audience :: Developers',
          "Operating System :: POSIX :: Linux",
          "Operating System :: POSIX :: BSD",
          "License :: OSI Approved :: GNU General Public License v2 (GPLv2)"
         ],
      keywords="teletext, closed-caption, VPS, WSS, VBI, DVB, video, capture, decoder, libzvbi",
      platforms=['posix'],
      ext_modules=[ext],
      cmdclass={'clean': MyClean},
      python_requires='>=3.2',
     )
