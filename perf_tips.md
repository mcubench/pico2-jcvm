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

The timer report totals 52.821 seconds across successful compile, test,
analysis, and tool-check intervals. Build and test durations vary with which
objects the command recompiles, so compare like-for-like runs before drawing a
performance conclusion.

The full Maven CAP converter build completed in 3:29 before timing was enabled.
That number was observed in the build output but is absent from the timer CSV.
The CSV also contains two failed build attempts, four failed test attempts,
and one interrupted monitor attempt; the report excludes non-OK runs from
successful duration averages. A bounded JCAlgTest probe also found no PC/SC
reader, so it sent no APDUs.

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
