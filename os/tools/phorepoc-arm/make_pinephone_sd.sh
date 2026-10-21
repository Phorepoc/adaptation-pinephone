#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-1.0

# make_pinephone_sd.sh -- a whole SD-card image for the PinePhone ( "First hardware
# test"), to write with balenaEtcher / dd. Run make_pinephone_image.sh first.
#
# Layout (MBR, not GPT: the A64 boot ROM wants the SPL at 8 KiB, where GPT's table would be). The A64 boot ROM reads
# the SD card BEFORE the eMMC, so the phone boots this card when it is in and its own eMMC system (Mobian, the
# fallback) when it is out -- nothing on the eMMC is touched. Partition NUMBERS (user, 2026-10-07) vs physical order:
#   raw   8 KiB         u-boot-sunxi-with-spl.bin (Debian's u-boot-sunxi, pinephone build: SPL + ATF + U-Boot)
#   raw   sector 4096   the kernel's SD log (slot A; slot B's follows) -- a64_glue.cpp KSdLogSector
#   raw   sector 6144   the A/B control sector {'PHSL', state, len, sum} -- KCtlSector, boot.cmd
#   P1    4 MiB, 60 MiB   FAT32, bootable: boot.scr + symbian.img = slot A, the golden image
#   P3    64 MiB, 1 GiB   FAT32 "C:" (the phone's C: drive; pre-formatted here; EFAT32.FSY)
#   P2    1088 MiB, 1 GiB raw (type 0xDA): slot B, written over USB by phlink -- KSlotBSector = 0x220000, boot.cmd
#   P4    2112 MiB, rest  "D:" (FAT32 type, size 0 = to the end of the card: the phone grows it; unformatted at first)
# The image holds only the front of the card (U-Boot .. C:'s FAT metadata), so flashing it stays quick.
# Output: .pinephone-build/phorepoc-pinephone-sd.img. The U-Boot binary is cached in .pinephone-build/uboot/.
O=.pinephone-build
# slot A = $O/golden.img (the text-shell ROM: build it, then cp symbian.img golden.img) -- symbian.img is slot B's
[ -f "$O/golden.img" ] && [ -f "$O/boot.scr" ] || { echo "build the golden ROM, then cp $O/symbian.img $O/golden.img"; exit 1; }
mkdir -p "$O/uboot"
W=$(pwd -W 2>/dev/null || pwd)
MSYS_NO_PATHCONV=1 docker run --rm -v "$W/$O:/o" debian:trixie bash -c '
  set -e
  export DEBIAN_FRONTEND=noninteractive
  dpkg --add-architecture arm64
  apt-get update -qq >/dev/null
  apt-get install -y -qq dosfstools mtools fdisk >/dev/null
  if [ ! -f /o/uboot/u-boot-sunxi-with-spl.bin ]; then
    cd /tmp && apt-get download -qq u-boot-sunxi:arm64 >/dev/null && dpkg-deb -x u-boot-sunxi_*.deb x
    cp x/usr/lib/u-boot/pinephone/u-boot-sunxi-with-spl.bin /o/uboot/
    ls u-boot-sunxi_*.deb > /o/uboot/VERSION
  fi
  # P1 (golden) and C: as FAT32 file systems; the 1 GiB C: file is sparse and only its front (boot sector, FATs, root)
  # goes into the card image
  truncate -s 60M /tmp/p1.img; mkfs.vfat -F 32 -n PHOREPOC /tmp/p1.img >/dev/null
  mcopy -i /tmp/p1.img /o/boot.scr ::boot.scr; mcopy -i /tmp/p1.img /o/golden.img ::symbian.img
  truncate -s 1G /tmp/c.img; mkfs.vfat -F 32 -s 8 -n SYMBIAN_C /tmp/c.img >/dev/null   # FAT32, 4 KB clusters: the phone mounts it with EFAT32.FSY
  cp /tmp/p1.img /o/p1.part; dd if=/tmp/c.img of=/o/c.head bs=1M count=8 status=none
  mdir -i /tmp/p1.img :: | grep -i "scr\|img"; fsck.vfat -n /tmp/c.img | tail -1
'
# the MBR + assembly on the host (sfdisk refuses partitions past the end of a 2 GiB-short image file)
py tools/phorepoc-arm/sd_layout.py "$O" || exit 1
rm -f "$O/p1.part" "$O/c.head"
echo "make_pinephone_sd: $O/phorepoc-pinephone-sd.img (U-Boot: $(cat "$O/uboot/VERSION"))"
