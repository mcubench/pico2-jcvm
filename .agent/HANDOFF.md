# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Latest `./tools/build arm --profile max_crypto` passed in normal mode, object deletion ON, debug switches OFF; flash headroom is 35,976 bytes.
- Latest authorized hardware refresh found 0 Pico 2 boards; elevated `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only` returned `list() failed`. No flash or cycle was attempted.
- LONGOP-01 T1 now has focused VPCD telemetry for RSA-1984 and RSA-1536 ×50: 51 native `doFinal` calls, live arrays 112→163→112 for each. Host `mallinfo2` sampled increases are ~39 KB; this does not measure Pico arena high-water. T1/D2 remain open pending target telemetry. Logs are under `artifacts/results/longop01-rsa1984-vpcd-20261117/`.
- T7 synthetic overlay harness and 1,590-APDU transcript replay passed; unchanged-client option 2 and physical T8 acceptance remain open.

## Next actions
1. Add embedded allocator/arena high-water telemetry for the RSA ×50 path or collect it on an identified physical Pico 2; rebuild ARM, then run the full JCVM cycle and inspect finite serial output.
2. Continue LONGOP-01 triage with the archived D1 failure log: distinguish card/reader stall from target panic/reset and retain D2 open until measured.
3. When PC/SC is available, run the unchanged 1.8.5 client option 2 and physical acceptance with per-device identity, hashes, result directories and telemetry.
4. Preserve existing uncommitted nested `choupi/os` worktree changes and root untracked SDK submodules.
