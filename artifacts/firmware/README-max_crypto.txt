Pico 2 JCVM max_crypto ARM firmware

Firmware: pico2-jcvm-max_crypto-arm-empty-card.uf2
Target: Raspberry Pi Pico 2 / RP2350, ARM Secure
Profile: max_crypto (147 of 150 declared JCAlgTest capabilities enabled)
Disabled capabilities: EC FP private key size 112 is outside the selected
JCOP4 reference set; RSA and RSA-CRT KeyPair generation at 4096 bits is not
supported. The unchanged JCAlgTest CAP has two KeyPair rows labeled 3072 that
actually request 4096; independent physical 3072-bit generation vectors pass.
Applet state: no applet is bundled in the firmware image. Reflashing firmware
does not erase the card's separate persistent CAP store; use a fresh card or
remove installed applets explicitly with GlobalPlatformPro.
Debug persistent-GC instrumentation: disabled.
Filesystem: snapshot v4, with copy-on-write migration from v2/v3 snapshots.
Image size: 2,098,176 bytes
Firmware SHA-256: 122a172ca7a412881afaa2529f1c1c7c4a782c3adb9f91ca436f5545ca071c2c
Build date: 2026-11-01

Flash
1. Hold BOOTSEL while connecting the Pico 2 over USB.
2. Copy pico2-jcvm-max_crypto-arm-empty-card.uf2 to the RPI-RP2 drive.
3. Wait for the Pico to reboot and enumerate as a PC/SC reader.
4. Load and install an applet separately with GlobalPlatformPro, for example:
     gp --install /path/to/AlgTest_JavaCard.cap
5. Run the unchanged JCAlgTest 1.8.2 client against that reader.

The expected JCAlgTest support manifest is
pico2-jcvm-max_crypto-arm-supported.csv
SHA-256: 81824f2bdde2f48f39cf3a3e3da942bf89a81317f32f218a8955dce8b455dee0

Build validation for this image: `./tools/build arm --profile max_crypto`,
`./tools/build riscv --profile max_crypto`, and
`./tools/test-rp2350-fs-overlay` passed. The overlay regression includes
repeated dynamic-package deletion/restart and randomized power-cut recovery.
The build used the current worktree, including pre-existing local changes in
`src/cap_load.cpp`; those source changes were not included in the artifact
refresh commit, so this image is not reproducible from a clean checkout of that
commit alone.
This refreshed image has not been flashed or runtime-tested on hardware; the
authorized reader helper reported no attached Pico 2 devices. Previous
physical/API results in the project logs refer to earlier image hashes and do
not validate this image. The RISC-V wrapper builds the VM core; only the ARM
output is a complete CHOUPI firmware image.
