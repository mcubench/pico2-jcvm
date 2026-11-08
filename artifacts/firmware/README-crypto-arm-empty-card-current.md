# Pico 2 ARM crypto release images

These RP2350 ARM Secure UF2 files are prepared as release assets. Each image
contains the firmware and a full 1 MiB of erased card storage. No CAP package
or applet is preinstalled; install an upstream CAP separately with GPPro.

Flashing either image clears persistent card state. Use BOOTSEL and copy only
the selected UF2 to the Pico 2 `RPI-RP2` drive.

| Profile | Capabilities enabled | File | Size | SHA-256 |
| --- | ---: | --- | ---: | --- |
| `max_crypto` | 159 / 162 | `pico2-jcvm-max_crypto-arm-empty-card-current.uf2` | 4,079,616 bytes | `b8e6829d8d102d57eba95e0af912e7e452afcf859510bc3023a84abe6f177d3c` |
| `symm_crypto` | 46 / 150 | `pico2-jcvm-symm_crypto-arm-empty-card-current.uf2` | 3,837,440 bytes | `40032e23306d73d4c095659b41ecb9b90441c447accaf870a88048b2bc501571` |

Both images are RP2350 ARM Secure UF2s. The current `max_crypto` image has
4,096 erased storage blocks at `0x10100000`–`0x101FFFFF`; no CAP or applet is
preinstalled. It contains 3,871 firmware blocks, ends at `0x100F1EEC`, and
leaves 57,620 bytes before the storage partition. It embeds release `0.1.1`
(`0101`) and CPLC build marker `FF2EB31F` (dirty worktree, based on HEAD
`2EB31F`). Its underlying firmware UF2 SHA-256 is
`a308b2f24bcc41d590a455991990dff014f45cd5c07de5d661cc859b0468b634`; the
empty-card package SHA-256 is shown in the table. Its 159/162 enabled
capabilities include six native HMAC algorithms. The ARM and RISC-V wrapper
builds passed for this source revision. All six HMAC RFC known-answer tests pass
in the VPCD fixture. The complete JCAlgTest support sweep has not
completed: its client stalls during the DES portion and the emulator logs
`APPLET_PROCESS status=F206 exception=16`; therefore full support acceptance
and physical-hardware validation remain open. This image was rebuilt from the
current worktree and has not been flashed. `symm_crypto` is the earlier
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
