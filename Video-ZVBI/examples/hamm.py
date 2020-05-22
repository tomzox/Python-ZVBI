#!/usr/bin/python3
#
#  libzvbi test
#
#  Copyright (C) 2003, 2005 Michael H. Schimek
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
#   Automated test of the odd parity and Hamming encoder and decoder
#   functions. Note this test runs for a long time.
#
#   (This is a direct translation of test/hamm.c in the libzvbi package.)

import sys
import random
import Zvbi

def sizeof(n):
    return 4

def CHAR_BIT():
    return 8

def cast_unsigned_int(v):
    return v & 0xFFFFFFFF

def cast_int(v):
    return v & 0xFFFFFFFF

def mrand48():
    val = random.randrange(0xFFFFFFF);
    if random.randrange(2) == 0:
        val *= -1
    return val

def parity(n):
    sh = sizeof(n) * CHAR_BIT() // 2;
    while sh > 0:
        n ^= n >> sh
        sh >>= 1

    return n & 1

def BC(v):
    return (v * cast_unsigned_int(0x01010101))  #0x0101010101010101

def population_count(n):
    n -= (n >> 1) & BC (0x55)
    n = (n & BC (0x33)) + ((n >> 2) & BC (0x33))
    n = (n + (n >> 4)) & BC (0x0F)

    return (n * BC (0x01)) >> (sizeof ("unsigned int") * 8 - 8)


def hamming_distance(a, b):
    return population_count(a ^ b)

def main_func():
    print("Testing parity...")

    for i in range (0, 10000):
        n = i if (i < 256) else cast_unsigned_int(mrand48 ())
        buf = bytes([n&0xFF, (n >> 8)&0xFF, (n >> 16)&0xFF])

        r = 0
        for j in range(0, 8):
            if (n & (0x01 << j)):
                r |= 0x80 >> j

        assert (r == Zvbi.rev8(n))

        if (parity (n & 0xFF)):
            assert (Zvbi.unpar8(n) == cast_int(n & 127))
        else:
            assert (-1 == Zvbi.unpar8(n))

        assert (Zvbi.unpar8(Zvbi.par8(n)) >= 0)

        buf2 = Zvbi.par_str(buf)
        buf3 = Zvbi.unpar_str(buf2)
        assert (0 == ((buf3[0] | buf3[1] | buf3[2]) & 0x80))

        tmp1 = Zvbi.par8(buf3[1])
        tmp2 = tmp1 ^ 0x80
        buf4 = bytes([0, tmp1, tmp2])

        buf5 = Zvbi.unpar_str(buf4)
        assert (buf5[2] == (buf5[1] & 0x7F))

    print("OK")
    print("Testing Hamming-8/4...")

    for i in range(0, 10000):
        n = i if (i < 256) else cast_unsigned_int(mrand48())

        A = parity(n & 0xA3)
        B = parity(n & 0x8E)
        C = parity(n & 0x3A)
        D = parity(n & 0xFF)

        d = (  ((n & 0x02) >> 1)
             + ((n & 0x08) >> 2)
             + ((n & 0x20) >> 3)
             + ((n & 0x80) >> 4))

        if (A and B and C):
            nn = n if D else (n ^ 0x40)

            assert (Zvbi.ham8(d) == (nn & 255))
            assert (Zvbi.unham8(nn) == d)

        elif not D:
            dd = Zvbi.unham8(n)
            assert (dd >= 0 and dd <= 15)

            nn = Zvbi.ham8(dd)
            assert (hamming_distance(n & 255, nn) == 1)

        else:
            assert (Zvbi.unham8(n) == -1)

        #Zvbi.ham16 (buf, n)
        #assert (Zvbi.unham16 (buf) == (int)(n & 255))

    print("OK")
    print("Testing Hamming-24/18...")

    for i in range(0, (1 << 24)):
        buf = bytes([i&0xFF, (i >> 8)&0xFF, (i >> 16)&0xFF])

        A = parity (i & 0x555555)
        B = parity (i & 0x666666)
        C = parity (i & 0x787878)
        D = parity (i & 0x007F80)
        E = parity (i & 0x7F8000)
        F = parity (i & 0xFFFFFF)

        d = (  ((i & 0x000004) >> (3 - 1))
             + ((i & 0x000070) >> (5 - 2))
             + ((i & 0x007F00) >> (9 - 5))
             + ((i & 0x7F0000) >> (17 - 12)))
        
        if (A and B and C and D and E):
            assert (Zvbi.unham24p(buf) == d)
        elif F:
            assert (Zvbi.unham24p(buf) < 0)
        else:
            err = ((E << 4) | (D << 3)
                     | (C << 2) | (B << 1) | A) ^ 0x1F

            assert (err > 0)

            if (err >= 24):
                assert (Zvbi.unham24p(buf) < 0)
                continue

            ii = i ^ (1 << (err - 1))

            A = parity (ii & 0x555555)
            B = parity (ii & 0x666666)
            C = parity (ii & 0x787878)
            D = parity (ii & 0x007F80)
            E = parity (ii & 0x7F8000)
            F = parity (ii & 0xFFFFFF)

            assert (A and B and C and D and E and F)

            d = (+ ((ii & 0x000004) >> (3 - 1))
                  + ((ii & 0x000070) >> (5 - 2))
                  + ((ii & 0x007F00) >> (9 - 5))
                  + ((ii & 0x7F0000) >> (17 - 12)))

            assert (Zvbi.unham24p(buf) == d)

        if (i & 0x00FFFF) == 0:
            print(".", end='')
            sys.stdout.flush()

    print("OK")

main_func()
