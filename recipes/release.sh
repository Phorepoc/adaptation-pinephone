#!/usr/bin/env bash
# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

# recipes/release.sh -- the PinePhone release files into $OUT ("ph image release -b pinephone", run on the host after
# every component is built): the SD-card image for balenaEtcher and the U-Boot source it embeds (its licence asks us
# to offer it).
set -eu
: "${OUT:?run it with ph image release -b pinephone}" "${PHOREPOC_BUILD:?}" "${PHOREPOC_ADAPTATIONS:?}"
# the SoC's entry shim (its host recipe: the first of the board's repositories that has one)
for r in $(echo "$PHOREPOC_ADAPTATIONS" | tr : ' '); do
	[ -f "$r/recipes/host/shim.sh" ] && { bash "$r/recipes/host/shim.sh" "$PHOREPOC_BUILD"; break; }
done
ph image zdata -b pinephone
ph image skin -b pinephone
ph image recipe image -b pinephone
sudo -n chown -R "$(id -u):$(id -g)" "$PHOREPOC_BUILD" 2>/dev/null || true   # (the container wrote it as root)
# the SD card (on the host: its script runs docker for the FAT tools + Debian's U-Boot)
(cd "$(dirname "$0")/../os" && O="$PHOREPOC_BUILD/pinephone" RELEASE=1 bash tools/phorepoc-arm/make_pinephone_sd.sh)
mkdir -p "$OUT"
cp --sparse=always "$PHOREPOC_BUILD/pinephone/phorepoc-pinephone-sd.img" "$OUT/phorepoc-pinephone.img"
xz -T0 -6 -f "$OUT/phorepoc-pinephone.img"
cp "$PHOREPOC_BUILD"/pinephone/uboot/u-boot-source-*.tar "$OUT/"
