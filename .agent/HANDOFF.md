# Handoff

Updated 2026-11-27. See [TODO.md](../TODO.md) for active work and
[EXT_REQUESTS.md](EXT_REQUESTS.md) for external dependencies.

## Current EE11 work

- P0 HOST-01 exact v0.3-rc1 jc32 replay completed 7,110 APDUs. APDU 7110
  returned `AA 9000`; Windows serial and starting state are requested.
- P0 RAM-pressure acceptance is using EE11 (`EE116F37C7E4A04E`), reader
  suffix `00 00`, with a normal max_crypto empty-card build from `fdbaa813`.
  Current flash and preceding replay logs are under
  `artifacts/results/p0-rc1-ee11/`. Check live processes before PC/SC work.

## Last observed physical state

- Development token `22A9DEAAA3D09795` (reader suffix `03 00`) runs normal
  max_crypto firmware from `05fb3d0b` (UF2 `2736e15a…1e81`) with storage
  preserved. ECTester `ECTester033x` is installed. Full 12-suite campaign
  (chunked via `ECTester033b`): 309/413, no FAULT, no ECDH/ECDSA UXSUCCESS
  (`artifacts/results/ectester-full-05fb3d0b-9795/`). Open: compressed P-224.
  Check live processes and USB identities before hardware or PC/SC work.
- EE11 is reserved for the P0 symmetric KAT. 07E7 previously
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
