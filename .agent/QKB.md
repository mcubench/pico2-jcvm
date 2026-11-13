# Issue knowledge base

Compacted: 2026-11-11. Reusable diagnoses and fixes; current tasks live in
[TODO](../TODO.md), device/build state in [HANDOFF](HANDOFF.md), and run history in
[progress](../progress.md). A scoped fix does not close broader acceptance.

The [byte-for-byte backup](archive/2026-11-11-qkb-compaction/QKB.md),
[SHA-256 manifest](archive/2026-11-11-qkb-compaction/SHA256SUMS) and
[migration notes](archive/2026-11-11-qkb-compaction/MIGRATION.md) preserve all original text.
The [complete issue catalog](QKB_INDEX.md) indexes every original heading and the earlier historical index.
Catalog titles retain dated wording; read later corrections and the summaries below before reusing a diagnosis.
Evidence links identify archived sections; resolve original relative links from `.agent/`.

## USB visible, physical reader missing — unresolved / volatile

Runtime USB, PC/SC reader publication, card presence and APDU success are separate checks.
ACL access alone does not prove reader publication; libccid VID/PID support was present in captured failures.
Use the latest [handoff](HANDOFF.md) for device state, then refresh authorized access and enumerate the exact reader.
Reader slot suffixes change: match the 16-digit chip ID and refresh the full reader name before each test.
Keep board-specific logs; switch to another identified board if one disappears.
Host-only visibility with no guest USB device suggests passthrough trouble, not a proven firmware fault.
Do not start or claim a physical campaign until the client connects to that exact reader.
Do not restart PC/SC while another test is active; service restart can interrupt VPCD transactions too.

Evidence: [E002](QKB_INDEX.md#e002), [E032](QKB_INDEX.md#e032), [E425](QKB_INDEX.md#e425), [E006](QKB_INDEX.md#e006).

## Host access and helper failures — viable execution path

Every USB/PCSC attempt must use elevated host execution from the first attempt.
For repository wrappers also set `PICO2_HOST_USB_ACCESS=1` on that command.
Only the approved `sudo -n /usr/local/sbin/pico2-pcsc-reader` exception is authorized;
do not reuse historical unrestricted sudo/service commands. Helper integrity/product-string failures
need the approved helper repair described in [external requests](EXT_REQUESTS.md).
Local VPCD sockets, AF_UNIX tests and ptrace can also fail under sandbox policy;
EPERM/bind/ptrace denial is a host-access failure, not a protocol regression.
Use the required execution path; if unavailable, stop hardware work and continue offline.

Evidence: [E417](QKB_INDEX.md#e417), [E029](QKB_INDEX.md#e029), [E367](QKB_INDEX.md#e367), [E413](QKB_INDEX.md#e413).

Host-access regression, 2026-11-12 22:49 UTC: the approved reader helper was
invoked through the required elevated host path, but `sudo` returned
`The "no new privileges" flag is set`. No USB work followed that failure;
do not retry in the restricted context. Continue offline until host execution
is restored.

## Targeted flashing and BOOTSEL — scoped viable fallback

Runtime `2e8a:0009` is not BOOTSEL `2e8a:000f`.
Use repository wrappers and a mandatory chip ID; build both architectures before flashing.
The targeted full-VM path is `tools/cycle choupi --profile <profile> --chipid <ID>`;
`cycle arm`/RISC-V smoke output is not full Java Card acceptance.
CDC 1200-baud reset has recovered boards when picotool reset lookup failed. On
2026-11-12, identity-pinned recovery entered BOOTSEL in 4/3/3 seconds on D92,
8E and 2BED; all three firmware-only flashes verified. Their finite monitors
passed and PC/SC listed all readers after the approved helper refreshed node
access. Refresh that helper after USB re-enumeration before treating a missing
PC/SC reader as firmware failure.
Close serial monitors before resetting; failure after doing so disproves monitor interference as a universal explanation.
If bounded reset fails, change identified board or request host passthrough recovery instead of repeatedly retrying.
Flashing preserves installed CAP storage unless an explicitly prepared empty-card image is used.
A flash success plus serial heartbeat proves runtime startup only.

On 2026-11-12 the VM reassigned two boards to a different USB topology path
during CDC-to-BOOTSEL re-enumeration. The selector now rediscovers a selected
board by the already pinned RP2350 chip ID when its old path disappears or is
occupied by another device; it still rejects a same-path identity mismatch if
the original chip cannot be found, and never falls back to a sole unrelated
board. `./tools/test-flash-target-selection` passes 11/11 synthetic cases.
The updated path was used to flash empty-card max_crypto to D92 and 8E; both
passed finite serial health. Physical registry verification is pending PC/SC
recovery.

Evidence: [E429](QKB_INDEX.md#e429), [E093](QKB_INDEX.md#e093), [E106](QKB_INDEX.md#e106), [E065](QKB_INDEX.md#e065).

## VPCD setup, stale state and process lifecycle — viable controls

Rebuild the selected PC profile after native changes; identify emulator/ROM/fixture hashes.
Use a freshly generated empty image for clean-card tests: stale images can retain apps, proxies or incompatible state.
Discover readers with bounded, logged retries and pin one reader through install, probe and cleanup.
A socket-connect timeout may hide an INSTALL failure; preserve emulator stdout and inspect pre-connect stages.
Terminate only the known test process group when a wrapper times out; a detached emulator can survive and consume CPU.
RV-20261106-32 now propagates tested startup, VPCD, install and persistence
failures to the parent exit code. `./tools/test-emulator-exit-status` covers
absent restored applet, refused connection, malformed frame, failed install,
image/sidecar save failures and clean close. Run it through host execution that
permits `ptrace`; continue checking diagnostics and results for new failure
paths beyond this matrix.

Evidence: [E360](QKB_INDEX.md#e360), [E089](QKB_INDEX.md#e089), [E288](QKB_INDEX.md#e288), [E297](QKB_INDEX.md#e297), [E342](QKB_INDEX.md#e342).

## CAP Class external references — validate Import token bounds before storage

External superclass references in Class records and external superinterface
references in Interface records also carry package tokens into the Import
component. Checking only Constant Pool references leaves malformed inheritance
metadata able to pass LOAD. Walk Class component records before persistence and
reject any external package token whose low seven bits are outside Import
count. Preserve the `0xFFFF` no-superclass sentinel. Regression uses unchanged
JCAlgTest 1.8.4 bytes with its applet superclass changed from `8203` to `FF00`;
the VPCD test confirms LOAD rejects before package storage. Keep separate
coverage for Import/Export consistency and transitive library dependencies.

## Late mixed-suite resource failures — unresolved; reproduce state

Capture exact ordered APDUs, initial storage, profile/build/backend and first terminal error.
Compare fresh versus accumulated state and cleanup enabled versus disabled; shorten the preceding sequence before broad reruns.
Track live RAM/handles, unreachable durable bodies and obsolete filesystem versions separately.
F205/NO_RESOURCE with persistent OutOfFlash is not proof of a RAM leak, unsupported algorithm or transport fault.
PC filesystem capacity/layout differs from the Pico overlay; do not transfer thresholds or timings between them.
An isolated RSA replay or a flat focused plateau does not close the full unchanged 1.8.4 workload.
Keep timeout, deliberate interruption, induced ptrace panic and genuine runtime failure distinct.
External Windows RSA preparation failed before the measurement APDU; the method label alone does not establish an arithmetic failure.
Continue RESOURCE-01 and offline-only JC-RESET-01 under [TODO](../TODO.md).

Evidence: [E012](QKB_INDEX.md#e012), [E409](QKB_INDEX.md#e409), [E368](QKB_INDEX.md#e368).

## Reachable same-owner alias target was reclaimed — root cause reproduced and guarded; physical acceptance open

`setPersistentField_Reference()` used to republish an alias to a persistent
array's cached body tag without confirming that the body still existed. A
focused regression deletes the body while retaining the heap handle; the old
path writes a dangling alias, while the current path throws `IOException` and
leaves the destination field unset. This protects against propagating a
corrupt edge but does not recover a body already lost.

The archived 2026-11-11 trace showed the first missing array-body read
(`03013894FFFFFE000000420003FFFE000000470000FFFE000005810001FFFF00000583`)
after two eight-record deletion batches, but lacked candidate-tag logging.
The 2026-11-12 candidate-tagged 8E run established the cause for a later
reproduction: reachable alias source `03013894FFFFFE000000420007` named body
`03013894FFFFFE000000420007FFFF00000045`, yet the same-owner reclaimer
selected that body for `stage=erase`. The heap graph had omitted that edge, and
candidate selection relied on heap reachability plus foreign aliases without
protecting a durable alias under a reachable same-owner prefix.

Commit `36fcfe69` adds that protection. A candidate body ending in the stable
array marker and identity is mapped back to its alias field prefix; if that
prefix is reachable and its serialized alias chain targets the body, the
candidate is retained. Invalid, cyclic, or unreadable alias chains abort the
inventory fail-closed. A regression with a live root and deliberately omitted
field-layout edge proves the body survives and strict restoration works. On
the corrected D92 physical run, the exact old target has appeared as
`stage=alias-protect`, not `stage=erase`; through the captured checkpoint there
were 1,092 alias protections, 184 unrelated erasures and zero
`HEAP_GC_FS_READ_FAIL` or deletion skips. The full unchanged 1.8.4 run is still
in progress, so this is early evidence rather than full RESOURCE-01 acceptance.

The earlier failing 8E run then reported `HEAP_GC_FS_READ_FAIL` for the erased
body and later skipped conservative heap reclamation, followed by handle
growth and `F205`. This confirms the chain from the missing durable body to
incomplete graph walk and retained handles for that reproduced case. Other
historical resource failures may still have additional causes.

Evidence: `progress.md`, entry `RESOURCE-01-REACHABLE-ALIAS-TARGET-GUARD-20261112`;
regressions `reachable_durable_alias_protects_body_from_incomplete_heap_walk`
and `persistent_array_handle_rejects_realiasing_missing_body`;
8E trace `artifacts/results/jcalgtest-1.8.4-resource01-20261112/8E464F4E2BBC0A19/option1-all-algorithms/serial.log`;
corrected D92 trace `artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option1-alias-guard-aa44cb12/serial.log`.

The host reclaimer now has a focused foreign durable-alias cycle regression:
two aliases owned outside the target applet point to each other. Alias
inventory returns `InvalidTag` before deleting the target or an unrelated
orphan; after removing the corrupt records, retry reclaims the now-unreachable
target records. This confirms bounded fail-closed behavior in the host stub,
not physical recovery or the cause of the archived missing-body incident.

Evidence: `progress.md`, entry `PERSIST-01-ALIAS-CYCLE-20261112`;
regression `persistent_alias_cycle_blocks_reclamation_then_retry_succeeds`.

## Request-gated deletion and batch budgets — implemented, physical acceptance open

Normal firmware reports object deletion unsupported. Diagnostic `PICO2_ENABLE_OBJECT_DELETION=1`
enables deferred servicing after the Java stack unwinds; it does not authorize production enablement.
An unbounded physical sweep erased 1,417 records before reply and coincided with transport loss;
its exact duration was unmeasured. Both PC and physical paths now share
`reclaimUnreachableObjectDeletionBatch()` and one total budget of eight records per APDU across applet/package namespaces.
Retain pending work and resume later; separate per-namespace limits would allow 16.
The VPCD batch fixture must drain with status APDU `00000D`, not new request `000008`.
One/four replacements pass with 2/6 collector calls and 10/40 records erased; assert both counts and per-call limit.
Physical response timing, actual interruption recovery and long-suite acceptance remain open.

Evidence: [E001](QKB_INDEX.md#e001), [E426](QKB_INDEX.md#e426), [E427](QKB_INDEX.md#e427), [E428](QKB_INDEX.md#e428).

## Persistent reclamation roots — fail closed; focused host evidence

RAM GC does not reclaim persistent records. Protect direct registered applet roots even if no heap object represents them,
including backing tags longer than the root prefix. Package-static graphs need their own inventory.
Trace live aliases across owner/package namespaces and cached peer statics, shareable server refs and RMI roots.
Malformed/unreadable edges, incomplete scans, active transactions or ambiguous duplicate package contexts must block erasure.
Tests demonstrate rooted peer objects survive while an orphan is erased, plus shared batches and retry behavior.
The production reclaimer participates in 1,000 modeled flash cuts; modeled cuts and a single-owner physical hook
are not physical multi-owner power-loss acceptance. PERSIST-01/02/03 gates still apply.

Evidence: [E372](QKB_INDEX.md#e372), [E373](QKB_INDEX.md#e373), [E412](QKB_INDEX.md#e412), [E414](QKB_INDEX.md#e414), [E384](QKB_INDEX.md#e384).

## Foreign-reference GC cycles — coordinated tracing, acceptance open

Foreign-reference registry edges alone must not become permanent roots.
The coordinated collector traces real roots, follows foreign instance/array edges from marked objects to a fixed point, then sweeps.
Host tests cover cycles/diamonds, repeated collection, cached static/proxy roots and production retained-context enumeration.
Ambiguous duplicate package contexts and incomplete target-context enumeration fail closed.
Physical reset/deletion, broader context edge cases and bounded actual RAM measurements remain RV-20261106-59 gates.

Evidence: [E418](QKB_INDEX.md#e418), [E419](QKB_INDEX.md#e419), [E421](QKB_INDEX.md#e421).

## Host filesystem edit relocation — viable fix for IO_LOCKED

Relocating an edited record through `write_impl()` indexed it in the defrag staging sector,
then attempted to erase that still-locked sector. Keep the old record indexed, build patched data in owned memory
and use normal `write()` allocation/replacement. Regression verifies reinitialization and no live staging entry.
`IO_LOCKED=129` is distinct from `OutOfFlash=1`; do not map both to NO_RESOURCE.
The 414-command current-profile replay passes; this fixes one failure, not RESOURCE-01 overall.

Evidence: [E011](QKB_INDEX.md#e011).

## Host compaction tracing overhead — confirmed fix and measurement

Per-word host flash programming and privilege checks caused 29,590 tracer traps / 40.032402 s per measured defrag.
Bulk programming plus removal of release-mode privilege traps reduced the measured PC VPCD defrag to 2,731 us / zero traps;
128 replacements pass 133/133 and APDU 124 took 0.314 s. FS tests pass 36/36 (two ignored), asymmetric VPCD 107/107.
Preserve flash AND/boundary behavior, failure injection and emulator MPU checks; embedded flash paths are unchanged.
The earlier “probable cause” and 43–102-second timings are superseded for this optimized host path,
not measurements of physical Pico flash.

Evidence: [E405](QKB_INDEX.md#e405).

## Persistent arrays and transaction performance — measured scope

Bulk 128-byte persistent copy reduced harness FS calls from **1,024 to 3**; 1,152 was corrected instrumentation.
Validate complete type/length/ranges, stage overlap safely and preserve non-atomic snapshot semantics.
Two physical boards passed 17/17 transaction checks; Util 19/19 and symmetric/asymmetric VPCD 102/102 and 107/107 pass.
Rollback-generation-keyed array metadata caching reduced 128 `size()` calls from 256 FS reads to 1.
It showed **no physical speedup**: cached arraylength 4.799–4.845 s versus 4.598–4.607 s;
mixed access 1.559–1.596 s versus 1.553–1.593 s. Invalidate on rollback and tag changes.
Do not equate fewer calls with lower end-to-end latency.

Evidence: [E406](QKB_INDEX.md#e406), [E407](QKB_INDEX.md#e407).

## Durable identity and rollback — reusable invariants

Persistent fields must alias stable object/array body identities instead of owning those bodies.
Replacing/clearing one field must preserve other aliases; failed first persistence and transaction abort must be retryable.
Keep cached scalar/reference/static values canonical with durable state after write failure and rollback.
Transient array identity must not make transient contents persistent. Missing-body recovery and rollback generation must agree;
reserve identities safely so abort does not reuse them, and encode both index bytes in persistent tags.
Focused RESOURCE-02 64-init AES/RSA plateaus are zero-growth; alias setup retains one 4-byte unreachable tag.
The full mixed workload and physical persistence gates remain open.

On rollback, `fs_sync()` must run with the persistent-transaction-active predicate temporarily false:
the Pico backend intentionally returns success without publishing while that predicate is true. Restore the active
guard and retain snapshots if this publication fails. Test doubles must model this deferral; a stub that always commits
on `fs_sync()` can conceal a rollback that reports success while leaving the durable snapshot unchanged. The R44
regression now checks failure quarantine and retry, plus simulated cold reset to the previous committed state.
This is host/integration evidence; physical failure/recovery remains unverified. See [R44](../TODO.md#open-review-acceptance--2026-11-06).

Evidence: [E061](QKB_INDEX.md#e061), [E060](QKB_INDEX.md#e060), [E397](QKB_INDEX.md#e397), [E401](QKB_INDEX.md#e401), [E402](QKB_INDEX.md#e402).

## Overlay capacity and package deletion — distinguish live data from tombstones

Deleted dynamic records formerly consumed all 8,192 index slots. Compaction now retains/remaps parent-slot references
and can recover tombstones on write pressure. A live full set still needs reclamation or a capacity change.
Track both payload bytes and index slots; old arena sizes are version-specific historical baselines.
Cross-store INSTALL/DELETE publication needs journal/replay/rollback at real durable boundaries.
Simulated cuts, warm resets and firmware reflash do not demonstrate actual USB power-loss recovery.
After failed DELETE, reconcile retained runtime contexts and stale routes as well as durable registry state.

Evidence: [E131](QKB_INDEX.md#e131), [E058](QKB_INDEX.md#e058), [E030](QKB_INDEX.md#e030), [E228](QKB_INDEX.md#e228).

## SIO/firewall ownership — implemented scope and limits

Firewall context belongs to the CAP; applet instances retain distinct lifecycle/owner state.
Compact handles require owner provenance across parameters, returns, arrays, statics and exceptions.
A client proxy must refer to a context-owned server route; never pass a raw server handle as a client object.
Check Shareable interface/class membership, argument kinds and typed stack/local metadata before use.
DELETE/reset must invalidate stale routes, including same-AID reinstall; persisted proxies require validated routing metadata or fail closed.
Foreign static/array/instance retention has scoped support, not blanket cross-context access permission.
Collect retained heaps before checking DELETE edges so unreachable registry entries do not falsely block deletion;
persistent alias scans must still protect real references. Complete CTX-01/03 acceptance before broadening support.

Evidence: [E113](QKB_INDEX.md#e113), [E116](QKB_INDEX.md#e116), [E044](QKB_INDEX.md#e044), [E337](QKB_INDEX.md#e337), [E359](QKB_INDEX.md#e359).

## SensitiveResult — publish only valid successful results

Clear prior sensitive state before a sensitive operation can fail, including JNI conversion paths.
On success, publish the specified boolean/short/object with owner provenance; passive calls must preserve lifetime.
Digest/Cipher/Signature and other enabled producers have focused interpreted SIO checks;
object results must remain roots until reset. Native simulation and applet execution are separate evidence.
Unsupported Checksum/AEAD providers do not count as covered producers. CTX-02 and RV-20261106-25 retain physical/conversion gaps.

Evidence: [E068](QKB_INDEX.md#e068), [E069](QKB_INDEX.md#e069), [E070](QKB_INDEX.md#e070), [E055](QKB_INDEX.md#e055).

## Crypto key/session lifecycle and dispatch — scoped fixes

Cryptographic intermediate bytes belong in reset/transaction-correct native session storage,
not persistent Java object fields. Transient key clearing must invalidate retained Cipher use.
CBC doFinal resets active IV to zero; explicit init can restore a supplied IV. Audit MAC and reset behavior separately.
JC305 inherited/redeclared Key methods require per-interface token maps; class index and CAP class token differ.
For CRT signatures, check component length before resolving a possibly null modulus; derive it from P/Q when absent.
Use native C++ dispatch; supplied upstream CAPs remain unchanged. Consult the catalog for algorithm-specific reasons,
key-size/cofactor limits, digest continuation and reference-vector controls.

Evidence: [E049](QKB_INDEX.md#e049), [E052](QKB_INDEX.md#e052), [E048](QKB_INDEX.md#e048), [E188](QKB_INDEX.md#e188), [E005](QKB_INDEX.md#e005).

## CAP parsing and interpreter hardening — defended paths, acceptance gaps

Standard GPPro LOAD omits Descriptor; Directory sizes describe original CAP components.
Do not infer arbitrary field kinds or cross-CAP types from numeric values when metadata is unavailable.
Validate complete component/header/Export/import and ConstantPool bounds; runtime instruction starts, branches,
handler ranges and method limits need checked coordinates and non-overflowing signed arithmetic.
Uploaded code must not invoke VM-internal native opcodes. Parser success is not proof of field access/firewall safety.
CAPIN tasks begin with a reproducer; keep local hostile inputs separate from unchanged upstream fixtures.
Known guards and focused VPCD recovery do not close all hostile-CAP/physical/sanitizer acceptance.

At INSTALL, validate the dynamic package's complete transitive Import closure,
not only the applet package. A library can import an absent package without
using it in its Constant Pool; a direct-only check then incorrectly accepts
the applet. LOAD may precede dependency packages, so defer closure resolution
until INSTALL. An eight-slot visited worklist handles cycles within the fixed
dynamic package table. `missing-library-dependency` in the RMI VPCD suite is
the regression; expected failure is `6A88`, while valid staged loading and
install remain supported.

Evidence: [E037](QKB_INDEX.md#e037), [E039](QKB_INDEX.md#e039), [E301](QKB_INDEX.md#e301), [E403](QKB_INDEX.md#e403).

## CCID routing, responses and reset — preserve recovery

Route each APDU once; pending responses belong to the originating applet/manager.
A failed time extension/final IN transfer must retain the result or explicitly abort/reset and rearm endpoints.
Logical power-off is a reset boundary: clear both transient classes, transactions and native session state without Java deselect callbacks.
An unauthenticated stale SCP03 session must not divert ordinary applet APDUs; protected manager operations still require authentication.
The post-GPPro-disconnect AES prepare regression passes on hardware. Keep broader power-loss/transport gates open.

Evidence: [E308](QKB_INDEX.md#e308), [E309](QKB_INDEX.md#e309), [E351](QKB_INDEX.md#e351), [E424](QKB_INDEX.md#e424).

## Build, converter and profile provenance — viable controls

Use repository wrappers. Keep generated ROM/JNI/profile artifacts coherent and rebuild after source/test relocation;
cached CMake paths and a stale PC emulator can invalidate a test result.
Do not infer full-VM support from a smoke build or capability manifest. Check firmware/CAP flash-window fit.
JC305 int signatures require converter `-i`; large test methods can trigger converter internal errors—split test methods instead.
Standalone allocation linkage was repaired during testing-data migration; separate old review persistence fixtures need aligned dependencies/slots.
The historical Rust FS workaround omits the default allocator feature to avoid obsolete `lock_api` nightly `const_fn`;
use the repository workflow rather than invoking compiler binaries directly.

Evidence: [E349](QKB_INDEX.md#e349), [E124](QKB_INDEX.md#e124), [E071](QKB_INDEX.md#e071), [E340](QKB_INDEX.md#e340), [E341](QKB_INDEX.md#e341), [E395](QKB_INDEX.md#e395).

Shared PC ROM/JNI/profile outputs are written by both `tools/build` and
standalone `tools/build-pc`. Both wrappers now serialize through
`build/.tools-build.lock`; nested PC generation reuses the parent's open file
descriptor. Keep build entrypoints behind these wrappers. The concurrent
ARM/PC/RISC-V profile regression passed and checked each architecture's
support-selection, ROM-mask and JNI provenance against its generated profile.
It does not cover external writers that bypass the wrappers.

Evidence: `progress.md`, entry `RV-20261106-33`; logs
`artifacts/logs/concurrent-builds-20261112T035503Z/`.

## Startup and Windows diagnostics — implementation accepted, timings open

Normal firmware has zero console/DTR wait; bounded diagnostic startup capture is explicitly enabled.
Windows timing acceptance needs at least three cold reconnects on current and original images with serial clients closed;
record PnP, PC/SC and SELECT timings plus exact image/driver identity. See [workflow](../docs/usb-startup-diagnostics.md).
Windows PRESENT/MUTE and certutil card-type mapping are distinct from Linux APDU success; preserve exact traces before assigning cause.
CPLC OS release date is not a build timestamp: the current two equipment-ID fields encode a 64-bit source commit prefix
(see handoff/progress), superseding historical advice to rewrite release dates for each build.

Evidence: [E300](QKB_INDEX.md#e300), [E072](QKB_INDEX.md#e072), [E079](QKB_INDEX.md#e079), [E145](QKB_INDEX.md#e145).

## Acceptance evidence — version, backend and completion matter

Use unchanged supplied JCAlgTest **1.8.4 / jc305**, matching versioned client and `tools/fixture-path --verify`.
The supplied Java Card API validation suite is separate. Legacy 1.8.2 reports stay historical.
Record exact image/profile/hash, chip/reader identity, ordered commands, exit status, duration and cleanup result.
An API heartbeat, isolated probe, incomplete CSV or timed-out matrix is not full-suite acceptance.
Do not classify expected NO_SUCH_ALGORITHM as a failure or silently relabel wrong results as unsupported.
Old “3072” rows that actually request 4096 are fixture/version-specific evidence; do not edit supplied CAPs to hide mismatches.
Archive all measurements and corrections; current remaining work belongs in [TODO](../TODO.md), not duplicate campaign diaries here.

Evidence: [E092](QKB_INDEX.md#e092), [E276](QKB_INDEX.md#e276), [E277](QKB_INDEX.md#e277), [E278](QKB_INDEX.md#e278).

## GPPro DELETE timeout followed by PC/SC loss — unresolved observation (2026-11-12)

After `GPPro --list` succeeded on all three newly flashed cards, deleting the
JCAlgTest applet on chip `2BED26ED20556535` exceeded a 30-second timeout. The
board remained USB `2e8a:0009` but lost its USB serial descriptor; its bounded
serial monitor had no output. PC/SC enumeration then stopped listing readers,
even though the approved access helper still granted access to all three USB
nodes. The other boards were not subjected to deletion. This does not establish
that DELETE caused the board symptom or that PC/SC itself caused the timeout.

After rebuilding the current empty-card UF2 and flashing D92/8E, refreshing
the approved helper restored PC/SC publication for those two boards. Their
GPPro lists now show only the ISD and no APP/PKG entries. This does not
establish whether the earlier GPPro DELETE caused the service or third-board
symptom. `2BED26ED20556535` still enumerates as `2e8a:0009` without a USB
serial descriptor and has no PC/SC reader; its bounded monitor had no startup
output and CDC-1200 did not enter BOOTSEL. Do not retry its write until its
chip ID is verifiable again. No viable recovery for that board is confirmed.
See the 2026-11-12 08:27 UTC handoff and progress entries
`MAXCRYPTO-THREE-PHYSICAL-BOARDS-20261112` and
`HOST-01-FLASH-PATH-RECOVERY-EMPTY-CARD-20261112`.

## Physical Pico reader disappears from PC/SC after a successful reset APDU — unresolved

**Symptom:** On corrected D92 image `1837533b…dde62`, the 1.8.4 all-algorithms
client ran about 13 minutes and produced 51 `ALG_` rows, then returned
`SCARD_E_NOT_TRANSACTED` for APDU `B0 75 12 00 01 16 00`. The preceding
`INS_CARD_RESET` completed with `9000` after erasing three applet records. The
next APDU did reach the applet: serial logs show `APDU_PROCESS stage=end
sw=9000 len=29`. However, there is no subsequent `CCID_RESPONSE` record for
that command; the last response snapshot is still the preceding `B0E2` reset.
No `HEAP_GC_FS_READ_FAIL`, deletion skip, `FAULT`, `F205` or `NO_RESOURCE`
appears on this corrected image.

**Observed cause status:** `choupi_vm_process_applet()` returns after the
`APDU_PROCESS stage=end` log, then `src/ccid_device.cpp` calls `fs_sync()` and
queues the CCID response. The current trace cannot tell whether it stalls in
that sync, response submission or USB completion. At exit the USB nodes still
enumerated as runtime `2e8a:0009`, while `pcsc_scan -r` listed only VPCD; the
approved helper granted two nodes and `pcscd.service` remained active. This is
not enough evidence to blame PC/SC alone or to call the firmware panic-free at
the response boundary. `dmesg --ctime` was denied with `Operation not
permitted`, so kernel USB reset events could not be checked. The earlier 8E
run is a separate reproduced heap/body failure: it emitted
`HEAP_GC_FS_READ_FAIL` and later `*** PANIC *** Out of memory`.

**Response:** Preserve the result/serial logs. Add gated stage telemetry around
applet return, `fs_sync()` begin/end, CCID response submission and IN completion;
then rerun on an identified physical board after PC/SC access returns. Do not
retry by busy polling. The alias-target fix has separate positive physical
evidence, but neither this post-applet response path nor RESOURCE-01 acceptance
is closed.

Evidence: `progress.md`, entry `RESOURCE-01-D92-PCSC-LOSS-AFTER-RESET-20261112`;
`artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option1-alias-guard-aa44cb12/client.log`,
`serial.log`, and `results/Pico_2_CHOUPI_CCID_[JCVM_CCID]_(D92E743D5F640C0F)_00_00_ALGSUPPORT__3b_80_01_81.csv`.

## CCID final response blocked by stale time-extension state — recovery deployed; long-run validation pending

On physical 2BED (`2BED26ED20556535`), the pre-recovery JCAlgTest option-2
campaign reached xfr=2037 and stalled on reset APDU `B0E2000000`. Object
deletion completed and erased eight applet records; persistent `fs_sync()`
returned status 0 after 1,379,916 us, and the VM response was `9000`. The
CCID trace then reported `response_submit_return pending=1 zlp=0 in_busy=0
submit_errors=0`; stage remained 6 and the client watchdog timed out after 60
seconds. There was no `F205`, persistent-write failure, or `FAULT` before the
stall.

Given `retry_pending_ccid_transfer()`'s gates, this state indicates the final
response was withheld because `native_extension_in_flight` remained set even
though the IN endpoint was idle. A time-extension completion callback had not
cleared that state. This explains why the final response was never submitted;
it does not establish why TinyUSB omitted/delayed the callback.

Recovery now records when a time-extension IN transfer is accepted. When a
final response is pending and the endpoint is idle for at least 100 ms, the
driver clears stale extension state and submits the response. An active native
operation is not affected because this recovery requires a pending final
response. `./tools/test-ccid-progress-flow` passes 30/30 and ARM/RISC-V builds
pass with object deletion plus CCID diagnostics enabled. This image is flashed
on D92 and 2BED, both of which passed finite startup checks.

The new option-2 run on D92 uses the supplied 1.8.5 client with the unchanged
1.8.4/jc305 CAP. At the captured checkpoint it had reached xfr=496 with normal
responses; it has not reached the old failure point or logged
`CCID_EXTENSION_RECOVER`. Do not mark this recovery proven until the event is
observed and later APDUs complete. Preserve the logs and keep broader RESOURCE-01
acceptance open. Artifacts:
`artifacts/results/jcalgtest-1.8.5-compaction-fix-20261113/2BED26ED20556535/`
and
`artifacts/results/jcalgtest-1.8.5-ccid-recovery-20261113/D92E743D5F640C0F/`.

The previously requested PC/SC restart has been completed by the user and the
physical readers are presently enumerated. Refresh the approved helper only
between campaigns; running it during a SmartcardIO session can restart PC/SC
and tear down that active connection.

## Maintenance

Add or update the matching diagnosis instead of prepending repeated availability checks.
Use **symptom → confirmed cause or hypothesis → viable response → validation scope/limit → evidence**.
Keep exact commands only when safe under current AGENTS.md; a historical command is not authorization.
Put new experiment logs/hashes in progress and link them. Keep stable issue IDs in the catalog;
add new catalog entries without renumbering existing ones. Do not upgrade an unresolved hypothesis to a viable fix without evidence.

## BOOTSEL reset integration audit — 2026-11-12 (offline)

Confirmed firmware integration gap, not a proven new regression: `src/ccid_device.cpp`
configuration has three interfaces (CDC control/data and CCID), no
`TUD_RPI_RESET_DESCRIPTOR`, and its application-driver callback returns only CCID.
This descriptor layout dates to `dc76f431` (2026-10-11). `CMakeLists.txt`
removes the SDK descriptor source. SDK `pico/usb_reset.h` explicitly requires
adding the reset descriptor and registering `usb_reset_interface_driver` when
using custom TinyUSB. The plan promises this interface but implementation lacks it.

CDC fallback added in `971887e4` (2026-11-08) sets `usb_bootsel_requested`
in the line-coding ACK callback; only the outer loop in `src/choupi_pico2.cpp`
consumes it. Synchronous `choupi_ccid_task()` work can service USB via native
progress while feeding the watchdog without checking this flag. Thus a received
reset can remain pending until the APDU returns; 20-second recovery is not
guaranteed. An unserviced USB loop also prevents receiving the request.
Neither mechanism proves the exact D92 failure without existing run-specific
telemetry. VM passthrough and host CDC request delivery remain alternatives.

Implementation update — 2026-11-12: added the SDK vendor reset descriptor and
class driver as interface 3 while retaining CDC interfaces 0/1 and CCID
interface 2. CHOUPI's CDC-1200 callback now queues a request; main-loop USB
service and interpreter cooperative progress consume it after TinyUSB returns.
The SDK CDC reset callback is disabled to avoid a duplicate callback and direct
reset from inside TinyUSB. Offline evidence: reset/CCID unit tests 26/26,
identity-pinned flash-target-selection tests 10/10, and ARM/RISC-V wrapper
builds pass. The ARM image ends at `0x100d91b0`, leaving 159,312 bytes.

Physical limitation remains: after the user reported three devices in BOOTSEL,
elevated guest `lsusb -d 2e8a:` returned three runtime `2e8a:0009` devices and
no BOOTSEL `000f`. No reset or flash was attempted. Hardware confirmation of
vendor reset, CDC-1200 reset during idle and a long synchronous workload, and
request-to-re-enumeration latency remains open; no latency metric is measured.

## JCAlgTest option 2 PC/SC failures on 8E and D92 — 2026-11-12

**Symptom:** The unchanged JCAlgTest 1.8.4 `ALG_PERFORMANCE_STATIC` run on 8E
(`8E464F4E2BBC0A19`) completed 26 measurements, then failed at DES ECB NOPAD
`Cipher_init()` with `SCARD_F_COMM_ERROR`. Earlier in the same turn, the
authorized USB/PCSC helper was run while this live campaign was active. The
helper restarts `pcscd`; this is a confirmed way to tear down a live
SmartcardIO connection and is the likely cause of this first failure.

A failover attempt on D92 (`D92E743D5F640C0F`) completed 32 measurements before
DES ECB PKCS5 `Cipher_init()` returned `SCARD_E_NOT_TRANSACTED`. The client
skipped that case, then saw `SCARD_E_READER_UNAVAILABLE` twice on DES3 2-key CBC
NOPAD `Cipher_doFinal()`. No reader-helper refresh was run during this D92
attempt. After the client stopped at its retry prompt, `pcsc_scan -r` still
listed D92. D92 serial telemetry showed successful `9000` APDU responses and
`TEST:SUMMARY pass=1 fail=0`; neither serial log contained `FAULT`, `6F00`, or
an `OBJECT_DELETION` event. This does not establish a firmware fault or a
completed GC test. The campaign is incomplete.

**Viable response:** Never restart the PC/SC helper while any SmartcardIO
campaign is active. Preserve its logs and, after the active client is stopped,
check `pcsc_scan -r` before using another identified reader. If PC/SC remains
available after a transaction error, record that separately from board loss.
The exact cause of the D92 errors remains unknown. The user reported all three
boards in BOOTSEL, but the last guest `lsusb -d 2e8a:` enumeration showed three
runtime `2e8a:0009` devices and no BOOTSEL `000f`; PC/SC listed only D92. This
host/device-mode discrepancy must be resolved before flashing.

Evidence: `artifacts/results/jcalgtest-1.8.4-resource01-20261112/8E464F4E2BBC0A19/option2-gc-nodebug/`
and `artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option2-gc-nodebug/`.

Follow-up, 2026-11-12: On 4CDA (`4CDA5BC1B136E46A`), a watchdog-bounded
option-2 repro stalled for 45 seconds on SHA-384 `MessageDigest_update()`
measurement APDU `B041000016`. The VM logged its `9000` APDU response, but CCID
telemetry remained at stage 6 (response submission) rather than stage 7
(successful IN completion); the SmartcardIO client emitted no response before
the watchdog terminated it. This points to the CCID response-delivery boundary,
not specifically RSA-3072 or persistent-storage compaction. Afterward the
physical readers disappeared from `pcsc_scan -r`; the helper still granted
access to three USB nodes. D92 and 2BED were then identity-pinned through
`tools/cycle choupi`; both ARM/RISC-V builds, firmware flashes and finite
runtime checks passed, but `pcsc_scan` still listed only Virtual PCD readers.
An unprivileged PC/SC service restart was denied. A human service restart is
requested in `.agent/EXT_REQUESTS.md`. This is a reproduction of missing host
delivery after a successful VM result, not a confirmed cause or a complete
option-2 run.

The prior D92 storage failure is separate evidence: at `files=2134`,
`bytes=98292/98304`, compaction retained a record beginning at offset `91502`
while its output prefix had reached `91510`, then returned `FS_STATUS_IO_UNKNOWN`
and the applet returned `F205`. Diagnostic logging now also records the
previous retained record's fields, but that richer record has not yet been
captured during a fresh compaction failure. No RSA array change is justified.

Offline root cause confirmed, 2026-11-12 22:49 UTC: compaction's in-place
open-addressing rehash walked mapping paths from the lowest slot, which can
start in the middle of a path. That leaves a stale duplicate in a slot later
overwritten by its predecessor and loses a live record. A deterministic
96 KiB stress case with 2,400 64-byte compressed tags and 4-byte values
reproduced the loss at its first compaction (`id=856`, then
`FS_STATUS_NO_SUCH_TAG`). The fix moves path components from zero-incoming-edge
heads before processing remaining cycles. The new test passes 32 delete/replace
cycles, validates every live value after each compaction, and verifies snapshot
restore. This explains the class of corruption behind the earlier overlap
rejection, though the exact old D92 offset pair has not been reproduced after
the fix. ARM and RISC-V `max_crypto` builds pass. A fixed-image hardware cycle
is still pending: the approved elevated helper call failed with the
`no new privileges` restriction; see `.agent/EXT_REQUESTS.md`.

## 2026-11-12 External no-debug run: reset failures precede RSA-3072 disconnect

Investigation ID: RESOURCE-01-EXTERNAL-RESET-PREFIX-20261112.
User-reported evidence for `pico2-jcvm-max_crypto-object-deletion-no-debug-empty-card-580f3f40.uf2`; full new external log and serial telemetry are not available locally.

The first reported failure is AES-128 ECB ISO9797 M1 `Cipher_doFinal()` preparation, not RSA:
`B0 31 00 00 16 00 11 00 19 FF FF 00 0F 00 80 00 07 01 00 FF FF 00 02 00 32 00 01 00`
returned `6F00` in 1482 ms. The client then printed “Free unused card objects...”, “Reset applet didn't work, speed of algorithm couldn't be mesured”, and `CANT_BE_MEASURED`. User reports many repetitions before the final RSA-private-3072 PKCS1 preparation transport failure, and reset calls after every command to be measured.

The supplied client excerpt first sends the preparation APDU; on non-9000 it invokes `resetApplet(cla, resetIns)`. Only a true reset result permits retry of the preparation APDU. A false result prints the reset-failure message and raises CANT_BE_MEASURED. Thus this message identifies a failed recovery attempt, not proof of a slow RSA measurement. The reported applet INS_CARD_RESET branch invokes `JCSystem.requestObjectDeletion()`.

Important attribution limits: the shown `6F00` belongs to B0 31 preparation. No reset-command response/status or exception is supplied, so an exception specifically inside requestObjectDeletion is a hypothesis. In current source, the enabled native request merely sets a noexcept boolean and records its context; actual collection runs after APDU execution in `choupi/src/main.cpp`. With deletion disabled, the native API explicitly throws SystemException. Verify the tested binary configuration and distinguish request entry, post-APDU collection, reset response, and next prepare response. Retained deletion requests may be serviced again; repeated calls must be tested with live roots and failing preparation paths.

Next: locate the first failing reset in the complete trace, retain its exact APDU/status/elapsed time and preceding successful operation, correlate serial exception and OBJECT_DELETION markers, and reproduce an unchanged upstream 1.8.4 AES prepare/reset sequence repeatedly before attempting the RSA prefix. Measure USB-progress gaps across post-APDU collection as well as native RSA. RSA time-extension starvation remains possible but cannot explain away the earlier AES/reset failures. No robust fix established; no hardware accessed.

Physical follow-up, 2026-11-12 19:11 UTC: reader access returned for 8E
(`8E464F4E2BBC0A19`); two other runtime USB Picos were not published by PC/SC.
On 8E, SELECT JCAlgTest returned `9000` in 224 ms, then the exact client
`INS_CARD_RESET` (`B0E2000000`) returned `9000` in 1,560 ms, and the reported
RSA-3072 private PKCS1 `Cipher_doFinal()` preparation APDU (`B031...`) returned
`AA9000` in 1,758 ms. The full sequence passed; a subsequent bounded GPPro
`--list` showed ISD, JCAlgTest applet and package. A saved finite serial monitor
reported `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`.

This is evidence against a deterministic 3072-bit array-capacity defect: the
native key builder computes `(bits + 7) / 8` in `uint16_t`, yielding a valid
384-byte modulus/private-component capacity and 192-byte half-size components
for 3072-bit RSA; the exact client preparation succeeded on-card. It does not
exclude a state-dependent memory/GC bug or explain the external PC/SC loss.
The physical image has heap-GC diagnostics enabled and is not the external
no-debug image, so timing is diagnostic only. Next reproduce the preceding
failed AES prepare/reset and run repeated exact reset/prepare cycles or the
unchanged 1.8.4 client while correlating CCID response progress. No source fix
is established by this one successful sequence.

Repeated physical follow-up, 2026-11-12 19:22 UTC: 8E's diagnostic firmware
(`PICO2_DEBUG_HEAP_GC=1`) produced large per-record traces during reset. One
bounded repetition took 11.2 s for SELECT and 14.5 s for reset, then the host
probe timed out before a `B031` command appeared in serial telemetry. This is
inconclusive: instrumentation and output volume differ from the user's
no-debug firmware, and the host timeout expired while waiting for the next
APDU. Using D92 with no-debug firmware removed that confound. Four consecutive
reset/prepare cycles all passed: resets `9000` in 1.468–1.522 s; RSA-3072
prepare `AA9000` in 1.617–1.691 s. The exact preparation remained stable after
repeated deletion requests. This further argues against insufficient
3072-bit component arrays, but does not reproduce the earlier AES 6F00 or
Windows PC/SC disconnect. Continue with the unchanged upstream client on a
no-debug device and capture the first AES prepare/reset failure and transport
status. Logs: `artifacts/results/rsa3072-physical-20261112/8E-heap-debug/` and
`artifacts/results/rsa3072-physical-20261112/D92-no-debug/`.

AES follow-up, 2026-11-12 19:29 UTC: on no-debug D92, the exact reported
AES-128 ECB ISO9797 M1 `B031` preparation, preceded by `B0E2`, returned
`AA9000` once and then across four consecutive reset/prepare cycles. Repeated
reset responses were `9000` in 1.477–1.530 s; AES preparations returned
`AA9000` in 1.713–1.828 s. Serial remained healthy. This does not reproduce
the external `6F00` even for its exact AES config, and reduces the likelihood
that a basic AES or 3072-bit allocation failure is deterministic. The current
probe omits the measurement `B043` APDU and broad preceding suite history; do
not claim full performance acceptance. Next diagnostic should use the
unchanged 1.8.4 client over a longer no-debug campaign, retain the first
failure's APDU/reset result and correlate card serial and PC/SC events. Logs:
`artifacts/results/rsa3072-physical-20261112/D92-no-debug/apdu-aes-reset-prepare-20261112T1924Z.log`,
`.../apdu-aes-repeated-reset-prepare-20261112T1925Z.log` and corresponding
serial logs.

Full-client follow-up, 2026-11-12 19:49 UTC: the unchanged 1.8.4/jc305 client
ran `-op ALG_PERFORMANCE_STATIC -fresh` on D92 no-debug firmware whose CPLC
pre-personalization ID was `580F3F40`, matching the tested build. It completed
SHA, MD5, RIPEMD160, SHA-256, SHA-384 and SHA-512 tests, then failed during
`ALG_SHA_224 MessageDigest_reset()` at a `B041` APDU with
`SCARD_E_NOT_TRANSACTED`. No reader-helper restart occurred during the client.
Firmware serial state showed `rx_events=1774`, `xfr=207`, `stage=6`, last APDU
`B041000016`, and a recorded response `sw=9000`; no `CHOUPI:CCID_ERRORS`,
`FAULT`, or failed runtime summary appeared. A post-failure serial snapshot
still passed runtime health with the same stage-6 state. USB still enumerated
D92 as `2e8a:0009` and its CDC serial remained available, but PC/SC no longer
published a physical reader. After the client had exited, the approved helper
refreshed access; a line-buffered scan listed only the two Virtual PCD readers.

In `src/ccid_device.cpp`, `CCID_RESPONSE` is recorded before the final bulk-IN
transfer is known to have completed; the CCID diagnostic stage changes to 7
in the successful IN completion callback. Thus the stuck stage 6 plus client
transaction failure points to the final CCID response transfer not completing
(or its callback not being serviced), rather than to RSA buffer capacity. This
is a strong localization, not a proven exact electrical/USB cause: current
telemetry does not expose pending-response state or distinguish submission
from host ACK. Add focused response-submit/IN-completion diagnostics, then
reproduce on no-debug firmware. Full option-2 acceptance failed; it is not a
pass. Logs:
`artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option2-rsa-reset-followup/`.
## RESOURCE-01 follow-up — persistent-storage exhaustion during option 2

On physical D92 (`D92E743D5F640C0F`), the unchanged JCAlgTest 1.8.4/jc305
`ALG_PERFORMANCE_STATIC` follow-up passed the earlier SHA-224 reset point but
later failed at AES-192 setup after the persistent overlay reached 2,134
records and 98,292/98,304 bytes. The attempted persistent write returned
filesystem status 1 (`OutOfFlash`), mapped by the VM to
`SystemException.NO_RESOURCE` / `F205`; later client cases reported
`CANT_BE_MEASURED`. Object-deletion passes did run and eventually erased
applet records, but the last write-failure path produced no successful
`FS_COMPACT` telemetry. Firmware CDC/runtime health remained alive.

This confirms storage exhaustion in this run, not an undersized RSA-3072
component array. It does not establish the cause of the separate Windows
`SCARD_E_NOT_TRANSACTED` at RSA-3072. Exact RSA-3072 reset/prepare passed four
times before storage filled, and this full run failed earlier at AES-192.
Next inspect why `compact_deleted_overlay_records()` rejected compaction and
whether current applet persistent roots keep unexpectedly many algorithm
objects alive. Preserve the campaign artifacts and do not claim option-2
acceptance. Artifacts:
`artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option2-ccid-debug-followup/`.

## 2026-11-13 Windows reset campaign: distinguish transport from persistent resource failure

Evidence: `artifacts/results/windows-maxcrypto-20261112-with-reset/ANALYSIS.md`. First AES measurement transport loss precedes memory errors. After RSA-1984 reconnect F205 precedes 6F00; replug does not clear lasting failure. F205 can mean failed overlay compaction, not only true capacity exhaustion. Reported 580f3f40 image predates independently reproduced rehash fix 92c91943 and CCID recovery 49a390d5. Existing viable fix for reproduced rehash mapping loss: path-head-first rehash (unit validation previously recorded); **not proven to resolve this Windows run**. Eight-record/APDU backlog is still a hypothesis without deletion telemetry. No implementation in this step.

## 2026-11-13: CCID response retry remains unresolved; D92 compaction rejects tag reconstruction

On physical 2BED (`2BED26ED20556535`), the recovery-build option-2 campaign
stalled at xfr=1156 on `B043000016`: the VM returned `9000`,
`fs_sync()` took 1,023,217 us, and the CCID response remained pending with the
IN endpoint reported idle and no submit errors. The client watchdog expired
after 60 seconds; no `CCID_EXTENSION_RECOVER` was logged. Thus the deployed
100 ms stale-extension recovery is not validated and failed to complete this
second observed pending response. A new debug-only `CCID_RESPONSE_WAIT`
trace reports extension flag, endpoint busy state, native operation activity,
age, and retry failures; it has not yet been flashed or observed. The trace was
extended to report `CCID_RESPONSE_IN_FLIGHT` once per second after TinyUSB
accepts the final response but before its successful IN completion callback.
This distinguishes a response stuck in the endpoint from one blocked before
submission; stage 6 alone cannot distinguish these paths. Latest diagnostic
image is `build/arm/pico2_choupi.uf2`, SHA-256
`6147b04aea8abb3e90ac112347bb4fc331c54eff3de7bf10aea21955d92d55a7`; it has
not been flashed because the identified devices did not enter BOOTSEL.

### D92 tag reconstruction diagnostics

Added debug-only `FS_TAG_RECON_FAIL` logging inside the compaction tag
reconstructor. It reports the failing slot, flags, tag length, serialized
offset/header, parent slot and length, depth, root length, and precise local
failure reason; recursive failures are reported along the chain. This is meant
to distinguish stale parent indices from malformed compressed/plain record
metadata. `./tools/test-rp2350-fs-overlay` still passes, including full-index
compaction with a parent-encoded child. The new trace is build-validated only;
no physical capture identifies D92's failing record yet. Current diagnostic
image: `build/arm/pico2_choupi.uf2`, SHA-256
`898e968b8f2b03cfb6533b8c19d093451f1d957d49114daa96bda6b75b4ac7a0`.

Added `filesystem_parent_encoded_records_survive_repeated_compaction`: a
2,400-record near-capacity test using 29-byte legacy parent-encoded tags and
37-byte values, deleting/replacing 100 children twice, then snapshotting,
restarting, and verifying all live records. It passes, so this synthetic
pattern does not reproduce D92's rejection; preserve the physical failure and
wait for `FS_TAG_RECON_FAIL` output before changing compaction semantics.

Follow-up stress also tombstones all ten parent tags while 2,199 children
remain live, forces capacity compaction, then recreates the parents and runs a
second compaction. It still passes. The D92 snapshot cannot be v3 at the
reported 98,273 bytes because v3 validation limits `bytes_used` to 77,824;
that failure was reached in v4 state, so v3 migration alone is not a likely
explanation.

The separate D92 run hit `FS_COMPACT_REJECT status=131
reason=tag_reconstruction` repeatedly with 2,468 live overlay entries and
98,273/98,304 bytes. Persistent writes then returned `F205` /
`SystemException_NO_RESOURCE`. This is distinct from the 2BED response stall.
No failed slot or parent-chain detail is currently logged, so root cause is
unresolved. Preserve both physical logs. Do not conflate `F205` with the
transport failure or claim option-2 acceptance. Logs are under
`artifacts/results/jcalgtest-1.8.5-ccid-recovery-20261113/`.

## 2026-11-13: Validate stored overlay record bounds before tag reconstruction

`reconstruct_overlay_tag()` previously validated that the record offset was
inside `overlay_bytes_used`, but trusted the serialized key length and then
read/copy key bytes without checking the complete key and value span. A
malformed or inconsistent serialized header could therefore read beyond the
used overlay region during compaction. The function now verifies that
`key_length + file.size` fits in the remaining bytes before accessing the
payload. The regression corrupts a stored header to claim a 127-byte key and
confirms compaction returns `FS_STATUS_IO_UNKNOWN` without reading beyond the
record. `./tools/test-rp2350-fs-overlay` passes; ARM/RISC-V wrappers build.

This is defensive hardening, not a confirmed root cause for the physical D92
`tag_reconstruction` rejection: the exact slot/header is still unknown and the
near-capacity parent-encoded regression does not reproduce it. If a future
physical run rejects compaction, use `FS_TAG_RECON_FAIL` to capture the actual
record and verify whether this guard is implicated. Keep the CCID transport
stall as a separate failure class. Follow-up host coverage now also tests a
nested child -> parent -> root chain with both ancestors tombstoned through
compaction, re-created after remapping, and verified after snapshot restore;
the full filesystem suite passes. This still does not reproduce the D92 failure.
