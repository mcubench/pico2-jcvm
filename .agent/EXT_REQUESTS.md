# External validation requests

## HOST-PCSC-HELPER-INSTALL-20261117 — Install the PC/SC access helper on the new machine

Created 2026-11-17 18:05. `sudo -n /usr/local/sbin/pico2-pcsc-reader` (the helper
AGENTS.md authorises) does not exist on this machine, and `pcscd`/`libccid` are not
installed, so no physical token can be reached (`./tools/list-readers`, flash and
cycle wrappers). These steps need root and cannot be done by the agent. The helper
source is `tools/pico2-pcsc-reader-root`; the installer is `tools/install-pcsc-sudoers`
(it only writes `/usr/local/sbin/pico2-pcsc-reader` and
`/etc/sudoers.d/pico2-pcsc-reader`, validates with `visudo`, refuses symlinks and
refuses to overwrite a different sudoers file). The helper grants the `pcscd` user
read/write ACLs on the USB nodes of attached `2e8a:0009` "Pico2 JCVM CCID" devices
(now 1 to 4 devices) and restarts `pcscd.service`; it takes no arguments.

Please run, from the repository root, as the agent's user (`agentdev`):

```sh
sudo apt install -y pcscd libccid libpcsclite1
sudo ./tools/install-pcsc-sudoers
sudo -n /usr/local/sbin/pico2-pcsc-reader   # with at least one token attached
```

The last command should print `PCSC:ACCESS_GRANTED count=N nodes=... service=pcscd.service`
(it fails with "Expected 1 to 4 ... found 0" if no token is attached, which is also
informative). Report: the output of the three commands, and `systemctl is-active pcscd`.
If `sudo ./tools/install-pcsc-sudoers` refuses because `/etc/sudoers.d/pico2-pcsc-reader`
already exists with different content, send me that file's contents rather than
overwriting it.

After this the agent will run the helper itself (exact form above, no extra arguments),
list readers with `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only`, record the
chip IDs of the attached tokens in `.agent/QKB.md`, and pick the working token. Remove
this request when done.

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

