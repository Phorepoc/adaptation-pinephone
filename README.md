# adaptation-pinephone

This repository makes Phorepoc run on the PINE64 PinePhone. The drivers for the phone's chip, the Allwinner A64, are in
[adaptation-a64](https://github.com/Phorepoc/adaptation-a64). What's here is the phone itself: its Bluetooth and Wi-Fi
setup and firmware, its sensors, and the tests we run on it.

## Try it

Download the `.img.xz` file from the [releases](https://github.com/Phorepoc/adaptation-pinephone/releases) and write it
to a microSD card with [balenaEtcher](https://etcher.balena.io). Put the card in and switch the phone on. Phorepoc runs
from the card, so nothing on the phone is touched. Take the card out and the phone boots its own system again.

## Building

The [pdk](https://github.com/Phorepoc/pdk) README has the commands for a PinePhone build, from the source to the SD card
image. With this repository, adaptation-a64 and mesa-symbian checked out next to core and the pdk, `-b pinephone` picks
them up by itself.

The release images are built by GitHub Actions at the commits listed in `board.lock`. Pushing a `v*` tag attaches them
to a draft release.

## What's in here

```
os/src/phorepoc-arm/board/        the phone's settings, plus Bluetooth and Wi-Fi configuration and firmware
os/src/<component>/               the Bluetooth driver (hctl_h5, initialisor_rtl), Wi-Fi start-up, phone tests
os/tools/phorepoc-arm/            the scripts that make the SD card image
registry/components_pinephone.py  the components this repository adds
```

At build time `os/` is laid over core's `os/`, so the paths line up with core's.

## License

Code we wrote is Apache-2.0. Symbian files we've changed are EPL-2.0, with GPL-2.0-or-later as a Secondary License, and
Symbian files we use unchanged stay EPL-1.0. The license texts are in `LICENSES/`.

The Realtek firmware in `os/src/phorepoc-arm/board/bt/fw/` and `wlan/fw/` comes from linux-firmware, and can be
redistributed under the license next to it.

Contributions are welcome under the same terms. Add your name to `AUTHORS` in your first pull request.
