Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Refreshed: 2026-11-04, after RV-21 fixed SIO exception propagation.

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,012,544 bytes
  SHA-256: 088987ee16747f2f7f048e23444ffcc559a3ef01dfad854356d6b9d656cd8f99

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive clears existing
CAP packages and persistent filesystem data. No applet is installed; install
the unchanged upstream CAP with GPPro after confirming the card is empty.

Firmware-only build output:

  build/arm/pico2_choupi.uf2
  Size: 1,915,392 bytes
  SHA-256: 398a73319dc924d8ce5508bf3baee5dc11ed2454cf5052b776f4c63edba29cd3

The firmware image ends at 0x100E9B0C, leaving 91,380 bytes before the
0x10100000 firmware boundary. PC, ARM and RISC-V max_crypto builds passed.
The SIO VPCD regression passed 33/33 cases, including exact exception class
and reason propagation and a successful follow-up call. The combined UF2 was
verified to contain the current firmware blocks and 4,096 all-0xFF storage
blocks. It has not been flashed or runtime-tested on a physical token.

For a firmware-only update that preserves stored card data, use
`build/arm/pico2_choupi.uf2` instead. To install an applet separately, use
GlobalPlatformPro with an unchanged upstream CAP, for example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
