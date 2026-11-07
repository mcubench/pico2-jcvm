# Firmware OS and Java Card VM — Architecture and Layering Review

Updated **2026-11-04** from baseline `fb48427`, subsequent diagnostic
`590d70b`, and the current checkout.
This replaces the 2026-10-24 architectural snapshot. It distinguishes source
implementation, recorded validation, and remaining refactoring work. Existing
uncommitted changes are not assumed to be finished or deployed. This update
performed source/document inspection only; it did not build or access hardware.

The Pico executes a native C++ Java Card VM interpreting CAP bytecode. Host
Java compilation is part of ROM-mask generation; `.java` and `.class` programs
do not run directly on the Pico. API behavior and cryptographic algorithms
belong in native C++ dispatch. Upstream applet CAPs exercise that behavior.

## 1. What changed since the previous analysis

Several earlier findings have been substantially addressed:

- Filesystem implementation moved from board startup to
  [`pico_filesystem.cpp`](../src/pico_filesystem.cpp). Lifecycle, filesystem,
  diagnostics, and CAP initialization now have dedicated interface headers.
- Key, Cipher, Signature, and MessageDigest behavior moved into native C++.
  Generated method bindings and ROM metadata replace the previous executable
  crypto provider classes. Some interpreted provider glue remains elsewhere.
- CAP lifecycle recovery now reconciles package slots, enabled-package bits,
  applet roots, and published instances. INSTALL preflights duplicate AIDs and
  registry capacity. Persistent arrays have independent identities; transaction
  rollback also invalidates cached static references.
- Host VPCD storage supports multiple packages and instances. Retained applet
  contexts, owner-aware references, SIO proxies, exception propagation, and
  SensitiveResult provenance make the old single-context description obsolete.
- Cooperative USB servicing and an immutable execution budget replace the old
  assumption that long interpreter/native operations always starve transport.
- CCID framing, routing, pending-response retries, and VPCD selection have
  focused state-machine helpers and regressions. They are not yet one shared
  production card-session implementation.
- Uploaded CAPs cannot execute VM-internal native opcodes. Method header/body,
  operand-fetch, and taken-branch checks have improved execution containment.
  This is not a complete hostile-CAP verifier.

The main remaining problem is no longer simply that everything resides in
board startup. Ownership is better defined, but policy still crosses platform,
transport, VM, persistence, and native API boundaries.

## 2. Layer responsibilities and current owners

“OS” means different things in this tree. The historical Rust project under
`choupi/os` implements host/STM32 facilities. The active Pico has native OS
services implemented by repository C++ plus the Pico SDK, without a separate
RTOS kernel. The Java Card operating environment additionally includes the
JCRE, card manager, API services, and interpreter.

| Layer | Responsibility | Current implementation and boundary |
| --- | --- | --- |
| Hardware | Native CPU execution, SRAM, external flash access, USB peripheral, clocks/timers, watchdog, and hardware facilities such as SHA-256. | RP2350/Pico 2. Hardware does not interpret AIDs, objects, transactions, or CAP trust. Applet isolation is not automatically an MPU/TrustZone domain. |
| Board support / HAL | Startup, bounded USB event processing, flash erase/program safety, monotonic time, entropy access, reset/watchdog registers, and device indication. | Pico SDK/TinyUSB, `choupi_pico2.cpp`, `usb_task_budget.hpp`, `reader_led.*`, and SDK calls inside storage/crypto. There is still no complete independent HAL facade. |
| Native OS services | Record storage, persistence/recovery mechanisms, resource accounting, execution budgets, event-loop policy, and diagnostics. | `pico_filesystem.cpp`, `cap_store.cpp`, `execution_budget.*`, and host Rust filesystem adapters. Several services still know VM/package policy. |
| Card runtime / JCRE / card manager | GP authorization and lifecycle publication; selection/reset/deselection; applet ownership; context transitions, APDU state, and Java transaction semantics. | `ccid_device.cpp`, `cap_load.cpp`, `gp_lifecycle_recovery.*`, `main.cpp`, `context.*`, and host `vpcd_session.*`. This layer is distributed across transports and VM glue. |
| JCVM engine | CAP resolution, frames/stacks, checked references, bytecode execution, exceptions, heap/GC mechanics, and runtime validation. | `interpretor.cpp`, `jc_bytecodes/`, `jc_cap/`, `jc_handlers/`, `heap.*`, and `jcvm_types/`. It should not own USB endpoint policy or physical flash commands. |
| Native Java Card APIs / crypto | API state machines, validation, exception reasons, key lifetime, checked VM marshalling, and cryptographic backend calls. | `jni.cpp`, `native_key.cpp`, `native_cipher.cpp`, `native_signature.cpp`, `native_message_digest.cpp`, and native RSA/EC/hash/block-cipher modules. Backend/platform separation remains incomplete. |
| Installed applets | Application decisions and data accessed through supported Java Card APIs. | Dynamically loaded upstream CAPs. They must not select raw native-call indices, access platform pointers, or treat another owner's compact handle as their own. |

These are responsibility boundaries, not a strict call stack. JCRE enters the
interpreter for lifecycle methods; native APIs need checked access to VM objects;
persistence serializes VM objects through record services. Those dependencies
are legitimate when ownership, lifetime, failure, and durability are explicit.
An `extern "C"` call on Pico remains an ordinary native call, not proof of a
syscall, privilege change, or memory-protection boundary.

## 3. Runtime composition and important paths

### Boot and build representation

The full `pico2_choupi` firmware remains **ARM-only** in
[`CMakeLists.txt`](../CMakeLists.txt). The RISC-V build compiles the VM objects
and a smoke image; it does not establish complete CCID/runtime parity. The Pico
SDK supplies startup, linker integration, C runtime, and USB stdio. There is no
repository-defined RP2350 reset handler or linked legacy Rust/STM32 OS kernel.

`choupi/src/main.cpp:main()` enters `src/choupi_pico2.cpp:main_arm()`. Board
startup initializes USB, calls `fs_init()`, enables the watchdog, invokes the
generated startup entry, then polls USB/CCID and emits diagnostics. Filesystem
initialization is now implemented in `pico_filesystem.cpp`.

Pico still embeds `build/choupi-pc/flash` and copies the corresponding generated
JNI header. The PC generation branch of
[`choupi/CMakeLists.txt`](../choupi/CMakeLists.txt) selects the empty
`Starter.runVPCD()` entry because card management is native. The historical
STM32 branch selects `Starter.run()`, which constructs the Java GP registry.
Do not infer the Pico startup policy from that legacy entry.

Host compilation/conversion produces JCA; the ROM-mask tooling constructs CAP
components and native-call wrappers. The generated
`javacard-native-methods.txt` selects API methods for native binding, while
[`extract-rom-metadata.py`](../tools/extract-rom-metadata.py) emits class,
export, and field metadata consumed by C++. Generated `flash`, `jni.hpp`, and
metadata headers must agree; they are build artifacts, not independent edits.

### APDU, transport, and card policy

```text
RP2350 USB / TinyUSB
  → bounded event processing → CCID framing / queued message
  → CCID route + SCP03 + CapLoadSession
  → JCRE selection / retained Context → interpreter → native APIs

Host socket / VPCD framing
  → vpcd::Session(CardCallbacks) → host card-manager callbacks
  → the same VM lifecycle / interpreter / native APIs

Persistent VM objects → FlashMemory_Handler → filesystem ABI
  → Pico overlay / snapshots / SDK flash
  or host Rust filesystem / emulated image + CAP registry sidecar
```

USB transfer callbacks assemble/queue work. Only the guarded main CCID task
dispatches commands into card management or the VM. Native/interpreter progress
may service USB but must not dispatch another APDU recursively.

[`ccid_apdu_route.hpp`](../src/ccid_apdu_route.hpp) classifies default/explicit
ISD SELECT, public/protected GP commands, applet commands, inactive slots, and
invalid slots. Failed authentication returns before applet dispatch. CCID still
owns secure-channel state, registry pagination, lifecycle routing, and selection
policy; sharing a classifier does not complete transport independence.

VPCD's callback contract now distinguishes restored, missing, and failed
restoration. Its session tracks the selected AID so same-applet reselection is
not confused with switching applets. Missing AIDs map to `6A82`; restoration
failures map to `6F00`. Production-runtime acceptance remains distinct from the
focused session regression.

### Native API dispatch example

A CAP call to `Cipher.getInstance`/`init`/`update`/`doFinal` resolves through API
exports and trusted ROM native bindings into
[`native_cipher.cpp`](../choupi/src/native_cipher.cpp). That code validates VM
objects/ranges, manages algorithm/mode/buffering/padding/IV state, and calls
native AES/DES/CTR or RSA primitives. `native_key.cpp` manages the native key
surface; `native_signature.cpp` and `native_message_digest.cpp` similarly own
those API families. Mbed TLS and Pico facilities implement backend operations.

This supersedes the earlier `SymmetricCipherProvider`/`RSACipherProvider`
architecture. The support generator actively removes their generated outputs,
as well as generated Signature and digest providers. It still emits
`RandomDataProvider`, `SecureRandomData`, and PIN-related provider glue.
`SecureRandomData` contains interpreted range/length checks and method behavior
around a native random bridge. Therefore the major crypto migration is real,
but native-only API ownership is not complete across the entire framework.

The correct follow-up is to migrate remaining behavior into native dispatch,
retaining compatible API metadata and upstream CAP inputs. Reintroducing Java
provider algorithms would reverse the required direction.

## 4. Memory, persistence, and lifecycle ownership

### References, contexts, and transient state

The compact `jref_t` encoding remains a handle, not a hardware address or a
complete cross-context identity. Context/heap ownership and foreign-reference
provenance must accompany it. A selected context and a bounded inactive-context
table preserve applet state across deselection. Package-related applet instances
can retain distinct owner/lifecycle state without treating equal compact handles
as the same object.

[`vm_applet_context.hpp`](../src/vm_applet_context.hpp) exposes lookup/restore
and Shareable lookup returning a client-heap proxy. Supported SIO invocation
switches context and checks owner/proxy state; deletion/reinstallation must
invalidate stale proxies. Recent fixes preserve server exception identity/reason
and VM-wide SensitiveResult object provenance. Applet arrays reject unsafe
foreign-reference retention. These are substantial software-firewall features,
not evidence of unrestricted multi-applet semantics or hardware isolation.

Reset now clears both transient event classes. Transient-array restoration
preserves logical shape while restoring zero contents; restoring references
must not recreate stale operational data. Cipher key copies and Signature
workspaces use lifecycle-cleared transient storage. Native operational scalar
fields for Cipher, Signature, and MessageDigest live in lazy nonpersistent
`JC_Instance` session sidecars and are discarded on reset. Persistent algorithm
configuration and permitted key/object state remain separate. Native scratch
buffers still need a consistent zeroization/ownership discipline.

The VM heap/GC resolves object reachability and owns native-state lifetime;
the JCRE decides reset/deselect/delete events. Native APIs must not bypass
those owners. General logical-channel support is still excluded by the declared
profile, despite supported retained-context and cross-AID SIO flows.

### Physical storage layout

[`flash_layout.hpp`](../src/flash_layout.hpp) remains the shared address authority.
Offsets below are relative to the configured 2 MiB flash:

| Region | Offset range | Role |
| --- | --- | --- |
| Firmware/ROM-mask reservation | `0x000000–0x100000` | Native image and embedded trusted packages. |
| Eight CAP slots | `0x100000–0x188000` | Each slot has metadata plus up to the supported CAP code length. |
| Filesystem snapshot 0 | `0x188000–0x1B0000` | Copy-on-write snapshot. |
| CAP metadata mirrors | `0x1B0000–0x1B8000` | Eight independently updated header mirror sectors. |
| Preserved legacy CAP2 extent | `0x1C0000–0x1C8000` | Preservation does not activate or convert that old package. |
| Filesystem snapshot 1 | `0x1C8000–0x1F0000` | Alternate snapshot; other gaps remain reserved/unassigned here. |

CAP headers use generation/CRC-protected primary/mirror metadata. Filesystem
snapshots use generation/CRC recovery and preserve the prior committed snapshot
while writing the next. Physical flash/XIP coordination belongs to the platform
backend. Record tags, Java object identities, and lifecycle publication do not
belong in a hardware driver.

Persistent arrays now have stable identities independent of the field currently
referencing them; replacing a field must not overwrite an array still retained
through another alias. Static-reference caches track transaction rollback.
Transactional namespace erasure snapshots affected records before DELETE.

### Transactions and lifecycle publication

[`FlashMemory_Handler`](../choupi/src/jc_handlers/flashmemory.cpp) owns VM
serialization, before-images, and applet versus internal transaction state.
Maximum reported commit capacity is now `0x7FFF` bytes with tracked remaining
capacity, replacing the old zero-capacity placeholder. Lifecycle boundaries
abort unfinished applet transactions; internal installation transactions remain
a distinct operation.

Native non-atomic Util calls prevalidate all ranges before writing. Their
persistent array mutation path bypasses transaction capacity/snapshot accounting
and adjusts an existing before-image so abort retains non-atomic bytes while
rolling back atomic changes. This describes tested abort semantics; it does
not prove non-atomic writes survive power loss during a deferred transaction.

[`gp_lifecycle_recovery.*`](../src/gp_lifecycle_recovery.hpp) now reconciles
CAP storage with enabled bits and persistent applet roots. It removes disabled
orphan CAP packages, clears enabled state with no package, and removes registry
instances whose roots are absent. INSTALL still uses its committed recovery
journal before publishing an instance. Recovery is invoked before relevant
validation, restore, and lifecycle operations. Duplicate/full-registry checks
now precede applet INSTALL execution.

The earlier claim that cross-store recovery was simply absent is obsolete.
Nevertheless, recovery policy still spans loader, lifecycle helper, VM
persistence, CAP store, and filesystem. `fs_sync()` on Pico still returns success
without flushing while the VM transaction is active; commit disables that
flag before syncing. This documented reverse dependency remains architectural
debt, not a newly demonstrated data-loss incident.

### Host storage parity and its limit

The PC backend now maintains package-indexed CAP data, multiple instances, and a
registry sidecar, rather than replacing the last loaded package. This supports
multi-package/SIO tests that the earlier emulator could not represent.

Its durability differs from Pico: host `fs_sync()` is a no-op over the process's
emulated filesystem, while `--save` checkpoints flash and a `.cap-registry`
sidecar on normal save paths. The image rewrite and sidecar rename do not form
an atomic, explicitly fsynced pair. Abrupt termination may lose changes or leave
mismatched checkpoint files. Recovery detects/reconciles supported states, but
host process-restart tests do not establish physical power-cut durability.
The detailed contract is [FILESYSTEM_ABI.md](FILESYSTEM_ABI.md).

## 5. Execution budgets, progress, and uploaded-code trust

### Cooperative execution without recursive APDU dispatch

[`ExecutionBudget`](../choupi/src/execution_budget.hpp) captures one immutable
start and limit for the outermost Pico interpreter scope. Nested interpreter
and SIO entries share it. Current limits are **600 seconds for applet work**
and **5 seconds for INSTALL**. Native progress updates a separate service
timestamp; it cannot renew the hard deadline. Fake-clock tests exercise expiry,
nesting, and wraparound; the broad host path does not enable the Pico budget.

The interpreter invokes cooperative progress at 4,096-bytecode intervals.
Native asymmetric RNG/progress callbacks service the watchdog and transport
while checking expiry. USB service processes at most eight queued events per
pass. That bounds callback count, not the maximum duration of every callback.
CCID progress/task guards prevent recursive VM dispatch; time-extension replies
keep the reader informed while work remains synchronous.

Final responses are now retained and retried nonblocking after an in-flight
time extension, with 1 ms retry throttling. Repeated synchronous IN submission
failure triggers a recoverable abort/rearm after 32 attempts; Bulk-OUT rearm
and ZLP submission also have retry handling. The old description of waiting
100 ms and potentially discarding the final response is superseded.

Historical measurements in [NATIVE_PROGRESS_CALLBACKS.md](NATIVE_PROGRESS_CALLBACKS.md)
remain useful, but that document's 120-second budget and final-response-wait
paragraphs predate the November 4 deadline/retry changes. Current source and
this section describe the newer behavior. The latest production endpoint
recovery still needs runtime acceptance, especially on Windows.

### Trusted framework versus uploaded CAPs

The [CAP execution trust model](CAP_EXECUTION_TRUST_MODEL.md) now explicitly
states that GP authentication does not make uploaded bytecode trusted. Dynamic
package IDs 56–63 are classified by
[`cap_execution_policy.hpp`](../src/cap_execution_policy.hpp). The executing
package, not merely the original caller, controls native-opcode permission.
Uploaded `impdep1`/`impdep2` raise `SecurityException`; trusted ROM API wrappers
may use the internal native-call bridge.

Method lookup bounds normal/extended headers before reading packed fields.
Descriptor method offsets/counts define each frame's body, capped at the next
method start. Uploaded code fails closed without a valid body range; trusted
ROM has a Method-component fallback. `pc_t` bounds operand fetches and relative
updates, and exception diagnostics check adjacent-byte reads.

Taken short/wide/switch branches in dynamic packages must target a decoded
instruction start. The width decoder rejects unknown/truncated instructions
and bounds variable switch payloads. An offline scan covers 11 deployed API
CAPs, 978 methods, and 8,720 branch/switch targets; three Descriptor overcounts
are clamped to the next method. ASan/UBSan helper tests exercise 100,000
malformed short spans. These checks do not verify every untaken edge, stack
type, nested linkage, or heap field kind/access. Nor do decoder sanitizer tests
execute a malformed CAP through the complete loader/interpreter.

The trust boundary therefore remains incomplete. Firmware ROM/native code runs
in one native address space; no per-applet MPU/TrustZone containment has been
established. Type/reference/linkage validation remains a VM obligation, even
when storage CRCs and GP authentication both pass.

## 6. Updated status of the original layering findings

The original L1–L10 identifiers are retained for continuity; “improved” means
source responsibilities changed, not that all runtime acceptance is complete.

| Finding | Current state | Remaining separation work |
| --- | --- | --- |
| L1: board monolith | **Partly addressed.** Startup and filesystem are separate files; CAP static initialization is behind a VM interface. | `pico_filesystem.cpp` still combines overlay/schema, migration, memory accounting, VM transaction deferral, CAP knowledge, and physical SDK flash operations. Extract portable store/codec and a flash backend. |
| L2: transport owns card policy | **Partly addressed.** CCID route, framing, progress/retry helpers and callback-based VPCD session exist. | SCP03/registry/lifecycle/selection policy is still implemented in different CCID and host paths. Establish one production card-session owner over both transports. |
| L3: interpreted crypto providers | **Substantially addressed for keys/Cipher/Signature/digests.** Native bindings, metadata, and C++ API state machines replace those providers. | Audit remaining generated RandomData/PIN behavior; complete native ownership without altering upstream test CAPs or removing required API class metadata. |
| L4: VM/platform coupling | **Improved.** ExecutionBudget separates immutable deadline from progress; USB servicing is bounded. | Clock/watchdog/diagnostic hooks, `PC_VERSION`, and direct SDK dependencies still span VM/native modules. Add explicit host/Pico platform services and consistent cancellation contracts. |
| L5: persistence coordination | **Recovery implemented for the reviewed publication states.** Journals, reconciliation, array identities, rollback caches, and transactional DELETE are present. | Consolidate lifecycle transaction policy; replace hidden `fs_sync` deferral with explicit durability semantics. Extend physical interruption tests and address host checkpoint atomicity separately. |
| L6: scattered ABI | **Partly addressed.** `vm_filesystem.h`, `vm_lifecycle.h`, `vm_diagnostics.h`, and `vm_cap_initialization.h` are explicit interfaces; filesystem statuses/lifetimes are documented. | Historical `ffi.h` remains broad; diagnostics and lifecycle outputs are large procedural interfaces. Make ownership/error contracts enforceable and separate transport diagnostics from VM interfaces. |
| L7: identity/session ownership | **Significantly advanced.** Retained contexts, owner-aware proxies/references, SensitiveResult provenance, and transient/session lifetimes are implemented. | Global selected/response state and context orchestration remain in `main.cpp`; logical channels/general firewall semantics remain incomplete. Keep GC roots, deleted-owner lifetime, and native-state cleanup coordinated. |
| L8: linkage compatibility | **Improved.** Generated ROM metadata and inherited interface/type fixes reduce assumptions; CAP execution checks are stronger. | Class/method/interface/constructor remapping still spans CP, class and invoke handlers. Centralize API linkage rules while completing nested validation and malformed runtime coverage. |
| L9: crypto backend/secret boundary | **Still open, with lifecycle improvements.** Transient key copies and volatile operation sidecars are now explicit. | SCP03 and applet crypto still select primitives/entropy independently; global time/zeroize hooks and temporary secret buffers need consistent platform ownership and clearing. |
| L10: build/host parity | **Partly addressed.** Host CAP storage is multi-package/multi-instance; native method manifests and artifact checks exist. | Pico still consumes shared PC-generated ROM assets and profile outputs. Isolate generation/profile manifests, enforce library dependencies, and distinguish ARM runtime from RISC-V compile/smoke coverage. |

## 7. Validation evidence and known compatibility limits

Recent records establish progress, with different scopes:

- November 3 full upstream API 3.0.5 VPCD sweeps completed: Security **901
  calls** (397 OK, 504 expected unsupported); Crypto **466 calls** (304 OK,
  161 expected unsupported, one documented SHA-512/PSS parameter rejection).
  These are host results, not full physical acceptance of today's source.
- November 4 cross-store recovery used shared-flash lifecycle publication/retry
  tests plus builds and a physical ARM boot. It seeded durable cut-point states;
  it did not interrupt a live device during every sector erase/program.
- Recorded SIO/interface and non-atomic Util regressions exercised production
  CAP-to-native paths on VPCD. Crypto lifecycle tests covered reset, transaction
  abort, and process restoration. This does not establish complete firewall
  behavior or power-loss guarantees for every transient/non-atomic case.
- The immutable budget had fake-clock validation and a physical RSA-CRT-3072
  operation returning successfully after 72,612 ms. Waiting 600 real seconds
  for hardware expiry was not part of that test.
- Latest CCID route/retry and VPCD selected-AID helpers have focused tests and
  dual-architecture builds. Production CCID acceptance and a production VPCD
  restore test remain open. Recent integration retries failed before VPCD
  connection: one recorded install-time `SecurityException`, another stopped
  in the Rust host emulator with ptrace signal `639`, and a bounded retry
  timed out. No branch-alignment diagnostic appeared; those failures have not
  been established as branch-validation regressions.
- Uploaded-CAP header/body/branch checks have unit/deployed-CAP scans and
  decoder sanitizers. Latest trust-boundary slices were not physically tested
  or accepted through malformed-CAP runtime execution.

The [declared profile](supported-profile.md),
[November 3 review](review_20261103.md), [TODO](../TODO.md), and
[current handoff](../.agent/HANDOFF.md) give compatibility exclusions and newer
work-item status. Extended APDUs and logical channels are not generally
implemented; persistent reclamation remains disabled by default. MessageDigest
still buffers a maximum **2,046 cumulative input bytes**, rather than providing
unbounded streaming. `max_crypto` selects implemented features, not every Java
Card algorithm or complete 3.0.5 conformance.

`TEST:PASS choupi_runtime` remains a repeated report of startup status captured
before the main loop. It is a health indicator, not a fresh known-answer test
of each later APDU, storage update, or algorithm. Keep firmware identity,
transport success, VM results, and API conformance evidence separate.

Firmware-only UF2 preserves card storage. The specially packaged empty-card
image includes all-`0xFF` storage blocks and clears installed CAPs/filesystem
state when flashed. The [current artifact record](../artifacts/firmware/README-max_crypto-current-20261103.txt)
contains dated sizes/hashes and address-map checks. Do not infer the loaded
board image from a rebuilt file or treat an older physical result as acceptance
of the latest empty-card artifact. No flashing is needed for this document update.

## 8. Refactoring priorities and completion criteria

1. **Complete the uploaded-code trust boundary.** Validate nested linkage and
   field access/kinds, add malformed-CAP loader/interpreter sanitizer coverage,
   and preserve valid upstream CAP compatibility. Keep ROM native permission
   separate from installer authentication.
2. **Unify card sessions without weakening transport ownership.** Put GP,
   selection, reset, and lifecycle policy behind one session API used by CCID
   and VPCD. Keep queued callbacks/progress non-reentrant. Verify actual dispatch,
   authentication failure, response completion, and reset on both paths.
3. **Separate persistence policy from flash mechanics.** Preserve existing
   snapshots/journals/reconciliation while extracting portable records and
   codecs over a platform backend. Define transaction commit versus flush
   explicitly; test crash states and host checkpoint failures independently.
4. **Finish native API ownership and resource contracts.** Migrate residual
   interpreted provider behavior, centralize linkage metadata, and make
   scratch/secret cleanup and transient/session lifetime uniform. Keep backend
   crypto independent of VM arrays except at the checked API marshalling edge.
5. **Enforce module/build boundaries.** Split engine, JCRE/card manager, API,
   storage, platform, and diagnostics targets. Generate architecture-neutral
   metadata independently of the host executable; isolate profiles and bind
   firmware to a manifest. Report RISC-V's actual scope accurately.

A completed separation would let CCID and VPCD share card policy, portable
storage operate without querying interpreter transaction state, and the engine
compile without Pico headers. Native API semantics would have a single C++
owner with explicit object lifetime; platform services would own time, entropy,
hardware acceleration, watchdog, and secure clearing. Those are testable
boundaries, not simply directory renames or a request to introduce an RTOS.
