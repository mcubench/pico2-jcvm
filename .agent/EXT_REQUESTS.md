# External validation requests

## WIN-ECTESTER-9B03A1E0-20261126 — Windows EC regression

Created 2026-11-26 22:12 CET. The tester confirmed `39fd4199` is the last
tested firmware. Use it as the comparison baseline. This normal ARM
max_crypto image has CPLC source ID `9B03A1E0D653B0D1` and includes the
EC private-value and compressed/hybrid point changes from `05fb3d0b`.
Flashing clears applet storage. Use a dedicated Pico 2.

- [Windows ECTester ZIP](../build/external-test/pico2-jcvm-ectester-9b03a1e0_20261126.zip),
  SHA-256 `ad33453b26ad10ae99d461a569a1309aa3e8f42c0166b2582d251e732efaaec7`.
- [Standalone UF2](../artifacts/firmware/pico2-jcvm-maxcrypto-9b03a1e0_20261126-empty-card.uf2),
  SHA-256 `3851987bb83b2bdf8de8dab208f17aab9623803ce8e1ccdd6b905c8bf68ecbde`.

Follow the ZIP README and use the same five suites and settings for the
comparison. Return the results directory with XML, reader name, chip ID,
input hashes, and any resets, disconnects, or timeouts. Compare compression
results case by case, including P-224, which remained open in the Linux run.
The exact packaged UF2 and Windows workflow await external validation.

The updated ZIP also contains the standard JCAlgTest 1.8.5/jc305 CAP,
the pinned 1.8.5-20261121 client, and its libraries. Follow the JCAlgTest
README sections for separate empty-card runs with the same UF2. The ZIP
also includes the pinned supplemental 1.8.6/jc32 pair for PQC testing.

This supersedes the ECTester portion of WIN-ECTESTER-39FD4199-20261126.
The [39fd4199 JCAlgTest ZIP](../build/external-test/pico2-jcvm-maxcrypto-39fd4199_20261126.zip)
remains available for its separately scoped baseline and PQC regressions.

## WIN-KEYPAIR-107D7663-20261126 — Pico 2 Windows regression

Created 2026-11-26 15:07 CET. Use this build for new Windows runs in place of
`WIN-PQC-PERF-58B5EC0A-20261125`. Keep results from already-started runs with
their original firmware identity.

- [Windows test ZIP](../build/external-test/pico2-jcvm-maxcrypto-107d7663_20261126.zip),
  SHA-256 `8472248b1c845244e1d98492ef407626c5382d31775d38a972c1002854851108`, 18,536,214 bytes.
- [Empty-card UF2](../artifacts/firmware/pico2-jcvm-maxcrypto-107d7663_20261126-empty-card.uf2),
  SHA-256 `f50713873412313977d8ba0079a4a0f734230c28d25d10678bfaf53c25961e05`,
  3,195,904 bytes, CPLC source ID `107D76632F9151C1`.

This normal ARM max_crypto image contains the existing-key EC, RSA and RSA-CRT
constructor fixes and the EC generation fix for preloaded domain parameters.
Flashing the empty-card UF2 clears applet storage. Use a dedicated test Pico 2.

Run the packaged PowerShell instructions for `pqc`, `pqcperf`, `all` and
`symmetric`. Carry forward the timed GPPro uninstall check from the previous
request after `all`. Return the result ZIPs, the uninstall transcript and
elapsed time, and any disconnects, resets or timeouts. Include the reader name,
chip ID and UF2 and client JAR hashes.

For a focused JCMathLib replay on a freshly flashed card, copy the supplied
[unit_tests.cap](../inbox/problem_jcmathlib/unit_tests.cap) and the
[descriptor-preserving loader source](../tools/LoadCapWithDescriptor.java) into
the extracted package. With a JDK and the exact reader name in `$reader`:

```powershell
javac -cp tools\gp.jar -d . LoadCapWithDescriptor.java
java -cp ".;tools\gp.jar" LoadCapWithDescriptor $reader unit_tests.cap
java -jar tools\gp.jar --reader $reader --apdu 00A404000B4A434D6174684C69625554 --apdu B024010020D0910DC146EC6027B9318706FD14D6D86199B1324AF4B9B13B4D7D1C09E89DDB00 --debug
```

Expected SELECT `9000`, then
`684886E0A3763013DC98C3837E8A6B6C30CCD899257A5CD89DA6BE8E04F44EED 9000`.
Return the complete APDU transcript. This variant uses a one-bit shift and
includes Le. The original JCMathLib shift helper handles counts 0–7.

The committed ARM build, archive checksums and erased storage were verified
locally. The KeyPair and JCMathLib replays previously passed on Pico 2 with a
development image from the same firmware code. This exact packaged UF2 and
the Windows PowerShell runner were not executed locally.

## WIN-PQC-PERF-58B5EC0A-20261125 — Pico 2 firmware with JC 3.2 PQC (support and performance tests), SCP03 and all fixes since rc1

Created 2026-11-25 22:18 CET at `b4adbe13`, repackaged 23:30 at `58b5ec0a` (the 1.8.6 client now reports version
`1.8.6` instead of `1.8.5`; firmware code unchanged). Replaces `WIN-PQC-SCP03-F3CF5729-20261125` (package `f3cf5729`, not yet
returned): firmware code is unchanged since `f9e7cf61` (only the CPLC source ID differs); new is the re-pinned
JCAlgTest 1.8.6/jc32 pair with PQC performance tests and a `pqcperf` mode. Use a dedicated test Pico 2.
Normal ARM max_crypto image (object deletion on, debug off); flashing erases applet storage.

In the firmware since `6d3bb526` (details in `progress.md`, 2026-11-25):
- Java Card 3.2 preview PQC API, native: ML-KEM-512/768/1024 (`javacardx.crypto.kem`), ML-DSA-44/65/87
  (`javacardx.security.pqc`, `Signature`), CAP 2.3 loading (`51d27d7e`, `8a56f4e0`).
- GP PUT KEY for a persistent SCP03 keyset; SCP03 C-ENC, R-MAC, R-ENC (`8b155dc5`, `aeecd243`).
- SELECT ends a GP secure channel session (`f9e7cf61`).

- [Empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-58b5ec0a.uf2),
  SHA-256 `3209b85c69eb02b39467a9add3d2986965e134aadb1dfd9534d281f54a6f253b`, 3,193,344 bytes, CPLC source ID `58B5EC0AC1475F82`.
- [Windows test ZIP](../build/external-test/pico2-jcvm-maxcrypto-deletion-58b5ec0a.zip),
  SHA-256 `40f4d757ad530de83be691a7e0ab087c81a2e0552c6fa0315d55de50670ac9e8`, 18,535,298 bytes.
- Inside the ZIP: unchanged JCAlgTest 1.8.5/jc305 CAP (`applet.cap`, `fa64582e…48fb`) with the 1.8.5-20261121 client
  (`3cca70cb…a703`) for modes `all`/`symmetric`; JCAlgTest 1.8.6/jc32 CAP (`applet-jc32.cap`, `5f19662d…6f8f`) with
  its client (`tools/AlgTestJClient_v1.8.6_jc32.jar`, `a90fe5bb…14fd`, reports `AlgTestJClient version; 1.8.6`) for modes `pqc` and `pqcperf`.

Validation here (EE116F37C7E4A04E, this exact UF2 flashed, empty card, packaged GPPro and clients; the PowerShell
block itself was not run, no PowerShell here):
- `pqcperf` equivalent (client answers `2`, `4`): rc 0, 33 measurements, 0 error rows, `AlgTestJClient version; 1.8.6`, about 1.5 minutes (the
  package README says 5–10 minutes; that is an overestimate), no FAULT in serial.
- `pqc` (`-op ALG_SUPPORT_BASIC`): rc 0, all 19 PQC rows yes, applet `1.8.6_jc32`, one CPLC equipment-ID line.
- Modes `all`/`symmetric` are unchanged from the earlier packages.

Steps:
1. Unzip, open PowerShell in the extracted folder, hold BOOTSEL while connecting the Pico 2.
2. Run the README block with `$mode = 'pqc'` (minutes). Expected: `DONE … client_exit_code=0`; in the CSV, the
   sections `javacardx.security.pqc.PQCKeyFactory` (12 rows), `javacardx.crypto.kem.KEM ALG_ML_KEM` (3) and
   `javacard.security.Signature ML-DSA` (4) all `yes`; a `CPLC;` line with data.
3. Reconnect with BOOTSEL and run `$mode = 'pqcperf'` (a few minutes). Expected: `DONE … client_exit_code=0`, a
   `…PQCONLY…csv` with 33 measurements and no error rows. One slow ML-DSA-65/87 `PQCKeyFactory_expandSeed()`
   measurement (~0.6 s) is a flash journal rollover and is expected.
4. Reconnect with BOOTSEL and run `$mode = 'all'` (5–8 hours). Expected: `Testing complete`, no `SW=6f00`, no panic
   or reader loss in `serial.log`.
5. Right after the `all` run, in the extracted directory, with `$reader` set as in the block:
   `Measure-Command { & java -jar tools\gp.jar --reader $reader -d --uninstall applet.cap *> uninstall.txt } | Select TotalSeconds`
   then `& java -jar tools\gp.jar --reader $reader --list`. Expected: DELETE `9000` within a few seconds, no USB
   re-enumeration, only the ISD listed.

Return each `results\<mode>_<timestamp>.zip`, `uninstall.txt`, the uninstall time and `--list` output, and any
USB/PC/SC disconnects, resets or timeouts. Include the chip ID, reader name, and UF2 and JAR hashes.

## WIN-RSACRT2048-B4ADBE13-20261127 — Windows RSA-CRT-2048 investigation

Created 2026-11-27 07:51 CET. For the Windows run that produced `6F00` at
APDU 7110, return the original serial log, chip ID and reader name. Describe
the starting card state, including whether the archived empty-card UF2 was
flashed immediately before installing jc32, what applets or runs preceded
option 2/1, and any resets or power cycles. Use existing logs if available.
The exact empty-card Linux replay returned `AA 9000` on EE11. See
[the progress record](../progress.md) under `20261127-0751-rsacrt2048-exact-replay`.

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
