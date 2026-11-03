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
# the image recipe's outputs share the packager's folder: keep them, build the golden ROM, put them back
keep=$(mktemp -d)
for f in symbian.img symbian.img.map boot.scr; do [ -f "$V/.pinephone-build/$f" ] && cp -f "$V/.pinephone-build/$f" "$keep/"; done
# without SECONDARY=efile the ROM silently drops ESHELL/EWSRV
( cd "$V" && PACKAGE=tools/phorepoc-arm/make_pinephone_image.sh BOARD=a64 SMP=1 MMODEL=flexible SECONDARY=efile \
    bash tools/kernel/build_ekern_rom.sh ) > "$OUT/golden.log" 2>&1 \
  || { echo "GOLDEN FAILED:"; tail -20 "$OUT/golden.log"; exit 1; }
cp -f "$V/.pinephone-build/symbian.img" "$OUT/golden.img"
cp -f "$keep"/* "$V/.pinephone-build/"; rm -rf "$keep"
echo "golden image: $(ls -l "$OUT/golden.img" | awk '{print $5}') bytes -> $OUT/golden.img"
