# External validation requests

## WIN-MAXCRYPTO-ONESHOT-D7E90A18 — JCAlgTest 1.8.5 option 1 and 2 with the out-of-memory fix, OneShot and InitializedMessageDigest

Created 2026-11-20 11:41 CET. This replaces `WIN-MAXCRYPTO-ONESHOT-655DFA44` (same content without
InitializedMessageDigest). A run already in progress on 655dfa44 or 981cde78 may be finished and
returned; it stays valid evidence for that image.

Same package layout, CAP (1.8.5/jc305, `applet.cap` SHA-256
`fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`), client and one-block
PowerShell run (`README.md` / `run-test.ps1`, `$mode = 'all'` or `'symmetric'`) as before.
New since 981cde78 (records in `progress.md`, 2026-11-20):
- Java Card 3.0.5 `Cipher.OneShot`, `Signature.OneShot`, `MessageDigest.OneShot`,
  `RandomData.OneShot` and `InitializedMessageDigest.OneShot` are implemented (RAM-only, released on
  close/entry-point return/reset).
- `MessageDigest.getInitializedMessageDigestInstance()` / `InitializedMessageDigest` (SHA-1/224/256/
  384/512 with `setInitialDigest`) are implemented; previously unsupported.
  JCAlgTest therefore reports OneShot support for the same algorithms as `getInstance()`
  (previously every OneShot row was `no`).
Also included (new since a266b836):
- Each live Java object costs less RAM (array handle 84 → 52 B, instance 104 → 56 B; measured
  176 → 144 B and 153 → 105 B of heap per object). At the old failure point (405 arrays /
  144 instances live) free heap is ~61 KB instead of ~11 KB.
- New Java objects are refused with `SystemException.NO_RESOURCE` when less than 16 KB of heap
  would remain, so native crypto never hits the SDK `Out of memory` panic.
- Deletion passes under memory pressure keep using the fast tag index (`tag_index_refused=0`).
- Everything from a266b836 (flash journal, deletion skip, native OwnerPIN) is included.

Normal ARM `max_crypto` image from commit `d7e90a18` (Arm GNU 14.2.1): object deletion ON, all debug and trace switches OFF, erased storage region, no
preinstalled applet.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-d7e90a18.uf2)
- UF2 SHA-256: `6a49614f43242bfd6ad546381a7fcfa157abd27a165bb2e787f02535002ec1ff`; size 2,966,016 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-d7e90a18.zip)
- ZIP SHA-256: `4119c1028836f44b7d6000ed12758e6774ac08050ffe8ee05bcfa43502da070e`; size 18,235,064 bytes.
- CPLC source identifier `0xD7E90A187DA2FB94`; linked end `0x10069f18`, firmware-window headroom
  614,632 bytes. Client JAR 1.8.5 SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.
- Validation here: build exit 0, no warnings; `PACKAGE:PASS`. Same firmware source on the emulator:
  JCSystem/OneShot probe suite 113/113, JCAlgTest `ALG_SUPPORT_BASIC` over VPCD with OneShot and
  InitializedMessageDigest rows matching the manifest exactly, MEM-01 T7 VPCD pass (on a1639b68). Physical smoke of this exact UF2 on
  07E74C5D7A038DEC (empty card): package CAP and a probe applet installed, OneShot and
  InitializedMessageDigest probe cases match the emulator, those APDUs write nothing to flash. The PowerShell block has not been run
  (no PowerShell here).

Steps:
1. Unzip, open PowerShell in the extracted folder, hold BOOTSEL while connecting the Pico 2.
2. Set `$mode` (`all` first, if only one run is possible) and paste the README block.
3. If the block stops with an error, return the `results\<mode>_<timestamp>` folder as it is.

Return:
- each `results\<mode>_<timestamp>.zip`: client log/CSV, `serial.log`, GPPro output and `run-identity.txt`;
- any USB/PC/SC errors, resets, stalls or timeouts, with their times.

Key questions:
- Do the `OneShot` sections of the support CSV report `yes` for the same algorithms as the
  corresponding `getInstance()` sections, and `InitializedMessageDigest` for SHA-1/SHA-2, with no
  errors?
- Does `all` reach the end with no `*** PANIC ***`, `FAULT` or reader loss, in particular through
  RSA-3072/4096 CRT?
- Does the serial log show any `NO_RESOURCE`, or `OBJECT_DELETION … pending=1` / `tag_index_refused=1`?
- Is the `HEADROOM:` verdict at the end of `serial-capture-console.txt` PASS? How many
  `FS_APDU … synced=1` lines are there?

## SPEC-MESSAGE-DIGEST-STREAMING — approve removal of the cumulative digest buffer cap

Created 2026-11-21 10:37 CET. Please confirm replacing the 2,046-byte
cumulative MessageDigest input limit in [SPEC.md](../SPEC.md) INV-28 with
streaming input up to the selected algorithm's message-length limit, subject
to the Java Card per-call array and short argument bounds.

The [native implementation](../choupi/src/native_message_digest.cpp) appends
every chunk to a fixed 2,046-byte CLEAR_ON_RESET array and hashes it at
doFinal(). After approval, implement bounded per-instance native digest state
for both MessageDigest and InitializedMessageDigest. Verify the digest of
more than 2,046 bytes across multiple updates, block boundaries, empty final
input, reset and instance reuse. No external test or firmware is needed to
answer this request.
