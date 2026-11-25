# External validation requests

## WIN-PQC-SCP03-F9E7CF61-20261125 — Pico 2 firmware with JC 3.2 PQC, SCP03 enc/R-MAC and all fixes since rc1

Created 2026-11-25 14:50 CET. Replaces `WIN-GP-DELETE-WATCHDOG-20261124` (package `6d3bb526`) and
`WIN-VM-ARRAY-READS-20261122` (package `be737f0e`); this image contains both fixes. Use a dedicated test Pico 2.
Normal ARM max_crypto image (object deletion on, debug off); flashing erases applet storage.

New since `6d3bb526` (details in `progress.md`, 2026-11-25):
- Java Card 3.2 preview PQC API, native: ML-KEM-512/768/1024 (`javacardx.crypto.kem`), ML-DSA-44/65/87
  (`javacardx.security.pqc`, `Signature`), CAP 2.3 loading (`51d27d7e`, `8a56f4e0`).
- GP PUT KEY for a persistent SCP03 keyset; SCP03 C-ENC, R-MAC, R-ENC (`8b155dc5`, `aeecd243`).
- SELECT now ends a GP secure channel session (`f9e7cf61`). Before, the PC/SC connection after a GPPro
  run inherited the session, so JCAlgTest logged "CPLC; failed to obtain CPLC".

- [Empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-f9e7cf61.uf2),
  SHA-256 `d46b27b6fdeef09bf1a663c8de019fc14f13a442a70dba489b96ac34b2d31dec`, 3,193,344 bytes, CPLC source ID `F9E7CF61F94B9A22`.
- [Windows test ZIP](../build/external-test/pico2-jcvm-maxcrypto-deletion-f9e7cf61.zip),
  SHA-256 `6e8cfc8f80b36671f14548123d59dd311d2b97d70362aca9e07602933bcc06d8`.
- Inside the ZIP: unchanged JCAlgTest 1.8.5/jc305 CAP (`applet.cap`, `fa64582e…48fb`) with the 1.8.5-20261121 client
  (`3cca70cb…a703`) for modes `all`/`symmetric`; JCAlgTest 1.8.6/jc32 CAP (`applet-jc32.cap`, `98365aae…bfce`) with
  its client (`tools/AlgTestJClient_v1.8.6_jc32.jar`, `615d7be4…c4f4`) for mode `pqc`.

Validation here (EE116F37C7E4A04E, this exact UF2, empty card): packaged GPPro installed `applet-jc32.cap`, then the
packaged 1.8.6 client ran `-op ALG_SUPPORT_BASIC -cardname … -fresh -outpath …` (the `pqc` command line): rc 0,
4506 rows, all 19 PQC rows yes, no NO_RESOURCE/error/6F00 rows, CPLC present. The PowerShell block itself was not run
(no PowerShell here). Modes `all`/`symmetric` are unchanged from the earlier packages.

Steps:
1. Unzip, open PowerShell in the extracted folder, hold BOOTSEL while connecting the Pico 2.
2. Run the README block with `$mode = 'pqc'` (minutes). Expected: `DONE … client_exit_code=0`; in the CSV, the
   sections `javacardx.security.pqc.PQCKeyFactory` (12 rows), `javacardx.crypto.kem.KEM ALG_ML_KEM` (3) and
   `javacard.security.Signature ML-DSA` (4) all `yes`; a `CPLC;` line with data.
3. Reconnect with BOOTSEL and run `$mode = 'all'` (5–8 hours). Expected: `Testing complete`, no `SW=6f00`, no panic
   or reader loss in `serial.log`.
4. Right after the `all` run, in the extracted directory, with `$reader` set as in the block:
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
