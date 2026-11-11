# Practical Pico 2 development patterns from pico2-jcvm

Repository review: **2026-10-31**, HEAD **`eeb02fb`**, including the current working tree. This guide extracts reusable patterns for a different native Pico 2 application; Java Card ownership, tags, and APDUs are examples rather than prerequisites.

**How to read:** the table of contents gives a short summary of each topic, and the recommendation index summarizes every recommendation in one or two sentences. Follow a recommendation link for a short implementation example and its real source location.

**Example conventions:** snippets are excerpts or labelled adaptations, not standalone firmware; application-defined helpers and parameters must be supplied by the caller. SDK snippets need the corresponding Pico SDK target libraries, repository examples need the cited headers/implementations, and most snippets assume normal task context rather than an interrupt.

**Evidence:** current code takes precedence over older notes, and later dated test records qualify earlier claims. Line links below use repository-relative paths with `#L` anchors and display an explicit `file:line` label; viewers without source-line anchor support can still open the file and use that line number. References describe this working-tree snapshot and may shift after edits.

This is a source/document inspection, with no new build, flash, USB operation, benchmark, or hardware validation. Only this document was created; progress, handoff, QKB, and existing changes were left untouched as requested.

## Table of contents

- [Build and platform boundaries](#build-and-platform-boundaries): Choose the correct target, preserve generated assets, and distinguish portable compilation from a complete firmware port.
- [Watchdog, scheduling, and recovery](#watchdog-scheduling-and-recovery): Keep startup bounded, feed the watchdog at meaningful progress points, and retain enough context to diagnose a reset.
- [Persistent storage and existing data](#persistent-storage-and-existing-data): Use explicit durability boundaries, validated copy-on-write snapshots, and safe handling of missing or corrupt records.
- [Dynamic memory, footprint, and object lifetime](#dynamic-memory-footprint-and-object-lifetime): Bound buffers, measure both allocation and storage limits, and preserve identity when reclaiming objects.
- [Receiving commands and USB transport](#receiving-commands-and-usb-transport): Separate diagnostics from binary traffic, validate framing, and retain ownership until the final transfer completes.
- [ARM optimization and smaller firmware](#arm-optimization-and-smaller-firmware): Prefer measured peripheral acceleration and selective compilation before low-level instruction tuning.
- [Data ownership, secrets, and hardware services](#data-ownership-secrets-and-hardware-services): Use device services behind portable interfaces and make identity and secret lifetimes explicit.
- [Validation, physical workflow, and troubleshooting](#validation-physical-workflow-and-troubleshooting): Use focused host checks first, then collect identified-board evidence without confusing host access failures with firmware faults.
- [Source map and caveats](#source-map-and-caveats): Where to find detailed history and how to interpret older records.

## Recommendation index

| Recommendation | Practical summary |
| --- | --- |
| [1. Build through the wrappers and select the actual runtime](#1-build-through-the-wrappers-and-select-the-actual-runtime) | Use the repository wrappers to keep toolchain, SDK, generated metadata, and profile selection consistent. ARM currently builds the full JCVM firmware; RISC-V builds shared VM objects and a smoke image, so its successful build does not prove a complete RISC-V runtime. |
| [2. Declare flash capacity and reserve application data explicitly](#2-declare-flash-capacity-and-reserve-application-data-explicitly) | The current repository deliberately uses a 2 MiB flash address space and reserves only its first 1 MiB for firmware. Reuse the checked layout approach, but verify the actual board/flash capacity before adopting these numbers for another product. |
| [3. Keep native application logic separate from interpreted payloads](#3-keep-native-application-logic-separate-from-interpreted-payloads) | The firmware is C++ running on RP2350; CAP files are interpreted VM payloads. For a different application, port native functionality and expose it through a checked interface rather than treating Java/class/CAP files as native firmware. |
| [4. Preserve incremental generation and serialize builds](#4-preserve-incremental-generation-and-serialize-builds) | Keep generated files when their inputs are unchanged, and prevent simultaneous writers to shared build outputs. Start with the existing two-job default and measure before raising concurrency. |
| [5. Enable the watchdog after bounded startup diagnostics](#5-enable-the-watchdog-after-bounded-startup-diagnostics) | The runtime enables a 15-second watchdog after USB startup output and filesystem initialization, then feeds it during normal work. Use a timeout appropriate to your longest healthy operation and ensure startup cannot wait forever for a terminal. |
| [6. Record reset breadcrumbs in watchdog scratch registers](#6-record-reset-breadcrumbs-in-watchdog-scratch-registers) | Store a compact stage and context before risky work, copy retained values at boot, and consume the marker after reporting. Scratch registers provide reset diagnostics, not storage guaranteed to survive power removal. |
| [7. Use cooperative tasks and wrap-safe elapsed time](#7-use-cooperative-tasks-and-wrap-safe-elapsed-time) | Keep each main-loop task bounded so USB, status indication, and watchdog service continue. For short intervals on a wrapping 32-bit timer, use unsigned subtraction rather than comparing an absolute future timestamp. |
| [8. Give long operations an explicit progress path](#8-give-long-operations-an-explicit-progress-path) | Feeding the watchdog alone does not keep a host transaction alive: long computation also needs USB service and protocol-specific progress. The repository refreshes VM inactivity and sends CCID time extensions while retaining the original command buffer. |
| [9. Distinguish a RAM write from durable flash commit](#9-distinguish-a-ram-write-from-durable-flash-commit) | `fs_write()` updates a RAM overlay; persistence requires a successful `fs_sync()`. In this API most filesystem operations return zero on success, while `fs_exists()` returns one for present data. |
| [10. Check existence, length, and schema separately](#10-check-existence-length-and-schema-separately) | A present record can be empty or have an incompatible schema; do not use a payload value or a zero length as the sole existence test. Validate the exact expected length before reading a fixed-size record, then validate its version and fields. |
| [11. Publish a snapshot header only after its payload](#11-publish-a-snapshot-header-only-after-its-payload) | The filesystem writes the inactive slot, stores the payload first, and publishes a CRC-protected generation header last. At boot it validates header, lengths, payload CRC, and record structure before accepting a snapshot, with fallback to the other valid slot. |
| [12. Execute flash mutation through a RAM-safe callback](#12-execute-flash-mutation-through-a-ram-safe-callback) | XIP flash cannot be treated as readable executable storage during erase/program operations. The repository uses `flash_safe_execute()` and RAM-resident callbacks, with page buffers and callback inputs available in SRAM. |
| [13. Make alignment and region sizes compile-time invariants](#13-make-alignment-and-region-sizes-compile-time-invariants) | Keep erase-sector boundaries and program-page requirements explicit, and reject firmware that overlaps reserved storage. Use `static_assert` for layout invariants and runtime checks for variable payload sizes. |
| [14. Batch related writes with an explicit transaction owner](#14-batch-related-writes-with-an-explicit-transaction-owner) | Use the higher-level transaction layer when several changes must roll back together; direct low-level writes alone do not create before-images. The current implementation snapshots prior tag contents, tracks capacity, and restores them on abort or failed commit. |
| [15. Batch field updates to reduce wear and latency](#15-batch-field-updates-to-reduce-wear-and-latency) | A byte-at-a-time durable update can repeatedly rewrite a large snapshot and stall a host call. Gather related bytes, update the record in one operation, and publish at an application durability boundary. |
| [16. Version storage and preserve evidence of corruption](#16-version-storage-and-preserve-evidence-of-corruption) | Extend field widths through an explicit format migration and keep old data readable until replacement publication succeeds. If metadata indicates existing data but no valid snapshot can be restored, stop and preserve it rather than treating the card as blank. |
| [17. Journal operations spanning separate durable stores](#17-journal-operations-spanning-separate-durable-stores) | Two independently valid snapshots do not make a multi-store operation atomic. Record enough intent to recover an incomplete operation before publishing both the data and its registry metadata. |
| [18. Prefer fixed buffers for bounded protocol state](#18-prefer-fixed-buffers-for-bounded-protocol-state) | Use fixed arrays for known maximum packet sizes and small streaming scratch space. Fixed storage gives predictable capacity but still consumes SRAM, so avoid placing large arrays on a small stack. |
| [19. Use ownership-aware dynamic allocation and enforce a size cap](#19-use-ownership-aware-dynamic-allocation-and-enforce-a-size-cap) | Use RAII containers or smart pointers for allocations whose size genuinely varies, and reject untrusted sizes before allocating. Define an allocation-failure policy at the command boundary instead of assuming every allocation succeeds. |
| [20. Budget metadata as carefully as payload](#20-budget-metadata-as-carefully-as-payload) | Compact metadata and compile-time size assertions can save substantial SRAM when multiplied by thousands of entries. Widening an offset or adding a pointer changes both RAM consumption and serialized format, so account for both. |
| [21. Measure heap headroom and every independent storage limit](#21-measure-heap-headroom-and-every-independent-storage-limit) | Track allocator free blocks plus the uncommitted heap-to-stack-limit gap, and monitor persistent record count separately from payload bytes. Report these as estimates and capacities, not proof that a large contiguous allocation will succeed. |
| [22. Preserve stable handles and scan every live root before collection](#22-preserve-stable-handles-and-scan-every-live-root-before-collection) | Keep externally stored identifiers stable when freeing objects; do not compact a handle table unless all references can be rewritten safely. Reclaim only after a complete root walk succeeds, including native mirrors and persistent graph edges. |
| [23. Separate RAM reclamation from durable record deletion](#23-separate-ram-reclamation-from-durable-record-deletion) | Freeing a native wrapper does not automatically prove its persistent records are unreachable. The persistent reclaimer is opt-in experimental code, with full preflight, strict roots, bounded batches, and fresh cursors after mutation. |
| [24. Treat zero-copy pointers and cursors as borrowed views](#24-treat-zero-copy-pointers-and-cursors-as-borrowed-views) | `fs_read_inplace()` returns a pointer into storage owned by the filesystem, and overlay mutations can move data. Consume a borrowed view before mutation or copy it to bounded owned memory; restart enumeration after writes/deletes. |
| [25. Separate diagnostic CDC from the application protocol](#25-separate-diagnostic-cdc-from-the-application-protocol) | The firmware exposes CDC diagnostics plus a CCID interface with separate bulk endpoints. Another application can use the same separation, but should advertise descriptors and transfer limits matching its actual protocol. |
| [26. Give TinyUSB one owner and defer expensive command processing](#26-give-tinyusb-one-owner-and-defer-expensive-command-processing) | Keep endpoint state transitions in one execution context and use callbacks to record bounded work. The current build disables the SDK USB background task and dispatches command processing from the main loop. |
| [27. Validate complete frames before dispatch and avoid overflow](#27-validate-complete-frames-before-dispatch-and-avoid-overflow) | USB packets are fragments of a logical message, so collect the header, validate the advertised size, and dispatch only a complete bounded frame. For new parsers, use subtraction-based capacity checks so an attacker cannot wrap a length addition. |
| [28. Reuse a checked command parser and respect response capacity](#28-reuse-a-checked-command-parser-and-respect-response-capacity) | Parse command variants explicitly and keep input length distinct from requested output length. The short-APDU parser rejects extended frames and treats an encoded Le of zero as 256. |
| [29. Handle exact USB packet boundaries and buffer ownership](#29-handle-exact-usb-packet-boundaries-and-buffer-ownership) | A nonempty bulk response whose size is an exact multiple of 64 bytes may require a zero-length termination packet in this protocol. Keep the receive buffer reserved until the final response completes, including any intermediate time-extension packets. |
| [30. Keep serial output bounded and binary data out of text translation](#30-keep-serial-output-bounded-and-binary-data-out-of-text-translation) | USB logging can wait on a slow or absent host and interfere with watchdog/transport timing. Use bounded startup waits and batched diagnostics; use the binary interface for payloads or explicitly control text translation when designing CDC binary traffic. |
| [31. Use RP2350 hardware acceleration through the SDK](#31-use-rp2350-hardware-acceleration-through-the-sdk) | The device SHA-256 path acquires the hardware with a deadline, streams bounded blocks, and finishes into a fixed result buffer. Keep a separate portable implementation for host tests, and check accelerator ownership and error returns. |
| [32. Compile only the algorithms and features your product needs](#32-compile-only-the-algorithms-and-features-your-product-needs) | Choose a narrow profile and exclude unused native modules from the source list, rather than relying only on runtime feature checks. Keep advertised capabilities consistent with the selected implementation. |
| [33. Optimize ARM from a measured hot path and linked output](#33-optimize-arm-from-a-measured-hot-path-and-linked-output) | Start from the wrapper's Release build, measure a representative workload, then inspect the resulting instructions, stack usage, spills, and memory traffic. Keep ISA-specific changes behind a portable interface and compare ARM and RISC-V correctness separately. |
| [34. Separate application timing from raw primitive throughput](#34-separate-application-timing-from-raw-primitive-throughput) | Measure baseline transport/VM overhead and the same payload repeatedly before attributing time to an algorithm. Record board, firmware hash, profile, workload, repetitions, and timing source with each result. |
| [35. Use the platform random service and avoid repeated setup per byte](#35-use-the-platform-random-service-and-avoid-repeated-setup-per-byte) | The native random path obtains one SDK random word for each four output bytes, with a different host implementation. Preserve that platform boundary rather than introducing a time-seeded pseudo-random generator for security-sensitive data. |
| [36. Wipe temporary secrets on all exits with RAII](#36-wipe-temporary-secrets-on-all-exits-with-raii) | Freeing a vector does not overwrite its secret bytes. The RSA code binds scratch buffers to a scope guard that calls `mbedtls_platform_zeroize()` on normal return and exception unwinding. |
| [37. Give independent persistent owners stable identifiers](#37-give-independent-persistent-owners-stable-identifiers) | Do not derive durable ownership solely from a class or array position when multiple instances can share that class. Persist a stable owner mapping and scope deletion to that owner. |
| [38. Implement status indication without blocking the application loop](#38-implement-status-indication-without-blocking-the-application-loop) | Use PWM plus a periodic task or short alarm callback for heartbeat and activity indication. Obtain the LED pin from board definitions so the same module can compile for boards without that pin. |
| [39. Test corruption, interrupted writes, and restart recovery offline](#39-test-corruption-interrupted-writes-and-restart-recovery-offline) | Model flash constraints and inject failures between mutations before relying on physical recovery. Test old-or-new snapshot recovery, migration, full-capacity writes, and retry behavior after reboot. |
| [40. Flash the intended image and require finite runtime evidence](#40-flash-the-intended-image-and-require-finite-runtime-evidence) | Build both architectures before flashing, identify the board, and distinguish a smoke pass from an application test. `cycle arm`/`cycle riscv` flash smoke firmware, whereas `cycle choupi` flashes the full ARM JCVM and checks a finite runtime log. |
| [41. Diagnose USB visibility, serial nodes, and PC/SC separately](#41-diagnose-usb-visibility-serial-nodes-and-pcsc-separately) | A visible USB device or working CDC terminal does not guarantee that a physical PC/SC reader has registered. Re-enumerate after reconnect, select the exact current reader by board identity, and keep discovery deadlines separate from command deadlines. |
| [42. Report long-run resource failures and evidence limits explicitly](#42-report-long-run-resource-failures-and-evidence-limits-explicitly) | Short known-answer tests and support declarations do not establish long-duration memory stability or complete API correctness. Keep the latest full-run findings visible and distinguish simulated reset, process restart, hardware boot, and physical power interruption. |

## Build and platform boundaries

### 1. Build through the wrappers and select the actual runtime

Use the repository wrappers to keep toolchain, SDK, generated metadata, and profile selection consistent. ARM currently builds the full JCVM firmware; RISC-V builds shared VM objects and a smoke image, so its successful build does not prove a complete RISC-V runtime.

```sh
./tools/build arm --profile baseline
./tools/build riscv --profile baseline
./tools/build-pc --profile baseline  # separate emulator build
```
For another application, retain this separation between portable components and board startup/peripheral integration; do not assume the PC executable is rebuilt by either firmware command.

Source: [tools/build:118](../tools/build#L118); [CMakeLists.txt:149](../CMakeLists.txt#L149).

### 2. Declare flash capacity and reserve application data explicitly

The current repository deliberately uses a 2 MiB flash address space and reserves only its first 1 MiB for firmware. Reuse the checked layout approach, but verify the actual board/flash capacity before adopting these numbers for another product.

```cmake
# Set before SDK initialization, as in this repository.
set(PICO_BOARD pico2 CACHE STRING "Board")
set(PICO_FLASH_SIZE_BYTES 2097152 CACHE STRING "Flash bytes")
```
`src/flash_layout.hpp` reserves eight `0x11000` CAP slots, two `0x28000` filesystem slots, metadata mirrors, and legacy data. These are application choices, not universal Pico 2 partitions; avoid destructive capacity probes from older reference notes.

Source: [CMakeLists.txt:6](../CMakeLists.txt#L6); [src/flash_layout.hpp:8](../src/flash_layout.hpp#L8); [src/choupi_pico2.cpp:1062](../src/choupi_pico2.cpp#L1062).

### 3. Keep native application logic separate from interpreted payloads

The firmware is C++ running on RP2350; CAP files are interpreted VM payloads. For a different application, port native functionality and expose it through a checked interface rather than treating Java/class/CAP files as native firmware.

The reusable boundary is **transport → validated command → application service → platform adapter**. Here, native cryptography lives in `choupi/src/native_*.cpp`, while platform filesystem/USB integration lives in `src/`; the PC SHA path uses Mbed TLS and the device path uses the Pico SDK.

Source: [choupi/src/native_sha256.cpp:9](../choupi/src/native_sha256.cpp#L9); [src/ccid_device.cpp:36](../src/ccid_device.cpp#L36).

### 4. Preserve incremental generation and serialize builds

Keep generated files when their inputs are unchanged, and prevent simultaneous writers to shared build outputs. Start with the existing two-job default and measure before raising concurrency.

```sh
PICO_BUILD_JOBS=2 ./tools/build arm --profile baseline
```
The wrapper locks `build/.tools-build.lock` and checks hashes for API, ROM-mask, and support-profile inputs. `perf_tips.md` contains historical timing samples, not guaranteed build times or evidence that more parallel jobs are faster.

Source: [tools/build:44](../tools/build#L44); [tools/build:58](../tools/build#L58); [perf_tips.md:258](../perf_tips.md#L258).

## Watchdog, scheduling, and recovery

### 5. Enable the watchdog after bounded startup diagnostics

The runtime enables a 15-second watchdog after USB startup output and filesystem initialization, then feeds it during normal work. Use a timeout appropriate to your longest healthy operation and ensure startup cannot wait forever for a terminal.

```cpp
#include "hardware/watchdog.h"
// After bounded platform initialization:
watchdog_enable(15000, true); // repository setting; debugger pause enabled
for (;;) {
    service_one_bounded_unit(); // application-defined
    watchdog_update();
}
```
The example helper is illustrative. In this repository, `fs_init()` failure loops before watchdog enable, so that failure path deliberately does not recover through the watchdog; choose your own policy explicitly.

Source: [src/choupi_pico2.cpp:1517](../src/choupi_pico2.cpp#L1517); [src/choupi_pico2.cpp:1561](../src/choupi_pico2.cpp#L1561).

### 6. Record reset breadcrumbs in watchdog scratch registers

Store a compact stage and context before risky work, copy retained values at boot, and consume the marker after reporting. Scratch registers provide reset diagnostics, not storage guaranteed to survive power removal.

```cpp
#include "hardware/structs/watchdog.h"
watchdog_hw->scratch[0] = 0x494e5354; // repository marker: INST
watchdog_hw->scratch[1] = operation_id; // application-defined
watchdog_hw->scratch[3] = stage;
// On boot: copy the fields, then clear scratch[0].
```
The repository records opcode, instruction count, stage, context, and origin. It reports the reset cause separately: an old marker with `cause=other` is not proof of a new watchdog reset; reserve scratch registers with the SDK/boot protocol in mind.

Source: [src/choupi_pico2.cpp:99](../src/choupi_pico2.cpp#L99); [src/choupi_pico2.cpp:1522](../src/choupi_pico2.cpp#L1522).

### 7. Use cooperative tasks and wrap-safe elapsed time

Keep each main-loop task bounded so USB, status indication, and watchdog service continue. For short intervals on a wrapping 32-bit timer, use unsigned subtraction rather than comparing an absolute future timestamp.

```cpp
#include "pico/time.h"
uint32_t last = time_us_32();
// In a repeatedly called task; intervals must be well below timer wrap:
const uint32_t now = time_us_32();
if (static_cast<uint32_t>(now - last) >= 1000000u) {
    last = now;
    publish_progress(); // application-defined, bounded
}
```
The main loop calls TinyUSB, CCID, LED, and watchdog services. `tight_loop_contents()` is a loop hook, not a delay or a guarantee that USB has been serviced.

Source: [src/choupi_pico2.cpp:1590](../src/choupi_pico2.cpp#L1590); [src/ccid_device.cpp:987](../src/ccid_device.cpp#L987); [src/reader_led.cpp:32](../src/reader_led.cpp#L32).

### 8. Give long operations an explicit progress path

Feeding the watchdog alone does not keep a host transaction alive: long computation also needs USB service and protocol-specific progress. The repository refreshes VM inactivity and sends CCID time extensions while retaining the original command buffer.

```cpp
// Existing platform hook, for native code in this firmware:
extern "C" void choupi_native_crypto_progress(void);
// Call at meaningful progress boundaries in a long native operation.
choupi_native_crypto_progress();
```
Use the actual hook definition below to see its watchdog/USB cooperation; another protocol needs its own equivalent. Keep an overall operation deadline or work budget so a live progress callback cannot hide an endlessly looping algorithm.

Source: [src/choupi_pico2.cpp:93](../src/choupi_pico2.cpp#L93); [src/ccid_device.cpp:985](../src/ccid_device.cpp#L985); [choupi/src/interpretor.cpp:48](../choupi/src/interpretor.cpp#L48).

## Persistent storage and existing data

### 9. Distinguish a RAM write from durable flash commit

`fs_write()` updates a RAM overlay; persistence requires a successful `fs_sync()`. In this API most filesystem operations return zero on success, while `fs_exists()` returns one for present data.

```cpp
#include "ffi.h"
extern "C" uint8_t fs_sync(void); // platform extension, not declared in ffi.h
// tag/data are valid application-owned buffers; no transaction is active.
bool save_record(const uint8_t *tag, uint8_t tag_len,
                 const uint8_t *data, uint32_t len) {
    if (fs_write(tag, tag_len, data, len) != 0) return false;
    return fs_sync() == 0;
}
```
`fs_sync()` returns success without publishing while a persistent transaction is active: the transaction owner must perform the commit. A sync failure must not produce a durable-success reply; the modified overlay may remain dirty in RAM.

Source: [src/choupi_pico2.cpp:1177](../src/choupi_pico2.cpp#L1177); [src/choupi_pico2.cpp:1103](../src/choupi_pico2.cpp#L1103).

### 10. Check existence, length, and schema separately

A present record can be empty or have an incompatible schema; do not use a payload value or a zero length as the sole existence test. Validate the exact expected length before reading a fixed-size record, then validate its version and fields.

```cpp
#include "ffi.h"
#include <array>
// Generic pattern: adapt the tag to your application's namespace.
std::array<uint8_t, 8> value{};
uint32_t stored_len = 0;
if (fs_exists(tag, tag_len) != 0) {
    if (fs_length(tag, tag_len, &stored_len) != 0 ||
        stored_len != value.size() ||
        fs_read(tag, tag_len, value.data(), stored_len) != 0) {
        return false; // corrupt/incompatible; do not silently overwrite
    }
    // Validate your schema/version bytes before use.
} else {
    // First-use defaults; save explicitly if durability is required.
}
```
This fragment belongs inside a boolean-returning loader with `tag` and `tag_len` supplied. The implementation can find immutable ROM defaults as well as mutable overlay records, so existence does not necessarily mean a user has previously saved the record.

Source: [src/choupi_pico2.cpp:1110](../src/choupi_pico2.cpp#L1110); [src/choupi_pico2.cpp:943](../src/choupi_pico2.cpp#L943); [src/choupi_pico2.cpp:1165](../src/choupi_pico2.cpp#L1165).

### 11. Publish a snapshot header only after its payload

The filesystem writes the inactive slot, stores the payload first, and publishes a CRC-protected generation header last. At boot it validates header, lengths, payload CRC, and record structure before accepting a snapshot, with fallback to the other valid slot.

```text
RAM overlay
  → erase inactive slot
  → program payload pages
  → program header containing generation and CRCs
  → validate the completed slot
  → change active slot in RAM
```
Keep the previous committed slot intact until the replacement is valid. CRCs detect accidental corruption; they do not authenticate attacker-controlled data, and modelled atomic recovery is not proof against every physical flash failure.

Source: [src/choupi_pico2.cpp:344](../src/choupi_pico2.cpp#L344); [src/choupi_pico2.cpp:305](../src/choupi_pico2.cpp#L305); [src/choupi_pico2.cpp:484](../src/choupi_pico2.cpp#L484).

### 12. Execute flash mutation through a RAM-safe callback

XIP flash cannot be treated as readable executable storage during erase/program operations. The repository uses `flash_safe_execute()` and RAM-resident callbacks, with page buffers and callback inputs available in SRAM.

```cpp
#include "pico/flash.h"
#include "pico/platform.h"
void __not_in_flash_func(write_callback)(void *opaque) {
    // Erase/program only a reserved application region.
    // Callback, callees, constants, and input bytes must be flash-safe.
}
// From normal context, with a valid SRAM context:
const int status = flash_safe_execute(write_callback, &context, 5000);
```
This is a structural sketch, not a complete flash writer; copy alignment and bounds checks from the source. If you add a second core, configure the SDK's safe-execution/lockout support for that arrangement before allowing writes; the wrapper does not make arbitrary other-core XIP activity safe by itself.

Source: [src/choupi_pico2.cpp:319](../src/choupi_pico2.cpp#L319); [src/choupi_pico2.cpp:414](../src/choupi_pico2.cpp#L414); [src/cap_store.cpp:268](../src/cap_store.cpp#L268).

### 13. Make alignment and region sizes compile-time invariants

Keep erase-sector boundaries and program-page requirements explicit, and reject firmware that overlaps reserved storage. Use `static_assert` for layout invariants and runtime checks for variable payload sizes.

```cpp
#include "hardware/flash.h"
alignas(FLASH_PAGE_SIZE) uint8_t page[FLASH_PAGE_SIZE];
static_assert((flash_layout::filesystem_offset & 0xfffu) == 0);
static_assert(flash_layout::filesystem_end <= 0x200000);
```
The current layout uses 4 KiB sector alignment and 160 KiB snapshot slots; its writer rounds programmed payload and erase size independently. The host flash model checks 4 KiB erases, 256-byte program counts/offsets, and buffer alignment.

Source: [src/flash_layout.hpp:41](../src/flash_layout.hpp#L41); [src/choupi_pico2.cpp:400](../src/choupi_pico2.cpp#L400); [tests/unit/rp2350_fs_overlay/rp2350_fs_overlay_test.cpp:105](../tests/unit/rp2350_fs_overlay/rp2350_fs_overlay_test.cpp#L105).

### 14. Batch related writes with an explicit transaction owner

Use the higher-level transaction layer when several changes must roll back together; direct low-level writes alone do not create before-images. The current implementation snapshots prior tag contents, tracks capacity, and restores them on abort or failed commit.

```cpp
using jcvm::FlashMemory_Handler;
if (!FlashMemory_Handler::beginPersistentWriteTransaction(true))
    return false;
if (!FlashMemory_Handler::writePersistentRecord(tag, data, length)) {
    (void)FlashMemory_Handler::rollbackPersistentWriteTransaction();
    return false;
}
return FlashMemory_Handler::commitPersistentWriteTransaction();
```
This function-body fragment assumes an `fs::Tag tag` and valid buffers. The current source sets `kMaxCommitCapacity = 0x7fff` (32,767 bytes of charged before-images/metadata); older architecture prose saying zero capacity is stale, and the limit is not a guarantee that that much contiguous heap is available.

Source: [choupi/src/jc_handlers/flashmemory.cpp:73](../choupi/src/jc_handlers/flashmemory.cpp#L73); [choupi/src/jc_handlers/flashmemory.cpp:321](../choupi/src/jc_handlers/flashmemory.cpp#L321); [choupi/src/jc_handlers/flashmemory.cpp:393](../choupi/src/jc_handlers/flashmemory.cpp#L393).

### 15. Batch field updates to reduce wear and latency

A byte-at-a-time durable update can repeatedly rewrite a large snapshot and stall a host call. Gather related bytes, update the record in one operation, and publish at an application durability boundary.

For a telemetry application, accumulate a bounded batch in RAM and commit on an explicit save command or checkpoint; document what can be lost before that checkpoint. QKB records persistent key clear/component writes that stalled EC setup and were improved by batching; avoid inferring a numeric wear lifetime or speedup without measuring your flash part and workload.

Source: [src/choupi_pico2.cpp:390](../src/choupi_pico2.cpp#L390); [choupi/src/jc_handlers/flashmemory.cpp:393](../choupi/src/jc_handlers/flashmemory.cpp#L393); [.agent/QKB.md:1279](../.agent/QKB.md#L1279).

### 16. Version storage and preserve evidence of corruption

Extend field widths through an explicit format migration and keep old data readable until replacement publication succeeds. If metadata indicates existing data but no valid snapshot can be restored, stop and preserve it rather than treating the card as blank.

The current filesystem is version 3, accepts version 2, and stores offset bit 16 in flag bit 6. Its arena is **77,824 bytes**, with **8,192** index entries; each individual record still has a 16-bit size, so increasing total capacity does not permit an arbitrarily large single record.

`fs_init()` emits `FAULT FS snapshot unavailable; preserve flash for recovery` when existing CAP/header evidence makes blank initialization unsafe. For another application, also version the payload schema independently from the container format.

Source: [src/choupi_pico2.cpp:142](../src/choupi_pico2.cpp#L142); [src/choupi_pico2.cpp:167](../src/choupi_pico2.cpp#L167); [src/choupi_pico2.cpp:1095](../src/choupi_pico2.cpp#L1095); [.agent/QKB.md:2389](../.agent/QKB.md#L2389).

### 17. Journal operations spanning separate durable stores

Two independently valid snapshots do not make a multi-store operation atomic. Record enough intent to recover an incomplete operation before publishing both the data and its registry metadata.

The CAP installer journals package/instance identifiers and owner, then coordinates filesystem state with mirrored CAP metadata; startup replays pending journals. Another application with a file plus an index can reuse the intent → data commit → index publication → journal cleanup sequence, but must make recovery idempotent and test every intermediate state.

Source: [src/cap_load.cpp:334](../src/cap_load.cpp#L334); [src/cap_load.cpp:296](../src/cap_load.cpp#L296); [src/cap_store.cpp:310](../src/cap_store.cpp#L310).

## Dynamic memory, footprint, and object lifetime

### 18. Prefer fixed buffers for bounded protocol state

Use fixed arrays for known maximum packet sizes and small streaming scratch space. Fixed storage gives predictable capacity but still consumes SRAM, so avoid placing large arrays on a small stack.

```cpp
#include <array>
std::array<uint8_t, 64> scratch{}; // bounded local work buffer
// Large, long-lived buffers belong in explicitly budgeted static state.
```
CCID uses fixed receive/transmit buffers; SHA-256 streams through a 64-byte block rather than duplicating the whole input. The overlay alone reserves 65,536 bytes for its 8-byte index entries plus 77,824 arena bytes, before erase bookkeeping and other runtime memory.

Source: [src/ccid_device.cpp:97](../src/ccid_device.cpp#L97); [choupi/src/native_sha256.cpp:51](../choupi/src/native_sha256.cpp#L51); [src/choupi_pico2.cpp:221](../src/choupi_pico2.cpp#L221).

### 19. Use ownership-aware dynamic allocation and enforce a size cap

Use RAII containers or smart pointers for allocations whose size genuinely varies, and reject untrusted sizes before allocating. Define an allocation-failure policy at the command boundary instead of assuming every allocation succeeds.

```cpp
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
std::unique_ptr<uint8_t[]> make_buffer(size_t n) {
    constexpr size_t limit = 1024; // example application policy
    if (n == 0 || n > limit) return {};
    return std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[n]{});
}
```
This adaptation uses explicit failure returns; repository VM code also uses throwing `std::vector`/`std::make_shared` with exceptions enabled. Its array backing storage is owned by `JCVMArray`, while heap slots own arrays/instances; copying a raw allocation without its ownership rules would introduce leaks or double frees.

Source: [choupi/src/heap.cpp:99](../choupi/src/heap.cpp#L99); [choupi/src/jc_types/jc_array.cpp:119](../choupi/src/jc_types/jc_array.cpp#L119); [CMakeLists.txt:16](../CMakeLists.txt#L16).

### 20. Budget metadata as carefully as payload

Compact metadata and compile-time size assertions can save substantial SRAM when multiplied by thousands of entries. Widening an offset or adding a pointer changes both RAM consumption and serialized format, so account for both.

```cpp
struct Entry {
    uint16_t hash;
    uint8_t tag_length, flags;
    uint16_t offset, size;
};
static_assert(sizeof(Entry) == 8);
```
This mirrors `OverlayFile`; snapshot records additionally carry a 16-bit slot index and occupy ten bytes. The current high-offset flag extends arena addressing without growing every SRAM entry, but requires all readers/writers to use the encoding helpers.

Source: [src/choupi_pico2.cpp:145](../src/choupi_pico2.cpp#L145); [src/choupi_pico2.cpp:153](../src/choupi_pico2.cpp#L153); [src/choupi_pico2.cpp:206](../src/choupi_pico2.cpp#L206).

### 21. Measure heap headroom and every independent storage limit

Track allocator free blocks plus the uncommitted heap-to-stack-limit gap, and monitor persistent record count separately from payload bytes. Report these as estimates and capacities, not proof that a large contiguous allocation will succeed.

```cpp
extern "C" uint32_t choupi_available_transient_memory_bytes(void);
extern "C" uint32_t choupi_available_persistent_memory_bytes(void);
const uint32_t ram_estimate = choupi_available_transient_memory_bytes();
const uint32_t arena_remaining = choupi_available_persistent_memory_bytes();
```
The first helper uses `mallinfo`/`mallinfo2`, `sbrk(0)`, and `__StackLimit`; fragmentation and stack peaks still matter. Persistent capacity returns zero when either the index or arena is exhausted, and free arena bytes include space still needed for tags/records.

Source: [src/choupi_pico2.cpp:1012](../src/choupi_pico2.cpp#L1012); [src/choupi_pico2.cpp:1041](../src/choupi_pico2.cpp#L1041).

### 22. Preserve stable handles and scan every live root before collection

Keep externally stored identifiers stable when freeing objects; do not compact a handle table unless all references can be rewritten safely. Reclaim only after a complete root walk succeeds, including native mirrors and persistent graph edges.

The heap resets dead `shared_ptr` slots and reuses holes instead of shifting surviving indices. A different application with a handle registry should include queued messages, callbacks, in-flight operations, and durable references in its root model; an incomplete scan must prevent deletion.

Source: [choupi/src/heap.cpp:92](../choupi/src/heap.cpp#L92); [choupi/src/heap.cpp:1133](../choupi/src/heap.cpp#L1133); [choupi/src/context.cpp:592](../choupi/src/context.cpp#L592); [.agent/QKB.md:2069](../.agent/QKB.md#L2069).

### 23. Separate RAM reclamation from durable record deletion

Freeing a native wrapper does not automatically prove its persistent records are unreachable. The persistent reclaimer is opt-in experimental code, with full preflight, strict roots, bounded batches, and fresh cursors after mutation.

Its batches hold at most eight candidate tags; it refuses active transactions and only mutates after the cursor walk has returned. Keep it out of ordinary runtime collection until physical interrupted-erase recovery is established; host fault injection and the recorded physical functional probe do not close that gate.

Source: [choupi/src/persistent_inventory.cpp:228](../choupi/src/persistent_inventory.cpp#L228); [choupi/src/persistent_inventory.cpp:208](../choupi/src/persistent_inventory.cpp#L208); [.agent/QKB.md:11](../.agent/QKB.md#L11).

### 24. Treat zero-copy pointers and cursors as borrowed views

`fs_read_inplace()` returns a pointer into storage owned by the filesystem, and overlay mutations can move data. Consume a borrowed view before mutation or copy it to bounded owned memory; restart enumeration after writes/deletes.

```cpp
const uint8_t *view = nullptr;
uint32_t length = 0;
if (fs_read_inplace(tag, tag_len, &view, &length) != 0) return false;
// Read view[0..length) now; do not retain it across filesystem mutation.
```
The mutable-tag cursor explicitly excludes ROM defaults and is invalidated by mutation. This lifetime rule also applies to a USB receive buffer still owned by an in-flight operation.

Source: [src/choupi_pico2.cpp:1160](../src/choupi_pico2.cpp#L1160); [src/choupi_pico2.cpp:1116](../src/choupi_pico2.cpp#L1116); [src/ccid_device.cpp:909](../src/ccid_device.cpp#L909).

## Receiving commands and USB transport

### 25. Separate diagnostic CDC from the application protocol

The firmware exposes CDC diagnostics plus a CCID interface with separate bulk endpoints. Another application can use the same separation, but should advertise descriptors and transfer limits matching its actual protocol.

The composite configuration has CDC on interfaces 0/1 and CCID on interface 2, with CCID endpoints `0x03`/`0x83`. The build removes the SDK CDC-only descriptor object so the application provides exactly one composite descriptor set; merely enabling USB stdio does not create your binary protocol.

Source: [src/ccid_device.cpp:44](../src/ccid_device.cpp#L44); [src/ccid_device.cpp:76](../src/ccid_device.cpp#L76); [CMakeLists.txt:278](../CMakeLists.txt#L278).

### 26. Give TinyUSB one owner and defer expensive command processing

Keep endpoint state transitions in one execution context and use callbacks to record bounded work. The current build disables the SDK USB background task and dispatches command processing from the main loop.

```cpp
// Current architecture, simplified:
for (;;) {
    tud_task();
    choupi_ccid_task(); // consumes pending message outside receive callback
    watchdog_update();
}
```
Some callback comments still describe an IRQ worker; the active build's background-task definition and main loop establish current ownership. If you introduce IRQ or multicore producers, use an appropriate synchronized queue; `volatile` alone is not a general inter-core synchronization protocol.

Source: [CMakeLists.txt:275](../CMakeLists.txt#L275); [src/ccid_device.cpp:974](../src/ccid_device.cpp#L974); [src/choupi_pico2.cpp:1586](../src/choupi_pico2.cpp#L1586).

### 27. Validate complete frames before dispatch and avoid overflow

USB packets are fragments of a logical message, so collect the header, validate the advertised size, and dispatch only a complete bounded frame. For new parsers, use subtraction-based capacity checks so an attacker cannot wrap a length addition.

```cpp
// Adaptation: header length and available storage are known locally.
constexpr size_t header = 10;
if (capacity < header || payload_length > capacity - header)
    return false;
const size_t expected = header + payload_length;
if (received < expected) return true; // valid partial frame; await more
if (received != expected) return false; // reject trailing bytes by policy
```
Here `capacity`, `received`, and `payload_length` are application parser inputs. The existing CCID assembler illustrates header/fragment accounting, but its `payload_length + 10` check should not be copied as an overflow-proof validation pattern.

Source: [src/ccid_device.cpp:938](../src/ccid_device.cpp#L938); [src/short_apdu.hpp:18](../src/short_apdu.hpp#L18).

### 28. Reuse a checked command parser and respect response capacity

Parse command variants explicitly and keep input length distinct from requested output length. The short-APDU parser rejects extended frames and treats an encoded Le of zero as 256.

```cpp
#include "short_apdu.hpp"
pico2_jcvm::ShortApduCommand parsed;
if (!pico2_jcvm::parse_short_apdu(command, command_len, parsed))
    return false;
const uint16_t input_len = parsed.incoming_length;
// Use expected_response_length only when has_expected_response_length is true.
```
The response-chain helper bounds each copy by pending bytes, host request, and output capacity. For paginated application results, bind continuation state to the original query and clear it when the session/query changes.

Source: [src/short_apdu.hpp:18](../src/short_apdu.hpp#L18); [src/short_apdu.hpp:71](../src/short_apdu.hpp#L71); [.agent/QKB.md:148](../.agent/QKB.md#L148).

### 29. Handle exact USB packet boundaries and buffer ownership

A nonempty bulk response whose size is an exact multiple of 64 bytes may require a zero-length termination packet in this protocol. Keep the receive buffer reserved until the final response completes, including any intermediate time-extension packets.

```cpp
constexpr bool needs_zlp(size_t n) {
    return n != 0 && n % 64 == 0;
}
static_assert(needs_zlp(64));
static_assert(!needs_zlp(63));
```
The repository's 54-byte APDU plus 10-byte CCID header once stalled GET STATUS at exactly 64 bytes. Its completion handler submits the pending ZLP, then rearms OUT; a time-extension completion deliberately does not rearm OUT.

Source: [src/ccid_device.cpp:55](../src/ccid_device.cpp#L55); [src/ccid_device.cpp:326](../src/ccid_device.cpp#L326); [.agent/QKB.md:887](../.agent/QKB.md#L887).

### 30. Keep serial output bounded and binary data out of text translation

USB logging can wait on a slow or absent host and interfere with watchdog/transport timing. Use bounded startup waits and batched diagnostics; use the binary interface for payloads or explicitly control text translation when designing CDC binary traffic.

The boot path polls for a finite 120 attempts, then continues, and the main loop publishes periodic health output. `reference/silicon.md` recommends `stdio_put_string(buf, len, false, false)` for batched SDK stdio and warns about CRLF translation; these are design notes, not a measured optimization in the active JCVM runtime.

Source: [src/choupi_pico2.cpp:1536](../src/choupi_pico2.cpp#L1536); [reference/silicon.md:125](../reference/silicon.md#L125); [reference/silicon.md:168](../reference/silicon.md#L168).

## ARM optimization and smaller firmware

### 31. Use RP2350 hardware acceleration through the SDK

The device SHA-256 path acquires the hardware with a deadline, streams bounded blocks, and finishes into a fixed result buffer. Keep a separate portable implementation for host tests, and check accelerator ownership and error returns.

```cpp
#include "pico/sha256.h"
pico_sha256_state_t state{};
if (pico_sha256_start_blocking_until(
        &state, SHA256_BIG_ENDIAN, false,
        make_timeout_time_ms(100)) != PICO_OK) return false;
pico_sha256_update_blocking(&state, bytes, length);
sha256_result_t result{};
pico_sha256_finish(&state, &result);
```
This function-body sketch assumes valid `bytes`/`length` and normal completion; all post-acquisition failure paths must release accelerator ownership. The source links the SDK SHA headers selectively and does not claim that CPU assembly would beat the peripheral.

Source: [choupi/src/native_sha256.cpp:76](../choupi/src/native_sha256.cpp#L76); [CMakeLists.txt:129](../CMakeLists.txt#L129).

### 32. Compile only the algorithms and features your product needs

Choose a narrow profile and exclude unused native modules from the source list, rather than relying only on runtime feature checks. Keep advertised capabilities consistent with the selected implementation.

```sh
./tools/build arm --profile minimal
./tools/build riscv --profile minimal
# Select a richer profile only when the application requires it.
```
CMake excludes optional sources from the initial glob and adds AES-CTR, SHA-256, and other algorithms only under capability flags. QKB records a real bug where the ARM source list still compiled AES-CTR in a minimal build; inspect generated build inputs as well as declarations.

Source: [CMakeLists.txt:51](../CMakeLists.txt#L51); [CMakeLists.txt:66](../CMakeLists.txt#L66); [.agent/QKB.md:2509](../.agent/QKB.md#L2509).

### 33. Optimize ARM from a measured hot path and linked output

Start from the wrapper's Release build, measure a representative workload, then inspect the resulting instructions, stack usage, spills, and memory traffic. Keep ISA-specific changes behind a portable interface and compare ARM and RISC-V correctness separately.

The reference notes suggest hoisting invariant setup and mode dispatch, batching peripheral work, and moving rare logging off hot paths. Treat RAM placement, DMA, inline assembly, higher clocks, and multicore execution as experiments requiring their own evidence; this repository does not establish a universal speedup or a production overclock setting.

Source: [tools/build:103](../tools/build#L103); [reference/silicon.md:129](../reference/silicon.md#L129); [reference/silicon.md:103](../reference/silicon.md#L103).

### 34. Separate application timing from raw primitive throughput

Measure baseline transport/VM overhead and the same payload repeatedly before attributing time to an algorithm. Record board, firmware hash, profile, workload, repetitions, and timing source with each result.

```cpp
#include "pico/time.h"
const uint32_t start = time_us_32();
run_fixed_workload(); // application-defined; validate its output
const uint32_t elapsed_us = static_cast<uint32_t>(time_us_32() - start);
```
A physical host-timed SHA-256 probe recorded 1,680.2 ms baseline and 1,979.5 ms for 50 calls, estimating **5.99 ms per 256-byte doFinal** after subtraction. That includes PC/SC and VM overhead and is not raw RP2350 SHA engine throughput; `perf_tips.md` build timings are a different measurement category.

Source: [progress.md:17874](../progress.md#L17874); [perf_tips.md:7](../perf_tips.md#L7).

## Data ownership, secrets, and hardware services

### 35. Use the platform random service and avoid repeated setup per byte

The native random path obtains one SDK random word for each four output bytes, with a different host implementation. Preserve that platform boundary rather than introducing a time-seeded pseudo-random generator for security-sensitive data.

```cpp
#include "pico/rand.h"
const uint32_t word = get_rand_32();
for (unsigned i = 0; i != 4; ++i)
    output[i] = static_cast<uint8_t>(word >> (8 * i));
```
This sketch assumes four writable output bytes. Existing use is evidence of the selected entropy interface, not certification of an end-to-end cryptographic random subsystem or a reason to skip product-specific entropy requirements.

Source: [choupi/src/native_random.cpp:45](../choupi/src/native_random.cpp#L45); [choupi/src/native_rsa_math.hpp:44](../choupi/src/native_rsa_math.hpp#L44).

### 36. Wipe temporary secrets on all exits with RAII

Freeing a vector does not overwrite its secret bytes. The RSA code binds scratch buffers to a scope guard that calls `mbedtls_platform_zeroize()` on normal return and exception unwinding.

```cpp
#include "native_rsa_math.hpp"
std::vector<unsigned char> secret(256);
jcvm::RsaSensitiveBufferGuard guard(secret);
// Fill and consume secret without reallocating it while it holds secrets.
// Scope exit wipes its current live bytes.
```
The guard does not recover copies or abandoned storage from earlier vector reallocations, so size secret buffers before filling and avoid implicit copies. `scp03.cpp` supplies a volatile-byte zeroizer on Pico; its hard-coded demonstration static key is not a production key-provisioning design.

Source: [choupi/src/native_rsa_math.hpp:23](../choupi/src/native_rsa_math.hpp#L23); [src/scp03.cpp:15](../src/scp03.cpp#L15); [src/scp03.cpp:22](../src/scp03.cpp#L22).

### 37. Give independent persistent owners stable identifiers

Do not derive durable ownership solely from a class or array position when multiple instances can share that class. Persist a stable owner mapping and scope deletion to that owner.

The current CAP metadata records a unique owner per applet instance and restores the mapping after restart. For another application, use the same principle for tenants, device profiles, jobs, or channels, and verify that deleting one owner cannot remove another owner's records.

Source: [.agent/QKB.md:846](../.agent/QKB.md#L846); [src/cap_load.cpp:1097](../src/cap_load.cpp#L1097); [src/choupi_pico2.cpp:1368](../src/choupi_pico2.cpp#L1368).

### 38. Implement status indication without blocking the application loop

Use PWM plus a periodic task or short alarm callback for heartbeat and activity indication. Obtain the LED pin from board definitions so the same module can compile for boards without that pin.

```cpp
#if defined(PICO_DEFAULT_LED_PIN)
gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_PWM);
const uint slice = pwm_gpio_to_slice_num(PICO_DEFAULT_LED_PIN);
pwm_set_clkdiv(slice, 30.0f);
pwm_set_wrap(slice, 4095);
pwm_set_enabled(slice, true);
#endif
```
The existing module calculates brightness with integer arithmetic and uses a 120 ms alarm to end activity. Alarm/IRQ callbacks should remain small; if adding shared state or multicore users, review synchronization and PWM slice/channel sharing for your selected pins.

Source: [src/reader_led.cpp:60](../src/reader_led.cpp#L60); [src/reader_led.cpp:23](../src/reader_led.cpp#L23); [src/reader_led.cpp:32](../src/reader_led.cpp#L32).

## Validation, physical workflow, and troubleshooting

### 39. Test corruption, interrupted writes, and restart recovery offline

Model flash constraints and inject failures between mutations before relying on physical recovery. Test old-or-new snapshot recovery, migration, full-capacity writes, and retry behavior after reboot.

```sh
./tools/test-rp2350-fs-overlay
./tools/test-persistent-transaction
./tools/test-persistent-inventory
./tools/test-short-apdu
./tools/test-cap-parser
```
These wrappers cover distinct layers; choose the tests relevant to a change. The overlay model injects simulated cuts, but actual USB power interruption and recovery of the persistent reclaimer remain separate physical acceptance work; no tests were rerun to author this guide.

Source: [tests/unit/rp2350_fs_overlay/rp2350_fs_overlay_test.cpp:66](../tests/unit/rp2350_fs_overlay/rp2350_fs_overlay_test.cpp#L66); [.agent/HANDOFF.md:146](../.agent/HANDOFF.md#L146); [.agent/QKB.md:11](../.agent/QKB.md#L11).

### 40. Flash the intended image and require finite runtime evidence

Build both architectures before flashing, identify the board, and distinguish a smoke pass from an application test. `cycle arm`/`cycle riscv` flash smoke firmware, whereas `cycle choupi` flashes the full ARM JCVM and checks a finite runtime log.

```sh
# Each USB command requires host-access execution with elevated sandbox
# permissions AND this environment flag; do not first try restricted access.
PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile baseline --serial CHIP_ID
PICO2_HOST_USB_ACCESS=1 ./tools/monitor --seconds 8
```
Replace `CHIP_ID` with an identified board serial; the cycle itself builds both targets and selects an 18-second monitor for JCVM. A boot `TEST:PASS choupi_runtime` does not prove protocol correctness, algorithm vectors, installed-state recovery, or power-loss behavior; treat failure markers, timeout, and nonzero exit as failure.

Source: [tools/cycle:36](../tools/cycle#L36); [tools/cycle:57](../tools/cycle#L57); [AGENTS.md:28](../AGENTS.md#L28).

### 41. Diagnose USB visibility, serial nodes, and PC/SC separately

A visible USB device or working CDC terminal does not guarantee that a physical PC/SC reader has registered. Re-enumerate after reconnect, select the exact current reader by board identity, and keep discovery deadlines separate from command deadlines.

```sh
# Host-access execution only, with elevated sandbox permissions:
PICO2_HOST_USB_ACCESS=1 ./tools/doctor
# Only the explicitly permitted host helper, when needed:
sudo -n /usr/local/sbin/pico2-pcsc-reader
```
The helper can restore device ACLs after enumeration but cannot guarantee recovery from a stuck daemon or missing passthrough. Stop physical work if host access is unavailable; do not infer a firmware crash or reflash repeatedly from a missing reader, and do not change OTP, boot security, partition tables, or machine configuration.

Source: [.agent/QKB.md:873](../.agent/QKB.md#L873); [.agent/QKB.md:2435](../.agent/QKB.md#L2435); [AGENTS.md:47](../AGENTS.md#L47).

### 42. Report long-run resource failures and evidence limits explicitly

Short known-answer tests and support declarations do not establish long-duration memory stability or complete API correctness. Keep the latest full-run findings visible and distinguish simulated reset, process restart, hardware boot, and physical power interruption.

The 2026-10-31 `max_crypto` Security run completed 901 calls with **94 NO_MEMORY** and **one WRONG_RESULT**; Crypto completed 466 calls with **38 NO_MEMORY**, **three WRONG_RESULT**, and **one EXC_CARDRUNTIME**. These latest findings limit broader claims despite earlier successful narrower profiles; they do not by themselves identify the exhausted allocator/store.

For another application, record workload growth, allocator/overlay counters, completed operations, output correctness, firmware hash, and cleanup outcome. Treat earlier QKB entries marked fixed as fixes for their stated workload/revision, not proof that every later workload is clean.

Source: [.agent/HANDOFF.md:23](../.agent/HANDOFF.md#L23); [.agent/HANDOFF.md:3](../.agent/HANDOFF.md#L3); [progress.md:18241](../progress.md#L18241).

## Source map and caveats

| Source | Use |
| --- | --- |
| [Current handoff](../.agent/HANDOFF.md) | Latest board/profile outcomes and unresolved gates; read the most recent relevant dated entry. |
| [QKB](../.agent/QKB.md) | Practical fixes, failure signatures, and historical investigations; later sections can supersede compacted summaries. |
| [Progress log](../progress.md) | Dated measurements, commands, hashes, and archived log locations; ignored artifacts may not be available in another checkout. |
| [Firmware architecture](FIRMWARE_JCVM_ARCHITECTURE.md) | Runtime/transport/VM/platform boundaries; confirm numeric limits against current source. |
| [Flash migration](flash-layout-migration.md) | Layout evolution and preservation constraints. |
| [Supported profile](supported-profile.md) | Scope of Java Card compatibility and remaining acceptance work. |
| [Development timing](../perf_tips.md) | Feedback-loop measurement ideas and historical host timing samples. |
| [Board notes](../reference/board.md), [silicon notes](../reference/silicon.md) | Broader design ideas; not all are implemented or validated here, and older destructive/toolchain examples are not the repository workflow. |
| [AGENTS instructions](../AGENTS.md), [continuity](../.agent/SESSION_CONTINUITY.md) | Build wrappers, host access, hardware safeguards, and normal collaboration rules. |

This guide deliberately does not turn the reference notes' voltage/clock experiments, pin errata claims, destructive flash-sizing suggestions, or unrelated toolchain installation commands into validated recipes. The active runtime is primarily single-context cooperative code; this repository does not establish a general multicore architecture or an RTOS port for another application.
