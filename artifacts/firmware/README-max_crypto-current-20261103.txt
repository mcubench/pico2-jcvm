Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Refreshed: 2026-11-04, after RV-19 signed division fix.

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,010,496 bytes
  SHA-256: ee1978b2748dcc10f1bd5ec24bfe6d2938441aa8653e26435c878f27eca5cf21

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive erases existing
CAP packages and persistent filesystem data. This is destructive to stored
card data. No applet is installed; install the unchanged upstream CAP with
GPPro after confirming the card is empty.

The firmware-only build output is:

  build/arm/pico2_choupi.uf2
  Size: 1,913,344 bytes
  SHA-256: 6bc3ec2b0d379a6a4f8d6d37c2fc27bcfcdf269a1e3a46aaf675adadfd1be99c

The firmware image ends at 0x100E9764, leaving 92,316 bytes before the
0x10100000 firmware boundary. PC, ARM and RISC-V max_crypto builds passed.
The JC array regression suite passed, including int-array and persistent
record-length boundaries. The RV-19 VPCD bytecode probe passed its signed
division, remainder and divide-by-zero checks. The image has not been flashed
or runtime-tested on a physical token.

For a less destructive firmware update, copy only `pico2_choupi.uf2`; it does
not clear CAPs already stored on the card.

Install an applet separately with GlobalPlatformPro. The upstream CAP remains
unchanged. For example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
