# External validation requests

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

Please run the refreshed diagnostic package and follow its exact hash check and
BOOTSEL copy commands:

- [Windows package](../build/external-test/pico2-jcvm-object-deletion-windows.zip)
- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card-cdd175bc.uf2)
- Package SHA-256: `318c9e69e55b7a44d0865bf66e18049288fcaf91ef60e240004e2ee45d9edf51`
- Firmware SHA-256: `a66e7d2b33c4695fa2e3415e4d198300ae92089f1ab93dbcd259968132d871e0`
- Source commit: `cdd175bc66af28528817c6998008269ed0955b6a`; the build used a dirty
  worktree, disclosed with its tracked-diff hash in the package's
  `BUILD-PROVENANCE.txt`.
- The package README includes exact Windows PowerShell hash verification and
  BOOTSEL flashing commands. It contains the unchanged JCAlgTest 1.8.4/jc305
  CAP and matching client, GPPro, and serial capture helper.

The diagnostic image uses `max_crypto` with object deletion enabled. It starts
with an empty card and exercises the periodic `INS_CARD_RESET` request path.
Run `2 -> PERFORMANCE TEST` (1–3 hours) on one identified Pico. When a second
token is available, flash/install the same package there and run
`1 -> All algorithms` (5–8 hours). Keep per-device result folders, reader names,
chip IDs, firmware hash, and complete serial logs separate.

### Windows USB enumeration failure reported 2026-11-11

After BOOTSEL mode and copying the UF2, Windows reported an “unrecognized USB
device.” Local validation of the freshly rebuilt current-source image
(`artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card-20261111-current.uf2`,
SHA-256 `8af721cfd59d22651fad5d17e0ef4dd81d1262402f3ba40cced1c232be410f94`)
on Linux passed flash verification, re-enumerated as a PC/SC reader, and
passed `TEST:SUMMARY pass=1 fail=0`; that does not identify the Windows cause.
Please return the exact UF2 filename and SHA-256 used for the failing upload,
Windows version, and the Device Manager status/error code and hardware IDs
(or the exact “Device Descriptor Request Failed” text if IDs are unavailable).

Return full result folders and serial logs, including partial output if a run
stalls. Preserve `OBJECT_DELETION:` lines and any `6F00`, `F205`, reset,
timeout, or PC/SC service errors. This diagnostic image is not for production;
normal builds keep deletion disabled.

## Resolved 2026-11-11: Reattach Pico USB devices to the development VM

The user confirmed that the earlier absence was a VM USB passthrough problem.
The refreshed host helper now grants access to three Pico USB nodes. `lsusb`
sees all three and `pcsc_scan -r` publishes readers for `8E464F4E2BBC0A19`
and `2BED26ED20556535`. The third node (`D92E743D5F640C0F`) is not currently
published by PC/SC, so physical testing continued on the two available readers
instead of waiting on that board. No further VM reattachment action is needed
for the active tests.

## Closed 2026-11-11: Recover the PC/SC service for the local JCAlgTest campaigns

The helper and PC/SC scan now work for two identified Pico readers, and the
physical campaign continues on those devices. D92 remains absent from PC/SC
despite USB enumeration, but is not blocking the active tests; no service
restart or other external action is currently requested. If D92 is needed
again, reconnect it to the VM and report the result of `timeout 10s pcsc_scan -r`.

## Restart PC/SC after physical object-deletion run — 2026-11-11

The fixed physical collector ran on `8E464F4E2BBC0A19` during the upstream
JCAlgTest support suite. Its serial trace shows `INS_CARD_RESET` completing
with `9000`, but the synchronous sweep reclaimed 1,417 applet records before
the host received `SCARD_E_NOT_TRANSACTED`. PC/SC then hung before listing
readers, even though `2BED26ED20556535` still passes its finite CDC runtime
monitor. I have bounded the collector to eight persistent records per APDU;
both architecture builds and the persistent-transaction unit suite pass, but
the physical batch-size fix still needs validation after PC/SC recovers.

Please run this in the development VM and send the complete output:

```sh
sudo systemctl restart pcscd.service
sudo -n /usr/local/sbin/pico2-pcsc-reader
timeout 10s stdbuf -oL pcsc_scan -r
```

Keep the three Pico tokens connected. Do not flash or reset them as part of
this service recovery. Once PC/SC lists `2BED26ED20556535`, I can continue the
bounded object-deletion test on that board.

## Reattach Pico USB devices after runtime-reset passthrough loss — 2026-11-11

The VM initially saw all three boards (`D92E743D5F640C0F`,
`2BED26ED20556535`, and `8E464F4E2BBC0A19`). A firmware-only flash attempt on
2BED could not use the reset interface; the CDC 1200-baud fallback did not
enter BOOTSEL, and the VirtualBox guest then lost all three RP2350 USB nodes.
No firmware was written to 2BED. The host-side 8E46 test did reclaim 1,417
unreachable applet records before its CCID transaction failed.

Please reattach the Pico devices to the development VM through VirtualBox
**Devices → USB** (including a BOOTSEL entry if Windows captured it). Keep
2BED in BOOTSEL if convenient; that will let me flash the new bounded-cleanup
image directly without another software reset. Otherwise reattach the runtime
devices and report when they appear. I will then confirm IDs, flash only after
the ARM/RISC-V builds, and continue on whichever reader is available.
