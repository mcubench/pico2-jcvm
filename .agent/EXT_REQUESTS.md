# External validation requests

## ECTESTER-INV09-20261126 — Extended APDU decision

Created 2026-11-26 15:19 CET. Confirm whether to extend INV-09 for the
supplied [ECTester 2.2.2 CAP](../inbox/ectester/ectester222.cap), or supply
a short-APDU applet and matching reader that can run the required suites
within INV-09.

On 22A9, GPPro loaded the package, then INSTALL returned `6A88` at
stage 10. The CAP imports `javacardx.apdu` and the applet implements
`ExtendedLength`. The upstream Java Card instructions say some commands
require extended APDUs. See the [install transcript](../artifacts/results/ectester-22A9DEAAA3D09795/install.log)
and [upstream Java Card instructions](https://github.com/crocs-muni/ECTester#javacard-testing).

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

## WIN-MAXCRYPTO-RC-V0.2-RC1 — release candidate v0.2-rc1 (commit 81c7ac5d), JCAlgTest options 1 and 2

Created 2026-11-21 15:33 CET. Replaces `WIN-MAXCRYPTO-ONESHOT-D7E90A18`, whose results were
returned and analysed (`inbox/pico2-jcvm-maxcrypto-deletion-d7e90a18_20261120/`: 18 ILLEGAL_VALUE
fixed in c42fab06; RSA-CRT 3072 `6f00` was the pre-9228c583 out-of-memory crash, gone on the current
image).

This is the firmware intended for GitHub Releases, tagged `v0.2-rc1` (annotated tag on
`81c7ac5d0d86c9be2f17912ffcdef244a1501390`; local only, not pushed). Normal ARM `max_crypto`
image: object deletion ON, every debug switch OFF, erased storage region, no preinstalled applet.

- [Standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-81c7ac5d.uf2)
- UF2 SHA-256: `1cbb049426f690d31070e7db3a23f476ec7459ddd7dab3fe7c5b5f5dd12f9f20`; size 3,028,992 bytes.
- [Complete Windows test package](../build/external-test/pico2-jcvm-maxcrypto-deletion-81c7ac5d.zip)
- ZIP SHA-256: `5f69328efa352f1db1529361c8c4e6e10eaa9ff75f2297503f7917d29543b8ec`; size 18,260,522 bytes.
- CAP: unchanged JCAlgTest 1.8.5/jc305 (`applet.cap`, SHA-256 `fa64582e…48fb`).
- Client: **1.8.5-20261121** (`tools/AlgTestJClient_v1.8.5.jar`, SHA-256
  `3cca70cbc243a773b998fa43792a135e26934d4bf616e7baa88c5369db18a703`; fewer internal repetitions
  for asymmetric algorithms, so `all` should finish well under the earlier 6.3 h). Same file name and
  PowerShell block (`README.md` / `run-test.ps1`, `$mode = 'all'` or `'symmetric'`) as before.
- CPLC source identifier `0x81C7AC5D0D86C9BE`; linked end `0x10071b00`, 582,912 bytes headroom.

New since d7e90a18 (details in `progress.md`, 2026-11-20/21):
- Object-deletion reclaim no longer panics when RAM runs out; it defers the request.
- HMAC keys accept block-length keys (LENGTH_HMAC_*_BLOCK_n); AES MAC/CMAC accept AES-192/256 keys;
  JC305 `HMACKey.clearKey/getSize` dispatch fixed.
- Many native algorithms added by the other development session (SHA3, AES-CFB/XTS/GCM/CCM,
  CRC16/32, CTR_DRBG, pseudo-random RandomData, RSA-4096 keygen, MessageDigest streaming, ...),
  so the support CSV will show more `yes` rows than d7e90a18.
- CAP loader and bytecode hardening (CAPIN-05/06/07).

Validation here: build exit 0, no C/C++ or Rust warnings (rebuild 0 warnings), `PACKAGE:PASS`,
`unzip -t` clean. Physical smoke of this exact UF2 on 07E74C5D7A038DEC: empty card (GPPro listed
nothing), unchanged CAP installed and SELECTABLE, JCAlgTest with the bundled client on the 24
HMAC-key / AES-MAC methods: `Testing complete`, no `6f00`, 3 ILLEGAL_VALUE only for AES MAC with a
512-bit AES key (expected: 512-bit AES keys now exist for XTS and do not suit AES MAC), serial
without PANIC/FAULT/NO_RESOURCE. The PowerShell block itself has not been run here.

Steps:
1. Unzip, open PowerShell in the extracted folder, hold BOOTSEL while connecting the Pico 2.
2. Set `$mode` (`all` first if only one run is possible) and paste the README block.
3. If the block stops with an error, return the `results\<mode>_<timestamp>` folder as it is.

Return:
- each `results\<mode>_<timestamp>.zip` (client log/CSV, `serial.log`, GPPro output, `run-identity.txt`);
- any USB/PC/SC errors, resets, stalls or timeouts, with their times.

Key questions:
- Does `all` reach `Testing complete` with no `*** PANIC ***`, `FAULT`, reader loss or `6f00`
  (in particular through RSA-3072/4096 CRT)?
- Are any `ILLEGAL_VALUE` / `ILLEGAL_USE` rows present other than RSA-512 OAEP, RSA NOPAD
  (input-dependent) and AES MAC with 512-bit AES keys?
- Is the `HEADROOM:` verdict PASS, and does the serial log show `OBJECT_DELETION:SKIP`?

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
