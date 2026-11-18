# Handoff — next steps only

Updated: 2026-11-18 12:27 CET.

## Current run
- Physical exploratory JCAlgTest option 2 is active on chip `07E74C5D7A038DEC`, exact reader `Pico 2 CHOUPI CCID [JCVM CCID] (07E74C5D7A038DEC) 00 00`. Client log shows menu `2 -> PERFORMANCE TEST -> 2` and `Used reader` confirms that exact token. This is not T8 acceptance until VPCD T7 passes.
- Firmware is normal `max_crypto`, source commit `853543d6`, object deletion ON/debug OFF. Firmware UF2 SHA-256 `89a744ca32134e838c38f9ff020ea7080305d76a0113757939432e2b9545d770`; empty-card UF2 SHA-256 `d0701dbfe103cb25bad3a05c38efdf8838e38747302a364a00d553b0c4e0fc8a`. Fresh empty flash verified, GPPro showed ISD only, unchanged 1.8.4/jc305 CAP installed and selectable. Client JAR 1.8.5 SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`; CAP SHA-256 `cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`.
- At 12:27 CET: 262 method groups, 3,872 APDU responses, 2,267 clean PRE_CLEANUP/TIMING records. Peak 707 files / 15,148 of 98,304 bytes; latest 617 / 13,151. No malformed telemetry, TEST:FAIL, FAULT, PANIC, NO_RESOURCE or PC/SC errors. Live heap has risen from 30 arrays / 16 instances at startup to 111 / 26; the last 100 samples hold at 111 / 26. This is still family setup; no full-suite plateau claim. Watch through completion or the 14,400 s wrapper bound; stop early on >90% storage or any failure/transport marker.
- Serial session is finite at 15,000 s. Artifacts: `artifacts/results/option2-exploratory-20261118T1049Z/07E74C5D7A038DEC/attempt1/`.
- JCAlgTest wrote `pico2_maxcrypto_20261118_07E74C5D7A038DEC_fixed_option2____PERFORMANCE_SYMMETRIC_DATAFIXED__already_measured.list` at repository root. Preserve it while the client is active; move it into the attempt folder after exit.

## Hardware access
- Do not run the PC/SC helper or list readers during the active run. It can restart PC/SC and abort every campaign. Live-run check was clear before launch. USB enumeration at launch identified 07E7 and F42D; a generic `22A9DEAAA3D09795` Pico was also present. PC/SC listed only 07E7.
- F42D's handoff-listed monitor PID was already gone at 11:49 CET. Do not switch tokens unless 07E7 becomes unavailable; if it does, record the failure and re-identify all tokens by chip ID.

## After this run
1. Capture final client/serial exit states, check every APDU status and run `tools/check-object-deletion-headroom.py` on the whole serial log. Report malformed lines, if any; do not treat invalid telemetry as T10 evidence.
2. Archive the generated resume list in the attempt folder and add the final campaign identity/hashes.
3. Append a progress checkpoint with measured resource trends and result; commit only the campaign artifacts/docs for that step.
4. Continue MEM-01: VPCD T7 request `HOST-VPCD-PCSC-01` is outstanding; physical T8 and T10 remain gated/open. Do not claim acceptance from this run.

## Unrelated worktree files
- Preserve `.agent/code_review_instructions_memory_20261117.md`, `inbox/`, and the two existing root `*_already_measured.list` files.
