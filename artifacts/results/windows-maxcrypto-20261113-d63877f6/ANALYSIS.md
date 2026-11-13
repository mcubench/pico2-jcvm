# First-failure analysis — 2026-11-13

## Conclusion

The first observed failure is **lost communication on the reset/GC APDU**, not a returned resource exception and not failure of the AES primitive. This is a remaining reset/cleanup/CCID liveness problem in an image that already includes the old compaction rehash fix and stale-extension recovery. It does not establish whether the VM got stuck, failed internally, or produced a response that the USB/CCID path failed to deliver. Without device telemetry, a unique root cause cannot be assigned.

## Exact sequence

References are line numbers in supplied `ALGTEST_log_1794571413897.log`.

1. Method at line 15820: `TYPE_AES LENGTH_AES_192 ALG_AES_BLOCK_128_ECB_NOPAD Cipher_doFinal()`.
2. Preparation `B031` succeeds (`AA9000`, 1,381 ms). All five zero-iteration baselines succeed: 1,546 / 1,744 / 1,346 / 1,310 / 1,349 ms; client baseline average 1,459.2 ms (it prints 1,350 ms for the last host timing).
3. A reset succeeds in 392 ms. The first full measurement `B043`, configured for 50 target iterations, returns `AA9000` in 1,693 ms. Client prints corrected aggregate measurement `235.80`; this is not yet a completed five-sample result.
4. The next command is **`B0E2000000`**, requesting reset/deletion before the second full measurement. At line 15936 it receives no APDU response and the host reports `0x8010002f`.
5. Client attempts another measurement `B043` anyway; this also fails. Reconnect obtains card metadata, but SELECT fails with the same error. The log ends at the reinsertion prompt (line 15951), consistent with stopping at the first incident. These later commands are recovery attempts, not independent first failures.

`0x8010002F` is `SCARD_E_COMM_DATA_LOST` (confirmed against local Windows bindings), rather than a Java Card status word. It must not be interpreted as F205 or memory exhaustion. There is no response duration for the failed reset, so the timeout length cannot be measured from this log.

Across the log: 1,923 command headers, 1,920 responses, **all SW=9000**; 956 adjacent reset/9000 pairs; 120 method headers and 83 completed operation-statistics blocks. Command minus response count is three: reset, following measurement, reconnect SELECT. Counts are not a support verdict. The CSV ends with the preceding completed AES-192 CBC init case; the failed ECB doFinal case appears in the log but has no completed CSV measurement.

## What this changes relative to the older Windows run

The previous run first stalled on an AES measurement, then after reinsertion eventually returned F205 and 6F00 during RSA preparations. This run stops before such a second phase. It therefore neither reproduces nor rules out the later persistent-resource failure. It does prove that simply including the rehash fix has not removed the first communication-loss failure class from this external test. It does not demonstrate that the fixed rehash algorithm itself failed.

The old leading storage hypothesis remains applicable to the older image only as a hypothesis. Assigning this new failure to that same old rehash defect would ignore its inclusion in the returned firmware. A remaining filesystem defect or resource problem could still make reset fail, but there is no storage status, failed-record trace, or memory measurement here.

## Ranked explanations

**1. Response/USB progress during or after cleanup.** Reset requests native object deletion; persistent root traversal, deletion and sync run in its servicing path. Separate local evidence already showed a reset returning VM 9000 after cleanup/sync while its final CCID response remained pending. Furthermore, a subsequent local 2BED run with stale-extension recovery still stalled at xfr=1156 with response pending; see QKB section “CCID response retry remains unresolved.” Thus the 100 ms idle-endpoint recovery was never proven to cover the whole transport failure class. It handles only final-response-pending + extension-in-flight + endpoint-idle with a sufficiently old timestamp, and requires its servicing function to run. It cannot resolve a busy endpoint, a submitted-but-uncompleted response, or execution that never returns to response servicing. New Windows evidence is consistent with these remaining paths, not proof of a particular one.

**2. Reset/collector execution fails to finish.** A sweep may trace a large or inconsistent object graph, skip for incomplete roots, or perform costly storage sync. The eight-record cap limits erasures, not traversal, compaction, allocations or time spent in sync. This could delay progress or expose another runtime defect. No serial FAULT, stack/heap state, deletion summary, or FS failure message was supplied, so this cannot be distinguished from response delivery.

**3. Host driver/service or USB link fault.** A PC/SC communication-loss code and subsequent SELECT failure allow this explanation too. Reconnect returning ATR/metadata does not establish fresh successful card communication; SELECT is the next actual failed exchange. No USB trace or service log isolates host versus device.

AES implementation failure is a weaker direct explanation: prepare and all baselines succeeded, then the first 50-iteration target measurement also succeeded; the failing APDU is reset. Corruption caused by the preceding operation is possible in principle but not established by any observed invariant or diagnostic.

## Eight-record budget and measurement quality

This actual 1.8.5 log sends **reset before preparation and before every baseline and measured invocation**. For a completed method that normally means 11 resets plus preparation and ten measurements: 22 APDUs, at most 176 record deletions if a request remains pending and tracing succeeds throughout. This corrects the once-per-method illustration from the earlier discussion; it is not evidence that 176 records were actually reclaimed. Requests can clear early and root failures can skip deletion. The failed target reached five baseline samples and one full sample, not all five full samples.

The higher reset frequency weakens a simplistic “only eight records per whole method” explanation. A measurement may still allocate within its 50-iteration APDU before any subsequent batch, but **no returned exhaustion error is present here**. Moreover, whole-APDU baseline timings vary substantially; retained cleanup or sync work can affect the subtraction-based performance estimate differently between baseline and target invocations. Existing CHECK markers and timing variability should not be treated as precise primitive throughput or evidence of crypto correctness.

## Next diagnostic boundary

The next useful observation is device-side state at the first failed reset: did reset bytecode finish, did heap tracing/deletion finish, did sync return, and was a final CCID response submitted/completed? Existing concise deletion/FS/fault output should be retained with client output even when debug tracing is disabled. If that is insufficient, a separately authorized diagnostic run can use existing response-state traces, while keeping no-debug acceptance testing separate. Preserve the failed board state before an empty-card reflash if available. No new external test request is issued in this step; missing telemetry is a limitation, not evidence of a specific cause.

No implementation, build, flash, or physical campaign was performed for this analysis. Fixture manifest verification passed (81 inputs); supplied file integrity and package identity were checked separately.
