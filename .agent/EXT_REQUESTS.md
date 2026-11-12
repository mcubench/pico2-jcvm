# External validation requests

## WIN_MAXCRYPTO_GC_NODEBUG_20261112_R2 — Current Windows test: max_crypto with object deletion, no debug tracing

Created 2026-11-12 15:45 UTC. This replaces the earlier 31cb571c package.
Please test the fresh RP2350 ARM image on Windows. It has `max_crypto`
capabilities, deferred JCSystem object deletion enabled, a fully erased
persistent-storage region and no preinstalled applet. Heap-GC, CCID-applet and
persistent-GC debug tracing are disabled; concise object-deletion summaries
remain enabled.

- [Current standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-no-debug-empty-card-580f3f40.uf2)
- UF2 SHA-256: `f17a1fae0c0105475092d81595d2899eb671f8b9686656267c5d2137709ab4b7`
- Size: 4,099,072 bytes. Firmware-only UF2 SHA-256:
  `f1d71864a0620ee28a1c4a93b1ff3dd768c9c30757227c87c83873963713ce52`.
- Source HEAD: `580f3f409c898f6ea71d8d15dc12f98a277d49a2` (dirty worktree).
  CPLC source ID: `580F3F409C898F6E`. Source-file diff SHA-256 over
  `choupi/src src config`: `64ee57808d759dec16296d48050a0ef307be06b602c206a4c9836189fec9f9f9`.
- Build: ARM and RISC-V wrappers passed with `max_crypto` and
  `PICO2_ENABLE_OBJECT_DELETION=1`; debug heap-GC, CCID-applet, and
  persistent-GC switches are OFF. `./tools/test-no-debug-traces` passed for
  both architectures; the ARM ELF retains concise deletion events and omits
  verbose APDU and persistent-reference trace strings. ARM firmware ends at
  `0x100f4460`, leaving 48,032 bytes before the configured 1 MiB boundary.
  RISC-V builds the VM core only, not complete CHOUPI firmware.
- Local physical check, 2026-11-12: the matching firmware-only UF2 was flashed
  through the identity-pinned CDC-1200 BOOTSEL fallback to D92
  (`D92E743D5F640C0F`, BOOTSEL observed after 4 s), 8E
  (`8E464F4E2BBC0A19`, 3 s) and 2BED (`2BED26ED20556535`, 3 s). All returned
  `FLASH:PASS`; three finite monitors returned `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. PC/SC subsequently listed all three readers;
  GPPro `--list` on D92 succeeded and showed the preserved JCAlgTest package
  and applet. The packaged empty-card UF2 itself was not flashed locally.
  Per-device serial logs:
  `artifacts/results/maxcrypto-gc-nodebug-flash-20261112-current/`.

Windows PowerShell steps:

1. Verify the downloaded image:
   `Get-FileHash .\pico2-jcvm-max_crypto-object-deletion-no-debug-empty-card-580f3f40.uf2 -Algorithm SHA256`
   and compare with the hash above.
2. Hold BOOTSEL while connecting the Pico 2, copy the UF2 to the `RPI-RP2`
   drive, and wait for it to disappear/re-enumerate.
3. Run `certutil -scinfo`; return the reader name and ATR. Confirm the card is
   empty with `java -jar .\gp.jar --reader "<exact reader name>" --list`.
4. Explicitly install the unchanged upstream JCAlgTest 1.8.4/jc305 CAP with
   GPPro, then use its matching 1.8.4 client. Run option `2 -> PERFORMANCE
   TEST` and option `1 -> All algorithms` if time permits. Keep separate
   result folders and serial logs; record reader name and chip ID.
5. Return Windows version, hash output, reader/ATR, GPPro install/selection
   results, client CSVs/logs and complete serial output. Preserve deletion
   summaries and any `6F00`, `F205`, reset, timeout or PC/SC errors.

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

The initial package below has been superseded by the bounded-reclaimer package
in the next section. Do not use the old firmware hash for another upload.

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

### Fresh shared-budget Windows package — 2026-11-11

Use this package for the next Windows test. The older `1651bbdd` package has a
known shared-budget defect and must not be used. The new firmware includes the
shared eight-record limit across applet and package-static records. It is an
empty-card image; install the unchanged CAP explicitly after the reader
enumerates.

- [Windows package](../build/external-test/pico2-jcvm-object-deletion-shared-budget-73beff17.zip)
- Package SHA-256: `39dd82ada1c10eac539c6e77b033bb08097ffd7cab993c3a40c6cdfe91934f65`
- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card-73beff17.uf2)
- UF2 SHA-256: `4353d4f2f068aa988fce7d4d92c667634ddf4d3684e4580c8dedae08c2efb3af`
- Source commit: `73beff17`; ARM `max_crypto`; object deletion enabled;
  CPLC source ID `0xFF73BEFF`; 48,752 firmware bytes remain.
- The package README has exact hash verification, BOOTSEL copy, PC/SC check,
  GPPro CAP installation, and serial capture commands. `BUILD-PROVENANCE.txt`
  discloses the dirty root diff and submodule source hashes used by the build.

Please first confirm Windows enumerates this exact UF2 with the reported
SHA-256 and provide the reader name and ATR. If Windows again shows an
unrecognized device, send the Windows version, Device Manager error/status and
hardware IDs (or descriptor-request error), and whether BOOTSEL returns after
unplug/replug. If it enumerates, explicitly install `AlgTest_v1.8.4_jc305.cap`
and run the 1.8.4 client following the package README; return results and full
serial output.

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

## Resolved 2026-11-12: PC/SC restored for D92 and 8E; 2BED needs reattachment

The fixed physical collector ran on `8E464F4E2BBC0A19` during the upstream
JCAlgTest support suite. Its serial trace shows `INS_CARD_RESET` completing
with `9000`, but the synchronous sweep reclaimed 1,417 applet records before
the host received `SCARD_E_NOT_TRANSACTED`. PC/SC then hung before listing
readers, even though `2BED26ED20556535` still passes its finite CDC runtime
monitor. I have bounded the collector to eight persistent records per APDU;
both architecture builds and the persistent-transaction unit suite pass, but
the physical batch-size fix still needs validation after PC/SC recovers.

### Result 2026-11-12 08:27 UTC

After flashing the current empty-card image, the approved helper and
`pcsc_scan -r` listed D92 and 8E. GPPro `--list` confirmed no APP/PKG entries
on either board, so a PC/SC service restart is no longer needed. The third
board `2BED26ED20556535` remains at USB `2e8a:0009` without its serial
descriptor and did not enter BOOTSEL after a path-specific CDC reset. It is
not safe to flash it until the ROM chip ID is visible again.

## PCSC-RESTART-AFTER-WATCHDOG-20261112 — Restore physical reader publication

Updated 2026-11-12 22:23 UTC. The approved helper grants access to three Pico
USB nodes. After a JCAlgTest APDU watchdog stopped the Java process, `pcsc_scan
-r` showed only Virtual PCD readers or timed out before listing readers. The
physical boards remain present as USB devices. D92 (`D92E743D5F640C0F`) and
2BED (`2BED26ED20556535`) were identity-pinned through the full CHOUPI cycle;
both builds, flash verification and finite runtime checks passed, but the
physical readers are still not published. An unprivileged
`systemctl --no-ask-password restart pcscd.service` was denied. No sudo service
restart was attempted.

There is no active SmartcardIO campaign. Please run in the guest:

```sh
sudo systemctl restart pcscd.service
sudo -n /usr/local/sbin/pico2-pcsc-reader
timeout --signal=TERM --kill-after=1s 10s pcsc_scan -r
```

Return the complete output, especially the exact chip IDs and reader names.
Continue with any published physical reader; no BOOTSEL reattachment is needed.
The latest agent attempt to refresh the helper using the required elevated
host-access path was blocked with `sudo: The "no new privileges" flag is set`.
If the first command works but the helper command fails, return both outputs;
do not reattach BOOTSEL devices for this issue.

## EXT-USB-VM-REATTACH-20261111-1824 — Current VM USB passthrough status

The user confirms that the physical boards are available and the earlier
problem was VM passthrough, not a board fault. At 2026-11-11 18:58 CET, after
that update, the authorized helper still found zero matching Pico devices
inside this VM. No device was flashed or reset during this check. This is the
current VM visibility result, not a report that the physical boards are
unavailable or malfunctioning. The required host-access diagnostic also
reported `No accessible RP-series devices in BOOTSEL mode were found`.

When changing passthrough, attach at least one Pico USB device to the
development VM through VirtualBox **Devices → USB**. The next local action is
to refresh the helper and identify the reader; the bounded deletion tests and
both architecture build gates already pass. Host-side physical availability
alone does not expose its USB node inside this guest.

### Earlier runtime-reset passthrough event — 2026-11-11

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

## Retry JCSystem object deletion with bounded cleanup (source commit 1651bbdd)

**Superseded; do not flash this image for current validation.** Code review
found the eight-record allowance was independently applied to applet and
package-static namespaces, so one APDU could erase up to 16 records. A shared
total budget is now implemented and validated by host tests, VPCD integration,
and ARM/RISC-V build wrappers. A fresh Windows package from the shared-budget
firmware has not yet been produced.

A fresh empty-card test package is ready. It includes the diagnostic ARM image,
unchanged JCAlgTest 1.8.4/jc305 CAP and client, GPPro, and serial capture tool.
The reclaimer now deletes at most eight persistent records per APDU and resumes
on later APDUs, so a large cleanup will not hold the CCID response while
erasing the full backlog.

- [Download the bounded Windows package](../build/external-test/pico2-jcvm-object-deletion-bounded-windows-1651bbdd.zip)
- [Download the standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card-1651bbdd.uf2)
- Package SHA-256: `a48df3e9c1dd981efaa3beb93f856f52cc9b100dc7c0f151d643fdb9df0548a1`
- UF2 SHA-256: `7fb42f4b5f706a3df26bc36ed5f2809de4d322f6662771b2d8722a3bdd0d40e4`
- Source commit: `1651bbdd`; ARM profile: `max_crypto`; object deletion:
  enabled; serial wait: `0 ms`; CPLC source ID: `0xFF1651BB`.
- UF2 size: `4,097,536` bytes. This is an empty-card image and erases the
  complete 1 MiB persistent-storage region when copied in BOOTSEL.

The package README contains PowerShell hash verification and exact BOOTSEL,
installation, and test steps. First confirm the device is recognized after
flashing and report its reader name and ATR; install the upstream CAP explicitly
with the packaged GPPro. Then choose `2 -> PERFORMANCE TEST` on one identified
token and `1 -> All algorithms` on a second token if available. Keep the
result and full serial log separate per chip ID. If Windows still reports an
unrecognized USB device, include the exact UF2 hash, Windows version, and
Device Manager status/error and hardware IDs. The bounded build has passed
unit tests and ARM/RISC-V builds but has not yet been tested on physical
hardware. Use it only as historical evidence, not as the current test image.
