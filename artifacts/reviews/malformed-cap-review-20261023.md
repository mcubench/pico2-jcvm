# Malformed CAP handling — code review (2026-10-23)

## Scope and trust boundary

Reviewed the current RP2350 ARM LOAD → flash store → INSTALL → JCVM path, including `src/cap_load.cpp`, `src/ccid_device.cpp`, `choupi/src/jc_cap/`, `choupi/src/jc_handlers/`, and `choupi/src/interpretor.cpp`. GP LOAD and INSTALL require an active MAC-verified SCP03 session at `src/ccid_device.cpp:377-413`. That limits who can upload a CAP; it does not establish that its contents are safe to parse. This is a **source review**. No malformed CAP was run on a Pico or in VPCD, so the exact observed device outcome remains unverified.

## Findings, in priority order

### 1. Critical — missing Class component can reach a null dereference

`src/cap_load.cpp:591-645` (`validate_cap_code`) requires a Header but does not require Class, ConstantPool, or Method components. `CapLoadSession::finishLoad` (`src/cap_load.cpp:777-819`) stores and enables the CAP after that outer validation. An install method containing an object allocation can reach `bc_new` → `Heap::addInstance` → `Class_Handler::getInstanceFieldsSize` → `ConstantPool_Handler::getClassFromClassIndex` (`choupi/src/jc_handlers/jc_cp.cpp:547-564`), where `getClass()->claz()` has no null check. An MCU fault from this dereference will not be caught by INSTALL's C++ exception handler.

**Fix:** Validate required components and their cross-references before storing a CAP. Add null checks in VM resolvers so an older malformed stored CAP fails with a controlled VM status.

### 2. High — malformed Method handler count wraps the bytecode span

`jc_cap_method_component::methods()` (`choupi/src/jc_cap/jc_cap_method.hpp:124-135`) subtracts `1 + handler_count × sizeof(handler)` from the declared component size into a `uint16_t`, without proving the table fits. For a short Method payload and a large handler count, the subtraction wraps and reports a large methods array beginning beyond the component. `Method_Handler::getMethodFromOffset` (`choupi/src/jc_handlers/jc_method.cpp:59-84`) checks an offset against that false span, then `callMethod` reads a method header from it (`:93-131`). The Applet component's install offset is checked for presence but not against a validated Method body (`src/cap_load.cpp:647-689`).

**Fix:** Reject Method components smaller than the count field plus the full handler table. Validate each method offset and full normal/extended header against the component end.

### 3. High — bytecode fetch and branches lack method bounds

`Interpretor::run` (`choupi/src/interpretor.cpp:376-385`) fetches through `pc_t::getNextByte` (`choupi/src/jcvm_types/pc_t.hpp:74-88`), which dereferences a raw pointer and advances it. Multi-byte operand reads and `updateFromOffset` (`:93-113`) also lack bounds checks. A truncated instruction, malformed branch, or method without a terminating return can read adjacent flash and potentially fault. The five-second INSTALL timeout (`choupi/src/interpretor.cpp:396-400`) is checked only after the fetch; it cannot protect an invalid dereference.

**Fix:** Carry validated method bytecode bounds into each frame, check every fetch and operand width, and require branch targets to be valid instruction boundaries inside that method.

### 4. High — Import records are walked beyond their component

The loader does not validate the Import payload. `validate_cap_imports` (`src/cap_load.cpp:432-459`) iterates the declared count. `Import_Handler::getPackageAID` (`choupi/src/jc_handlers/jc_import.cpp:88-112`) advances by each record's unverified `AID_length`, without checking the Import component end. It can hand `getPackageIndex` (`:121-140`) a pointer outside the component. Its `catch (...)` handles C++ exceptions, not invalid memory access.

**Fix:** Parse Import records with an end pointer. Require every fixed prefix and AID to fit and the declared count to consume exactly the expected payload.

### 5. Medium — cyclic superclass references can stall an interpreter instruction

`Class_Handler::getInstanceFieldsSize` (`choupi/src/jc_handlers/jc_class.cpp:568-590`) follows superclass links in `while (true)` without cycle detection or a depth limit. A malformed self-reference reached by object allocation can remain inside one bytecode handler. The interpreter refreshes its watchdog once per opcode, so this path can end in a reset rather than a normal INSTALL failure. Other class-resolution paths already have depth limits.

**Fix:** Validate class hierarchy ordering/cycles at LOAD and add a bounded traversal here.

## Common cause and persistence impact

`validate_cap_code` checks outer component framing and the Header/Applet fields it reads, but not most inner structures. `JC_Cap` (`choupi/src/jc_handlers/jc_cap.cpp:53-203`) stores pointers into the CAP image and assumes the inner structures are valid. `JCVM_ARRAY_SIZE_CHECK` checks some declared indexes, not whether the declared array occupies bytes within its component. INSTALL's exception handling cannot recover from a hardware fault or interrupt an infinite loop inside one handler.

LOAD writes the raw CAP slot and enables the package before INSTALL performs import validation and executes the applet (`src/cap_load.cpp:785-819`, `:908-951`). A failed INSTALL can therefore leave a malformed package in flash. This review did **not** establish that such a package alone creates a boot loop; restoration and selection have additional checks.

## Remediation and verification plan

1. Add one bounds-checked CAP validator at LOAD over `(data, length)`: component uniqueness/presence, variable-length records, counts, offsets, and cross-component references. Reject before `choupi_store_cap_package`.
2. Harden `JC_Cap` and resolver entry points for absent or malformed components already persisted by older firmware.
3. Bound method entry, opcode/operand fetch, branches, and class hierarchy traversal.
4. Run malformed fixtures through PC VPCD first: absent Class, short Method with excessive handler count, Import count/AID overrun, truncated operand, branch outside a method, and cyclic superclass. Require a controlled failure and a successful subsequent valid APDU. Use the repository ARM and RISC-V build gates before any physical Pico test.

**Validation status:** static source inspection only; no build, fuzzing, VPCD execution, flash, or hardware reproduction was performed for this review.
