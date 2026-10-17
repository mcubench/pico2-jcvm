# Project issue knowledge base

Use this file as the recurring issue checklist. Record the observed symptom,
the reliable recovery, and any limits. Promote only reproducible fixes to the
viable-solution status; a single successful probe is evidence, not a universal
fix.

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
