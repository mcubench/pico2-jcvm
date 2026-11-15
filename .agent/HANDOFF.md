# Handoff — next steps only

Updated: 2026-11-15.

## Current external-test image
- Standalone empty-card UF2: `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-54bba2b3-dirty.uf2` (SHA-256 `11da9cebad8c04884084e95bbd40f5b7282a65a579e18abfb72abbeb8d81d98f`, 4,127,232 bytes).
- Fresh normal ARM `max_crypto`, deletion ON, debug OFF; built from base HEAD `54bba2b3` plus dirty worktree/submodules. Build provenance: `build/arm/build-provenance.txt`. Firmware end `0x100f7b08`, 34,040 bytes remain. Empty persistent storage; no applet preinstalled. External instructions: `WIN-OBJECT-DELETION-MAXCRYPTO-54BBA2B3-DIRTY`. No physical flash or Windows test here.

## Active diagnostics and next steps
- Fresh 1.8.5-client / unchanged 1.8.4 CAP `ALG_PERFORMANCE_STATIC` VPCD run stopped after the previous F205 point: RSA-CRT-private 2048 / ISO14888 `Cipher_doFinal()` and `Cipher_init()` both returned `9000`. Four auto-compactions peaked at 78,075/98,304 bytes and erased 8 records each. The quiescent saved overlay passes integrity: 1,847 slots, 1,514 live records, 73,245 used, 25,059 available. Stop was intentional at a later RSA-public-3072 case; resulting reader removal and ptrace shutdown panic are interruption noise. Full plateau telemetry and physical T8 remain open. Logs: `artifacts/results/jcalgtest-support-vpcd-20261115T123504Z/`.
- `MEM-01 T7/T10` remains open until fresh-client run clears its prior failure and physical T8 acceptance passes. Preserve the 96 KiB / 8,192-slot INV-28 limit.
- Source step `20261115-1340-mem01-prefix-tags` is committed as `d508cae8`; overlay, transaction, inventory suites and normal ARM build passed. Continue with the next concrete T10 acceptance/test gap after the active run.
- Physical USB/PCSC availability is not freshly confirmed. Before hardware work, run the approved helper and bounded reader listing through the required elevated host-access path; use the identified-board failover pool.
- LONGOP-01 serial evidence request is `WIN-RSA1984-X50-SERIAL-20261115`; its Windows test instructions now point at the current D508CAE8 image.
