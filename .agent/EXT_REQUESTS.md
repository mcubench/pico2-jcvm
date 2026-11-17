# External validation requests

## HOST-LIBCCID-PICO2-20261117 — Teach libccid the Pico 2 CHOUPI CCID reader

Created 2026-11-17 19:15. The JCVM firmware is flashed and running on token
`EE116F37C7E4A04E` (USB `2e8a:0009`, "Pico2 JCVM CCID"), but PC/SC lists no reader
(`SCARD_E_NO_READERS_AVAILABLE`): libccid 1.5.2 on this machine has no entry for
`0x2E8A:0x0009` in `/etc/libccid_Info.plist` (the old machine had a locally patched
table). This needs root. Please run from any directory:

```sh
sudo /home/agentdev/projects/pico2-jcvm/tools/install-libccid-pico2
sudo env SUDO_USER=agentdev /home/agentdev/projects/pico2-jcvm/tools/install-pcsc-sudoers
```

- The first script appends one entry (`0x2E8A`, `0x0009`, "Pico 2 CHOUPI CCID") to each of the
  three parallel arrays in `/etc/libccid_Info.plist`, validates the result, keeps a backup at
  `/etc/libccid_Info.plist.pico2-backup`, installs it and restarts `pcscd`. It is idempotent.
  The edit logic is `tools/libccid_add_pico2.py` (a test run on a copy added exactly three lines).
- The second command re-installs the (updated) PC/SC helper: Debian runs `pcscd` as root, so
  the old helper's "pcscd service account does not exist" check is removed.
- Reply when done; I will run the helper and `./tools/list-readers --pico-only`.

## WIN-MEM01-EMPTY-NODEBUG-98FCB151 — Test current deletion-enabled build

**Status 2026-11-17:** this image predates the `OBJECT_DELETION:PRE_CLEANUP` telemetry and the 90% headroom gate. A fresh normal max-crypto empty-card package is planned (see HANDOFF MEM-01 item 2) and will replace this request; results from this image remain valid for its hash.

Created 2026-11-16 08:02 CET. This fresh package supersedes
`WIN-MEM01-EMPTY-NODEBUG-B6CCF96C` for new runs. It is a normal ARM
`max_crypto` image with persistent object deletion ON, all debug and trace
switches OFF, 0 ms startup wait, an erased 1 MiB storage region, and no
preinstalled applet. Its source is commit `98fcb151` plus the pre-existing
dirty worktree/submodule state recorded in the package provenance.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-98fcb151.uf2)
- UF2 SHA-256: `e1210b1afdf20afa335e32b188541804384ada64f5e4e074311b03e06e7595d8`; size 4,126,720 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-98fcb151.zip)
- ZIP SHA-256: `327b6454fce940bb972e1f50474d798dff964ec0ecc552acd84b843aa85505a4`.
- CPLC source identifier: `0x98FCB15176729161`; ARM build passed in `normal` mode, object deletion ON, debug OFF. Linked end `0x100f7af0`; 34,064 bytes remain. Physical flash and Windows testing were not performed here.
- The package contains the unchanged JCAlgTest 1.8.4/jc305 CAP and the supplied 1.8.5 host client. Their hashes are recorded in `BUILD-PROVENANCE.txt` and `SHA256SUMS.txt`.

Follow the package README to verify and flash through BOOTSEL, confirm the card
is empty, install the CAP explicitly with GPPro, and run option 2. Return the
complete result directory and serial log, Windows version, reader and ATR, chip
ID, firmware and client hashes, and any USB/PCSC errors, resets, stalls or
timeouts.


## WIN-RSA1984-X50-SERIAL-20261115 — Capture serial evidence of the RSA-1984 ×50 failure

Created 2026-11-15 12:35 CET. Diagnostic run (not acceptance) for TODO
LONGOP-01. It needs only about 5 minutes per attempt. If the full option-2
run under `WIN-MEM01-EMPTY-NODEBUG-98FCB151` is in progress, let it
finish first. The current image does not contain a LONGOP-01 fix, so it may
still fail at this operation.

1. Flash the current empty-card UF2 from `WIN-MEM01-EMPTY-NODEBUG-98FCB151`
   (SHA-256 `e1210b1a…e7595d8`). Install the unchanged JCAlgTest CAP with GPPro.
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

