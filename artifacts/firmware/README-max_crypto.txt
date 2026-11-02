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
Image size: 1,900,032 bytes
Firmware SHA-256: 7456bf4826c0aa9578e989992f566fd58878c8f212d06ed23e9b27ac756b114d
Build date: 2026-11-02
Included update: native Signature MAC now honors the IV supplied to
`Signature.init(key, mode, parameters, ...)`; deterministic nonzero-IV VPCD
vectors and the three matching upstream API calls pass. See the IV validation
record in `progress.md`.

Flash layout: the image ends at `0x100e7df8`, 98,824 bytes below the reserved
1 MiB firmware boundary at `0x10100000`. The RP2350 has 2 MiB total flash; CAP,
filesystem, metadata, and legacy regions occupy the rest. The UF2 file size
includes transfer block overhead and is not the amount written contiguously.
The build wrapper checks this boundary and fails on overlap. The native static-field reads return Java Card default zero when no persistent value exists; same-CAP sibling static-state VPCD coverage passes. This firmware update has not been validated on a physical board.

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
`./tools/build riscv --profile max_crypto` both passed on 2026-11-02. The ARM
linker map places the image end at `0x100e7df8`, leaving 98,824 bytes in the
1 MiB firmware window. This refreshed build has not been flashed or runtime-
tested on physical hardware; earlier physical/API results refer to prior image
hashes. The image was built from the current shared worktree, including local
changes in `src/cap_load.cpp`, so it is not reproducible from a clean checkout
until those changes are integrated. The RISC-V wrapper builds the VM core; only
the ARM output is a complete CHOUPI firmware image.
