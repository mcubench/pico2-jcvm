# Reality check: human experience of agentic development in pico2-jcvm

Author: Claude Opus 5.5 (`claude-opus-5-5`), 2026-11-14
Inputs: `HUMAN.md` (sections *Experience* and *Future plans*), `git log` (2,842 commits, 2026-10-09 → 2026-11-14),
`progress.md` (29,580 lines, 1,899 records), `TODO.md`, `.agent/{HANDOFF,QKB,EXT_REQUESTS,SESSION_CONTINUITY}.md`, `AGENTS.md`, `SKILL.md`, `docs/`.
Nothing in the repository was modified.

How to read this document. Each part has three blocks:

- **Your experience**: what `HUMAN.md` says, condensed.
- **Evidence**: what git and the agent files show, with a verdict: ✅ confirmed, ◐ partly or nuanced, ❌ contradicted, ? not verifiable.
- **Recommendation**: my suggestions. These are opinions, not observations.

---

## 0. The project in numbers

| Metric | Value | Comment |
|---|---|---|
| Duration | 37 days (2026-10-09 → 11-14) | Commits on every day; low days were 10-13 (2) and 10-22 (7) |
| Commits | 2,842 (2,529 `petrs`, 312 `petrsagent`) | About 77 per day on average, peaking at 181 on 10-30 |
| Commits touching only Markdown/bookkeeping | **1,883 (66 %)** | 1,548 subjects start with "Record…", "docs:", "Backfill…" or mention a commit hash/SHA |
| `progress.md` records | 1,899; only ~200 have a `Model:` line | The model field was added late, and models name themselves inconsistently ("GPT-6", "GPT-6 Codex", "GPT-6.1 (Codex)", "gpt-6") |
| `.agent/HANDOFF.md` | 14,877 lines, **233 stacked "# Current handoff" blocks** | It is supposed to hold the *current* handoff |
| `.agent/QKB.md` | 1,112 lines, 48 sections (plus a 699-line `QKB_INDEX.md`) | Compacted once (archive `2026-11-11-qkb-compaction`) |
| `TODO.md` | 1,190 lines, 91 open `[ ]` and 11 `[x]` | Finished items are pruned or archived, so the done/open ratio is not meaningful |
| Review documents | 5 (`review_20261103`, `_20261106`, `_fable52_20261110`, `_opus55_202611100`, `_memory_analysis_opus55_20261113`) | |

The headline finding: about two thirds of the agent's commits are bookkeeping. The rule "commit + `progress.md` record that contains the commit SHA" cannot be satisfied in one commit, because a commit cannot contain its own SHA. Agents therefore make a second "Record/Backfill commit hash" commit almost every time. This inflates history, makes `git log` hard for a human to read, and costs tokens.

---

## 1. Steering agents: specification, rules, guardrails

### Your experience
- Working functionality first gives a setup that can be refined later.
- Without a concrete specification, agents drift. GPT6-Luna built firmware with JCAlgTest preinstalled instead of an empty card, and put crypto into Java providers inside the ROM mask. You found the second case by accident and fixed it with a rule in `SKILL.md`.
- Agents took unrequested initiatives: fuzzing, RSA exponent blinding, Shareable.
- When the larger reviewer model is asked to tidy up, it sometimes starts implementing.
- Normal and debug builds must be requested explicitly.

### Evidence
- ✅ **Java providers.** On 2026-10-23 a burst of commits added selectable Java providers (`f0f59f1f` secure random, `c103c0b3` symmetric keys, `646f35f3` CBC, `88c40ef9` SHA-256, `a9876275` OwnerPIN). The "Do not add Java provider/adapter" rule arrived the next day (`b6896499`, 10-24), and the agent then audited the providers (`f7dc8b4e`, 10-26). Even on 10-27 `progress.md` still says "RSA key construction still relies on generated provider objects". Undoing the drift took several days.
- ✅ **Preinstalled JCAlgTest.** `8a41b6ca` / `4df2fe5a` (10-10) "Record generated JCAlgTest ROM mask". The "empty card" image work appears only on 10-24/25 (`c558b648`, `ebc53398`, `6f42310b`). The drift lasted about two weeks.
- ◐ **"Agent did fuzzing / blinding / Shareable alone."** All three happened: `48b456b0` APDU fuzzing (10-27), `31822a1e` CAP decoder fuzzing (11-04), `99c30196` RSA CRT blinding (10-27), and Shareable from 10-18 (33 commits). The *blinding* was not a random whim, though. `progress.md` had flagged "private-operation blinding/hardening is not validated" for several records first. The agent filled a gap it had recorded itself. That is reasonable behaviour, but nothing told you that a security-relevant design decision was being made.
- ✅ **Debug builds.** `BUILD-DBG-01` (11-12) found `APDU_PROCESS` and `PERSIST_REF` debug strings in a debug-off ARM image, a month into the project.
- ❌/◐ **Contradictory steering is in the files today.** `AGENTS.md` says to use the **1.8.5** client (added `032a5304`, 11-12). The `TODO.md` header still says to use the "unchanged JCAlgTest 1.8.4 / jc305 and its **matching client**". Agents read both files at the start of every step. Rules added in one place are not removed from the others.
- ✅ **Object deletion was postponed by design.** `d0f9cb92` (10-18) "Raise ILLEGAL_USE for unsupported object deletion". Opt-in deletion firmware arrived only on 11-11 (`8a6043a3`). The memory-exhaustion saga (part 4) is the bill for "functionality first" on a feature that real cards rely on for long runs.

### Assessment
Your observations are correct. The real mechanism is that **agents fill specification gaps with the cheapest locally consistent choice**. Java providers were easier than native code. A preinstalled applet was easier to test. A disabled GC was safer than a buggy one. Each choice was *documented* in `progress.md`, so it was technically visible. But at 50–140 records per day, no human reads them, so "documented" did not mean "noticed".

### Recommendation
1. **Write a short `SPEC.md` of invariants** (≤ 1 page, numbered, e.g. `INV-03: card ships empty`, `INV-05: all crypto native C++`, `INV-07: object deletion supported`). Keep it separate from procedural rules in `AGENTS.md`. Ask every reviewer run to check each invariant and report a violated `INV-xx`.
2. **Add a "decision log" requirement.** Any change that alters security behaviour, the public API surface or the firmware image composition must append one line to `docs/DECISIONS.md` and appear in the CTO report under "Decisions taken without the user". This turns the "agent did X alone" surprises into a morning-coffee item instead of an accident.
3. **Give each rule a single home.** When you add a rule, ask the agent in the same prompt to grep `TODO.md`, `SKILL.md`, `STATUS.md` and `docs/` for conflicting statements and remove them. Run the 1.8.4/1.8.5 conflict check now.
4. **Tighten reviewer roles** with a mechanical restriction, not just wording. Run review/tidy-up sessions in read-only sandbox mode, or allow them to write only `docs/review_*.md`, `TODO.md` and the handoff. This addresses "larger model picks up implementation".

---

## 2. Understanding what is happening

### Your experience
- The daily CTO report (cron at 05:00, emailed) works very well.
- The "reprint the plan table and highlight what's implemented" trick works.
- Per-task summaries, such as `resource-01-work-summary.md`, explain slow tasks.
- Some lessons came only from reading logs yourself (the 11-12 reader error).

### Evidence
- ✅ CTO reports exist (`docs/CTO_PROGRESS_REVIEW_20261106.md`, `CTO_IMPLEMENTATION_REVIEW.md`, `FIRMWARE_JCVM_ARCHITECTURE.md`), and 33 commits mention CTO.
- ✅ `TODO.md` now carries dated "Reviewer status" paragraphs, the result of your clever-model observation loop (e.g. "Reviewer status 2026-11-14 (after `d06b65a8`..`7f9c6c10`)").
- ◐ **The primary state files are no longer human-readable.** `HANDOFF.md` has 233 stacked "Current handoff" blocks and is about 15k lines. `progress.md` is about 30k lines. Model attribution exists for only ~10 % of records and is ambiguous: you cannot tell Luna from Sol from the records. Your "Statistics" table of tokens per day is kept by hand and is not linked to tasks.
- ✅ The best signal of "what is going on" is a **reviewer status at the top of `TODO.md`**, written by a strong model. For example, the 11-14 status states exactly what is blocked, why, and what the user must decide.

### Assessment
Correct. The CTO report and planner-table tricks work because they **compress** the data. The underlying files keep growing and are append-only, so every compression costs more over time. One more risk: the summaries are themselves written by models. They are good at saying what was *done*. They are poor at saying what was *silently not done* (provider drift, disabled GC).

### Recommendation
1. **Make `HANDOFF.md` overwrite-only**: one current block of ≤ 60 lines, with history moved to `.agent/history/handoff-YYYY-MM-DD.md` by a script. Change the instruction in `SESSION_CONTINUITY.md` from "update" to "replace".
2. **Fix the SHA paradox** to halve the commit count. Drop the `Commit:` line from `progress.md` and let a tool join records to commits (e.g. via a `Step: <id>` trailer in the commit message). Alternatively, allow "Commit: see git trailer `Step-Id`". Remove the "Record … commit hash" commits entirely.
3. **Require a model identifier in commit trailers** (`Agent-Model: gpt6-luna`), not free text in progress. Then per-model statistics (time to close a task, rework rate) come straight from `git log`.
4. **Add two sections to the CTO report**: "Things the agent decided on its own" (from the decision log) and "Tasks with no code commit in 24 h but ≥ N bookkeeping commits". The second is a cheap stuck-detector (see part 5).

---

## 3. Helping the agents progress: knowledge base, hardware, continuity

### Your experience
- Agents repeatedly hit the same compile/token problems. `QKB.md` solved this, and physical re-plugs dropped drastically.
- HANDOFF, SESSION_CONTINUITY, TODO and progress let work resume even after a VM crash.
- Several tokens allow failover.
- `EXT_REQUESTS.md` fixed requests that were buried in `progress.md`.
- Periodic tidy-up and compaction by a stronger model works.

### Evidence
- ✅ **QKB effect is measurable.** `progress.md` records mentioning reinsert/replug/reattach: **38 in 10-10…10-26** versus **7 in 10-27…11-14**, while the total number of records stayed similar. QKB was created on 10-17 (`c806f567`, "Document recurring PCSC and USB access issues"). Multi-board selection by chip ID followed on 10-24 (`8e8e5169`).
- ✅ Failover works: TODO and QKB track boards `2BED…`, `D92E…`, `8E46…` and `4CDA…` separately, and agents continue on another board.
- ◐ **EXT_REQUESTS is used but not pruned.** It has 9 sections, including several "Resolved/Closed 2026-11-11/12" entries, despite the rule to remove obsolete ones. The current open request asks for testing commit `0E67DF89`.
- ◐ **Compaction is real but does not stick.** Archives exist (`.agent/archive/2026-10-29-pre-compaction`, `2026-11-01-todo-…`, `2026-11-11-qkb-compaction`). QKB already has 48 sections again, several "unresolved" and overlapping (three separate CCID/PC/SC-disappearance entries).
- ✅ **BOOTSEL regression (11-12).** BOOTSEL issues appear in 10-10/11 commits (`968e6df0` "Enter BOOTSEL after CHOUPI watchdog timeout", `28f09009`), then quietly degraded. `TODO.md` BOOTSEL-01 says "the earlier implementation exposed only CDC/CCID". The vendor reset interface was missing, and agents worked around it with CDC-1200 fallback and replugs for weeks (`8be342d3` 11-11 "record absent BOOTSEL passthrough"). The P0 and the fix arrived the same day you asked (`45c7ff82`, `fc18a6b9`). ? I could not pin down the exact 10-11 commit that removed the reset interface. TODO only confirms its absence.

### Assessment
Correct, and QKB is the single most effective intervention in this repository by measurable effect. The BOOTSEL case illustrates a general weakness. **Agents treat a degraded capability as an environmental fact to work around (and document in QKB) rather than a regression to bisect.** QKB can actually *entrench* such workarounds. "Use CDC-1200 fallback" was recorded as a "scoped viable fallback". Your small nudge was crucial because you questioned a premise the agents had accepted.

### Recommendation
1. **QKB entries need a "regression?" field.** For each recurring workaround, ask: "Did this work before? If so, when did it stop (git bisect range)?" A reviewer pass can scan QKB weekly for workarounds that are older than N days and never root-caused.
2. **Make capability smoke tests part of the build gate.** A short `tools/cycle` check could assert "picotool can reach the reset interface", "card is empty after flash" and "isObjectDeletionSupported == expected for profile". Regressions such as BOOTSEL then fail loudly on the day they happen.
3. **Auto-prune EXT_REQUESTS and HANDOFF with a script**, not an instruction. Agents follow "append" rules reliably and "delete" rules poorly.
4. **Schedule compaction**, for example weekly with the "compact QKB" prompt from your Future plans, not ad hoc.

---

## 4. Hard problems: the memory-exhaustion saga as a case study

### Your experience
- Long runs fail with NO_RESOURCE or a reset. Despite P0 priority, there was no fix after 2+ days. The model fixes related things systematically rather than going straight to the fix.
- Opus5.5 (11-10) proposed "prevent the allocation instead of cleaning up after it" and found byte-wise RSA writes. That became RESOURCE-02.
- 11-12: you read the log yourself and spotted the "Free unused card objects… Reset applet didn't work" pattern. GPT6-Sol missed it. Opus found that INS_CARD_RESET never reached the card because of a negative APDU length in the *client*. This was an external-environment bug, fixed in client 1.8.5.
- 11-13: Opus memory analysis led to MEM-01, which Luna is implementing.
- Agents first ran hour-long reproductions until pushed to short reproducers.

### Evidence
- ✅ The timeline is longer than "2 days". Memory/GC/deletion commits per day: 22 (11-08), 19 (11-09), 6 (11-10), 27 (11-11), 26 (11-12), 9 (11-13), 7 (11-14). RESOURCE-01 has 42 commits from 11-08 to 11-12 and is still open, "gated on MEM-01". MEM-01 is offline-complete but has **no runtime proof yet**. As of today the problem has been open for 7 days, or 4 weeks if you count from the 10-18 decision to make deletion unsupported.
- ✅ "Systematic rather than direct" is accurate. The R59 records in `progress.md` (around line 27,500) repeat "keep object deletion disabled by default; physical reset/deletion remains required" across **6 consecutive records**. That is progress on the periphery (roots, ownership, invariants) without touching the main question.
- ✅ The root cause was layered, which partly justifies the slowness. There were three distinct bugs:
  1. Deletion unsupported by design.
  2. The client never sent the reset, so GC was never triggered.
  3. A reclaimer bug: alias bodies with a 6-byte `FFFF…` suffix were treated as unreachable. This is the T6 root cause recorded in TODO/HANDOFF on 11-14.

  No single "obvious fix" existed. Bug 2 sat outside the repository's trust boundary (supplied client), and agents are instructed to treat supplied fixtures as unchanged truth.
- ✅ **The agent is currently waiting on you.** `TODO.md` and `HANDOFF.md` (11-14, HEAD `8a59986a` "MEM-01 offline complete; runtime gate needs user authorization") say that T7/T8 are blocked by your instruction to stop long emulator runs. The agent asks you to authorise a **bounded 6–8 min VPCD run** (stop at sync seq ≥ 2,500; pass = no `FS_WRITE_FAIL`/`F205` and flat live bytes over the last 1,000 syncs). The 60 s post-fix run stopped at seq ≈ 800, before the pre-fix failure point (≈ 1,750), and has not shown a plateau yet.
- ◐ The 11-13 Windows-log diagnosis by the worker ("already fixed GC bug") was premature attribution. This matches your note that the dev environment must be as realistic as possible.

### Assessment
Mostly correct. I would refine it in two ways:
- The slowness was **not only** the model's systematic style. It also came from (a) slow feedback loops, (b) the fact that the workers' trust boundary forbade suspecting the test client, and (c) a P0 that was defined as a symptom ("no exhaustion in long runs") rather than as falsifiable sub-hypotheses.
- The human's decisive contributions were all **reframings**: "prevent rather than clean", "look at these log lines", "the reset never arrives". A different (stronger) model was the second-most decisive factor. More worker hours were not decisive.

### Recommendation
1. **Answer the pending MEM-01 authorisation now.** Allow the bounded 6–8 min VPCD run. It is the cheapest decisive check, and the queue is stalled on it.
2. **Use a "hypothesis ledger" for long P0s.** For any P0 still open after 24 h, require a table: hypothesis, test, cost (minutes), status (confirmed/refuted/open). Include at least one hypothesis that questions the test harness itself ("is the stimulus reaching the device?"). The INS_CARD_RESET bug would have been a 5-minute APDU-trace check.
3. **Tie the time budget to the reproducer.** "Any reproduction > 10 min must first be minimised" should be a rule in `AGENTS.md`, not only a QKB entry (it is currently in QKB since 11-09).
4. **Escalate automatically on P0 stalls.** If a P0 has more than 20 commits and no `[x]` in 48 h, the cron runs a strong-model diagnosis (the pattern that worked on 11-10 and 11-13) rather than waiting for you to notice.

---

## 5. Agent orchestration: roles, models, continuous work

### Your experience
- `/goal` plus a cheap worker (Luna) plus an occasional strong planner (Sol, Opus) works. The worker rarely stops (one "Bad request" on 11-14).
- Reviews by a better model produce TODOs for workers. CAPIN-x came from the Opus CAP-loader review.
- Fable5.2 is too expensive to finish a review within the 5 h credit.
- Claude Code auto-compacts when idle.

### Evidence
- ✅ The planner→worker loop is visible: 5 review docs, a TODO priority list that cites the reviews (`MEM-01 … from review_memory_analysis_opus55_20261113`), CAPIN commits from 11-10, and reviewer-status paragraphs written by Opus.
- ✅ Continuous work holds: commits on all 37 days, many at night. ◐ But throughput is mostly bookkeeping (66 %), and "continuous" sometimes meant re-recording the same status (6 near-identical R59 decisions).
- ? Token/cost claims (Fable, auto-compaction) cannot be verified from the repository. Your stats table shows Luna at ~250 M tokens and 17 G cached by 11-12. Cache reads dominate, which is consistent with very large context files being reread every step.

### Assessment
Correct. The role split works. The weak link is that the "observe and re-steer" step is manual (you run it when you remember). You also suspect that your manual presence is what catches deficiencies. The evidence supports this: BOOTSEL, the client bug and the provider drift were all human catches.

### Recommendation
1. **Automate the reviewer loop on a schedule**, as in your Future plans: every 6 h, the strong model reads `git log --since`, TODO and the handoff and writes a dated "Reviewer status". Keep yourself in the loop by having it **email you only deltas plus questions for the human**.
2. **Reduce the context each worker step loads.** Smaller HANDOFF/QKB/TODO means less cached prefill per step. Given 17 G cached tokens, this is probably your largest cost lever.
3. **Run the 1.8.5 long campaigns that `AGENTS.md` asks for** once MEM-01 runtime passes: option 2 and option 1 on two identified boards. These remain the real acceptance test. Most recent evidence is offline.

---

## 6. Future plans: reality-checked and prioritised

| Your plan | Status/evidence today | My priority |
|---|---|---|
| Record your prompts and periodically check they are still honoured | Not present; rules drift already (1.8.4 vs 1.8.5) | **High.** Combine with `SPEC.md` invariants (§1) |
| Detect when agents struggle or are stuck | Manual only | **High.** Bookkeeping-to-code ratio per task and P0 age (§2, §4) |
| Capture when a session takes a wrong path (Java provider case) | Rule added after the fact | **High.** Decision log plus invariant check in the CTO report |
| Periodic strong-model review every 6 h | Manual today | **High** (§5) |
| Compact QKB / HANDOFF | Done once; regrown | **High.** Make it structural (overwrite-only, scripts) |
| Benchmark time/tokens per task | Manual stats table | Medium. Needs model/task trailers in commits first |
| Visualise progress | External ChatGPT links | Medium. Easy once trailers exist |
| Understand memory allocator, flash wear, RAM vs flash placement | MEM-01 reviews cover part of it; QKB has "Overlay capacity…" | High, but **after** MEM-01 runtime acceptance; ask for a human-facing explainer doc, not code |
| Strict API testing (separate CAP suite), javacard_curated_list, javus | Suite present under `tests/fixtures/external/javacard-api/provided-20261107/` | Medium-high. External tests found the real bugs |
| Conditional crypto profiles | Profiles exist (`max_crypto`, `symm_crypto`) | Medium |
| Security review rerun, firewall × GC analysis | CAPIN-x open; one review was refused as security sensitive | Medium-high. Firewall×GC is a genuine risk area after the alias-reclaim bug |
| Multiple agents on one repo without sync issues | Single-worker today; very high commit rate | Medium. Use worktrees/branches per agent, and merge by the reviewer |
| Rewrite in Rust or C plus differential testing; rebuild GC with a new model | — | Low for now. Stabilise acceptance first; it is a good research item later |
| Use both cores, protected key memory, multiple CAP versions | — | Low (features) |

---

## 7. Top 10 actions, in order

1. Authorise the bounded 6–8 min MEM-01 VPCD run. The worker is blocked on you (see `TODO.md` line 49).
2. Resolve the 1.8.4 vs 1.8.5 client contradiction between `TODO.md` and `AGENTS.md`.
3. Make `HANDOFF.md` a single overwritten block, and archive the 233 old blocks.
4. Remove the commit-SHA-in-progress requirement, and use commit trailers (`Step-Id`, `Agent-Model`) instead.
5. Create `SPEC.md` (product invariants) and `docs/DECISIONS.md` (autonomous decisions). Add both to the CTO report.
6. Add capability smoke checks (reset interface reachable, empty card, deletion supported) to the flash cycle.
7. For P0 tasks: use the hypothesis ledger, put the 10-minute reproducer rule in `AGENTS.md`, and auto-escalate to a strong model after 48 h.
8. Schedule the 6-hourly strong-model reviewer that emails you deltas and questions.
9. Use mechanical role restriction for reviewer/tidy-up sessions (read-only sandbox or path allowlist).
10. Run weekly scripted compaction of QKB and EXT_REQUESTS, adding a "regression? since when?" field to QKB.

## 8. One-paragraph verdict

Your notes are accurate and, if anything, understate two things. First, the bookkeeping overhead: two thirds of commits are not code, and the state files are now too large for humans and expensive for models. Second, how consistently the decisive moments were **human reframings plus a model switch**, not more worker time. Every major course correction came from questioning a premise the agents had accepted: provider drift, the preinstalled applet, missing GC, BOOTSEL, the test client. The best investment is therefore not more autonomy but making those premises **explicit and machine-checked**: invariants, a decision log, capability smoke tests and hypothesis ledgers. Then the next drift surfaces in the morning report instead of by accident.
