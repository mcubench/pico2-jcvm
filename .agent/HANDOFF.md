# Handoff — next steps only

Updated: 2026-11-15.

## Current external-test image
- Standalone empty-card UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-5abadb7d-dirty.uf2` (SHA-256 `b939546c33be122e0d4d82072001ebc1f1e07ea75f7bb1872869889d89730801`, 4,127,232 bytes).
- Complete Windows package: `build/external-test/pico2-jcvm-object-deletion-nodebug-empty-card-5abadb7d-dirty.zip` (SHA-256 `3fbdc2be502f78e4b2c3ae85d92ac8f36b561de30444eeeffade25bb8b9c2e83`). Request: `WIN-MEM01-EMPTY-NODEBUG-5ABADB7D`.
- Normal ARM `max_crypto`; object deletion ON; debug OFF; 0 ms startup wait; erased storage/no applet. Includes schema-v5 persistent tag-prefix headroom change; overlay limit remains 96 KiB / 8,192 slots. Source HEAD `5abadb7d` plus dirty worktree/submodules. No physical flash or Windows test performed.

## Active diagnostics and next steps
- MEM-01 T7/T10 VPCD option-2 campaign remains partial. Prior fresh-client run passed the prior RSA-CRT-private 2048 / ISO14888 F205 point and recorded four successful compactions. Saved overlay: 1,847 index slots, 1,514 live records, 73,245 bytes used, 25,059 free. Full plateau telemetry and physical T8 remain open. Logs: `artifacts/results/jcalgtest-support-vpcd-20261115T123504Z/`.
- Resume setup currently fails with `VPCD CAP registry does not match saved memory image`; a temporary erased-storage registry reconstruction also failed. Do not claim T7 complete. Diagnose resume pairing or rerun from a clean install.
- Physical USB/PCSC availability is not freshly confirmed. Before hardware work, run the approved helper and bounded reader listing through the required elevated host-access path; use the identified-board failover pool.
- LONGOP-01 Windows serial evidence request: `WIN-RSA1984-X50-SERIAL-20261115`.
