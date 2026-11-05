Current max_crypto firmware for Raspberry Pi Pico 2 / RP2350
Updated: 2026-11-05

## Empty-card image for Windows

Flash this combined UF2 in BOOTSEL mode:

  artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2

Size: 4,026,368 bytes
SHA-256: 0868a0ec81b13bd5a5a7c9e42f2ac64fe061ade69d12ac93bded6aa852ec5873

It contains the current ARM `max_crypto` firmware and 4,096 erased storage
blocks covering `0x10100000`–`0x101FFFFF`. Flashing it removes stored CAPs and
persistent card data. No applet is preinstalled; upload an unchanged upstream
CAP separately with GlobalPlatformPro.

The firmware-only UF2 is `build/arm/pico2_choupi.uf2` (1,929,216 bytes,
SHA-256 `e520925668f07bd722f20643e462e558fb8eee7021d6c1d6498c611cddeb6360`).
Firmware ends at `0x100EB604`, leaving 84,476 bytes before the storage region.
Use the firmware-only image only when preserving existing card data.

Profile `max_crypto` enables 147 of 150 declared JCAlgTest capabilities,
including RSA and ECC. Both ARM and RISC-V wrappers passed on 2026-11-05;
`picotool info -a` identifies the combined artifact as RP2350 ARM Secure, and
the UF2 map check confirms 7,863 unique data blocks and all 4,096 storage
blocks contain `0xFF`. This ARM image was also flashed to Pico 2
`D92E743D5F640C0F`; finite serial monitoring passed with
`TEST:SUMMARY pass=1 fail=0` and no `FAULT`.

Regenerate after `./tools/build arm --profile max_crypto` with:

  ./tools/package-empty-card-uf2
