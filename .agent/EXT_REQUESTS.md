# External validation requests

## WIN-CAMPAIGN-3BE60283-20261127 — one Windows campaign, no serial needed

Created 2026-11-27 10:20 CET. Replaces WIN-KEYPAIR-107D7663, WIN-PQC-PERF-58B5EC0A
and WIN-ECTESTER-9B03A1E0. Results already produced with those packages are
still welcome. Return them with their original firmware identity.

- [Windows ZIP](../build/external-test/pico2-jcvm-maxcrypto-3be60283_20261127.zip),
  SHA-256 `9199fa3a1e61b5dcde5bc5326cc34e2116b454eefdef5f9c23c8c5d4e98b1597`, 18,607,018 bytes.
- [Empty-card UF2](../artifacts/firmware/pico2-jcvm-maxcrypto-3be60283_20261127-empty-card.uf2),
  SHA-256 `dd745022ed6f700d59870ed93c9c33c2eaf23f153534b9fd72636a7969c601cb`,
  CPLC source ID `3BE60283A059019A`. Normal max_crypto. Flashing clears applet storage.

Steps: extract the ZIP and run
`powershell -ExecutionPolicy Bypass -File run-all.ps1` in that folder. At each
prompt, reconnect the Pico 2 with BOOTSEL held. The order is `pqc` (30+ min),
`pqcperf`, `jcmathlib`, three cold unplug/replug timings, `symmetric` (1–3 h),
then `all` (5–8 h) with timed GPPro DELETE and `--list`. Use `-Modes` to
resume. Return `results\campaign_<timestamp>.zip`. A serial log is not
required: every run stores `health-*.txt` from GET DATA `DF7E` (boot count,
`6F00`/`6999` count). If you run the client by hand instead, run
`tools\health.ps1 -Reader '<reader>'` right after it, without unplugging, and
return its output with the client log and CSV.

Closes on `tools/check-windows-results.py <zip>` passing: WIN-KEYPAIR and
PQC regressions, JCMathLib shift, GP-01 DELETE watchdog (< 10 s, ISD only),
HOST-02 (three cold reconnects), LONGOP-01/JC-RESET-01/AC-01 (`all` and
`symmetric` complete, boot count 1, no internal-error SW).

Linux validation of this exact UF2 on 07E7: see progress
`20261127-1030-windows-campaign-request`. PowerShell was not executed here.

## WIN-RSACRT2048-B4ADBE13-20261127 — answer from memory only

Created 2026-11-27 07:51 CET, reduced 10:20. No serial log is needed. For the
Windows `b4adbe13` run that returned `6F00` at APDU 7110, state what you remember:
was the empty-card UF2 flashed right before installing jc32, what ran before,
and were there resets or replugs. "Unknown" is a valid answer. It closes the
request as not reproduced (the exact Linux replay returned `AA 9000`). The new
campaign's health record then serves as the regression check.

## CRYPTO-XDH-ABI-20261121 — Java Card named-parameter XDH

Created 2026-11-21 15:02 CET. Supply matching Java Card SDK API sources or
export files for the target XDH API and an unchanged reference CAP that
exercises named-parameter key import and agreement. Include expected
X25519/X448 shared-secret outputs and invalid-public-key results for the
curves the target supports. The current build uses JC305 exports and has no
named-parameter XDH key ABI.

Update 2026-11-24: the user supplied the Java Card SDK 26.1 (export files in
`.local-toolchain/java-card/provided/jc_26_1/lib/tools.jar`), which covers the
API/export part. Still needed: an unchanged reference CAP and expected
X25519/X448 outputs and invalid-public-key results.
