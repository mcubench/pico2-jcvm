# Rule conflicts: decision sheet

Prepared by Claude Opus 5.5 on 2026-11-14, against repository HEAD `850fdf46`.
Scope: `TODO.md`, `SKILL.md` (and `reference/` linked from it), `STATUS.md` and
`docs/`, checked against `AGENTS.md`, `.agent/SESSION_CONTINUITY.md` and
`plan.md`. Duplicates found in several files are merged into one item.

**How to use this sheet:** mark one variant per item with `[x]`, or write your
own on the `Other:` line. ★ = my recommendation. Line numbers are as of HEAD
`850fdf46` and may shift slightly.

---

## A. Steering and priorities (affect what the worker does next)

### 1. Stale "no hardware work" freeze versus the failover pool
- `TODO.md:24-26` says: "The user has since physically disconnected the devices;
  do no more hardware work until they are reported available…"
- Similar wording in `SESSION_CONTINUITY.md:17-18` ("On 2026-11-01 … zero Pico
  readers; continue offline") and `STATUS.md` ("no further hardware operation
  is authorized until they report the boards available").
- This conflicts with `AGENTS.md` (failover pool; continue on another device)
  and with the actual hardware runs on 11-14.

- [X ] **A ★** Delete these freezes. Board state is volatile: always refresh the
  approved helper and check the HANDOFF board-state section.
- [ ] B Keep one pointer line to HANDOFF and move the dated availability
  statements to `progress.md`.
- [ ] C A freeze is real: I will state it in `AGENTS.md` with a date and an end
  condition.
- Other: ______

### 2. Leftover "no long emulator runs" wording
- `TODO.md:86` says "…do not extend this into a long VPCD campaign unless
  asked".
- `TODO.md:56` says "compatible with the speed constraint".
- Both conflict with the new long-run rule in `AGENTS.md`.

- [ x] **A ★** Delete both phrases and cross-reference the `AGENTS.md` long-run
  rule.
- Other: ______

### 3. Two "P0 / highest priority" items, and RESOURCE-01 gated two ways
- MEM-01 is marked P0 and "highest priority" (`TODO.md:11`, `:38`).
- BOOTSEL-01 is also marked P0 and "highest priority" (`TODO.md:12`, `:126`,
  `:130`).
- RESOURCE-01 is gated "on MEM-01" at `:13` but "after BOOTSEL-01" at `:182`.
- `STATUS.md` still lists BOOTSEL first and RESOURCE second, with no MEM-01.

- [x ] A MEM-01 is the only P0. BOOTSEL-01 becomes P1 (do it when a board is in
  hand). RESOURCE-01 is gated on MEM-01.
- [ ] **B ★** Split by mode: MEM-01 is the P0 for offline and simulator work;
  BOOTSEL-01 is the P0 for hardware sessions. State this once at the top and
  remove "highest priority" from the section headings.
- [ ] C One ordered list with a single P0 label.
- Other: ______

### 4. Physical option 1/2 campaigns: owner and precondition
- T8 runs "only after T1–T7" and after an empty-card wipe (`TODO.md:122`,
  `:61`).
- RESOURCE-01 says "rerun … once a physical reader is available" (`:186`).
- `AGENTS.md` says to run them whenever PC/SC is available.

- [ ] A MEM-01 T8 is the single owner. Physical runs may go in parallel with T7
  whenever a board is available.
- [ ] B Strict gate: no physical campaigns until T7 passes. Add this exception
  to `AGENTS.md`.
- [x ] **C ★** Exploratory physical runs are allowed whenever a board exists.
  Only runs after T7, on wiped boards, count as T8 acceptance. RESOURCE-01 and
  RESOURCE-02 cite T8.
- Other: ______

### 5. Overlapping owners of CCID / PC/SC reader loss; JC-RESET-01 "offline only"
- RESOURCE-01 (`:197`), HOST-01 (`:948`) and the MEM-01 exclusion (`:122`) all
  cover transport failures.
- JC-RESET-01 says "do not test with real tokens" (`:716`). Yet the mandated
  option 1 physically reproduces its scenario.

- [x ] **A ★** HOST-01 owns all reader loss and CCID stalls. RESOURCE-01 and
  JC-RESET-01 feed evidence to it, and HOST-01 is a stated T8 blocker.
- [ ] B Keep the split. JC-RESET-01 becomes: "no dedicated physical repro;
  reader loss seen during option 1 is recorded under HOST-01".
- [ ] C Lift JC-RESET-01's offline-only restriction.
- Other: ______

### 6. Stale MEM-01 status blocks that still give orders
- `TODO.md:91-102` says "Next, in this order: T7, T1, T2", but T1 and T2 are
  done.
- `:51` says "T1 done except…", against `:49`.
- `:84` says "uncommitted harness", but the files are committed.
- `:47` says "one commit per task", against `AGENTS.md`'s one commit per
  logical step.

- [x ] **A ★** Collapse `TODO.md:47-102` into one current-status paragraph and
  move the superseded reviewer blocks to the archive.
- [ ] B Keep them, prefixed "Historical (superseded):", and fix the commit
  wording.
- Other: ______

### 7. Duplicate tasks with different instructions (RV-20261106-x versus CAPIN-x)
- RV-55 (dependency closure) is already done in CAPIN-01.
- RV-43 (package-stack allocation) is already done in CAPIN-03/04.
- RV-16 says "measure before adding a branch cache", but CAPIN-08 keeps the
  cache.

- [x ] **A ★** Mark RV-55 and RV-43 "implemented via CAPIN; acceptance only".
  Make RV-16 a prerequisite inside CAPIN-08.
- [ ] B Close the RV duplicates and move their remaining test cases into
  CAPIN.
- Other: ______

---

## B. Build, flash and hardware procedure

### 8. `AGENTS.md`'s flash command flashes the smoke firmware, not the JCVM
- `AGENTS.md` documents `./tools/cycle arm` / `./tools/cycle riscv`, but
  `tools/cycle:46-49` flashes `pico2_jcvm_smoke.uf2` for those targets. Only
  `cycle choupi` flashes the VM.
- Docs and TODO RV-20 already use `cycle choupi`.
- An agent following `AGENTS.md` gets `CYCLE:PASS` on the wrong firmware.

- [ x] **A ★** Change `AGENTS.md` to `./tools/cycle choupi [--profile P]
  --serial ID` and describe arm/riscv as smoke-only.
- [ ] B Change the tool: `cycle arm` means the full VM, plus a new `smoke`
  target. Then update `AGENTS.md`.
- [ ] C Add a warning line to `AGENTS.md` only.
- Other: ______

### 9. RISC-V scope
- `plan.md:33` lists RISC-V as out of scope; `:267` says "ARM only default;
  RISC-V warning-free where cheap".
- `SESSION_CONTINUITY.md:51-55` says to follow the plan.
- `AGENTS.md`, `tools/cycle` and the docs make both builds a hard gate before
  flashing.
- TODO PERF-07 and `:518` ask for physical RISC-V measurements.

- [ ] **A ★** RISC-V is a mandatory warning-free build gate with no runtime or
  physical parity. Reword the plan and SESSION_CONTINUITY, and make PERF-07
  ARM-only.
- [x ] B RISC-V is out of scope: drop the gate from `AGENTS.md` and
  `tools/cycle`.
- [ ] C Gate RISC-V only for changes to shared, platform-independent source.
- [ ] D Full RISC-V parity, including physical measurements.
- Other: ______

### 10. Wipe to an empty card or preserve storage before runs
- Wipe: `TODO.md:61`, `:102`, `:122`, `:124`, and
  `review_memory_analysis_opus55` (`:291`, `:301`).
- Preserve: BOOTSEL `:133`, PERSIST-03 `:659`, AC-02 `:1005`, the runbook
  ("skip if already installed"), `windows-asymm-symm-test.md` and
  `flash-layout-migration.md`.
- `AGENTS.md` says nothing about this, and no doc explains how to wipe on Linux.

- [x ] **A ★** Add a rule to `AGENTS.md`: acceptance and campaign runs start
  from an empty-card UF2 (via `tools/flash`). Development cycles preserve
  storage. PERSIST tests create their own state on a fresh image.
- [ ] B Same as A, plus a `--empty-card` option in `tools/cycle` used by the
  runbook.
- [ ] C Preserve by default, but record storage provenance (which image last
  wrote the store).
- Other: ______

### 11. Object deletion: default OFF, but acceptance needs it ON
- `tools/build`, `README.md:99` and `supported-profile.md:37` all say off or
  unsupported, and `TODO.md:183`, `:613`, `:198` keep it disabled.
- But T7, T8 and the external image all use deletion ON, which the wrapper
  labels "diagnostic".
- Nothing defines when the default flips.

- [ ] A The acceptance profile is max_crypto with object deletion ON and debug
  OFF. The build provenance labels it "acceptance", not "diagnostic". Qualify
  `supported-profile` and README.
- [ ] B Keep the default OFF until a named gate (e.g. T8 + PERSIST-01), then
  flip it to ON and rerun on the normal image.
- [ x] **C ★** Both: use A now, and flip the default at the B gate.
- Other: ______

### 12. `SKILL.md` references contain forbidden commands
- `reference/board.md:110` has `picotool erase -r …`, and `:49` and `:98` have
  erase steps.
- Raw `cmake -B …` appears too.
- `AGENTS.md` forbids `picotool erase` and direct compiler calls.

- [x ] **A ★** Put a banner at the top of `board.md`: "generic RP2350 notes; in
  this repository use the wrappers, never `picotool erase`, wipe via an
  empty-card UF2".
- [ ] B Rewrite the erase steps in `board.md` to use the empty-card UF2.
- [ ] C Drop `board.md` from `SKILL.md`.
- Other: ______

### 13. Listing readers with `pcsc_scan`
- The runbook (`:40`), QKB and `tools/test-gp*-vpcd` use `pcsc_scan -r`.
- `reference/physical-token-access/SKILL.md:50` says to avoid it (it hangs at
  high CPU), and `development-performance.md` reports 237 MB of output.

- [ ] A Allow only `timeout N pcsc_scan -r` after a pcscd readiness check.
- [ ] B Use the bounded Java PC/SC probe everywhere.
- [x ] **C ★** Add a `tools/list-readers` wrapper (bounded) and use it
  everywhere.
- Other: ______

### 14. Who runs the sudo helper
- `reference/physical-token-access/SKILL.md:44` says "have the user run
  `sudo -n …pico2-pcsc-reader`".
- `AGENTS.md` lets the agent run it.
- The runbook prefixes it with `PICO2_HOST_USB_ACCESS=1`, which is not the
  exact allowed form.

- [ x] **A ★** The agent runs it in the exact `AGENTS.md` form (elevated, no
  prefix). Fix the skill and the runbook.
- [ ] B It is user-run only: narrow `AGENTS.md`.
- Other: ______

---

## C. JCAlgTest client and test inputs (leftovers of the 1.8.5 switch)

### 15. Runbook commands do not pass the 1.8.5 client
This is my omission from the previous step. The runbook text says "add
`--client-jar` to every command", but the commands at `:56`, `:68` and `:90`
lack it, so `tools/run-jcalgtest` falls back to 1.8.4.

- [x ] **A ★** Put `--client-jar …1.8.5…` inline in all commands and add a
  `sha256sum` check against the 1.8.5 fixture README.
- [ ] B Set a `JCALGTEST_CLIENT=` variable once at the top.
- [ ] C Fix the tooling default instead: `manifest.json` and `fixture-path` use
  1.8.5, and `--verify` checks its hash. The flag then becomes unnecessary
  everywhere.
- Other: ______

### 16. Windows run sheets and packagers still ship the 1.8.4 client
- `docs/windows-asymm-symm-test.md`, `docs/symm-crypto-windows-test.md` and
  `docs/windows-jcalgtest-object-deletion.md` use the 1.8.4 client.
- `tools/package-*-windows-test` call `fixture-path jcalgtest.client`.
- One `EXT_REQUESTS.md` entry says "matching client".

- [x ] **A ★** Fix the packagers to bundle 1.8.5 (or do 15-C). Archive the
  three docs as superseded by the per-package READMEs. Remove the stale
  EXT_REQUESTS entry.
- [ ] B Update all three docs and both packagers.
- [ ] C Add a "historical 1.8.4 client" banner only.
- Other: ______

### 17. Remaining "1.8.4 client", "baseline" and "matching client" wording in active text
- In TODO: `:172`, `:217`, `:712`, `:796-797`, `:810`.
- `SESSION_CONTINUITY.md:61-63`, `STATUS.md` RESOURCE item, `supported-profile.md:62`,
  `README.md:105`, `resource-01-implementation-plan.md:98` and
  `testing-data-migration.md`.

- [x ] **A ★** Rewrite them explicitly as "1.8.4/jc305 CAP + 1.8.5 client".
- [ ] B Rely on the `AGENTS.md` redefinition and fix only SESSION_CONTINUITY.
- Other: ______

### 18. Client version reported in the CSV, and the reference baseline
- The 11-14 CSV says "client 1.8.4" even though the 1.8.5 JAR was used. Docs
  say the old client reports itself as 1.8.3.
- The runbook compares results against a jCardSim reference that was generated
  with the old client.

- [x ] **A ★** Acceptance requires the JAR's SHA-256 to be recorded as the
  authoritative client identity. Regenerate the jCardSim reference with 1.8.5.
- [ ] B Record the JAR hash only, and label the reference "mixed-client".
- Other: ______

### 19. Which CAP mislabels the RSA 3072/4096 probes
- `supported-profile.md:42` says the historical 1.8.2 CAP mislabels them.
- `windows-asymm-symm-test.md:87` says the 1.8.4 CAP does (143/145 expected).

- [ x] **A ★** Have an agent verify against the 1.8.4 CAP and correct the wrong
  doc.
- [ ] B Both have it: record 143/145 as the 1.8.4 expectation.
- Other: ______

---

## D. Documentation hygiene

### 20. Stale image pointers ("use image X")
- Four different UF2s are described as current (`TODO.md:187`, `:194`, `:196`,
  `:1153-1158`, plus `STATUS.md`).
- `:1156` cites a nonexistent EXT request ID.
- HANDOFF has yet another SHA.

- [x ] **A ★** Keep one "Current images" table (acceptance and diagnostic UF2
  path and SHA-256) at the top of `TODO.md`, updated on each storage fix, and
  delete the other "use X" sentences.
- [ ] B Use a single manifest file under `artifacts/firmware/` instead.
- Other: ______

### 21. `STATUS.md` is stale and duplicates TODO and HANDOFF
- Last updated 11-12, it has a pre-MEM-01 priority list, the hardware freeze,
  "1.8.4/jc305 options 2 and 1", and four image descriptions.

- [ ] **A ★** Reduce it to a ≤ 20-line human summary (goal, acceptance state,
  link to TODO and HANDOFF). It should hold no instructions and be regenerated
  by the CTO report job.
- [x ] B Delete `STATUS.md` and let the CTO report replace it.
- [ ] C Keep it, but require the worker to update it at each P0 change.
- Other: ______

### 22. Per-commit "update handoff" wording against the new rules
- `resource-01-implementation-plan.md:114` and `usb-startup-diagnostics.md:98`
  say to update the handoff with every commit. The new rules make HANDOFF
  next-steps only and forbid status-only commits.

- [x ] **A ★** Change them to "progress record with Step-Id; update HANDOFF only
  when next steps or blockers change".
- [ ] B Remove the procedural sentences and point to `AGENTS.md`.
- Other: ______

### 23. `SESSION_CONTINUITY.md` protects a tree that no longer exists
- Line 9 says "preserve … `AlgTest_JavaCard/`", but that tree has been
  archived.

- [x ] **A ★** Remove it, or replace it with the archive path.
- Other: ______

### 24. Docs that are obsolete as guidance
- [x ] **A ★** Move these to `docs/archive/`, each with a one-line "superseded
  by …" header:
  - `windows-asymm-symm-test.md`, `symm-crypto-windows-test.md`,
    `windows-jcalgtest-object-deletion.md`
  - `development-performance.md`, `sha256-reset-timing-analysis-20261105.md`
  - `resource-01-implementation-plan.md` (overtaken by MEM-01),
    `documentation-moves-2026-11-02.md`

  Add a "snapshot, not procedure" banner to `PICO2_PRACTICAL_DEVELOPMENT_GUIDE.md`.
- [ ] B Keep them all in place with banners only.
- Other: ______

---

When you have marked your choices, ask a session to "apply the selected
variants from jcvm_rule-conflicts_opus55_20261114.md". Items 8-B, 9-B, 10-B,
13-C and 15-C change tooling, not just text.
