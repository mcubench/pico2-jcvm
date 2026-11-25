# External validation requests

## WIN-PQC-PERF-B4ADBE13-20261125 — Pico 2 firmware with JC 3.2 PQC (support and performance tests), SCP03 and all fixes since rc1

Created 2026-11-25 22:18 CET at `b4adbe13`. Replaces `WIN-PQC-SCP03-F3CF5729-20261125` (package `f3cf5729`, not yet
returned): firmware code is unchanged since `f9e7cf61` (only the CPLC source ID differs); new is the re-pinned
JCAlgTest 1.8.6/jc32 pair with PQC performance tests and a `pqcperf` mode. Use a dedicated test Pico 2.
Normal ARM max_crypto image (object deletion on, debug off); flashing erases applet storage.

In the firmware since `6d3bb526` (details in `progress.md`, 2026-11-25):
- Java Card 3.2 preview PQC API, native: ML-KEM-512/768/1024 (`javacardx.crypto.kem`), ML-DSA-44/65/87
  (`javacardx.security.pqc`, `Signature`), CAP 2.3 loading (`51d27d7e`, `8a56f4e0`).
- GP PUT KEY for a persistent SCP03 keyset; SCP03 C-ENC, R-MAC, R-ENC (`8b155dc5`, `aeecd243`).
- SELECT ends a GP secure channel session (`f9e7cf61`).

- [Empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-b4adbe13.uf2),
  SHA-256 `9373ebfb15b8427c8b978268e85733f2f6e43de9b217bc1692ca06dee315ae23`, 3,193,344 bytes, CPLC source ID `B4ADBE13D4694DF9`.
- [Windows test ZIP](../build/external-test/pico2-jcvm-maxcrypto-deletion-b4adbe13.zip),
  SHA-256 `a2b461c52118799816366af06ac00a4d0395042ea40e79aa5c27203fa91b66b6`, 18,535,299 bytes.
- Inside the ZIP: unchanged JCAlgTest 1.8.5/jc305 CAP (`applet.cap`, `fa64582e…48fb`) with the 1.8.5-20261121 client
  (`3cca70cb…a703`) for modes `all`/`symmetric`; JCAlgTest 1.8.6/jc32 CAP (`applet-jc32.cap`, `5f19662d…6f8f`) with
  its client (`tools/AlgTestJClient_v1.8.6_jc32.jar`, `0e98fd56…6c1`) for modes `pqc` and `pqcperf`.

Validation here (EE116F37C7E4A04E, this exact UF2 flashed, empty card, packaged GPPro and clients; the PowerShell
block itself was not run, no PowerShell here):
- `pqcperf` equivalent (client answers `2`, `4`): rc 0, 33 measurements, 0 error rows, about 1.5 minutes (the
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
