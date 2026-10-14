# Development performance tips

These are measured observations and low-risk experiments for keeping the Pico 2
development loop responsive. The initial task-time sample is small; repeat each
measurement before treating a change as a lasting improvement.

## Current measurements

From `artifacts/metrics/development-time.csv`:

| Task | Successful elapsed time | Notes |
|---|---:|---|
| Incremental ARM CHOUPI build | 3.903 s | `./tools/build arm`; rebuilt and linked the changed ARM target. |
| Short APDU host tests | 0.458 s | `./tools/test-short-apdu`; all cases passed. |
| Timer syntax check | 0.055 s | Two successful runs. |
| Manual analysis interval | 4.641 s | Wall time, not CPU time. |
| Upload | Not measured | No upload was attempted in this sample. |

The full Maven CAP converter build completed in 3:29 before timing was enabled.
That number was observed in the build output but is absent from the timer CSV.
The sample also contains a failed timer invocation and an interrupted monitor
attempt; the report excludes both from successful duration averages.

## Suggestions

1. **Keep the fast loop incremental.** Run the focused host test after a small
   change, then `./tools/build arm`. Preserve generated CAP and ROM-mask outputs
   so ordinary C++ edits do not trigger the Java/Maven pipeline. Use the
   repository wrappers so their manifests can detect stale generated files.
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
   counts with `./tools/task-time report`. The current sample is too small to
   establish stable averages.
