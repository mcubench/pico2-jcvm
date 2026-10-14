# Development performance tips

These are measured observations and low-risk experiments for keeping the Pico 2
development loop responsive. The sample now covers several builds and host
tests, but it is still too small to estimate stable long-term averages.

## Current measurements

From `artifacts/metrics/development-time.csv`:

| Task | Successful elapsed time | Notes |
|---|---:|---|
| ARM build | 3.120–9.152 s; mean 5.127 s | Five successful `./tools/build arm` runs; two failed attempts are excluded. |
| Focused host tests | 0.458–5.184 s; mean 2.493 s | Nine successful runs across short APDU, install rollback/status, and RP2350 filesystem behavior. |
| Timer syntax check | 0.055 s | Two successful runs. |
| Manual analysis interval | 4.641 s | Wall time, not CPU time. |
| PC API + ROM-mask generation | 1:01 + 5:13 | One full regeneration; the Maven reactor spent 5:13 resolving metadata before finishing from its local cache. |
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
