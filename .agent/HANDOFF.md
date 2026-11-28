# Handoff

Updated 2026-11-28. See [TODO.md](../TODO.md) for active work and
[EXT_REQUESTS.md](EXT_REQUESTS.md) for external dependencies.

## Development diagnostics audit

- [Audit checklist](../docs/debugging_code_20261128.md) covers nine categories.
  Existing normal ARM max_crypto artifact from `69c62719` fails the INV-24
  trace check. VM and SELECT strings are present in its UF2 flash payload.
  Source, artifacts and package paths were inspected offline. No board state
  was checked or changed.
  The [migration plan](../docs/diagnostics-migration-plan.md) and standing
  AGENTS.md review checks are documented. Firmware implementation is pending.

## Current EE11 work

- P0 HOST-01 exact v0.3-rc1 jc32 replay completed 7,110 APDUs. APDU 7110
  returned `AA 9000`; Windows serial and starting state are requested.
- P0 RAM-pressure ran 208/208 checks. EE11 (`EE116F37C7E4A04E`, reader
  suffix `00 00`) now runs the normal max_crypto build from `5f24421e`. The expanded transaction probe passed 25/25, including
  ArrayLogic exception catches and persistent byte, short and int rollback, and atomic capacity rejection. Cleanup passed.
  Oversized non-atomic updates survived a firmware reboot. Cleanup returned
  EE11 to ISD-only. See progress `20261127-0851-arraylogic-int-rollback`.

## Last observed physical state

- Development token `22A9DEAAA3D09795` (reader suffix `03 00`) runs normal
  max_crypto firmware from `05fb3d0b` (UF2 `2736e15a…1e81`) with storage
  preserved. ECTester `ECTester033x` is installed. Full 12-suite campaign
  (chunked via `ECTester033b`): 309/413, no FAULT, no ECDH/ECDSA UXSUCCESS
  (`artifacts/results/ectester-full-05fb3d0b-9795/`). Open: compressed P-224.
  Check live processes and USB identities before hardware or PC/SC work.
- 07E7 `07E74C5D7A038DEC` holds package `3be60283` (empty-card image plus
  pqcperf and JCMathLib runs). It is not ISD-only. Wipe it before acceptance use.
- EE11 is the working token for the P0 physical checks. 07E7 previously
  exhausted storage during an API probe. F42D retained ~83 KB of applet
  storage. Use the [QKB roster](QKB.md#physical-token-roster) for chip IDs.

## Next work

- Windows: one request `WIN-CAMPAIGN-3BE60283-20261127` (`run-all.ps1`).
  When the ZIP returns, run `tools/check-windows-results.py ZIP` and close the
  items it passes. The ECTester comparison is closed on Linux evidence. Open:
  the intermittent `6999` (HOST-01, see progress `20261127-1015-…`).
- Continue P0 physical acceptance. CAPIN-04 frame limit and RV-60 legal
  matrix passed on EE11, and fixture cleanup returned it to ISD-only.
  Remaining subcases are tracked in [TODO.md](../TODO.md).
- Continue batched P0 physical checks on EE11. RSA/RSA-CRT 4096 physical runs
  remain deferred at user request.
- v0.2 is published per the prioritized TODO. Current release work targets v0.3.
