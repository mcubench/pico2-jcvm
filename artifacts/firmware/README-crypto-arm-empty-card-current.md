# Pico 2 ARM crypto release images

These RP2350 ARM Secure UF2 files are prepared as release assets. Each image
contains the firmware and a full 1 MiB of erased card storage. No CAP package
or applet is preinstalled; install an upstream CAP separately with GPPro.

Flashing either image clears persistent card state. Use BOOTSEL and copy only
the selected UF2 to the Pico 2 `RPI-RP2` drive.

| Profile | Capabilities enabled | File | Size | SHA-256 |
| --- | ---: | --- | ---: | --- |
| `max_crypto` | 147 / 150 | `pico2-jcvm-max_crypto-arm-empty-card-current.uf2` | 4,047,360 bytes | `23f1977f6ee2328ec94722e75da3ac8a1b9748fb54c081e8262c37763d238834` |
| `symm_crypto` | 46 / 150 | `pico2-jcvm-symm_crypto-arm-empty-card-current.uf2` | 3,837,440 bytes | `40032e23306d73d4c095659b41ecb9b90441c447accaf870a88048b2bc501571` |

Both images are RP2350 ARM Secure UF2s. A block-map check confirmed unique
addresses, consistent block numbering, and 4,096 storage blocks filled with
`0xFF` at `0x10100000`–`0x101FFFFF`. The current `max_crypto` firmware ends
at `0x100EDFC0` (73,792 bytes remain before the storage partition); its UF2
contains 3,808 firmware blocks plus the 4,096 erased storage blocks. `picotool
info -a` verified the freshly built image. It has not been flashed or tested on
hardware; the older max_crypto image had passed a physical runtime cycle and
read-only GPPro checks on Pico 2 `D92E743D5F640C0F`, which does not establish
physical validation for this image. `symm_crypto` remains the earlier artifact
and was not revalidated in this step. Files are local release assets and have
not been uploaded to GitHub.

Rebuild with:

```sh
PICO2_DEBUG_SERIAL_WAIT_MS=0 ./tools/build arm --profile max_crypto
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 \
  artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2
PICO2_DEBUG_SERIAL_WAIT_MS=0 ./tools/build arm --profile symm_crypto
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 \
  artifacts/firmware/pico2-jcvm-symm_crypto-arm-empty-card-current.uf2
```
