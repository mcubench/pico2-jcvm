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

## Targeted flashing and BOOTSEL — scoped viable fallback

Runtime `2e8a:0009` is not BOOTSEL `2e8a:000f`.
Use repository wrappers and a mandatory chip ID; build both architectures before flashing.
The targeted full-VM path is `tools/cycle choupi --profile <profile> --chipid <ID>`;
`cycle arm`/RISC-V smoke output is not full Java Card acceptance.
CDC 1200-baud reset has recovered boards when picotool reset lookup failed, but it is not reliable on every board.
Close serial monitors before resetting; failure after doing so disproves monitor interference as a universal explanation.
If bounded reset fails, change identified board or request host passthrough recovery instead of repeatedly retrying.
Flashing preserves installed CAP storage unless an explicitly prepared empty-card image is used.
A flash success plus serial heartbeat proves runtime startup only.

Evidence: [E429](QKB_INDEX.md#e429), [E093](QKB_INDEX.md#e093), [E106](QKB_INDEX.md#e106), [E065](QKB_INDEX.md#e065).

## VPCD setup, stale state and process lifecycle — viable controls

Rebuild the selected PC profile after native changes; identify emulator/ROM/fixture hashes.
Use a freshly generated empty image for clean-card tests: stale images can retain apps, proxies or incompatible state.
Discover readers with bounded, logged retries and pin one reader through install, probe and cleanup.
A socket-connect timeout may hide an INSTALL failure; preserve emulator stdout and inspect pre-connect stages.
Terminate only the known test process group when a wrapper times out; a detached emulator can survive and consume CPU.
Some historical emulator errors exited zero: check VM markers and actual results as well as exit status
until RV-20261106-32 acceptance is complete.

Evidence: [E360](QKB_INDEX.md#e360), [E089](QKB_INDEX.md#e089), [E288](QKB_INDEX.md#e288), [E297](QKB_INDEX.md#e297), [E342](QKB_INDEX.md#e342).

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

## Maintenance

Add or update the matching diagnosis instead of prepending repeated availability checks.
Use **symptom → confirmed cause or hypothesis → viable response → validation scope/limit → evidence**.
Keep exact commands only when safe under current AGENTS.md; a historical command is not authorization.
Put new experiment logs/hashes in progress and link them. Keep stable issue IDs in the catalog;
add new catalog entries without renumbering existing ones. Do not upgrade an unresolved hypothesis to a viable fix without evidence.
