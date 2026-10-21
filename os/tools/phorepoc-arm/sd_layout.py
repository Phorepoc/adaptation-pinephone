# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

"""sd_layout.py <dir> -- assemble the PinePhone SD-card image (make_pinephone_sd.sh calls it; layout there).

Reads <dir>/uboot/u-boot-sunxi-with-spl.bin, <dir>/p1.part (P1 FAT32, golden) and <dir>/c.head (the front of C:'s
pre-formatted 1 GiB FAT32), writes <dir>/phorepoc-pinephone-sd.img with a hand-made MBR: partition entries may point
past the end of the image file (slot B, D:), which sfdisk refuses for a file. Partition NUMBERS (MBR entry order):
P1 golden, P2 slot B, P3 C:, P4 D:. Sector numbers are shared with a64_glue.cpp and boot.cmd -- keep in step.
"""
import os
import struct
import sys

MIB = 2048                        # sectors per MiB
P1 = (4 * MIB, 60 * MIB)          # golden (slot A): FAT32 LBA, bootable
C = (64 * MIB, 1024 * MIB)        # P3 = C: (FAT32 LBA, 0x0C)
SLOT_B = (1088 * MIB, 1024 * MIB) # P2 = slot B (raw; 0x220000) -- KSlotBSector
CTL = 6144                        # the A/B control sector (a64_glue.cpp KCtlSector, boot.cmd 0x1800)
D = (2112 * MIB, 0)               # P4 = D: (size 0 = to the end of the card; the phone grows it)
assert SLOT_B[0] == 0x220000


def entry(boot, ptype, start, size):
    # CHS fields = the LBA-only "unused" marker (0xFE 0xFF 0xFF); everything reads the LBA fields
    return struct.pack("<B3sB3sII", 0x80 if boot else 0, b"\xfe\xff\xff", ptype, b"\xfe\xff\xff", start, size)


def main():
    d = sys.argv[1]
    spl = open(os.path.join(d, "uboot", "u-boot-sunxi-with-spl.bin"), "rb").read()
    p1 = open(os.path.join(d, "p1.part"), "rb").read()
    chead = open(os.path.join(d, "c.head"), "rb").read()
    assert len(p1) <= P1[1] * 512, "P1 image larger than its partition"
    assert 8192 + len(spl) <= 4096 * 512, "U-Boot would overlap the SD log at sector 4096"
    img = bytearray((C[0] * 512) + len(chead))
    # MBR: entries in partition-number order, physical order differs (P1, C:, slot B, D:)
    mbr = (entry(True, 0x0C, *P1) + entry(False, 0xDA, *SLOT_B) + entry(False, 0x0C, *C) + entry(False, 0x0C, *D))
    img[446:446 + 64] = mbr
    img[510:512] = b"\x55\xaa"
    img[440:444] = struct.pack("<I", 0x50485350)   # disk signature 'PSHP'
    img[8192:8192 + len(spl)] = spl
    img[P1[0] * 512:P1[0] * 512 + len(p1)] = p1
    img[C[0] * 512:C[0] * 512 + len(chead)] = chead
    # a release card: the image in slot B + the control sector {'PHSL', INSTALLED (3), len, word sum} (a64_glue.cpp
    # KCtlSector; boot.cmd boots an INSTALLED slot B every time). The image file then runs to the end of the image.
    slotb = os.path.join(d, "slotb.img")
    sb = b""
    if os.path.exists(slotb):
        sb = open(slotb, "rb").read()
        assert len(sb) <= SLOT_B[1] * 512, "image larger than slot B"
        words = len(sb) // 4
        total = sum(struct.unpack_from("<%dI" % words, sb)) & 0xFFFFFFFF
        img[CTL * 512:CTL * 512 + 16] = struct.pack("<IIII", 0x4C534850, 3, len(sb), total)
    out = os.path.join(d, "phorepoc-pinephone-sd.img")
    with open(out, "wb") as f:
        f.write(img)
        if sb:
            f.seek(SLOT_B[0] * 512)                   # (the gap stays a hole: a sparse file, small once compressed)
            f.write(sb)
        size = f.tell()
    print(f"sd_layout: {out} ({size >> 20} MiB{', slot B = the installed image' if sb else ''}): P1 @{P1[0]} +{P1[1]}, P2 slot B @{SLOT_B[0]:#x} +{SLOT_B[1]}, "
          f"P3 C: @{C[0]} +{C[1]}, P4 D: @{D[0]} (to the end of the card)")


if __name__ == "__main__":
    main()
