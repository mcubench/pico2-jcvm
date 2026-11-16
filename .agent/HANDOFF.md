# Handoff — next steps only

Updated: 2026-11-16.

## Current state
- Low-watermark deletion batches now compact tombstones even for applet-requested `JCSystem.requestObjectDeletion()`. The fix is committed as `98fcb151` (`20261116-0800-mem01-lowwatermark-compaction`). Focused policy, overlay, normal ARM build, and VPCD checks passed.
- Fresh external image is `artifacts/firmware/pico2-jcvm-max_crypto-object-deletion-on-empty-card-98fcb151.uf2` (SHA-256 `e1210b1afdf20afa335e32b188541804384ada64f5e4e074311b03e06e7595d8`) and ZIP is `build/external-test/pico2-jcvm-maxcrypto-deletion-98fcb151.zip` (SHA-256 `327b6454fce940bb972e1f50474d798dff964ec0ecc552acd84b843aa85505a4`). Normal mode, deletion ON, debug OFF, empty card; no physical/Windows validation.
- Current external request: `WIN-MEM01-EMPTY-NODEBUG-98FCB151` in `.agent/EXT_REQUESTS.md`.
- Physical reader enumeration previously timed out after helper access refresh. Refresh with the authorized helper and bounded `./tools/list-readers` before physical work; do not assume boards absent or available.

## MEM-01 next actions
1. Run a clean, unskipped 1.8.5 option-2 VPCD campaign from a freshly initialized/verified empty-card plus CAP-registry pair; prior run stopped at RSA private CRT 1536 `setDP1()` without a pass marker.
2. Implement T10 pre-cleanup high-water/file-count telemetry and a >90% runner failure gate; then rerun focused and full suites.
3. When identified physical boards enumerate, run option 2 and option 1 on separate normal max-crypto empty-card Pico 2 tokens when possible. Record chip ID, firmware/JAR hashes, per-device results and bounded serial logs.
4. Continue diagnosing HOST-01/LONGOP-01 RSA operation stalls and the unmet overlay-growth plateau gate. Safe-point reclaim/write retry remains open pending a proven safe replay point.
