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

## Firmware

`os/src/phorepoc-arm/board/{bt,wlan}/fw/` holds Realtek firmware from linux-firmware, redistributable under the
licence next to it (`LICENCE.rtlwifi_firmware.txt`).

## Licence

Eclipse Public License 1.0 (see `LICENSE`), except the firmware above.
