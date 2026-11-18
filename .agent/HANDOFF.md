# Handoff — next steps only

Updated: 2026-11-18 13:20 CET.

## Current hardware state
- No JCAlgTest client or monitor is active. The fixed-image exploratory option 2 completed on 07E7; details, logs and hashes are in `artifacts/results/option2-exploratory-20261118T1049Z/07E74C5D7A038DEC/`.
- Last USB inventory: 22A9 is generic Pico firmware; F42D and 07E7 enumerate as JCVM CCID. The approved access helper granted USB access to both JCVM nodes, but the post-refresh `list-readers --pico-only` listed only `Pico 2 CHOUPI CCID [JCVM CCID] (07E74C5D7A038DEC) 00 00`. F42D is not currently usable through PC/SC. Do not restart pcscd again while campaigns are live.
- 07E7 still runs the previously tested normal `max_crypto` image, source `853543d6`, deletion ON/debug OFF, UF2 SHA-256 `89a744ca32134e838c38f9ff020ea7080305d76a0113757939432e2b9545d770`; its option-2 applet/store is not empty.
- A fresh ARM `max_crypto` build from docs tip `a746c295` completed with `BUILD:PASS` and mode normal; image SHA-256 `ce131357446b476def6c2d16741dfc2efa2418cca373674bc4273408030fb85f`. The build emitted warnings (including 60 `javac-os` Rust warnings); this new image was not flashed. Do not flash until the required warning-free build is established.

## Next action
- Option 1 exploratory run can continue on the sole confirmed 07E7 reader, with a unique card name/output path and separate serial capture. F42D is not a PC/SC failover token at this observation. This would use the existing non-empty option-2 card, so it cannot count as T8 acceptance.
- Before any PC/SC action, check live processes and this handoff. If a new token becomes available, identify by chip ID and keep per-token evidence separate. T7 remains blocked on `Virtual PCD 00 00` (`HOST-VPCD-PCSC-01`); keep T7/T8/T10 open.
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the two pre-existing root `*_already_measured.list` files.
