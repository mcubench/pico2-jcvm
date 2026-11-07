# External validation requests

## 2026-11-07 Verify REL-01 CPLC values on Windows 4 minutes

- Step/experiment: `REL-01-windows-gppro-cplc-display`
- When: '2026-11-07 17:57 CET' - '2026-11-07 18:01 CET'
- Change: Prepare an external check for the physical CPLC release and build-source fields, since this VM currently sees the three tokens as runtime USB CCID devices but PC/SC lists no readers.
- Validation: The current ARM/RISC-V max_crypto builds passed. `./tools/test-gp-get-data` passed 25/25. No GPPro hardware query could run in this VM because `pcsc_scan -r` listed no reader.
- Result: Image to test: `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`, SHA-256 `9c40b757b67eb8dbada757edcf4355fd43e7c917f93a65b52e70a17bf6941505`; profile max_crypto; empty card; release 0.1.1; expected CPLC `OperatingSystemReleaseLevel=0101` and `ICPersonalizationEquipmentID=FFDCFCC7`.
- Decision: On Windows, flash this image via BOOTSEL, then run `gp --info -d`. Confirm the displayed release level and equipment ID match the expected values and send the output. The empty-card image erases installed applets and persistent data.
- Commit: `1d176d71`
- Artifacts/logs: Firmware path and SHA-256 above; no physical log yet.

## 2026-11-07 Verify refreshed REL-01 max_crypto image externally 3 minutes

- Step/experiment: `REL-01-refreshed-max_crypto-empty-card-image`
- When: '2026-11-07 21:27 CET' - '2026-11-07 21:29 CET'
- Change: Rebuilt both architectures and repackaged the current ARM max_crypto firmware with erased applet/storage area for Windows external validation.
- Validation: `./tools/test-gp-get-data` passed 25/25; ARM and RISC-V max_crypto wrappers passed; packaging passed with 3,854 firmware blocks and 4,096 erased storage blocks; picotool identifies RP2350 ARM Secure. A refreshed authorized PC/SC scan timed out after 8 seconds without a reader, so physical GPPro validation remains open.
- Result: Image `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`, 4,070,912 bytes, SHA-256 `93f020578c53fae98c78233f5f511ee566c603f21548cd05742bc28e19fda618`; release `0.1.1` (`0101`); CPLC source ID `FF1DC424`; empty card.
- Decision: Flash this image via BOOTSEL for external testing, then run `gp --info -d`. Confirm release level `0101` and equipment ID `FF1DC424`; report the output to close physical REL-01 acceptance. The empty-card image erases installed applets and persistent data.
- Commit: `de960d02`
- Artifacts/logs: Firmware path and SHA-256 above; no physical log.
