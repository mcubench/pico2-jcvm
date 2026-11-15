# Handoff — next steps only

Updated: 2026-11-15.

## Current external-test image
- Standalone empty-card UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-b6ccf96c-dirty.uf2` (SHA-256 `eeabd990b3ec2586cc7f165318425f003ffd6ce4b7a4865d1ecb9e328c9c1a99`, 4,127,232 bytes).
- Complete Windows package: `build/external-test/pico2-jcvm-maxcrypto-deletion-b6ccf96c-dirty.zip` (SHA-256 `a8cb6420e91d4c1a7832dd3e165527e011a47e80b881eda0a0691c8f63e493c3`). Request: `WIN-MEM01-EMPTY-NODEBUG-B6CCF96C`.
- Normal ARM `max_crypto`; object deletion ON; debug OFF; 0 ms startup wait; erased storage/no applet. Source HEAD `b6ccf96c` plus dirty worktree/submodules. No physical flash or Windows test performed.

## Active diagnostics and next steps
- MEM-01 T7/T10 VPCD option-2 campaign remains partial. Prior fresh-client run passed the prior RSA-CRT-private 2048 / ISO14888 F205 point and recorded four successful compactions. Saved overlay: 1,847 index slots, 1,514 live records, 73,245 bytes used, 25,059 free. Full plateau telemetry and physical T8 remain open. Logs: `artifacts/results/jcalgtest-support-vpcd-20261115T123504Z/`.
- Resume setup currently fails with `VPCD CAP registry does not match saved memory image`; a temporary erased-storage registry reconstruction also failed. Do not claim T7 complete. Diagnose resume pairing or rerun from a clean install.
- Physical USB/PCSC availability is not freshly confirmed. Before hardware work, run the approved helper and bounded reader listing through the required elevated host-access path; use the identified-board failover pool.
- LONGOP-01 Windows serial evidence request: `WIN-RSA1984-X50-SERIAL-20261115`.
