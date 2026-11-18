# External validation requests

## WIN-MEM01-MAXCRYPTO-853543D6 — JCAlgTest option 2 on the deletion-pass fix build

Created 2026-11-18 07:35 CET. The previous request `WIN-MEM01-MAXCRYPTO-DE729D28` was returned
(`artifacts/results/windows-maxcrypto-20261117-de729d28-deletion/`): it passed RSA-1984 ×50 but
lost the card at RSA-CRT-1280 NOPAD, and the board rebooted itself.

Cause, reproduced locally: JCAlgTest keeps two RSA key pairs per type and length, so every B0E2
(object deletion) scanned a growing live set. The pass grew to 10–15 s and ran without feeding
the 15 s watchdog or sending CCID time extensions.

Normal ARM `max_crypto` image from commit `853543d6` (Arm GNU 14.2.1): object deletion ON, all
debug and trace switches OFF, 0 ms startup wait, erased 1 MiB storage region, no preinstalled
applet. The build reports `state=dirty` only because of untracked non-source files in the
working copy; no tracked file differs from `853543d6`.

Changes since de729d28 (`progress.md` Step-Ids `20261118-0653-deletion-pass-speed`,
`20261118-0702-deletion-pass-hw-ab`, `20261118-0723-gc-on-request`):
- The object-deletion pass uses a sorted index of live object tags and reuses one heap trace per
  batch. On the failing card state, the pass went from 10.2 s to 2.2 s and B0E2 from 10.2 s to
  3.05 s.
- Long deletion and heap loops feed the watchdog and send CCID time extensions.
- The heap is garbage-collected only on JCSystem.requestObjectDeletion (B0E2), no longer after
  every APDU. The old per-APDU collection cost about 0.9 s per APDU late in the run.
- New serial lines `OBJECT_DELETION:TIMING …` and `CHOUPI:HEAP_GC_TIMING …` give per-pass timing.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-853543d6.uf2)
- UF2 SHA-256: `d0701dbfe103cb25bad3a05c38efdf8838e38747302a364a00d553b0c4e0fc8a`; size 4,131,328 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-853543d6.zip)
- ZIP SHA-256: `b38adc3bcdf5459457768dc26336b6525a5fc4326fdc9cc3db47727bdcd84299`; size 18,271,719 bytes.
- CPLC source identifier `0x853543D636539904`; linked end `0x100f833c` (31,940 bytes of
  firmware window remain). Built by `tools/package-maxcrypto-windows-test`;
  `BUILD-PROVENANCE.txt` and `SHA256SUMS.txt` are in the ZIP.
- The ZIP also holds the unchanged JCAlgTest 1.8.4/jc305 CAP, the supplied 1.8.5 client
  (SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`), GPPro and
  the serial capture script.
- Validation here (Linux, not acceptance):
  - This exact UF2 was flashed with `tools/flash` to token 07E74C5D7A038DEC.
  - GPPro listed an empty card (ISD only), installed the unchanged CAP, and the applet was SELECTABLE.
  - `tools/test-longop01-rsa1984-physical` passed 18/18 APDUs: RSA-1984 PKCS1 ×50 47.9 s,
    RSA-2048 ×50 48.6 s, no PANIC or FAULT.
  - The first attempt stopped at the probe's own 180 s budget after a 66.6 s RSA-2048 key
    generation; this was not a firmware failure.
  - The same code on 07E7, with the RSA key cache filled through CRT-3072 (beyond both failing
    runs), kept every B0E2 ≤ 4.9 s with no reset.
  - Logs: `artifacts/results/windows-package-smoke-853543d6-20261118T062616Z/`,
    `artifacts/results/longop01-rsa1984-physical-20261118T063041Z/`,
    `artifacts/results/deletion-pass-ab-20261118/`.
  - Not tested on Windows; no full option-2 run on this image yet.

Steps:
1. Unzip and follow `README.md`: verify the UF2 hash, flash through BOOTSEL, confirm the card is
   empty, and install the CAP explicitly with GPPro.
2. Run option `2 -> PERFORMANCE TEST -> 1 (all algorithms)` from the start, with a new card name.
3. Capture the board's serial output for the whole run with `tools\capture-object-deletion.ps1`
   (115200 baud, never 1200). Set `-Seconds` longer than the run (e.g. 43200); the default is
   3600. Please do not use a terminal program with a limited scrollback.
4. If the reader disappears, note whether the board came back by itself and keep the capture
   running across the reconnect.

Return:
- the complete client result directory (log, CSV, `already_measured.list`) and the whole serial log;
- Windows version, reader name and ATR, chip ID, UF2 and client JAR hashes;
- any USB/PC/SC errors, resets, stalls or timeouts, with their times.

Key questions:
- Does the run get past RSA-CRT-1280 NOPAD and reach the end of option 2 with no reader loss and
  no `*** PANIC ***`?
- Do the `OBJECT_DELETION:TIMING … total_us=` values stay well below 15,000,000 (expected
  about 2–3 s late in the run)?
