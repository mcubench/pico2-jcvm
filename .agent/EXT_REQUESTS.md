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
- Decision: Physical REL-01 acceptance is now confirmed on Linux with reader D92; release level `0101` and equipment ID `FF1DC424` match. This image remains available for separate Windows external testing. It is an empty-card image and erases installed applets and persistent data when flashed.
- Commit: `de960d02`
- Artifacts/logs: Firmware path and SHA-256 above; physical logs `artifacts/logs/rel-01-monitor-D92-20261107.log` and `artifacts/logs/rel-01-gppro-physical-D92-20261107.log`.

## 2026-11-09 Refresh Pico-only PC/SC access helper for RESOURCE-01 physical test 8 minutes

- Step/experiment: `RESOURCE-01-refresh-pcsc-helper`
- When: '2026-11-09 00:21 CET' - '2026-11-09 00:29 CET'
- Change: Prepare the host access path for physical reclaimer validation. The
  repository helper filters by USB VID/PID and exact Pico2 manufacturer/product
  strings; the installed `/usr/local/sbin/pico2-pcsc-reader` has a different
  SHA-256 and is stale. Do not execute the stale helper because it may include
  the user's unexpected PacketRF device.
- Validation: Current worktree `./tools/build arm --profile max_crypto` and
  `./tools/build riscv --profile max_crypto` passed. Diagnostic builds with
  `PICO2_DEBUG_PERSISTENT_GC=1` also passed for both wrappers. Prepared ARM
  test image: `build/arm/pico2_choupi_persistent_gc_test.uf2`, 1,986,560 bytes,
  SHA-256 `b1098c8e0ad7e07761e0d42bce24f65bc3bc451b42a07d42f0037cce4d27ebc8`;
  ELF contains the `CHOUPI:PERSISTENT_GC` telemetry marker. `visudo` is present
  at the installer's expected `/usr/sbin/visudo` path.
- Result: Safe physical access is not ready until the installed helper is
  replaced. No USB/PCSC scan or physical device operation was attempted.
- Decision: From repository root, run `sudo ./tools/install-pcsc-sudoers`, then
  report completion. After that I can invoke the refreshed allowlisted helper,
  select an identified Pico2 reader by chip ID, flash only that board with the
  diagnostic image, and run `tools/test-persistent-reclaimer-physical`. The
  unexpected PacketRF device is excluded.
- Commit: `662aff8c`
- Artifacts/logs: Diagnostic image path/hash above; no hardware log yet.

## 2026-11-09 Extend the physical reclaimer probe to RSA-4096 replacements 9 minutes

- Step/experiment: `RESOURCE-01-PHYSICAL-RSA-RECLAIMER-PROBE`
- When: '2026-11-09 00:29 CET' - '2026-11-09 00:38 CET'
- Change: Extended the local validation applet with a reference-clear command
  that does not call the unsupported deletion API. Extended the host-side
  SmartCardIO probe and physical wrapper to run 1..16 repeated RSA key
  replacements at 512/1024/2048/4096 bits. The diagnostic firmware's post-APDU
  reclaimer should clear the previous graph after each replacement. The
  wrapper now requires at least one nonempty reclaim callback per replacement
  cycle and bounds the serial monitor and APDU test.
- Validation: `./tools/build-persistent-reclaimer-probe` and host `javac`
  passed; `bash -n tools/test-persistent-reclaimer-physical` passed. Guarded
  VPCD `./tools/test-object-deletion-vpcd 4 rsa-public 4096` passed 11/11 and
  erased 40 owner tags. This host probe uses the separate request-gated PC
  harness; no USB or physical device was accessed.
- Result: The physical harness is ready to test repeated 4096-bit RSA
  replacement on an identified Pico2, but still awaits refresh of the
  Pico-only sudo helper described above.
- Decision: Once the helper is refreshed, use
  `PICO2_HOST_USB_ACCESS=1 ./tools/test-persistent-reclaimer-physical '<exact Pico2 reader name with chip ID>' 8 4096`.
  Flash only `build/arm/pico2_choupi_persistent_gc_test.uf2`; never select the
  unexpected PacketRF device.
- Commit: `bb3cb62c`
- Artifacts/logs: VPCD control
  `artifacts/logs/object-deletion-vpcd-20261108T233722Z.log`; physical result
  pending.

## Windows validation: fresh max_crypto empty-card image

Please validate the current RP2350 ARM `max_crypto` firmware on a Windows host.
The image has no pre-installed applet and clears the card's persistent storage
region; install the upstream JCAlgTest CAP explicitly during the test.

- [Download the complete Windows test package](../build/external-test/pico2-jcvm-max_crypto-windows-test.zip)
  (includes firmware, GPPro, pinned JCAlgTest 1.8.4/jc305 CAP and matching
  client, expected support CSV, comparator, and run sheet).
- [Download the standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2)
- Package SHA-256: `c872e23383daa298212caefafa24f3c860a3c1ea9c18e0a6b2f8db44124da804`
- UF2 SHA-256: `5f0b8383261b511c11c23f5cd9f96c93948fd66616527dc5f2fd48e733418502`
- UF2 size: 4,085,248 bytes (UF2 transport blocks include firmware and a full
  erased persistent-storage region).
- Build provenance: profile `max_crypto`, ARM `rp2350-arm-s`, serial wait
  `0 ms`, CPLC source ID `0xFF393005`; firmware end `0x100F2974`, leaving
  54,924 bytes before the configured flash limit.

The package [README](../build/external-test/max_crypto-package/README.md)
contains the complete PowerShell steps. In brief:

1. Hold BOOTSEL while connecting the Pico 2 and copy the packaged UF2 to the
   `RPI-RP2` drive.
2. Use `Get-Service SCardSvr` and `certutil -scinfo` to confirm Windows sees
   `Pico2 JCVM CCID`; send the exact reader name and ATR. If `certutil` reports
   a card-provider lookup error, include it and continue with the GPPro check.
3. Run `java -jar .\tools\gp.jar --reader $reader --list` and confirm there
   are no `APP:`/`PKG:` entries, then explicitly install
   `AlgTest_v1.8.4_jc305.cap` using the package's GPPro command.
4. Run the included `AlgTestJClient.jar` for `ALG_SUPPORT_EXTENDED`, then run
   `compare-jcalgtest-support.py` against the included max_crypto expected CSV.

Please return the Windows version, exact reader name/ATR, before/after GPPro
listings, JCAlgTest CSV, and comparator output. The CAP remains the unchanged
pinned upstream fixture; do not modify it.

## Test JCSystem object deletion on Windows

Please run the diagnostic package at
`build/external-test/pico2-jcvm-object-deletion-windows.zip` using the steps in
its `README.md`. The image is built with `PICO2_ENABLE_OBJECT_DELETION=1` and
is intentionally diagnostic; it exercises the unchanged upstream JCAlgTest
1.8.4 / jc305 `INS_CARD_RESET` request path. Capture the Pico CDC serial stream
with the included PowerShell script while running the interactive client
option `2 -> PERFORMANCE TEST` (1–3 hours) on one physical Pico and
`1 -> All algorithms` (5–8 hours) on a different physical Pico when possible.
Install the unchanged CAP on both tokens and keep their result folders, reader
names, chip IDs, and serial logs separate.

Return the full result folders and `object-deletion-serial.log`, including
partial output if the client or reader stops responding. In particular,
preserve `OBJECT_DELETION:` lines, any `6F00`/`F205`, and PC/SC service errors.
Do not use this image for production; normal builds still keep deletion off.

## Reattach Pico USB devices to the development VM

The VM lost Pico `2BED26ED20556535` during the software BOOTSEL reset used for
the local physical JCAlgTest setup. Pico `D92E743D5F640C0F` is still visible as
USB CDC and passes the finite monitor, but neither Pico appears in PC/SC now.
Please use VirtualBox **Devices → USB** to detach/reattach the relevant Pico
device(s) to the VM. If Windows has captured a BOOTSEL device, attach that
device to the VM too. Once reattached, report completion; I will recheck
`lsusb -d 2e8a:0009`, `pcsc_scan -r`, and Java SmartcardIO before continuing.
No additional firmware flash or card APDU has been performed.

## Recover the PC/SC service for the local JCAlgTest campaigns

The Pico `D92E743D5F640C0F` is currently visible in guest USB enumeration as
`2e8a:0009 Pico2 Pico2 JCVM CCID` at `/dev/bus/usb/001/045`, and its USB
descriptors include a CCID interface. However, `pcsc_scan -r` has either listed
only the two VPCD readers or hung after its banner; it never listed D92. The
Pico-only helper grants `pcscd` access. A non-sudo `systemctl restart
pcscd.service` timed out while the daemon was in this state.

Please run these commands in the development VM, then send the complete output:

```sh
sudo systemctl restart pcscd.service
sudo -n /usr/local/sbin/pico2-pcsc-reader
timeout 10s pcsc_scan -r
```

If the final scan still omits D92, leave the token connected and report that
result; I will continue with offline work until PC/SC publishes it. Do not
flash or send card APDUs as part of this service recovery.
