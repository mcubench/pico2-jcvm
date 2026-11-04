Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,009,472 bytes
  SHA-256: 7f7538c0b83faf826682848f39d4148cb4530f2fc11c95731f52bba33e03357a

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive erases existing
CAP packages and persistent filesystem data. This is destructive to stored
card data. No applet is installed; install the unchanged upstream CAP with
GPPro after confirming the card is empty.

The firmware-only build output is:

  build/arm/pico2_choupi.uf2
  Size: 1,912,320 bytes
  SHA-256: 34e95e0e1504add5130f5a0d7d11c102f939708ce3fa82699e33e7d6d7a8680e

The firmware image ends at 0x100E9590, leaving 92,784 bytes before the
0x10100000 firmware boundary. ARM and RISC-V max_crypto builds passed; the
symmetric PC VPCD suite passed 99/99 phases, including AES/DES CBC lifecycle
vectors. The image has not been flashed or runtime-tested on a physical token.

For a less destructive firmware update, copy only `pico2_choupi.uf2`; it does
not clear CAPs already stored on the card.

Install an applet separately with GlobalPlatformPro. The upstream CAP remains
unchanged. For example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
