# External validation requests

## VMPERF-PICO2-AB-20261122 — physical bytecode timing

Created 2026-11-22 08:18 CET. The agent execution profile forbids the
escalation flag required by repository USB and PC/SC rules. Run this
comparison on a host with the required access. The host VPCD measurements
and ARM build are complete.

- Firmware and provenance are in
  [the CAP-cache benchmark artifact directory](../artifacts/build-audit/cap-view-cache-20261122/),
  as `pico2_choupi-cap-cache.uf2` and `arm-final-provenance.txt`.
  Use the normal max_crypto firmware. The baseline is `0cf976dd`, with UF2
  SHA-256 `effccdec0c2b3319d02db2955a94451e07aa8e396c5ccf9d6514a693603acda6`.
- Compare on one identified Pico 2, EE116F37C7E4A04E if available. Before
  flashing the candidate, confirm the CPLC and capture a baseline replay
  if that board still runs the baseline. If reproducing on another board,
  use the same chip ID for both runs. Archive the exact image hashes and
  preserve separate raw files for the two builds.
- Use standard GlobalPlatformPro LOAD for the unchanged [JCAlgTest
  1.8.5/jc305 CAP](../tests/fixtures/external/jcalgtest/1.8.5/AlgTest_v1.8.5_jc305.cap)
  on both runs. Its fixture SHA-256 is `fa64582e…b48fb`. Confirm
  `GETVERSION=1.8.5_jc305`. Use identical card preparation and CAP
  components for both builds. The [previous baseline](../artifacts/build-audit/bytecode-performance-20261122/hardware-ee11.log)
  contains the timings if the original firmware can no longer be replayed.
- For each run, capture bounded serial output with `tools/monitor` by chip
  ID while running [ApduTimer](../tests/hardware/ApduTimer.java) on the exact
  reader. The artifact directory contains the ordered arguments,
  one per line, in `physical-apdu-args.txt`. For example, after loading
  those arguments with Bash `mapfile -t apdus < physical-apdu-args.txt`,
  run `PICO2_HOST_USB_ACCESS=1 timeout 240 ./tools/env java tests/hardware/ApduTimer.java "$reader" "${apdus[@]}"`
  from the repository root with the required host access.
  Allow up to 240 seconds for the baseline and use the same limit for the
  candidate. Verify CPLC, GETVERSION and every status word. The workload
  responses should all be `AA9000`.
- Return each build's APDU and serial logs, CPLC, UF2 SHA-256, chip ID,
  exact reader name, and sample medians. For net latency, subtract the
  zero-repeat median from each batch and divide by the repetition count.
  Include the first measured commands after power-on separately from the
  repeated commands. Check serial output for `FAULT`, `PANIC` and
  `TEST:FAIL`, and retain the `CHOUPI:FS_APDU` records.

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
