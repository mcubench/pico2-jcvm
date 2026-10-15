# Development performance tips

These are measured observations and low-risk experiments for keeping the Pico 2
development loop responsive. The sample now covers several builds and host
tests, but it is still too small to estimate stable long-term averages.

## Current measurements

From `artifacts/metrics/development-time.csv` and the host install log below:

| Task | Successful elapsed time | Notes |
|---|---:|---|
| ARM build | 3.120–9.152 s; mean 5.127 s | Five successful `./tools/build arm` runs; two failed attempts are excluded. |
| Focused host tests | 0.458–5.184 s; mean 2.493 s | Nine successful runs across short APDU, install rollback/status, and RP2350 filesystem behavior. |
| Timer syntax check | 0.055 s | Two successful runs. |
| Manual analysis interval | 4.641 s | Wall time, not CPU time. |
| PC API + ROM-mask generation | 1:01 + 5:13 | One full regeneration; the Maven reactor spent 5:13 resolving metadata before finishing from its local cache. |
| Host applet installation | 0.45 s wall; 0.40 s user; 0.00 s system; 6,120 KiB max RSS | One successful install with the supplied ROM mask; this is a single `/usr/bin/time` sample, not a stable average. |
| PC CHOUPI JCRE boot smoke | 34.837 s | One failed run; treat this as time to a failure, not successful runtime performance. The timer does not record its internal phase breakdown. |
| Upload | Not measured | No upload was attempted in this sample. |

The initial timer sample totaled 52.821 seconds. The latest report below
includes later compile and test work; keep build classes separate because
durations vary with regenerated Java outputs and the objects rebuilt.

## Follow-up samples — 2026-10-15

The timer now includes one focused PC generation run, one full PC VM build,
three incremental PC VM builds, and both firmware architectures. These are
different build classes and should not be combined into one average.

| Task | Successful elapsed time | Result and limits |
|---|---:|---|
| PC Java API + ROM-mask generation | 5.120 s | Incremental wrapper run; generation passed. |
| PC VM build | 30.790 s | Full target build passed and included Java OS build work. |
| PC VM incremental rebuild | 9.916–10.778 s | Three C++ rebuilds passed; one earlier compile attempt failed before correction at 2.453 s. |
| ARM firmware build | 7.952 s | Passed; UF2 generated. No flash was performed. |
| RISC-V firmware build | 35.131 s | Passed after rebuilding shared VM sources. |
| VPCD JCAlgTest APDU attempt | 5.341–5.974 s | Three bounded runs reached ATR and SELECT, then failed at the JCSystem info APDU with `6F00`; these are failure-to-diagnosis times, not successful runtime measurements. |
| Upload and board test | Not measured | No upload or physical test was performed. |

Use the already configured PC CMake target for a C++-only iteration after the
Java API and ROM-mask inputs are unchanged; this measured about 10 seconds per
rebuild, compared with 30.790 seconds for the full PC target build. Keep the
normal wrapper as the generation and correctness check. The VPCD failure now
identifies `PUTFIELD_A` at Method component offset 39 (CP index 5), with an
empty operand stack; fix that correctness issue before using APDU timings to
judge throughput. These timings came from one session and are not controlled
benchmarks.

## Development-loop timing sample — refreshed 2026-10-15

The latest task-time report contains 26 successful intervals totaling 183.503
seconds and 17 non-OK intervals. Successful durations exclude failed and
interrupted work; the non-OK count measures iteration friction, not runtime
performance.

| Category | Successful runs | Mean | Longest | Non-OK runs | What to do next |
|---|---:|---:|---:|---:|---|
| Compile | 14 | 11.165 s | 35.131 s | 4 | Keep using the wrappers and compare clean, generated-output, and incremental builds separately. |
| Test | 9 | 2.493 s | 5.184 s | 13 | Run the smallest relevant host regression first; retain the bounded integration run for end-to-end checks. |
| Thinking | 1 | 4.641 s | 4.641 s | 0 | Too few samples to compare; manual intervals are wall time, not CPU time. |
| Tool check | 2 | 0.055 s | 0.055 s | 0 | Negligible in this sample. |
| Upload | 0 | Not measured | Not measured | — | Time BOOTSEL recovery, transfer, re-enumeration, and validation independently when hardware work resumes. |

These are short, mixed-purpose intervals, not a controlled benchmark. The
82.460-second interrupted serial capture and 34.837-second failed PC smoke
test remain useful examples of waits that need phase timing, but should not be
included in successful task averages. The newer 5–7 second VPCD failures are
also failure-to-diagnosis times, not applet throughput. For future records,
include target, clean/incremental state, source revision, job count, and
wall/user/system time so a slowdown can be tied to a stage.

The most useful immediate speedup is to reduce failed iteration time: check
device and reader preconditions before a long integration timeout, and run a
focused test before rebuilding firmware. Do not raise parallelism based on
these timings; they contain no CPU-load or responsiveness comparison. Likewise,
the single long Maven observation should first be repeated through the current
wrapper before changing dependency caching.

The full Maven CAP converter build completed in 3:29 before timing was enabled.
That number was observed in the build output but is absent from the timer CSV.
The CSV contains four failed compile attempts, twelve failed test attempts,
and one interrupted test attempt; the report excludes non-OK runs from
successful duration averages. The most recent bounded VPCD attempts reached
ATR and SELECT before returning `6F00` for the JCSystem information APDU.

The successful host installation completed quickly in its one recorded sample
and used 90% CPU over 0.45 seconds. There is no evidence that this phase needs
optimization yet. The 34.837-second failed PC smoke run deserves a phase-level
trace before optimization: the timer only says where the command started and
that it failed, so it cannot establish whether the time was spent in startup,
VM execution, or a wait for the emulator client.

A later full PC build regenerated the Java API in 1:01 and the ROM-mask Maven
reactor in 5:13. Maven reported DNS failures while checking optional repository
metadata, then completed successfully from cached dependencies. These are
single observed stage times, not timer-CSV averages.

## Suggestions

1. **Keep the fast loop incremental.** ARM builds currently take 3.120–9.152 s
   in this sample, and focused host tests take 0.458–5.184 s. Run the narrow
   host test after a small change, then `./tools/build arm`. Preserve generated
   CAP and ROM-mask outputs so ordinary C++ edits do not trigger the Java/Maven
   pipeline. PC CMake now models API and ROM-mask generation as file-output
   steps keyed to their source inputs, so repeat host builds reuse generated
   outputs. Use the repository wrappers so their manifests can detect stale
   generated files.
2. **Profile CAP conversion separately.** The observed 3:29 converter build is
   the largest known compile cost. Time `./tools/build-pc` and ARM builds over
   several edits; identify which Java, Maven, and ROM-mask inputs invalidate the
   generated artifacts before changing caching or build dependencies.
3. **Keep parallel compilation bounded.** The default `PICO_BUILD_JOBS=2` avoids
   saturating the VM's eight cores. Raise it only for a measured clean build,
   while watching responsiveness and memory pressure.
4. **Check USB nodes before waiting for serial.** The recent monitor attempt
   spent 82.460 s before interruption. At that time sysfs showed USB
   `2e8a:0009`, but the VM had no `/dev/ttyACM*` or `/dev/serial/by-id/*` node.
   Check both USB mode and serial-node presence first; when absent, report the
   exact mismatch and retry after a short bounded interval instead of waiting
   through repeated polling.
5. **Measure hardware operations as separate intervals.** Record BOOTSEL
   recovery, upload, reboot/enumeration, PC/SC discovery, and serial validation
   separately. Keep PC/SC and serial waits bounded so a missing reader does not
   look like slow firmware behavior.
6. **Build evidence over time.** Use `./tools/task-time run` for commands and
   `start`/`stop` for manual work, then compare successful totals and non-OK
   counts with `./tools/task-time report`. Record whether each run was clean or
   incremental so those costs are not mixed in the same average.

## Next speedup measurements

- Keep the build wrapper's two-job default while the VM is used interactively;
  it was added after reports of all eight cores becoming saturated. The current
  timing sample does not compare CPU load or responsiveness at different job
  counts, so it does not establish whether a higher setting would save time.
- For a controlled comparison, collect at least three clean and three
  incremental ARM build samples at `PICO_BUILD_JOBS=1`, `2`, and `4`. Record
  wall time and peak CPU usage separately, and stop increasing jobs if VM
  responsiveness degrades. Keep generated artifacts and the source revision
  constant within each comparison.
- Time hardware preflight, upload, reboot/enumeration, PC/SC discovery, and
  serial capture as separate bounded steps. Upload is still unmeasured, and a
  missing USB node or PC/SC reader should be reported as a precondition failure
  instead of consuming a long test timeout.
- Add a per-stage build breakdown only if repeated samples show the same stage
  dominates. The known 3:29 CAP converter build predates the timer and is a
  single observation, so first measure it through the current wrapper before
  changing build caching or dependency rules.

## Additional high-value suggestions

- **Find the source of VM CPU saturation before changing the build.** The VM
  has previously become unresponsive with all eight cores busy. During a
  repeatable reproduction, record the top CPU-consuming process and its
  threads, plus wall time and CPU time for the command. If the load comes from
  a device or PC/SC wait loop, replace tight polling with blocking I/O or a
  sleep between bounded retries; if it comes from compilation, compare the
  existing two-job setting with one job before raising the limit. The cause
  has not yet been measured, so avoid attributing those freezes to a specific
  tool.
- **Make hardware readiness waits cheap and explicit.** Check USB mode, device
  nodes, pcscd health, and reader enumeration once before starting an APDU
  probe. Retry with a short bounded backoff and print which precondition is
  missing. Keep the APDU timeout separate from reader discovery so a missing
  reader cannot consume the full test duration or appear as slow card code.
  Avoid `pcsc_scan -r` while pcscd is unavailable: one bounded attempt in this
  environment emitted a rapid, repeated `SCardEstablishContext: Service not
  available` stream (over 237 MB captured by the command tool before it
  terminated). Check daemon readiness first and prefer the finite Java probe.
- **Extend timing records with comparable-run metadata.** For each build sample,
  record target, clean or incremental state, source revision, job count, wall
  time, and CPU time; optionally record peak resident memory. For hardware
  runs, record preflight, BOOTSEL recovery, upload, reboot, reader discovery,
  and test time independently. This makes the next bottleneck visible without
  conflating host work with device waits.
- **Prioritize repeated costs over speculative tuning.** The incremental PC
  generation fix removes repeated Java/Maven work when its inputs are
  unchanged. Measure a clean regeneration and several no-op/incremental builds
  through the current wrappers; only optimize the remaining stage if it is
  consistently a large share of end-to-end time. Keep generated artifacts and
  source revision fixed for comparisons.

## Practical profiling workflow

- **Identify the process behind a freeze.** In a second terminal, capture the
  busiest processes and then their threads while the slowdown is happening:

  ```sh
  ps -eo pid,ppid,pcpu,stat,comm --sort=-pcpu | head -15
  ps -L -p <pid> -o pid,tid,pcpu,stat,comm --sort=-pcpu | head -20
  ```

  Compare this with the command being timed. A compiler process points to build
  parallelism; a long-lived probe or bridge points to its wait/retry path. Save
  the process snapshot with the timing record so the cause can be checked later.
- **Separate wall time from CPU time.** For a repeatable command, record both
  with `/usr/bin/time -f 'wall=%e user=%U sys=%S cpu=%P maxrss_kb=%M'` while
  retaining the normal repository wrapper. High CPU time close to wall time
  indicates active computation; low CPU time with high wall time indicates a
  wait, I/O delay, or retry. These are diagnostic clues, not a root-cause proof.
- **Prevent competing builds from distorting results.** Run one build per
  configured build directory at a time. Concurrent build processes can
  duplicate work, contend for memory, and make a small job-count comparison
  meaningless. Keep the source revision and generated-file state fixed across
  samples.
- **Use bounded backoff for device discovery.** Make one readiness check, then
  retry at increasing short intervals up to a fixed deadline. Report which
  resource is missing (USB device mode, serial node, PC/SC service, or reader)
  and stop at the deadline. Do not let a preflight loop consume the applet
  response timeout, and do not use a continuously printing scan as a wait loop.
- **Choose optimizations from a profile.** If the same conversion stage
  dominates multiple clean builds, inspect its dependency inputs and cache
  behavior. If incremental C++ builds dominate, check whether unrelated source
  files are being rebuilt. If device waits dominate, improve the bounded
  readiness path first. Record the before/after samples using the same clean or
  incremental state; do not infer a speedup from unlike runs.

## Immediate speedup priorities from the latest run

1. **Keep host applet install as a baseline, not an optimization target.** Its
   single measured run took 0.45 s wall time (0.40 s user CPU, 6,120 KiB peak
   RSS). Repeat it a few times with the same image and warm caches before
   deciding whether install work matters to the development loop.
2. **Instrument the 34.837-second failed PC smoke path by phase.** Add or use
   timestamps around emulator launch, VPCD connection, ATR, SELECT, and each
   APDU. Preserve a fixed overall timeout and print the phase that timed out or
   failed. Compare wall and CPU time with `/usr/bin/time`; avoid attributing the
   delay to Java execution until the phase trace shows that is where it occurs.
3. **Reduce repeat work only where the profile confirms it.** Existing
   incremental PC generation already skips unchanged Java API and ROM-mask
   outputs. For a clean build, time those generation stages separately from
   C++ compilation; for an incremental build, confirm they remain skipped.
4. **Use the existing host smoke path for fast iteration once it is passing.**
   Focused unit tests are 0.458–5.184 s in the current sample, much cheaper
   than board setup. Keep the focused test near the change, then run the
   relevant wrapper build. Hardware upload and physical reader discovery still
   have no comparable timing sample, so measure them separately when available.

## Ranked experiments

| Priority | Experiment | Evidence and expected value | How to judge it |
|---|---|---|---|
| 1 | Split PC generation, ROM-mask generation, and C++ build timings; check whether Maven's long metadata resolution repeats when inputs are unchanged. If dependencies are cached, compare a documented offline build mode while keeping the normal wrapper as the correctness baseline. | One regeneration took 5:13 in the Maven reactor after DNS failures, versus 1:01 for Java API generation. This is the largest observed build stage, but it is only one run. | Capture three clean and three no-op runs per stage. Confirm identical outputs and successful wrapper builds before adopting any offline setting. |
| 2 | Add monotonic phase timestamps and child-process accounting to the VPCD applet harness: launch, connect, ATR, SELECT, each APDU, and teardown. | The latest handler diagnostic took 7.39 s wall, 0.27 s user, 1.98 s system (30% CPU), then failed with `6F00`. Those totals do not identify the slow phase; comparatively low CPU also gives no evidence for a CPU-bound spin loop. | Repeat the exact command three times; report phase durations, parent/child CPU, exit status, and the last completed APDU. Optimize only a phase that repeatedly dominates. |
| 3 | Separate incremental, clean, and generation-invalidating ARM builds in the timer report. | Successful ARM builds span 3.120–9.152 s (mean 5.127 s); the spread may reflect different rebuild work, so it is not a reliable single baseline. | Keep source revision and output state fixed; record job count and wall/user/system time for at least three runs per build class. |
| 4 | Compare focused tests with the full host applet test after fixing the current APDU failure. | Focused host tests take 0.458–5.184 s, while the prior complete smoke attempt took 34.837 s and failed. Fast tests can shorten each fix/validate loop. | Confirm the focused test covers the changed behavior, then run the full bounded VPCD test before accepting the change. |
| 5 | Measure hardware preflight, upload, re-enumeration, reader discovery, and serial capture independently when physical testing resumes. | Upload has no comparable timing sample; past missing-node and PC/SC cases consumed time before applet execution. | Record each bounded phase and its readiness result. Treat missing hardware/reader as precondition failure, not target runtime. |

These are experiments, not claimed speedups. In particular, the 5:13 Maven
duration and 7.39-second VPCD failure are single observations; repeat them under
the same conditions before changing build or runtime behavior.

## Latest task-time sample and practical suggestions — 2026-10-15

`./tools/task-time report` currently summarizes 26 successful intervals
(183.503 s total) and 18 non-OK intervals. The successful work is dominated by
compile (14 runs, 156.315 s total, 11.165 s mean, 35.131 s max) and test (9
runs, 22.437 s total, 2.493 s mean). One 4.641-second manual thinking interval
and two 0.055-second tool checks are too few to characterize those categories.
There are no upload intervals, so upload time remains unmeasured.

The newest recorded JCAlgTest VPCD attempt took 13.116 s and failed with
`6F00`; current debugging moved from the earlier `PUTFIELD_A` report to a
`CHECKCAST` `SecurityException` after correcting JC305 class-token mapping.
This is failure-to-diagnosis time, not successful APDU performance. Older
`PUTFIELD_A` and `GETFIELD` measurements remain historical evidence from
different code states and should not be compared as runtime speedups.

### Suggestions

1. **Time each useful work unit.** Use `./tools/task-time run compile --note
   "ARM incremental" -- ./tools/build arm` for command-backed work and
   `start`/`stop` around a bounded analysis or manual upload. Keep notes precise
   enough to distinguish clean from incremental builds, focused from full
   tests, and upload from device re-enumeration. The timer records elapsed wall
   time, not CPU time.
2. **Make upload a measurable sequence.** When physical testing resumes, time
   BOOTSEL entry/preflight, firmware transfer, USB re-enumeration, PC/SC reader
   discovery, and serial validation separately. This will show whether delays
   come from transfer or recovery; do not roll a missing-reader wait into test
   execution time.
3. **Prioritize repeated work and failed iteration time.** Compile accounts
   for most recorded successful time, while 18 non-OK attempts show substantial
   iteration friction. Keep the existing incremental generated-output path,
   run focused checks before full integration, and retain a bounded timeout for
   failing VPCD cases. Do not interpret the failure count as a runtime speed
   metric.
4. **Add resource measurements before tuning.** For repeated long commands,
   capture wall, user, system, peak memory, source revision, and build state
   alongside the task-time entry. Use comparable samples to decide whether a
   build is CPU-bound, blocked on I/O/network access, or rebuilding excess
   inputs. The current timer alone cannot explain the 35.131-second longest
   compile interval.
5. **Avoid concurrent heavy builds in this VM.** The user has observed all
   eight VM cores saturating during prior work, but this timing sample does not
   include per-process CPU or contention data. Run one build at a time and
   compare bounded job counts under the same source and generated-file state
   before changing parallelism. For USB/PCSC discovery, use bounded retries
   with backoff and a deadline instead of busy polling.

These recommendations describe measurement steps and low-risk experiments;
they do not claim a speedup or identify the cause of CPU saturation. The next
useful dataset is several comparable ARM incremental builds, one controlled
VPCD phase trace after the current `CHECKCAST` defect is resolved, and separate
physical upload/preflight timings when the board test is safe to run.

## VPCD applet-debug loop

The latest trace passes ATR and applet SELECT, then the JCSystem information
APDU fails with `6F00`. It throws `StackUnderflowException` at `PUTFIELD_A`
in package 63, Method component offset 39 (CP field index 5), with operand
depth zero. The three recent bounded runs took 5.341–5.974 seconds. This is a
correctness failure and failure-to-diagnosis time, not applet throughput. It
supersedes older `SecurityException` and `GETFIELD_S_THIS` diagnoses.

The 34.837-second PC smoke attempt and older 7.39-second diagnostic are
historical timings from different failure paths. Do not compare them as a
speedup against the recent runs. A prior trace was 215,263 bytes, with repeated
`FsRead1b` diagnostics and 51 exception-handler entries; neither log volume nor
the number of scanned entries proves a performance bottleneck. Measure before
optimizing filesystem reads, dispatch, or lookup.

- **Time each APDU boundary.** Print elapsed time for VPCD connect, ATR, SELECT,
  each command send, and response receive. Include the failing APDU and last
  completed phase in the timeout message; this distinguishes VM execution from
  transport or cleanup delay.
- **Keep timeout layers consistent.** Give one APDU a short response deadline,
  cap the whole test independently, and reserve a small fixed cleanup window.
  On timeout, terminate and reap the emulator process group so a lingering
  child cannot consume CPU or distort the next run.
- **Compare wall and CPU time on failures.** Record `/usr/bin/time` wall, user,
  and system time for the integration command. High CPU close to wall time
  points toward active VM work or a spin loop; low CPU with high wall time
  points toward blocked transport, retries, or shutdown. Inspect the busiest
  process and its threads before changing polling behavior.
- **Keep diagnostics selective.** Gate CP/field traces behind a debug option
  and cap output to the first relevant lookup and exception. Large repeated
  logs slow runs and hide the transition that identifies the failing field.
- **Aggregate filesystem and exception diagnostics.** For one fixed failing
  APDU, count filesystem reads by operation and record total/max elapsed time;
  report exception-handler entries scanned and total dispatch time. Suppress
  repeated per-read logging in normal runs. Only consider caching filesystem
  metadata or indexing handler ranges if repeated samples show either phase is
  material to elapsed time; preserve the unoptimized path as the correctness
  baseline.
- **Use the smallest reproducer first.** Keep ATR/SELECT coverage, then run the
  JCSystem information APDU with a short deadline while tracing the
  `PUTFIELD_A` call/return stack around package 63, offset 39. Add later applet
  commands only after that request returns the expected response; this avoids
  paying for unrelated test phases on every fix attempt.
