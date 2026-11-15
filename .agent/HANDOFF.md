# Handoff — next steps only

Updated: 2026-11-15.

## Current external test package
- Fresh no-debug ARM `max_crypto`, object deletion ON, empty persistent storage.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-45ec828f-low-watermark.uf2`
- SHA-256: `47c0e5f194139e3720e43292f4a47468cbb6892990c9e1724813389b469117b9` (4,126,208 bytes).
- Windows package: `build/external-test/pico2-jcvm-maxcrypto-deletion-low-watermark-45ec828f.zip`
- ZIP SHA-256: `6f17e5b1006537178560e082f2a073eedc55171a475754f214095c02e05616bf`.
- Request: `WIN-OBJECT-DELETION-LOW-WATERMARK-45EC828F` in `.agent/EXT_REQUESTS.md`. Package includes upstream 1.8.4/jc305 CAP, supplied client 1.8.5, GPPro and Windows steps.
- Build passes: linked end `0x100f7920`; 34,528 bytes remain. VPCD auto-GC stress passes 515/515 APDUs; no physical/Windows testing was performed.

## MEM-01 status and next actions
- Low-watermark reclamation and tombstone compaction use the pre-sweep free-space snapshot on both physical and PC overlay paths. Regression: 512 unreachable-array replacements, one auto-trigger, one compaction, live root preserved. Log: `artifacts/logs/object-deletion-vpcd-20261115T073454Z.log`.
- Commit this focused VPCD parity/threshold fix after review; preserve other worktree changes.
- T7 unchanged-client option 2 with the real overlay backend remains open; this focused fixture is not acceptance. T8 physical option 2 and option 1 remain open. T10 direct failed-write retry remains open.
- Last hardware refresh stalled at PC/SC reader enumeration; no recent physical board state is established. Before hardware work, refresh the approved helper and enumerate readers, then continue on another identified board if one disappears.
