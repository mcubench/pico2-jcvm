# Handoff — next steps only

Updated: 2026-11-15.

## Current external test package
- Fresh no-debug ARM `max_crypto`, object deletion ON, empty persistent storage.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-45ec828f-low-watermark.uf2`
- SHA-256: `47c0e5f194139e3720e43292f4a47468cbb6892990c9e1724813389b469117b9` (4,126,208 bytes).
- Windows package: `build/external-test/pico2-jcvm-maxcrypto-deletion-low-watermark-45ec828f.zip`
- ZIP SHA-256: `6f17e5b1006537178560e082f2a073eedc55171a475754f214095c02e05616bf`.
- Request: `WIN-OBJECT-DELETION-LOW-WATERMARK-45EC828F` in `.agent/EXT_REQUESTS.md`. Package includes upstream 1.8.4/jc305 CAP, supplied client 1.8.5, GPPro and Windows steps.
- Build passes: linked end `0x100f7920`; 34,528 bytes remain. No physical or Windows testing was performed.

## MEM-01 status and next actions
- Low-watermark fix committed as `4f8fd6c2`; physical and PC overlay paths use the pre-reclaim free-space snapshot for the trigger and compaction decision.
- Focused VPCD auto-GC regression: 512 unreachable-array replacements, one automatic trigger and compaction, 515/515 APDUs, live root preserved.
- Captured option-2 replay from empty storage passed 1,972 APDUs (two known GET DATA differences), auto-triggered at 77,889/98,304 bytes, and compacted to 17,177 sampled bytes; a restored emulator passed its final four APDUs. A second and third restored cycle passed APDUs 3–1972 (1,970 commands each), triggering at 78,211 and 78,213 / 98,304 bytes and compacting to 15,221 and 15,219 sampled bytes; the final live set was 367 files with a 2-byte cycle-to-cycle delta. Both restore checks passed the final four APDUs. Fresh-run overlay SHA-256 `c6174a01c0e1d6cbf34cee1e58310adc2a653849547a51831ddcdc3dd01e4b7e`; second-cycle state SHA-256 `79d91adbd5efe451143c485861e6af9334649773a9b578bf364c1bc8a403c2ad`; third-cycle state SHA-256 `2e5871ae4e5414129037452315391b876024f1c7bfbb3d43ef0fcc939a50b9dd`. Logs: `artifacts/results/mem01-lowwatermark-option2-20261115/`, `artifacts/results/resource01-apdu-replay-vpcd-20261115T080613Z/`, `...T080913Z/`, `...T081044Z/`, `...T081711Z/`, `...T082317Z/`. This is replay evidence, not a fresh client campaign.
- `./tools/test-object-deletion-policy`, `./tools/test-persistent-transaction`, `./tools/test-persistent-inventory`, and `./tools/test-rp2350-fs-overlay` pass. ARM max_crypto build passed after the source change.
- T7 fresh unchanged-client option 2 and T8 physical option 2/option 1 remain open. T10 write-failure retry remains open.

## Hardware/PCSC state
- Latest approved helper attempt found four matching Pico CCID devices, outside its 1–3-device limit, and refused ACL changes. Bounded elevated `./tools/list-readers --timeout 8` then timed out (exit 124); no exact reader was identified. Do not flash or claim a fresh client run from this state.
- A restricted direct VPCD socket run failed with `EPERM`; the same bounded replay succeeded via the elevated host-access path. Use that path for direct VPCD replay in this VM.
- Retry hardware work only after the approved helper and bounded reader enumeration succeed; use exact identified reader names and fail over across identified boards.
