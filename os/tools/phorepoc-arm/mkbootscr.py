# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

"""mkbootscr.py -- write a U-Boot legacy script image (boot.scr) from a text script, like
`mkimage -A arm -O linux -T script -C none -d boot.cmd boot.scr`, without needing u-boot-tools.

Legacy image = a 64-byte big-endian header (magic 0x27051956, header CRC32, time, data size, load, entry,
data CRC32, os, arch, type, compression, 32-byte name) + data. For a script, the data is mkimage's multi-file
layout that U-Boot's `source` skips: a be32 length table {script length, 0}, then the script text.
Usage: py tools/phorepoc-arm/mkbootscr.py <boot.cmd> <boot.scr> [name]
"""
import struct
import sys
import time
import zlib

IH_MAGIC, IH_OS_LINUX, IH_ARCH_ARM, IH_TYPE_SCRIPT, IH_COMP_NONE = 0x27051956, 5, 2, 6, 0


def bootscr(script: bytes, name: str = "boot script") -> bytes:
    data = struct.pack(">II", len(script), 0) + script
    def header(hcrc):
        return struct.pack(">IIIIIIIBBBB32s", IH_MAGIC, hcrc, int(time.time()), len(data), 0, 0,
                           zlib.crc32(data) & 0xFFFFFFFF, IH_OS_LINUX, IH_ARCH_ARM, IH_TYPE_SCRIPT,
                           IH_COMP_NONE, name.encode()[:31])
    return header(zlib.crc32(header(0)) & 0xFFFFFFFF) + data


if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, "rb") as f:
        text = f.read().replace(b"\r\n", b"\n")
    out = bootscr(text, sys.argv[3] if len(sys.argv) > 3 else "boot script")
    with open(dst, "wb") as f:
        f.write(out)
    print(f"mkbootscr: {dst} ({len(out)} bytes, script {len(text)} bytes)")
