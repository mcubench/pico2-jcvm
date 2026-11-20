# External validation requests

## WIN-MAXCRYPTO-ONESHOT-655DFA44 — JCAlgTest 1.8.5 option 1 and 2 with the out-of-memory fix and OneShot support

Created 2026-11-20 11:13 CET. This replaces `WIN-MAXCRYPTO-OOMFIX-981CDE78` (same out-of-memory fix,
without OneShot). A run already in progress on 981cde78 may be finished and returned; it stays valid
evidence for that image.

Same package layout, CAP (1.8.5/jc305, `applet.cap` SHA-256
`fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`), client and one-block
PowerShell run (`README.md` / `run-test.ps1`, `$mode = 'all'` or `'symmetric'`) as before.
New since 981cde78 (records in `progress.md`, 2026-11-20):
- Java Card 3.0.5 `Cipher.OneShot`, `Signature.OneShot`, `MessageDigest.OneShot` and
  `RandomData.OneShot` are implemented (RAM-only, released on close/entry-point return/reset).
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

Normal ARM `max_crypto` image from commit `655dfa44` (Arm GNU 14.2.1): object deletion ON, all debug and trace switches OFF, erased storage region, no
preinstalled applet.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-655dfa44.uf2)
- UF2 SHA-256: `c28d9906dba9ae477fed12ea1109165c4e618d651fd781fc5f81146205b62784`; size 2,961,408 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-655dfa44.zip)
- ZIP SHA-256: `dc3e196b1708fd53d04670ed7eb6836b8fbc2254767de658c7fed68a56478f65`; size 18,233,738 bytes.
- CPLC source identifier `0x655DFA44FEB5DEF1`; linked end `0x100696dc`, firmware-window headroom
  616,740 bytes. Client JAR 1.8.5 SHA-256
  `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`.
- Validation here: build exit 0, no warnings; `PACKAGE:PASS`. Same firmware source on the emulator:
  JCSystem/OneShot probe suite 111/111, JCAlgTest `ALG_SUPPORT_BASIC` over VPCD with OneShot rows
  matching the manifest exactly, MEM-01 T7 VPCD pass (on a1639b68). A physical smoke test of this exact UF2
  follows when token 07E7 finishes its current option-1 run. The PowerShell block has not been run
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
  corresponding `getInstance()` sections, with no errors?
- Does `all` reach the end with no `*** PANIC ***`, `FAULT` or reader loss, in particular through
  RSA-3072/4096 CRT?
- Does the serial log show any `NO_RESOURCE`, or `OBJECT_DELETION … pending=1` / `tag_index_refused=1`?
- Is the `HEADROOM:` verdict at the end of `serial-capture-console.txt` PASS? How many
  `FS_APDU … synced=1` lines are there?
