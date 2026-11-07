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
