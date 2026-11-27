#!/usr/bin/env python3
"""Write the .mif that tells the console which applet class lives in a .mod.

Format (read from the SDK example helloworld.mif and the zeebx loader, not copied from code):
  0x00 u16 magic 0x0011 | 0x10 u32 offset of the boundary table | 0x14 u32 number of sections
  boundary table: (sections + 1) u32 | applet record: 20 bytes = class id, 0, number, 0, 0
Smallest valid file: 0x20 header + 8 boundary bytes + 20 record bytes = 60 bytes.

  gen_mif.py OUT.mif 0x0BE07100
"""
import struct
import sys


def build(clsid: int, number: int = 1000) -> bytes:
    table = 0x20
    data = table + 8
    end = data + 20
    head = bytearray(table)
    struct.pack_into("<H", head, 0x00, 0x0011)
    struct.pack_into("<I", head, 0x10, table)
    struct.pack_into("<I", head, 0x14, 1)
    return bytes(head) + struct.pack("<II", data, end) + struct.pack("<IIIII", clsid, 0, number, 0, 0)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    out, clsid = sys.argv[1], int(sys.argv[2], 0)
    blob = build(clsid)
    open(out, "wb").write(blob)
    table, = struct.unpack_from("<I", blob, 0x10)
    start, = struct.unpack_from("<I", blob, table)
    got, = struct.unpack_from("<I", blob, start)
    print(f"MIF {out} ({len(blob)} bytes) class id {got:#010x}")
    return 0 if got == clsid else 1


if __name__ == "__main__":
    sys.exit(main())
