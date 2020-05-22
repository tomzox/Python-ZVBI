#!/usr/bin/python3
#
#  libzvbi test of export interfaces
#
#  Copyright (C) 2000, 2001 Michael H. Schimek
#  Perl Port: Copyright (C) 2007 Tom Zoerner
#  Python Port: Copyright (C) 2020 Tom Zoerner
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
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

# Description:
#
#   Example for the use of export option management in class Zvbi.Export.
#   Test of page export options and menu interfaces. The script lists all
#   available export modules (i.e. formats) and options. (This is a direct
#   translation of test/explist.c in the libzvbi package.)

import sys
import re
import argparse
import Zvbi

TypeStr = {Zvbi.VBI_OPTION_BOOL: "VBI_OPTION_BOOL",
           Zvbi.VBI_OPTION_INT: "VBI_OPTION_INT",
           Zvbi.VBI_OPTION_REAL: "VBI_OPTION_REAL",
           Zvbi.VBI_OPTION_STRING: "VBI_OPTION_STRING",
           Zvbi.VBI_OPTION_MENU: "VBI_OPTION_MENU"}

def INT_TYPE(v):
    return ((v['type'] == Zvbi.VBI_OPTION_BOOL) or
            (v['type'] == Zvbi.VBI_OPTION_INT) or
            (v['type'] == Zvbi.VBI_OPTION_MENU))

def REAL_TYPE(v):
    return (v['type'] == Zvbi.VBI_OPTION_REAL)

def MENU_TYPE(v):
    return not (v.get('menu') is None)

def BOUNDS_CHECK(oi):
    if MENU_TYPE(oi):
        if ((oi['def'] >= 0) or
            (oi['def'] <= oi['max']) or
            (oi['min'] == 0) or
            (oi['max'] > 0) or
            (oi['step'] == 1)):
            pass
        else:
            raise Exception("ERROR: bounds check for menu failed:", str(oi));
    else:
        if ((oi['max'] >= oi['min']) or
            (oi['step'] > 0) or
            ((oi['def'] >= oi['min']) and (oi['def'] <= oi['max']))):
            pass
        else:
            raise Exception("ERROR: bounds check for menu failed:", str(oi));


def STRING_CHECK(oi):
    if MENU_TYPE(oi):
        if ((oi['def'] >= 0) or
            (oi['def'] <= oi['max']) or
            (oi['min'] == 0) or
            (oi['max'] > 0) or
            (oi['step'] == 1)):
            pass
        else:
            raise Exception("ERROR: check for STRING failed:", str(oi));
    else:
        if oi.get('def') is None:
            raise Exception("ERROR: check for STRING failed:", str(oi));

def keyword_check(keyword):
    if ((keyword is None) or not re.match(r'^[a-zA-Z0-9_]+', keyword)):
        raise Exception("ERROR: invalid keyword: \"%s\"" % keyword);

def print_current(oi, current):
    if REAL_TYPE(oi):
        print("    current value=%f" % current)
        if MENU_TYPE(oi):
            if (current < oi['min']) or (current > oi['max']):
                raise Exception("ERROR: current:%d not in range %d..%d" % (current, oi['min'], oi['max']));
    else:
        print("    current value=%d" % current)
        if not MENU_TYPE(oi):
            if (current < oi['min']) or (current > oi['max']):
                raise Exception("ERROR: current:%d not in range %d..%d" % (current, oi['min'], oi['max']));

def test_modified(oi, old, new):
    #if not old == new:
    #    print("but modified current value to %s" % new)
    pass

def test_set_int(ex, oi, current, value):
    print("    trying to set %d: " % value, end='')

    try:
        ex.option_set(oi['keyword'], value)
        print("success.")
        test_modified(oi, current, new_current)
    except Zvbi.ExportError as e:
        print("ERROR: export failed:", e)

    try:
        #new_current = 0x54321
        new_current = ex.option_get(oi['keyword'])
        test_modified(oi, current, new_current)
    except Zvbi.ExportError as e:
        print("option_get failed:", e)
        #if not new_current == 0x54321:
        #    print("but modified destination to new_current")

    current = new_current
    print_current(oi, new_current)


def test_set_real(ex, oi, current, value):
    print("    try to set %f: " % value)
    try:
        ex.option_set(oi['keyword'], value)
        print("success.")
    except Zvbi.ExportError as e:
        print("option_get failed:", e)

    #new_current = 8192.0
    try:
        new_current = ex.option_get(oi['keyword'])
        test_modified(oi, current, new_current)
    except Zvbi.ExportError as e:
            print("option_get failed", e)
            # XXX unsafe
            #if not new_current == 8192.0:
            #    print("but modified destination to new_current")

    current = new_current
    print_current(oi, new_current)


def test_set_entry(ex, oi, current, entry):
    valid = (MENU_TYPE(oi)
                 and (entry >= oi['min'])
                 and (entry <= oi['max']))

    print("    try to set menu entry %d: " % entry, end='')

    try:
        ex.option_menu_set(oi['keyword'], entry)
        if valid:
            print("success.")
        else:
            print("ERROR: option_menu_set unexpected success.")

        new_current = ex.option_get(oi['keyword'])
        test_modified(oi, current, new_current)

    except Zvbi.ExportError as e:
        if valid:
            print("ERROR: option_menu_set failed:", e)
        else:
            print("failed as expected", e)

    valid = MENU_TYPE(oi)

    #new_entry = 0x3333
    try:
        new_entry = ex.option_menu_get(oi['keyword'])
        if not valid:
            print("\nvbi_export_option_menu_get: unexpected success.")
    except Zvbi.ExportError as e:
        if valid:
            print("\nvbi_export_option_menu_get failed:", e)
        #if new_entry != 0x33333:
        #    raise Exception("vbi_export_option_menu_get failed, "+
        #                    "but modified destination to new_current\n")

    if ((oi['type'] == Zvbi.VBI_OPTION_BOOL) or
        (oi['type'] == Zvbi.VBI_OPTION_INT)):
        if MENU_TYPE(oi):
            if not (new_current == oi['menu'][new_entry]):
                raise Exception("FAILED")
        else:
                test_modified(oi, current, new_current)

        current = new_current
        print_current(oi, new_current)

    elif oi['type'] == Zvbi.VBI_OPTION_REAL:
        if MENU_TYPE(oi):
            # XXX unsafe
            if not (new_current == oi['menu'][new_entry]):
                raise Exception("FAILED")
        else:
            test_modified(oi, current, new_current)

        current = new_current
        print_current(oi, new_current)

    elif oi['type'] == Zvbi.VBI_OPTION_MENU:
        current = new_current
        print_current(oi, new_current)

    else:
        raise Exception("Unknown type:" + oi['type'])
    return current


def dump_option_info(ex, oi):
    type_str = TypeStr.get(oi['type'])
    if not type_str:
        raise Exception("invalid type in", str(oi))

    if not oi.get('label'):
        oi['label'] = "(null)";
    if not oi.get('tooltip'):
        oi['tooltip'] = "(null)";

    print("  * type=%s keyword=%s label=\"%s\" tooltip=\"%s\"" %
          (type_str, oi['keyword'], oi['label'], oi['tooltip']))

    keyword_check(oi['keyword'])

    if ((oi['type'] == Zvbi.VBI_OPTION_BOOL) or
        (oi['type'] == Zvbi.VBI_OPTION_INT)):
        BOUNDS_CHECK(oi)
        if MENU_TYPE(oi):
            print("    %d menu entries, default=%d: " %
                  (oi['max'] - oi['min'] + 1, oi['def']), end='')

            for i in range(0, oi['max'] + 1 - oi['min']):
                print("%d%s" % (oi['menu'][i],
                                (", " if (i < oi['max']) else "")), end='')
            print("")
        else:
            print("    default=%d, min=%d, max=%d, step=%d" %
                   (oi['def'], oi['min'], oi['max'], oi['step']))

        val = ex.option_get(oi['keyword'])

        print_current(oi, val)
        if opt.check:
            if MENU_TYPE(oi):
                val = test_set_entry(ex, oi, val, oi['min'])
                val = test_set_entry(ex, oi, val, oi['max'])
                val = test_set_entry(ex, oi, val, oi['min'] - 1)
                val = test_set_entry(ex, oi, val, oi['max'] + 1)
                val = test_set_int(ex, oi, val, oi['menu'][oi['min']])
                val = test_set_int(ex, oi, val, oi['menu'][oi['max']])
                val = test_set_int(ex, oi, val, oi['menu'][oi['min']] - 1)
                val = test_set_int(ex, oi, val, oi['menu'][oi['max']] + 1)
            else:
                val = test_set_entry(ex, oi, val, 0)
                val = test_set_int(ex, oi, val, oi['min'])
                val = test_set_int(ex, oi, val, oi['max'])
                val = test_set_int(ex, oi, val, oi['min'] - 1)
                val = test_set_int(ex, oi, val, oi['max'] + 1)

    elif oi['type'] == Zvbi.VBI_OPTION_REAL:
        BOUNDS_CHECK(oi)
        if MENU_TYPE(oi):
            print("    %d menu entries, default=%d: " %
                   (oi['max'] - oi['min'] + 1, oi['def']))
            for i in range(0, oi['max'] + 1 - oi['min']):
                print("%f%s" % (oi['menu'][i],
                                (", " if (i < oi['max']) else "")))
        else:
            print("    default=%f, min=%f, max=%f, step=%f" %
                   (oi['def'], oi['min'], oi['max'], oi['step']))

        val = ex.option_get(oi['keyword'])

        print_current(oi, val)
        if opt.check:
            if MENU_TYPE(oi):
                test_set_entry(ex, oi, val, oi['min'])
                test_set_entry(ex, oi, val, oi['max'])
                test_set_entry(ex, oi, val, oi['min'] - 1)
                test_set_entry(ex, oi, val, oi['max'] + 1)
                test_set_real(ex, oi, val, oi['menu'][oi['min']])
                test_set_real(ex, oi, val, oi['menu'][oi['max']])
                test_set_real(ex, oi, val, oi['menu'][oi['min']] - 1)
                test_set_real(ex, oi, val, oi['menu'][oi['max']] + 1)
            else:
                test_set_entry(ex, oi, val, 0)
                test_set_real(ex, oi, val, oi['min'])
                test_set_real(ex, oi, val, oi['max'])
                test_set_real(ex, oi, val, oi['min'] - 1)
                test_set_real(ex, oi, val, oi['max'] + 1)

    elif oi['type'] == Zvbi.VBI_OPTION_STRING:
        if MENU_TYPE(oi):
            STRING_CHECK(oi)
            print("    %d menu entries, default=%d: " %
                   (oi['max'] - oi['min'] + 1, oi['def']))
            for i in range(0, oi['max'] + 1 - oi['min']):
                print("%s%s" % (oi['menu'][i],
                                (", " if (i < oi['max']) else "")))
        else:
            print("    default=\"%s\"" % oi['def'])

        val = ex.option_get(oi['keyword'])

        print("    current value=\"%s\"" % val)
        if opt.check:
            print("    try to set \"foobar\": ", end='')
            try:
                ex.option_set(oi['keyword'], "foobar")
                printf("success.")
            except Zvbi.ExportError as e:
                print("failed:", e)

            val = ex.option_get(oi['keyword'])
            print("    current value=\"%s\"" % val)

    elif oi['type'] == Zvbi.VBI_OPTION_MENU:
            print("    %d menu entries, default=%d: " %
                   (oi['max'] - oi['min'] + 1, oi['def']), end='')
            for i in range(0, oi['max'] + 1 - oi['min']):
                print("%s%s" % (oi['menu'][i],
                                (", " if (i < oi['max']) else "")), end='')

            print("")
            val = ex.option_get(oi['keyword'])

            print_current(oi, val)
            if opt.check:
                val = test_set_entry(ex, oi, val, oi['min'])
                val = test_set_entry(ex, oi, val, oi['max'])
                val = test_set_entry(ex, oi, val, oi['min'] - 1)
                val = test_set_entry(ex, oi, val, oi['max'] + 1)

    else: # unreachable
        raise Exception("unknown type")

def list_options(ex):
    print("  List of options:")

    i = 0
    while True:
        try:
            oi = ex.option_info_enum(i)
            if not oi.get('keyword'):
                raise Exception("invalid option info:" + str(oi))

            foo = ex.option_info_keyword(oi['keyword'])
            dump_option_info(ex, oi)
            i += 1

        except StopIteration:
            break


def list_modules():
    print("List of export modules:")

    i = 0
    while True:
        try:
            xi = Zvbi.Export.info_enum(i)
            if not xi['keyword']:
                raise Exception()
            foo = Zvbi.Export.info_keyword(xi['keyword'])

            if not xi.get('label'):
                xi['label'] = "(null)"
            if not xi.get('tooltip'):
                xi['tooltip'] = "(null)"
            if not xi.get('mime_type'):
                xi['mime_type'] = "(null)"
            if not xi.get('extension'):
                xi['extension'] = "(null)"

            print(("* keyword=%s label=\"%s\"\n" +
                   "  tooltip=\"%s\" mime_type=%s extension=%s") %
                   (xi['keyword'], xi['label'],
                    xi['tooltip'], xi['mime_type'], xi['extension']))

            keyword_check(xi['keyword'])

            ex = Zvbi.Export(xi['keyword'])

            foo = ex.info_export()
            if not foo:
                raise Exception()

            list_options(ex)

            ex = None
            i += 1
        except StopIteration:
            break

        print("-- end of list --")


def ParseCmdOptions():
    global opt
    parser = argparse.ArgumentParser(description='List all export modules and options')
    parser.add_argument("--check", action='store_true', default=False, help="enable internal testing")
    opt = parser.parse_args()


ParseCmdOptions()
list_modules()
