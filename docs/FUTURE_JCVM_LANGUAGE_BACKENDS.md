# FUTURE WORK ONLY — language-selectable JCVM and platform implementations

> **DO NOT IMPLEMENT THIS PLAN NOW.** This document is analysis and a proposal,
> not an active task, authorization, backlog assignment, or instruction to an
> agent. Reading it must never trigger refactoring, backend development,
> dependency replacement, flashing, or migration of stored card state. Begin
> implementation only after a separate, explicit user request. All proposed
> names, interfaces, commands, and acceptance gates below are future designs;
> they are not features supported by the current repository.

Analysis date: 2026-10-31. Based on the working tree, including existing
uncommitted changes. No implementation was changed or hardware tested for this
analysis.

## Recommendation

Select the VM implementation and platform implementation independently. Provide
presets that select compatible combinations, including an eventual all-Rust
firmware. Do not make replacement of the Pico platform shim a mandatory part of
every VM replacement. Conversely, do not describe a Rust VM using the existing
shim as an all-Rust firmware.

Two separate contracts allow C++, Rust, and C VMs to use the same tested storage
and transport, and allow a platform rewrite to be checked against the existing
VM. Rewriting both simultaneously removes a useful reference and makes storage,
APDU, and interpreter failures harder to locate. A combined preset gives users
one convenient choice without coupling the implementations internally.

“Drop-in” should mean unchanged CAPs, host clients, APDU behavior, lifecycle,
security semantics, and declared persistent-state compatibility. It does not
mean loading arbitrary dynamic libraries, changing languages at runtime, or
assuming that arbitrary heap representations are interchangeable.

## What exists today: source evidence

Paths below are relative to the repository root.

| Area | Current implementation and evidence | Consequence |
| --- | --- | --- |
| Pico firmware assembly | Root `CMakeLists.txt` builds `choupi_vm_core` as a C++ object library; `pico2_choupi` explicitly adds `choupi/src/main.cpp`, `src/choupi_pico2.cpp`, CCID, CAP load/store, SCP03, ROM-mask assembly and Mbed TLS C sources. | Backend selection must change the whole target composition, not just the interpreter file. |
| Pico platform | `src/choupi_pico2.cpp` includes `choupi/os/src/ffi.h` and implements `fs_*`, `path_*`, memory reporting, boot, diagnostics and progress services in C++. It calls `runtime` directly. | The current Pico path is a C++ platform shim over Pico SDK, not a linked Rust OS. |
| PC platform | `choupi/CMakeLists.txt` builds Cargo with `host,big_ram` and links `javacard_os`; `choupi/os/Cargo.toml` declares a Rust staticlib. `choupi/src/main_pc.cpp` contains VPCD integration and storage/registry handling. | PC and Pico need distinct platform adapters implementing the same semantics. |
| OS boundary | `choupi/os/src/ffi.h` wraps declarations in `extern "C"`; `ffi.rs` exports `#[no_mangle] extern "C"` functions. | Ordinary Rust/C++ symbol mangling is already avoided here; this is not a Rust ABI boundary. |
| Reverse OS calls | `ffi.rs` declares `runtime(package: u8, class: u8, method: u8)` without a return value; Pico declares it returning `uint16_t`. The Rust OS has context/remote-call entry paths. | Centralize the declaration and explicitly preserve or translate the status; the current declarations are not a single authoritative contract. |
| Lifecycle | `choupi/src/main.cpp` exports restore/select/deselect/process/clear and runtime/install entry points with C linkage. `src/ccid_device.cpp` calls these directly. | A useful starting seam exists, but is scattered and tied to implementation details. |
| Loader entanglement | `src/cap_load.cpp` includes `heap.hpp`, `jc_handlers/flashmemory.hpp`, `package.hpp`, static/import handlers and `jc_types/jc_array.hpp`; it calls `runtime_install_method` with many diagnostic outputs. | The loader cannot currently remain unchanged while a completely independent VM replaces C++. |
| Persistence | `choupi/src/jc_handlers/flashmemory.cpp` owns transaction snapshots, tag interpretation, typed access and sync; `src/choupi_pico2.cpp` owns flash overlay snapshots and package/instance storage. | A common filesystem ABI alone does not establish object-store compatibility or atomicity. |
| VM internals | `context.cpp`, `heap.cpp`, `frame.cpp`, `interpretor.cpp`, `jc_bytecodes/`, `jc_handlers/`, `jc_types/`, `jcvm_types/` use C++ objects and exceptions. | These must remain private to a backend. |
| Native APIs | `native_*.cpp` implement digest, key, cipher, MAC, signature and agreement behavior with VM-specific objects and dispatch. | Sharing crypto arithmetic is possible; sharing native dispatch unchanged is not a language-independent solution. |
| Build tooling | `tools/build` currently accepts architecture and support profile, generates/copies `jni.hpp` and ROM-mask artifacts from `build/choupi-pc`, and targets `choupi_vm_core`. | Generation, cache identity and wrapper behavior must also become backend-aware. |

The root CMake currently gates the full `pico2_choupi` executable on
`rp2350-arm-s`; RISC-V builds compile the VM core and smoke target. Do not
report a RISC-V VM-core compile as proof of a complete RISC-V card firmware.

## Coupling to remove, beyond name mangling

1. **Scattered declarations:** create authoritative public headers instead of
   hand-declaring entry points in transports, boot code and loaders. Check
   signatures in both call directions, including return values.
2. **VM details in common services:** move CAP linking, static initialization,
   object reconstruction and heap manipulation behind backend methods. Keep GP
   transfer framing and authentication outside the VM.
3. **Persistence policy leakage:** the shim's `fs_sync` queries
   `choupi_persistent_write_transaction_active`. Replace this reverse dependency
   with an explicit transaction protocol; define who may commit and when.
4. **Borrowed storage:** `fs_read_inplace` returns pointers. Specify validity
   across writes, compaction and callbacks, or replace public borrowed access
   with bounded copy/read operations. A Rust reference must never outlive an
   invalidated storage address.
5. **Diagnostic/progress cycles:** shim, CCID and VM call each other's debug and
   native-progress functions. Replace required interpreter debug symbols with
   optional structured events. Progress may service USB/watchdog but must not
   recursively dispatch another APDU.
6. **Global execution assumptions:** active contexts, selected applets and
   transient state currently use globals. Start with an explicitly serialized
   single-card contract; use opaque instance handles so this does not become a
   permanent language or multi-card constraint.
7. **Error models:** C++ exceptions, Rust panics and Java Card exceptions differ.
   Java exceptions remain VM data/control flow; backend faults and host service
   failures use explicit statuses. Never unwind across a foreign boundary.
8. **Semantic dependencies:** tag widths, package IDs, class indexes, object
   handles, endian rules, API/native method tokens and ROM-mask format must be
   documented independently of C++ class layouts.

## Proposed architecture and ownership

```text
USB/CCID or host VPCD transport
        -> card manager (GP, SCP03, registry, APDU routing)
        -> JCVM backend contract
           -> jcvm_cpp | jcvm_rust | jcvm_c
        -> platform services contract
           -> existing Pico C++ | Rust Pico | Rust host adapters
```

The diagram shows logical boundaries; the VM also directly consumes platform
services. Transport, card management, VM and platform are separate components.
Presets may choose their implementations together.

The VM owns CAP validation/linking, bytecode execution, Java exceptions, Java
Card APIs, native dispatch, object identity, firewall, transient lifecycle and
logical transactions. Card management owns authenticated GP commands, registry
policy and transfer sessions. The platform owns physical storage, entropy,
time, allocation policy, flash synchronization, hardware and transport progress.
Place durable instance metadata at one explicitly designated owner; do not keep
two independently authoritative registries.

Expose two versioned C-compatible contracts, tentatively
`include/jcvm_backend.h` and `include/jcvm_platform.h`. C ABI is a language-neutral
calling convention, not a requirement to compile C sources. Rust can implement
both sides with `extern "C"` and `#[repr(C)]`, or use internal traits behind the
same semantic contract in a Rust-only composition.

Backend operations should cover initialization/shutdown, capabilities, package
validation/load/unload, install, instance lookup/restore, select/deselect,
APDU process, reset/power events and diagnostics. Return opaque package/instance
handles rather than C++ classes or interpreter offsets. Separate runtime status,
Java exception information and APDU status word. Define response capacity and
length independently, no partial success on insufficient output space, and
whether the API returns data alone or data plus SW. Choose one convention.

Platform operations should cover bounded storage read/write/enumeration,
transaction begin/commit/abort or equivalent durable batching, entropy, monotonic
time and bounded progress. Specify errors including absent record, full store,
I/O failure and interrupted commit. Specify atomicity across VM object state,
static fields, registry updates and CAP activation; plain filesystem writes do
not automatically provide Java Card transactions.

For every operation define null/zero-length behavior, input limits, aliasing,
thread/reentry rules, ownership and buffer lifetime. Use fixed-width integers,
explicit status constants and pointer/length pairs. Avoid public STL, Rust
slices/strings/Vec, compiler-sized enums, C++ references and packed runtime
objects. Version structures with ABI version and structure size; check mandatory
function pointers and capability compatibility before use. Verify layout and
alignment on host, ARM and RISC-V. Allocation and deallocation must stay with the
same owner; private backend heaps avoid cross-allocator frees.

## Compile-time selection

Proposed options:

- `JCVM_BACKEND=jcvm_cpp|jcvm_rust|jcvm_c`, default `jcvm_cpp`.
- `JCVM_PLATFORM=pico_cpp|pico_rust|host_rust` (illustrative names).
- A firmware preset such as `rust_only` selecting Rust VM, platform, transport,
  card manager and crypto providers together.

Extend repository wrappers, eventually allowing an illustrative invocation like
`./tools/build arm --backend jcvm_rust --platform pico_rust --profile max_crypto`.
This command is **not supported now**. Architecture and algorithm profile remain
independent axes. Reject unknown/unimplemented backends and incompatible
combinations at configure time; never silently fall back to C++.

Use explicit component source lists and exactly one selected backend target.
Replace recursive discovery where it could accidentally compile another
implementation. All common callers link the contract target; only the C++
adapter includes private VM headers. Preserve `choupi_vm_core` as a compatibility
alias if tools need it, without pulling C++ into other selections.

CMake should import a Rust staticlib from a reproducible Cargo invocation with
locked dependencies, explicit target and profile. Match ARM hard-float ABI,
RISC-V ISA/ABI, panic strategy and target features to the firmware. Keep SDK
startup/linker ownership initially. Do not mix cross-language LTO until ordinary
linking and ABI tests pass. Isolate caches by architecture, backend, platform,
profile and toolchain; generation manifests must include these identities and
API/support inputs. Emit selections and hashes in artifacts and boot diagnostics.
Share ROM-mask/native-token generation as build tooling, not as an implicit
requirement to build the C++ emulator first.

## All-Rust firmware: a separate, broader milestone

A Rust VM plus Rust shim still leaves C/C++ unless the following are replaced:
Pico SDK startup/runtime and hardware calls, TinyUSB integration/CCID,
`ccid_device.cpp`, reader LED code, GP/SCP03/card registry, CAP load/store,
Mbed TLS C providers, C++ support objects and C library/compiler support pulled
in by the linker. ROM-mask assembly also needs an explicit policy.

Define the strict objective before this track starts: no C/C++ source or linked
C/C++ object code in the device image. Host tools that generate unchanged CAP/API
artifacts need not be Rust. Decide whether minimal architecture assembly is
allowed; if not, startup, interrupt vectors and compiler-generated runtime
helpers need their own feasibility review. “No C/C++” and “every instruction
originates from Rust source” are different requirements.

Use a Rust firmware entry point and target-appropriate hardware/USB, flash,
entropy and cryptographic implementations. Preserve USB descriptors/reader
behavior, flash safety, timing and supported algorithm semantics. Audit the
resolved dependency graph, compiler runtime, build scripts, linker map and
archive members; dependency names alone cannot prove language purity. A strict
preset must fail when a forbidden object/provider is linked. Rust safety does
not replace review of unsafe MMIO, FFI, entropy, persistence or side channels.

Keep native Java Card behavior in the selected native VM implementation. Do not
move algorithms into Java providers/adapters or alter CAPs to hide differences.
A future Rust/C implementation requires explicit authorization to extend the
current C++ implementation policy; this document does not grant it.

## Persistent-state interchangeability

Treat unchanged CAP compatibility and installed-state compatibility as separate
acceptance claims. The physical overlay snapshot format and VM logical object
schema are also distinct. Preserve the existing flash layout and avoid changing
security/OTP/partition configuration for backend experimentation.

First capture a schema specification from `flashmemory.cpp`, heap restoration,
CAP store and registry code: tag namespaces, IDs, primitive endian encoding,
arrays, references, statics, native keys, deletion, reachability and transaction
recovery. Include schema versions and the existing snapshot migration behavior.
Never serialize raw C++/Rust structs, pointers, vtables or crypto provider state.

Prefer a shared canonical logical record schema consumed by every backend.
Alternatively supply an explicit, validated migration with copy-on-write and
recovery. Until that exists, a backend must reject an incompatible store without
modifying it; advertise it as CAP-compatible only, not a full state-preserving
drop-in. No automatic formatting, destructive reset or silent key loss.
Test C++ -> Rust -> C++ and all supported backend pairs, including rollback
refusal when necessary. Back up test stores; use nonproduction keys and fixtures.

## Ordered implementation steps — only after future authorization

1. **Freeze evidence and contracts.** Inventory exports/imports, reverse calls,
   direct includes and dependency closures for PC and Pico. Capture clean-image
   and installed-state APDU baselines with exact CAP/profile/image hashes.
   Record known failures rather than declaring current behavior universally
   correct. Gate: reviewed ownership map and behavioral specification.
2. **Wrap existing C++ behavior.** Introduce the public contracts and C++ adapter;
   move lifecycle/install declarations into their authoritative interface.
   Translate errors at the adapter. Gate: existing host and hardware regressions
   retain their measured behavior with no storage-format change.
3. **Separate platform services.** Adapt existing Pico C++ and PC Rust services;
   remove reverse transaction/debug dependencies and specify callback lifetimes.
   Gate: contract tests run identically on both service implementations.
4. **Untangle loader/card management.** Move VM-specific linking, statics and heap
   work behind the backend. Keep GP framing/authentication and registry policy
   outside. Gate: common components compile without private VM headers and
   install/delete/persistence regressions pass.
5. **Add build selection.** Default to C++; reject unavailable alternatives.
   Isolate caches, manifests and artifact names. Gate: selected source closure
   and linked symbols prove exactly one implementation is present.
6. **Implement one new VM incrementally.** Rust is the recommended next backend
   given the all-Rust objective. Bring up parsing, interpreter, exceptions,
   objects, lifecycle, transactions and native APIs in testable stages. Gate:
   common conformance suite and differential tests meet declared capabilities;
   incomplete backends are explicitly experimental, never silent substitutes.
7. **Establish state compatibility.** Implement the canonical schema/migration
   and bidirectional switching checks before promising persistent drop-in use.
   Gate: crash recovery and key/object identity remain correct across switches.
8. **Replace platform and remaining firmware components independently.** Test
   Rust platform with the established VM where feasible, then combine Rust
   components. Gate: all-Rust dependency audit and full behavioral validation.
9. **Add a C backend if separately requested.** Reuse the contracts and tests;
   do not infer that creating an ABI automatically implements another VM.

For future platform-independent source changes, follow repository build gates:
`./tools/build arm` and `./tools/build riscv`, resolve warnings/errors, then flash
only the requested supported architecture and inspect finite serial output.
Full RISC-V firmware support is a distinct gate where currently absent. All
USB/PCSC work requires the prescribed host-access path from its first attempt;
stop hardware work if unavailable. Never claim host testing proves physical
power-loss safety.

## Testing independent implementations

### Shared conformance and contract tests

Build one backend-neutral harness over the public interfaces. Run identical
fixtures with the same profile, memory limits, entropy stream and initial store.
Cover malformed lengths/nulls, capacity limits, unsupported ABI versions,
allocation/storage failure and forbidden callback reentry. Validate native
primitive widths, structure layouts, signatures and symbol linkage on each
architecture. Keep C++ internal tests, but do not make them the common oracle.

Exercise bytecodes (including overflow/sign extension), CAP versions/imports,
exception handlers, object/array bounds, firewall/shareable behavior, install,
select/reselect/deselect, reset, transient clearing, transactions and deletion.
Test Java Card API exception classes/reasons and unsupported algorithms, not
only APDU status words. Use unchanged existing upstream CAPs for end-to-end
behavior; use native host fixtures for low-level contracts. Include known-answer
crypto vectors, invalid keys/parameters and resource-exhaustion recovery.

### Differential execution

Run every backend in a separate process against a cloned clean store and replay
identical APDU/lifecycle sequences. Compare response bytes, SW, registration,
selection, persistent logical records and post-reset behavior. Internal pointer
values, allocation addresses and instruction debug offsets are not comparable.
Compare canonical logical state rather than raw snapshot bytes when storage
encoding differs.

Inject deterministic entropy/time through test-only platform adapters. Never
ship deterministic entropy in production. For randomized signatures/keygen,
compare deterministic transcripts where possible and verify mathematical
properties and public results independently; byte inequality alone is not a
failure. Feed all implementations the same storage failures and interruption
schedule.

C++ is a regression reference, not automatically the specification oracle. A
mismatch triggers triage against Java Card semantics, known-answer vectors and
existing evidence; it must not be “fixed” by copying a known C++ bug. Maintain
explicit, scoped known-difference records with a reason and expiration gate.

### Fuzzing and stateful sequences

Fuzz backend-neutral CAP parsing/linking, APDU framing, ABI buffer validation,
record decoding and lifecycle sequences. Mutate lengths, indexes, import tokens,
exception tables, nesting and truncated inputs; combine install/select/process/
transaction/reset/delete operations. Run coverage-guided campaigns per backend
and a differential runner that minimizes sequences on behavioral divergence.
Use appropriate C/C++ sanitizers and Rust unsafe-code checking where supported,
plus bounded instruction/time/allocation budgets so hangs are reproducible.

Store minimized inputs, seeds, backend revisions, toolchains, profiles, initial
store hash and exact replay commands in a regression corpus. Triage memory
corruption, panic, timeout, resource leaks, invalid recovery and semantic
mismatch separately. Crashes must not be normalized into equal `6F00` results.

### Platform and persistence fault injection

Run the same abstract storage model tests against PC, Pico shim and future Rust
storage. Interrupt erase/program/commit at bounded checkpoints; verify recovery
selects a valid old or complete new state, and that Java transactions and GP
installation do not become partially visible. Include full stores, compaction,
dangling references, stale borrowed reads and failed registry activation.
Host emulation supplies repeatability; physical bounded power-cut tests establish
hardware behavior. Test backend changes with installed applets, statics, arrays,
keys and pending recovery, not only an empty card.

### Acceptance matrix and reporting

Track backend x platform x architecture x support profile, distinguishing
compile-only, host execution, physical APDU and physical restart/power-loss
results. Include combinations with the same VM/different platforms and the same
platform/different VMs; these isolate faults. Unsupported combinations must fail
configuration clearly. Test both cold and warm starts.

Run existing repository unit wrappers and selected unchanged JCAlgTest/API CAP
suites through host and physical transports. PC VPCD does not exercise Pico CCID;
cover GP GET DATA/GET STATUS, secure messaging, pagination and timing on the
actual transport too. Treat `TEST:FAIL`, `FAULT`, timeout and nonzero status as
failures. Archive logs and exact identities; absence of hardware is a recorded
coverage gap, not a pass.

Measure flash/RAM, stack/heap high-water, APDU latency, crypto throughput and
storage wear/write amplification per identical workload. Set explicit budgets
before claiming performance equivalence. Functional drop-in behavior does not
require identical performance, but transport timeouts and memory exhaustion
are observable compatibility failures.

## Completion criteria for a future backend release

A selectable backend is ready only when its advertised capability set passes
the shared tests, differences are explained, both architecture build gates are
satisfied, supported physical targets pass, and state compatibility is either
proved or explicitly restricted. An all-Rust release additionally needs the
linked-image language audit. Keep C++ as the default until a separately approved
change establishes another default. This analysis itself authorizes none of
these steps.
