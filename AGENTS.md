# Pico 2 development instructions

The target is a Raspberry Pi Pico 2 with an RP2350 connected by its USB port.

Target CPU architecture: ARM Cortex-M33 (`rp2350-arm-s`). Hazard3 RISC-V
(`rp2350-riscv`) is **out of scope** (user decision 2026-11-14): no RISC-V build
gate, no RISC-V runtime or physical measurements. The `riscv` wrapper target may
remain for ad-hoc checks only.

Use the repository wrappers; do not invoke compiler binaries directly.

Java Card runtime boundary:

- Pico 2 runs the native Java Card VM, which interprets installed CAP files.
  It does not run `.java` or `.class` programs as firmware.
- Implement Java Card API behavior and cryptographic algorithms in native C++
  VM code, exposed through native API dispatch.
- Do not add Java provider/adapter implementations to firmware or create or
  modify CAP files to implement algorithms. Use existing upstream CAPs only to
  exercise native API behavior.

- Build ARM: `./tools/build arm [--profile P]`
- Build, flash the **full JCVM** and capture logs:
  `./tools/cycle choupi [--profile P] --serial ID` (or `--chipid`/`--usb-path`).
  `./tools/cycle arm` / `riscv` flash only the **smoke-test** firmware. Never use
  them to validate JCVM behavior; their `CYCLE:PASS` says nothing about the VM.
- Capture finite serial output: `./tools/monitor --seconds 8`
- List PC/SC readers (bounded): `./tools/list-readers [--pico-only]`, only when
  necessary (see the PC/SC warning below). Do not use `pcsc_scan`; it has hung
  at high CPU and produced unbounded output.
- Diagnose the host setup: `./tools/doctor`

After changing source:

1. Build ARM.
2. Fix all errors and warnings.
3. Flash the full JCVM (`./tools/cycle choupi`).
4. Inspect serial output.
5. Treat `TEST:FAIL`, `FAULT`, a timeout, or any nonzero command status as failure.
6. Diagnose, edit, and repeat until the hardware test passes.

Runtime output uses `TEST:PASS`, `TEST:FAIL`, and `TEST:SUMMARY` for known-answer tests; `BENCHMARK:PASS` for measured Bitcoin double-SHA-256 throughput; `MINING:START`, `MINING:PROGRESS`, and `SHARE:FOUND` for mining work; and `FAULT` for runtime failures.

Safety constraints:

- Never run `picotool otp`, `picotool erase`, or commands that alter boot security, OTP, flash partition tables, or machine configuration.
- Run every USB/PCSC operation (`lsusb`, attached-device `picotool`, serial monitor, raw CCID, or physical SmartcardIO) through the host-access execution path from its first attempt. For repository wrappers, pass `PICO2_HOST_USB_ACCESS=1` on that single command and invoke it with `sandbox_permissions: "require_escalated"`; both are required, and the wrapper refuses to reach libusb/PCSC without them. For direct system commands, use `sandbox_permissions: "require_escalated"` on the command itself. Never probe one of these commands in the default restricted context or retry there after a failure. If host access is unavailable, stop hardware work and continue offline.
- Do not use `sudo` from an autonomous coding cycle except exceptions listed in section 'sudo exception'.
- Do not modify files outside this repository.
- Do not flash unless the ARM build currently passes without errors or warnings.
- Physical board availability is volatile. Never treat an old "boards disconnected / no hardware work" note as standing. Before hardware work, identify the available boards by chip ID with `PICO2_HOST_USB_ACCESS=1 lsusb -v -d 2e8a:0009` (`iSerial` is the chip ID) and use whatever identified board is available (failover pool). The exact PC/SC reader name is `Pico 2 CHOUPI CCID [JCVM CCID] (<CHIPID>) 00 00`.
- **Warning: PC/SC refresh and reader listing can abort other campaigns.** Several sessions share one pcscd. Before any PC/SC step, check for live runs (`pgrep -fa 'AlgTestJClient|jcalgtest-apdu-watchdog|tools/monitor'` and `.agent/HANDOFF.md`).
  - The approved helper `sudo -n /usr/local/sbin/pico2-pcsc-reader` always restarts `pcscd.service`, dropping every open PC/SC session on every token. Never run it while any campaign (yours or another session's) is live. Run it only when a token is on USB but missing from PC/SC and nothing is running. pcscd normally picks up re-enumerated tokens by hotplug.
  - Use `./tools/list-readers` only when a reader name must be confirmed, not as a routine step.
- Physical tokens (several may be attached): identify each by its 16-hex chip ID and select by that ID, never by reader index or bus number. Keep using one working token for development and tests as long as it is usable; switch only when it is unavailable (record the failure and the switch in `progress.md`). Use two tokens only for a deliberate comparison or to parallelise independent long runs, stating which. Never mix evidence across tokens. Procedure: `reference/physical-token-access/SKILL.md`; roster: `.agent/QKB.md`.

## sudo exceptions
```
sudo -n /usr/local/sbin/pico2-pcsc-reader
```
The agent runs this helper itself, with elevated execution, in exactly this form
(no `PICO2_HOST_USB_ACCESS=1` prefix or other additions). It is not a user-only
step.
It restarts `pcscd.service` and aborts all live PC/SC sessions; run it only when
needed and never during a live campaign (see Safety constraints).

## .git/index.lock read-only failure 
When git add or git commit fails because the sandbox
cannot write to .git/index.lock, retry the operation
with elevated sandbox permissions.

Do not retry the same command unchanged inside the
same restricted sandbox.

Only escalate the required Git operation.
Never use destructive Git commands or push without
explicit authorization.

## Persistence, atomicity and flash writes (user directive 2026-11-19)

Goal: a stable cryptographic token that writes flash only when needed. Java
Card does **not** make every operation atomic, so the VM must not pay for
atomicity the platform does not promise. What the platform guarantees:

- **Single data element.** One update of one persistent field, class field or
  array element is atomic: after a tear it holds either the old or the new
  value (JCRE spec, "Transactions and Atomicity").
- **Block updates.** `Util.arrayCopy`, `Util.arrayFill` and `Util.setShort`
  on a persistent destination are atomic. `Util.arrayCopyNonAtomic` and
  `Util.arrayFillNonAtomic` do not use the transaction facility, even inside a
  transaction; a tear may leave a partly changed array.
- **Transactions.** Only updates between `JCSystem.beginTransaction()` and
  `commitTransaction()` are all-or-nothing. A tear or `abortTransaction()`
  rolls them back, and references to objects created inside an aborted
  transaction become null.
- **Transient arrays.** Their contents are never atomic, never in a
  transaction and never written to flash.
- **Outside transactions.** Several separate updates (several fields, several
  array elements, the steps of one APDU) are **not** atomic as a group. An
  applet that needs that must use a transaction; the card need not provide it.
- **Explicit exceptions.** OwnerPIN's try counter, validated flag and blocking
  state never take part in a transaction. The decrement must be durable
  before the PIN comparison result can be observed.

Rules for VM work:

- Keep flash writes to what durability and the guarantees above require. Do
  not add copies, journals or snapshots only to make non-transactional groups
  of updates atomic.
- The VM's **own** metadata (records that describe objects, references,
  identities and the filesystem index) must stay internally consistent after
  any tear. Grouping several records for that reason is required, and is a VM
  integrity need, not an applet atomicity guarantee. State the reason in the
  code when you group records.
- SPEC INV-14 is interpreted per data element and per transaction, as listed
  above; it is not weakened by this section.
- Measure flash traffic (`CHOUPI:FS_APDU` `sync_bytes`, `journal_bytes`) for
  any persistence change and record it in `progress.md`.

## Product specification

`SPEC.md` holds the user-confirmed product invariants (`INV-xx`). Read it at
session start, together with this file. Never weaken, reinterpret or remove an
invariant. If a task conflicts with one, stop and ask through
`.agent/EXT_REQUESTS.md`. Reviews must check every invariant and report
violations by ID. "Known gap" entries in `SPEC.md` are TODO work, not
permission to diverge.

## Change discipline

Preserve existing worktree changes. Validate each coherent step, append its
record to [progress.md](progress.md), and create one new commit for that step
and record. Follow the [record format](.agent/SESSION_CONTINUITY.md#progress-record).
Use a unique `YYYYMMDD-HHMM-short-slug` Step-Id in the record and in the commit
trailer, with an `Agent-Model` trailer. Preserve earlier records. Never amend,
backfill a commit hash, or commit a status-only restatement. Commit only files
belonging to the step.

## Session Continuity

This repository may be worked on by multiple Claude/Codex sessions/accounts.

For any substantial task, read and follow:
`.agent/SESSION_CONTINUITY.md`

Maintain `.agent/HANDOFF.md` as described there so another Claude/Codex
session can safely continue interrupted work.

Never discard existing uncommitted changes merely because they were
created by another session.

## Issues knowledge base

Maintain `.agent/QKB.md` file containing issues encountered (e.g., inaccessible token, failures to run lsusb, sudo related issues...). When robust fix for the issue is found, mark it as viable solution and try to use it later when issue is encountered again. 

## Validation suites

Follow [tests/README.md](tests/README.md) for fixture selection, verification,
campaigns and evidence. Use the unchanged 1.8.5/jc305 CAP with the explicit
`jcalgtest.client.1.8.5-20261121` client override as the baseline. Run the
pinned 1.8.6/jc32 pair in addition for the 3.2 preview. Preserve fixture bytes,
use the separate API suite's own client, and run `./tools/fixture-path --verify`
before acceptance.

### Build modes and card state (SPEC INV-24, INV-27)
- There are two build modes. **normal** is the default: object deletion ON and all debug/trace switches OFF. **diagnostic** is any debug switch ON, or `PICO2_ENABLE_OBJECT_DELETION=0`. `tools/build` records `mode=normal|diagnostic` in its provenance. Only normal images are evidence.
- **Acceptance and campaign runs** (MEM-01 T8, RESOURCE-01/02, PERSIST, AC, option 1/2 campaigns) use a normal `max_crypto` image.
- Acceptance runs start from an **empty card**: wipe with an empty-card UF2 (built by `tools/package-empty-card-uf2`, flashed with `tools/flash` to the identified board), then install the unchanged CAP. Never use `picotool erase`.
- **Development cycles** (`./tools/cycle choupi`) preserve installed storage. PERSIST power-loss tests create their own state on a freshly wiped normal image rather than relying on older stores.

### Development code and release review

Use [the diagnostics migration plan](docs/diagnostics-migration-plan.md) for
the proposed switch names and rollout. Apply the following checks to new
runtime diagnostics, test hooks, build changes and release artifacts.

- Identify the purpose of added code that observes or alters runtime behavior.
  Put debug reporting under telemetry, measurement-only code under profiling,
  and fault injection or forced behavior under test hooks. Use platform
  selectors such as `PC_VERSION` for platform behavior.
- Follow every diagnostic from collection to delivery. Check buffers, counters,
  timers, retained registers, serial output, APDU payloads and package contents.
  Compile out development-only work with its output in normal builds. Keep
  required protocol, safety, recovery and resource checks active.
- Check every affected test or campaign consumer. Preserve valid normal-image
  evidence and record agreed changes to documented telemetry interfaces. Follow
  the product invariant process if a measurement requirement conflicts with
  INV-24 or INV-27.
- Check effective build options, including reused cache values and linked
  dependencies, against provenance. Inspect loadable bytes in the exact
  release UF2. Use the link map or behavior tests for hooks without strings.
  Apply the separate normal-image hardware acceptance requirements above.

### Long simulator/emulator runs (user directive 2026-11-14)
Long VPCD/simulator runs are permitted. This replaces the earlier "stop long emulator runs" instruction. Each long run must have a stated reason and an expected benefit. Before starting one, think first:
1. Write down the target scenario. Name the exact state or failure you need to reach (e.g. "arena at ≥70 % live occupancy with alias bodies, then compaction", "sync seq > 1,750").
2. Check whether that scenario can be reached by cheaper means than a naive long run: a focused unit or fake-flash test, preloading or synthesizing the state, a smaller arena or index to hit limits sooner, a reduced client option or algorithm subset, skipping straight to the triggering APDU sequence, or replaying a recorded trace. Prefer the cheapest method that reaches the same state with the same code paths.
3. Run long only if no cheaper method reaches the scenario faithfully, or if the long run is itself the acceptance evidence (e.g. unchanged client end-to-end). Bound it: give an explicit stop condition (sync seq, time or event) and pass/fail criteria, and log the telemetry needed to decide. Stop early once the outcome is decided.
4. Record the reason, the alternatives considered and why they were rejected, the expected benefit and the outcome in `progress.md`.

## Request for external actions (to be taken outside this development setup)
- When external action outside what agent can do in this environment, add it to dedicated file  `.agent/EXT_REQUESTS.md` with clear description what you are asking from external human user to do. Provide paths to all required information like firmware build, steps to take and result to submit back to agent. Insert date and time when request was created and assign it clear description ID (e.g., WIN_TEST_MAXCRYPTO_01). When request is fulfilled, check and remove the request if no longer relevant. When inserting new request, check existing ones and remove obsolete ones.  
