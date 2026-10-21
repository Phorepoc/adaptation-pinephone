#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-1.0

# make_pinephone_image.sh [rom] -- package a BOARD=a64 ROM for the PinePhone's U-Boot.
# Output in .pinephone-build/:
#   symbian.img = the AArch64 shim (TARGET 0x40300000) padded to 1 MB, then the ROM -- `load ... 0x40200000`
#   boot.scr    = src/phorepoc-arm/a64/boot.cmd as a U-Boot script image (tools/phorepoc-arm/mkbootscr.py)
# Copy both to the boot partition U-Boot reads boot.scr from. Default rom: .bootstrap-build/ekern.rom
# (built with BOARD=a64 SMP=1 MMODEL=flexible ... bash tools/kernel/build_ekern_rom.sh).
set -eu
cd "$(dirname "$0")/../.."
ROM="${1:-.bootstrap-build/ekern.rom}"; O=.pinephone-build; mkdir -p "$O"
[ -f "$ROM" ] || { echo "no ROM at $ROM"; exit 1; }
# the shim needs docker (aarch64 gcc); rebuild it only when its source changed -- the ROM-only loop stays docker-free
# SHIM_PROBE=0: no A32 LED probe (its 1 s holds + uncached whole-ROM checksum run under U-Boot's 16 s slot B watchdog,
# before the kernel can kick it: a ~200 MB ROM spends seconds there -- 2026-10-16, full images reset before stage 3)
if [ "${SHIM_PROBE:-0}" = 1 ]; then SHIM=.a53-build/shim-ram.bin; SDEFS=-DA64_LEDS; else SHIM=.a53-build/shim-ram-noprobe.bin; SDEFS=; fi
if [ ! -f $SHIM ] || [ src/phorepoc-arm/a53shim/shim.S -nt $SHIM ] \
   || [ tools/phorepoc-arm/build_shim.sh -nt $SHIM ]; then
  DEFS="$SDEFS" TARGET=0x40300000 OUT=$SHIM bash tools/phorepoc-arm/build_shim.sh >/dev/null
fi
# RAM_MB (default 2304: EKA2 keeps free RAM < 2 GB -- Mmu::Init2FinalCommon pins every page above that -- so ~2 GB + the ROM + kernel is the useful maximum) = the RAM Symbian uses: {'PHRM', MB} baked into the shim pad's last 8 bytes (0x402FFFF8 once
# loaded; a64.s GetRamMb reads it: multiples of 256 from 512 to 2816, else 1024). A boot.cmd with the boot-config sector overrides it.
RAM_MB="${RAM_MB:-2304}" py - "$ROM" "$O/symbian.img" "$SHIM" <<'PY'
import os, sys
rom = open(sys.argv[1], 'rb').read()
shim = open(sys.argv[3], 'rb').read()
assert len(shim) <= 0x100000
import struct
n = len(rom) & ~3                                   # the A64_LEDS probe's ROM checksum (shim.S a32probe)
if struct.pack('<I', 0xC6C6C6C6) in shim:
    total = sum(struct.unpack_from('<%dI' % (n // 4), rom)) & 0xFFFFFFFF
    shim = shim.replace(struct.pack('<I', 0xC6C6C6C6), struct.pack('<I', n), 1)
    shim = shim.replace(struct.pack('<I', 0xC5C5C5C5), struct.pack('<I', total), 1)
    print('probe checksum: %d bytes, sum 0x%08x' % (n, total))
assert len(shim) <= 0x100000 - 8
pad = bytearray(shim + bytes(0x100000 - len(shim)))
pad[0x100000 - 8:] = struct.pack('<II', 0x4D524850, int(os.environ['RAM_MB']))
open(sys.argv[2], 'wb').write(bytes(pad) + rom)
PY
[ -f "$ROM.map" ] && cp -f "$ROM.map" "$O/symbian.img.map" && echo "make_pinephone_image: ROM placement map -> $O/symbian.img.map (image, run address, size; symbolise a crash lr with it)"
py tools/phorepoc-arm/mkbootscr.py src/phorepoc-arm/a64/boot.cmd "$O/boot.scr" "Phorepoc PinePhone" >/dev/null
echo "make_pinephone_image: $O/symbian.img ($(wc -c < "$O/symbian.img") bytes) + $O/boot.scr"
