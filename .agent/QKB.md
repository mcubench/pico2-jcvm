# Project issue knowledge base

## JCSystem shareable lookup requires a context-switch bridge — 2026-10-18

- **Cause:** `JCSystem.getAppletShareableInterfaceObject()` previously threw
  `NotYetImplemented`. The current runtime has one applet context and no
  applet registry or callback bridge for invoking a server applet's custom
  `getShareableInterfaceObject()` implementation.
- **Scoped behavior:** The native returns `null`, which is correct for absent
  server AIDs and for applets using `Applet`'s default implementation. Case 48
  verified null AID, unknown AID, and self lookup on the probe applet.
- **Limitation:** This does not implement custom shareable objects. Do not treat
  the null fallback as full Java Card applet firewall/shareable support; a
  callback requires explicit context ownership and switch/restore semantics.

Use this file as the recurring issue checklist. Record the observed symptom,
the reliable recovery, and any limits. Promote only reproducible fixes to the
viable-solution status; a single successful probe is evidence, not a universal
fix.

## JC305 SensitiveArrays invocation used the wrong embedded method token — 2026-10-18

- **Symptom:** A JC305 applet calling
  `SensitiveArrays.isIntegritySensitive(byte[])` received `6F00`; the emulator
  trace reported `exception=20 opcode=FE`. The native handler was present, but
  the call resolved to the wrong framework method.
- **Cause:** JC305 import tokens number the five `SensitiveArrays` static
  methods 0–4. The embedded framework class has a constructor at token 0 and
  numbers those methods 1–5. The framework remapper translated the class token
  but had no method-token mapping for `SensitiveArrays`.
- **Viable fix:** For the framework package and imported class token 22, map
  static method tokens `{0,1,2,3,4}` to `{1,2,3,4,5}`. Focused cases 42–47
  passed after this mapping; full VPCD JCSystem suite passed 50/50 in 138.900
  s. PC, ARM M33, and RISC-V builds passed.
- **Correction:** The earlier diagnosis that the embedded API did not export
  `SensitiveArrays` was incorrect. The class exists; the defect was its static
  method-token mapping.

## SensitiveArrays is intentionally unsupported when capability is false — 2026-10-18

- **Viable behavior:** `isIntegritySensitiveArraysSupported()` returns false;
  valid creation raises `SystemException.ILLEGAL_USE`; invalid type/memory
  raises `ILLEGAL_VALUE`; negative length raises `NegativeArraySizeException`;
  `assertIntegrity` and `clearArray` raise `ILLEGAL_VALUE` for non-sensitive
  objects. Null inputs retain `NullPointerException` behavior.
- **Validation:** Cases 42–47 cover the capability, query, creation, assertion,
  and clear behavior, including reason codes. Focused and full VPCD tests
  passed. No physical board was accessed.

## JCSystem.makeGlobalArray implementation — 2026-10-18

- **Viable fix:** `Heap` now owns a runtime-wide global array heap with tagged
  references. It creates transient `CLEAR_ON_RESET` arrays for type codes 1–5,
  resolves them from active applet heaps, and clears their contents on reset.
  Global array references are rejected from applet instance/static fields and
  array components. Invalid types raise `SystemException.ILLEGAL_VALUE` and
  negative lengths raise `NegativeArraySizeException`.
- **Validation:** Probe cases 36–41 cover byte-array access and transient
  classification, invalid type/length, reference-storage rejection, Object[]
  null entry, and applet-object store rejection. PC, ARM M33, and RISC-V builds
  passed. Focused cases passed; full VPCD suite passed 44/44 in 144.901 s.
  No physical device was accessed.
- **Limits:** Only one applet context is modeled, so cross-applet calls and
  reset clearing through a reference retained across contexts are not directly
  exercised. The VM has no shareable-object ownership model, so it rejects
  non-null applet-owned objects in global Object[]. Global handles reserve one
  bit in the 15-bit array offset, limiting a heap to 16,383 array handles.

### Initial audit (superseded by the implementation above)

- **Finding:** The JC305 API specifies that global arrays are accessible from
  any applet context and that references to them cannot be stored in applet
  static fields, instance fields, or array components. The current runtime has
  one `Heap` per `Context`; handles are heap-local and do not tag global arrays.
- **Decision:** Do not implement `makeGlobalArray()` by merely creating an
  ordinary `CLEAR_ON_RESET` transient array. That would appear to work only
  inside one context while violating cross-applet visibility, lifetime, and
  storage restrictions.
- **Required design:** Add a JCRE-owned global array store/handle class that
  remains resolvable from each active context, clear global arrays on reset,
  and reject storing global references in applet fields or reference arrays.
  Then map JCSystem's boolean, byte, short, int, and object type codes and
  validate invalid types with `SystemException.ILLEGAL_VALUE`.
- **Evidence/validation:** Inspected the API contract in
  `javacard/framework/JCSystem.java` and heap ownership/array-handle code in
  `Context`, `Heap`, and `JC_Array`. This was a read-only design audit; no code,
  build, or runtime test was performed.

## JCSystem.isAppletActive always threw NotYetImplemented — 2026-10-18

- **Cause:** The native method had no behavior despite this runtime having a
  registered/restored current applet and its AID in `Context`.
- **Viable fix:** For the current single applet context, read the supplied AID
  instance's encapsulated byte array and compare it with the current applet
  AID. Return false for null, malformed, unregistered, or nonmatching AIDs;
  return true only for the registered/restored applet that matches the active
  context.
- **Evidence:** Case 35 checks the current AID and a well-formed unknown AID.
  Focused VPCD passed; full suite passed 38/38 in 221.009 s. PC, ARM, and
  RISC-V builds passed. No physical device was used.
- **Scope:** This does not report applets active on other logical channels or
  model multiple applet contexts. The runtime currently exposes only the
  basic channel and selected context.

## JCSystem object-deletion request silently succeeded while unsupported — 2026-10-18

- **Cause:** `JCSystem.isObjectDeletionSupported()` correctly returned false,
  but `requestObjectDeletion()` was a no-op. The supplied Java Card API
  specifies `SystemException.ILLEGAL_USE` when deletion is unsupported.
- **Viable fix:** Carry the pending VM-raised `SystemException` reason in the
  active `Context`; consume and reset it during Java exception dispatch. The
  deletion request sets reason 6 (`ILLEGAL_USE`) and throws the mapped
  `SystemException`.
- **Evidence:** Case 34 caught the exception and checked reason 6. Focused
  VPCD passed; the full JCSystem VPCD suite passed 37/37 in 173.290 s. PC, ARM,
  and RISC-V builds passed. No physical device was used.
- **Test-harness note:** When a new APDU case is added, rebuild the generated
  probe CAP with `./tools/build-jcsystem-transaction-probe` before running the
  harness. Otherwise a stale CAP can execute unrelated bytecode and fail at
  `6F00`, making it appear the native path is broken.

## Restricted-context USB access — complete fail-fast coverage — 2026-10-17

- The first guard covered the usual `flash`, `monitor`, and physical JCAlgTest
  commands, but missed direct-libUSB `bridge-pico-ccid-vpcd` and
  `repair-jcalgtest-raw-ccid` entry points. `tools/cycle` also started both
  target builds before its nested `flash` wrapper rejected the restricted
  context, wasting build time before failing.
- Added one Python `NoNewPrivs` guard shared by both direct-libUSB tools and
  moved the existing shell guard to the start of `tools/cycle`, before builds.
  All known USB/PCSC entry points now have a fail-fast check: flash, monitor,
  cycle, physical APDU and memory probes, physical named-reader JCAlgTest,
  raw CCID bridge, and raw CCID repair.
- Validation in `NoNewPrivs: 1` context: all eight entry points exited with
  the explicit host-access diagnostic in 0.75–1.52 seconds; none reached
  libusb/PCSC, and `cycle` did not build. Shell syntax and Python compilation
  checks passed. Use host-access execution for USB work from the outset; these
  guards are a fail-fast backstop, not a retry mechanism.

### Recurrence prevention update

- The wrapper guards originally failed open if `/proc/self/status` could not
  be read or did not contain `NoNewPrivs`. Both shell and Python guards now
  fail closed unless they can verify `NoNewPrivs: 0` before USB/PCSC access.
- These checks cannot intercept direct commands that bypass repository
  wrappers. The operating rule is therefore to select the host-access execution
  path on the first USB/PCSC command; never issue a restricted-context probe
  first. In the current restricted context, all eight wrapper entry paths
  reject in under one second, and a simulated unreadable proc status also
  rejects before any USB call.
- **Viable prevention:** Use host access from the outset, with fail-closed
  wrapper guards as a second layer. If host access is unavailable, leave
  hardware untouched and continue offline work.

## USB passthrough and Pico visibility

- **Symptom:** VirtualBox/host UI says the Pico is attached, but guest `lsusb`
  reports `unable to initialize libusb: -99`, or the Pico does not appear.
- **Viable recovery:** In the VM UI, detach and reattach the Pico USB device.
  This resolved the earlier mismatch where the host showed it connected but
  the guest could not see it. Then verify the guest enumeration once with
  `lsusb -d 2e8a:0009` (running CHOUPI) or `lsusb -d 2e8a:000f` (BOOTSEL).
- **Limit:** USB bus/device numbers change on re-enumeration. Re-run the
  authorized PC/SC ACL helper after each new running-mode USB node.

## PC/SC permission after USB re-enumeration

- **Symptom:** `lsusb` sees `2e8a:0009`, while PC/SC lists only virtual readers
  or does not list the Pico.
- **Viable recovery:** The user authorized this exact no-prompt helper in
  `AGENTS.md`: `sudo -n /usr/local/sbin/pico2-pcsc-reader`. It applies access
  to the current USB device node and restarts/reloads the PC/SC setup. A prior
  invocation printed `PCSC:ACCESS_GRANTED node=/dev/bus/usb/001/023` and
  `pcsc_scan -r` then listed the physical CHOUPI reader. Invoke only through
  the permitted host-access path; do not use any other sudo command.
- **Limit:** This VM session sometimes has `no_new_privileges`, causing the
  helper to fail from the restricted shell. A host-authorized elevated
  execution path worked previously. If the helper reports success but no
  physical reader appears, diagnose the service/driver without an unbounded
  scanner.

## `pcsc_scan` runaway CPU / VM saturation

- **Symptom:** PCSC reader checks hang, saturate CPUs, or leave `pcsc_scan`
  running after a timed client command.
- **Viable recovery:** Avoid `pcsc_scan` in automation. A stale process
  (`pcsc_scan -r`, PID 33195) was observed at 99.2% CPU and was terminated
  explicitly; this removed a major VM load. Use the repository's bounded Java
  probe/client or direct bounded CCID helper instead. If an old scanner is
  suspected, inspect `ps` first and stop only the identified stale scanner.
- **Limit:** `timeout pcsc_scan` did not reliably terminate the process in
  prior runs, so adding `timeout` is not sufficient protection.

## Physical JCAlgTest static SHA-256 repeat failure

- **Symptom:** JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` passes applet select,
  GETVERSION, JCSystem info, SHA-256 support, and the first measured
  `MessageDigest.doFinal()`, then the PC/SC reader becomes unavailable during
  subsequent repeated SHA calls. Clean attempts measured roughly 0.72 s for
  the first APDU; the run did not complete the operation.
- **Status:** Open. Isolated fresh-session SHA-256 prepare/doFinal APDUs passed
  (`AA9000`). The official physical static client failed again after cleanup
  and Pico reboot, so stale scanner load is not the only cause. A serial
  capture after a failed run showed `CHOUPI:WATCHDOG_RECOVERY ... cause=other`
  and retained diagnostic fields; this alone does not prove a hardware
  watchdog reset. Investigate APDU/CCID recovery and repeated SHA state on the
  card. Do not count unsupported algorithms (`NO_SUCH_ALGORITHM`) as failures.
- **Evidence:** `artifacts/logs/jcalgtest-static-physical-clean-20261017.log`,
  `artifacts/logs/monitor-jcalgtest-static-reset-20261017.log`, and
  `artifacts/results/jcalgtest-static-physical-20261017/`.

## Host/sandbox PC/SC context split — 2026-10-17

- **Symptom:** Default sandbox commands report libusb `-99` and Java
  SmartcardIO sees no physical reader, while USB and the reader are visible
  through the host-access execution context.
- **Viable resolution:** Run USB/PCSC commands through the host-access path
  (`require_escalated` in this agent interface); this is an execution context
  change, not another `sudo` command. In that context,
  `./tools/probe-jc-algtest` found Virtual PCD 00/01 plus the physical Pico and
  passed SELECT, GETVERSION `1.8.2_jc305`, JCSystem info, SHA-256 prepare and
  doFinal (`AA9000`), and expected unsupported RSA-SHA1 (`1203`). After every
  device re-enumeration, first run the specifically authorized
  `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper to refresh the node ACL.
- **Confirmed setup:** The Pico appears as `2e8a:0009`; USB interface 2
  advertises class `Chip/SmartCard`, endpoints `03`/`83`, and the installed
  libccid table maps VID/PID `0x2E8A`/`0x0009` to `Pico 2 CHOUPI CCID`.
  pcscd is socket activated and its idle auto-exit is normal. Do not infer that
  PC/SC is broken from a sandbox-only empty reader list.
- **Limit:** A host-context `usbreset 001/057` succeeded but did not prevent
  the subsequent static test failure. `picotool reboot -F -u` reports no reset
  interface on this composite device; the bounded CDC 1200-baud fallback
  entered BOOTSEL, `picotool info -a` confirmed it, and
  `picotool reboot --application` returned to the current image without
  flashing.

## Fail fast before hardware commands in the restricted context — 2026-10-17

- **Cause:** This VM exposes two command contexts. In the default restricted
  context `/proc/self/status` reports `NoNewPrivs: 1`; USB/libusb and the
  physical PC/SC reader are inaccessible there. Repeating `picotool` or serial
  probes from that context only wastes time and returns misleading visibility
  errors. The host-access execution path reports `NoNewPrivs: 0` and reaches
  the same USB devices.
- **Viable prevention:** `tools/common.sh` now provides
  `require_host_usb_access`; the flash, serial monitor, physical APDU probe,
  and named-reader JCAlgTest wrappers call it before hardware access. A
  restricted-context invocation exits immediately with an instruction to use
  host access. The simulator mode remains allowed in the restricted context.
- **Validation:** The five restricted-context wrapper checks all exited in
  under one second with the expected message. The escalated path refreshed
  the ACL, flashed the ARM image, captured serial output, and ran the physical
  probes successfully. Always call hardware wrappers through the host-access
  execution path; do not first try the default context.

## JCSystem available-memory reporting — 2026-10-17

- **Implementation:** The Pico transient-memory provider reports Newlib
  `mallinfo().fordblks` plus the unused `_sbrk` range up to the linker heap
  limit. The RP2350 map places the main stack in Scratch Y, so this heap range
  does not overlap the main RAM heap. The persistent provider reports unused
  bytes in the 64 KiB writable overlay, returning zero if its layout or file
  index cannot accept more records. Scalar `getAvailableMemory` saturates at
  `0x7fff`; the jc304 two-short form returns the 32-bit byte count, high word
  first. The PC target keeps returning zero because it does not own the Pico
  persistent overlay or allocator.
- **Physical validation:** JCAlgTest 1.8.2 reports `getAvailableMemory` values
  through its JCSystem info APDU. The live JC305/JC304 probe passed on the Pico:
  persistent available `47,777` bytes and both transient types `325,352`
  bytes at the time of the query. The short persistent and deselect reports
  matched the saturated extended values. The applet's reset-RAM field adds
  benchmark-array sizes and is therefore not a raw scalar-memory result.
  `getMaxCommitCapacity()` and `getUnusedCommitCapacity()` remain zero while
  commit transactions are unsupported.
- **Build/runtime:** ARM and RISC-V wrappers passed. The ARM firmware flashed;
  an eight-second serial capture reported `TEST:PASS choupi_runtime` and
  `TEST:SUMMARY pass=1 fail=0`. The physical probe also passed SELECT,
  GETVERSION `1.8.2_jc305`, JCSystem info, repeated SHA-256, and expected
  unsupported RSA-SHA1 (`1203`). Artifacts are under `artifacts/logs/` with the
  `jcsystem-memory-accounting-continue-20261017` and
  `jcsystem-live-memory-continue-20261017` stems.

## Physical SHA-256 static repeat failure — 2026-10-17 update

- The official JCAlgTest 1.8.2 static client ran in the host-access context
  against exact reader `Pico 2 CHOUPI CCID [JCAlgTest CCID]
  (D92E743D5F640C0F) 00 00` (reader index 3). It passed applet SELECT,
  GETVERSION, and JCSystem info. Five zero-repeat baseline calls averaged
  `529.0`; the first measured 256-byte / 50-call SHA-256 doFinal APDU returned
  `AA9000` in `761 ms` (reported sample `260.00`). The next identical APDU
  failed with `SCARD_E_READER_UNAVAILABLE` and the official client exited 1.
- After refreshing the reader ACL, the isolated SHA probe returned
  `F105` (`CryptoException.ILLEGAL_USE`) following that static run. A full
  MCU reset through CDC-to-BOOTSEL and `picotool reboot --application`, with
  no flash write, restored the isolated probe (`AA9000`). This shows the
  failure depends on accumulated/runtime SHA state, but its precise cause is
  not yet known. Treat the reader error and subsequent `F105` as failures;
  `NO_SUCH_ALGORITHM` for other algorithms remains expected.
- A focused probe after a clean MCU reboot passed ten separate one-call SHA
  APDUs, one exact 256-byte/50-call static SHA APDU, and one more one-call SHA
  APDU (`AA9000` for all). This narrows the problem: one 50-call APDU followed
  by one call is healthy, while the official client fails during a further
  repeated measured call after its baseline sequence. The runtime/CCID path
  needs a closer comparison with the client's exact APDU order and repeat
  count. Probe evidence:
  `artifacts/logs/jcalgtest-sha-50call-followup-physical-20261017.log`
  (SHA-256 `afd4b7dab4483670cc5e44d39ac431047b6f15487e733c835f5296b71a2b183d`).
- Eight-second serial capture after the failed run showed seven
  `TEST:PASS choupi_runtime` samples and no `TEST:FAIL`/`FAULT`; CCID diagnostics
  showed repeated `GetSlotStatus` (`msg=65`) and no XfrBlock count. The capture
  contained one `WATCHDOG_RECOVERY cause=other` marker; it does not establish a
  watchdog reset. Evidence: `artifacts/logs/jcalgtest-static-physical-hostctx-20261017.log`
  (SHA-256 `f423bcbca20ebaf211c02fc393171938afaef65b632f00ccff44d9e1cf738b90`)
  and `artifacts/logs/monitor-jcalgtest-static-hostctx-20261017.log` (SHA-256
  `7fd345669b97c9ac62745f8d60e4ed5a8d4d8637eade79431cc2cd5bd2f69357`).
- No unbounded `pcsc_scan` process or active JCAlgTest process was found during
  the host-context checks. Continue to avoid `pcsc_scan` because of the
  previously observed runaway case.

## Intermittent reader recovery after MCU reboot — 2026-10-17

- After a second clean CDC/BOOTSEL/application reboot, the Pico enumerated as
  `2e8a:0009` at node `001/062` and the authorized ACL helper succeeded, but a
  host-context Java probe listed only the two virtual readers. The first
  helper call after this reboot was not sufficient; repeating it did not
  restore physical reader discovery.
- An attempted raw CCID endpoint transfer timed out (`libusb -7`) before an
  APDU was sent. The bounded serial monitor remained healthy
  (`TEST:PASS choupi_runtime`, no `FAULT`) and showed only CCID
  `GetSlotStatus` processing (`msg=65`, `xfr=0`). `pcscd --hotplug` could not
  signal the root-owned daemon (`Operation not permitted`).
- **Current status:** intermittent PC/SC/CCID availability after reset remains
  an open host/device interaction. A prior host-context probe did enumerate
  the physical reader and complete APDUs, so preserve the host-access context
  and helper workflow; use a VM USB detach/reattach if the physical reader
  remains absent. Do not run `pcsc_scan`.
- Evidence: `artifacts/logs/jcalgtest-sha-repeated-50-apdus-physical-20261017.log`,
  `artifacts/logs/jcalgtest-sha-rawccid-repeat-physical-20261017.log`, and
  `artifacts/logs/monitor-jcalgtest-ccid-timeout-20261017.log`.

## Static SHA repeat failure mechanism — 2026-10-17

- **Superseded by the resolution below:** At first, `F105` was attributed to a
  retained SHA input length. The native trace later showed `current=0` on all
  76 observed 256-byte appends, so this was not the root cause of the official
  reader loss. Earlier `F105` runs occurred after heap exhaustion/panic and
  are not evidence that normal SHA state cleanup overflowed.
- **Confirmed applet failure mechanism:** With the official v1.8.2 settings
  (256-byte input, 50 `doFinal()` calls), a repeated run can return `F105`
  while the MCU and CCID endpoint remain alive. The trace shows
  `CryptoException` reason `0005`; in this API that is
  `CryptoException.ILLEGAL_USE`. `Sha256Digest.doFinal()` throws that reason
  when `Sha256NativeBridge.update()` rejects appending input. The native
  bridge rejects it when the retained input length plus the new input exceeds
  its 2,046-byte limit; for a 256-byte input, the retained length was therefore
  above 1,790 bytes. This is an applet digest-state/overflow path, not a
  missing-algorithm result or a confirmed MCU reset.
- A focused physical probe now replays the official sequence: prepare the
  50-call settings once, send five zero-repeat baseline APDUs, then measure.
  In one run the first 50-call APDU returned `AA9000` and the next transmit
  failed with `SCARD_E_READER_UNAVAILABLE`. In a later run, all five baseline
  APDUs returned `AA9000`, then the first measured APDU returned `F105`. The
  concurrent serial capture records the exception and `xfr=20`, followed by
  successful `GetSlotStatus` handling and `TEST:PASS choupi_runtime`; thus the
  latest `F105` was delivered by the applet while the device remained alive.
- **Still open:** The evidence identifies stale/over-limit digest state as the
  direct cause of `F105`, but does not yet identify which reset/finalization
  path leaves that state uncleared. The earlier `SCARD_E_READER_UNAVAILABLE`
  may be a transport/client symptom of the same path, but that relationship is
  not proven. Keep the issue open until state cleanup is traced and both
  repeated APDUs complete successfully.
- **General lesson for future algorithms:** Test reusable crypto objects over
  multiple APDUs, after zero-repeat baseline calls, and after both successful
  and failed operations. Verify that native and Java state is reset/cleared on
  every completion and exception path. Distinguish applet status words from
  PC/SC reader errors; do not attribute either automatically to USB or the
  algorithm backend.
- Evidence: `artifacts/logs/jcalgtest-static-state-repro-20261017.log`
  (SHA-256 `dd713bfab8772a15d04661b9ebd244c8972cd3a2085d0b4f7d11ff08a0043ede`)
  and `artifacts/logs/monitor-alg-static-state-repro-20261017.log` (SHA-256
  `0c11622d7844e92db567de647d8b686321c2af87af92baa83e2041d8dedc6fa1`).
- A follow-up diagnostic sent `B034` again after `F105`, which allocated a
  new 2,048-byte transient array and stored the new digest reference, but the
  next measured APDU still returned `F105`. Reprepare alone does not recover
  the current failure. This shifts the next investigation toward how the VM
  reads/writes the digest object's array field and transient-array contents;
  it is not evidence that `reset()` alone is the only faulty path. Trace:
  `artifacts/logs/jcalgtest-static-reset-diagnostic-20261017.log` (SHA-256
  `462ac435d379a4615aa7b9ab719c9f9cdddc9aa78484977b750db0a205bd54dd`) and
  `artifacts/logs/monitor-alg-static-reset-diagnostic-20261017.log` (SHA-256
  `418169398bd3ccff8a300edb3d92021cb1614992d864142e2c33def19208d72d`).

## Resolution: static SHA repeat heap exhaustion — 2026-10-17

- **Root cause:** `JC_Instance::getField_Reference()` bypassed the persistent
  instance's already available `getFields()` cache and deserialized a new
  reference from flash on every `GETFIELD_A`. For the digest object's
  `input` field, each read rebuilt a 2,048-byte transient `JC_Array` wrapper;
  `Heap::addArray()` then copied and retained another wrapper in the applet
  heap. Repeated `doFinal()` calls therefore consumed heap in roughly 2,060-
  byte allocations until `malloc` failed. The second static APDU panicked
  partway through its 50 calls, and PC/SC reported the reader unavailable.
- **Diagnostic evidence:** Temporary native tracing showed `current=0` on all
  76 observed 256-byte appends. Measurement one completed; measurement two
  stopped after 26 further appends at `malloc 2060 failed to allocate memory`
  and `*** PANIC ***`. This disproves the earlier stale-length hypothesis.
  The instrumentation was removed before the final build/flash.
- **Fix:** Persistent reference reads now use `JC_Instance::getFields()`, which
  restores references once and returns the cached heap handle thereafter.
  This preserves reference identity and avoids per-call deserialization and
  heap growth.
- **Hardware validation:** ARM and RISC-V wrappers both built successfully;
  the non-instrumented ARM image was flashed. The physical focused probe
  passed five baseline calls, two consecutive official 256-byte/50-call SHA
  measurements, ten single-call repeats, another 50-call measurement, and a
  following single call (`AA9000` throughout). The official JCAlgTest 1.8.2
  `ALG_PERFORMANCE_STATIC` client also completed with exit 0. SHA-256
  `doFinal()` measured 250 invocations at `2.01 ms/op` average; SHA-256 update
  measured `2.50 ms/op`, and reset measured `3.13 ms/op`. No
  `SCARD_E_READER_UNAVAILABLE`, `F105`, or panic occurred during these SHA
  checks.
- **General lesson for future algorithms:** A persistent object may hold a
  transient array reference. `GETFIELD_A` must return the same live heap
  object throughout the applet context; deserializing such a field on every
  read duplicates transient arrays and leaks heap. Reuse the field cache for
  reference identity and test repeated stateful operations, not just one call.
- **Separate remaining result:** The full official client recorded 34
  `NO_SUCH_ALGORITHM` results (expected) and 1,866 `CANT_BE_MEASURED` rows,
  many with `6F00`, across non-SHA operation combinations. They were not
  addressed by this focused heap/reference fix.
- Evidence: native trace `artifacts/logs/monitor-alg-static-state-length-20261017.log`
  (76 append records; includes allocation failure/panic), focused physical
  pass `artifacts/logs/jcalgtest-static-cache-fix-physical-20261017.log`,
  official client `artifacts/logs/jcalgtest-static-cache-fix-official-20261017.log`,
  and post-fix serial `artifacts/logs/monitor-arm-persistent-reference-cache-20261017.log`.

## Interpret JCAlgTest follow-on `6F00` after unsupported prepare — 2026-10-17

- In the physical JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` output, the first
  non-SHA `6F00` appears for `RandomData.ALG_TRNG`. The immediately preceding
  prepare APDU (`B033`, specification `3`) returned data byte `03` with
  `9000`, meaning `CryptoException.NO_SUCH_ALGORITHM`. The client nevertheless
  sent a measurement APDU (`B042`, 50 repeats), which returned `6F00` and was
  labeled `CANT_BE_MEASURED`.
- `RandomData.ALG_SECURE_RANDOM` (specification `2`) immediately before it
  prepared with `AA9000` and its 50-call measurement completed `AA9000`.
  This supports the implementation path for the supported algorithm. The
  `ALG_TRNG` follow-on `6F00` is not evidence of a supported-operation bug;
  the explicit prepare status already identifies the requested algorithm as
  unsupported. Do not implement TRNG merely to eliminate this follow-on row.
- General rule for future JCAlgTest triage: inspect the prepare status before
  treating a later measurement `6F00` as a VM failure. If prepare reports
  `NO_SUCH_ALGORITHM`, classify the measurement as a consequence of unsupported
  setup. Investigate only operations whose prepare returned success, while
  continuing to treat unsupported algorithms as expected.
- Evidence: `artifacts/results/jcalgtest-static-cache-fix-physical-20261017/ALGTEST_log_1792223036058.log`, lines around 900–1130; run transcript
  `artifacts/logs/jcalgtest-static-cache-fix-official-20261017.log`.


## Correction: first residual static `6F00` is supported `setSeed` — 2026-10-17

- A second pass through the raw result log corrected the previous
  `ALG_TRNG`-first interpretation. The **first** failed measurement is
  `RandomData.ALG_SECURE_RANDOM.setSeed()` (specification 2, method 2): prepare
  returned `AA9000`; five zero-repeat `B042` baselines returned `AA9000`; the
  first 50-repeat `B042` measurement returned `6F00`. The next `ALG_TRNG`
  prepare (`B033`) also returned `6F00`, after the supported-path failure.
  Therefore the following TRNG rows in this run are not valid evidence that
  TRNG alone returns `NO_SUCH_ALGORITHM`.
- The SDK implementation of `RandomData.setSeed()` validates the buffer range
  then performs no state change for `ALG_SECURE_RANDOM`. The cause of its
  failure under 50 repeats is not established. The earlier section
  “Interpret JCAlgTest follow-on `6F00` after unsupported prepare” is
  superseded for this run.
- No new hardware test was possible in the current execution context: `lsusb`
  returned `unable to initialize libusb: -99`; `picotool` failed libusb
  initialization; and the permitted helper exited because the container has
  `no new privileges`. The captured client result is still available for
  diagnosis.
- General lesson: determine the first failing operation in chronological
  order. Later prepare or measurement statuses may be consequences of a prior
  supported-path failure and must not be classified independently.
- Evidence: `artifacts/results/jcalgtest-static-cache-fix-physical-20261017/ALGTEST_log_1792223036058.log`, first failing sequence around lines 1030–1130; SDK implementation `choupi/javacard-sdk/javacard-api/src/javacard/security/RandomData.java`.

## Fix supported RandomData.setSeed static failure — 2026-10-17

- **Root cause:** The uploaded JCAlgTest CAP uses JC305 `RandomData` virtual
  method token `2` for `setSeed([BSS)V`; the embedded API's public method table
  puts `nextBytes([BSS)S` at token `2` and `setSeed([BSS)V` at token `3`. There
  was no RandomData token translation. The void setSeed call therefore resolved
  to short-returning nextBytes. Its `sreturn` left one word on the caller's
  operand stack; the next loop iteration eventually overflowed at
  `GETFIELD_A_THIS` (`6F00`, exception id 2). The same order difference exists
  for `RandomData.OneShot`.
- **Fix:** For the JC305 security package, remap virtual method tokens for
  RandomData class tokens 14 and 32 using `{0, 1, 3, 2, 4, 5}`. This maps
  JC305 `setSeed` to the embedded void method and JC305 `nextBytes` to the
  embedded short-returning method. The mapping is in
  `choupi/src/jc_handlers/jc_cp.cpp`.
- **Hardware validation:** Both `./tools/build arm` and `./tools/build riscv`
  passed. The clean ARM firmware was flashed to the physical Pico 2. The
  focused secure-random probe passed the exact 50-repeat workload (`AA9000`),
  and official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` exited 0. Its
  setSeed measurement passed all 250 invocations at `0.36 ms/op` average.
  Official output recorded 40 `NO_SUCH_ALGORITHM` results, which are expected.
- **Remaining static-run triage:** The completed run recorded 1,855
  `CANT_BE_MEASURED` rows, including 95 measurement/preparation APDUs with
  `6F00`. The first `6F00` after the setSeed fix occurs at DES
  `Cipher_doFinal()` prepare (`B031`, `ALG_DES_CBC_NOPAD`, key length 64), not
  in RandomData. This is a separate unresolved failure; inspect its prepare
  and method-dispatch path before changing any cipher support. Do not infer
  that every `CANT_BE_MEASURED` row is a VM defect.
- **General lesson:** Cross-version CAP compatibility includes virtual method
  token order as well as class-token order. A return-type mismatch can leave a
  seemingly unrelated stack word and only fail on a later iteration. Verify
  imported and embedded method descriptors/order for both void and value
  returns when porting SDK APIs.
- Evidence: `artifacts/logs/jcalgtest-secure-random-setseed-clean-50-20261017.log`
  (SHA-256 `2d73aab519f2050881bcb7d80ce82fbea3cf0937cdccf121654d27c17a090b69`),
  `artifacts/logs/jcalgtest-static-randomdata-token-map-20261017.log` (SHA-256
  `266df057ddd8df658cd339ea1b61ae5916a6b2396ad9ba8cb592a71b5d9fda20`), and
  `artifacts/results/jcalgtest-static-randomdata-token-map-20261017/ALGTEST_log_1792234644321.log`.

## Isolate first DES static prepare failure — 2026-10-17

- The first DES `ALG_PERFORMANCE_STATIC` failure is specifically the prepare
  APDU `B0 31` for `ALG_DES_CBC_NOPAD`, 64-bit DES key, `Cipher_doFinal`; its
  payload is `00 11 00 01 FF FF 00 03 00 40 00 07 01 00 FF FF 00 02 00 32 00 01`.
  It returns `6F00` after 550 ms. The immediately preceding secure-random
  `setSeed` measurement completed successfully. Later DES prepare variants
  also return `6F00`.
- JCAlgTest's `prepare_class_Cipher()` sequence is `prepare_Key()` (build two
  DES keys and set their values), `Cipher.getInstance(ALG_DES_CBC_NOPAD)`,
  `Cipher.init()`, then secure-random input fill. The applet's `m_trng` field
  is initialized once as `RandomData.ALG_SECURE_RANDOM`; the prior TRNG
  unsupported result concerns a separate RandomData test and does not itself
  explain this prepare failure. The archived serial capture ends before the
  DES APDU, so it contains no exception frame for this failure.
- Added `--des-cipher-prepare` to `tools/JCAlgTestApduProbe.java` to replay
  only this exact prepare request. The repository wrapper compiled the probe,
  but the run could not transmit: Java SmartcardIO reported that the Pico
  reader was absent. The approved ACL helper also exited because this
  container has the no-new-privileges flag. No new physical result was
  obtained; root cause remains unclassified.
- Next: in the host-access context, run the focused probe while capturing a
  bounded serial trace. Use the exception frame to distinguish key creation,
  DES `setKey/getKey`, Cipher construction/init, and random input fill before
  changing behavior. Do not infer a DES algorithm bug solely from `6F00`.
- Evidence: official transcript
  `artifacts/logs/jcalgtest-static-randomdata-token-map-20261017.log`, raw
  result `artifacts/results/jcalgtest-static-randomdata-token-map-20261017/ALGTEST_log_1792234644321.log`,
  serial capture `artifacts/logs/monitor-jcalgtest-static-randomdata-token-map-20261017.log`.

## Fix JC305 Cipher virtual-token mismatch in static workload — 2026-10-17

- **Root cause:** The first DES CBC no-padding `Cipher_doFinal` prepare failed
  because JCAlgTest 1.8.2 was compiled against the JC305
  `javacardx.crypto.Cipher` export, whose virtual tokens differ from the
  embedded CHOUPI export. JC305 uses token 3 for `init(Key, byte)`; embedded
  CHOUPI uses token 1. The unmodified token dispatched to `getAlgorithm()`
  (token 3 in CHOUPI), returning a byte where the caller expected `void`.
  This left an operand on the caller stack and led to a later invalid field
  access and `6F00`. The mismatch was visible in serial dispatch traces and
  confirmed by comparing both `.jca` public method tables.
- **Related receiver/interface resolution:** DES key setup also exposed that
  `invokeinterface` must resolve a receiver class with its owning package's
  import table and compare resolved interface identities across packages.
  JC305 `DESKey` interface tokens 4/5 (`getKey`/`setKey`) also reverse the
  embedded export order. Both mappings are scoped to the uploaded JC305 CAP.
- **Fix:** Added the `javacardx.crypto.Cipher` class-token-1 virtual map
  `{0,6,3,1,2,7,4,5}` (token 0 preserves inherited `Object.equals`). The
  physical trace now shows `method_raw=3 method_mapped=1`; the exact failing
  prepare APDU returns `AA9000`.
- **Validation:** ARM and RISC-V wrapper builds passed. ARM firmware flashed
  to the Pico 2; bounded serial output reported `TEST:PASS choupi_runtime`
  without `TEST:FAIL` or `FAULT`. The focused `--des-cipher-prepare` probe
  passed. Official JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` exited 0 and
  completed. DES CBC no-padding `Cipher_doFinal` and `Cipher_init` measurements
  returned `AA9000`. Totals were 68 `NO_SUCH_ALGORITHM` and 1,824
  `CANT_BE_MEASURED`; these totals include unsupported or otherwise
  unmeasurable rows and do not imply 1,824 VM defects. The first later `6F00`
  is a distinct 3-key DES CBC ISO9797 M1 prepare; it remains unclassified and
  must be triaged by prepare status before changing support.
- **Reusable lesson:** Cross-version compatibility must compare each imported
  class's full virtual method table, including inherited tokens and return
  descriptors, against the embedded export. A `void`/value-return mismatch
  can corrupt caller stack state and surface as a later unrelated field error.
  For `invokeinterface`, resolve the receiver using its package context and
  compare canonical interface identity, not raw class-reference numbers.
- Evidence: exact probe `artifacts/logs/jcalgtest-des-cipher-token-map-fix-20261017.log`;
  official transcript `artifacts/logs/jcalgtest-static-des-cipher-token-map-fix-20261017.log`;
  raw log and CSV under `artifacts/results/jcalgtest-static-des-cipher-token-map-fix-20261017/`;
  bounded serial trace `artifacts/logs/monitor-arm-jcalgtest-static-des-cipher-token-map-fix-20261017.log`.

## JSR saved-PC storage and remaining static-suite OOM — 2026-10-17

- **Confirmed JSR correctness defect:** `Frame::old_pc_t` stored `pc_t&`, so
  saved return addresses aliased the live program counter. `Frame::savePC()`
  also appended a new list node on every `jsr` in a frame and never reused a
  slot after `ret`; its return index is only one byte. The fix stores `pc_t` by
  value, records the JSR fall-through address (`instruction_pc + 3`), reuses a
  slot after `ret`, and throws `StackOverflowException` if more than 256 slots
  are simultaneously required. `Frame::savePC()` and `Stack::savePC()` are no
  longer `noexcept` so this overflow is reported through VM exception handling.
- **Do not treat this as the complete ALG_PERFORMANCE_STATIC OOM fix:** after
  the JSR correction, the final physical run still panicked during the first
  50-call AES-128 CBC no-padding `Cipher_doFinal` measurement. Five baseline
  APDUs returned `AA9000` (average `654.6` ms); the measurement APDU then lost
  the reader. Serial ended with `malloc 44 failed to allocate memory` and
  `*** PANIC *** Out of memory`. Thus JSR saved-PC storage was a real VM defect,
  but it is not sufficient to explain the remaining memory exhaustion. The
  exact owner of the final 44-byte allocation has not been identified.
- **Temporary cipher-buffer experiment:** replacing the CBC vectors with
  fixed-size stack blocks removed the 256-byte allocation records, but an
  earlier image still exhausted memory at a 12-byte allocation. That
  experiment was reverted; do not infer that the vectors were the sole leak.
- **Validation:** ARM and RISC-V wrapper builds passed. The JSR-corrected ARM
  image flashed, bounded serial boot reported `TEST:PASS choupi_runtime`, and
  the focused physical DES prepare returned `AA9000`. The full official
  JCAlgTest 1.8.2 static suite did not complete; it reached the AES static
  measurement and panicked as described. `NO_SUCH_ALGORITHM` rows remain
  expected and were not treated as defects.
- **Next diagnosis:** instrument `Stack::push_Frame/pop_Frame` depth and
  `Heap::addInstance/addArray` counts around the repeated AES APDU to identify
  whether the 44-byte growth is a retained call frame, a Java object, or another
  allocation. Keep captures bounded; do not retry a run that panics without a
  newly flashed or reset image.
- Evidence: `artifacts/logs/build-arm-jsr-pc-slot-reuse-final-20261017.log`,
  `artifacts/logs/build-riscv-jsr-pc-slot-reuse-final-20261017.log`,
  `artifacts/logs/flash-arm-jsr-pc-slot-reuse-final-20261017.log`,
  `artifacts/logs/monitor-arm-jsr-pc-slot-reuse-final-20261017.log`,
  `artifacts/logs/jcalgtest-jsr-final-reader-probe-20261017.log`,
  `artifacts/logs/jcalgtest-static-jsr-pc-slot-reuse-final-20261017.log`, and
  `artifacts/logs/monitor-arm-jsr-pc-slot-reuse-static-final-20261017.log`.

## Physical ALG_PERFORMANCE_STATIC heap retention — 2026-10-17

- **Confirmed remaining OOM cause:** The RP2350 VM heap has no object
  reclamation. `Heap::addArray()` and `Heap::addInstance()` append owning
  `shared_ptr`s to index-addressed lists, and there is no removal/GC path.
  `Heap::clearTransientArrays()` clears transient array contents but does not
  remove array objects. The JNI implementation of
  `JCSystem.requestObjectDeletion()` is currently a no-op. This causes
  unreachable Java objects created by repeated JCAlgTest cipher calls to
  accumulate until the global allocator fails. The JSR saved-PC defect was
  independent and already fixed; this heap retention is the remaining static
  suite failure.
- **Physical evidence:** With temporary per-APDU counters on the named reader,
  the official 1.8.2 `ALG_PERFORMANCE_STATIC` run passed SELECT, GETVERSION,
  JCSystem info, SHA digest operations, and the five AES-128 CBC no-padding
  `Cipher_doFinal` baselines (average `566.4 ms`). The first 50-call measurement
  then returned `SCARD_E_READER_UNAVAILABLE`; the board recovered through its
  watchdog path. During cipher measurements, each 50-call operation added
  about 100 tracked arrays (roughly two per operation) and added instances as
  cipher/key objects were created. Before the failing AES measurement, the
  counters showed `arrays=594 instances=192 frames=0 frame_high=6`; the final
  APDU could not print its completion counters because allocation failed.
  SHA-256 `B041` repeat calls kept the tracked array count flat at 13, so the
  observed growth is in the cipher measurement path, not the SHA accelerator.
- **Stack ruled out:** After each completed APDU the interpreter frame count
  was zero, with maximum observed frame high-water `11`. The failure is not a
  retained interpreter call stack.
- **Safe-fix constraint:** Heap offsets are encoded directly in `jref_t` and
  instances/arrays can be retained by statics, object fields, persistent
  fields, APDU state, and active frames. Deleting list entries or resetting
  per APDU would invalidate live references. A correct fix needs a tracing
  collector or a carefully specified allocation lifetime/root scheme that
  scans every reference-bearing root and preserves/remaps indexed handles.
  Do not paper over this by clearing the heap or pooling arrays by size.
- **Validation and cleanup:** ARM and RISC-V wrapper builds passed with the
  temporary counters. The instrumented image booted and its reader probe
  passed. After evidence capture, the temporary counters were removed, both
  architectures rebuilt successfully, and the clean ARM image was flashed;
  bounded serial output reported `TEST:PASS choupi_runtime` with no
  `TEST:FAIL`/`FAULT`. The official static suite remains failed and must not be
  reported as passing. `NO_SUCH_ALGORITHM` remains expected.
- **Evidence:**
  `artifacts/logs/jcalgtest-static-memory-counters-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-memory-counters-static-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-memory-counters-post-failure-20261017.log`;
  `artifacts/logs/monitor-arm-jsr-memory-counters-cleanup-20261017.log`;
  and build/flash records with the `jsr-memory-counters` stem. Key SHA-256
  digests: official transcript `53d3175ab15d4ee0ea141486714863653d48b312f95f93384b65c3b47e39ab83`;
  in-run serial `af644823a7965a6ed3b4eead17f56450de516a3357f5dd12892ad07fd443ab1f`;
  post-failure boot `cda099b9774a3a6858b98489aab67a3b7c6f612d2073a53dcfb289e63dbaf21a`;
  clean firmware boot `8d87aaf5bf04667a739e3acd8c307777f2b262380d8151a3a820027af0c12236`.
  Raw client output and partial CSV are also archived at
  `artifacts/results/jcalgtest-static-memory-counters-20261017/`:
  `ALGTEST_log_1792239359688.log` (`bf13f1222f87ead5262db72c58ddbd58b9dc79dd5329269af0395270499c8022`) and
  `Pico_2_CHOUPI_CCID_[JCAlgTest_CCID]_(D92E743D5F640C0F)_00_00____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED_1792239359688__3b_80_01_81.csv`
  (`9180b734a6f04cd5fa3118089d90eb08ec9717d15c1133cf9171f277f9e26cd6`).

## Supported sweep heap exhaustion — 2026-10-17

- **Observed:** The physical `ALG_SUPPORT_EXTENDED` sweep again stopped at
  `SIG_CIPHER_DSA PAD_PKCS5 ALG_SHA_384` after 1,636 completed CSV rows. The
  concurrent serial log contains `malloc 12 failed to allocate memory` at
  `artifacts/logs/monitor-arm-ccid-sweep-repro-20261017.log:62594`; the client
  then reported `SCARD_E_READER_UNAVAILABLE`. This corrects the earlier
  interpretation that this capture had no allocation failure. No CCID transfer
  error counter or firmware fault was logged before the allocation failure.
- **Cause:** `CryptoException.throwIt()` constructs a new Java exception for
  every unsupported/error result. `Heap::addInstance()` appends every instance
  to the heap's owning list, and the current heap lifecycle has no object
  reclamation path; `clearTransientArrays()` only clears arrays. The increasing
  Java exception allocations therefore remain retained across the sweep and
  explain the finite-heap failure. The 12-byte failed allocation and the exact
  allocation count were not independently instrumented, so do not claim each
  failed 12-byte block maps one-to-one to a `CryptoException`.
- **Rejected workaround:** A Java static cached exception does not provide a
  runtime-owned live object in this VM. Static reference reads/writes route
  through persistent flash serialization; a 50-throw VPCD probe observed a
  different object identity at throw 12. The source change and failing probe
  were reverted. Do not cache JCRE entry-point exceptions in ordinary Java
  static fields.
- **Status:** Root cause is identified; repair remains open. Implement a
  VM-managed per-context runtime exception object or a correct object
  reclamation mechanism, then run the 50-throw VPCD regression and repeat the
  physical supported sweep. Keep unsupported algorithms as expected results.
- **Evidence:** Partial official client output is in
  `artifacts/logs/jcalgtest-supported-arm-ccid-recovery-counters-20261017.log`
  and its results directory; serial allocation failure is in
  `artifacts/logs/monitor-arm-ccid-sweep-repro-20261017.log`. The rejected
  Java-static experiment's converter and VPCD logs are
  `artifacts/logs/build-pc-exception-reuse-v2-20261017.log`,
  `artifacts/logs/build-arm-exception-reuse-v2-20261017.log`, and
  `artifacts/logs/jcsystem-exception-reuse-vpcd-v2-20261017.log`.

## InitializedMessageDigest.OneShot unsupported factory — 2026-10-17

- **Symptom:** The official JCAlgTest 1.8.2 `ALG_SUPPORT_EXTENDED` sweep
  returned `6F00` at `javacard.security.InitializedMessageDigest.OneShot`.
  The VM diagnostic identified `NullPointerException` at `INVOKEVIRTUAL`;
  JCAlgTest calls `OneShot.open(...)` and then `close()` inside a `try` that
  catches only `CryptoException`.
- **Cause:** `InitializedMessageDigest.OneShot.open(byte)` was a TODO stub
  returning `null`. This turns an unsupported algorithm into a later NPE and
  corrupts the rest of the sweep's result stream.
- **Viable fix:** Unsupported factories must throw their documented
  `CryptoException.NO_SUCH_ALGORITHM`; never return `null` for a Java Card
  object factory. The stub now throws reason 3. A focused applet call verifies
  the returned reason, and the physical official CSV records
  `InitializedMessageDigest.OneShot / ALG_SHA_256;no` without `6F00`.
- **Remaining limit:** Two full physical supported-algorithm sweeps stopped at
  the same request, `SIG_CIPHER_DSA PAD_PKCS5 ALG_SHA_384`, after 1,637 CSV
  rows. The first ended with `SCARD_E_READER_UNAVAILABLE`; the second did too.
  A 240-second concurrent serial capture during the second run recorded normal
  CCID traffic through 3,556 RX events / 2,909 transfers and recurring
  `TEST:PASS choupi_runtime`, with no applet `6F00`, CCID transfer error, or
  reset. A later analysis of the serial tail found `malloc 12 failed to
  allocate memory`, so this run did contain an allocation failure. Afterward
  USB still enumerated as `2e8a:0009`, but PC/SC
  listed only VPCD readers even after the authorized ACL helper. Reflashing the
  same ARM image restored PC/SC publication; the bounded physical applet probe
  then passed. This narrows the failure to CCID/PCSC recovery after the long
  sweep. The later malloc failure and unbounded Java instance retention explain
  the applet's finite-heap failure; PC/SC reader publication still also fails
  after the APDU error. Do not claim the full sweep passed.
- **Evidence:**
  `artifacts/logs/jcalgtest-supported-arm-oneshot-fix-20261017.log`,
  `artifacts/logs/monitor-arm-oneshot-unsupported-after-official-20261017.log`,
  `artifacts/logs/jcalgtest-physical-after-supported-20261017.log`,
  `artifacts/logs/jcalgtest-supported-arm-disconnect-repro-20261017.log`,
  `artifacts/logs/monitor-arm-supported-disconnect-repro-20261017.log`,
  `artifacts/logs/flash-arm-supported-disconnect-recovery-20261017.log`,
  `artifacts/logs/monitor-arm-supported-disconnect-recovery-20261017.log`,
  and `artifacts/logs/jcalgtest-physical-after-disconnect-reflash-20261017.log`.

## Runtime CryptoException roots and physical follow-up — 2026-10-17

- **Supported-sweep cause:** The runtime cache initially recognized only ROM
  class offset 234. `CryptoException` also resolves in the imported
  `javacard.security` package as class offset 233 under AID
  `A0000000620102`. That allocation bypassed the cache, so each throw created
  an instance retained by the append-only `Heap`. A 50-throw VPCD probe caught
  different refs (14 then 16) and failed its identity assertion even though
  the ROM view repeatedly returned ref 6. A dynamic export-token lookup was
  also unsafe because unrelated framework export tables can share AID
  `A0000000620101` and lack that token.
- **Viable fix:** Identify the pinned ROM and imported views by their
  package-AID/class-offset pairs, then maintain one VM-owned root per view in
  each applet `Heap`. Repeated throws within a view reuse the same object and
  run the constructor again to update the reason. The 50-throw VPCD test now
  passes, including reason checks and identity. All preceding transaction,
  SHA-256, JCSystem-memory, and expected-unsupported cases also pass.
- **Physical validation:** ARM and RISC-V builds passed. The ARM CHOUPI image
  flashed and its bounded cycle reported `TEST:PASS choupi_runtime`. The named
  physical JCAlgTest reader completed `ALG_SUPPORT_EXTENDED` with 4,520 result
  rows: 15 `yes`, 4,505 `no` (unsupported rows are expected). The run exited
  successfully. A concurrent four-minute serial capture had no `malloc ...
  failed`, `PANIC`, `FAULT`, or `TEST:FAIL`. This supersedes the earlier
  supported-sweep claim that the physical run remained incomplete.
- **Separate static-suite status:** A new physical `ALG_PERFORMANCE_STATIC`
  run reached a 250-call DES CBC `Cipher_doFinal` measurement (average
  `28.64 ms/op`) and then failed on the next
  `TYPE_DES LENGTH_DES3_2KEY ALG_DES_CBC_NOPAD Cipher_init()` request with
  `SCARD_E_READER_UNAVAILABLE`. Its bounded serial capture ended without
  `malloc <n> failed`, `PANIC`, `FAULT`, or `TEST:FAIL`; after the authorized
  ACL refresh, `pcsc_scan -r` again listed the Pico reader. This run does not
  establish that the prior static-suite heap issue is fixed: earlier evidence
  directly attributed its OOM to append-only cipher arrays (594 arrays and
  192 instances before failure). It adds a distinct reader-unavailable event
  without a confirmed firmware allocation failure. Static mode remains open.
- **Evidence:** `artifacts/logs/jcsystem-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/build-pc-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/build-arm-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/build-riscv-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/cycle-choupi-runtime-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/jcalgtest-supported-arm-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/monitor-arm-crypto-class-view-roots-20261017.log`,
  `artifacts/logs/jcalgtest-static-arm-crypto-class-view-roots-20261017.log`,
  and `artifacts/logs/monitor-arm-static-crypto-class-view-roots-20261017.log`.
- **Artifacts:** Supported CSV SHA-256
  `f90520551b76e8f90725d7df61fc46bf21a6c7e006b4a86949cc9911e5a5b951`;
  official transcript SHA-256
  `f17f7a8ce908a78a1919907859b493fdde5127d603781336d7506d5069513f59`;
  four-minute serial SHA-256
  `da8e6000bc3b27bdc341625beb673b05d4945f69e8db0e9a8ad1c62310e6123a`.

## RandomData.OneShot unsupported result and support-matrix audit — 2026-10-17

- **Cause:** `RandomData.OneShot.open()` was a TODO stub that threw
  `CryptoException.ILLEGAL_USE`. JCAlgTest's 1.8.2 support probe catches
  `CryptoException` and records its reason, so all six unsupported OneShot
  algorithm rows became `error(ILLEGAL_USE)` instead of `no`.
- **Viable fix:** An unimplemented factory must throw its documented
  `CryptoException.NO_SUCH_ALGORITHM`. The source now throws reason 3. A
  focused physical JCAlgTest VPCD APDU asserted response type `2B`, reason
  `03`, and `9000`; VPCD passed 11/11. ARM and PC builds passed. The official
  physical `ALG_SUPPORT_EXTENDED` sweep then completed all 4,525 rows with
  counts `15 yes / 4,510 no / 0 other`; all six OneShot rows are now `no`.
- **Physical health:** The named Pico reader remained available through the
  full run. Two bounded serial captures contained recurring
  `TEST:PASS choupi_runtime` and no `TEST:FAIL`, `FAULT`, `PANIC`, failed
  allocation, or `CHOUPI:CCID_ERRORS` record. ARM image flash and boot capture
  also passed. No RISC-V build or test was run.
- **AC4 audit finding:** The 15 physical `yes` rows all appear as `yes` in the
  jCardSim reference, but two are API false positives: `OwnerPINBuilder`
  returns `null` for `OWNER_PIN_X` and `OWNER_PIN_X_WITH_PREDECREMENT`, while
  JCAlgTest reports a factory as supported whenever it does not throw. These
  documented types need real implementations or a way to report absence
  without breaking the sweep; until resolved, do not mark the full
  source/results consistency check complete. Full matrix comparison and hashes
  are in `artifacts/results/jcalgtest-support-comparison-20261017.md`.
- **Separate limit:** `ALG_PERFORMANCE_STATIC` remains unresolved due the
  append-only heap and separate PC/SC loss from earlier runs. Do not infer that
  the supported-sweep fix resolves static-mode array retention.
- **Evidence:** `artifacts/logs/build-arm-randomdata-oneshot-unsupported-20261017.log`,
  `artifacts/logs/build-pc-randomdata-oneshot-unsupported-v2-20261017.log`,
  `artifacts/logs/flash-arm-randomdata-oneshot-unsupported-20261017.log`,
  `artifacts/logs/monitor-arm-randomdata-oneshot-unsupported-20261017.log`,
  `artifacts/logs/pc-vpcd-randomdata-oneshot-unsupported-20261017.log`,
  `artifacts/logs/jcalgtest-supported-arm-randomdata-oneshot-fix-20261017.log`,
  and two 240-second physical serial captures.
- **Hashes:** Physical CSV
  `f9c74aaa7d5b3a3e7fc30897a4fa167bd7cb70cb8c79c5ad1e948d9e0aa401bf`;
  transcript
  `0cb527b6fbd0886febc71e008b4a0f408f9af01f1ce54e3671d00409c563aa57`;
  jCardSim CSV
  `67d9feebf9756da69db034bab05cddd9acb38d31aa980888fa7b5db22ef1757b`.

## OwnerPINx dispatch and inherited-field resolution — 2026-10-17

- **Cause 1 — internal `invokespecial`:** The rommask encoded a
  `SuperMethodRef` with the method-local ordinal (`OwnerPINxWithPredecrementImpl.update`
  appeared as token 0), while the VM resolves a class virtual/package method
  token. Token 0 selected `Object.equals` and interpreted the call with the
  wrong argument count. Internal super refs must use the declaring class's
  public or package method table token; package tokens carry the package bit.
  The VM resolves the ref through the immediate superclass, then checks the
  receiver in the caller's operand stack before entering the callee.
- **Cause 2 — inherited instance fields:** `InstanceFieldRef` tokens are local
  to their declaring class, but object storage is one flat field vector. The
  VM previously used the token directly, so subclass field token 0 aliased a
  base-class field. Add each superclass's declared instance-field count while
  resolving the declaration chain. The regression exposed this when OwnerPIN
  treated the subclass's transient predecrement marker as its PIN array.
- **Cause 3 — virtual reset after a successful PIN check:** `OwnerPIN.check()`
  set the validated flag and then called overridable `reset()`. The predecrement
  subclass override called `super.reset()`, clearing validation immediately.
  Successful checks now restore the try counter directly without virtual
  dispatch.
- **Viable validation:** The full VPCD JCSystem/crypto/OwnerPINx regression
  passed all 16 checks (`TEST:SUMMARY pass=16 fail=0 elapsed_s=59.295`). After
  removing temporary diagnostics, focused case 13 passed again with response
  `030302000100000202000201030103009000` and summary `pass=3 fail=0`.
  `./tools/build-pc` and the ARM M33 build both passed. The PC build emitted 49
  existing Rust javacard-os warnings and two packed-member alignment warnings;
  ARM emitted no warnings. No physical board was used.
- **Boundary:** This dispatch/layout fix does not resolve the separate
  `ALG_PERFORMANCE_STATIC` append-only heap retention issue documented above.
- **Evidence:** `artifacts/logs/jcsystem-ownerpinx-full-vpcd-20261017.log`,
  `artifacts/logs/jcsystem-ownerpinx-cleanup-vpcd-20261017.log`,
  `artifacts/logs/build-pc-ownerpinx-cleanup-20261017.log`, and
  `artifacts/logs/build-arm-ownerpinx-cleanup-20261017.log`.

## JCAlgTest VPCD startup delay before support queries — 2026-10-17

- **Observed:** The desktop VPCD integration can install and SELECT the
  JCAlgTest 1.8.2 applet, then fail to receive GETVERSION. Three bounded runs
  timed out on that first post-SELECT APDU at 20, 90, and 30 seconds. The 90-
  second run used the release VM; the 30-second diagnostic run used the debug
  VM. No OwnerPINX support APDU was sent, so this is not evidence against the
  OwnerPINX implementation.
- **Trace finding:** JCAlgTest defers initialization until the first process
  APDU. When its large transient arrays fit, `AlgPerformanceTest` construction
  also creates `JavaCardAES`, whose constructor allocates four 256-element
  lookup tables and generates the S-boxes in interpreted Java bytecode. The
  debug trace was still in that S-box generation loop at index 91 when stopped.
  The VM's normal release trace was not enabled for the 90-second timeout, so
  the exact no-debug instruction count remains unknown.
- **Current handling:** Keep the small OwnerPINx VPCD behavior regression as
  the reliable functional check. Do not use this desktop JCAlgTest path as an
  official support result until GETVERSION completes. The physical support
  sweep has not been rerun after OwnerPINx implementation. The build was
  returned to release mode after tracing.
- **Evidence:** `artifacts/logs/pc-vpcd-ownerpinx-support-20261017.log`,
  `artifacts/logs/pc-vpcd-ownerpinx-support-retry-20261017.log`,
  `artifacts/logs/pc-vpcd-ownerpinx-debug-20261017.log`,
  `artifacts/logs/build-pc-ownerpinx-support-debug-20261017.log`, and
  `artifacts/logs/build-pc-ownerpinx-support-release-20261017.log`.


## Host-access invocation latch — 2026-10-17

- **Recurrence:** `NoNewPrivs: 0` is not sufficient evidence that a tool was
  launched in the host-access execution context. A command can otherwise reach
  libusb from a restricted execution and spend tens of seconds failing.
- **Viable prevention:** Every repository USB/PCSC wrapper now also requires
  `PICO2_HOST_USB_ACCESS=1`, in addition to a readable `/proc/self/status`
  reporting `NoNewPrivs: 0`. Invoke that single command with
  `sandbox_permissions: "require_escalated"` and the marker. If the marker is
  forgotten, the wrapper exits before calling libusb, PC/SC, or starting a
  build. Direct commands such as `lsusb` must themselves be issued with
  `require_escalated`; never probe them in the default context.
- **Limit:** The marker is a caller-side latch, not an escalation mechanism.
  It cannot make direct commands safe or elevate a wrapper. No USB command was
  run while validating this change.

## Heap reclamation design constraints — 2026-10-17

- **Handle stability:** `jref_t` stores a 1-based 15-bit offset into either the
  `Heap::arrays` or `Heap::instances` list. Erasing or compacting list entries
  changes the meaning of every later live reference. Any collector must leave
  stable slots (for example, null tombstones plus a free list) and may reuse a
  slot only after proving that no live reference can reach it.
- **Collection boundary:** `Stack` stores untyped raw words for locals and
  operands. During interpretation, popped references can also be held only in
  native C++ locals. Collection inside bytecode/native calls cannot safely
  discover all roots. The interpreter returns with an empty frame list after a
  completed APDU, which is the earliest practical initial safepoint.
- **Roots still missing from a simple APDU-boundary sweep:** `Context` owns
  applet registration/restoration, AID, and APDU references, while `Heap` owns
  the VM CryptoException roots. Static references are now cached in the heap,
  but the cache has no enumeration API for a collector. Persistent object
  fields are cached after first access and can contain transient-array
  references; reference arrays also need element tracing.
- **Safe next design step:** first add canonical VM roots for loaded static
  references (keyed by package and static token), and explicit root enumeration
  for `Context`/`Heap`. Then add mark traversal through typed instance fields
  and reference-array elements, treating live persistent objects as roots or
  enumerating their serialized roots. Only after that should stable-slot sweep
  reuse be implemented and stress-tested across APDU boundaries. Do not clear
  the heap per APDU, compact lists, or pool arrays solely by size.
- **Validation status:** This is a source-architecture review only. No code,
  build, hardware, or performance validation was changed or run for this design
  note. Existing physical SHA measurements establish the one-time cached field
  fix, but do not validate a future garbage collector.
- **Resolved additional retention/identity path:** Before commit `425602b`,
  `bc_getstatic_a()` called `FlashMemory_Handler::getPersistentField_Reference()`
  on every access, reconstructing duplicate heap wrappers and breaking
  reference identity. It now uses a per-heap static root cache keyed by package
  and field; the cache is scoped to context lifetime. Its remaining collector
  gap is that roots cannot yet be enumerated (see the inventory below).

## Heap-root inventory after static-cache implementation — 2026-10-17

- **Direct context roots:** `Context` holds the AID object, registered and
  restored applet references, APDU buffer and APDU object references, and an
  outgoing buffer reference. These must be surfaced to a collector while the
  context is live. Some have `shared_ptr` mirrors; those C++ owners must not
  outlive reclamation of their corresponding heap slot.
- **Direct heap roots:** runtime CryptoException references and cached static
  reference entries. The static cache currently has only keyed get/set methods,
  so add read-only enumeration before using it as a collector root set.
- **Graph edges:** Instance fields store a compact `jref_t` plus a
  `FieldType`; the `FIELD_TYPE_OBJECT` and array variants identify references.
  Reference arrays store compact `jref_t` entries. Primitive fields and arrays
  must not be interpreted as references. Persistent instance fields load
  lazily, and persistent reference-array reads deserialize children on demand;
  traversal must account for heap growth while loading them and must not walk
  the containers with invalidated iterators.
- **Safepoint requirement:** `jref_t` has no generation counter. Stack locals
  and operands are untyped words, and bytecode/native routines keep references
  in C++ locals. Collect only after the APDU interpreter call and its native
  caller frames have returned, with no Java frames and no outstanding native
  references. At that point the root return-value cache must also be empty or
  explicitly included. Context-held APDU references remain roots if retained.
- **Slot requirement:** Heap references encode a 1-based offset into separate
  array and instance lists. Never erase list nodes or compact either list.
  A tombstone/free-list scheme can preserve offsets, but a slot may be reused
  only after every Java, context, cached-field, array-element, and native root
  has been accounted for. `getArray`/`getInstance` must reject tombstones before
  reuse is enabled.
- **Decision:** The next safe implementation step is an explicit root visitor
  for `Context` and `Heap`, followed by typed object/array edge enumeration.
  First run marking without reclaiming slots and compare the marked live set
  against heap contents across APDU boundaries. Add tombstones and reuse only
  after that trace is complete. No collector or runtime test was run in this
  source-inspection step.

## Canonical roots for serialized static references — 2026-10-17

- **Cause:** `GETSTATIC_A` decoded the serialized reference from flash on every
  read. Each decode appended a fresh array or instance to the heap. Repeated
  reads therefore returned different handles and retained duplicate wrappers.
- **Viable fix:** Route `GETSTATIC_A` through `Static_Handler::getPersistentRef`
  and cache each restored handle in the owning `Heap`, keyed by resolved
  package ID and static-field number. `PUTSTATIC_A` updates the root only after
  the persistent write succeeds. The heap lifetime matches the selected
  applet context; this VM destroys that context on deselect, and GP DELETE
  deselects before deleting package state.
- **Regression:** JCSystem transaction-probe case 14 reads the install-written
  static object twice after restore and checks reference identity. The focused
  VPCD case returned `019000`; the full VPCD suite passed 17/17 in 109.117 s.
  PC and ARM builds passed. No hardware or RISC-V build was used.
- **Limit:** This only canonicalizes Java static references; it is not a
  collector and does not make heap slots reusable. If persistent transaction
  rollback becomes supported for static references, the in-memory root must
  also be reconciled on abort.


## Root and typed-edge visitor API — 2026-10-18

- **Implemented:** `Heap::appendRootReferences()` exposes cached
  static-reference roots. `Context::appendRootReferences()`
  adds AID, registration/restoration, APDU, and outgoing-buffer handles, and
  resolves context-held APDU `shared_ptr` mirrors back to stable heap handles.
- **Implemented:** `Heap::appendChildReferences()` follows only typed
  reference fields and reference-array entries. Walking persistent fields or
  arrays can deserialize children and append heap entries, so callers must use
  a work queue and tolerate heap growth; do not iterate a presumed fixed-size
  snapshot as though loading were pure.
- **Not implemented:** No mark state, APDU-boundary invocation, slot tombstones,
  reclamation, or slot reuse exists yet. These visitor methods do not inspect
  Java stack words or native locals. Callers must establish a quiescent boundary
  and account for stack return values and other VM-owned roots before any future
  reclamation. In particular, VM-owned roots that are not in the static cache
  are not surfaced by `Heap::appendRootReferences()` yet.
- **Validation:** PC, ARM M33, and RISC-V builds passed. Full VPCD
  JCSystem/transaction regression passed 17/17 in 78.115 s. No hardware was
  used. The first ARM build found a range-loop copy warning in the new context
  visitor; it was corrected to a const-reference loop before the passing ARM
  build.

## Mark-only typed heap traversal — 2026-10-18

- **Implemented:** `Heap::traceReachableFrom()` performs a work-queue walk from
  caller-supplied references, keeps array and instance marks separate, and
  deduplicates handles before traversing typed fields and reference-array
  elements. It checks reference offsets against current slot counts and grows
  the mark vectors when lazy persistent reads append objects during traversal.
- **Failure reporting:** Invalid handles and caught Java Card `Exceptions`
  increment separate counters and make the result incomplete. Host allocation
  failures are not swallowed. The returned totals reflect the heap after the
  walk, including objects loaded during edge traversal.
- **Limit:** This only traces the supplied roots and does not mutate or reclaim
  heap slots. It is not called by runtime code or yet validated against an
  actual complete root set. `Context` root enumeration currently includes
  static/context handles, but runtime exception roots are not yet exposed by
  the committed heap visitor; Java stack words and native locals also remain
  outside the visitor API. Never use this result to authorize reclamation.
- **Validation:** PC, ARM M33, and RISC-V builds passed. The full VPCD
  JCSystem/transaction regression passed 17/17 in 70.499 s. No board was
  flashed. The PC build retains pre-existing packed-member warnings.
- **Decision:** Next expose every VM-owned root and define a safe post-APDU
  boundary, then exercise the tracer diagnostically across repeated APDUs.
  Keep slot destruction and reuse disabled until root coverage and mark
  accounting are demonstrated.

## Runtime CryptoException handles are heap roots — 2026-10-18

- **Implemented:** The heap keeps separate reusable `CryptoException`
  instances for its ROM and imported security-package class views. The
  `NEW` factory path routes only matching package/class allocations to those
  slots. `Heap::appendRootReferences()` now includes each non-null runtime
  exception handle alongside cached Java static references.
- **Reason:** Repeated `CryptoException.throwIt()` calls must not append one
  permanent instance per call, and a future heap walk must retain the shared
  runtime-owned instances even though Java statics are not their owner.
- **Validation:** PC, ARM M33, and RISC-V builds passed. VPCD JCSystem suite
  passed 17/17 in 71.327 s, including the 50-throw stable-identity/reason
  check. No hardware was flashed. The root visitor itself is not yet invoked
  by the runtime tracer.
- **Limit:** This closes only the two runtime exception roots. Stack reference
  return values, active Java words, and native locals still need coverage or a
  proven quiescent boundary. Do not reclaim slots based on current tracing.
- **Decision:** Determine how to represent a typed root return value and prove
  the APDU caller has returned before running diagnostic mark accounting.

## Preserve top-level reference return values as typed roots — 2026-10-18

- **Implemented:** `Stack` now distinguishes a reference result from primitive
  top-level results. `ARETURN` stores the compact handle with a reference tag;
  short/int return paths clear the tag, and a new root frame clears the stale
  result state. `Context::appendRootReferences()` adds the cached reference
  result when one is present.
- **Reason:** The prior root-return cache stored only a `uint32_t`, so a
  reference returned to native code could not be distinguished from a
  primitive or included in the root set.
- **Validation:** PC, ARM M33, and RISC-V builds passed. Full VPCD JCSystem
  regression passed 17/17 in 70.323 s. No hardware was flashed.
- **Limit:** This covers only a top-level returned reference. It does not
  scan local/operand words in active Java frames or C++ native locals. A
  collector still requires a proven quiescent caller boundary.
- **Decision:** Locate the exact APDU call-return boundary and verify that the
  Java stack is empty and native references are out of scope before invoking
  mark-only tracing.

## Opt-in post-APDU heap trace — 2026-10-18

- **Implemented:** PC builds accept `PICO2_HEAP_TRACE=1` to enumerate roots
  and run the mark-only tracer after `choupi_vm_process_applet()` exits its
  APDU `try` scope. The interpreter and temporary native references from that
  scope have been destroyed; the diagnostic also requires `Stack::empty()`.
  Default PC behavior and embedded ARM/RISC-V code do not run this trace.
- **Observed:** The VPCD JCSystem suite produced 15 post-APDU trace samples.
  All reported `complete=1`, zero invalid handles, and zero edge-load
  failures; none were skipped for active Java frames. The final sample had
  8 roots, 6/16 arrays reachable, and 4/12 instances reachable. Lazy
  persistent reads caused heap totals to grow during the walk, confirming
  that the work queue follows newly loaded objects.
- **Validation:** PC, ARM M33, and RISC-V builds passed. VPCD regression
  passed 17/17 in 73.382 s with tracing enabled. No hardware was used.
- **Limit:** A complete mark result in this applet run does not prove every
  possible native/global root is accounted for, and unreachable counts are
  not yet evidence safe to clear. No slot was reclaimed or reused. The trace
  hook is diagnostic and remains opt-in.
- **Decision:** Audit remaining persistent/runtime root paths, add a focused
  test for root-set completeness, and only then design stable-slot tombstones.

### Global arrays are outside applet-heap tracing

- **Cause:** `jref_t` marks global arrays with the array bit plus an offset
  bit, but `Heap::traceReachableFrom()` previously treated every array handle
  offset as an index into the local applet heap. A global handle could
  therefore falsely mark an unrelated local array, and child traversal could
  walk a global array as if it were owned by the applet heap.
- **Viable fix:** Ignore global-array references when enqueueing local heap
  marks and when enumerating children. Global arrays are owned by the runtime
  global heap and follow the separate reset lifecycle.
- **Validation:** PC, ARM, and RISC-V builds passed. VPCD JCSystem cases 36–41
  passed with two trace passes after each APDU: 12 traces complete, zero
  invalid references, and zero edge-load failures. One local array was
  unreachable in cases 40–41, as expected; no slots were mutated. The focused
  log is `artifacts/logs/heap-global-tracer-focused-vpcd-20261018.log`.
- **Limit:** This fixes a tracer classification error; it does not establish
  complete roots for all native/global holders and does not authorize heap
  reclamation. Reclamation remains disabled.

### Repeated-walk validation

- The PC diagnostic accepts a repeat count from `PICO2_HEAP_TRACE`, clamped
  to 1–4. With value `2`, the VPCD JCSystem suite passed 17/17 in 96.618 s
  and produced 15 pairs of consecutive walks. Every one of the 30 walks was
  complete, with zero invalid references and zero edge-load failures; no
  pair's array or instance totals grew between pass 1 and pass 2.
- This rules out tracer-induced duplicate deserialization for the persistent
  graph shapes exercised by this probe. It does not prove canonical identity
  for every persistent reference-array/alias graph or prove root completeness
  for all applets. Repeated tracing is diagnostic only and has measurable
  overhead; leave it disabled in normal operation.

## Persistent reference-array child identity — 2026-10-18

- A focused VPCD probe exposed a separate identity defect: a persistent
  `Object[]` containing the same `byte[]` twice restored two different child
  handles (`first == second` was false). Persistent object references already
  had alias handling, but array references serialized each occurrence under a
  distinct field tag.
- Persistent arrays now retain their canonical flash tag after a successful
  write. Repeated writes at another reference location store an array-alias
  record; reads resolve that record back to the canonical array tag. Heap
  lookup by persistent tag also reuses a previously loaded instance or array.
- Focused case 15 passed with `019000`; the full VPCD JCSystem suite passed
  18/18 in 81.386 s. The two-pass diagnostic emitted 18 paired trace results;
  every walk completed with zero invalid handles and zero edge-load failures.
  No hardware was flashed or accessed.
- This verifies repeated array children in the exercised persistent array
  shape. It does not cover cyclic object arrays, all persistent field layouts,
  or safe slot reclamation. Keep tombstones and reuse disabled pending broader
  root and alias validation.

## Persistent instance fields sharing an array — 2026-10-18

- Added a second VPCD probe where two fields of a persisted holder refer to one
  byte array. With the holder fields typed `byte[]`, case 16 verified both
  `first == second` and a write through the second field observed through the
  first. Focused case passed `019000`; full suite passed 19/19 in 85.020 s.
  All 34 repeated heap walks completed with zero invalid refs or edge failures.
- An initial version declared the fields as `Object` and cast them back to
  `byte[]`; that APDU failed with `ClassCastException` at `checkcast` opcode
  `0x94`. Replacing the field declarations with their actual `byte[]` type
  made the identity/mutation probe pass. The cast failure is separate from the
  alias result and remains untriaged; do not treat the typed-field result as a
  fix for general `checkcast` behavior. Evidence is in
  `artifacts/logs/jcsystem-persistent-object-array-alias-vpcd-20261018.log`.
- No embedded source changed, no board was accessed, and no reclamation was
  enabled. Next isolate the cast failure, then add root/alias graph shapes
  before considering tombstones.

## Array `checkcast` type-code mapping — 2026-10-18

- A focused cast from `Object` to `byte[]` failed with `ClassCastException`
  (`opcode=0x94`). `docheck()` directly cast the bytecode's `atype` to
  `jc_array_type`; the bytecode encodes array types as decimal `10`–`14`, while
  the internal enum uses `0x10`–`0x14`. These representations do not match.
- Replaced the cast with an explicit mapping for boolean, byte, short, optional
  int, and reference arrays. Unsupported/invalid codes raise
  `SecurityException`; valid but mismatched array types still produce a
  failed type check.
- Focused VPCD case 17 passed (`019000`), including after case 16 had mutated
  the shared array. PC, ARM, and RISC-V builds passed. Full VPCD JCSystem suite
  passed 20/20 in 114.250 s; all 36 repeated heap walks were complete with
  zero invalid refs and zero edge-load failures. No board was accessed.
- This validates a positive byte-array cast. Add a negative cast regression
  for mismatched array types before treating the full `checkcast` family as
  covered. The explicit type mapping is the viable fix for this mismatch.

### Correction — catch and reuse VM-generated ClassCastException

- The first negative-cast regression exposed a second defect: `checkcast`
  threw the C++ `Exceptions::ClassCastException` directly, which bypassed
  Java exception-table dispatch and returned `6F00` even inside a Java catch.
- The bytecode now resolves `java.lang.ClassCastException` from the applet's
  imports, obtains a per-heap JCRE-owned instance, and sends it through the
  normal `doThrow()` handler search. The heap visitor includes this exception
  root. Repeated mismatched casts reuse the same instance.
- Case 18 passed 50 mismatched casts in one APDU, checking both catch behavior
  and stable exception identity. Full VPCD suite passed 21/21 in 110.937 s;
  all 38 repeated heap walks completed with zero invalid refs and zero
  edge-load failures. Final sample: arrays 6/16, instances 6/14. PC, ARM, and
  RISC-V builds passed; no hardware was accessed.
- This establishes catchable/reusable ClassCastException for this path. Other
  VM-generated runtime exceptions still need review for consistent Java
  handler dispatch.

## VM-generated Java runtime exception dispatch — 2026-10-18

- **Symptom:** `arraylength` on a null reference raised internal
  `Exceptions::NullPointerException`; the interpreter caught it as a VM error
  and returned `6F00`, even though the CAP had a matching Java catch handler.
- **Cause:** Bytecode implementations use C++ `Exceptions` values for runtime
  faults, but the interpreter's per-instruction catch path called the VM-fatal
  status handler directly. Those internal exceptions never became Java object
  references and never entered `Bytecodes::doThrow()`'s exception-table search.
- **Viable fix:** At the per-instruction catch point, map supported VM
  exception values to class export tokens in the applet's `java.lang` import,
  obtain one rooted reusable exception instance per imported class in the
  applet heap, and send it through `doThrow()`. Resume interpretation when
  Java finds a handler. Leave security/unmapped exceptions on the existing
  fatal path. Current tested mappings are index bounds, array index bounds,
  negative array size, null, class cast, arithmetic, and generic runtime.
- **Related bounds defect:** `JC_Array` delegated upper-bound errors to the
  generic `JCVMArray` backing store, which raised `IndexOutOfBoundsException`.
  All typed array read/write entry points now validate against the Java array's
  logical length and raise `ArrayIndexOutOfBoundsException` before touching
  storage. Keep this check at the Java array boundary; generic list/array
  container checks retain their own exception semantics.
- **Evidence:** Focused VPCD cases 19–22 caught null, array-index, arithmetic,
  and negative-size exceptions (`019000` for all). Case 18's 50 mismatched
  casts still passed. Full suite passed 25/25 in 104.317 s; 23 pairs (46 total)
  of heap trace walks were complete with zero invalid refs and zero edge-load
  failures. PC, ARM, and RISC-V builds passed. No board was accessed.
- **Limit:** This is not exhaustive. `ArrayStoreException`, VM security
  violations, and other exception values not listed in the mapping remain
  outside Java dispatch and must be audited separately. Heap reclamation stays
  disabled.

### Correction — dispatch ArrayStoreException

- JC 3.0.5's checked-in `java/lang/javacard/lang.jca` exports
  `ArrayStoreException` at token 11. The dispatcher now maps internal
  `Exceptions::ArrayStoreException` to that class. A VPCD probe assigned the
  active applet instance into an `Exception[]` through an `Object[]` reference;
  Java caught the VM-raised mismatch and returned `019000`.
- PC, ARM, and RISC-V builds passed. Full VPCD suite passed 26/26 in 116.840 s;
  24 pairs (48 total) of heap walks were complete with zero invalid references
  or edge-load failures. No board was accessed.
- The initial probe used `new Object()` and the PC emulator terminated with an
  uncaught `jcvm::Exceptions` before reaching the array store. Allocation of
  `Exception[]` and same-type `Exception` stores work, and using the active
  applet instance reliably exercises the mismatched store. **Open issue:**
  isolate and fix the `new java.lang.Object()` path; current evidence does not
  identify the throwing source. Logs:
  `artifacts/logs/jcsystem-arraystore-exception-dispatch-vpcd-20261018.log` and
  `artifacts/logs/jcsystem-arraystore-object-allocation-probe-vpcd-20261018.log`.
- The earlier note that `ArrayStoreException` was unmapped is superseded by
  this correction. Security violations and other unmapped exception values
  remain outside Java dispatch.

### Correction — dispatch JCSystem SystemException

- `JCSystem.getAvailableMemory(byte)` forwards an invalid type into the native
  binding, which raises internal `Exceptions::SystemException`. The Java class
  is in the imported `javacard.framework` package, not `java.lang`; its JC 3.0.5
  class export token is 13. Exception lookup now matches a requested package
  AID and token, and the SystemException path sets `reason` field 0 to
  `ILLEGAL_VALUE` (1) before Java handler dispatch.
- Case 24 passed through VPCD: type 3 was caught as `SystemException`,
  `getReason()` equaled `SystemException.ILLEGAL_VALUE`, and response was
  `019000`. Full suite passed 27/27 in 98.571 s. All 50 heap walks were
  complete with zero invalid references and zero edge-load failures. PC, ARM,
  and RISC-V builds passed; no board was accessed.
- This covers the invalid-type native path only. Other framework exception
  values and reason codes still need explicit mappings and tests. The generic
  VM exception enum does not carry reasons, so only paths with a known reason
  should synthesize one.

## Root java.lang.Object allocation terminated the emulator — 2026-10-18

- **Symptom:** A valid applet APDU containing `new Object()` terminated the PC
  emulator with `terminate called after throwing an instance of
  'jcvm::Exceptions'`. Allocating `Exception[]` and constructing other
  non-root classes worked.
- **Cause:** `Class_Handler::getInstanceFieldsSize()` used a `do/while` loop
  that always resolved one superclass. For root `java.lang.Object`, whose
  superclass reference is absent, that lookup raised `SecurityException`.
  The exception escaped through `JC_Instance` constructors declared
  `noexcept`, which caused termination before the interpreter's Java/VM
  exception handling.
- **Viable fix:** Walk class sizes with an explicit root check before resolving
  the parent. Remove `noexcept` from `JC_Instance` constructors that perform
  CAP lookup or class field sizing, preserving normal error propagation for
  invalid CAP metadata. Case 25 (`new Object()`) now passes (`019000`).
- **Regression evidence:** PC, ARM, and RISC-V builds passed. Full VPCD suite
  passed 28/28 in 102.270 s; 26 pairs (52 total) of heap walks completed with
  zero invalid references and zero edge-load failures. No board was accessed.
- The prior ArrayStoreException diagnostic about `new Object()` is superseded
  by this fix. It had been a constructor-path failure before the mismatched
  array store, not a dispatch failure.

## VM-raised APDUException bypassed Java catch handlers — 2026-10-18

- **Cause:** Internal APDU state-machine errors were represented by
  `Exceptions::APDUException`, but the VM-to-Java exception dispatcher had no
  Java Card framework mapping for that enum value. A handler importing
  `javacard.framework.APDUException` therefore could not catch native APDU
  state violations.
- **Viable fix:** Resolve the framework APDUException through export token 12
  and initialize its reason field to `ILLEGAL_USE` for the current internal
  APDU state violations. The focused probe invokes
  `setIncomingAndReceive()` twice, returns immediately after the catch, and
  reads the captured reason on a subsequent APDU. It passed with reason 1.
- **Test-harness note:** A first probe called `setOutgoing()` and then used the
  normal final response path; its `6F00` was from trying to send after the APDU
  state had already failed. Returning from `process()` and checking the stored
  catch result on the next APDU isolates exception dispatch from response
  transmission state.
- **Evidence:** PC, ARM, and RISC-V builds passed. Focused VPCD cases 26–27
  passed. Full VPCD suite passed 30/30 in 159.731 s. No physical board was
  accessed.

## Invalid transient-array clear event raised the wrong exception — 2026-10-18

- **Cause:** `getTransientClearEvent()` returned the two supported lifecycle
  events but raised internal `SecurityException` for any other value. The API
  contract specifies `SystemException.ILLEGAL_VALUE`, which is catchable by an
  applet.
- **Viable fix:** Raise internal `SystemException`; the VM's existing mapping
  supplies the `ILLEGAL_VALUE` reason. Case 29 passes for event value 3.
- **Coverage:** Case 28 creates byte, boolean, short, and object transient
  arrays with reset/deselect events; writes and reads values; and verifies
  `JCSystem.isTransient()` reports the requested event. Focused and full VPCD
  tests passed; the full suite passed 32/32 in 113.436 s. PC, ARM, and RISC-V
  builds passed. No physical device was used.
- **Lifecycle cause:** VPCD deselection destroyed the selected applet context;
  restoration rebuilt transient arrays with zeroed contents for both event
  types. This incorrectly cleared `CLEAR_ON_RESET` arrays on deselection.
  `Heap::clearTransientArrays()` existed but had no runtime call sites.
- **Viable lifecycle fix:** Retain and reuse the same applet `Context` for a
  same-AID reselection. After invoking the applet's `deselect()` method, clear
  only `CLEAR_ON_DESELECT` arrays. On card reset/power-off, additionally clear
  `CLEAR_ON_RESET` arrays. VPCD selection no longer invokes the reset clear
  callback. Cases 30–33 verify reset-array retention across deselect, deselect
  clearing, and both events clearing after card reset.
- **Evidence:** Focused lifecycle tests passed; full VPCD suite passed 36/36 in
  114.875 s. PC, ARM, and RISC-V builds passed. No physical device was used.
- **Scope:** The runtime has one selected applet context; this fix reuses that
  context when the same AID is selected again. Cross-applet context caching
  and shareable-interface behavior remain outside this verified path.
