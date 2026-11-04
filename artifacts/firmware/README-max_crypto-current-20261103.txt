Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,009,472 bytes
  SHA-256: 247b0d30bd2c0dab5fdfd5f4360a086932e5868ac82deb1993bfa26b624a5c5d

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive erases existing
CAP packages and persistent filesystem data. This is destructive to stored
card data. No applet is installed; install the unchanged upstream CAP with
GPPro after confirming the card is empty.

The firmware-only build output is:

  build/arm/pico2_choupi.uf2
  Size: 1,912,320 bytes
  SHA-256: 5087cb3352b61752f9c4e68a25aeb6f8ac3f59d700987a7bd3d1e04e6aec3bfc

The firmware image ends at 0x100E9564, leaving 92,828 bytes before the
0x10100000 firmware boundary. Both ARM and RISC-V max_crypto builds passed;
the PC VPCD regression passed 107/107 phases. The image has not been flashed
or runtime-tested on a physical token.

For a less destructive firmware update, copy only `pico2_choupi.uf2`; it does
not clear CAPs already stored on the card.

Install an applet separately with GlobalPlatformPro. The upstream CAP remains
unchanged. For example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
