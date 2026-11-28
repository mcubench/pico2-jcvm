# Development-only code and telemetry audit

2026-11-28 · Source baseline `9d2c2e49` · Raspberry Pi Pico 2, ARM Cortex-M33

The shipped target is the native JCVM in `pico2_choupi.uf2`. The ARM wrapper also builds `pico2_jcvm_smoke`, and the PC build generates the ROM mask and native dispatch header that feed the ARM build. This audit follows those build paths, the card's USB interfaces, and the tools that collect campaign evidence. Examples below are search anchors for a repeatable audit.

**Confirmed INV-24 violation.** The existing `max_crypto` ARM build identifies itself as `mode=normal` in provenance, with CPLC source ID `0x69C62719AD2AFB79`. `./tools/test-no-debug-traces` failed on it. The actual UF2 flash payload contains `CHOUPI:VM_PROGRESS`, `CHOUPI:EXCEPTION_DISPATCH` and `CHOUPI:SELECT_RX`. It also contains the CCID response-data, per-APDU filesystem, PQC profiling and object-deletion headroom messages described below. UF2 SHA-256 was `8a85c1f107ab06335fd72a43c8b071c3ac6cb32911294ab611cfcda62a8af706`. This artifact predates the source baseline. The source still has the trace sites. [INV-24](../SPEC.md#5-builds-identity-and-evidence) · [Earlier review](review_claude_20261127_validation_codex.md) · [Existing check](../tools/test-no-debug-traces)

## How to repeat the audit

1. Run the source searches below from the repository root. Record the emitter or state writer, its callers, its build/runtime guard, the data it handles, and the sink or reader. Inspect the enclosing function and all preprocessor branches. `PC_VERSION` selects the host implementation. `#ifndef PC_VERSION` selects ARM code in this build and permits diagnostics even in normal mode. A throttle, a failure predicate, or a disconnected CDC listener still permits the code in the image.
2. Follow the active CMake target lists, generated ROM files, and patched dependencies. Check the effective compile definitions in Ninja's generated build rules and the corresponding `CMakeCache.txt`. For each release profile, record source revision, profile, provenance and SHA-256 of the exact artifact. If a fresh artifact is needed, use `./tools/build arm --profile max_crypto` (or the profile being released). Inspect the full JCVM outputs. [ARM build](../tools/build) · [Target definitions](../CMakeLists.txt) · [PC and ROM-mask build](../tools/build-pc)
3. Run the existing trace check, then inspect the loadable bytes. Whole-ELF `strings` includes DWARF and symbol tables. Use `readelf -SW`, `readelf -lW` and the matching `pico2_choupi.bin` to corroborate hits. For the final UF2, including an empty-card package, decode the 512-byte records as in the packager, validate their magic, and scan flash payloads in address order. Search across adjacent records so a split string still matches. The card storage region begins at `0x10100000`. Verify the firmware payload against the audited build and the packaged storage against the empty-card requirement. Use the map, generated `.dis` and source call sites for hooks or state that need no strings. [Empty-card packaging](../tools/package-empty-card-uf2)
4. Label each hit with the evidence available, such as source-only, gated, linked in a normal image, or observed at a runtime interface. For shared helpers, determine which reads affect VM correctness and which feed only a report. Record test or campaign consumers before testing changes to telemetry. A clean static scan verifies artifact contents within its coverage. Hardware acceptance has its own requirements. [INV-27](../SPEC.md#5-builds-identity-and-evidence) · [Validation suites](../tests/README.md)

A first pass for unfamiliar emitters and guard names:

```bash
rg -n 'printf\(|fprintf\(|puts\(|putchar\(|fwrite\(|TRACE_JCVM_|panic\(|debug_assert!|println!|tud_cdc.*write' src choupi/src choupi/os/src
rg -n 'DEBUG|TRACE|DIAG|TELEMETRY|NDEBUG|SELF_TEST|SANITIZ|PROFILE' CMakeLists.txt choupi/CMakeLists.txt config tools/build tools/build-pc
strings -a build/arm/pico2_choupi.bin |
  rg 'CHOUPI:|OBJECT_DELETION:|TEST:|BENCHMARK:|MINING:|FAULT|Assertion|[(]assert'
```

Look up format strings built from fragments, wrapper macros and dynamically supplied prefixes. `src/pico_stdio_stream.c` routes the firmware's `fprintf` output, including stderr, to Pico stdio.

## Types to check

### 1. VM execution, exception and linkage traces

These report opcodes, PCs, stack depth, object references, class information, field values, and exception-dispatch decisions. Search for detailed work done only to build a message, including reads before a print, trace budgets and `fflush`. Normal ARM compilation of the `ATHROW`, `EXCEPTION_*` and `VM_PROGRESS` emitters violates INV-24. The PC has additional environment-controlled traces.

```bash
rg -n 'TRACE_JCVM_|CHOUPI:(ATHROW|EXCEPTION_|UNCAUGHT_JAVA|VM_(ENTRY_EXCEPTION|CAUGHT_EXCEPTION|EXCEPTION|PROGRESS)|APDU_PROCESS|CHECKCAST|CLASS_CAST|INTERFACE_|NULL_RECEIVER|JC305_SECURITY|NEW_|SIO_SERVER|CAP_REMOTE)' choupi/src src
rg -n 'exception_trace_budget|CHOUPI_VM_(TRACE|PROGRESS)' choupi/src
```

Start in [exception bytecodes](../choupi/src/jc_bytecodes/bc_exceptions.cpp), the [interpreter](../choupi/src/interpretor.cpp), [VM entry points](../choupi/src/main.cpp) and [trace macros](../choupi/src/debug.hpp). Keep exception handling, bytecode validation, execution budgets and status-word behavior in the correctness path.

### 2. USB, CCID and APDU traffic dumps

Search ingress and egress paths as well as the periodic printer. `SELECT_RX` dumps a request prefix at ingress. `SELECT_APDU` and `SELECT` expose applet lookup, restore and processing stages. The response snapshot in `send_response` retains a header and status and copies bounded response bytes for the matching GET STATUS command. Periodic CDC output can expose bytes that have already left the CCID buffer.

```bash
rg -n 'CHOUPI:(SELECT|CCID)|debug_(last|recent|response|stage)|choupi_ccid_debug_|hex=|tud_cdc' src/ccid_device.cpp src/choupi_pico2.cpp src/vm_diagnostics.h
```

Check buffer copies, command histories, transfer-error counters, and readers of those counters. Confirm the authorized recipient and diagnostic need for raw bytes, IDs and headers. Preserve CCID framing, response retry and status words. [CCID producer](../src/ccid_device.cpp) · [Serial consumer](../src/choupi_pico2.cpp)

### 3. Diagnostic data carried inside APDU responses

A protocol response can expose internal state without a serial message or a printable string. Two Pico paths warrant separate review.

- **Health record.** ISD GET DATA `DF7E` supports unauthenticated reads in the documented flow. Version 2 carries boot/reset state, a retained VM PC and trace info, uptime, and APDU/internal-error counters and last-error metadata. Follow `choupi_health_record` through both GET DATA forms, the CCID producer, and the POWMAN and watchdog state. This is documented campaign telemetry. Its removal or restriction needs a corresponding decision for the Windows campaign and supported profile. [GET DATA handler](../src/gp_get_data.hpp) · [Record layout](supported-profile.md) · [Windows decoder](../tools/windows-health.ps1)
- **INSTALL failure payload.** In `src/ccid_device.cpp`, the failure branch for INSTALL with P1 `0C` (install and make selectable) builds a 31-byte response including stage, VM status, exception opcode/PC, bytecode prefix/operand, opcode/depth history and final SW. Follow it into `Scp03Session::wrapResponse`. With no R-MAC, the wrapper returns early and the CCID caller sends the payload. For error status words with R-MAC active, the wrapper reduces the response to the two status bytes. This is a source finding. Check the captured wire format for the modes under review. Compare the PC implementation, which prints details and sets status separately. Preserve the final failure status and authentication checks. [ARM response](../src/ccid_device.cpp) · [SCP03 wrapping](../src/scp03.cpp) · [Install state](../src/cap_load.hpp) · [PC response](../choupi/src/main_pc.cpp)

```bash
rg -ni 'df7e|0x7e|health_record|health_apdus|health_sw_error|installVm(Exception|Debug)|installFailure(Stage|StatusWord)' src choupi/src
rg -n 'DF7E|health-before|health-after|health\.ps1' tools tests docs/supported-profile.md
```

### 4. Crash breadcrumbs and in-memory diagnostic state

Trace producers even when the only display occurs after a fault or reboot. `vm_diagnostics.h` connects field/array writes, restore failures and opcode/depth history to snapshots printed by the Pico loop. Values can include object references, field values and addresses. The `choupi_install_*` hooks also write RP2350 watchdog scratch registers. The boot path can recover these breadcrumbs and expose them through `WATCHDOG_RECOVERY` and the health record. Follow writes, clears and reads to check retention across reset and ensure the development-only instrumentation is absent from the hot path.

```bash
rg -n 'vm_diagnostics|runtime_diagnostics|choupi_vm_record_|choupi_vm_.*_state|choupi_install_(trace|stage|context|origin|debug)|watchdog_hw->scratch|powman_hw->scratch|WATCHDOG_RECOVERY|opcode_history|depth_history' src choupi/src
```

Distinguish breadcrumbs from watchdog servicing, timeout enforcement, and any reset/recovery decisions or SDK-reserved scratch use. [Declarations](../src/vm_diagnostics.h) · [VM state](../choupi/src/main.cpp) · [Pico boot and reporting](../src/choupi_pico2.cpp)

### 5. Persistence, allocation and garbage-collection telemetry

`FS_APDU` measures applet APDU and sync time, flash traffic and occupancy. Other `FS_*` and `PERSIST_*` messages include operation stages, tags, sizes and recovery details. Object-deletion instrumentation spans coarse events, timing, and verbose inventories of roots, aliases, tags and heap objects. Search the counters, inventory scans and tag-print loops in addition to the messages.

```bash
rg -n 'CHOUPI:(FS_|PERSIST_|RESTORE_|HEAP_GC|MEMORY|JC_TRANSIENT_ALLOC)|OBJECT_DELETION:|report_fs_apdu|ChoupiFsApduStats|choupi_fs_take_apdu_stats|sync_bytes|journal_bytes' src choupi/src
rg -n 'FS_APDU|journal_bytes|OBJECT_DELETION:(PRE_CLEANUP|HEAP_GC)|PERSISTENT_INVENTORY|GC_SCAN_DIAGNOSTICS' tools tests/README.md AGENTS.md
```

Per-object dumps and per-APDU timing are development instrumentation. Aggregate evidence has current consumers. `PRE_CLEANUP` feeds the 90% headroom gate and Windows capture, `HEAP_GC` is required by the existing trace check, and the persistence workflow requires `sync_bytes` and `journal_bytes` measurements. Account for these consumers explicitly in any release policy. Keep object deletion enabled and retain reachability checks, transaction and recovery operations, and counts used for capacity or GC decisions. [Filesystem](../src/pico_filesystem.cpp) · [GC](../choupi/src/persistent_inventory.cpp) · [Headroom checker](../tools/check-object-deletion-headroom.py) · [Persistence workflow](../AGENTS.md)

### 6. Performance probes and crypto resource measurements

`USB_PROGRESS` measures cooperative servicing and extension traffic. `RSA_MEMORY` samples free/minimum memory for a selected APDU. `PQC` reports operation and parameter set, time, status, allocator high water and private stack usage. Search timing captures and peak calculations around each sink, then follow the data to distinguish reporting from functional checks. In PQC, the measured stack extent also detects an overflow and rejects results. Retain that check and workspace wiping. Keep watchdog/USB progress calls used by long operations.

```bash
rg -n 'USB_PROGRESS|native_progress_stats|RSA_MEMORY|target_memory_operation|CHOUPI:PQC|stackUsed|pqc_arena_end|vm_perf::' src choupi/src tools tests/hardware
```

Check `tools/build-vm-perf-profile` as a distinct build path. It copies source and injects timing instrumentation before calling the ARM wrapper. A profile name or ordinary build provenance alone cannot identify that source variant. [PQC workspace](../choupi/src/native_pqc.cpp) · [Instrumented build](../tools/build-vm-perf-profile) · [CCID progress test](../tests/hardware/ccid_progress_physical.py)

### 7. Build switches, runtime toggles and injected behavior

Inspect options that enable diagnostics or alter behavior for a test. Follow defaults to cache values, generated compiler definitions and provenance. The ARM wrapper records heap-GC summaries, CCID applet traces, persistent-GC traces, reclaim allocation injection, serial wait and object deletion. Root CMake additionally defines `CHOUPI_DEBUG_HEAP_GC_TRACE` and `CHOUPI_DEBUG_MALLOC`. The wrapper neither sets nor records those two switches, so an existing CMake cache can leave them enabled despite a normal provenance label. The cache inspected for this audit had both OFF.

```bash
rg -n 'CHOUPI_DEBUG_|PICO2_DEBUG_|PICO_DEBUG_MALLOC|CHOUPI_(JCVM|OS)_DEBUG|CHOUPI_ENABLE_OBJECT_DELETION|mode=(normal|diagnostic)' CMakeLists.txt choupi/CMakeLists.txt tools/build tools/build-pc build/arm/CMakeCache.txt
rg -n 'getenv\(|var_os\(|PICO2_TEST_|CHOUPI_TEST_|CHOUPI_EXPECT_|FAIL_RECLAIM_ALLOC' src choupi/src choupi/os/src
```

Examples include simulated allocation failure in reclaim, a service delay waiting for a serial listener, and PC-only faults in install journal clearing/recovery, CAP instance persistence and delete cleanup. For each, find the guard around the call and the action. Check dependencies for `MLKEM_DEBUG`, `MLDSA_DEBUG`, `MBEDTLS_DEBUG_C`, `CFG_TUSB_DEBUG`, Rust `debug_assertions` and C/C++ assertion flags in the actual build. A library's debug source file alone does not establish linkage. Check that production input checks remain active when development assertions are compiled out. [Build switches](../CMakeLists.txt) · [Allocation injection](../src/recoverable_allocation.cpp) · [Load and delete hooks](../src/cap_load.cpp) · [Rust debug sink](../choupi/os/src/debug.rs)

### 8. Boot/test output, developer interfaces and host-only harnesses

Look for smoke/KAT banners, heartbeat summaries, test drivers, developer consoles and alternate USB interfaces. Inspect target membership and interface descriptors before flagging a match. `board_smoke.c` is a separate target. The full JCVM emits startup stages and `TEST:PASS choupi_runtime`, which `tools/cycle choupi` checks. The full device also declares a vendor BOOTSEL reset interface used for firmware updates. Review its reachability and purpose when auditing developer interfaces. `choupi/os/src/main.c` contains a legacy STM32 flash test console. Confirm exclusion through build targets rather than treating that console as RP2350 code. Apply the same check to VPCD, host emulator MPU tracing, test CAPs and SDK debug proxies.

```bash
rg -n 'BOOT |TEST:|BENCHMARK:|MINING:|FAULT|TUD_RPI_RESET_DESCRIPTOR|reset_usb_boot|dumpfs|debugproxy|CHOUPI_LOG_HOST_MPU' src choupi/src choupi/os tools/cycle CMakeLists.txt choupi/CMakeLists.txt
rg -n 'add_executable|target_sources|PC_VERSION|rommask-only' CMakeLists.txt choupi/CMakeLists.txt tools/build tools/build-pc
```

Retain public USB identity, CPLC and required GP/APDU responses. For failure paths, distinguish a diagnostic string or dump from the fail-closed action. For known-answer tests, check whether a self-test is part of the library's production behavior or a standalone validation harness. [Smoke target](../src/board_smoke.c) · [Cycle log consumer](../tools/cycle) · [USB descriptors](../src/ccid_device.cpp)

### 9. Debug metadata, generated files and package contents

The inspected Release ELF still carries `.debug_*` and `.symtab`. These non-loadable sections can disclose paths and symbols if the ELF ships. Inspect DWARF, maps, disassembly, compiled host classes, CAP Debug components if applicable, and logs in the actual distribution manifest. Inspect generated `build/choupi-pc/{flash,jni.hpp,generation.sha256}` through their generating inputs and hashes. Check which files the packagers copy and compare them to the ZIP or release listing. Windows *test bundles* intentionally include campaign collectors and fixtures. An empty-card firmware release must have an ISD-only card state. Preserve pinned fixture bytes and build identity.

```bash
readelf -SW build/arm/pico2_choupi.elf |
  rg 'debug|symtab|strtab|\.text|\.rodata'
rg -n 'provenance|mode=|\.uf2|\.elf|\.map|\.log|health|capture|zip|install -m|cp ' tools/package-*
```

Match firmware blocks to the audited artifact after packaging. A filename containing `nodebug` and a provenance check alone do not establish the absence of diagnostic code. [Windows campaign packager](../tools/package-maxcrypto-windows-test) · [Empty-card packager](../tools/package-empty-card-uf2)

## Invariant cross-check

INV-24 is the confirmed violation in this audit. For **INV-01–05**, the audit used the ARM native JCVM build graph. The separate smoke image, host tests and legacy OS code were treated as distinct targets. Hardware KATs and core ownership were outside this audit. For **INV-06–12**, preserve CCID and GP behavior, the supported API and APDU contracts, empty-card state and specified SCP03 defaults while checking telemetry surfaces. The INSTALL error path merits a wire-format check against the documented behavior. For **INV-13–16**, preserve durability, rollback, reachability and allocation behavior when removing reporting. For **INV-18–22**, preserve execution and parser checks, firewall decisions, key handling and controlled error paths. The observed data exposure and timing telemetry warrant inspection, with no claim here about exploitability or side-channel measurements. For **INV-23–28**, verify the exact profile and identity, keep fixtures pinned and acceptance physical, and preserve the resource envelope. Current evidence covers the existing ARM artifact and selected source paths. It does not establish full conformance for those other invariants. [Product specification](../SPEC.md)

No fresh build, simulator, USB, PC/SC, flash, or hardware execution was used. Vendor and legacy trees were sampled through build inputs and diagnostic searches. The audit did not inspect every historical release ZIP or run the Windows collectors.
