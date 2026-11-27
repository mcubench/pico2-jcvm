# Handoff

Updated 2026-11-27. See [TODO.md](../TODO.md) for active work and
[EXT_REQUESTS.md](EXT_REQUESTS.md) for external dependencies.

## Active run

- P0 HOST-01 holds EE11 (`EE116F37C7E4A04E`). Exact normal max_crypto
  v0.3-rc1 empty image (SHA-256 `9373ebfb…f2f6e43de9b217bc1692ca06dee315ae23`),
  pinned jc32 CAP, Linux PC/SC reader suffix `00 00`. CPLC and GETVERSION
  confirmed the pair. Replay APDUs 1–7110 with serial capture is live in
  unified exec session `76593`, bounded at 9,000 seconds. Inspect
  `artifacts/results/p0-rc1-ee11/{replay,serial,install}.log`.
  Do not flash EE11 or refresh PC/SC during the replay.
- The warning-free normal ARM build from `fdbaa813` is packaged locally
  as `artifacts/results/p0-rc1-ee11/next-ram-pressure-empty.uf2`.

## Last observed physical state

- Development token `22A9DEAAA3D09795` (reader suffix `03 00`) runs normal
  max_crypto firmware from `05fb3d0b` (UF2 `2736e15a…1e81`) with storage
  preserved. ECTester `ECTester033x` is installed. Full 12-suite campaign
  (chunked via `ECTester033b`): 309/413, no FAULT, no ECDH/ECDSA UXSUCCESS
  (`artifacts/results/ectester-full-05fb3d0b-9795/`). Open: compressed P-224.
  Check live processes and USB identities before hardware or PC/SC work.
- EE11 was wiped for the exact-image replay above. 07E7 previously
  exhausted storage during an API probe. F42D retained ~83 KB of applet
  storage. Use the [QKB roster](QKB.md#physical-token-roster) for chip IDs.

## Next work

- Continue the Windows ECTester comparison against `39fd4199` using the new
  `9b03a1e0` package, and continue the KeyPair external regression using
  [EXT_REQUESTS.md](EXT_REQUESTS.md). Track ECTester follow-ups in PROFILE-01.
- CAPIN-07 and CAPIN-04 need scoped hardware verification. Start with the
  existing offline evidence referenced in [TODO.md](../TODO.md).
- Validate RAM-pressure collection on an empty normal max_crypto EE11 image
  with the full symmetric KAT, GP cleanup, serial output and flash telemetry.
  If the cutoff recurs, use `PICO2_DEBUG_HEAP_GC=1` for allocation and collection
  samples. RSA/RSA-CRT 4096 physical runs remain deferred at user request.
- v0.2 is published per the prioritized TODO. Current release work targets v0.3.
