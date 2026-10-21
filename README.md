# adaptation-pinephone

The PINE64 PinePhone device adaptation of Phorepoc, on [adaptation-a64](https://github.com/Phorepoc/adaptation-a64):
the board files (Bluetooth and WLAN configuration and firmware, sensors) and the phone's own components and tests
(the RTL8723CS Bluetooth transport and initialiser, WLAN bring-up).

## Layout

```
os/src/phorepoc-arm/board/        the board: bt/, wlan/ (firmware), cf/ ...
os/src/<component>/               hctl_h5, initialisor_rtl, wlanhwinit and the phone tests
registry/components_pinephone.py  this adaptation's components (merged into core's registry)
```

`os/` is laid over [core](https://github.com/Phorepoc/core)'s `os/` at build time, so paths match core's.

**Status:** the components here build one by one with the pdk (`ph component build -b pinephone -arch arm <name>`). The
PinePhone **image** (the ARM kernel and ROM link, the AArch64 boot shim, the U-Boot packaging) is not in the public
pdk yet; it is being ported. Until then, the PC board is the one that builds a complete image.

## Firmware

`os/src/phorepoc-arm/board/{bt,wlan}/fw/` holds Realtek firmware from linux-firmware, redistributable under the
licence next to it (`LICENCE.rtlwifi_firmware.txt`).

## Licence

Eclipse Public License 2.0 with GPL-2.0-or-later as a Secondary License (see `LICENSES/EPL-2.0.txt`; the Nokia-derived files are our updated versions relicensed from EPL-1.0 under its section 7, unmodified Symbian sources are fetched from upstream and remain EPL-1.0), copyright The Phorepoc Authors (`AUTHORS`, `NOTICE`), except the firmware above.

Contributions are welcome under the same terms: add yourself to `AUTHORS`.
