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
Image size: 2,089,472 bytes
Firmware SHA-256: 03ccac6577950f4e280a1a10d3c52b8d15d03ab52165b220a59a0a9a75e4e7df

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

Build validation: `PICO2_DEBUG=0 ./tools/build arm --profile max_crypto` and
the corresponding RISC-V wrapper build passed. The ARM image was flashed to
Pico 2 `2BED26ED20556535` and passed its finite runtime monitor. Host overlay
snapshot/migration tests pass. The full physical API Security retest stopped
when the reader disappeared, so storage-related `NO_MEMORY` reduction is not
yet confirmed. The RISC-V wrapper builds the VM core; only the ARM output is a
complete CHOUPI firmware image.
