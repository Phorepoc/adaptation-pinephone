#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

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
#
# RELEASE=1 (the release workflow): the card a user writes. The image ($O/symbian.img) goes into slot B with the
# control sector INSTALLED, which boot.cmd boots every time; P1 holds only boot.scr (no golden image). The U-Boot
# source package (Debian's, the exact version embedded: GPL-2.0) is put next to the image for the release.
O=${O:-.pinephone-build}
PY=python3; python3 -c "" 2>/dev/null || PY=py         # (Windows: python3 may be the Store stub)
if [ "${RELEASE:-0}" = 1 ]; then
	[ -f "$O/symbian.img" ] && [ -f "$O/boot.scr" ] || { echo "no $O/symbian.img + boot.scr: run make_pinephone_image.sh first"; exit 1; }
	cp -f "$O/symbian.img" "$O/slotb.img"
else
	# slot A = $O/golden.img (the text-shell ROM: build it, then cp symbian.img golden.img) -- symbian.img is slot B's
	[ -f "$O/golden.img" ] && [ -f "$O/boot.scr" ] || { echo "build the golden ROM, then cp $O/symbian.img $O/golden.img"; exit 1; }
	rm -f "$O/slotb.img"
fi
mkdir -p "$O/uboot"
OW=$(cd "$O" && (pwd -W 2>/dev/null || pwd))      # the output folder as docker sees it (any path)
MSYS_NO_PATHCONV=1 docker run --rm -e RELEASE="${RELEASE:-0}" -v "$OW:/o" debian:trixie bash -c '
  set -e
  export DEBIAN_FRONTEND=noninteractive
  dpkg --add-architecture arm64
  sed -i "s/^Types: deb$/Types: deb deb-src/" /etc/apt/sources.list.d/debian.sources
  apt-get update -qq >/dev/null
  apt-get install -y -qq dosfstools mtools fdisk dpkg-dev >/dev/null
  if [ ! -f /o/uboot/u-boot-sunxi-with-spl.bin ]; then
    cd /tmp && apt-get download -qq u-boot-sunxi:arm64 >/dev/null && dpkg-deb -x u-boot-sunxi_*.deb x
    cp x/usr/lib/u-boot/pinephone/u-boot-sunxi-with-spl.bin /o/uboot/
    ls u-boot-sunxi_*.deb > /o/uboot/VERSION
  fi
  if [ "$RELEASE" = 1 ] && ! ls /o/uboot/u-boot-source-*.tar >/dev/null 2>&1; then
    # the source of exactly the binary above (u-boot-sunxi is built from the u-boot source package)
    v=$(sed -n "s/^u-boot-sunxi_\(.*\)_arm64\.deb$/\1/p" /o/uboot/VERSION)
    mkdir -p /tmp/src && cd /tmp/src && apt-get source -qq --download-only "u-boot=$v" >/dev/null
    tar -cf "/o/uboot/u-boot-source-$v.tar" -C /tmp/src .
  fi
  # P1 and C: as FAT32 file systems; the 1 GiB C: file is sparse and only its front (boot sector, FATs, root) goes
  # into the card image
  truncate -s 60M /tmp/p1.img; mkfs.vfat -F 32 -n PHOREPOC /tmp/p1.img >/dev/null
  mcopy -i /tmp/p1.img /o/boot.scr ::boot.scr
  [ "$RELEASE" = 1 ] || mcopy -i /tmp/p1.img /o/golden.img ::symbian.img
  truncate -s 1G /tmp/c.img; mkfs.vfat -F 32 -s 8 -n SYMBIAN_C /tmp/c.img >/dev/null   # FAT32, 4 KB clusters: the phone mounts it with EFAT32.FSY
  cp /tmp/p1.img /o/p1.part; dd if=/tmp/c.img of=/o/c.head bs=1M count=8 status=none
  mdir -i /tmp/p1.img :: | grep -i "scr\|img"; fsck.vfat -n /tmp/c.img | tail -1
'
# the MBR + assembly on the host (sfdisk refuses partitions past the end of a 2 GiB-short image file)
"$PY" tools/phorepoc-arm/sd_layout.py "$O" || exit 1
rm -f "$O/p1.part" "$O/c.head" "$O/slotb.img"
echo "make_pinephone_sd: $O/phorepoc-pinephone-sd.img (U-Boot: $(cat "$O/uboot/VERSION"))"
