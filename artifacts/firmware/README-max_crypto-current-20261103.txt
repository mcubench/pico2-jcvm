Current max_crypto ARM firmware for Raspberry Pi Pico 2 / RP2350

Refreshed: 2026-11-04, after RV-20 defined arithmetic and shift fixes.

Profile: max_crypto (147/150 declared JCAlgTest capabilities enabled,
including RSA and ECC). No applet or CAP is bundled.

For Windows testing with a genuinely empty card, flash this combined UF2:

  build/arm/pico2_choupi_max_crypto_empty_card.uf2
  Size: 4,010,496 bytes
  SHA-256: b1543eba771770db6e671351122ddaee810c23803034a3b8482197cedf58f0ef

It contains the current ARM firmware plus 1 MiB of 0xFF blocks covering
0x10100000–0x101FFFFF. Copying it to the Pico 2 BOOTSEL drive erases existing
CAP packages and persistent filesystem data. This is destructive to stored
card data. No applet is installed; install the unchanged upstream CAP with
GPPro after confirming the card is empty.

The firmware-only build output is:

  build/arm/pico2_choupi.uf2
  Size: 1,913,344 bytes
  SHA-256: 1506bd0429e6413ef78bf2e3f43942ab0ed1a896d8ba85c48ba7e4e84cff28b6

The firmware image ends at 0x100E97E4, leaving 92,188 bytes before the
0x10100000 firmware boundary. PC, ARM and RISC-V max_crypto builds passed.
The JC array regression suite passed, including int-array and persistent
record-length boundaries. The RV-20 VPCD bytecode probe passed wrapping
arithmetic, increment, shifts, signed division and remainder checks. The
combined UF2 was verified to contain the current firmware blocks and 4,096
all-0xFF storage blocks. The image has not been flashed or runtime-tested on
a physical token.

For a less destructive firmware update, copy only `pico2_choupi.uf2`; it does
not clear CAPs already stored on the card.

Install an applet separately with GlobalPlatformPro. The upstream CAP remains
unchanged. For example:

  gp --reader "Pico2 JCVM CCID 0" --install /path/to/AlgTest_v1.8.4_jc305.cap
