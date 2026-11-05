# Pico 2 ARM crypto release images

These RP2350 ARM Secure UF2 files are prepared as release assets. Each image
contains the firmware and a full 1 MiB of erased card storage. No CAP package
or applet is preinstalled; install an upstream CAP separately with GPPro.

Flashing either image clears persistent card state. Use BOOTSEL and copy only
the selected UF2 to the Pico 2 `RPI-RP2` drive.

| Profile | Capabilities enabled | File | Size | SHA-256 |
| --- | ---: | --- | ---: | --- |
| `max_crypto` | 147 / 150 | `pico2-jcvm-max_crypto-arm-empty-card-current.uf2` | 4,025,856 bytes | `09d0a1ee76b3512314795eea796148141fc2e22fb7d7a28d21c97045aee61d15` |
| `symm_crypto` | 46 / 150 | `pico2-jcvm-symm_crypto-arm-empty-card-current.uf2` | 3,837,440 bytes | `40032e23306d73d4c095659b41ecb9b90441c447accaf870a88048b2bc501571` |

Both images passed `picotool info -a` as RP2350 ARM Secure UF2s. A block-map
check confirmed unique addresses, consistent block numbering, and 4,096
storage blocks filled with `0xFF` at `0x10100000`–`0x101FFFFF`. Firmware-only
ends are `0x100EB518` (`max_crypto`) and `0x100D45A8` (`symm_crypto`). The current `max_crypto` image was rebuilt with HOST-02 normal startup, passed
ARM and RISC-V wrapper builds, and passed a physical ARM runtime cycle plus
read-only GPPro SELECT/CPLC/Card Data checks on Pico 2
`D92E743D5F640C0F`. The packaged UF2 was validated as an RP2350 ARM Secure
image with all 4,096 storage blocks erased. `symm_crypto` remains the earlier
artifact and was not revalidated in this step. Files are local release assets
and have not been uploaded to GitHub.

Rebuild with:

```sh
PICO2_DEBUG_SERIAL_WAIT_MS=0 ./tools/build arm --profile max_crypto
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 \
  artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2
PICO2_DEBUG_SERIAL_WAIT_MS=0 ./tools/build arm --profile symm_crypto
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 \
  artifacts/firmware/pico2-jcvm-symm_crypto-arm-empty-card-current.uf2
```
