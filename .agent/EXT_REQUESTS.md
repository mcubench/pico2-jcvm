# External validation requests

## WIN-MEM01-MAXCRYPTO-C050A193 — Test current deletion-enabled max_crypto build (with telemetry)

Created 2026-11-17 20:00 CET. This supersedes the former request for the 98FCB151 image for new
runs (keep that image's results for its own hash). Normal ARM `max_crypto` image built
from clean commit `c050a193` with Arm GNU 14.2.1: object deletion ON, all debug and trace
switches OFF, 0 ms startup wait, erased 1 MiB storage region, no preinstalled applet. It
adds the `OBJECT_DELETION:PRE_CLEANUP` serial telemetry and the libstdc++ demangler is
dropped (36,024 bytes of firmware window remain).

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-c050a193.uf2)
- UF2 SHA-256: `345f64fc4d1052151be523c89a57fffa56d83dd8acb52abd7c666b6e6ef2762b`; size 4,123,136 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-c050a193.zip)
- ZIP SHA-256: `e18800a248784293714807009a2c7467122d5b82980bbf26b65b98edcfedcc22`.
- CPLC source identifier `0xC050A193E8141DBF`; linked end `0x100f7348`. Built by
  `tools/package-maxcrypto-windows-test`; `BUILD-PROVENANCE.txt` and `SHA256SUMS.txt` are in the ZIP.
- The ZIP also holds the unchanged JCAlgTest 1.8.4/jc305 CAP, the supplied 1.8.5 client
  (SHA-256 `b6a753be…eacec3`), GPPro and the serial capture script.
- Validation here: ARM build, host unit suites, ZIP integrity and checksum check. This exact
  UF2 was not flashed; an earlier build from the same firmware sources (differing only in the
  CPLC id) ran on a Linux host and passed PC/SC and SCP03 probes. Not tested on Windows.

Unzip, follow `README.md` (verify the UF2 hash, flash through BOOTSEL, confirm the card is
empty, install the CAP explicitly with GPPro, run option 2 and capture serial). Return the
complete result directory and the whole serial log (it contains the `PRE_CLEANUP` lines),
Windows version, reader name and ATR, chip ID, firmware and client hashes, and any
USB/PCSC errors, resets, stalls or timeouts.

## WIN-RSA1984-X50-SERIAL-20261115 — Capture serial evidence of the RSA-1984 ×50 failure

Created 2026-11-15 12:35 CET. Diagnostic run (not acceptance) for TODO
LONGOP-01. It needs only about 5 minutes per attempt. If the full option-2
run under `WIN-MEM01-MAXCRYPTO-C050A193` is in progress, let it
finish first. The current image does not contain a LONGOP-01 fix, so it may
still fail at this operation.

1. Flash the current empty-card UF2 from `WIN-MEM01-MAXCRYPTO-C050A193`
   (SHA-256 `345f64fc…2762b`). Install the unchanged JCAlgTest CAP with GPPro.
   Use the pinned 1.8.5 client and report its SHA-256; it must be
   `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.
2. Copy `artifacts/results/windows-maxcrypto-20261115-25216292-dirty/pico2_maxcrypto_20261115_25216292-dirty____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED__already_measured.list`
   next to the client. Keep an untouched copy and restore it before each
   attempt. Start option 2, type the card name `pico2_maxcrypto_20261115_25216292-dirty`
   exactly, and answer `y`. The next measured operation is RSA-1984 PKCS1
   `Cipher_doFinal()`.
3. Before starting the client, open the board's COM port at **115200 baud
   (never 1200)** in a terminal that writes a file with a timestamp on every
   line (for example Tera Term Log with timestamps). Check that `rx_events`
   in the `CHOUPI:CCID` lines rises with each APDU. Confirm the COM port and
   the reader are the same board: their Device Manager instance paths end in
   the same serial (chip ID).
4. Keep logging until 2 minutes after the client reports the error. Do not
   press `r`/`s` until then. Then record `certutil -scinfo`, `sc query SCardSvr`,
   `Get-PnpDevice -Class SmartCardReader | fl FriendlyName,Status,Problem,InstanceId`,
   and System event log entries from the failure window.
5. Repeat once from a fresh replug.

Return the complete timestamped COM log (the whole file, not excerpts), the
client log and CSV, the PowerShell outputs, the UF2 and JAR hashes, and the
chip ID. The key question is what the serial log shows during the ≈45 s ×50
APDU and right after it:
- a `CCID_RESPONSE apdu=B043000016 len=3 sw=9000` record;
- `*** PANIC ***` and its message;
- a boot banner;
- nothing.

