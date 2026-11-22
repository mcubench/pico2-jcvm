# Second pass on CAP validation and Pico 2 bytecode performance

**Optimize checked primitive-array access first.** After the CAP caches,
byte and short array loads were the largest measured instruction group in
software AES, at **31–35%** of sampled time across two RP2350 profiles.
Reopening persistent array metadata alone accounted for **13%** in the
detailed run. Branch-boundary scans accounted for **6–9%**, and CAP
lookups inside opcode handlers accounted for about **1%**. Repeated CAP
validation dominated the original host profile. The earlier physical
A/B measured the combined gain from both caches.
[Original review and physical cache comparison](review_VMperf_20261122.md)

## Current normal firmware and ARM profile

The same installed JCAlgTest 1.8.5/jc305 applet ran on
EE116F37C7E4A04E, reader `Pico 2 CHOUPI CCID [JCVM CCID]
(EE116F37C7E4A04E) 00 00`. The normal image was `a2a69d952c309690`
with UF2 SHA-256
`cf1ea6b55d560c9a053a8dd706b284f3f3abd87865089dc0140a51372af385d7`.
It used the standard GPPro component set without Descriptor. Each replay
reset the reader and ran the same prepare and 0/1/5/50-repeat APDUs with
16-byte inputs. A zero-repeat median was subtracted from the 50-repeat
batch and divided by 50. XOR and native AES had three 50-repeat samples.
Software AES had one 50-repeat sample and shorter checks for scaling.
[Normal APDU log](../artifacts/build-audit/bytecode-second-pass-20261122/normal-apdus.log)

| Normal-image workload | 50-repeat APDU | Zero-repeat median | Net ms/op |
| --- | ---: | ---: | ---: |
| Software XOR | 133 ms | 60 ms | 1.46 |
| Software AES | 21,128 ms | 60 ms | 421.36 |
| Native-backed AES-128 ECB | 224 ms | 61 ms | 3.26 |

The software AES control agrees with the earlier optimized result of
425.68 ms/op. The native-backed row includes the full applet and Java Card
API call and its Mbed TLS operation.

The diagnostic build sampled approximately one instruction in 64 with the
RP2350 microsecond timer. It recorded about 20,200 samples from roughly
1.30 million instructions in the 50-repeat AES APDU. Timing for the branch
boundary check and CAP lookup is included in handler time. The first
diagnostic APDU took 27.743 s, 31% longer than the normal APDU. The image
with additional array probes took 20.905 s. The builds had different
instrumentation and code layout. Their difference shows that the
diagnostic times themselves are sensitive to the probe build. Use the
normal image for throughput and the profiles for approximate attribution.
[First profile](../artifacts/build-audit/bytecode-second-pass-20261122/profile-summary.txt) · [Array profile](../artifacts/build-audit/bytecode-second-pass-20261122/array-profile-summary.txt)

Both diagnostic images report CPLC source prefix `65ba03b3eb12b9f5`.
Their UF2 SHA-256 values are
`e8eb7018f24ea76132dde2b5814b35c0647d348bcc14fc310f80334385cd64a6`
for the first image and
`e874adcc64bf87fe76ffd99b96afd49cc24ae281c7c519950197142ab60980fa`
for the array image. All 35 prepare and workload APDUs returned
`AA9000` on each image. EE11 was restored to the exact normal UF2 and
its CPLC, runtime summary, applet SELECT, and GETVERSION were confirmed.
[Restored image check](../artifacts/build-audit/bytecode-second-pass-20261122/restore-normal-apdus.log)

| Sampled AES phase or handler group | First profile | Array profile |
| --- | ---: | ---: |
| Checked byte and short array loads (`baload` and `saload`) | 31.4% | 35.0% |
| Reference field read (`getfield_a_this`) | 11.6% | 11.2% |
| Byte array store (`bastore`) | 4.7% | 4.3% |
| Branch-boundary scan, within branch handlers | 6.4% | 9.1% |
| CAP view lookups, within handlers | 0.6% | 1.3% |
| Checked fetch plus opcode decode | 13.2% | 11.2% |
| Watchdog reload and interpreter bookkeeping | 9.6% | 7.2% |

The table lists selected costs. Branch and CAP timings are subdivisions of
handler work. The watchdog reload itself accounted for 1.8–3.1%. The
bookkeeping bucket includes trace history, watchdog scratch writes,
execution-budget checks, and periodic CCID progress. Individual costs
within that bucket need separate probes. The shorter XOR and native AES
batches have too few samples for the same attribution precision.

## What to improve next

### 1. Read a primitive element from one checked record

The secure persistent byte and short accessors reopen the array record
through `getPersistentField_Array()`. That function parses the record
and constructs a temporary `JC_Array`, after which the accessor checks
the reconstructed array and fetches the payload. The short accessor
reads its two bytes with separate filesystem lookups. The existing
`readPersistentArrayRange()` shows how one checked record view can
supply multiple bytes. [Array object access](../choupi/src/jc_types/jc_array.cpp#L412) · [Record reconstruction](../choupi/src/jc_handlers/flashmemory.cpp#L2342) · [Checked range read](../choupi/src/jc_handlers/flashmemory.cpp#L2607) · [Short element read](../choupi/src/jc_handlers/flashmemory.cpp#L2669)

Of 1,576 sampled byte loads, 475 reopened persistent array metadata. Each
of the 477 sampled short loads did so. Record reconstruction accounted
for 43.013 ms of the 330.555 ms summed over all instruction samples
(13.0%). Payload retrieval accounted for another 14.590 ms (4.4%). The
reconstruction measurement includes allocating the temporary array
wrapper. The first experiment should target that 13% bucket and the
duplicate lookup for a short. The 35% total for array loads also includes
work the VM needs to retain. [Detailed array profile](../artifacts/build-audit/bytecode-second-pass-20261122/array-profile-summary.txt)

The first prototype should carry the live array's checked type, length,
and index to one storage lookup. Check the returned record's identity
through the existing tag path, record type and length, and the complete
element range, then read the value within the same operation. Keep the
owner and receiver checks, array storage mode, byte and boolean handling,
short byte order, and the existing missing-body and rollback behavior.
A borrowed pointer should expire before a write, compaction, or rollback
can move or replace its record. This approach needs no new persistent
writes or extra array copies.

Validate byte, boolean, and short reads across persistent and transient
storage, including zero-length arrays, corrupt or missing records, alias
identity, and transaction abort/recovery. Measure array loads and the
same 16-byte software AES case on normal ARM images. If bounds handling
is still hot, measure tag construction and the existing array-length
cache as a separate experiment.

### 2. Shorten the interpreter loop after profiling its subphases

Sampled fetch and decode together occupied about 11–13% of AES time. An
experiment can reuse the current frame and PC within one instruction
and simplify the decoder-to-handler handoff. Fetch and operand bounds
must remain enforced against that frame, including on method return and
exception paths. Calls can change the frame or package, so frame
references should be reacquired whenever that happens. [Interpreter loop](../choupi/src/interpretor.cpp#L390)

Watchdog reload alone has a smaller ceiling. Measure scratch tracing
separately before changing its cadence. Any change needs a worst-case
service interval, continued CCID progress, and usable reset diagnostics.
Current hardware measurements leave no reason to reduce those protections
before testing the checked array path.

### 3. Bound branch-validation work with a small, checked cache

The taken-branch check rescans from the method start to the target. That
cost grows with the distance from the start, and the AES result puts it
below arrays and the interpreter loop. At the measured share, even
eliminating the scan cost would give about **1.07×** on this workload if
the measured mix held. Other applets may have longer hot methods or more
branches. Using the second profile puts that arithmetic ceiling at
about **1.10×**. [Instruction-boundary scan](../src/cap_instruction_boundaries.hpp#L133)

If branch-heavy workloads justify it, try a bounded cache for successfully
verified targets, keyed by package revision, the exact method range, and
target offset. Keep the live range test and run the existing checked scan
on a miss. A per-method bitmap is an alternative when dense target coverage
justifies it, at one bit per byte of method body plus metadata. A bitmap
for an entire 19,178-byte Method component would use about 2.4 KiB before
metadata. Budget the working set explicitly and retain a safe fallback.
Inferred Descriptor-free frame ranges still have the existing
independent-method-start assurance gap.

### 4. Keep CAP security checks and reduce duplication only where measured

The diagnostic reset and selection sequence recorded three
`validate_cap_code()` calls over the 26,743-byte transmitted image, at
0.714, 0.581, and 0.558 ms. This includes parsing and checks that can be
done without resolving dependencies. It excludes import-dependent
INSTALL preflight and does not measure a full CAP-store CRC scan.
Warm dynamic and ROM CAP lookup time inside handlers was 0.6–1.3%
of sampled AES time. Extending cache lifetimes or removing repeat
validation at lifecycle boundaries has little support as the next
steady-state optimization. [Lifecycle sample](../artifacts/build-audit/bytecode-second-pass-20261122/profile-serial.log)

The global store revision invalidates views conservatively even when an
unrelated slot changes. Keeping that design simplifies the proof against
stale views after interrupted writes and same-address replacement. If
cold multi-package workloads later show meaningful cost, reuse the
validated structural view within a single LOAD/INSTALL/restore stage
before considering per-slot generations. Keep AID and slot checks,
dependency linkage, storage integrity checks and the existing runtime
trust boundary. A full verifier remains separate work and must establish
its guarantees before any corresponding runtime work can be moved.
[CAP execution trust model](CAP_EXECUTION_TRUST_MODEL.md)

The existing field-offset cache is still useful. The remaining
`getfield_a_this` time includes live reference and ownership handling.
Measure cache hit rate and those subphases before changing its size or
lifetime. A longer-lived cache would also need to account for changes
to dependency availability and applet contexts.

## Verification and scope

The production VM stayed unchanged. Both diagnostic runs use a copied
source tree under `build/` and exist for attribution under INV-24.
The audit uses the Cortex-M33 and the unchanged CAP and pinned fixture
under INV-01 through INV-05 and INV-26. The cycle preserves the existing
CCID and management path and the card's storage under INV-06 through
INV-16. An array-read experiment should preserve tear, rollback, and
reclamation semantics before any result is accepted.

The known static verification and firewall gaps remain tracked under
INV-18 and INV-19. Malformed-input recovery coverage remains open
under INV-22. The profile establishes no side-channel or key-storage
guarantee under INV-20 and INV-21. Any follow-on must preserve the declared
resource limits and distinguish normal hardware measurements from
diagnostic timing and full empty-card acceptance under INV-23 through
INV-28. [Product invariants](../SPEC.md)

## Reproduction

Build the isolated image with
`./tools/build-vm-perf-profile build/vmperf-arm-profile-new`. Its output
is a diagnostic ARM `max_crypto` image. Use the documented physical-token
procedure to flash and collect a bounded serial log. Replay the previously
recorded JCAlgTest APDUs and run
`python3 tests/hardware/summarize_vm_perf_profile.py SERIAL_LOG APDU_LOG`.
Keep normal and diagnostic logs separate. [Profile builder](../tools/build-vm-perf-profile) · [Counter parser](../tests/hardware/summarize_vm_perf_profile.py) · [Physical procedure](../reference/physical-token-access/SKILL.md)
