Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Refreshed: 2026-11-04, after RV-22 fixed inherited interface checks.

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,012,544 bytes
  SHA-256: 178efc20a53ab7fea314e0251bcbb05c8f84deefe2bed537d57b15764907368d

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive clears existing
CAP packages and persistent filesystem data. No applet is installed; install
the unchanged upstream CAP with GPPro after confirming the card is empty.

Firmware-only build output:

  build/arm/pico2_choupi.uf2
  Size: 1,915,392 bytes
  SHA-256: 0cc13ca28be022152134a2c66e8f23a916bce59c6917ea144c3f71b430dc70b1

The firmware image ends at 0x100E9BEC, leaving 91,156 bytes before the
0x10100000 firmware boundary. PC, ARM and RISC-V max_crypto builds passed.
The full cross-AID SIO VPCD regression passed 34/34 cases, including exact
exception class/reason propagation and inherited-interface `checkcast` and
`instanceof` through same-package and imported superclasses. The combined UF2 was
verified to contain the current firmware blocks and 4,096 all-0xFF storage
blocks. It has not been flashed or runtime-tested on a physical token.

For a firmware-only update that preserves stored card data, use
`build/arm/pico2_choupi.uf2` instead. To install an applet separately, use
GlobalPlatformPro with an unchanged upstream CAP, for example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
