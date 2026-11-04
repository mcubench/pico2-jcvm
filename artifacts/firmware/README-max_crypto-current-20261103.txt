Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Refreshed: 2026-11-04, after RV-25 added exclusive CCID APDU routing,
explicit ISD selection, SCP failure gating, and inactive-slot rejection.

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,015,616 bytes
  SHA-256: 6a4e25626a465b125ecd89a40a442c78a8111984f9876c1685acce57c6c2e50e

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive clears existing
CAP packages and persistent filesystem data. No applet is installed; install
the unchanged upstream CAP with GPPro after confirming the card is empty.

Firmware-only build output:

  build/arm/pico2_choupi.uf2
  Size: 1,918,464 bytes
  SHA-256: 689bbc0985df4e3096012c42cbabe3c7198199109f0c9f70acfbf80c909565e7

The firmware image ends at 0x100EA1D4, leaving 89,644 bytes before the
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
passed after RV-25 source changes. The production CCID path has not yet been
exercised at runtime on physical hardware. VPCD validation log for RV-24:
`artifacts/logs/rv24-nonatomic-vpcd-20261104.log`.

The full cross-AID SIO VPCD regression also passed 34/34 checks. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261104T163909Z.log` (focused Util test)
and `artifacts/logs/sio-cross-aid-vpcd-20261104T163028Z.log` (full regression).

For a firmware-only update that preserves stored card data, use
`build/arm/pico2_choupi.uf2` instead. To install an applet separately, use
GlobalPlatformPro with an unchanged upstream CAP, for example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
