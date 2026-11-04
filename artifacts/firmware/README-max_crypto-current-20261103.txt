Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Refreshed: 2026-11-04, after RV-26 added retained CCID final responses,
bounded nonblocking IN retry, and Bulk-OUT rearm retry.

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,016,640 bytes
  SHA-256: 7112f8f52a14ef60027904648338f85c7e7dee8e02739c9a52de8046fa6b5d2e

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive clears existing
CAP packages and persistent filesystem data. No applet is installed; install
the unchanged upstream CAP with GPPro after confirming the card is empty.

Firmware-only build output:

  build/arm/pico2_choupi.uf2
  Size: 1,919,488 bytes
  SHA-256: 692c1fb42f8c7dd2aa92a37b6b23aa999f93c657eeb7144f63e66e2cbd72dbed

The firmware image ends at 0x100EA3BC, leaving 89,156 bytes before the
0x10100000 firmware boundary. PC, ARM and RISC-V max_crypto builds passed.
Focused VPCD tests passed 7/7: capacity overflow is catchable and leaves data
unchanged; non-atomic copy on a record larger than commit capacity does not
consume capacity and survives abort; mixed atomic/non-atomic updates on the
same array preserve the non-atomic value while atomic bytes roll back; updates
to separate arrays behave likewise; and an atomic write after a non-atomic
write rolls back to that non-atomic baseline. The combined UF2 was verified by
picotool as RP2350 ARM Secure and contains the current firmware blocks plus
4,096 all-0xFF storage blocks. It has not been flashed or runtime-tested on a
physical token. The CCID route unit test passed 12/12, and ARM/RISC-V builds
passed after RV-25 source changes. RV-26's CCID flow test passed 15/15 and
ARM/RISC-V builds passed after its source changes. The production CCID path
has not yet been exercised at runtime on physical hardware. `pcsc_scan -r`
currently lists only Virtual PCD readers, even though USB briefly enumerated
the Pico 2 CCID identity. VPCD validation log for RV-24:
`artifacts/logs/rv24-nonatomic-vpcd-20261104.log`.

The full cross-AID SIO VPCD regression also passed 34/34 checks. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261104T163909Z.log` (focused Util test)
and `artifacts/logs/sio-cross-aid-vpcd-20261104T163028Z.log` (full regression).

For a firmware-only update that preserves stored card data, use
`build/arm/pico2_choupi.uf2` instead. To install an applet separately, use
GlobalPlatformPro with an unchanged upstream CAP, for example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
