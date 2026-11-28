# Compile-time diagnostics migration

Status: proposed, 2026-11-28. The [source and artifact audit](debugging_code_20261128.md) records the baseline and repeatable searches. The standing review checks live in [AGENTS.md](../AGENTS.md#development-code-and-release-review). [SPEC.md](../SPEC.md#5-builds-identity-and-evidence) governs build modes and acceptance.

## Target configuration

| CMake option | Scope | Default |
| --- | --- | --- |
| `CHOUPI_ENABLE_TELEMETRY` | Debug traces, traffic dumps, crash breadcrumbs, diagnostic counters and inventories, and debug data exposed to a host. | OFF |
| `CHOUPI_ENABLE_PROFILING` | Timing, flash traffic and resource measurements collected solely for performance analysis. | OFF |
| `CHOUPI_ENABLE_TEST_HOOKS` | Fault injection, simulated interrupted operations and debug-only waits or assertions that alter runtime behavior. | OFF |

Give the ARM and PC wrappers the same public switch names. Use affirmative CMake target definitions for enabled options and leave them undefined when disabled. Keep `PC_VERSION` for platform selection. Keep `pico2_jcvm_smoke`, the PC simulator, and host test harnesses in their existing separate targets. A normal ARM image has all three switches OFF and object deletion ON. Derive provenance from effective build definitions. Any enabled development switch or disabled object deletion makes an ARM build diagnostic.

## 1. Resolve the evidence consumers

Record the agreed treatment of these contracts before moving their producers.

| Current output | Consumer and decision needed |
| --- | --- |
| `OBJECT_DELETION:PRE_CLEANUP` and `HEAP_GC` | The headroom checker, Windows capture, and trace test require these records. Agree how normal-image headroom evidence will be obtained. Keep the mandatory headroom check until its replacement is verified. |
| `CHOUPI:FS_APDU` | Persistence validation needs `sync_bytes` and `journal_bytes`. Specify where measurement will occur and how measurements will be identified. Preserve normal-image acceptance separately. |
| `CHOUPI:USB_PROGRESS` and `CHOUPI:PQC` | Physical progress checks and PQC investigations consume these. Decide which measurements are diagnostic and how functional behavior will be verified on a normal image. |
| ISD GET DATA `DF7E` | Windows campaign tooling reads it without a secure channel. Decide whether a minimal health record belongs to the documented product interface. Put the VM trace payload behind telemetry and update the decoder, checker, docs and tests together for any accepted format change. |
| Startup status and fault messages | `tools/cycle choupi` uses the runtime pass marker. Define the supported startup/fault signals before changing the consumer or replacing them with a readiness check. |

Any retained normal-image record needs a documented purpose, fields and interface. Resolve conflicts with INV-24 or INV-27 through the repository's external-request process. Diagnostic measurements can support investigation. Acceptance still requires normal-image evidence.

## 2. Wire the switches through the build

- Update [root CMake](../CMakeLists.txt), [PC CMake](../choupi/CMakeLists.txt), [tools/build](../tools/build) and [tools/build-pc](../tools/build-pc). Supply all option values explicitly on each configure. Apply definitions to the targets compiling the affected sources. Verify that host-only settings stay out of the generated ROM mask and ARM compile definitions unless deliberately selected for ARM.
- Migrate `CHOUPI_DEBUG_HEAP_GC`, `CHOUPI_DEBUG_HEAP_GC_TRACE`, `CHOUPI_DEBUG_PERSISTENT_GC`, `CHOUPI_DEBUG_CCID_APPLET`, `CHOUPI_DEBUG_MALLOC` and the debug-output use of `CHOUPI_JCVM_DEBUG` to their owning switch. Use profiling for timing and high-water samples and telemetry for event and object traces. Tie reclaim failure injection and serial startup wait to test hooks. Reject a nonzero wait unless test hooks are enabled. Find and update callers before retiring the old names. Check Rust OS diagnostics and linked SDK/library controls in the effective build.
- Record each effective switch in build provenance. Have [the Windows test packager](../tools/package-maxcrypto-windows-test) and other release consumers validate the relevant mode, profile and artifact identity. Exercise a diagnostic build followed by a normal build in the same cache. Exit this step with a verified mode mapping and no inherited debug setting capable of silently enabling diagnostics.

## 3. Migrate producers and consumers in coherent changes

| Class and source anchors | Change and check |
| --- | --- |
| VM and CCID telemetry in `choupi/src/interpretor.cpp`, `choupi/src/jc_bytecodes/`, `src/ccid_device.cpp` and `src/choupi_pico2.cpp` | Gate trace calls, diagnostic-only lookups, temporary data copies and history updates together. Check SELECT and applet dispatch with the switch OFF. Keep VM exceptions and status words and CCID retry/transfer handling. |
| Fault and APDU diagnostics in `src/vm_diagnostics.h`, `choupi/src/main.cpp`, `src/gp_get_data.hpp`, `src/cap_load.cpp` and `src/ccid_device.cpp` | Gate diagnostic state writers and readers, including watchdog scratch breadcrumbs and INSTALL debug payload construction. Preserve watchdog servicing and final INSTALL failure status. Check the INSTALL response under supported SCP03 response modes and test the agreed `DF7E` behavior in normal and telemetry builds. |
| Filesystem, deletion and native operation measurements in `src/pico_filesystem.cpp`, `src/ccid_device.cpp`, `choupi/src/persistent_inventory.cpp` and `choupi/src/native_pqc.cpp` | Gate measurement-only counters, scans, timers and reports. Apply the agreed evidence contracts. Keep limits, reachability and recovery checks, CCID progress and PQC stack overflow detection. Check that timing-only code disappears and required behavior still executes. |
| Test hooks in `src/recoverable_allocation.cpp`, `src/cap_load.cpp` and `choupi/src/main_pc.cpp` | Build failure-injection branches only when enabled. Keep host-only environment hooks within the PC target. Check that injection works in a test-hook build and cannot activate in a normal build. |
| Profile build in `tools/build-vm-perf-profile` | Move its source-injected probes under the profiling switch and use the normal wrapper path. Retire the source-rewriting path once its measurements have been reproduced. Verify comparable timing fields in the profiling output. |

## 4. Verify the release boundary

1. Build the full ARM JCVM in `max_crypto` with every switch OFF and with each switch ON individually. Check provenance, effective definitions, and representative positive and negative markers. Build the PC target with the options used by existing harnesses.
2. Extend [tools/test-no-debug-traces](../tools/test-no-debug-traces) to check the loadable firmware bytes. Use the link map or targeted behavior checks for hooks with no printable strings. Verify that the final UF2 embeds the same audited firmware bytes and that an empty-card package has the required card state. Use the actual artifact for each release profile and record its hash.
3. Run focused tests of the affected protocols and diagnostic consumers. For physical acceptance, use the required normal, empty-card image and unchanged pinned fixtures from [tests/README.md](../tests/README.md). Record diagnostic runs separately from acceptance.

The migration is complete when normal ARM images exclude development-only output and its collection code, each switch activates only its documented class, and the checked release artifact matches its provenance.
