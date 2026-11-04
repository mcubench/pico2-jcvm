Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Refreshed: 2026-11-04, after RV-23 added native prevalidation for
`Util.arrayCopyNonAtomic` and `Util.arrayFillNonAtomic`.

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,014,592 bytes
  SHA-256: 041144a8a5116111aef9f4e501f87355e90a4e1e9731cdf7969a75da9ac2e328

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive clears existing
CAP packages and persistent filesystem data. No applet is installed; install
the unchanged upstream CAP with GPPro after confirming the card is empty.

Firmware-only build output:

  build/arm/pico2_choupi.uf2
  Size: 1,917,440 bytes
  SHA-256: 803997081387ed53fa4fad5f4afafa1d93447666e5b6341d886838e0fa6f3d7e

The firmware image ends at 0x100E9F08, leaving 90,360 bytes before the
0x10100000 firmware boundary. PC, ARM and RISC-V max_crypto builds passed.
The focused cross-AID VPCD Util test passed invalid negative and out-of-range
copy/fill cases, null references at zero length, valid zero-length boundary
operations, valid overlapping copy, exact exception catches, unchanged arrays
after failure, and unassigned `SensitiveResult` after failure. PC, ARM and
RISC-V max_crypto builds passed. The combined UF2 was verified to contain the
current firmware blocks and 4,096 all-0xFF storage blocks. It has not been
flashed or runtime-tested on a physical token.

The full cross-AID SIO VPCD regression also passed 34/34 checks. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261104T163909Z.log` (focused Util test)
and `artifacts/logs/sio-cross-aid-vpcd-20261104T163028Z.log` (full regression).

For a firmware-only update that preserves stored card data, use
`build/arm/pico2_choupi.uf2` instead. To install an applet separately, use
GlobalPlatformPro with an unchanged upstream CAP, for example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
