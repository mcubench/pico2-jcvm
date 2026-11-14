# Handoff — next steps only

Replace this file, do not append. Keep ≤ 60 lines. Task criteria are in
`TODO.md`, history in `progress.md`, recurring diagnoses in `.agent/QKB.md`.

Updated: 2026-11-14.

## Next actions
1. **MEM-01 T7:** perform the bounded VPCD option-2 proof through sync sequence
   ≥2,500. The previous post-fix run stopped at ≈800; this bounded overlay run
   reaches the target more cheaply than a full physical campaign. Log live
   record/byte counts at each compaction and alias-root counts.
2. **HOST-01:** inspect why the D92 final response stayed queued with an idle
   IN endpoint (`pending=1`, `in_busy=0`, no submit errors or completion) and why
   no wait/recovery telemetry appeared. Avoid another long physical run until
   the state transition is understood or instrumented.
3. **MEM-01 T8:** after T7, use the no-debug acceptance image and an empty-card
   image before the option-2/option-1 runs. Keep one identified board per run.

## Board state (volatile; re-enumerate before physical work)
- D92 `D92E743D5F640C0F`: diagnostic image SHA-256
  `079ae6883c60b2fe12013c3e84bac0f5a25f35554f1215ddb96304d67c01225a`; option-2
  stopped on `SCARD_E_NOT_TRANSACTED` at AES-192. Firmware stayed USB-visible.
- 8E `8E464F4E2BBC0A19` and 2BED `2BED26ED20556535`: runtime USB devices; prior
  option-2 sessions also stalled. All three runtime USB devices were visible
  after D92's failure, but PC/SC listed no physical reader after the approved
  helper refresh. No BOOTSEL device was visible then.
- Logs: `artifacts/results/jcalgtest-1.8.5-physical-20261114-ccid-debug/D92E743D5F640C0F/option2-performance/`.
