# External validation requests

## WIN-6F00-RESERVE-FIX-20261124 — Pico 2 firmware fixing the RSA-CRT `6f00` rows

Created 2026-11-24. Use a dedicated test Pico 2. Normal ARM max_crypto image
(object deletion on, debug off); flashing erases applet storage. Replaces
`WIN-VM-ARRAY-READS-20261122`, whose results were returned and analysed.

- [Empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-00b42283.uf2),
  SHA-256 `9f618b67cb91b638487df4bb03565487297c9bb441e5264ec90a3e8200c06be2`, 3,035,136 bytes, CPLC source ID `00B42283E3BD6A78`.
- [Windows test ZIP](../build/external-test/pico2-jcvm-maxcrypto-deletion-00b42283.zip),
  SHA-256 `1ce8db3bdce6f0e85656964e8e51f7e9cd980a884f6222b90a88796d07db20d3`.
- Unchanged JCAlgTest 1.8.5/jc305 CAP and 1.8.5-20261121 client (SHA-256 `3cca70cb…a703`), both inside the ZIP.
- Run JCAlgTest option 2 and option 1. Expected: no `SW=6f00`; the 30 methods listed in
  `progress.md` (step `20261124-0900-6f00-external-package`) were `6f00` before.
- Return CSVs, client log and serial log with chip ID, reader name, UF2 and JAR hashes.

## WIN-VM-ARRAY-READS-20261122 — Pico 2 primitive-array read firmware

Created 2026-11-22 09:44 CET. Use a dedicated test Pico 2. This normal ARM
max_crypto image erases applet storage during flashing. Object deletion is on
and debug switches are off.

- [Empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-be737f0e.uf2)
- UF2 SHA-256 `dcaf8616a825c5ba50b3f8aa1b4b4d544bb22b035d688e0d8e4875201de34f13`, 3,030,016 bytes.
- CPLC source ID `BE737F0EDEF8A050`.
- [Complete Windows test ZIP](../build/external-test/pico2-jcvm-maxcrypto-deletion-be737f0e.zip)
  with PowerShell runner, GPPro, the pinned CAP and client, serial capture, and checksums.
  ZIP SHA-256 `99c1e1ccc5e099d1d93e15a073b549c2c89ffef6e0c005912466d7269d21c420`.
- Use the unchanged JCAlgTest 1.8.5/jc305 [CAP](../tests/fixtures/external/jcalgtest/1.8.5/AlgTest_v1.8.5_jc305.cap)
  (SHA-256 `fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`)
  and the [1.8.5-20261121 client](../tests/fixtures/external/jcalgtest/1.8.5-20261121/client/AlgTestJClient.jar)
  (SHA-256 `3cca70cbc243a773b998fa43792a135e26934d4bf616e7baa88c5369db18a703`).

For the ZIP, extract it and follow the included README to run the Windows
workflow. For a manual run with the standalone firmware:

1. Hold BOOTSEL while connecting the Pico 2 and copy the UF2 to the boot drive.
2. Install the CAP using GlobalPlatformPro, then run JCAlgTest option 2
   (PERFORMANCE TEST, 256-byte data) and option 1 (All algorithms) as needed.
   Use separate identified boards for long parallel runs.
3. Return CSVs and client and serial logs with the Pico 2 chip ID, reader name,
   UF2 and JAR hashes, and any timeout, unexpected status word, or reset.

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
