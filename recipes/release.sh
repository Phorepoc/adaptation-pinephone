#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# recipes/release.sh -- the PinePhone release files into $OUT ("phe image release -b pinephone", run on the host after
# every component is built), development images for now:
#   phorepoc-pinephone-update.img.xz   the image alone, to push to a phone's slot B with phe device push
#   phorepoc-pinephone-sdcard.img.xz   a development SD card for balenaEtcher: the golden image in slot A, the image
#                                      in slot B (make_pinephone_sd.sh RELEASE=dev)
#   u-boot-source-*.tar                the source of the U-Boot the card embeds (its licence asks us to offer it)
set -eu
: "${OUT:?run it with phe image release -b pinephone}" "${PHOREPOC_BUILD:?}" "${PHOREPOC_ADAPTATIONS:?}"
# the SoC's entry shim (its host recipe: the first of the board's repositories that has one)
for r in $(echo "$PHOREPOC_ADAPTATIONS" | tr : ' '); do
	[ -f "$r/recipes/host/shim.sh" ] && { bash "$r/recipes/host/shim.sh" "$PHOREPOC_BUILD"; break; }
done
phe image zdata -b pinephone
phe image skin -b pinephone
phe image recipe image -b pinephone
phe image recipe golden -b pinephone
sudo -n chown -R "$(id -u):$(id -g)" "$PHOREPOC_BUILD" 2>/dev/null || true   # (the container wrote it as root)
# the SD card (on the host: its script runs docker for the FAT tools + Debian's U-Boot)
(cd "$(dirname "$0")/../os" && O="$PHOREPOC_BUILD/pinephone" RELEASE=dev bash tools/phorepoc-arm/make_pinephone_sd.sh)
mkdir -p "$OUT"
cp --sparse=always "$PHOREPOC_BUILD/pinephone/phorepoc-pinephone-sd.img" "$OUT/phorepoc-pinephone-sdcard.img"
cp "$PHOREPOC_BUILD/pinephone/symbian.img" "$OUT/phorepoc-pinephone-update.img"
xz -T0 -6 -f "$OUT/phorepoc-pinephone-sdcard.img" "$OUT/phorepoc-pinephone-update.img"
cp "$PHOREPOC_BUILD"/pinephone/uboot/u-boot-source-*.tar "$OUT/"
