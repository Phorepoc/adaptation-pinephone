#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# recipes/golden.sh -- the golden image ("phe image recipe golden -b pinephone", after "phe image recipe image"): slot A
# of a development card, the text-shell ROM the phone falls back to when a slot B boot fails, and from which an image
# can always be uploaded again (the kernel's USB receiver). Built from the tree recipes/image.sh assembled.
# Output: <build>/pinephone/golden.img
set -eu
PHOREPOC_BUILD="${PHOREPOC_BUILD:?run it with phe image recipe golden -b <board>}"
V="$PHOREPOC_BUILD/arm/kview"; OUT="$PHOREPOC_BUILD/pinephone"
[ -f "$V/tools/kernel/build_ekern_rom.sh" ] || { echo "no $V: run phe image recipe image -b pinephone first" >&2; exit 1; }
[ -f "$OUT/symbian.img" ] || { echo "no $OUT/symbian.img: run phe image recipe image -b pinephone first" >&2; exit 1; }
# what recipes/image.sh hands its kernel build: the generated executive headers (genexec), the view and its py shim
KINC="$PHOREPOC_BUILD/arm/kinc"
[ -f "$KINC/kernel/exec_kernel.h" ] || { echo "no $KINC/kernel/exec_kernel.h: run phe image recipe image -b pinephone first" >&2; exit 1; }
export PHOREPOC_KINC="$KINC" KVIEW="$V" PATH="$V/bin:$PATH"
# the text-shell ROM's own programs (build_ekern_rom.sh SECONDARY=efile without PHOREPOC=1): the text ESTART (the
# release's estart, not the SSM ROM's e32strt) built here, the rest from the ARM component build
A="$PHOREPOC_BUILD/arm"; PDK="${PHOREPOC_PDK:?run it with phe image recipe golden -b <board>}"
python3 "$PDK/tools/emitter/build_estart_exe.py" "$A/device/estart.exe" > "$OUT/golden-estart.log" 2>&1 \
  || { echo "build_estart_exe failed:"; tail -5 "$OUT/golden-estart.log"; exit 1; }
ln -sf "$A/device/estart.exe" "$V/emitter/estart.exe"
for f in elocal.fsy ewsrv.exe edisp.dll ekdata.dll econs.dll eshell.exe elocl_lan.001 elocl_reg.826 elocl_col.001 phfs.exe ektran.dll domaincli.dll; do
  [ -f "$A/img/$f" ] || { echo "no $A/img/$f: build the ARM components first" >&2; exit 1; }
  ln -sf "$A/img/$f" "$V/emitter/$f"
done
# the image recipe's outputs share the packager's folder: keep them, build the golden ROM, put them back
keep=$(mktemp -d)
for f in symbian.img symbian.img.map boot.scr; do [ -f "$V/.pinephone-build/$f" ] && cp -f "$V/.pinephone-build/$f" "$keep/"; done
# without SECONDARY=efile the ROM silently drops ESHELL/EWSRV. ROM_STABLE=0: the compact sequential layout -- the image's
# placement map (rom.placement.json) would put these few files at the full ROM's addresses (a 190 MB golden that does not
# fit the card's 60 MB boot partition) and would be overwritten with the golden's slots
( cd "$V" && PACKAGE=tools/phorepoc-arm/make_pinephone_image.sh BOARD=a64 SMP=1 MMODEL=flexible SECONDARY=efile ROM_STABLE=0 \
    bash tools/kernel/build_ekern_rom.sh ) > "$OUT/golden.log" 2>&1 \
  || { echo "GOLDEN FAILED:"; tail -20 "$OUT/golden.log"; exit 1; }
cp -f "$V/.pinephone-build/symbian.img" "$OUT/golden.img"; cp -f "$V/.pinephone-build/symbian.img.map" "$OUT/golden.img.map"
cp -f "$keep"/* "$V/.pinephone-build/"; rm -rf "$keep"
echo "golden image: $(ls -l "$OUT/golden.img" | awk '{print $5}') bytes -> $OUT/golden.img"
