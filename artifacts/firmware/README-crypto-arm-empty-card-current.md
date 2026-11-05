# Pico 2 ARM crypto release images

These RP2350 ARM Secure UF2 files are prepared as release assets. Each image
contains the firmware and a full 1 MiB of erased card storage. No CAP package
or applet is preinstalled; install an upstream CAP separately with GPPro.

Flashing either image clears persistent card state. Use BOOTSEL and copy only
the selected UF2 to the Pico 2 `RPI-RP2` drive.

| Profile | Capabilities enabled | File | Size | SHA-256 |
| --- | ---: | --- | ---: | --- |
| `max_crypto` | 147 / 150 | `pico2-jcvm-max_crypto-arm-empty-card-current.uf2` | 4,026,368 bytes | `0868a0ec81b13bd5a5a7c9e42f2ac64fe061ade69d12ac93bded6aa852ec5873` |
| `symm_crypto` | 46 / 150 | `pico2-jcvm-symm_crypto-arm-empty-card-current.uf2` | 3,837,440 bytes | `40032e23306d73d4c095659b41ecb9b90441c447accaf870a88048b2bc501571` |

Both images passed `picotool info -a` as RP2350 ARM Secure UF2s. A block-map
check confirmed unique addresses, consistent block numbering, and 4,096
storage blocks filled with `0xFF` at `0x10100000`–`0x101FFFFF`. Firmware-only
ends are `0x100EB604` (`max_crypto`) and `0x100D45A8` (`symm_crypto`). Neither
image was flashed or validated on a physical token in this release-preparation
step. The files are local release assets and have not been uploaded to GitHub.

Rebuild with:

```sh
./tools/build arm --profile max_crypto
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 \
  artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2
./tools/build arm --profile symm_crypto
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 \
  artifacts/firmware/pico2-jcvm-symm_crypto-arm-empty-card-current.uf2
```
