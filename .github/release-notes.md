These are development images: a card that always has a safe fallback, and an image to push to it.

## A new card

1. Write `phorepoc-pinephone-sdcard.img.xz` to a microSD card with [balenaEtcher](https://etcher.balena.io).
2. Put the card in and switch the phone on.

The card boots Phorepoc. It also holds a small fallback system (the golden image): if a pushed image fails to boot, the
phone falls back to it, and you can push again from there. Take the card out and the phone boots its own system again.

## Updating a card

With the phone on USB and the [pdk](https://github.com/Phorepoc/pdk)'s PhoneLab service running:

```sh
xz -d phorepoc-pinephone-update.img.xz
phe device push phorepoc-pinephone-update.img -w "Idle: ready"
```

The image goes into the card's test slot and boots once. If that boot fails, the phone is back on the golden image at
the next restart.
