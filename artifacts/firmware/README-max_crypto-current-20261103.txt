Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Refreshed: 2026-11-04, after RV-18 checked array-size changes.

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,010,496 bytes
  SHA-256: b30953d6e4ce191638150af647992089a14f6d99f5bb3c167bd31574f8cb49fd

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive erases existing
CAP packages and persistent filesystem data. This is destructive to stored
card data. No applet is installed; install the unchanged upstream CAP with
GPPro after confirming the card is empty.

The firmware-only build output is:

  build/arm/pico2_choupi.uf2
  Size: 1,913,344 bytes
  SHA-256: 32bdc2bb486a05ef8730b47cbafa4b0662b16bb686684b5fd2dc4e493f6f75fb

The firmware image ends at 0x100E9764, leaving 92,316 bytes before the
0x10100000 firmware boundary. PC, ARM and RISC-V max_crypto builds passed.
The JC array regression suite passed, including int-array and persistent
record-length boundaries. The image has not been flashed or runtime-tested on
a physical token.

For a less destructive firmware update, copy only `pico2_choupi.uf2`; it does
not clear CAPs already stored on the card.

Install an applet separately with GlobalPlatformPro. The upstream CAP remains
unchanged. For example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
