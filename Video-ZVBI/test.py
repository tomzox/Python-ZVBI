#!/usr/bin/python3
#
# Copyright (C) 2007,2020 T. Zoerner
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# For a copy of the GPL refer to <http://www.gnu.org/licenses/>

import Zvbi

# Well, so far we only test if the module loads correctly.
# For manual testing, see the examples/ sub-directory.

print("OK module booted, library version %d.%d.%d\n" % (Zvbi.lib_version()))

if not Zvbi.check_lib_version(0, 2, 35):
    print("Library version is outdated")
