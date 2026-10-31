Pico 2 JCVM max_crypto ARM firmware

Firmware: pico2-jcvm-max_crypto-arm-empty-card.uf2
Target: Raspberry Pi Pico 2 / RP2350, ARM Secure
Profile: max_crypto (147 of 150 declared JCAlgTest capabilities enabled)
Card state: empty; no applet is preinstalled.
Debug persistent-GC instrumentation: disabled.
Image size: 2,088,448 bytes
Firmware SHA-256: cf21bae06443f3b7e0e8d6150eb2927cbaecc908fe6cca19fd435791e8d4d2f2

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

Build validation: `./tools/build arm --profile max_crypto` and
`./tools/build riscv --profile max_crypto` passed. The RISC-V wrapper builds
the VM core; only the ARM output is a complete CHOUPI firmware image.
