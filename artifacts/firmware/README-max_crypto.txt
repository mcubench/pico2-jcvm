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
Image size: 1,887,232 bytes
Firmware SHA-256: 029f4a7cc1dd38d0f77fc52cc733f90b7599e01606f934068318fd2a4a505d62
Build date: 2026-11-02
Included update: native Signature MAC now honors the IV supplied to
`Signature.init(key, mode, parameters, ...)`; deterministic nonzero-IV VPCD
vectors and the three matching upstream API calls pass. See the IV validation
record in `progress.md`.

Flash layout: the image ends at `0x100e648c`, 105,332 bytes below the reserved
1 MiB firmware boundary at `0x10100000`. The RP2350 has 2 MiB total flash; CAP,
filesystem, metadata, and legacy regions occupy the rest. The UF2 file size
includes transfer block overhead and is not the amount written contiguously.
The build wrapper checks this boundary and fails on overlap.

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

Build validation for this image: `./tools/build arm --profile max_crypto` and
`./tools/build riscv --profile max_crypto` passed. The overlay regression
includes repeated dynamic-package deletion/restart and randomized power-cut
recovery; it passed before this rebuild, with no overlay source changes since.
The build used the current worktree, including pre-existing local changes in
`src/cap_load.cpp`; those source changes were not included in the artifact
refresh commit, so this image is not reproducible from a clean checkout of that
commit alone.
This size-fit candidate has not been flashed or runtime/performance-tested on
hardware. Previous physical/API results refer to earlier image hashes and do
not validate this image. It was built from the current shared worktree,
including uncommitted source changes in `src/cap_load.cpp`; it is not
reproducible from a clean checkout of the artifact commit alone. The RISC-V
wrapper builds the VM core; only the ARM output is a complete CHOUPI firmware
image.
