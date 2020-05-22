#!/usr/bin/python3
#
#  libzvbi test
#  Copyright (C) 2006 Michael H. Schimek
#  Perl Port: Copyright (C) 2007 Tom Zoerner
#  Python Port: Copyright (C) 2020 Tom Zoerner
#
# Description:
#
#   This script contains tests for encoding and decoding the VPS data
#   service on randomly generated data.
#
#   (This is a direct translation of test/test-vps.c in libzvbi.)

import random
import Zvbi

def test_invalid(cni):
    try:
        foo = Zvbi.encode_vps_cni(cni)
        raise Exception("Unexpected success encoding VPS CNI 0x%X" % cni)
    except Zvbi.Error:
        pass

def main():
    cnis = [0x000, 0x001, 0x5A5, 0xA5A, 0xFFF]

    random.seed()
    rands = bytearray()
    for i in range(0, 13):
        rands.append(random.randrange(0, 256))

    buffer_dec = bytes(rands)
    buffer_enc = bytes(rands)

    cni2 = Zvbi.decode_vps_cni(buffer_dec)

    buffer_enc = Zvbi.encode_vps_cni(cni2)
    #if not buffer_enc == buffer_dec:
    #    print("ERROR: mismatch")

    for enc_cni in cnis:
        buffer_enc = Zvbi.encode_vps_cni(enc_cni)

        dec_cni = Zvbi.decode_vps_cni(buffer_enc)
        if not dec_cni == enc_cni:
            raise Exception("ERROR: mismatch CNI 0x%X -> 0x%X" % (enc_cni, dec_cni))

    test_invalid(-1)
    test_invalid(0x1000)
    test_invalid((1<<31)-1)

    print("OK")

main()
