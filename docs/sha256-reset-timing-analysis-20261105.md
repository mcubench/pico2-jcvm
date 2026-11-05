# SHA-256 reset timing analysis — 2026-11-05

The supplied 6.20 ms `MessageDigest_reset()` result overstates reset cost.
The local JCAlgTest 1.8.2 client subtracts a per-operation `doFinal()` time
once from a complete 50-operation batch. Using the supplied 3.78 ms
`doFinal()` result, the corrected differential estimate is 2.50 ms/reset.
This includes interpreter, native dispatch, receiver lookup and loop overhead;
it is not a direct timing of the C++ reset function.

## Applet and native implementation

`AlgTest_JavaCard/src/algtest/AlgPerformanceTest.java:1549` resets the digest
once before entering any measurement loop. Its reset branch (line 1566)
executes `doFinal(m_ram1, 0, chunkDataLen, m_ram2, 0)` followed by `reset()`
on every iteration. The doFinal branch executes only doFinal in its loop.
The source explicitly comments that time subtraction is needed.

The supplied config decodes to digest class 0x15, algorithm 4 (SHA-256),
method 4 (reset), input length 0x0100 (256), 0x0032 (50) whole-operation
repeats and one sub-operation. Five measurement batches give 250 iterations.

`choupi/src/native_message_digest.cpp:217` implements reset by resolving the
receiver and calling `clearInput()`. That helper clears only the accumulated
input length and sets the native session length to zero. Successful doFinal
already calls the same helper before returning (line 212). Consequently the
explicit reset in this benchmark has zero input bytes to wipe. It does not
perform another SHA-256 computation or wipe the entire 2046-byte capacity.
The native session short setter updates RAM rather than committing flash.

## Host client units bug

Inspected `build/algtest-client-1.8.2/AlgTestJClient.jar` with `javap -c -p
algtestjclient.PerformanceTesting`; no Java code or CAP was modified.

- `testMessageDigest` stores the return of the doFinal measurement and passes
  it as the subtraction argument to the reset measurement.
- The measurement computes the return value as total corrected milliseconds
  divided by whole-operation repeats times measurement batches: ms/operation.
- The inner measurement subtracts the zero-repeat baseline from each complete
  batch, then subtracts that argument directly, before dividing by repeats.

For N repeats, D ms/doFinal, wall time W and baseline B, the client reports
`(W - B - D) / N`. The required subtraction is `(W - B - N*D) / N`.
The reported value therefore retains `(N-1)*D/N` ms per iteration from
doFinal. The same subtraction path also serves the update test; assessing
that test's appropriate comparator is separate work.

## Numerical reconstruction

The CSV's "operation raw measurements" are already baseline- and
subtraction-adjusted batch values, not original APDU wall times.

| Quantity | Value |
| --- | ---: |
| Baseline mean, B | 1282.60 ms |
| Repeats per batch, N | 50 |
| Supplied doFinal time, D | 3.78 ms/operation |
| Corrected batch mean as printed | 310.22 ms |
| Printed per-operation mean before rounding | 6.2044 ms |
| Missing subtraction per batch, 49 × 3.78 | 185.22 ms |
| Corrected reset estimate | 2.50 ms/operation |

Reconstructed wall times are 1607, 1589, 1597, 1595 and 1595 ms:
each printed batch plus 1282.60 plus 3.78 is an integer. Corrected reset
samples are 2.708, 2.348, 2.508, 2.468 and 2.468 ms/operation.
This strongly matches the local client's subtraction behavior.

The large zero-repeat baseline is measured separately: it includes applet
entry, settings parsing, the initial reset and response/transport/runtime
overhead. These observations do not isolate its 1282.60 ms cost, or prove
which component dominates the residual 2.50 ms. In particular, do not
attribute either to SHA hardware, flash writes or USB alone without profiling.

## Validation, limits and next action

Validated the arithmetic with Python and inspected applet, native digest,
native session field and local client bytecode paths. No firmware source was
changed, no builds were needed, and no hardware was flashed or measured.
The user's actual client binary, installed CAP and firmware identity were
not supplied; the diagnosis is established for the local client and strongly
consistent with the supplied CSV. D is rounded to two decimal places and
the complete doFinal measurement block was not supplied.

For a host-side fix, multiply the per-operation subtraction argument by the
matching batch operation count (50 here). Validate equal data lengths and
repeat/sub-operation settings, and use the full precision doFinal result.
Retain the upstream CAP unchanged. To identify native cost separately, time
native dispatch/reset on the device with RAM counters and report after the
timed section; compare matched batches on the same installed image.

Local client SHA-256:
`43d1d9c1cf92b8f5720c87ec0815e89ae5c713ec254713dcb24ffa431f4e6e90`.
Bytecode inspection artifact: `artifacts/sha256-reset-client-javap.txt`, SHA-256
`90ce5413747122e4d0e7d8f7ad3443a447c20acf0b61f6e776dbf990e0419b2f`.
