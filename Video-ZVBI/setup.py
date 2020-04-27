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
        # files created by configuration stage
        self.rmfile('myconfig.h')
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

extrasrc = []
extrainc = []
extradef = []
extralibs = []
extralibdirs = []

#
# Perform a search for libzvbi and abort if it's missing
#
extralibs = ['zvbi']

if re.match(r'bsd$', platform.system(), flags=re.IGNORECASE):
    extralibs += ['pthread', 'png', 'z']

#X#if (($#ARGV >= 0) && ($ARGV[0] =~ /^LIBPATH=(.*)/)):
#X#    libpath = $1;
#X#    shift @ARGV;
#X#    extralibdirs += [libpath]
#X#    match = re.match(r'^(/.*)/lib$', libpath)
#X#    if match:
#X#        extrainc += [match.group(1) + "/include"]
#X#    if not libpath:
#X#        print("Cannot access %s: %s" % (libpath, os.strerror($!)))
#X#        exit(1)
#X#    print("Probing for prerequisite libzvbi in $libpath...")
#X#else:
#X#    print("Probing for prerequisite libzvbi at standard paths...")
#X#
#X#my $ll = ExtUtils::Liblist->ext($libs, 1, 1);  # verbose, return names
#X#if $#$ll < 0:
#X#    print STDERR "\nFATAL: Prerequisite zvbi library not found on your system.\n".
#X#                 "If it's located at a non-standard path, run Makefile.PL\n".
#X#                 "with 'LIBPATH=/your/path' on the command line (and replace\n".
#X#                 "'/your/path' with the directory which holds libzvbi.so)\n";
#X#    exit(1)
#X#
#X##
#X## Compile path and base name of the library (sans minor version) into the
#X## module for dynamic loading
#X##
#X#my $lso_path;
#X#foreach (@$ll) {
#X#        if (m#(^|/)(libzvbi\.so($|[^a-zA-Z].*))#) {
#X#                $lso_path = $_;
#X#                if (($lso_path =~ /(.*libzvbi\.so.\d+)/) && (-r $1)) {
#X#                        $lso_path = $1;
#X#                }
#X#                print STDERR "Library path for dlopen(): $lso_path\n";
#X#                last;
#X#        }
#X#}
#X#
#X## load optional symbols dynamically from shared library
#X## this requires to pass the library path and file name to dlopen()
#X#die "\nFATAL: libzvbi.so not found in:\n".join(",",@$ll)."\n" unless defined $lso_path;
#X#extradef += [('USE_DL_SYM', 1), ('LIBZVBI_PATH', '"' + lso_path + '"')]
#X#
#X## use packaged header file instead of the one that may or may not be installed
#X## should only be enabled together with USE_DL_SYM
#X#extradef .= [('USE_LIBZVBI_INT', 1)]

# ----------------------------------------------------------------------------

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

ext = Extension('Zvbi',
                sources       = ['src/zvbi.c'] + extrasrc,
                include_dirs  = ['src'] + extrainc,
                define_macros = extradef,
                libraries     = extralibs,
                library_dirs  = extralibdirs,
                #undef_macros  = ["NDEBUG"]   # for debug build only
               )

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
          'Development Status :: 4 - Beta',
          "Programming Language :: C",
          "Programming Language :: Python :: 3",
          'Topic :: Multimedia :: Video :: Capture',
          'Intended Audience :: Developers',
          "Operating System :: POSIX :: Linux",
          "Operating System :: POSIX :: BSD",
          "License :: OSI Approved :: GNU General Public License v2 (GPLv2)"
         ],
      keywords="teletext, closed-caption, VPS, WSS, VBI, DVB, video, capture, decoder, libzvbi, zvbi",
      platforms=['posix'],
      ext_modules=[ext],
      cmdclass={'clean': MyClean},
      python_requires='>=3.2',
     )
