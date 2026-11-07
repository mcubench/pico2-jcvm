# Pico 2 ARM crypto release images

These RP2350 ARM Secure UF2 files are prepared as release assets. Each image
contains the firmware and a full 1 MiB of erased card storage. No CAP package
or applet is preinstalled; install an upstream CAP separately with GPPro.

Flashing either image clears persistent card state. Use BOOTSEL and copy only
the selected UF2 to the Pico 2 `RPI-RP2` drive.

| Profile | Capabilities enabled | File | Size | SHA-256 |
| --- | ---: | --- | ---: | --- |
| `max_crypto` | 147 / 162 | `pico2-jcvm-max_crypto-arm-empty-card-current.uf2` | 4,048,896 bytes | `988a148d72f624ba8727f99ae4bd2308a2b03c8ae8065bf95951adbd2bfc51c0` |
| `symm_crypto` | 46 / 150 | `pico2-jcvm-symm_crypto-arm-empty-card-current.uf2` | 3,837,440 bytes | `40032e23306d73d4c095659b41ecb9b90441c447accaf870a88048b2bc501571` |

Both images are RP2350 ARM Secure UF2s. A block-map check confirmed unique
addresses, consistent block numbering, and 4,096 storage blocks filled with
`0xFF` at `0x10100000`–`0x101FFFFF`. The current `max_crypto` firmware ends
at `0x100EE230` (73,168 bytes remain before the storage partition); its UF2
contains 3,811 firmware blocks plus the 4,096 erased storage blocks. The card
storage is erased and no CAP or applet is preinstalled. This build includes the
currently validated max_crypto set; HMAC is deliberately disabled pending
completion of HMAC-01's native runtime validation. The image was rebuilt for
this worktree and has not been flashed or tested on hardware. `symm_crypto`
remains the earlier artifact and was not revalidated in this step. Files are
local release assets and have not been uploaded to GitHub.

Rebuild with:

```sh
PICO2_DEBUG_SERIAL_WAIT_MS=0 ./tools/build arm --profile max_crypto
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 \
  artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2
PICO2_DEBUG_SERIAL_WAIT_MS=0 ./tools/build arm --profile symm_crypto
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 \
  artifacts/firmware/pico2-jcvm-symm_crypto-arm-empty-card-current.uf2
```
