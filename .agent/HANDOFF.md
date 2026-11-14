# Handoff — next steps only

Rules for this file: **replace it, do not append**. Keep it to ≤ 60 lines,
covering only the next concrete actions, current blockers and board state.
Task definitions and acceptance criteria live in `TODO.md`, history in
`progress.md`, recurring issues in `.agent/QKB.md`. The full pre-compaction
handoff history (233 blocks, to 2026-11-14 16:38) is in
`.agent/history/handoff-archive-20261114.md`.

Updated: 2026-11-14 (compacted by user request; state as of commit `4f225cbb`).

## User directive in force
Long simulator/emulator runs are permitted again, but each needs a stated reason
and benefit. First reason whether the target scenario can be reached more cheaply
than by a naive long run. See `AGENTS.md` → "Long simulator/emulator runs".

## Next steps (in order)
1. **MEM-01 T7 runtime proof (VPCD).** Target scenario: sync seq past ≈1,750
   (pre-fix `FS_WRITE_FAIL` at `bytes=94311`) with many live mixed-size records
   and alias bodies. First consider the cheaper churn-test extensions (a)–(e) in
   `TODO.md` T7. Otherwise run the bounded VPCD option-2 run: stop at sync seq
   ≥ 2,500 and log live records and bytes per compaction. Pass = no
   `FS_WRITE_FAIL`/`F205`, flat live bytes over the last 1,000 syncs, and bounded
   `reachable_alias`. If bytes still grow, use `PICO2_GC_SCAN_DIAGNOSTICS=1`
   `ALIAS_ROOT` counts.
2. **MEM-01 T8 physical.** The last 8E option-2 run (1.8.5 client, 1.8.4 CAP)
   failed after about 32 min with `SCARD_E_NOT_TRANSACTED` →
   `SCARD_E_READER_UNAVAILABLE` near `ALG_DES_ECB_ISO9797_M2 Cipher_init()`.
   There was no `F205`/`NO_RESOURCE`, and 717 deletion requests were served.
   This is a transport failure, not memory. Before rerunning:
   - Diagnose the CCID/PC/SC reader loss (offline state-machine review).
   - Feed the client's retry/skip prompt so it does not die on stdin.
   - Explain why the CSV reports client 1.8.4 when the 1.8.5 JAR was used.

   Logs: `artifacts/results/jcalgtest-1.8.5-physical-20261114/8E464F4E2BBC0A19/option2-performance/`.
3. **BOOTSEL-01 remaining acceptance.** Exercise CDC-1200 reset during a long
   synchronous applet operation and record the BOOTSEL latency. Idle CDC-1200
   works on 8E (3,119 ms).
4. When a second board enumerates, run option 1 on it (not before).

## Board state (volatile — re-check with the approved helper)
- 8E `8E464F4E2BBC0A19`: runtime USB visible, but its physical reader is missing
  from PC/SC; the helper restart did not restore it.
- D92 `D92E743D5F640C0F`: not enumerated after flash. Do not retry until it
  reappears.
- Other boards the user reported in BOOTSEL are not visible to the guest. Tell
  the user about this mismatch (via `.agent/EXT_REQUESTS.md`) if hardware work
  is blocked.
- Last flashed UF2 SHA-256: `92a603fb041259b6028360290f17f0fb9bc55ab2aac78ebe8b2739969609984d`.
