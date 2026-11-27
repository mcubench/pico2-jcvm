# Handoff

Updated 2026-11-27. See [TODO.md](../TODO.md) for active work and
[EXT_REQUESTS.md](EXT_REQUESTS.md) for external dependencies.

## Current EE11 work

- P0 HOST-01 exact v0.3-rc1 jc32 replay completed 7,110 APDUs. APDU 7110
  returned `AA 9000`; Windows serial and starting state are requested.
- P0 RAM-pressure run passed 208/208 checks and returned EE11 to an ISD-only
  state. EE11 (`EE116F37C7E4A04E`, reader suffix `00 00`) runs the normal
  max_crypto RV-48 validation image built from the worktree based on
  `d138ef70`. The preceding image passed the standard transaction probe
  18/18 with cleanup
  (`artifacts/results/p1-ee11-transaction/`). The preceding KAT logs are in
  `artifacts/results/p0-rc1-ee11/`. Check live processes before PC/SC work.

## Last observed physical state

- Development token `22A9DEAAA3D09795` (reader suffix `03 00`) runs normal
  max_crypto firmware from `05fb3d0b` (UF2 `2736e15a…1e81`) with storage
  preserved. ECTester `ECTester033x` is installed. Full 12-suite campaign
  (chunked via `ECTester033b`): 309/413, no FAULT, no ECDH/ECDSA UXSUCCESS
  (`artifacts/results/ectester-full-05fb3d0b-9795/`). Open: compressed P-224.
  Check live processes and USB identities before hardware or PC/SC work.
- EE11 is the working token for the P0 physical checks. 07E7 previously
  exhausted storage during an API probe. F42D retained ~83 KB of applet
  storage. Use the [QKB roster](QKB.md#physical-token-roster) for chip IDs.

## Next work

- Continue the Windows ECTester comparison against `39fd4199` using the new
  `9b03a1e0` package, and continue the KeyPair external regression using
  [EXT_REQUESTS.md](EXT_REQUESTS.md). Track ECTester follow-ups in PROFILE-01.
- Continue P0 physical acceptance. CAPIN-04 frame limit and RV-60 legal
  matrix passed on EE11, and fixture cleanup returned it to ISD-only.
  Remaining subcases are tracked in [TODO.md](../TODO.md).
- Continue batched P0 physical checks on EE11. RSA/RSA-CRT 4096 physical runs
  remain deferred at user request.
- v0.2 is published per the prioritized TODO. Current release work targets v0.3.
