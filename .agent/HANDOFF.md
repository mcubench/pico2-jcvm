# Handoff — next steps only

Updated: 2026-11-14.

## Current external image
- Normal ARM `max_crypto`; object deletion ON; all debug/trace options OFF;
  empty persistent storage and no preinstalled applet.
- UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-normal-empty-card-97d9027e.uf2`
- UF2 SHA-256: `9157bc1a41661f04ded8fff40136c27825c427a90f87bc03742d84711fd358ad`
- Windows package: `build/external-test/pico2-jcvm-object-deletion-normal-windows-97d9027e.zip`
- Package SHA-256: `eb1b6e9bdad555ddac5f1241f999c60a37ebffebda191644a10c250e0063b7f3`
- External request: `WIN-OBJECT-DELETION-NORMAL-97D9027E` in `.agent/EXT_REQUESTS.md`.
- Build, empty-card packaging, internal checksums, and ZIP integrity passed.
  The package has not been flashed or tested on physical hardware here.

## Next actions
1. Await external Windows test results for the package above. Require the exact
   UF2 hash, Windows version, reader/ATR, chip ID, full result folders and
   serial logs; keep option 2 and option 1 results separate by token.
2. **MEM-01 T7:** finish bounded overlay-backed VPCD option-2 proof through
   sync sequence ≥2,500, including live records/bytes and alias roots at
   compaction. Earlier high-occupancy synthetic overlay tests pass; unchanged
   JCAlgTest option-2 VPCD acceptance is still open.
3. **HOST-01 / MEM-01 T8:** after T7, use a current normal image on identified
   physical boards for option 2 and option 1. Refresh the approved reader
   helper and verify exact PC/SC reader enumeration before each campaign.
