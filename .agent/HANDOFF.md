# Session handoff — updated 2026-11-01

## Cross-context Shareable reference returns

The current proxy method dispatch now converts a returned server-local
instance into a client proxy when its class implements a Shareable interface,
roots the object in the server Context, and registers the proxy in the client
Context. Null and JCRE-global array/instance handles pass directly; local
arrays and non-Shareable objects fail closed. `invokeinterface` still accepts
only zero-argument methods. Typed void/short/int and reference results are
tracked by `Stack::ReturnKind`.

Validation: `./tools/test-rmi-registry` passes; the PC `current`, ARM `current`,
and RISC-V `current` builds pass; elevated VPCD multipackage test passes
47/47. This integration suite does not use a proxy. An immediate authorized
reader refresh returned `found 0` despite the user's report that physical
boards are connected; no flash or serial monitor was attempted. The existing
upstream CAP set and source search did not find a positive SIO probe. No CAP
was created or modified.

Next: continue searching for an existing upstream Shareable service CAP. If
none exists, keep positive-call acceptance blocked by the CAP-only test
constraint; proceed with bytecode-level argument descriptor checks and preserve
fail-closed behavior until conversions are testable. Then validate on a
physical board once the authorized reader helper enumerates it.

## Proxy cast validation and integration recheck

Proxy `checkcast`/`instanceof` now verify both that the target is an
`ACC_SHAREABLE` interface and that the server object's actual class implements
it. This avoids treating every Shareable interface as implemented by each
proxy. `./tools/test-rmi-registry` and final PC/ARM/RISC-V builds pass. The
authorized VPCD multipackage suite passes 47/47; the transaction probe case 48
passes 3/3 from the preceding final-build validation. None of these probes
invokes a positive Shareable service. Source/deploy inspection found no
available positive fixture; keep CTX-01 open. No physical access was used.

The latest firmware size increased slightly due to the cast/runtime checks;
ARM image end address is `0x100e05d8`. The current source still supports only
zero-argument Shareable calls and void/short/int/null results; arguments and
nonnull server-local references fail closed. Next inspect whether an existing
upstream applet exercises Shareable export; do not create or modify CAP files.

## Shareable proxy invocation groundwork — current worktree

Native Shareable lookup from the prior step now has a guarded `invokeinterface`
path. A proxy call requires the target interface to carry `ACC_SHAREABLE`,
resolves the implementation against the actual server object, and executes it
through nested `Interpretor` in the server context. This step supports only
zero-argument service methods. Typed return tracking transfers void, short and
int values across contexts; null references transfer safely, while nonnull
server-local reference returns are rejected until proxy translation is added.
All arguments currently fail closed.

Ordinary access to proxy implementation details is blocked: virtual/special
calls, all direct instance-field opcodes, and casts outside Shareable interfaces
are denied. This has not been tested with a positive two-applet fixture; the
existing upstream transaction probe only verifies absent/unknown/default-null
lookup behavior. No CAP was changed. Physical devices are reported connected,
but no hardware operation was needed or run in this step.

Validation on the final source: `./tools/test-rmi-registry` passes including
typed return bookkeeping and proxy roots; elevated
`PICO2_HOST_USB_ACCESS=1 ./tools/test-jcsystem-transaction-applet --case 48`
passes `TEST:SUMMARY pass=3 fail=0 elapsed_s=74.206`; `./tools/build-pc
--profile current`, `./tools/build arm`, and `./tools/build riscv` pass. ARM
image end address is `0x100e04dc`. Root `void` return-kind capture is tested
in the registry unit. Existing Java Card converter, Rust OS, and
packed-CAP warnings remain.

Next implement an unchanged/upstream-compatible positive Shareable fixture or
find an existing deploy CAP exercising successful cross-AID lookup and
interface calls. Then cover argument descriptors, safe reference result
translation, denied references, exceptions, and context restoration. Do not
claim CTX-01 complete from this groundwork.

## Previous-context AID native behavior — implemented offline (2026-11-01)

`Context::setActiveContext()` now stores the source applet AID on context entry;
`JCSystem.getPreviousContextAID()` returns a JCRE-owned global AID through the
existing `SensitiveResult` channel, or null when the prior context was JCRE.
The context regression reports `previous_context_aid=1` (JCRE entry,
client/server switches, return switch, and same-context re-entry).

Validation passed: `./tools/test-rmi-registry`, `./tools/build-pc --profile
current`, `./tools/build arm`, and `./tools/build riscv`. Build output still
contains pre-existing Java Card converter, packed-CAP-member, and Rust warnings.
The user reports that boards are connected, but a fresh authorized
`PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader` returned
`found 0`; elevated `lsusb -d 2e8a:` also found none. No flash or physical
validation was attempted. Next implement positive cross-AID SIO lookup and a
context-owned proxy/dispatch path with argument and return firewall checks;
keep this JCSystem step and the broader SIO behavior separate in acceptance.

## SIO dispatch investigation — native interception required

The generated ROM API JCA confirms `NativeImplementation.getAppletShareableInterfaceObject`
is a bytecode method, not a native entry. `JCSystem.getAppletShareableInterfaceObject`
invokes it using `invokestatic`, and its bytecode has the same-AID comparison.
Do not add a C++ function with the same symbol and assume it will run. The
native implementation must intercept the resolved ROM method in VM dispatch,
leaving API source and CAP files unchanged. The `invokestatic` resolver already
has the external package/class/method reference available. After interception,
resolve server AID to an installed instance, obtain its retained `Context`,
execute the server's `Applet.getShareableInterfaceObject` override in that
context, and return a context-owned proxy. `invokeinterface` must recognize
that proxy, enforce Shareable interface/method visibility and firewall rules,
run the target in its owning context, and validate reference arguments/results.
No positive two-applet fixture is currently present in `tools/tests`; existing
transaction probe only asserts absent/default-null behavior. The unchanged
`AlgTestApiMulti_303.cap` is a candidate integration probe after adding a
positive fixture without modifying the CAP.

## Lazy applet-context resolver — implemented, not yet wired to SIO

Added `choupi/src/context_registry.hpp` plus
`choupi_vm_find_or_restore_applet_context()` in `choupi/src/main.cpp`. It first
searches the selected and retained contexts by owner and full AID; if absent, it
finds an installed dynamic instance, restores its persistent applet root into
an unused inactive-context slot, and returns that context without changing the
selected applet. It returns null for invalid/missing AIDs, failed restoration,
or context-table exhaustion. The helper is not yet invoked by native Shareable
lookup.

Validation: `./tools/test-rmi-registry` passes with
`retained_context_lookup=1`; `./tools/build-pc --profile current`,
`./tools/build arm`, and `./tools/build riscv` pass; the elevated
`PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage` passes 47/47. Logs:
`artifacts/logs/host-vpcd-multipackage-apdu-20261101T155027Z.log` and its
matching restart/corrupt-sidecar/missing-sidecar logs. The physical reader
helper still found zero devices; no hardware flash was attempted.

Next: intercept the resolved ROM `NativeImplementation.getAppletShareableInterfaceObject`
call in `Bytecodes::bc_invokestatic`, invoke the server applet's virtual
`getShareableInterfaceObject` in the resolved context, then retain a
context-owned proxy. Follow with `invokeinterface` context switching and
firewall checks; do not return a server heap reference directly to a client.

## Latest verification — 2026-11-01

The current worktree snapshot was revalidated on 2026-11-01 after the CAP
Directory parser commit. Elevated `PICO2_HOST_USB_ACCESS=1
./tools/test-vpcd-multipackage` passed `TEST:SUMMARY pass=47 fail=0`, covering
three loaded packages, 21 applet instances, fresh-process restore, selection,
deletion, cross-AID transient lifecycle, and final registry cleanup. The ARM
`current` and RISC-V `current` repository builds also passed. These checks ran
against the current worktree, which still contains unrelated uncommitted source
edits; those edits were not staged or committed by this validation. The
authorized PC/SC helper (with the host-access marker) found zero readers, and
elevated `lsusb -d 2e8a:` returned no devices. `./tools/doctor` confirms build
prerequisites only. No physical flash or hardware test was possible.

The current-source host persistence regression passed again via the required
elevated path: `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence`.
Install/commit passed 3/3; a fresh emulator process restored the persistent
heap and passed 38/38 assertions across four GC-pressure rounds. Logs:
`artifacts/logs/vpcd-persistence-commit-20261101T141917Z.log`,
`artifacts/logs/vpcd-persistence-restore-20261101T141917Z.log`, and the
associated emulator logs with the same timestamp. This does not establish Pico
power-loss recovery.

Although the user reports that physical boards are connected, the authorized
`sudo -n /usr/local/sbin/pico2-pcsc-reader` helper returned `found 0` and an
elevated `lsusb -d 2e8a:` found no Pico devices in the VM. No flash or physical
test was attempted. Resume physical validation only after the authorized path
enumerates a reader; continue offline otherwise.

## CAP Directory cross-component size validation — implemented

`choupi/src/jc_handlers/jc_cap.cpp` now verifies each present CAP component's
payload length against the corresponding Directory size entry after all
component frames are parsed. GP LOAD may omit components while leaving their
original nonzero Directory entries, so absent components are skipped. The
initial strict-zero rule failed against stale PC emulator output; after
rebuilding with the subset-aware rule, the elevated VPCD multipackage suite
passed 47/47. The ASan/UBSan parser harness passed 4,208 checks across all 48
deploy CAPs and 100,000 random inputs. PC `max_crypto`, ARM `current`, and RISC-V
`current` builds passed. The reader helper still reports zero devices; do not
claim physical validation or flash until it enumerates a board.

## Current priority: finish ARCH-08 physical USB progress acceptance

The high-level project summary in `STATUS.md` was reconciled with current TODO,
physical API, and USB-enumeration evidence in commit `9364a9b`.

ARCH-09 production progress/dispatch coverage is implemented in the latest
worktree. `src/ccid_device.cpp` calls shared production helpers in
`src/ccid_progress_flow.hpp` for pending-message dispatch, guarded USB progress,
time-extension waits, and final-response ordering. `tools/test-ccid-progress-flow`
passes 9/9 focused host checks; PC, ARM, and RISC-V builds pass. Implementation
commit:
`309aa5b` (`Exercise production CCID progress flow`); `progress.md` records the
hash and validation details.

The physical boards are plugged in per the user, but the authorized
`sudo -n /usr/local/sbin/pico2-pcsc-reader` helper reports `found 0`. Treat that
as missing guest-visible USB passthrough, not firmware failure; do not run a
flash or restricted-context USB probe. Refresh the helper again immediately
before any hardware attempt.

Next, finish ARCH-08 when a board enumerates: run the non-destructive
`JCAlgTestApduProbe --rsa-3072-keypair-generation` on an identified `max_crypto`
reader while flooding benign USB control requests, then measure the bounded
service duration, queue drain, and CCID extension/final-response ordering.
Validate the `tools/flash` multi-device selector on identified boards. Keep
ARCH-08 open until these stress/timing/physical checks pass.

## ARCH-08 implementation evidence and open physical gates

ARCH-07 is implemented and validated in commits `a252e73` and `1187442`.
ARCH-08 now has an implementation but remains open for its stress and timing
acceptance. The pinned TinyUSB v0.18.0 checkout has no event-count limit and
its Pico queue ignores timeout values. Added a repository-owned patch pinned to
commit `86ad6e56c1700e85f1c5678607a762cfe3aa2f47`; the patch adds nonblocking
`tud_task_budgeted(max_events)`, returns the processed count, and leaves later
FIFO entries queued. `tools/build` applies/checks this patch through
`tools/apply-tinyusb-event-budget`. All JCVM call sites use an eight-event
budget, including startup CDC wait, main loop, crypto progress, and CCID
time-extension wait. PC, ARM, and RISC-V builds passed.

Physical evidence: the ARM `current` image passed an 18-second monitor on Pico
2 `2BED26ED20556535` (`artifacts/logs/cycle-choupi-20261101T132943Z.log`); the
ARM `max_crypto` image passed on `8E464F4E2BBC0A19`
(`artifacts/logs/cycle-choupi-20261101T133931Z.log`). Attempts to complete the
RSA-3072 long-operation gate were inconclusive. The crypto test CAP install on
2BED returned `6A80` and cleanup confirmed it absent. The upstream JCAlgTest
CAP was already present on 8E, but PC/SC stopped listing that device before
its probe could run. A later max_crypto flash to 2BED verified the image, but
the reader/CDC devices disappeared before the finite monitor; the authorized
helper then reported zero Pico devices and `lsusb -d 2e8a:` found none. Do not
interpret these transport/capacity outcomes as an ARCH-08 algorithm failure.

The event-count limit is established by code and both architecture builds, but
no sustained control-event workload or per-call wall-time maximum has been
measured. Next, when a board is enumerated, run the non-destructive
`JCAlgTestApduProbe --rsa-3072-keypair-generation` command on an identified
`max_crypto` reader, then add a control-request flood during that APDU and
measure the bounded service duration and response ordering. The host-only
probe sends the upstream CAP's prepare/generate APDUs and does not install or
modify a CAP. `tools/flash` now selects a sysfs-identified live board by its
current bus/address after resolving its serial; syntax checks pass, but the
multi-device selector change itself still needs a hardware cycle. After ARCH-08
acceptance, continue with the next TODO task; ARCH-09's host production-path
harness is now in place.

At the last refresh, no Pico USB devices were enumerated despite the boards
being physically connected. Use the authorized helper before the next physical
operation. Existing Rust OS
and packed-CAP warnings remain; none was introduced by the TinyUSB event-budget
change.

## Bounded applet-context retention — focused lifecycle passes (2026-11-01)

Implemented in the current uncommitted worktree:

- `choupi/src/main.cpp` retains the active context and a bounded table of up
  to `JCVM_MAX_APPLETS - 1` inactive contexts, keyed by applet owner and full
  AID. Context restore reuses a cache entry or restores a fresh heap before
  moving the previous context into the cache. Table exhaustion reports
  `6A84`; other restore failures report `6F00`.
- Card reset clears `CLEAR_ON_RESET` arrays in all cached heaps. Inactive heaps
  get one incremental GC pass per 32 APDUs to cap extra root-walk cost. PC
  diagnostics also trace cached roots.
- Successful GP DELETE invalidates matching selected and inactive contexts in
  both physical CCID and PC VPCD card-manager paths. Canonical global AID
  objects remain alive because other contexts may still reference them.
- `tools/tests/pc_vpcd_multipackage_test.py` builds the native JCSystem probe
  CAP and runs transient-array checks at the end of the multipackage suite.
  Earlier registry, lookup, reset, restart, and deletion scenarios remain
  isolated from the fixture.

Validation: `./tools/build-pc --profile current`, `./tools/build arm`,
`./tools/build riscv`, `python3 -m py_compile
tools/tests/pc_vpcd_multipackage_test.py`, and the authorized
`PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage` passed. VPCD reports
47/47, including both CLEAR_ON_RESET and CLEAR_ON_DESELECT after switching
away and back, selecting a surviving applet after package DELETE, probe
cleanup, and an empty final registry. The physical ARM image was cycled onto
Pico 2 `8E464F4E2BBC0A19`; finite serial output ended with
`TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. On that device,
the transaction probe passed transient-state checks across an applet switch,
its applet and package were deleted, and SELECT of the surviving Core applet
returned `9000`. GPPro confirmed the probe package was absent afterward. The
second Pico reader `2BED26ED20556535` remained available as failover.

The root cause was root enumeration mixing applet-local references from
separate heaps. Embedded and PC diagnostics now walk only the selected
context's roots; package-wide static roots are not interpreted in a local
heap. `getCap()` also rejects missing dynamic CAP data directly. VPCD and
physical post-delete selection both pass. Broader package static sharing,
firewall/shareable calls, and logical channels remain open. The PC test wrapper
does not rebuild the emulator, so run `./tools/build-pc --profile current`
before each rerun.

Validation artifacts:

- VPCD APDU log: `artifacts/logs/host-vpcd-multipackage-apdu-20261101T125822Z.log`
  SHA-256 `8179640d2e66b9cf4e023f5732a43117596721943a68c3b077e11bcdc72d19a3`.
- VPCD restart log: `artifacts/logs/host-vpcd-multipackage-restart-20261101T125822Z.log`
  SHA-256 `f6d33d34819dfb43099a2aa92b0450f953909f573247b2e583550b27064ee8ee`.
- Physical post-delete log: `artifacts/logs/physical-delete-reselect-8E464F4E2BBC0A19-20261101.log`.
- Current finite monitor: `artifacts/logs/monitor-choupi-8E464F4E2BBC0A19-20261101.log`
  SHA-256 `b054d15da01d989b53cb46e9faf7b9acc36d1836760bcfc00ae002ab1863703a`.
- Finite runtime monitor: `artifacts/logs/cycle-choupi-20261101T125044Z.log`
  SHA-256 `2482238cbfd095886868f14b4e05f662ab0c69b4478f906c0515dd4cf9689f9b`.
- ARM UF2 SHA-256 `8d341848a4e54e1fd29de067ad0f77888c729726cf72e9724020c088e40dbe02`
  (1,824,768 bytes). The build includes other pre-existing dirty source
  changes; this hash is not reproducible from the focused fix commit alone.

Next: continue the TODO's remaining context/firewall work. Keep physical
testing paused until a Pico enumerates in the VM. Source review confirms the
cross-applet Shareable gap is deeper than lookup: the ROM API helper currently
rejects a server AID different from the client AID, and `invokeinterface`
resolves receivers only through the active context's heap. A raw reference
cannot cross heap boundaries. See the new QKB entry for the required
context-owned proxy, nested server invocation, and firewall checks. The
`Interpretor(Context&, ...)` constructor plus `ActiveContextScope` provide a
potential nested-execution primitive; they do not implement SIO by themselves.

## Multi-applet context lifetime audit — 2026-11-01

The physical-reader helper returned zero readers, so no hardware operation was
attempted. Source inspection found that selecting a different applet AID calls
`choupi_vm_restore_applet()`, which replaces the sole
`selected_applet_context` with a new `Context`. The inactive applet heap is
discarded. This prevents transient arrays and their references from surviving
applet switching; `CLEAR_ON_RESET` contents are also lost before a card reset.
The `TODO.md` multi-applet item now records this concrete gap. Do not retain
inactive heaps without adding their roots to collection and defining a bounded
RAM policy. Next, design a context registry and a regression using existing
deploy CAPs; preserve the declared profile exclusion until that work passes.

The elevated PC VPCD multipackage regression passed 42/42 on 2026-11-01 via
`PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage`. It loaded 3 packages
and 21 instances, tested selection of Core/Multi A/Multi B, persistent AID
identity, restart, owner scans, and deletion. It does not check transient array
identity or `CLEAR_ON_RESET` contents after an AID switch. Logs:
`artifacts/logs/host-vpcd-multipackage-apdu-20261101T104132Z.log` (SHA-256
`36c3633cbab73368b2c8bfd17575d9363a2d52214e8579161724a1dd799e9db7`) and
`artifacts/logs/host-vpcd-multipackage-restart-20261101T104132Z.log` (SHA-256
`e938327d476345a034f022364e5a3793845c0c05bfc317284bf34ee2c1ae2fb7`).

## Signature MAC IV fix — 2026-11-01

The full API 3.0.5 Crypto sweep's three `WRONG_RESULT` rows were caused by
`runMac()` constructing a zero IV unconditionally, even after
`Signature.init(key, mode, parameters, ...)` stored a supplied IV on the
Signature instance. The native MAC path now reads that array. The deterministic
PC VPCD suite adds nonzero-IV sign and verify vectors for DES-MAC8-NOPAD,
DES-MAC8-ISO9797-M2, and AES-MAC-128-NOPAD; all 79 vectors passed
(`TEST:SUMMARY pass=81 fail=0`, counting selection/emulator phases). ARM and
RISC-V `max_crypto` builds also passed. The unchanged API 3.0.5 Crypto CAP and
official client ran via PCSC Virtual PCD with only the three IV calls selected:
all 3 returned `OK`, 463 unrelated calls were skipped, and cleanup passed. A
full Crypto VPCD sweep separately timed out at 800 seconds after 190 partial
client rows (112 `OK`, 78 `UNSUPPORTED_PER_SPEC`) and before reaching the IV
calls. A follow-up selecting `ALG_RSA_SHA_384_PKCS1` completed all seven
matching API rows as `OK`, but its client also timed out afterward; the outer
180-second bound interrupted cleanup. A narrower `ALG_RSA_SHA_384_PKCS1_PSS`
run completed four matching API checks as `OK` and cleaned up in 88 seconds.
This points to slow full-area VPCD/client throughput rather than an isolated
RSA-384 error; the full-run timeout still needs an adequate timeout or a more
efficient test route. CSVs and hashes are in
`artifacts/results/deploy/vpcd-iv-fix-20261101/`.
Physical confirmation is still pending because the authorized reader helper
found zero physical Pico readers; no flash was attempted. Next, when a Pico
appears, rerun the full Crypto area and check for residual `WRONG_RESULT` or
transport errors. RSA SHA-512 PSS's one `EXC_CARDRUNTIME reason=0x0001`
corresponds to the upstream 1024-bit key/64-byte-salt invalid input, already
confirmed by separate known-answer analysis; do not change the native PSS
implementation for it.

## Windows crypto-profile bundle refresh — 2026-11-01

Regenerated the ignored external-test archive with the unchanged upstream
JCAlgTest 1.8.2 CAP, official client and GPPro, current expected CSVs, and ARM
images for `symm_crypto` and `asymm_crypto`. The wrapper package workflow passed
both ARM/RISC-V profile build gates, checked all per-file SHA-256 entries, and
passed `unzip -t`. Archive:
`build/external-test/pico2-jcvm-windows-test.zip`, SHA-256
`a26c86489c2fd9887aac58c987585eae8d10410317ca3279c939ff524124c7fa`. The
profile UF2 hashes are `d36b6128fc2caea130e689614a12d62906ac46fa6add30085b253aa46b01e826`
(`symm_crypto`) and
`cda8a3b7dee0d7536575bc3173d00f04848cf9d68a370ae207d595a9a8148e2f`
(`asymm_crypto`). The CAP retains its pinned upstream SHA-256
`b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
Windows flashing, PC/SC and client results remain pending. This bundle is in
the ignored `build/external-test/` directory.

Current-source PC VPCD known-answer revalidation also passed after the bundle
refresh: symmetric tests ended `TEST:SUMMARY pass=81 fail=0` and
`cipher_vectors=32 mac_vectors=47 iv_vectors=6`; asymmetric tests ended
`TEST:SUMMARY pass=99 fail=0` and `asymmetric_vectors=97`. The initial
restricted symmetric invocation failed at local socket creation; the same
test passed through the authorized host-access execution path. These are PC
VPCD results, not physical-card validation. Logs:
`artifacts/logs/symm-crypto-kat-vpcd-20261026.log` (SHA-256
`6b6c6a0dc53828fcb3521e0dc71f124f4da13d9fab7183b5b6f9520958876808`) and
`artifacts/logs/asymm-crypto-keypair-vpcd-20261027.log` (SHA-256
`85528e25f28aeb55050fdfaa8afda5f33b44d893e1afb4f01317292e259acec7`).

## Package-delete overlay compaction — 2026-11-01

The persistent `NO_MEMORY` investigation found that successful package deletion
left dynamic-package tombstones in the 8,192-slot overlay index. Added
`compact_deleted_overlay_records()` after `fs_erase_package_state()`. It drops
unused failed-write reservations, tombstones for dynamic package IDs 56–63,
and tombstones with no ROM record, while preserving live entries and
ROM-backed tombstones. Payloads are packed in place; the hash table is rebuilt
using the existing erase scratch arrays, with no extra 64 KiB BSS table.
Parent-slot encoded records safely skip compaction.

`./tools/test-rp2350-fs-overlay` passes all tests. The expanded regression
covers all eight dynamic package IDs, each with 128 applet-state records plus
static and lifecycle tags, followed by deletion and restart. It preserves an
unrelated record and passes 64 randomized snapshot power cuts with old-or-new
recovery. A further test fills all 8,192 overlay slots, deletes package 56,
and verifies restart with only an unrelated record left. The compactor checks
each record offset and encoded-key length before reading payload bytes and has
no error return after index permutation begins. ARM and RISC-V `max_crypto`
builds pass. Latest ARM ELF end is
`0x10100038`; BSS end is
`0x20048214`. The latest source change has not been physically flashed: the
authorized helper currently reports zero Pico readers, and an earlier cycle
failed its monitor because the device disappeared from guest USB. Do not retry
hardware in a restricted context; continue offline until a device is available.

The elevated `PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` workflow
passed on 2026-11-01 using the PC VPCD backend: install/commit 3/3, then
fresh-process restore plus four GC-pressure rounds 38/38. It confirms the
current applet persistence and heap behavior in VPCD; it does not exercise the
RP2350 overlay compactor or prove physical power-cycle recovery. Logs are under
`artifacts/logs/vpcd-persistence-*-20261101T090958Z.log`.

Refreshed the external-test bundle at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`; it is byte-for-
byte equal to the latest ARM build, 2,098,176 bytes, SHA-256
`1dc6d486f49e9688eedcdf65785f1629cc6de93b23c7931e22d577ed6696d82d`. This
refresh includes the Signature MAC IV fix. ARM and RISC-V wrappers pass; the
image has not been flashed or runtime-tested on hardware because the reader
helper reports zero physical Pico devices. Its README records the limitation.
The unchanged support manifest SHA-256 is
`81824f2bdde2f48f39cf3a3e3da942bf89a81317f32f218a8955dce8b455dee0`.
The binary was built from the current worktree, including pre-existing
uncommitted `src/cap_load.cpp` changes; its README records that the artifact
commit alone is insufficient to reproduce it from a clean checkout.

Immediate next action: when a Pico is enumerated, run a physical cycle using
the latest `max_crypto` image, then a bounded temporary CAP install/delete
workload and confirm `CHOUPI:FS_COMPACT`. Repeat the upstream full Crypto area
and determine how many remaining `NO_MEMORY` outcomes are due to live objects
rather than stale deleted keys. Keep 8E's temporary Security CAP and any
existing 2BED packages intact.

## Current-image API runtime recheck — 2026-11-01

**Latest full-suite update (2026-11-01):** Full API 3.0.5 Crypto on 2BED
completed all 466 rows: 292 `OK`, 161 `UNSUPPORTED_PER_SPEC`, nine
`NO_MEMORY`, three `WRONG_RESULT`, and one `EXC_CARDRUNTIME`; there were no
`EXC_RUNTIME` rows. A fresh filtered run for KeyAgreement/Cipher executed
65 calls (50 `OK`, 15 `UNSUPPORTED_PER_SPEC`, no `NO_MEMORY`) and removed the
CAP successfully, but the client hit its 150-second bound before the filtered
plan finished; AEADCipher was not reached. This suggests suite-order or
cumulative state pressure, but is not a full isolation proof. An exact-case
filter then reproduced storage failures: the full-suite serial trace had
8,016 records and 94,573/98,304 bytes. The v4 snapshot's dynamic payload limit
at that record count is 94,592 bytes, leaving at most 19; the failure maps to
`NO_RESOURCE`/`NO_MEMORY`. Later, at 8,192 records, two exact filtered calls
failed while eight passed. This confirms persistent overlay exhaustion, not a
missing crypto implementation. The original captures predate the diagnostic
fix; current `FS_WRITE_FAIL` output includes the effective dynamic payload
limit, fixed arena size, file count and free slots. Package-delete compaction
is recorded in the current handoff section above. Crypto IV wrong-results and
RSA SHA-512 PSS `EXC_CARDRUNTIME` remain unexplained.

The full Security run assigned to 8E failed transport at transient RSA private
2048 allocation (`SCARD_E_NOT_TRANSACTED`) after 56 `OK`, 9
`UNSUPPORTED_PER_SPEC`, and 5 card errors; 8E then disappeared from PC/SC and
serial enumeration. Its temporary Security CAP remains installed for cleanup
when that exact reader returns. Keep using 2BED for current work.

Filtered-run artifacts: CSV
`artifacts/results/deploy/max-crypto-full-current-20261101/isolated-memory-2BED.csv`
(SHA-256 `2996f7e8d5b5cc3ead35620e952b7d28c97915216c1859aee392d5ac8596cd09`);
serial `artifacts/logs/max-crypto-isolated-memory-2BED-20261101.serial.log`
(SHA-256 `de0b789f19b046a7ef34d0f7318771c5b972a4ac4da0ef12f6bcf6144f2df77c`).
Exact failing-case CSV
`artifacts/results/deploy/max-crypto-full-current-20261101/nine-memory-2BED.csv`
(SHA-256 `a7d30c2eb7045d5d386b582f4e1a545d48df2fc8a574f7e4a0c48ea69982dbe9`);
serial `artifacts/logs/max-crypto-nine-memory-isolation-2BED-20261101.serial.log`
(SHA-256 `a28357fee22e19f4d7fac9d51fe8dd5b940377ffe74b482fbbd1308198cea6af`).

The `FS_WRITE_FAIL` format now reports `files=current/max free_slots=N
bytes=used/effective_limit arena=fixed_limit`. ARM and RISC-V builds passed;
`PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile max_crypto --serial
2BED26ED20556535` flashed and verified the updated ARM image and passed its
finite monitor. Image SHA-256:
`52a6980806b8dcf4b418e5ff75b9e401f010cfe3a14f9206cf8679a50b1b2f13`.
`./tools/test-rp2350-fs-overlay` passed (`TEST:SUMMARY fail=0`) and exercised
the new fields at `files=4894/8192 free_slots=3298 bytes=98287/98304
arena=98304`. The physical failure branch was not hit after flashing. A
subsequent AEADCipher-class filter returned constructor `OK` plus seven
`UNSUPPORTED_PER_SPEC` rows, but
the client timed out at 60 seconds; temporary CAP cleanup passed. No storage
failure appeared in the bounded serial capture. Treat the timeout as failed
validation, not as an API pass. TODO tracks persistent reclamation as the next
storage task.

Rebuilt ARM, RISC-V, and PC `max_crypto` with bounded build jobs. ARM and
RISC-V wrapper builds passed. The PC wrapper initially failed linking because
two stale bytecode object files were zero bytes after an interrupted build;
touching the two modified sources forced regeneration, after which
`./tools/build-pc --profile max_crypto` passed. Physical cycle on Pico 2
`2BED26ED20556535` passed both architecture build gates, flash verification,
and the finite 18-second monitor; current ARM UF2 SHA-256 is
`986c432afeaba6562844c3d62208b30da9b9667f8afb23411a21a4d277fa2581`.

On that image, the exact API 3.0.5 RSA-512-only test returned `OK` (900 calls
skipped), and its temporary CAP cleanup passed. A wider Security filter
returned `OK` for RSA-512 and RSA-CRT-512 and `UNSUPPORTED_PER_SPEC` for
DSA-512. The filtered DES-MAC8 Crypto test returned seven `OK`, one
`WRONG_RESULT` on the IV test, and no `EXC_RUNTIME`; both temporary CAPs were
removed. The 60-second Security serial capture and 90-second Crypto serial
capture contain no `CHOUPI:NULL_RECEIVER` line, while their runtime
self-checks repeatedly reported `TEST:SUMMARY pass=1 fail=0`.

This supersedes the earlier claim that these focused cases currently reproduce
the full-sweep NPEs. It does not establish why the old results differed and
does not prove full Security/Crypto correctness. Run complete current-image
Security and Crypto areas next, then inspect the DES-MAC8 IV wrong result. The
small API CSVs and serial log hashes are recorded in `progress.md`.

VPCD is not currently usable for comparison: the approved reader helper and
`pcsc_scan -r` enumerate two Virtual PCD readers, but `choupi` cannot connect to
`127.0.0.1:35963` (`Connection refused`). The old 81,920-byte
`algtest-v182-empty-flash.bin` also has a stale dynamic-package bit without a
registry sidecar; use the 524,288-byte
`algtest-v182-current-empty-flash.bin` for clean emulation. Even with that
image, the endpoint still refused connections. No emulator API results were
produced.

The previous section below records the historical 2026-10-31 failure and is
retained for audit; its conclusion is not current-image evidence.

## Focused v4 API runtime-failure reproductions — 2026-10-31

On Pico 2 `2BED26ED20556535`, isolated upstream API 3.0.5 tests reproduced
the two representative failures without a full sweep. Security ran only
`KeyPair.genKeyPair()` for RSA-512 (900 calls skipped); the single enabled call
returned `EXC_RUNTIME`. Its serial trace reported caught exception ID 28
(`NullPointerException`) at CAP pc 6387, opcode `83`. Crypto ran only
`ALG_DES_MAC8_NOPAD`: three constructor/getter checks passed and five method
variants returned `EXC_RUNTIME`, with caught ID 28 at CAP PCs 8624, 9291, 9842,
and 10317. Both applet CAPs were removed successfully afterward.

This shows the errors reproduce independently of prior sweep state and without
`NO_MEMORY`. A null-reference problem near the CAP/native API boundary is a
working hypothesis, not a confirmed root cause. Next, disassemble/map these
CAP PCs to the exact API subcalls, then add bounded native-dispatch diagnostics
for method token and receiver reference. Avoid changing crypto arithmetic
until the failing native entry points are identified. Full results and hashes
are in `progress.md`.

The focused repro documentation is in commit `456d250`; the preceding v4
capacity rerun is recorded in `b891643` and linked in `ca19e2a`.

## Current v4 persistent-capacity API validation — 2026-10-31

The v4 snapshot image removed the prior `NO_MEMORY` outcome on identified Pico
2 `2BED26ED20556535`: isolated unchanged API 3.0.5 Security completed 901 calls
(150 `OK`, 206 `UNSUPPORTED_PER_SPEC`, 543 `EXC_RUNTIME`, two skipped), and
Crypto completed 466 calls (158 `OK`, 102 `UNSUPPORTED_PER_SPEC`, 206
`EXC_RUNTIME`). Both exact-reader CAP cleanup stages passed. This verifies the
96 KiB v4 data arena can complete these runs without the former capacity status,
but the profile does not pass API correctness due to the large runtime-error
counts. The first runtime rows are Security RSA-512 `KeyPair.genKeyPair()` at
call 242 and Crypto `Signature.ALG_DES_MAC4_NOPAD` at call 153. The physical
serial captures include VM-caught exception ID 28 (`NullPointerException` by
the `Exceptions` enum); runtime `TEST:SUMMARY` output is only the firmware
self-check.

On 8E `8E464F4E2BBC0A19`, Security failed at transient RSA key allocation with
`SCARD_E_NOT_TRANSACTED`; excluding that call moved failure to RSA-CRT transient
reset allocation. Serial remained healthy, but PC/SC stopped registering 8E
even after `sudo -n /usr/local/sbin/pico2-pcsc-reader`. The failed run left its
temporary Security CAP installed; preserve and clean it only after that exact
reader returns. 2BED remains the working physical failover device.

A fresh PC/VPCD comparison successfully uploaded Security but did not finish
within the 600-second bound; it produced no API call rows and is not evidence.
The exact result CSVs and capture hashes are recorded in `progress.md`.

Next: diagnose the first runtime exception on 2BED with serial capture during
the specific API call; inspect native dispatch and heap object lifetime, then
repeat full Security/Crypto. Do not mistake runtime self-test output for API
suite success. The v4 acceptance item in `TODO.md` is complete only for its
capacity question; this new API correctness issue remains open.

Documentation/progress commit: `b891643` (`Record v4 API capacity rerun`).

## Current ARCH-04 physical native-progress validation — 2026-10-31

The focused native progress callback and dispatch reentry tests already passed
6/6, and the VPCD asymmetric suite passed 99/99. Closed the physical gate on
Pico 2 `2BED26ED20556535`: `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi
--profile max_crypto --serial 2BED26ED20556535` built ARM and RISC-V, flashed
the ARM image, and passed the 18-second runtime monitor. Build image SHA-256:
`de06d766469b459c299ce5831a9d3ad5a8139643cf4630e431fe019bd1724719`.

On that fresh image, the physical RSA-3072 key-generation/signature APDU
returned `019000` in 49,184 ms; temporary CAP cleanup passed. This exceeds the
15-second watchdog interval, exercising native random-byte progress servicing
and the CCID long-operation extension path through final APDU response. The
subsequent 8-second serial capture repeatedly reported
`TEST:SUMMARY pass=1 fail=0`. The firmware has no direct counter for extension
frames, so no exact frame count is claimed. Logs and hashes are recorded in
`.agent/QKB.md` and `progress.md`. Root documentation/progress commit:
`d53c10f` (`Verify native progress path on Pico 2`).

The cycle log contained one `WATCHDOG_RECOVERY ... cause=other` trace during
the intentional CDC-to-BOOTSEL reset; the flash cycle and runtime checks
passed, and the long APDU itself did not reset the board. Treat it as a
reset-retained diagnostic trace unless it recurs during ordinary runtime.

Next: continue ARCH-06's physical install/rollback gate. Preserve unrelated
worktree changes.

## Current ARCH-05 physical startup and persistence — 2026-10-31

On the freshly flashed 2BED `max_crypto` image, the persistent GP registry
listed the pre-existing JCAlgTest and ApiSec applets. `JCSystemTransactionPhysicalProbe
--reset-only` caused a PC/SC CCID slot reset (`PCSC:RESET_PASS`); the subsequent
`tools/test-pico-powerup` run again listed the JCAlgTest package/applet and
passed applet SELECT (`9000`), GETVERSION (`1.8.2_jc305`), and JCSystem-info
(`9000`). This verifies startup and registry/applet persistence across firmware
reflash and an explicit card-slot reset. The script's own log says
`power_cycle=not_verified_by_script` because reset was a separate preceding
operation; the sequence is recorded across both logs.

A deeper current JCSystem transaction-fixture install returned GPPro `6A80`
on both boards and cleanup confirmed no test package remained. 2BED is already
documented at only 8,821 free persistent bytes while the fixture requires
32,770; do not repeat the large install without freeing safe space. Earlier
8E physical evidence passed 9/9 transaction/power-cycle checks on 2026-10-30.
Root physical startup/persistence acceptance is met; this failed oversized
fixture is a capacity limitation, not a firmware startup failure.

Artifacts: `artifacts/logs/cycle-choupi-20261031T214423Z.log`,
`artifacts/logs/ccid-slot-reset-2BED-20261031.log`, and
`artifacts/logs/pico-powerup-persistence-20261031T220152Z.log`; their hashes
are in the matching `progress.md` record.
Root validation/tracker commit: `4dfbbd1` (`Verify Pico startup and registry
persistence`).

ARCH-06 physical GPPro install/rollback validation is now complete; see the
current section below. Next: continue the highest-priority unfinished item in
the persistent-object reclamation backlog at the start of `TODO.md`. Preserve
all unrelated worktree changes.

## Current ARCH-06 physical GPPro install/rollback — 2026-10-31

The freshly flashed ARM `max_crypto` 2BED image accepted GPPro LOAD/INSTALL of
`build/symm-crypto-probe/output/symm-crypto-probe.cap`. The applet selected and
completed the RSA-3072 generation/signature APDU (`019000`, 49,184 ms); script
cleanup deleted the temporary applet and package and reported
`PHYSICAL:CLEANUP_PASS`. The exact CAP is a test-only package; no production
applet was changed.

The separate physical JCSystem transaction fixture's LOAD succeeded but
INSTALL returned `6A80`. Cleanup on both 2BED and 8E verified that no temporary
package remained. For 2BED this matches the known limit: a 32,770-byte
persistent array exceeds the remaining 8,821-byte commit budget. This exercises
the failed-install rollback path, not a VM regression. Existing JCAlgTest and
ApiSec registrations remained present. The full transaction fixture has
separate historical 9/9 physical pass evidence on 8E (2026-10-30).

Physical install log: `artifacts/logs/asymm-crypto-pcsc-20261031T214638Z.log`
(SHA-256 `2f20190fdb89991d5aae79bf028ff2bcf4868c28a1e766998fb0fce27686bc81`).
Failed-install/cleanup logs:
`artifacts/logs/jcsystem-transaction-physical-20261031T215221Z.log` and
`...T215407Z.log`; hashes are in `progress.md`. No reader packages were removed
except the temporary probe package and applet installed by the successful
test.
Root physical install/rollback commit: `99dfe3b` (`Verify physical GPPro install
and rollback`).

## ARCH-02 PC disk-checkpoint contract — 2026-10-31

Documented the PC emulator's disk checkpoint behavior in
`docs/FILESYSTEM_ABI.md`. `fs_sync()` only reports success after in-memory
filesystem writes; `--save` writes the full memory image after normal control
reaches the save block, then writes the CRC-bound VPCD registry sidecar. The
image is truncated and rewritten in place; the sidecar uses temp+rename, and
neither is explicitly fsync'd. This is a process-level save, not an atomic pair
or host power-loss guarantee. Resume rejects a missing/corrupt/mismatched
sidecar when dynamic packages are present. TODO ARCH-02 is now complete at the
contract level; atomic image replacement and host durability are future
hardening, not implemented here.

Validation: source-path inspection of `main_pc.cpp` confirms save ordering,
VPCD disconnect/idle-timeout fallthrough, and sidecar CRC checks; `git diff
--check` passes. No code behavior changed, so builds and hardware tests were
not run. Root documentation commit: `6731e03` (`Document PC emulator
checkpoint durability`).

ARCH-02's contract-definition item is complete; atomic image replacement and
host power-loss durability remain possible future hardening work.

## Current filesystem error-code parity — 2026-10-31

Defined shared C ABI filesystem statuses: `0` success, `1` OutOfFlash, `2`
NoSuchTag, `3` InvalidLengthForTag, `0x81` locked flash, `0x82` out-of-bounds,
and `0x83` unknown flash/snapshot I/O. Pico now distinguishes those conditions
for reads, writes, erases, typed access, prefix erase, and sync. Typed 2/4-byte
offset arithmetic rejects overflow before scaling. OS `edit_at` validates tag
and bounds before removing a record, and OS prefix erase now accepts the shared
128-byte maximum. `fs_exists`, `fs_next_tag`, and `fs_init` keep their distinct
contracts. The mapping is documented in `docs/FILESYSTEM_ABI.md`.

Validation: overlay and power-cut harness passes `TEST:SUMMARY fail=0`, with
new checks for missing/invalid/oversized tags, bounds, overflow, capacity
exhaustion, and injected snapshot I/O failure. Rust OS filesystem tests pass
28/28 (two ignored), syscall emulator tests 4/4, and the FFI status table test
passes. PC, ARM, and RISC-V `max_crypto` wrapper builds pass. Existing upstream
Rust and packed-member warnings remain. No hardware flash was run; this step's
storage behavior is exercised in the overlay harness and OS emulator.

OS implementation commit: `7362164` (`Align filesystem error statuses across
backends`); root integration commit: `21c8336` (`Align filesystem status codes
across backends`).
Next: continue the remaining filesystem durability and JCVM acceptance items.

## Current filesystem ABI alignment — 128-byte tags

Aligned the OS Rust filesystem with Pico's 128-byte tag capacity. OS records
with tags through 29 bytes retain the original encoding; tag lengths 30–128 use
reserved five-bit header code 30 followed by one length byte. The syscall
length-prefixed scratch buffer is now 129 bytes, and applet/static tag access
checks accept the shared limit. Existing records require no migration.

Validation: OS filesystem suite passes 27/27 (two ignored), including legacy
and extended records across restart, replacement and erase, malformed length
rejection, and the 29/30/128/129 boundaries. The elevated OS syscall emulator
suite passes 4/4, including 128-byte tag I/O and rejection of empty and
129-byte syscall-buffer tags. PC,
`PICO2_DEBUG=0` ARM and RISC-V `max_crypto` wrapper builds pass. Existing Rust
upstream warnings remain. No physical flash was run because this change only
affects the host OS filesystem; Pico already accepted 128-byte tags. Root
integration testing still does not directly exercise a GP CAP with a 128-byte
persistent tag.

OS implementation commit: `52772f1` (`Align OS filesystem tag length with
Pico`); root integration commit: `23ab3b1` (`Align OS filesystem tag capacity
with Pico`).

## Previous filesystem ABI alignment — oversized reads

Aligned Pico `fs_read` with the OS filesystem: both copy the smaller of the
caller buffer and record length, return success for a larger destination, and
leave its unused tail unchanged. The Pico overlay regression verifies the
copied prefix and sentinel tail; the OS syscall regression verifies the same.
`./tools/test-rp2350-fs-overlay` passes with `TEST:SUMMARY fail=0`. The focused
OS host syscall test passes 3/3 when run with ptrace access; its first restricted
attempt failed with `PTRACE_ATTACH EPERM`. PC, ARM, and RISC-V `max_crypto`
wrapper builds pass. Identified Pico 2 `2BED26ED20556535` was flashed with
`build/arm/pico2_choupi.uf2`; the 18-second finite monitor reported repeated
`TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. This validates
startup, not a filesystem APDU. The generic `cycle arm` briefly flashed and
tested the smoke target before the explicit `cycle choupi` restored JCVM
firmware; both final cycles passed.

Implementation source is in root `src/pico_filesystem.cpp`; an OS syscall
regression is in the `choupi/os` submodule. The FS ABI document and TODO now
record this discrepancy as resolved. Builds retain existing upstream Rust
warnings. Physical APDU behavior remains untested for this exact edge case.
Root implementation commit: `7de8aef`; OS test commit: `c3ccaa5`.

Next: continue the highest-priority offline correctness item in `TODO.md`;
retain physical power-cycle and PC/SC availability gates as open. Preserve the
unrelated dirty changes present in the worktree, especially `src/cap_load.cpp`,
`AGENTS.md`, supplied docs/data, and test tool edits.

## Previous ARCH-06 VM static initialization boundary — 2026-10-31

Moved CAP-declared static-array creation and persistent static-reference setup
from `src/cap_load.cpp` to `choupi/src/vm_cap_initialization.cpp`, declared in
`src/vm_cap_initialization.h`. `CapLoadSession` now calls
`jcvm::initializeCapStaticArrays(package_id)`; `cap_load.cpp` no longer includes
the heap, static handler, array handler, or static-field component parser
headers. The card-manager still owns GP LOAD/INSTALL sequencing, transactions,
and rollback. Existing uncommitted `cap_load.cpp` edits for host erase-prefix
routing and load diagnostics were preserved and are not part of the ARCH-06
change.

Validation: PC, ARM, and RISC-V max_crypto builds pass. Elevated
`tools/test-gppro-empty-install-vpcd` installed the unchanged upstream JCAlgTest
CAP. Elevated `tools/test-vpcd-multipackage` passed 42/42, including three
packages, 21 applet instances, restart restore, and deletion. Logs are under
`artifacts/results/gppro-empty-install-vpcd-20261031T204052Z/` and
`artifacts/logs/host-vpcd-multipackage-*-20261031T204220Z.log`. The latest
`pcsc_scan -r` lists only Virtual PCD 00 00 and 00 01; physical install and
rollback validation is open. Implementation commit: `26aacef` (`Move CAP static
initialization into VM`); the SHA is recorded in the progress-only follow-up.

Next: revisit physical gates for ARCH-04/05/06 if a Pico reader returns;
otherwise continue the highest-priority incomplete implementation in
`TODO.md`. Preserve unrelated dirty edits, especially `src/cap_load.cpp`'s
pre-existing host and diagnostic changes.

## Current ARCH-05 Pico filesystem extraction — 2026-10-31

Moved the complete Pico filesystem/overlay implementation, C ABI functions,
CAP-store adapter and persistent-memory availability function from
`src/choupi_pico2.cpp` into `src/pico_filesystem.cpp`. The Pico startup source
now contains watchdog/USB initialization, transient-memory reporting, VM
startup and the main loop. Root CMake and the isolated overlay test now compile
the new module. No ABI, flash constants, transaction order or storage-format
logic changed.

Validation: `./tools/test-rp2350-fs-overlay` passes all storage, migration and
power-cut checks with `TEST:SUMMARY fail=0`; max_crypto ARM and RISC-V wrappers
pass. ARM firmware image end remains `0x100ff5cc`. `./tools/build-pc
--profile max_crypto` passed before this Pico-only source extraction; it was not
rerun afterward. The current elevated `pcsc_scan -r` shows only Virtual PCD 00
00 and 00 01, so physical startup/reboot persistence and serial checks remain
unverified. The ARCH-05 code extraction is done; retain its physical gate as
open in `TODO.md`. Implementation commit: `ef5552e` (`Extract Pico filesystem
implementation`).

Next: continue ARCH-06 (encapsulate loader access to VM internals) in `TODO.md`.
ARCH-04's physical crypto callback check is also still open and should be
rerun if a Pico reader becomes available. Preserve unrelated worktree edits.

## Current ARCH-04 native progress reentry — 2026-10-31

Added `src/native_progress_guard.hpp`; `choupi_ccid_native_crypto_progress()`
cannot recursively call TinyUSB, and `choupi_ccid_task()` cannot recursively
dispatch APDUs. `docs/NATIVE_PROGRESS_CALLBACKS.md` records the callback order,
main-loop/APDU boundary, watchdog ownership and TinyUSB event-drain behavior.
The focused guard harness passes 6/6. PC, ARM and RISC-V `max_crypto` builds
pass. The elevated VPCD asymmetric test passes all 97 vectors / 99 total
checks, including RSA/EC key generation and applet APDU completion; this does
not exercise Pico callbacks. The only PC/SC readers currently listed are the
two virtual devices, so the required physical check remains open. Do not claim
watchdog servicing or CCID time extensions verified until a long native crypto
APDU is run on Pico and its final response is observed. Implementation commit:
`6bc468f` (`Guard native progress callback reentry`).

The callback's own work is fixed, but TinyUSB's `tud_task()` drains events
until empty. The active CCID bulk OUT endpoint is not rearmed until the APDU
response, preventing an ordinary CCID stream from replenishing events during
the call; continuous control-event flooding is not a strict WCET case covered
here.

Next: proceed with ARCH-05 (startup/filesystem extraction) in `TODO.md`, while
keeping the ARCH-04 physical verification gate open. Preserve all unrelated
worktree edits listed by `git status`.

## Current ARCH-03 diagnostic declarations — 2026-10-31

Added `src/vm_diagnostics.h` as the authoritative C ABI declaration source for
VM diagnostic state accessors/recorders, installation tracing state, and CCID
diagnostic accessors. Replaced duplicate prototypes across the Pico shim,
transport, VM main, context, heap, bytecode and handler sources. The diagnostic
implementations and output formatting did not change.

Validation: PC debug-enabled build passed with
`CHOUPI_JCVM_DEBUG=ON ./tools/build-pc --profile max_crypto`; release ARM and
RISC-V `max_crypto` wrappers passed. Rebuilt PC, ARM and RISC-V once more after
removing an empty preprocessor guard. Definitions and header declarations
compile together on all target paths. No serial monitor/physical reader check
was run because this was declarations-only; runtime output behavior is
unchanged by source inspection. The normal GCC/Rust builds still emit existing
packed-member and upstream Rust lint warnings, with no new warnings from this
header. Root implementation commit: `54bcb3f` (`Centralize VM and transport
diagnostic declarations`).

Next: ARCH-04 in `TODO.md` (define progress callback execution and reentry
rules). Keep the user’s unrelated worktree edits untouched.

## Current ARCH-02 filesystem ABI contract — 2026-10-31

Added `docs/FILESYSTEM_ABI.md` documenting filesystem status values, tag and
record bounds, buffer ownership, `fs_read_inplace` pointer lifetime, Pico sync
and persistent-transaction durability, and the PC emulator's no-op `fs_sync`
behavior. Inspection found backend differences in maximum tag length,
oversized `fs_read`, detailed status codes, and host-side disk checkpointing;
all four are recorded as unchecked follow-ups in `TODO.md`.

Fixed OS syscall `fs_read_inplace` for valid empty records (previously indexed
byte zero and panicked). Typed offset reads now check record bounds before
loading, and 2/4-byte write offset scaling rejects integer overflow. Added
regressions for borrowed data, empty records, out-of-range typed reads, Pico
in-place lifetime across `fs_sync`, oversized Pico read, plus a snapshot-test
isolation reset because the new early sync changed slot parity.

Validation: `tools/test-rp2350-fs-overlay` passes with `TEST:SUMMARY fail=0`;
`tools/env cargo test --manifest-path choupi/os/Cargo.toml
--no-default-features --features host fs::tests -- --nocapture` passes 22/22
(2 ignored); the focused `fs_syscalls` test passes 1/1 when run with host
`ptrace` access. The sandbox-only attempt failed with `PTRACE_ATTACH` EPERM.
`./tools/build-pc --profile max_crypto`, `PICO2_DEBUG=0 ./tools/build arm
--profile max_crypto`, and its RISC-V counterpart pass. No physical test was
run; this was ABI/storage-contract work and no hardware path was requested.
Nested OS source commit: `08f78a6` (`Fix filesystem syscall bounds and empty
reads`). Root implementation commit: `6aec961` (`Specify filesystem ABI and
harden bounds checks`).

Next: ARCH-03 in `TODO.md` (consolidate diagnostic declarations). Preserve the
remaining unrelated worktree changes and the nested OS submodule commit.

## Current ARCH-01 runtime/lifecycle ABI — 2026-10-31

Added `src/vm_lifecycle.h` as the shared C ABI declaration source for VM entry,
installation, applet restore, select/deselect, process, and clear operations.
The Pico, CCID, loader, VM, and PC VPCD translation units include it instead
of carrying duplicated lifecycle signatures. Rust host and embedded `runtime`
FFI declarations now match the C++ `uint16_t` result. Both callers explicitly
discard that status to preserve the existing zero remote-call return; runtime
diagnostics retain the VM status.

Validation: `./tools/build-pc --profile max_crypto`, ARM and RISC-V wrappers,
and the elevated VPCD multipackage lifecycle test all pass; the latter reports
`TEST:SUMMARY pass=42 fail=0` and exercises install, selection, APDU processing,
and process restart. Log artifacts:
`artifacts/logs/host-vpcd-multipackage-apdu-20261031T094555Z.log` and matching
restart/reject logs. No physical check was run. A bounded elevated PC/SC scan
in the preceding step found no registered reader. The Rust FFI change is in
submodule `choupi/os`; its local commit and the root gitlink update must both be
preserved.

ARCH-02 is complete; see the current handoff section above. The original stop
instruction applied after ARCH-01 and has since been superseded by the user
request to continue the project.

## Current GET DATA application list — 2026-10-31

Implemented GET DATA `2F00` in the Pico CCID and PC VPCD card-manager routes.
Both GP `80 CA` and ISO `00 CB` require `5C00` and return the ISD plus all
registered applet-instance AIDs as Application Templates. Responses obey
short-APDU Le and support `61xx`/GET RESPONSE, including SCP03-protected GET
RESPONSE. Missing/malformed tag lists return `6A80`.

Validation: `./tools/test-gp-get-data` 25/25; `./tools/test-short-apdu` passed;
Python syntax and Java probe compilation passed. PC, ARM, and RISC-V
`max_crypto` builds passed. The elevated raw VPCD multipackage test passed
42/42 with 21 instances, checking open GP/ISO requests, malformed lists, a
chained authenticated list, and full list restoration after process restart.
Log artifacts: `artifacts/logs/host-vpcd-multipackage-apdu-20261031T093126Z.log`
and matching restart/reject logs. A bounded elevated `pcsc_scan -r` timed out
without listing readers, so SmartcardIO and physical CCID GET DATA remain
unverified. No board was flashed.

Next: follow the highest-priority unchecked implementation task in `TODO.md`
(ARCH-01 is first in the scheduled architecture maintenance block); continue
offline while no PC/SC reader is registered.

## Current GET DATA ISO response change — 2026-10-31

`src/gp_get_data.hpp` now emits raw data-object values for ISO `00 CB`; GP
`80 CA` continues returning complete BER-TLV objects. Pico CCID and PC VPCD
routes, host tests, and `GPGetDataApduProbe.java` cover both forms. PC,
max_crypto ARM, and max_crypto RISC-V builds pass; host unit checks pass 18/18.
The open/authenticated APDU integration has not run: three bounded PC/SC scans
found no `Virtual PCD 00 00` reader. Even after the authorized reader helper,
pcscd was active but no VPCD reader was published. See QKB and the new wrapper
`tools/test-gp-get-data-vpcd`; rerun it when the reader is available. Artifacts
are under `artifacts/results/gp-get-data-vpcd-20261031T085055Z/`.

## Current blocker — physical max_crypto API rerun after snapshot v4

The former 77,824-byte filesystem cap is addressed in source by snapshot
format v4: an occupancy bitmap and compact records increase the bounded data
arena to 96 KiB within the existing 2 MiB layout. V2/v3 snapshots migrate
copy-on-write. Host overlay tests pass, both max_crypto architecture wrappers
pass, and the v4 ARM image was flashed to Pico 2 `2BED26ED20556535`; its finite
monitor passed. The exact-reader API Security attempt ended mid-APDU when the
physical reader disappeared, so the expected reduction in `NO_MEMORY` has not
been verified. Current PC/SC scan shows only two virtual readers.

Current external ARM image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` (2,089,472
bytes, SHA-256
`03ccac6577950f4e280a1a10d3c52b8d15d03ab52165b220a59a0a9a75e4e7df`). The
image contains no applet, but flashing firmware does not erase the separate
CAP store. The interrupted Security install may remain on the absent physical
board and must be checked/cleaned up by exact reader before retrying.

Next: when a physical Pico reader returns, rerun API 3.0.5 Security, inspect
all 901 call results and cleanup, then run Crypto. Keep the prior 2 MiB layout;
do not use the unverified upper flash range. The external image should match
the current build and state clearly that reflashing firmware does not erase a
card's separate persistent CAP store.

## Current max_crypto physical API Crypto rerun — 2026-10-31

On Pico 2 `2BED26ED20556535`, the unchanged deploy API 3.0.5 Crypto area
completed 466 calls with the deploy README's RSA-2048 key-generation exclusion:
263 `OK`, 161 `UNSUPPORTED_PER_SPEC`, 38 `NO_MEMORY`, three `WRONG_RESULT`,
and one `EXC_CARDRUNTIME`. Memory failures affected Signature, KeyAgreement,
Cipher, and AEAD. Three Signature IV known-answer checks returned unexpected
values; RSA-PSS raised `ILLEGAL_USE` once. The Crypto CAP cleanup passed, and
the final exact-reader GPPro listing showed only ISD and the original JCAlgTest
package/applet. CSV SHA-256:
`3a7859bb1f8283e1ce88578ea601aa3c7d39acd5d448a4526506a3c6e9593c10` at
`artifacts/results/deploy/pico2-2BED-max_crypto-crypto-20261031.csv` (ignored).

The current max_crypto image is ready at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` (2,088,448 bytes,
SHA-256 `3331edcb99400e774ad3e7678d0626fd9ab3986bde163077226a552dbe198213`).
External users must install applets explicitly; no test applet is preinstalled
by the image. Next: investigate repeated `NO_MEMORY`, transient AES reset, and
Signature IV results before describing this as a clean API baseline.

## Current max_crypto physical API Security rerun — 2026-10-31

On Pico 2 `2BED26ED20556535`, the unchanged deploy API 3.0.5 Security area
completed all 901 calls. Results: 302 `OK`, 504 `UNSUPPORTED_PER_SPEC`, 94
`NO_MEMORY`, and one `WRONG_RESULT`. The first `NO_MEMORY` was call 460 and
later affected several unrelated key families/EC accessors; this suggests
resource exhaustion during the long suite but does not yet establish its
cause. The wrong result was `Key.isInitialized()` for an AES transient-reset
key after deselect. Security CAP cleanup succeeded. CSV SHA-256:
`c6ba9233e27584280bd5632b1ca281509fedd2cc19c08fbd4184b01179694045` at
`artifacts/results/deploy/pico2-2BED-max_crypto-security-20261031.csv` (ignored).

Next: run Crypto using the deploy README's RSA-2048 key-generation exclusion,
then investigate the Security resource exhaustion and reset-state finding.

## Current max_crypto physical API Core deploy rerun — 2026-10-31

Ran the upstream `AlgTest_JavaCard/deploy` API 3.0.5 Core area on Pico 2
`2BED26ED20556535`, running the refreshed `max_crypto` ARM image. Core
completed all 291 calls: 272 `OK`, 15 `UNSUPPORTED_PER_SPEC`, and four
optional Ext calls unavailable because Ext installation returned `6A88`.
The runner rolled back the partial Ext package and removed its Multi and Core
CAPs. A final exact-reader GPPro listing showed only ISD and the pre-existing
JCAlgTest applet/package. CSV:
`artifacts/results/deploy/pico2-2BED-max_crypto-core-20261031.csv` (ignored by
Git, SHA-256
`fdcfbb8423d30df092f2fca7a8045472ebb81feb9f70dabc9173fb8688759a6c`). Final
GPPro transcript:
`artifacts/logs/gppro-list-physical-2BED-after-core-20261031.log` (ignored by
Git, SHA-256
`504b4fa8be609ab3c5915c6a4c5402b35b5ca8bfcc84d4223f328399826635a8`).

Next: rerun the deploy Security and Crypto areas on the current image, keeping
RSA-2048 generation excluded per the deploy instructions.

## Physical three-entry GET STATUS pagination — 2026-10-31

Added `tools/test-get-status-multipage-physical` and extended the direct SCP03
probe to accept a first-page Le and expected AID list. The wrapper preflights
that the Multi package/AIDs are absent, loads unchanged upstream
`AlgTestApiMulti_305.cap`, creates its two applet instances, runs an exact
reader-pinned GET STATUS check, and cleans up only that temporary package.
Pico 2 `2BED26ED20556535` returned one 32-byte entry with Le=40 and `6310`;
P2=03 with Le=256 returned both Multi applet entries and `9000`. All three
expected instance AIDs appeared exactly once. The deletion and final listing
checks passed, restoring the original JCAlgTest-only state. A separate default
Le=1 physical check passed after the parameterization.

Logs: `artifacts/logs/physical-get-status-multipage-20261031T061734Z.log`
(SHA-256
`4b3e27af31e2387c9b0f320329e1e6fe7d49180ed902a1550d34508bdac5a512`) and
`artifacts/logs/physical-get-status-le-20261031T061848Z.log` (SHA-256
`1b7196a61fded893a8b80e456352b2bfc6c285848e8846dee32358be02270522`).
Test code commit: `01a0805`.

Next: GET DATA beyond CPLC/Card Recognition Data and physical cold-power
registry reconstruction remain open in TODO.md.

## Short-APDU-bounded GET STATUS pagination — 2026-10-31

The Pico CCID and PC VPCD GET STATUS response builders now respect short APDU
Le. Case-2 and case-4 Le values cap response data (with encoded zero meaning
256); case-3 and malformed/extended-length frames use the implementation
buffer limit. The PC multipackage regression installs 21 instances and checks
that all AIDs occur exactly once across correctly terminated pages both before
and after a fresh emulator process restores the registry.

Validation passed: `./tools/test-short-apdu`; rebuilt PC emulator followed by
`PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage` (`TEST:SUMMARY
pass=41 fail=0`, three pages `236/248/186` bytes with status
`6310/6310/9000` before and after restart); `PICO2_DEBUG=0 ./tools/build arm
--profile max_crypto`; and the corresponding RISC-V wrapper build. The exact
ARM image was flashed to Pico 2 `2BED26ED20556535`; its finite monitor passed
(`TEST:SUMMARY pass=1 fail=0`). GPPro `--list` succeeded on the physical
`Pico 2 CHOUPI CCID [JCVM CCID] (2BED26ED20556535) 00 00` reader and listed ISD,
JCAlgTest, and its package. The physical card has one applet after the
temporary Multi package cleanup. A direct SCP03 PC/SC probe sends
Le=1, verifies empty `6310`, then sends matching P2=03 with Le=256 and verifies
the JCAlgTest AID plus `9000` (`TEST:SUMMARY pass=2 fail=0`). Do not use GPPro
`--secure-apdu` for this check: its wrapper transmits Le=00 regardless of the
requested Le. Logs:
`artifacts/logs/host-vpcd-multipackage-apdu-20261031T060525Z.log`,
`artifacts/logs/host-vpcd-multipackage-restart-20261031T060525Z.log`,
`artifacts/logs/cycle-choupi-20261031T055936Z.log`, and
`artifacts/logs/gppro-list-physical-2BED-20261031-current.log`,
`artifacts/logs/physical-get-status-le-20261031T061319Z.log` (SHA-256
`bac3e5e627312edf9be024ffc12ee6761cd6abfb693da384debb099af42fb40f`). Firmware and
README refreshed at `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`
(2,088,448 bytes, SHA-256
`3331edcb99400e774ad3e7678d0626fd9ab3986bde163077226a552dbe198213`). Source
and tests are committed as `5b3a6c7`.

Next: continue the remaining GET DATA and package lifecycle gaps in TODO.md.

## Persistent reclaimer physical probe and reachability correction — 2026-10-31

The reclaimer primitive was exercised through a compile-time-only hook on Pico
2 `2BED26ED20556535`; the hook is disabled in release builds and is not wired
into ordinary collection. A temporary probe CAP allocated two persistent
objects, cleared one reference, and verified that collection deleted exactly
one orphan body tag while the separately rooted value remained readable. The
test applet/package cleanup passed. This exposed a prefix bug in
`Heap::hasReachablePersistentTagPrefix`: a marked root incorrectly protected
all descendants. The check now recognizes only the exact object tag and its
direct two-byte field/element records; nested objects require their own marked
record. Host regression and ARM/RISC-V debug builds pass.

Physical evidence is in
`artifacts/logs/persistent-reclaimer-monitor-2BED-20261031-fixed.log` and
`artifacts/logs/persistent-reclaimer-physical-20261031T052908Z.log`; hashes and
details are recorded in `.agent/QKB.md`. No physical power interruption was
performed. Keep the primitive out of default runtime collection until an
interrupted erase is tested on hardware and recovery/retry is verified.

The latest `max_crypto` ARM empty-card image is prepared for external testing.
Both wrapper builds passed with debug flags off. The UF2 at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` is 2,088,448
bytes, SHA-256 `cf21bae06443f3b7e0e8d6150eb2927cbaecc908fe6cca19fd435791e8d4d2f2`,
and compares byte-for-byte with `build/arm/pico2_choupi.uf2`. Its README and
support manifest are alongside it. No physical flash was done for this
external artifact refresh. The release image has the persistent-GC debug hook
disabled.

## Persistent reclaimer primitive — 2026-10-31

Added `PersistentReclaimer::reclaimUnreachableAppletTags` in
`choupi/src/persistent_inventory.cpp`. It is a bounded-memory opt-in primitive,
not called from normal runtime collection. It refuses active Java Card
transactions, performs a full read-only inventory preflight before deletion,
and then re-traces roots and gathers at most eight candidate tags per batch.
It erases only after each cursor walk returns, then starts a fresh walk, so
filesystem mutation never occurs while traversing a cursor. An erase failure
can leave a subset of already-proven unreachable tags removed; a later call can
continue safely.

`./tools/test-persistent-transaction` passes the new cases: preserve reachable
tag subtrees and other owners, reject active transactions, fail closed on
incomplete roots and cursor stalls, and retry after an injected erase failure
using a fresh heap. `./tools/build arm --profile max_crypto` passes and
`./tools/build riscv --profile max_crypto` passes its VM-core build. The test
harness now explicitly compiles as `PC_VERSION` and supplies the ROM/byte-write
stubs required by this additional erase path.

The ARM build changed after this addition, so the external max_crypto image
was refreshed and compared byte-for-byte with `build/arm/pico2_choupi.uf2`.
The current artifact is 2,087,936 bytes, SHA-256
`9d32fdfeb19645a016a70f6d0ab9499c4ce2f127d4c737a455a73b2dedbbbd89`.
`artifacts/firmware/README-max_crypto.txt` records this size and hash.

No physical power cut was tested, no token data was deleted, and this primitive
is not wired into the normal collector. Keep that gate closed. Next, add
The RP2350-overlay model now stages ten owner-tag erases (more than one
reclaimer batch) and injects 1,000 randomized power cuts during COW snapshot
commit. `./tools/test-rp2350-fs-overlay` passes: every reboot restores either
all ten old candidates or none, the live root remains intact, and a retry after
the old snapshot wins removes every candidate. This still does not call the VM
reclaimer or establish physical board power-loss behavior. Next, validate an
actual USB power interruption with an identified Pico, confirm the applet root
survives, then verify cleanup can resume before considering runtime activation.
Preserve the dirty worktree snapshot that predates these steps.

## Complete physical max_crypto JCAlgTest run — 2026-10-31

The official unchanged JCAlgTest 1.8.2 client completed
`ALG_SUPPORT_EXTENDED` on Pico 2 `2BED26ED20556535` with exit status 0. The
report contains all 4,525 normalized identities and matches jCardSim row for
row. Results: 198 yes, 4,327 no, zero other statuses. Against the generated
147-entry `max_crypto` declaration, only RSA and RSA-CRT `KeyPair` at 3072 are
missing: the upstream CAP queries 4096 for those two rows. A temporary expected
file excluding only these known malformed rows compares the remaining 145/145
capabilities exactly, with no unexpected positives. Keep the upstream CAP
unchanged and retain this mismatch in reports. This completed report supersedes
the earlier 900-second partial run for the refreshed `max_crypto` image; it
does not close the standalone `symm_crypto_ctr` profile, Windows acceptance, or
other project gates. CSV SHA-256
`b3aa5964c499a16e812c9b336712415127874a7a3d638d8989404dca51b30d81`; raw
client log SHA-256
`641a9057fc990f6a717190880902bec74a242764aec2d3018cd0bc53e451c09f`.
Artifacts are in
`artifacts/results/jcalgtest/max_crypto_full_2BED_20261031/`.
The reusable comparison is `python3 tools/compare-jcalgtest-support.py` with
the generated `build/javacard-support/jcalgtest-supported.csv`; its expected
failure is exactly the two upstream-labeled KeyPair rows. A filtered temporary
expectation gives `COMPARE:PASS` for the other 145 identities.

## Standalone symm_crypto_ctr physical validation — 2026-10-31

Both `./tools/build arm --profile symm_crypto_ctr` and
`./tools/build riscv --profile symm_crypto_ctr` passed. The first cycle command
used target `arm`, which flashes the small random-stream smoke image; it passed
its smoke test but is not JCVM evidence. The correct
`PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile symm_crypto_ctr
--serial 2BED26ED20556535` then flashed the full ARM JCVM image and passed its
18-second finite monitor with repeated `TEST:PASS choupi_runtime`, no
`TEST:FAIL` or `FAULT`. The image is 1,828,352 bytes, SHA-256
`8e5e727543fe29d72fa9c6a55d835fef42cdb7d81c8a1e8a26a24e6cf5f0ec8c`.
After reboot, PC/SC initially listed only VPCD readers; the authorized helper
granted access to the Pico node and a subsequent scan registered the exact
reader. The unchanged 1.8.2 report was saved in
`artifacts/results/jcalgtest/symm_crypto_ctr_full_2BED_20261031/` and completed
with exit status 0. The CSV matches all 4,525 jCardSim row identities and
contains 75 yes, 4,450 no, zero other. The generated 48-entry expected
manifest matches exactly: 48 expected and actual, zero unexpected or missing.
Both the Cipher `ALG_AES_CTR` and `CIPHER_AES_CTR PAD_NOPAD` rows report yes.
CSV SHA-256 `4dd12ac4de12f0cc127b0af090d5fab32a4eb7ce8cb3d8bfd178b4ecb4ec1f98`;
raw log SHA-256
`3b190ad422fe0752255f8b8397a5ab796199875845058df14be26f521480cf8d`.
The generated 48-entry expected manifest is
`build/javacard-support/jcalgtest-supported.csv` (SHA-256
`cdfc7ed0923a63740d4da8747e5101078fa66e533f350326a60c2175918791b5`).
Cycle log: `artifacts/logs/cycle-choupi-20261031T035348Z.log` (SHA-256
`95486ebfff85c265434154f1e71e2bb184062045a816f566209fb66471326c2b`).

## Maximum-support ARM external-test artifact refreshed — 2026-10-31

The downloadable ARM `max_crypto` UF2 was stale relative to the image built
and flashed in the latest cycle. Refreshed
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` from
`build/arm/pico2_choupi.uf2`. Both are 2,086,400 bytes and have SHA-256
`2bc214ba3fdb3d020a549233bdaeb9c22fa71774118c97e742ec39219a26bd29`; `cmp`
passed. The board is Pico 2 `2BED26ED20556535`, flashed successfully with a
passing finite runtime monitor. Its physical JCAlgTest
`ALG_SUPPORT_EXTENDED` run hit the 900-second wrapper limit at 1,155/4,525
rows (179 yes, 976 no), without a logged transport error; it is partial.
Artifacts are in `artifacts/results/jcalgtest/max_crypto_physical_2BED_20261031/`.
The physical run exposed stale CPLC dates `6302` (October 29). Updated the
native bytes and exact host/native fixtures to `6304` (October 31). The GET DATA
unit test passes 14/14; ARM and RISC-V `max_crypto` wrapper builds pass. The
ARM cycle flashed and verified the image on 2BED, and the finite monitor passed
with repeated `TEST:PASS choupi_runtime`, no `FAULT`. Open and SCP03-authenticated
physical `9F7F` probes both returned the exact new `6304` bytes and `9000`; the
reader was enumerated after running the authorized PC/SC helper. The external
UF2 now matches the flashed ARM build byte-for-byte: 2,086,400 bytes, SHA-256
`f97cb0694a516d6dd0de3e07e868afe95f1b5bbac9e60407733d277761dcd776`; it
matches `build/arm/pico2_choupi.uf2`. Physical APDU log:
`artifacts/logs/physical-gp-get-data-cplc-2BED-20261031.log` (SHA-256
`b4a4bb53964e98ee2c8541c00b7f4a0687ae9cedc1e8cd40e90e12b511708e42`). The
initial JCAlgTest attempt was partial at 1,155/4,525 due its 900-second bound;
the completed follow-up is recorded in the entry above.
Preserve unrelated dirty files listed in prior handoff sections.

## Physical AES-CTR known-answer check on max_crypto — 2026-10-31

The flashed `max_crypto` superset image passed eight NIST SP 800-38A AES-CTR
vectors on Pico 2 `2BED26ED20556535`: single-shot and 19-byte-split streaming
encrypt/decrypt through both the single and cipher/padding factory paths. The
official support partial also contains `ALG_AES_CTR;yes` and
`CIPHER_AES_CTR PAD_NOPAD;yes`. The custom probe CAP was installed through
GPPro and then deleted; a final GPPro list shows the existing JCAlgTest applet
still selectable and the probe package absent. Logs are listed in TODO and
QKB. This does not close the exact `symm_crypto_ctr`-only profile acceptance or
the complete support report.

## Focused SHA-256 static performance on max_crypto — 2026-10-31

The profile-aware host probe `tools/probe-jc-algtest --sha256-static-only`
completed on physical Pico 2 `2BED26ED20556535`. SHA-256 prepare, five
zero-repeat baselines, two 50-call static measurements, ten one-call repeats,
another 50-call measurement, and a post-static call all returned `AA9000`.
Baselines averaged 1,680.2 ms; the two official-format 50-call APDUs averaged
1,979.5 ms, giving an approximate host baseline-corrected 5.99 ms per 256-byte
`doFinal`. This measurement includes PC/SC transport and VM overhead; it is not
the full official `ALG_PERFORMANCE_STATIC` report. The first attempt ran the
same SHA sequence successfully but exited nonzero because a later probe
incorrectly expected RSA-SHA1 to be unsupported on `max_crypto`; the new stop
option exits after SHA checks. Logs and hashes are in QKB.

## APDU incoming-length virtual dispatch fixed — 2026-10-31

The JC305 `APDU.getIncomingLength()` discrepancy is fixed. JC305 APDU token 15
had been mapped to embedded `isValidCLA()` and returned `1`. The mapping now
routes it to embedded token 16; `getOffsetCdata` maps to token 17 and
`isValidCLA` to token 15. The generated lookup CAP includes a regression APDU
with Lc=13 and asserts incoming length 13, data offset 5, and valid CLA. The
response is `0D0D05019000`; the VPCD multipackage lifecycle passes 37/37.
Commit `ebabbe1` expands this regression across all three getters. The PC
`max_crypto` build and ARM/RISC-V `max_crypto` wrapper builds pass. No physical
test was possible: the approved PC/SC helper granted access to two USB nodes,
but bounded `pcsc_scan -r` found no reader. The bug fix and regression are in
commit `0f80232`; the implementation and validation record are in
`progress.md`. Next,
continue the APDU/API backlog and pursue physical validation when a reader is
registered. Preserve the unrelated worktree snapshot listed below.

## GET STATUS continuation query binding — 2026-10-31

Pico CCID and PC VPCD now require a GET STATUS `P2=03` command to repeat the
previous query category and search bytes. The authenticated VPCD regression
passes 39/39. The ARM and RISC-V `max_crypto` builds pass, and the ARM
`max_crypto` image was flashed to Pico 2 `2BED26ED20556535`. GPPro sent four
secure APDUs in one physical session: the initial query returned `9000`,
changed category and changed AID search each returned `6985`, and the unchanged
continuation returned `9000`. The post-test CDC monitor passed. PC/SC is
currently registering that Pico. Logs:
`artifacts/logs/physical-get-status-continuation-2BED-20261031.log` and
`artifacts/logs/cycle-choupi-20261031T022105Z.log`. Commit `0885372` contains
the implementation and regression. Continue the remaining
GET STATUS pagination/lifecycle audit and the rest of TODO; the continuation
query gate itself is covered.

## GPPro empty-card VPCD integration and reader discovery retry — 2026-10-31

The unchanged upstream JCAlgTest 1.8.2 CAP installs successfully through GPPro
from a fresh empty VPCD image. The latest test confirms the card starts with no
JCAlgTest package/applet, then GPPro LOAD/INSTALL completes and `--list` shows
the package `LOADED` and applet `SELECTABLE`. The focused GET DATA unit suite
passes 14/14 for Card Recognition Data (`0066`), CPLC (`9F7F`), unknown tags,
and response-capacity handling.

The first test attempt timed out during its single PC/SC discovery scan. After
a direct line-buffered scan enumerated both VPCD readers, a rerun passed. The
preflight now uses line-buffered logs and up to three bounded scans; its latest
full integration passed at
`artifacts/results/gppro-empty-install-vpcd-20261031T021340Z/`. Retry exhaustion
was not fault-injected. Physical access remains unavailable: the helper grants
two Pico USB nodes, but PC/SC does not register either as a physical reader.
Next continue offline with remaining GET STATUS continuation and API gaps; do
not claim this VPCD test as physical validation.

The `./tools/test-short-apdu` harness also needed its local FFI stub updated
after the heap tracer began using `fs_exists`; that harness fix is committed as
`3e1bcbd`, and its 26 cases plus the 250,000-input parser fuzz run pass.
The related JC array test now supplies `JC_Instance::hasLoadedFields()` and
uses `0x3ffe` for an invalid local handle; `0x7ffe` is in the global-handle
namespace. `./tools/test-jcarray` passes all 27 cases; commit `f3722bf`.

The full current-source JCSystem transaction CAP run passed 85/85 VPCD checks
in 87.427 seconds, including restart restore, API/exception behavior,
`lookupAID()`, lifecycle callbacks, and GC pressure. Evidence is in
`artifacts/logs/jcsystem-transaction-apdu-vpcd-20261031T015450Z.log` and
validation commit `63aefd4`. Physical PC/SC remains unavailable.

Refreshed the downloadable max crypto ARM image from the latest passing
`./tools/build arm --profile max_crypto` output after the APDU token-map fix.
Artifact: `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`,
2,086,400 bytes, SHA-256
`c739e78a21763724596ffb81878447b69038f67f1ff63d83a7d548d0b5c980e9`.
The file compares byte-for-byte with `build/arm/pico2_choupi.uf2`; the expected
support CSV hash is unchanged. Commit `c73fc70`. No physical flash was done.

An eight-second CDC monitor on identified Pico 2 `2BED26ED20556535` then
returned success with repeated `TEST:PASS choupi_runtime` and no `FAULT`
(`CHOUPI:CCID rx_events=2761 tasks=1945`). This verifies startup liveness only;
PC/SC still lists no reader, and no APDU or flash was done.

## Current continuation — JCSystem.lookupAID permanent EPO behavior — 2026-10-31

`lookupAID()` and `getAID()` now return runtime-wide JCRE `AID` Entry Point
Objects. Global instance/array handles and dedicated persistent reference
records preserve identity across applet contexts, a simulated VPCD card reset,
and a fresh process. The authenticated multipackage lifecycle test passes
36/36; `./tools/test-rmi-registry`, PC max profile build, and ARM/RISC-V
max-profile wrapper builds pass. No physical test was possible: the approved
PC/SC access helper granted two USB nodes, but bounded `pcsc_scan -r` found no
readers. The `APDU.getIncomingLength()` diagnostic mismatch remains open in
QKB/TODO. The earlier same-context lookup implementation is committed at
`027c384`; the global EPO implementation and validations are committed at
`03356ba`. Next, isolate the APDU incoming-length mismatch, then continue the
prioritized API/native backlog. Preserve unrelated worktree changes listed in
the session snapshot below.

## Declared Java Card compatibility target — 2026-10-31

`docs/supported-profile.md` now states the Java Card Classic 3.0.5
compatibility target, accepted CAP 2.1/2.2 input scope, native JCVM/CAP
execution boundary, selected crypto-profile behavior, and known exclusions.
This is explicitly a compatibility target rather than full specification
conformance. The TODO profile item now tracks the remaining per-API behavior
test audit.

Validation was by source/configuration inspection only; no firmware build or
test was needed for this documentation step. Physical PC/SC remains
unavailable: the approved helper grants pcscd access to two USB nodes, while
bounded `pcsc_scan -r` returns no readers. Continue with offline correctness
work; do not enable persistent-record deletion until USB cold-power recovery
passes on hardware. Profile declaration commit: `5ebbfe5`; its progress-SHA
closeout is a separate follow-up commit.

## CAP 2.2 external remote-interface linking and VPCD test recovery — 2026-10-31

Applet installation now resolves each external CAP 2.2 remote-interface
reference with `ConstantPool_Handler::classref2interface`, requiring an
exported interface in an installed package; CAP 2.1 is skipped before reading
the CAP 2.2 signature-pool offset. Export tokens are bounds-checked before
lookup. ARM and RISC-V `max_crypto` wrapper builds pass.
`./tools/test-cap-parser` passes 4,205 checks, 100,000 fuzz inputs, and 48
corpus CAPs. The official SDK JCRMI VPCD client passes 18/18 on final source.
`tools/test-rmi-external-link-vpcd` uses two generated CAP packages and passes:
valid external interface INSTALL (`9000`), exported class rejection (`6F00`),
and missing Export token rejection (`6F00`).
The final-source authenticated multipackage VPCD regression also passes 25/25
after this bounds fix.
The authenticated VPCD multipackage regression passed 25/25 after this
refactor, including two-package LOAD/INSTALL, three applets, restart restore,
and deletion. Those CAPs use no CAP 2.2 external remote-interface target.
Regression record commit: `25a97f4`.

The unchanged JCAlgTest GPPro LOAD/INSTALL check is currently blocked by host
VPCD registration. `Virtual PCD 00 01` reported present but GPPro returned
`Unpowered card`; the wrapper now targets the usual `Virtual PCD 00 00` port
35963 and bounds the discovery scan, but that scan returned no output. After
the authorized reader helper restarted `pcscd`, service status was active yet a
bounded `pcsc_scan -r` found no readers. No physical card was flashed. Inspect
the VPCD backend/service and rerun GPPro install before claiming CAP 2.1
installation regression coverage. Preserve unrelated `src/cap_load.cpp`
changes; this step adds one loader call to that file.

Current external-test ARM image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`, 2,077,184
bytes, SHA-256 `5e8a7cb2267312821b4622729d1f3e994ffc4e7f582c964ffdae85a4d491e53a`.
The missing GPPro/PC-SC registration check remains environmental; return to
persistent-object reclamation and actual USB power-cycle recovery after the
cross-package RMI resolver work recorded below.
Implementation commit: `ed5511b`.

## CAP 2.2 external remote-interface integration tests — 2026-10-31

`validateRemoteInterfaceTargets` owns the Class-component traversal used by
install-time remote-interface linking. The generated two-package VPCD fixture
exercises its actual Import/Export resolver: a valid external interface
installs with `9000`; a class target and an out-of-range Export token both
fail safely with `6F00`. The missing token case found unchecked Export-table
indexing, fixed by validating the mapped export token against `class_count`
before calling `classref2interface`. The unchanged SDK JCRMI client passes
18/18, parser tests pass (4,205 / 100,000 fuzz / 48 CAPs), and ARM/RISC-V
max-profile builds pass. No physical board was flashed.

The refreshed max-crypto ARM UF2 is 2,077,184 bytes, SHA-256
`5e8a7cb2267312821b4622729d1f3e994ffc4e7f582c964ffdae85a4d491e53a`.
External-install logs:
`artifacts/logs/rmi-external-link-valid-interface-20261031T002457Z.log`,
`artifacts/logs/rmi-external-link-exported-class-20261031T002523Z.log`, and
`artifacts/logs/rmi-external-link-missing-export-20261031T002550Z.log`.
The official SDK client log is
`artifacts/logs/rmi-client-official-vpcd-client-20261031T002708Z.log`.
Next: resume persistent-object reclamation and cold USB power-cycle recovery.
Implementation commit: `1741120`.

## Persistence safety validation and physical reader state — 2026-10-31

Current-source recovery checks pass offline: `./tools/test-rp2350-fs-overlay`
ends with `TEST:SUMMARY fail=0`; the elevated host-access VPCD persistence run
passes install/commit 3/3 and fresh-process restore/GC pressure 38/38 over four
rounds. These do not establish USB power-loss recovery. Two Pico2 JCVM CCID
devices enumerate as USB `2e8a:0009` at nodes 001/015 and 001/017, and the
approved reader helper grants pcscd access to both. `pcscd` is active, but
bounded `pcsc_scan -r` still times out without readers. No physical APDU or
flash was possible. Continue offline with remaining independent work; do not
enable persistent-object deletion until actual cold USB power-cycle recovery
has been verified.
Recovery evidence record commit: `3b2d03f`.

## CAP 2.2 external remote-interface Import indexes — 2026-10-30

CAP 2.2 external remote-interface references now validate their seven-bit
package token against the complete Import component, including when CAP
components are in a different order. Missing Import and out-of-range token
fixtures are rejected; valid imported references parse. `./tools/test-cap-parser`
passes 4,205 checks, 100,000 fuzz inputs, and 48 CAP corpus files; both
`max_crypto` ARM and RISC-V wrapper builds pass. The official SDK JCRMI VPCD
probe passes 18/18. Refreshed ARM external-test image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`, 2,075,648 bytes,
SHA-256 `3f2bdf483acb086e465f51ece612e8b6487e7be4ae958d5158d8c1c896836acd`.
No physical board was flashed. Imported class tokens still need to be checked
against the target package's exported interfaces at installation. Next, pursue
this CAP-link validation, then return to top-priority safe persistent-object
reclamation and actual USB power-cycle recovery. Preserve unrelated dirty
`src/cap_load.cpp` edits.
Implementation commit: `6bd2619`.

The official client log is
`artifacts/logs/rmi-client-official-vpcd-client-20261030T233752Z.log`; the
emulator log is
`artifacts/logs/rmi-client-official-vpcd-emulator-20261030T233752Z.log`.

## CAP 2.2 Directory layout and official SDK JCRMI integration — 2026-10-30

The helper INSTALL failure (`stage=9 / 6F00`) is fixed. JCDK 3.0.5 CAP 2.2
Directory components carry twelve size entries; the validator had interpreted
the import count as a custom-component count. It now validates twelve entries
for CAP 2.2 and retains the observed eleven-entry CAP 2.1 layout. The exact
converter Directory fixture passes. `./tools/test-cap-parser` passes 4,203
checks, 100,000 fuzz inputs, and 48 CAP corpus files; the generated RMI CAP
passes standalone parsing. ARM `max_crypto` and RISC-V wrapper/core builds
pass. The official SDK 2.2.1 JCRMI VPCD probe passes all 18 checks.

Refreshed empty-card max ARM image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`, 2,075,136 bytes,
SHA-256 `efb5fca5c96365e98a56a20d764807f9e35d1ecb841f0fe24cc8046ba7b0de36`.
The VPCD logs are `artifacts/logs/rmi-client-official-vpcd-{client,emulator}-20261030T232628Z.log`.
No physical reader was registered, so this was not a board test. Implementation is committed as `5f82ed8`; preserve unrelated dirty changes, especially `src/cap_load.cpp`.

## CAP 2.2 internal remote-interface references — 2026-10-30

The parser now records internal interface-record boundaries in a bounded
15-bit bitmap and checks remote interface references after structural parsing,
so forward references work. An internal target must point to an interface
record; offsets inside records and class-record offsets are rejected. External
references are retained for later installed-package resolution. Parser test
passes 4,200 checks, 100,000 fuzz inputs, and 48 CAP corpus files; ARM and
RISC-V `max_crypto` builds pass. Fresh GPPro LOAD/INSTALL/list on PC VPCD
passes for the unchanged JCAlgTest CAP. No physical validation was performed.
The refreshed empty-card max ARM image is 2,075,136 bytes, SHA-256
`78cd6aa1e6d1d24e8d195087b527ea49e4f5fe101ca0dac14d1a19526cec8960`.
Implementation is committed as `ffab347`; preserve unrelated worktree
changes. The JCRMI helper INSTALL failure was resolved by the CAP 2.2 Directory fix recorded above.


## CAP 2.2 remote signature validation — 2026-10-30

`choupi/src/jc_handlers/jc_cap.cpp` now validates remote method hashes in
ascending order and requires each `signature_offset` to be both inside the
signature pool and at a descriptor start. A converter-emitted JCDK 3.0.5 CAP
fixture has positive coverage plus interior-offset, out-of-range-offset, and
descending-hash rejection cases in `tools/tests/cap_parser/cap_parser_test.cpp`.
`./tools/test-cap-parser` passes 4,194 checks across 100,000 fuzz inputs and 48
CAP corpus files. It also rejects remote package-method tokens and public
tokens outside the class method table. Max-profile ARM and RISC-V wrapper
builds pass, and GPPro VPCD LOAD/INSTALL/list succeeds for unchanged JCAlgTest.
The PC VPCD official JCRMI client
test currently fails before connection: helper applet INSTALL returns stage 9
`6F00`; earlier 2026-10-25 client success remains historical. The cause is
unresolved and is recorded in QKB. Do not claim fresh JCRMI integration pass.

Parser code, regression cases, TODO/QKB/handoff updates, and their progress
record are committed as `69708c5`, separately from unrelated dirty files.
Preserve current edits in `src/cap_load.cpp`, `AGENTS.md`, and other pre-existing
worktree entries. Next, investigate the INSTALL failure and the missing
captured diagnostics, then continue the prioritized TODO; physical PC/SC had
only VPCD readers on the latest recorded scan.

## Refreshed external-test max-crypto ARM image — 2026-10-30

Rebuilt current source with `./tools/build arm --profile max_crypto` (147/150
capabilities) and refreshed
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`: 2,074,112 bytes,
SHA-256 `8ef6492df92514d9a677f25d8d2bb1eff59c28160fe2609063b9b311d8ed2727`.
The companion expected-support CSV is unchanged (SHA-256
`81824f2bdde2f48f39cf3a3e3da942bf89a81317f32f218a8955dce8b455dee0`). ARM
image inspection and `./tools/build riscv --profile max_crypto` passed; no
device was flashed. The approved reader helper found two USB nodes, but a
bounded `pcsc_scan -r` timed out without output, so physical card availability
is not established. The image README has the current hash and size.
The refreshed artifact and progress record are committed as `c9aa6c4`.
The CAP 2.2 public method-token checks, converter fixture regressions, and
refreshed current-source UF2 are committed as `3ed1a42`.

Current-source PC VPCD vector reruns also pass: `symm_crypto` 73/73 crypto
vectors (75 checks), `asymm_crypto` 97/97 (99 checks), and
`symm_crypto_ctr` 8/8 CTR vectors (10 checks). The wrappers build focused
profiles, not one combined `max_crypto` run. Hashes and logs are in QKB. These
are emulator checks; no Pico was visible through PC/SC for physical validation.
The evidence and progress record are committed as `3ede9fd`.

Read `AGENTS.md` and `.agent/SESSION_CONTINUITY.md` before work. The prioritized
opening section of `TODO.md` tracks active scope; later historical checklists
may be stale. `progress.md` is the append-only work record.

## External-test max-crypto ARM image refreshed — 2026-10-30

Rebuilt `max_crypto` from current source with `./tools/build arm --profile
max_crypto`; the ARM wrapper passed and produced the empty-card image
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` (2,073,600
bytes, SHA-256
`4715fb57cdb87df343616bd4c1e5ea94debfc96ff5b4819312ef73590651c9d4`). It
enables 147 of 150 declared capabilities. Expected JCAlgTest rows remain in
`artifacts/firmware/pico2-jcvm-max_crypto-arm-supported.csv` (SHA-256
`81824f2bdde2f48f39cf3a3e3da942bf89a81317f32f218a8955dce8b455dee0`). The
artifact README has flashing and validation notes. This is a build-only
refresh; no physical device was flashed or tested. The refresh is committed as
`0b90024`.

## Fresh VPCD multipackage lifecycle — 2026-10-30

After rebuilding the PC JCVM for `base`, `PICO2_HOST_USB_ACCESS=1
./tools/test-vpcd-multipackage` passed 25/25 checks: two packages and three
instances installed, GET STATUS and SELECT passed before/after process restart,
all three owner roots were inventoried, and delete/cascade-delete left an empty
registry. This exercises the current worktree, including the pre-existing
uncommitted `src/cap_load.cpp` changes; those edits were not committed here.
This is VPCD evidence only. Hardware enumeration and cold power-cycle recovery
remain to be checked separately. Log hashes and paths are in `.agent/QKB.md`.
The documentation record is committed as `1906ef2`.

## Latest continuation: VPCD emulator connection retry — 2026-10-30

`choupi/src/main_pc.cpp::connect_vpcd()` now retries local `ECONNREFUSED` for
up to roughly three seconds (31 attempts, 100 ms between attempts), fixing a
race where the PCSC VPCD listener could start after reader enumeration. PC
build passed and `PICO2_HOST_USB_ACCESS=1
./tools/test-gppro-empty-install-vpcd` passed through the approved host-access
path. The clean VPCD test completed GPPro LOAD/INSTALL and verified the package
as loaded and applet selectable. The retry branch was not fault-injected in
this run. Details and output are in `.agent/QKB.md` and
`artifacts/results/gppro-empty-install-vpcd-20261030T205954Z/`.

This is emulator-only; no ARM/RISC-V build or physical flash was needed. Keep
unrelated worktree edits intact, especially `src/cap_load.cpp`, which remains
uncommitted from separate loader work.

## Current hardware access — 2026-10-30 21:09 UTC

Two Pico 2 devices, `8E464F4E2BBC0A19` and `2BED26ED20556535`, enumerate over
USB with CCID 1.10/T=1 descriptors, but a bounded `pcsc_scan -r` still reports
only the two VPCD readers. `tools/doctor` passes. No physical APDU, flash, or
USB power-cycle test is possible until PC/SC registers a physical reader. See
`.agent/QKB.md` for the USB/CCID diagnostic evidence; do not change firmware or
driver files without IFD daemon errors.

## Latest CAP parser hardening — 2026-10-30

Dynamic CAP validation now accepts only compact 2.1 and 2.2 Header versions.
It pre-reads the Header version so component order does not accidentally select
the wrong Class parser, and rejects Class layouts whose VMMT semantics are not
implemented (CAP 2.3+). `./tools/test-cap-parser` passes 4,186 checks over 48
supplied CAPs and 100,000 deterministic fuzz inputs. ARM and RISC-V
`max_crypto` wrappers pass; PC `max_crypto` passes. GPPro clean-card
installation and VPCD multi-package restart/delete pass (25/25). No physical
validation was possible. See the latest TODO and QKB entries. Builds included
the unchanged, unrelated uncommitted `src/cap_load.cpp`; preserve that file.

## Corrected profile comparison instructions — 2026-10-30

`readme.md` now saves the generated expected-support CSV after each `base` and
`current` profile build, compares each physical report against the shared
jCardSim reference, and shows a separate expected-set diff. A historical
2026-10-24 base report had three `OwnerPINBuilder` runtime errors. A fresh
current-source base-profile VPCD run on 2026-10-30 completed all 4,525
normalized rows (10 yes, 4,515 no, zero other), matched the generated 10/10
capability manifest, and all three `OwnerPINBuilder` rows returned `yes`.
Those historical errors did not reproduce. Current-profile report comparison
also passes against all 15 declared capabilities. Evidence and hashes are in
`.agent/QKB.md`; detailed files are under
`artifacts/results/jcalgtest-base-vpcd-current-20261030-rerun3/`. This is not
physical-card validation.
The documentation and result record are committed as `5093b6a`.

## Current continuation: optional native module gating — 2026-10-30

Fixed the `minimal` profile wrapper builds. The top-level Pico CMake source
list now excludes `native_aes_ctr.cpp` from its glob and appends it only when
an AES-CTR capability is enabled; `native_signature.cpp` marks its digest-only
helper maybe-unused for no-digest profiles. ARM and RISC-V `minimal` builds
pass and omit `native_aes_ctr.cpp.o`; ARM and RISC-V `max_crypto` builds pass
and include it. `python3 tools/test-javacard-support.py` passes 14/14.

The verified ARM max-profile artifact was refreshed from the latest wrapper
build: `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`, SHA-256
`e013ff4f87331dc99980d9fe9edf10b0a1579838fa5b3feefb166beb93632340`,
2,073,600 bytes. Its support CSV remains 147 rows, SHA-256
`81824f2bdde2f48f39cf3a3e3da942bf89a81317f32f218a8955dce8b455dee0`.
No physical flash or validation was performed. USB/PCSC physical testing stays
blocked until a Pico reader is registered. Existing alignment warnings in
packed CAP structure headers still appear in builds.

## External-test image background: max_crypto ARM — 2026-10-30

User-requested external-test image is available at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` (refreshed SHA-256
is recorded above). The
`max_crypto` profile enables 147/150 capabilities by unioning the existing
`current`, `symm_crypto`, `asymm_crypto`, and `symm_crypto_ctr` profiles; it
leaves the card empty for explicit CAP installation. The companion expected
JCAlgTest rows are in
`artifacts/firmware/pico2-jcvm-max_crypto-arm-supported.csv` (147 rows).
Symmetric, asymmetric, and CTR VPCD KATs passed 71/71, 97/97, and 8/8.
No physical flash or hardware test was performed. A bounded PC/SC check did
not return, so physical availability remains unconfirmed.
The AES-CTR source/profile step is committed as `1ea9c7e`; the max image/profile
step is committed as `8a2c5ec`. See the latest progress record for the full
build and test evidence. Usage instructions and file hashes are in
`artifacts/firmware/README-max_crypto.txt`.

Native `Cipher.ALG_AES_CTR` support and selectable profile `symm_crypto_ctr`
are implemented. The standalone profile's physical report now passes all
4,525 rows, with exact 48/48 generated-manifest parity on Pico 2 2BED. Previous
profiles (`current`, `base`, `no_aes`, `minimal`,
`symm_crypto`, `asymm_crypto`) retain their capability selections. The CTR
native path stores the counter in the existing Cipher IV array, stream block in
the otherwise-unused modulus array, and partial-block offset in
`modulusLength`; no ROM class field layout changed. `doFinal()` resets stream
state to the initialized counter. The test-only applet only invokes the API;
cryptographic work stays in C++.

Validation: support generator 12/12; dedicated PC build includes
`native_aes_ctr.cpp`; VPCD NIST SP 800-38A AES-128 CTR single algorithm and
paired factory, one-shot and split streaming encrypt/decrypt 8/8 (split after
19 bytes to exercise a partial keystream block); ARM and RISC-V wrapper builds
passed for `symm_crypto_ctr`. The generated support set has 48 entries. ARM
UF2 is 1,815,040 bytes, SHA-256
`4c42949db4b318cfa6658e10ba8fc7cb88be715d12292f0191a9468c636cf65b`.
The final elevated `pcsc_scan -r` did not return within the bounded check; no
physical reader was confirmed, so no flash or hardware claim was made. The
supplied JCOP4 1.7.9 matrix has no AES-CTR row; the dedicated profile is an API
extension and not claimed matrix parity. VPCD log:
`artifacts/logs/aes-ctr-vpcd-20261030T185802Z.log` (SHA-256
`f057331e506e4ae1b764915bfdb4c37606f6b65dc0a3e30a649e13eb102555ad`).

The requested standalone physical support comparison is complete. The NIST
CTR KATs were previously run on the `max_crypto` superset; repeat them on the
exact standalone profile if future changes touch CTR dispatch or profile
linking. Continue the remaining TODO items and preserve unrelated dirty
changes.

Full pre-compaction history: [archived handoff](archive/2026-10-29-pre-compaction/HANDOFF.md).
The archive is historical evidence, not current instructions or current board state.
[Issue knowledge base](QKB.md) contains current troubleshooting and a historical index.
Archive integrity is recorded in `archive/2026-10-29-pre-compaction/manifest.json`.

## Latest physical profile validation

On 2026-10-30, Pico 2 `2BED26ED20556535` completed the unchanged JCAlgTest
1.8.2 `ALG_SUPPORT_EXTENDED` matrix on the existing `symm_crypto` ARM image:
4,525 normalized rows, 73 yes, 4,452 no, zero other. The generated support
manifest matched all 46 declared positives exactly. jCardSim row identities
matched. A default 900-second runner timeout produced only a partial report;
`JCALGTEST_TIMEOUT_SECONDS=3600` completed in about 27 minutes. The finite
post-run monitor passed. Detailed commands, hashes, and logs are in QKB's
“Physical JCAlgTest `symm_crypto` support report” section. No source or card
firmware was changed; no CAP was installed or deleted. Persistent-object
deletion remains disabled pending actual USB power-cycle/recovery validation.
Documentation and evidence summary commit: `daddc01`.

The supplied JCOP4 P71 symmetric matrix audit is complete: all 41 target
positive identities match the generated `symm_crypto` profile and the physical
2BED report; existing symmetric KAT coverage is 71/71. See QKB for the exact
comparison log/hash. Do not infer AES-CTR support from this matrix. Continue
with remaining backlog; persistent-record deletion remains disabled until
physical USB power-cycle recovery is validated.
Parity audit documentation commit: `6183ebf`.

The `asymm_crypto` profile audit also matches all 136 shared JCOP4 target
identities. Its current generated set has 145 entries; the physical 1.8.2
report is 143/145 because the unchanged upstream CAP sends RSA/RSA-CRT 4096 on
two rows labeled 3072, while direct 3072 generation vectors pass. Regenerate
the expected set from current config for comparisons; the cached external-test
CSV is stale. See QKB for exact logs. API `/deploy` result corrections and
private-key hardening remain open.
Asymmetric profile audit documentation commit: `01b826e`.

## Physical Service-profile recheck and PC/SC reader selection

The earlier asymmetric-profile 102/124 `EXC_RUNTIME` report from the unchanged
3.0.5 Service CAP has not reproduced on a correctly selected physical board.
Normal `asymm_crypto` firmware on Pico 2 `8E464F4E2BBC0A19` passed the complete
Service suite twice: 124/124 calls, CAP upload and cleanup successful. The
asymmetric image passed ARM and RISC-V wrapper builds, was flashed and verified
on 8E, and its finite startup serial monitor passed. The second successful
run was made with the normal firmware and `--pcsc-lib` only; its serial trace
showed the Service APDUs and normal runtime summaries. No firmware source
change was made. A temporary diagnostic trace was reverted after its own
124/124 passing run.

The deploy runner's `--pcsc-lib` was not passed to its Java helper that maps
reader names to client indices. It now supplies the same
`-Dsun.security.smartcardio.library` property to that helper and the test
client. Without the property, this host's Java SmartcardIO enumerates no PC/SC
readers even though `pcsc_scan -r` lists them; with it, 8E resolves to index 3
after two VPCD entries. Always provide
`--pcsc-lib /usr/lib/x86_64-linux-gnu/libpcsclite.so.1` on this host. This
resolves a real runner robustness issue, but does not prove that it caused the
older 102 runtime exceptions. Record any fresh failure with its exact reader
name and Java PC/SC library.

Current identified board state: 8E is on normal `asymm_crypto`; 2BED remains
on normal `symm_crypto`. No Service CAP remains installed after the tests.
Evidence and hashes are in the QKB section “Physical `/deploy` Service profile
recheck and Java PC/SC library.”

## Latest full physical Crypto test and reader state

On 2026-10-30, a full 3.0.5 Crypto run on 8E (`asymm_crypto`) stopped at 233
calls. The first transport failure occurred on the RSA-512 SHA-1 known-signature
case; four following calls reported the reader disconnected. Cleanup could not
find the 8E PC/SC reader. The same board still enumerated over USB with normal
CDC/CCID descriptors, and an eight-second serial monitor showed eight passing
runtime summaries and no `FAULT`, but PC/SC continued to list only 2BED. The
cause is unproven; do not count this as a complete Crypto result. The Crypto
CAP may remain installed on 8E; verify and uninstall it when PC/SC access
returns.

Using the other physical device as failover, the full unchanged Crypto CAP on
2BED (`symm_crypto`) completed 466 calls: 138 `OK`, 304
`UNSUPPORTED_PER_SPEC`, and 24 `NO_MEMORY` (`SystemException.NO_RESOURCE`).
Upload and cleanup passed. A read-only JCAlgTest registry/version/JCSystem-info
probe returned `9000` after the run, followed by eight passing serial runtime
summaries. The `NO_RESOURCE` origin is not identified; keep persistent
reclamation disabled pending its recovery acceptance requirements.

After the full run's successful cleanup, a focused rerun of the DES-64 and
AES-192 known-answer Cipher rows still returned `NO_MEMORY` / `NO_RESOURCE`
(2/2); upload and cleanup passed. These are persistent repeatable row failures,
not only late-run fallout. Firmware maps filesystem `OutOfFlash` to this status,
but the exact exhausted capacity path remains to be traced. Focused CSV hash:
`ad53a69876f892724a8be1727fbb48c93d2ac4b726f4435bce034ead36565c8a`.

Memory fields: following the full run, JCSystem-info P1=01 reported current
legacy persistent free memory `0x177A` (6,010 bytes). After the focused CAP
lifecycle, a clean version-only P1=00 probe reported `0x1751` (5,969 bytes) in
both the legacy short and extended fields. The 41-byte delta spans install,
tests, cleanup, and a failed probe attempt; do not attribute it to one call.
When using `tools/probe-jc-algtest` only for memory, include `--version-only`
after `--jcsystem-info-p1 00`; without it the tool continues to a SHA-256
prepare test and can fail with `SCARD_E_PROTO_MISMATCH`.

A concurrent serial trace during a repeat of the failing rows captured
`CHOUPI:FS_WRITE_FAIL op=new tag_len=14 size=4 files=6263 bytes=65533/65535`
followed by a `SystemException.NO_RESOURCE` path. The 4-byte record was
rejected with two bytes left in the 64 KiB overlay. Once the Crypto CAP was
uninstalled, JCSystem-info again reported 5,969 bytes free. This confirms the
immediate failure is overlay capacity during the loaded package's work; it
does not by itself prove persistent GC is the remedy.

**Superseded by the 2026-10-30 overlay-v3 validation below:** the index now
uses flag bit 6 for offset bit 16 and the bounded arena is 77,824 bytes. The
same 2BED board was flashed with `symm_crypto`; focused DES-64 and AES-192
known-answer calls passed, then the full unchanged 3.0.5 Crypto suite completed
466 calls (161 `OK`, 305 `UNSUPPORTED_PER_SPEC`, zero other results), with
successful upload and cleanup. Persistent GC is still a separate open design
item; do not enable record deletion without power-loss validation.

## Physical RSA NOPAD retest

The previously noted NOPAD physical retest is complete. On 2026-10-29, the
unchanged API 3.0.5 Crypto CAP passed all four `ALG_RSA_NOPAD` rows on Pico 2
`2BED26ED20556535` with the `asymm_crypto` image; CAP upload and cleanup both
succeeded. The surrounding 466-call area had one unrelated runtime exception.
The CSV and SHA-256 are recorded in TODO.md and QKB. Do not keep NOPAD on the
physical retest list; broader focused deploy reruns and private-key hardening
remain open.

On 2026-10-30, a current-image focused Crypto run on Pico 2
`8E464F4E2BBC0A19` also passed the four NOPAD rows plus the secp192r1 DH and
DHC known-secret rows (6/6 selected calls, `OK`; 460 unrelated calls filtered).
Crypto CAP upload and cleanup passed. CSV SHA-256:
`321cd73f1664deaac455729b2806b426185733a36fb3d3bc46009fd7263ecdbb`. The
remaining VPCD rerun item still covers other affected rows and is not closed by
this focused physical result.

## Refreshed Windows profile test package

The Windows bundle was rebuilt on 2026-10-30 with
`./tools/package-asymm-symm-windows-test`. It contains current ARM UF2s for
`symm_crypto` and `asymm_crypto`, regenerated expected support CSVs (46 and
145 positives), the unchanged upstream JCAlgTest CAP, GPPro, the official
client, comparator, reference CSV, and corrected run sheet. ARM and RISC-V
wrapper builds passed for both profiles. ZIP SHA-256 is
`26a1d6e6d34033c44a22915c8a41c644917de85fd04c5d6d3598606889708be3`; image
hashes and validation details are in the 2026-10-30 Windows-package progress
record. The README now supplies the required jCardSim CSV argument to the
comparator and explains the known 143/145 asymmetric query result. Windows
validation is still pending; this step did not flash or access physical cards.
The ignored loose file `build/external-test/expected-support-asymm_crypto.csv`
is still stale; use the CSV inside the refreshed ZIP.

## Latest physical reader check and power-cycle limit

On 2026-10-30, the approved reader helper and `pcsc_scan -r` found physical
Pico 2 `2BED26ED20556535`. The read-only `tools/test-pico-powerup` check listed
the installed JCAlgTest package/applet and received `9000` for SELECT,
GETVERSION (`1.8.2_jc305`), and JCSystem-info. Its completion marker now
explicitly says `power_cycle=not_verified_by_script`; it does not reset or
power off the device. An eight-second serial capture showed seven passing
runtime summaries and no `TEST:FAIL`/`FAULT`. The USB topology inspection
showed only a root hub, no `uhubctl`, and no exposed per-port VBUS control.
No physical power cycle occurred. Logs and hashes are in QKB. Do not treat
this check as power-loss recovery evidence.

## Current memory-reporting investigation

The TODO investigation is complete; measurements did not reproduce inflated
32-bit memory reporting, and no accounting correction was made. During a live
JCSystem-info APDU on physical Pico 2 `2BED26ED20556535`, debug counters showed
`mallinfo arena=46124 used=45188 free=936`, `brk=0x20048000`,
`__StackLimit=0x20080000`, gap 229,376, and API total 230,312 (`936 + 229376`).
The ARM map gives a 275,504-byte gross gap from static RAM end `0x2003CBD0` to
`__StackLimit`; committed arena plus the uncommitted gap equals it to alignment.

Controlled physical AES-128 construction under `symm_crypto` matched allocator
and API deltas exactly: persistent/reset-transient/deselect-transient types
`0x0F`/`0x0D`/`0x0E` consumed 464/608/608 transient bytes. Cipher factory
measurements were 1,288–1,456 bytes. The short API result is capped at 32,767;
JCAlgTest's `0x847f` scalar reset field comes from its own signed-short addition
of two 576-byte arrays. Reset and deselect report the same estimate because
this VM has one shared transient allocator. `PICO2_DEBUG_HEAP_GC=1` now emits
raw counters; the normal build omits that diagnostic. Both debug architecture
builds passed. After the experiment the full, normal `symm_crypto` image was
re-flashed and verified; the 18-second monitor passed repeated
`TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. One recurring
`WATCHDOG_RECOVERY ... cause=other` marker appeared during each cycle and is
still an unrelated open investigation. Board 2BED is currently running normal
symm_crypto firmware.

Evidence logs: JCSystem/Cipher baseline
`artifacts/logs/jcsystem-memory-probe-2BED-20261030.log` (SHA-256
`5515f87702c561c819ffc418af005749479f90e8a030d0a1a343ce9f12d5c74a`),
symm key constructors `artifacts/logs/jcsystem-memory-key-raw-apdus-2BED-20261030.log`
(SHA-256 `597514a8b0db7e7e3609ed9144c5ef7f950e054b39baa133174d182f2a21e14e`),
raw allocator serial `artifacts/logs/jcsystem-memory-raw-counters-2BED-20261030.log`
(SHA-256 `09da99e3278f03f333cfe4c18bd2aac910af4ee196eabbe959db03de2c08d7d5`),
debug cycle `artifacts/logs/cycle-choupi-20261030T062316Z.log`, and normal
restoration cycle `artifacts/logs/cycle-choupi-20261030T062730Z.log` (SHA-256
`fa71cbceeb1cc1bce5cdf8b896366922ccc3b17333071d28551a36e28309423e`).
The probe options `--jcsystem-info-p1`, `--cipher-support`, and `--key-support`
are in `tools/JCAlgTestApduProbe.java`.

## Next concrete work

1. Continue safe persistent-object reclamation for repeated API workloads.
   Stable per-instance persistent root identity is now implemented: new Pico
   CAP registry version 5 metadata records globally unique applet owner IDs,
   install journal `INS2` carries them, and PC VPCD sidecar VCR2 preserves
   them across process restarts. Legacy CAP metadata v3/v4, INS1 journals, and
   VCR1 sidecars remain readable with owner `0`. OS filesystem permissions
   allow trusted RuntimeEnvironment/Installer contexts to access these logical
   owner tags while keeping applet contexts isolated. Two same-class instances
   retain different IDs in overlay recovery tests; VPCD multipackage restart /
   DELETE previously passed 23/23 and JCSystem transaction applet passes
   85/85. ARM and RISC-V builds pass. New VPCD coverage confirms all three
   installed instance roots are visited before and after restart. Enumeration
   now requires one root per registered instance with matching owner/package/
   class metadata and valid AIDs; mismatch fails closed. Physical multi-instance
   install/root restore has not been tested. A post-APDU root audit found one
   interpreter context, with its native/runtime roots enumerated by Context and
   Heap visitors (`test-rmi-registry` passes). The JCArray heap regression now
   proves partial root enumeration, an invalid in-memory array edge, and a
   strict persistent-array child read exception each return an incomplete trace
   and reclaim zero heap handles. The FlashMemory transaction test rejects
   truncated/mismatched array-alias headers and aliases to scalar records. It
   now also links the actual Heap, JC_Array, JC_Instance, and strict FlashMemory
   decoder. Malformed object-field aliases, truncated reference-array aliases,
   and a self-referential array-alias record each make the mark incomplete and
   reclaim zero handles. Reference-array fixtures now include valid serialized
   array-object headers, so traversal reaches the malformed child records.
   A separate host case recreates a volatile heap over retained
   filesystem-fixture records, restores a valid persistent array-alias graph,
   and reclaims only an unrelated slot. The rebuilt PC VPCD
   `./tools/test-vpcd-persistence` now passes after a fresh emulator process:
   38 restore/GC checks including restoration of a reclaimed persistent array,
   nested aliases, and four GC-pressure rounds. This is not physical power-loss
   evidence. The offline RP2350 overlay suite also passes 75 checks, including
   1,000 randomized multi-record snapshot cuts, 1,000 install-journal clear
   cuts, and 1,000 cross-store registry/filesystem header cuts; those exercise
   the host storage model, not the card's collector during power loss. Keep
   persistent record deletion disabled until Pico 2 power-cycle recovery and
   interrupted-update behavior are validated.

   Latest opt-in inventory stress check: `PICO2_PERSISTENT_INVENTORY=1
   PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-persistence` passed the install /
   commit stage (3/3 checks) and the fresh-process restore plus four GC-pressure
   rounds (38/38). Both install-time inventory scans and all 37 restore-time
   scans completed with zero unreachable owner tags, invalid refs, edge
   failures, or root failures, including around GC runs reclaiming volatile
   heap handles. This is PC VPCD evidence only; keep record deletion disabled.
   Logs: `artifacts/logs/vpcd-persistence-commit-20261030T054451Z.log`,
   `artifacts/logs/vpcd-persistence-install-emulator-20261030T054451Z.log`,
   `artifacts/logs/vpcd-persistence-restore-20261030T054451Z.log`, and
   `artifacts/logs/vpcd-persistence-restore-emulator-20261030T054451Z.log`.

   Fresh verification on 2026-10-30 after the DES-MAC changes rebuilt the PC
   emulator and reran `./tools/test-vpcd-persistence`: install/commit passed
   3/3; fresh-process restore and four GC-pressure rounds passed 38/38. Logs:
   `artifacts/logs/vpcd-persistence-commit-20261030T203932Z.log` and
   `artifacts/logs/vpcd-persistence-restore-20261030T203932Z.log`. The three
   offline suites also pass: persistent inventory, persistent transaction,
   and RP2350 overlay/recovery (`TEST:SUMMARY fail=0` in each). No source
   changes were required by this rerun.

   Reader check immediately afterward: the approved helper granted access to
   two USB nodes, but `pcsc_scan -r` listed only `Virtual PCD 00 00` and
   `Virtual PCD 00 01`. The physical nodes were not confirmed as Pico readers;
   no physical power cycle or flash was attempted. Keep physical recovery
   validation open.

   Physical 2BED (`2BED26ED20556535`) remained available for read-only
   validation: the JCAlgTest package/applet was listed, SELECT, GETVERSION
   (`1.8.2_jc305`), and JCSystem-info returned `9000`, and an 8-second serial
   monitor produced eight `TEST:SUMMARY pass=1 fail=0` lines with no `FAULT`.
   No card contents were changed and no USB power cycle was attempted. Logs:
   `artifacts/logs/pico-powerup-persistence-20261030T055138Z.log` and
   `artifacts/logs/persistent-inventory-physical-monitor-2BED-20261030T0551Z.log`.

   Latest native change: persistent array-alias type resolution now follows
   nested aliases to a valid array terminal and rejects cycles at a 32-alias
   bound, matching strict reference restore. Host tests include a valid
   two-link alias chain through collection and cyclic chains through the type
   decoder/strict reader. `./tools/test-persistent-transaction` passes;
   ARM/RISC-V wrappers pass; VPCD persistence passes 3 install/commit and 38
   restore/GC checks. The ARM image was flashed/verified on 2BED; the finite
   monitor passed and read-only JCAlgTest SELECT, GETVERSION, and JCSystem-info
   APDUs returned `9000`. Image SHA-256
   `b4a26add8ef5bfc28b948633d2f4790caaf48aaaa7addcae8f236f57c1ab8de6`; cycle
   log `artifacts/logs/cycle-choupi-20261030T050119Z.log`; APDU log
   `artifacts/logs/pico-powerup-persistence-20261030T050239Z.log`.

   Host emulation now exposes a bounded-memory Rust-OS tag cursor; Pico
   firmware uses a matching cursor over the mutable overlay in
   `src/choupi_pico2.cpp`. The physical cycle passed startup, but did not call
   the cursor. Native read-only visitors now page applet-owned `0x03, owner`
   and package-static `0x02, package` tags, rejecting stalled/malformed cursor
   responses. CAP `reference_count` now generates each two-byte static slot
   tag after validating `2 * reference_count <= image_size`. Static roots now
   load from installed CAP metadata through a strict flash reader and the heap's
   package/field cache; missing slots mean null, but malformed or unreadable
   present records fail closed. Registered applet roots ending in `0xFF` are
   decoded for both class-index widths and checked against their serialized
   package/class identity. The opt-in PC-only post-APDU diagnostic
   `PICO2_PERSISTENT_INVENTORY=1` calls both runtime adapters and seeds the
   existing read-only heap tracer with selected-context roots plus discovered
   static/applet roots. A context-forwarding bug that overwrote the heap pointer
   was fixed. Unchanged JCSystem VPCD cases 14–16 and nested-alias case 82 pass
   (6/6 harness checks). Across four scans after setup, both adapters return
   `Complete`, with six static roots and one applet root; all loaded objects
   are marked reachable (`arrays=11/11`, `instances=7/7`, invalid refs=0, edge
   failures=0). The tracer now strictly loads serialized persistent object
   fields and persistent reference-array elements; malformed types/tag widths
   fail the walk, while transient and unpersisted objects use in-memory edges.
   Strict VPCD cases 14–16 and 82 pass. Root enumeration now reports
   completion, and the device collector loads static roots from every package
   reported installed plus registered roots for every installed applet instance
   before marking; any inventory error skips collection. Registry enumeration
   validates AID lengths and requires exactly one root matching the instance's
   owner/package/class metadata. CAPs without a Static Field
   component contribute no static roots. The ARM runtime and PC diagnostic
   share the same package enumeration. Physical 2BED JCAlgTest
   SELECT/version/JCSystem-info APDUs all returned `9000`, with serial showing
   each collection reach `gc_end` and no `HEAP_GC_SKIP`. A 7-byte array-body
   tag ending in `0xFF` was misclassified
   as an applet root; exact 5/6-byte recognition fixed it. PC/ARM/RISC-V builds
   and the inventory unit test pass. Full physical JCSystem transaction-probe
   installation could not run on 2BED because only 8,821/65,535 bytes were
   free; 8E stopped enumerating after flash. New instances have stable distinct
   owner IDs and registered roots are now enumerated per installed instance.
   Physical multi-instance installation and power-cycle recovery remain open;
   no persistent records have been deleted.
   The 2026-10-30
   all-package static-root change is validated on 2BED with the full ARM image
   and three read-only APDUs; log:
   `artifacts/logs/persistent-inventory-all-package-device-monitor-2BED-20261030.log`.
   Continue those gaps before authorizing persistent-record reclamation.
   New read-only consumer step (2026-10-30):
   `PersistentInventory::visitUnreachableAppletTags` now traces supplied
   roots first and reports owner-scoped serialized tags outside all marked
   persistent-object tag subtrees. Incomplete tracing returns
   `IncompleteReachability` before candidate callbacks. Host transaction tests
   cover a reachable reference-array alias graph, an orphan in the same owner,
   another owner's records, and malformed reachable data; both persistent
   transaction and standalone inventory suites pass. ARM and RISC-V builds
   pass, and the current ARM image flash-verifies and passes the finite startup
   monitor on physical 2BED. The consumer is now connected to the opt-in PC
   VPCD diagnostic, which supplies context roots plus installed static and all
   registered applet roots, then scans all owners. Fresh-process multipackage
   validation passed before and after restart with 3/3 owner scans complete,
   14/14 arrays and 4/4 instances reachable, zero invalid refs/edge failures,
   and zero candidate tags. A separate debug-only physical path is enabled with
   `PICO2_DEBUG_HEAP_GC=1 ./tools/build arm`; after a complete post-APDU heap
   trace it scans the selected legacy owner read-only. On physical 2BED the
   unchanged JCAlgTest 1.8.2 applet returned `9000` to SELECT/version/JCSystem
   info; the inventory output reported owner 0, 71 static roots, one applet
   root, complete statuses, and zero candidate tags. The physical log is
   `artifacts/logs/persistent-inventory-physical-monitor-2BED-20261030-final.log`
   (SHA-256 `aed092de9894cf7688a30dbd663148b9a1f4d1da7787a6a2cfa90294907f5879`);
   APDU log
   `artifacts/logs/pico-powerup-persistence-20261030T053745Z.log` (SHA-256
   `3941c7bba8baf6783b60d66bed9108e65d4376a5963c77bccd9ea6b33c2c8394`).
   This is one legacy owner-0 applet, not physical multi-instance or power-cycle
   coverage. No persistent records were deleted. Current ARM image SHA-256
   `2f1d13a2a539a890bb9a914c2f5759595d6d13b14ceec39c9d4ea3a2ef16b10e`;
   cycle log `artifacts/logs/cycle-choupi-20261030T053625Z.log` SHA-256
   `b0252c9ea524691652ea83e34c5f376f25f28fadb3e706b0af4658d08b09de0e`.
   Cursor handling is now explicit: the candidate callback is streaming and
   receives provisional results; a cursor stall after the first result returns
   `InvalidCursor`, so callers must discard the partial list unless the final
   status is `Complete`. The host transaction test verifies the stall leaves
   both records intact. Next: validate read-only owner scans on hardware with a
   suitable multi-instance fixture, then continue interrupted-update and real
   power-cycle recovery before any persistent-record deletion.
2. Remaining acceptance/hardening: Windows validation, physical power-cycle
   checks where still open, and private RSA hardening.
   Available-memory accounting and JCOP4 P71 symmetric coverage are recorded
   investigation requests, not implemented features.

## Latest collector regression fix — 2026-10-30

Physical SHA-256 `doFinal` had returned `FF01` after a successful setup APDU.
The collector retained persistent references from strict flash snapshots but
missed newer references in a loaded object's in-memory field cache. It could
therefore reclaim the digest instance between APDUs. `Heap::visitChildReferences`
still validates serialized records strictly, then also traverses cached fields
only when the cache is loaded and current for the rollback generation. A new
host regression confirms the cached persistent child survives while unrelated
garbage is reclaimed.

Validation on 2026-10-30: persistent transaction test passes, ARM and RISC-V
`symm_crypto` wrappers pass, and `./tools/test-vpcd-persistence` passes install /
commit 3/3 plus restore / GC 38/38 after process restart. On 2BED
`2BED26ED20556535`, the ARM `symm_crypto` image was flash-verified and its
18-second monitor had 18 passing runtime summaries and no `TEST:FAIL`/`FAULT`.
The JCAlgTest SHA-256 prepare and doFinal APDUs both returned `AA9000` in three
consecutive probes. Current image SHA-256:
`18a7a230adf2e1719c26e1f79ccd48f60e93ae108c4b989ade4a2a89d943604e`. Logs:
`artifacts/logs/cycle-choupi-20261030T105252Z.log`,
`artifacts/logs/sha256-dofinal-loaded-cache-2BED-20261030.log`, and VPCD logs
with timestamp `20261030T105520Z`. The one `WATCHDOG_RECOVERY ... cause=other`
marker remains unexplained and separate from these passing checks. The stale
QKB note is now marked fixed.

## Physical `ALG_PERFORMANCE_STATIC` follow-up — 2026-10-30

Two normal `symm_crypto` runs on physical 8E disconnected at the third measured
DES CBC PKCS5 `Cipher_init()` transfer with `SCARD_E_NOT_TRANSACTED`, followed
by `SCARD_E_READER_UNAVAILABLE`. A GC-diagnostic image was flashed to physical
2BED for correlation. Its official run reached that same DES CBC PKCS5
`Cipher_init()` case and passed it, then proceeded through DES ECB and 3DES
measurements without an APDU/USB failure. The wrapper's 900-second timeout
ended this diagnostic run during 3DES CBC NOPAD `Cipher_init()`; it is not a
full-suite pass. GC snapshots were stable at 148 live / 198 allocated arrays
and 36 live / 39 allocated instances, with zero invalid edges or root/edge
failures. The diagnostic serial trace kept returning `TEST:SUMMARY pass=1
fail=0`. This argues against a deterministic failure at the former DES point
or accumulating GC corruption, but the normal-image disconnect cause remains
unproven and could still involve timing or USB/PCSC behavior.

The diagnostic image was replaced with the standard ARM `symm_crypto` image;
the normal cycle passed both architecture builds, flash verification, and its
18-second monitor (18 passing summaries, no `TEST:FAIL`/`FAULT`). The normal
image SHA-256 remains
`18a7a230adf2e1719c26e1f79ccd48f60e93ae108c4b989ade4a2a89d943604e`. PC/SC
currently lists 2BED. Logs: `artifacts/logs/heap-gc-static-debug-2BED-20261030.log`,
`artifacts/logs/heap-gc-static-debug-2BED-continued-20261030.log`, and
`artifacts/results/jcalgtest/performance-static-gc-debug-2BED-20261030/`.
Normal-image repeated failures are in
`artifacts/results/jcalgtest/performance-static-post-gc-fix-8E-20261030/`.
Next: reproduce with the normal image while capturing serial/USB state on the
same board, or reduce the official run to a precise failing method if supported
by the upstream client. Do not call `ALG_PERFORMANCE_STATIC` accepted yet.

Follow-up capture on normal 2BED: the same official client was allowed 3,600
seconds with concurrent serial and kernel/udev USB monitoring. It completed
the DES and AES Cipher sections (126 DES and 108 AES method entries), then
continued into the unsupported RSA/signature matrix before the outer timeout
returned 124. There were 4,214 CSV lines and 937 method entries. The partial
client log contains no `SCARD_E_*`; the serial capture has no `TEST:FAIL`,
`FAULT`, or watchdog recovery markers; the USB log contains only its startup
banner (no add/remove events). A fresh `pcsc_scan -r` still lists 2BED. Thus
the repeated 8E transport failure did not reproduce on 2BED under the normal
image, even with prolonged load and monitoring. This is not a complete static
suite pass. The log also records 72 `FF01` responses for DES MAC Signature
measurement attempts, each reported by the client as `CANT_BE_MEASURED`; treat
these as a separate unsupported-signature/applet-exception investigation, not
as reader loss. Artifacts:
`artifacts/logs/static-normal-full-2BED-serial-20261030.log`,
`artifacts/logs/static-normal-full-2BED-usb-udev-20261030.log`, and
`artifacts/results/jcalgtest/performance-static-normal-full-2BED-20261030/`.

Host follow-up isolated the 72 DES MAC `CANT_BE_MEASURED` rows. An unchanged
upstream JCAlgTest 1.8.2 VPCD run reproduced the exact DES MAC4 ISO9797 M1
static settings (256-byte input, 50 sign calls); initially its measurement
timed out. The persistent applet-owned `Signature` object caused repeated
unnecessary reads and clears of unused message/recovery arrays and an IV that
the MAC backend does not mutate. Native MAC completion now clears the message
buffer only when `update()` data is pending and does not touch the unchanged
IV. The exact upstream prepare APDU returned `AA9000` in 24.686 s and the
measurement returned `AA9000` in 47.425 s. The synthetic symmetric VPCD suite
passes 73/73, including 128-byte repeated signing and buffered-update plus
direct input. Log:
`artifacts/logs/jcalgtest-desmac-256byte-50repeat-optimized2-vpcd-20261030.log`
(SHA-256
`4be4132e3443325f44eb9ae55ba71c6f4af14e6a8ae006d8b169a475c24682cb`).
Physical confirmation and the full official static report remain open; the
host result does not validate the physical reader-disconnect issue.

Follow-up validation after the final MAC input/IV optimization: PC build and
both `./tools/build arm --profile max_crypto` and
`./tools/build riscv --profile max_crypto` pass. The VPCD symmetric harness
passes 75 APDU checks / 73 crypto vectors when run through the approved host
network path; a restricted run failed with `Operation not permitted` before
the elevated retry. Refreshed external ARM UF2:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`, SHA-256
`e013ff4f87331dc99980d9fe9edf10b0a1579838fa5b3feefb166beb93632340`.
Physical Signature static confirmation remains open; no Pico was flashed.

The 2026-10-29 physical Crypto and Security retests requested in the previous
handoff are complete; see the run counts and hashes below.

## Latest physical state and results

These are last observations, not a live enumeration. Reconfirm board identity,
reader name, profile, and installed state before every hardware task.

- **JCAlgTest 1.8.2 physical `ALG_SUPPORT_EXTENDED` on `asymm_crypto`:** 8E
  `8E464F4E2BBC0A19` was flashed with ARM image SHA-256
  `64607ee4d6b95e89cff0d0a9c5f0ee675a9b49ccd310dc31259d5fbbc6739d74` and
  its unchanged upstream CAP was reinstalled. The complete report has 4,525
  matching rows, 196 yes, 4,329 no, and zero other values; it matches the
  2026-10-28 physical reference. Declared-support comparison is 143/145 due to
  the upstream RSA/RSA-CRT 3072 rows sending 4096; preserve these two known
  mismatches. Report SHA-256
  `ce655a87c41cdf160c2e514abf0ff54ea5902b074489da2573a49055fad52997`;
  log SHA-256 `881ce1f6d3f727ba3686c3b085d29c7fa01c0ee6b4d9cd9230b69c076cfb6c6a`.
  On 2BED, the full report passed RSA-512 and RSA-736 after reinstall but hit
  the explicit 900-second command timeout at 4,484/4,525 rows; all 196 yes
  results were present, but this report is partial. Both readers were visible
  after the run. Artifacts are under
  `artifacts/results/jcalgtest/asymm_crypto_complete_20261030_physical_8E_reinstalled/`
  and `artifacts/results/jcalgtest/asymm_crypto_complete_20261030_physical_2BED_reinstalled/`.
- **Profile-switch applet state:** On 8E, the inherited JCAlgTest instance
  returned `FF01` for JCSystem and RSA support after flashing `asymm_crypto`.
  Deleting only its applet/package and reinstalling the unchanged upstream CAP
  restored normal `9000` responses and enabled the complete run. Repeat this
  reinstall after a firmware profile change before interpreting test APDUs.
- **Watchdog marker:** the 8E cycle emitted one
  `WATCHDOG_RECOVERY marker=494E5354 cause=other` with stale trace fields.
  Startup consumes the RP2350 scratch marker once, and `cause=other` means the
  watchdog API did not identify a watchdog-caused reboot. Runtime checks passed
  throughout; no firmware change is indicated by this diagnostic alone.

- 2BED: `2BED26ED20556535` was previously flashed at 2026-10-30 04:17 UTC
  with the ARM `current` image SHA-256
  `8ce742b58ce39bf275e20bd6221f3e1a6289b2603d8f5d49f488a4f05a8c1093`; that
  image was superseded by the `symm_crypto` collector-fix image listed above.
  Flash verification succeeded and the finite monitor passed with repeated
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`; log:
  `artifacts/logs/cycle-choupi-20261030T041755Z.log` (SHA-256
  `b59c48f302d59bd9cecd14b6877b89abbe7036d49a080bbef831127b572b332f`). The
  startup monitor was followed by read-only SELECT, GETVERSION, and JCSystem-
  info APDUs; all returned `9000`. The authorized PC/SC ACL helper had to be
  rerun after the reader was initially absent from the scan. The cycle emitted
  one stale-field `WATCHDOG_RECOVERY` marker with
  `cause=other`; runtime checks passed. The earlier `asymm_crypto` test outcomes
  below remain historical results for that profile, not the current image.
  The cycle emitted no `TEST:FAIL` or nonzero command status. Its complete
  API 3.0.5 Security rerun completed 901 calls with 396 `OK`, 504
  `UNSUPPORTED_PER_SPEC`, one `NO_MEMORY`, zero `SW_ERROR`/wrong results, and
  successful CAP cleanup. D92 `D92E743D5F640C0F` did enter BOOTSEL and the
  corrected image was reported flash-verified, but it did not reappear in the
  subsequent PC/SC scan; current firmware/runtime state is unconfirmed. Do not
  infer D92 recovery or test results from flash verification alone. Latest
  2BED was subsequently flashed with the normal `symm_crypto` ARM image above.
  After the fix, isolated JCAlgTest SHA-256 prepare and doFinal both returned
  `AA9000` in three consecutive probes; see the collector regression section
  above. The latest
  scan showed Virtual PCD 00 00, Virtual PCD 00 01, and physical 2BED; use
  another identified board as failover if it is available.
  A fresh read-only `./tools/test-pico-powerup` run on 2026-10-30 confirmed
  2BED still lists the JCAlgTest package/applet and its SELECT, GETVERSION
  (`1.8.2_jc305`), and JCSystem-info APDUs return `9000`; log
  `artifacts/logs/pico-powerup-persistence-20261030T044725Z.log` (SHA-256
  `bfcc4cc3fdf299ab3654d8be46e1b00926f5b2b795aa9b4e58551a160fa5d2ab`). This
  did not trigger a physical power cycle.
  A subsequent `Card.disconnect(true)` card reset on 2BED returned `9000` for
  SELECT before and after reset, `1.8.2_jc305`/`9000` for GETVERSION, and
  `9000` for JCSystem-info after reconnect; captured log:
  `artifacts/logs/pcsc-card-reset-2BED-20261030T0455Z.log`. This is a CCID card
  reset, not removal of USB power.
  RISC-V currently provides VM-core smoke coverage; it is not full
  CCID firmware acceptance.
- Current Core 3.0.5, exact reader and `--no-scenarios`: baseline D92 completed
  291 rows (272 OK, 11 unsupported, eight unavailable scenarios, zero
  SW_ERROR). Baseline 2BED repeated with 198 OK and 74 `6F00` errors. Serial
  tracing showed the errors were uncaught `IOException`s at `arraylength` on
  missing persistent array records. Successful GP DELETE removed applet data
  but left the selected applet context and heap alive. Added explicit context
  invalidation after successful DELETE. The updated 2BED passed two consecutive
  291-row runs (272 OK, 11 unsupported, eight unavailable scenarios, zero
  SW_ERROR); cleanup succeeded each time. CSVs and hashes are recorded in
  `progress.md` under `physical-core-delete-context-invalidation`.
- `Applet.deselect()` returned expected `-19` on both boards: resolved.
  Earlier physical `-15` and VPCD `1` observations are superseded.
- Optional Ext scenario INSTALL still returns `6A88` and rolls back.
  Results: `artifacts/results/deploy/core-physical-retest-20261029/`;
  exact CSV hashes are in the latest physical-Core `progress.md` record.
- Open and SCP03-authenticated CPLC GET DATA `9F7F` passed on both boards.
  The 42-byte value uses synthetic project markers, not unique factory identity
  or registered manufacturer codes. Unit encoder tests passed 14/14.
  Log: `artifacts/logs/gp-cplc-physical-D92E743D5F640C0F-20261029.log`.
  Cycles: `artifacts/logs/cycle-choupi-20261029T143004Z.log` and
  `artifacts/logs/cycle-choupi-20261029T143255Z.log`.
- 8E `8E464F4E2BBC0A19` previously accepted a verified flash but lost runtime
  enumeration; no runtime pass may be inferred. Use identified available boards
  as failover; an old absence report does not establish present availability.
- Firmware reflashing preserves installed CAP state. Do not call a card empty
  without a fresh GPPro listing. Most result/log artifacts are local and Git-ignored.

### Physical API 3.0.5 asymm_crypto retest — 2026-10-29

Persistent storage error handling is now fixed: `fs_write` status 1 (OutOfFlash)
maps to `SystemException.NO_RESOURCE` instead of `IOException`/`6F00`. A 65,535
byte overlay bound is required by the current `uint16_t` file/snapshot offsets;
77,824 bytes overflowed these offsets and corrupted the D92 snapshot. Corrected
2BED retest results and artifacts are recorded in the following progress entry.
Safe storage reclamation remains open.

- Exact reader: `Pico 2 CHOUPI CCID [JCVM CCID]
  (2BED26ED20556535) 01 00` after the `asymm_crypto` flash and PC/SC ACL
  refresh. Both ARM and RISC-V `./tools/build` gates passed as part of
  `./tools/cycle choupi --profile asymm_crypto --chipid 2BED26ED20556535`.
- Crypto completed 466 calls: 302 `OK`, 163 `UNSUPPORTED_PER_SPEC`, one
  `EXC_CARDRUNTIME reason=0x0001` on the upstream RSA SHA-512 PSS default-salt
  vector. That vector uses a 1024-bit key; the required SHA-512 salt does not
  fit, so retain the size check. Every other previously failing Crypto row
  passed, including EC secp192r1 signing/key agreement, RSA NOPAD, and the
  earlier factory/cipher cases. Hash:
  `28400be72b530b81d264dbad3dbd11f860ba3af6c5756c06dac792ee8e4d05b1`.
- Security completed twice, 901 calls each. Pass 1: 396 `OK`, 503
  `UNSUPPORTED_PER_SPEC`, two `SW_ERROR`. Pass 2: 396 `OK`, 504 unsupported,
  one `SW_ERROR`. Both CAP cleanups succeeded. The earlier RSA-public-4096
  `KeyBuilder` boundary returned `OK`; RSA/RSA-CRT KeyPair 4096 was correctly
  unsupported. Focused `setP`/`setPQ` CRT-2048, `ALG_SHA` one-shot factory,
  and RSA-public-4096 checks returned 3 `OK`. The repeated full-run RSA CRT
  `setP` 2048 `6F00` also reproduced in a narrowed prefix through preceding
  classes, but not when isolated.
- Earlier serial evidence identifies persistent filesystem exhaustion:
  `fs_write` rejected a 131-byte record with 5,929 records and 65,409 bytes
  used. The old `IOException`/`6F00` mapping is fixed as summarized above.
  This is storage pressure, not evidence of faulty RSA arithmetic. Safe
  persistent-object reclamation remains open; loaded persistent objects must
  not be freed by merely clearing their stable heap handles. Serial log:
  `artifacts/logs/2BED-asymm-security-prefix-serial.log`.
- Crypto CSV SHA-256:
  `28400be72b530b81d264dbad3dbd11f860ba3af6c5756c06dac792ee8e4d05b1`.
  Security CSV hashes:
  `925fa829ccd85435af6bfedd568d614579cd1ccc655cc890fa7c0de206c547e2`,
  `8136c7961e4ff3576418199c530b0fb13a6b8164320bac5e86315f7f75448784`.
  Focused CSV SHA-256:
  `24ae0ea320e42c35e1ae10324186c7905785dbc44a010aa6fbf5f1dd5d99380b1`.

## Combined-profile acceptance baseline

- D92 physical `asymm_crypto` report (2026-10-28): 4,525/4,525 rows,
  196 yes, 4,329 no, zero other; identities match JCAlgTest 1.8.2 reference.
  Manifest comparison is 143/145: the upstream RSA/RSA-CRT 3072 rows send
  4096 (`0x1000`) rather than 3072 (`0x0C00`). Preserve those two mismatches;
  do not modify CAPs or enable 4096 to hide them.
  Evidence: `artifacts/results/jcalgtest/asymm_crypto_D92_complete_20261028/`.
- True physical RSA-3072 / CRT-3072 vectors passed in 31.224 / 170.478 s
  (2026-10-28), log `artifacts/logs/asymm-crypto-pcsc-20261028T191018Z.log`.
  Earlier complete physical suite passed 79/79, log
  `artifacts/logs/asymm-crypto-pcsc-20261027T213548Z.log`; VPCD passed 99/99.
- Dated physical deploy baseline is not passing acceptance: Core 291
  (271 OK, 15 unsupported, four unavailable scenarios, one deselect failure
  since resolved); Security 901 (348 OK, 504 unsupported, 34 6F00, 12 wrong,
  one runtime exception, two skipped); Crypto 466 (275 OK, 163 unsupported,
  14 6F00, 14 reason mismatches). Source:
  `artifacts/results/deploy/asymm_crypto_D92_20261028/core_security_crypto_retry.csv`.
- Physical Security follow-up passed 27 supported constructors + 12 expected
  unsupported and nine supported generation cases + five expected unsupported.
  Full sweep still interrupted at public RSA-4096 KeyBuilder.
- Prior symmetric report passed 4,525 rows (73 yes, 4,452 no); normalized
  support comparison 46/46. Historical profile images/hashes are in the archive;
  they are not hashes of the current firmware.

## Recent native fixes and limits

All crypto/API implementation belongs in native C++ dispatch. Keep upstream
CAPs unchanged. Historical Java-provider plans are superseded by this boundary.

- JC305 imported key-interface method maps, null `instanceof`, and NativeKey
  marker checks corrected; supported/unsupported marker cases pass VPCD.
- Persistent key clear/set/domain copies batch record writes with transaction
  snapshots (`4a178a2`, `b101b0a`). Focused EC errors and P-256 copies pass
  VPCD; physical retest pending.
- Signature digest continuation/state layout (`4b546aa`, `d6ebeeb`) and
  malformed precomputed-hash reason (`1aaf17c`) fixed. Upstream initial-digest
  group: 10 OK, eight expected unsupported, 448 filtered of 466. RSA and
  ECDSA SHA-1/256 continuation/precomputed cases pass VPCD. SHA-224/384/512
  continuation/precomputed coverage remains open: attempted upstream filters
  selected no such cases. Artifact directory:
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/`.
- MessageRecovery mode/parameter reasons fixed (`be0ea69`, `2ae55a4`);
  NOPAD now permits both RSA directions, four selected VPCD calls pass, and
  all four physical API 3.0.5 NOPAD rows pass on 2BED. PKCS#1/OAEP mode
  restrictions remain.
- ECDSA SHA-384 selected calls pass 4/4; SHA-224/512 traced combined retry
  passes 8/8 after intermittent 6F00. DH/DHC secp192r1 passes fresh-image
  retries. No root cause established for the intermittent failures.
  `Cipher()` factory passes twice on VPCD; physical discrepancy remains open.
  Crypto evidence: `artifacts/results/deploy/crypto-current-vpcd-20261029/`,
  `crypto-dh-vpcd-20261029/`, `cipher-factory-vpcd-20261029/` under the same deploy root.
- SHA-512 PSS default salt with the upstream 1024-bit test key correctly fails
  size validation (130 encoded bytes required, 128 available); explicit-zero-salt
  native vectors pass. Do not disable the algorithm or weaken the check.
- RSA scratch zeroization added across cipher/signature/PSS/OAEP/CRT copies
  (`f7595d9`, `4ecd522`, `df2d2d9`, `bedf23a`, `da8ee60`); final-size reserves
  avoid abandoned sensitive vector allocations. Builds and VPCD 99/99 pass;
  physical validation pending. Private input blinding (including imported keys
  without E and non-CRT operations) and complete CRT fault verification remain
  open. Full verification requires a matching public-exponent association.
- Current-profile digest guards and ARM SHA-256 source selection fixed;
  ARM and RISC-V wrappers pass. Earlier optional-family build failures resolved.

## Emulator workflow and evidence limits

- Rebuild PC after source changes: target builds do not refresh the emulator.
  Start each isolated Crypto sweep with a zero-length flash image and fresh
  matching registry sidecar. Confirm card presence; prevent overlapping runners.
  Prefer paired reader `Virtual PCD 00 00` / `127.0.0.1:35963` for focused
  continuation cases. Another endpoint has intermittently stalled.
- Existing JCAlgTest VPCD two-process persistence test passed 30/30, 76.8 s
  total (58.449 + 18.333 s), first connect 23.570 s. This supersedes the older
  field-access blocker but is not physical power-cycle evidence.
  `artifacts/logs/host-vpcd-jcalgtest-20261029-two-pass.log`.
- Official RSA-512 VPCD support APDU took 133.756 s; stopped report is partial.
  Independent 99/99 native probe measured RSA/CRT 1.068/1.149 s (512),
  1.334/3.469 s (1024), 47.488/23.334 s (3072). Cause unknown.
  `artifacts/logs/asymm-crypto-keypair-vpcd-timing-20261029.log` and
  `artifacts/results/jcalgtest-vpcd-asymm-current-pcsc-20261029/`.
- Unfiltered Crypto sweep stopped at 190/466 (112 OK, 78 unsupported, zero
  failures); cleanup passed. This is partial evidence, not a full-suite pass.
- Plain PC VPCD bypasses Pico CCID GET DATA dispatch. Use encoder unit tests
  and actual CCID hardware for CPLC; VPCD LOAD/INSTALL coverage remains valid.

## Worktree preservation and documentation maintenance

At compaction start, unrelated changes existed in `AGENTS.md`, `src/cap_load.cpp`,
`tools/test-jcsystem-transaction-physical`, nested SDK worktrees, and untracked
ALGTEST logs, measurement lists, and the supplied JCOP4 matrix. Preserve them;
inspect fresh `git status` before staging. No firmware was changed by compaction.
Keep this file a current snapshot; add reusable diagnoses to QKB and detailed
chronology to `progress.md`. Consult archived sections for older measurements,
commit IDs, feature-specific tests, and interrupted tasks before resuming them.

## Latest continuation: overlay v3 / Crypto capacity retest — 2026-10-30

- `src/choupi_pico2.cpp` contains the v3 implementation committed as
  `181415d`; `tools/tests/rp2350_fs_overlay/rp2350_fs_overlay_test.cpp` has an
  uncommitted follow-up regression. Version 2 remains readable; new
  snapshots use v3, with the high offset bit stored in `OverlayFile.flags` bit
  6. The arena is 77,824 bytes and fits the 160 KiB slot exactly at the
  configured maximum index size.
- Validation passed: `./tools/test-rp2350-fs-overlay` (`TEST:SUMMARY fail=0`),
  `./tools/build arm`, `./tools/build riscv`, and the profile-specific ARM cycle
  on 2BED. ARM UF2 SHA-256:
  `a4790d06c5691ac5f222bc51fb47e207268e7b805bd749958940de82d130f977`.
- Physical 2BED `2BED26ED20556535`, `symm_crypto`: the unchanged upstream
  Crypto CAP passed DES-64 and AES-192 known-answer rows; complete Crypto
  passed 466 calls with 161 `OK`, 305 `UNSUPPORTED_PER_SPEC`, zero other
  statuses. Upload and cleanup passed. Full CSV:
  `artifacts/results/deploy/crypto-overlay-v3-full-2BED-20261030.csv` (SHA-256
  `3cb62be1b5cd8539fcddbd450a900f1f83236fa375eb0bf20cec79c9ca2efa0e`).
- Captured full-run serial log has no `FS_WRITE_FAIL`, `PERSIST_EXCEPTION`,
  `TEST:FAIL`, or `FAULT`. The post-run registry probe timed out, and a fresh
  scan listed only VPCD readers. Do not claim post-run physical registry or
  cold-power verification. Cycle log and additional evidence hashes are in
  QKB’s “RP2350 overlay capacity v3 resolves physical API Crypto exhaustion”.
- Next: run targeted whitespace checks, append the progress record, and commit
  only the overlay test plus `.agent/QKB.md`, `.agent/HANDOFF.md`, and
  `progress.md`. Preserve other dirty worktree changes. After commit, continue
  the remaining TODO items, particularly safe persistent reclamation and
  physical power-loss validation.
- Follow-up to `181415d`: a high-offset v3 interrupted-snapshot
  regression now samples seven cuts across erase, payload, and header writes.
  The previous >64 KiB record remains intact and recovery accepts only the old
  snapshot or the complete new record. The full overlay test passes. Both 8E
  and 2BED still enumerate over USB, but PC/SC lists only VPCD readers, so no
  physical failover or power-loss test is currently available.
- A further host-path check confirmed both 8E and 2BED are still enumerated
  with CCID USB interfaces and retain their expected USB serials. The approved
  access helper grants both nodes, but PC/SC still lists only the two VPCD
  readers. Finite eight-second serial monitors on both boards passed 8/8
  runtime summaries with no `TEST:FAIL`/`FAULT`. `pcscd.socket` is active and
  `pcscd.service` auto-exited normally; this does not explain reader loss. Logs
  and hashes are in QKB’s “Pico USB enumeration without PC/SC reader
  registration”. Physical applet APDUs remain blocked until PC/SC registers a
  Pico reader; do not infer a firmware crash from the missing PC/SC entry.
- New diagnostic: the installed libccid Info.plist contains the exact
  `0x2E8A:0x0009` Pico entry and matching friendly name. USB exposes CDC ACM
  plus CCID interface 2; both finite serial monitors pass. The approved helper
  grants `pcscd` access and restarts it, but the physical readers remain absent
  from `pcsc_scan -r`. System journal access is unavailable to this user, so
  the IFD initialization error remains unknown; do not modify driver or
  firmware without that evidence.

## Future backend analysis — 2026-10-31

Added `docs/FUTURE_JCVM_LANGUAGE_BACKENDS.md` at the user's request. This is
explicitly future work only: do not implement it without a new user request.
Recommends separate VM/platform contracts and selection, combined presets, and
an audited all-Rust track covering transport/card manager/crypto/startup too.
Includes differential tests, fuzzing and persistence switching gates. Source
inspection and documentation whitespace checks only; no firmware change,
build, flash or USB/PCSC access. All earlier worktree changes preserved.

## Scheduled architecture maintenance — 2026-10-31

User requested scheduling only: TODO.md now contains ARCH-01 through ARCH-06
for shared runtime declarations, filesystem contracts, diagnostic declarations,
progress callback rules, Pico startup/storage separation and loader encapsulation.
No implementation performed. Start with ARCH-01; keep the wider backend design
explicitly deferred. Preserve the other sessions' existing worktree changes.

## Architecture completion review — 2026-11-01

Reviewed committed ARCH-01–06 implementations and later validation records.
Most original intent is covered. TODO.md now schedules ARCH-07 (remaining
shared lifecycle/filesystem declarations), ARCH-08 (strictly bounded USB
progress work), and ARCH-09 (production CCID progress/dispatch regression).
No implementation changed. Existing progress-guard wrapper passed 6/6; it
exercises simulated service paths, which motivates ARCH-09. No hardware work
or builds performed for this documentation review. Preserve ongoing uncommitted
applet-context work; broader language backend work remains deferred.

## Native Shareable lookup and client proxy — implemented, invocation pending

`Bytecodes::bc_invokestatic` now intercepts only the resolved ROM
`NativeImplementation.getAppletShareableInterfaceObject` entry. It reads the
requested AID from the JCRE global-AID registry, resolves/restores the matching
server applet context, calls that applet's virtual
`Applet.getShareableInterfaceObject` override, and returns a new placeholder
instance in the client heap. The server reference stays in per-context proxy
metadata; the server context retains it as a GC root. The callback uses the
embedded Applet token then reverse-maps to the imported CAP override token.
When client and server are the same context, the nested callback returns on the
existing caller operand stack rather than the root-result slot; the helper
consumes that result before returning to the intercepted call.

The unchanged transaction-probe CAP passes case 48 (null AID, unknown AID,
and the default null-returning own-applet callback): `TEST:SUMMARY pass=3
fail=0`. The native registry/root test passes, the full VPCD multipackage suite
passes 47/47, and PC/ARM/RISC-V builds pass. The first case-48 run failed with
`6F00`; the cause was passing an imported token into a ROM-origin callback
resolution and not consuming the same-context nested result correctly. Both
cases are fixed and the unchanged probe passes on the final source.

Still missing: cross-AID positive lookup, proxy `invokeinterface` dispatch,
interface visibility validation, argument/result ownership conversion, and
field/virtual-method firewall checks. The existing upstream Multi CAP exercises
multiple applets but does not provide a positive Shareable service fixture.
Keep all upstream CAP files unchanged. The authorized reader helper reports
`found 0`; no physical flash or check was possible.

Next: implement and test proxy-aware `invokeinterface` dispatch. Reject direct
field access, `invokevirtual`, and other object operations on proxy placeholders
unless the JC firewall explicitly permits them. Add cross-context exception,
return and ownership checks before claiming CTX-01 complete.

## TODO backlog compaction — 2026-11-01

`TODO.md` now uses 18 stable task IDs and separates remaining acceptance from
implemented groundwork. Every one of the original 31 unchecked items maps to an
open/conditional owner in `.agent/archive/2026-11-01-todo-pre-compaction/MIGRATION.md`.
The full original and all 150 completed records are preserved byte-for-byte with
a SHA-256 manifest and a 181-checkbox index. Use the compacted TODO for current
priorities; archived checklists may describe superseded source or board state.
Only documentation was changed; no build, flash or USB/PCSC validation was run.
Existing source changes and ongoing SIO work were preserved. Continue the task
already in progress using its new TODO owner (CTX-01 for SIO); compaction does
not authorize persistent-GC enablement or close any physical acceptance gate.

## Positive Shareable proxy call on unchanged upstream CAP — 2026-11-01

The positive fixture was found in the unchanged
`AlgTest_JavaCard/deploy/caps/AlgTestApiCore_305.cap`; the earlier note that no
positive fixture existed is superseded. Its applet returns itself for SIO
parameter 1 and implements `ApiTestShareable.ping()`, which returns `0x1234`.
On fresh VPCD storage, the focused API 3.0.5 Core run completed all three
selected Shareable rows with status OK, including
`tagging interface Shareable ... usable through a sub-interface` with
`returned=1 effect=as expected`. Artifact:
`artifacts/results/shareable-vpcd-verified-20261101T183526Z/`.

The first positive attempt produced `EXC_RUNTIME`. VM trace identified enum
exception 1 (`StackUnderflowException`): after popping the proxy handle, the
nested `Interpretor` was constructed with initial references, which selects
the “arguments remain on caller stack” entry mode. Use an explicit
`InterpretorInitialLocal` receiver instead. The final code also checks the
concrete server class against the requested Shareable interface before method
resolution. RMI registry test and PC/ARM/RISC-V builds pass.

This only verifies the same-applet, same-context route. Cross-AID context
restoration, arguments, nonnull reference returns, exception propagation,
proxy lifetime across reset/GC, and physical hardware remain open. The latest
authorized USB reader helper found zero Pico devices; no physical operation
was attempted. Keep upstream CAPs unchanged.

## Same-context Shareable calls preserve caller operands — 2026-11-01

The same-context branch in `invokeinterface` now replaces the proxy receiver
in place with the retained server object and uses normal `Method_Handler`
dispatch. This keeps all caller operand words in their original typed layout,
so same-context methods may take primitive or reference arguments without
crossing an applet heap. The cross-context branch remains limited to receiver-
only methods until descriptor-aware copying and ownership conversion exist.

Validation: `tools/test-rmi-registry` passes its new `stack_receiver_replace=1`
assertion. A fresh VPCD run of the unchanged `AlgTestApiCore_305.cap` passes all
three focused Shareable rows and cleanup; the interface call reports
`returned=1 effect=as expected`. PC, ARM and RISC-V builds pass. Artifacts:
`artifacts/results/shareable-same-context-stack-20261101T185456Z/`.

This still does not test cross-AID server context routing, argument conversion
across contexts, exception propagation, reference ownership/lifetime or
physical hardware. The preceding authorized reader refresh found zero Pico
devices. Keep CTX-01 open and preserve upstream CAPs unchanged.

## Cross-context primitive SIO arguments — in progress, 2026-11-01

`invokeinterface` now reads a Shareable interface method signature from the
CAP Descriptor component for calls whose proxy belongs to another context. It
copies boolean/byte/short/int arguments into typed initial locals, then removes
the caller's argument words before entering the server method. Null reference
arguments are allowed; all non-null object and array references fail closed
until ownership translation is implemented. Signature lookup walks the
variable-length class descriptor table; the legacy `jc_cap_descriptor_component::types()`
accessor assumes fixed-size class records, so this path deliberately does not
use it.

Validation: `tools/test-rmi-registry`, PC build, ARM firmware build, RISC-V
build, and scoped whitespace check passed. The full VPCD multipackage suite
passed its package install, restart, AID lookup, and deletion phases, then
timed out installing the transaction probe during cleanup/reinstall
(`INSTALL a000000062030101`); root cause is unknown and the failure is
preserved in `artifacts/logs/host-vpcd-multipackage-*20261101T191433Z.log`.
An attempted transaction probe case 48 did not reach VPCD (`timed out waiting
for emulator VPCD connect`), so it is not counted as SIO validation. The
approved USB helper and elevated `lsusb -d 2e8a:0009` found no physical Pico in
the VM; PC/SC only lists the two VPCD readers.

No CAP was created or modified. `tools/test-rmi-registry` now covers the local
signature-to-local conversion for byte/short layout, int word order, null and
non-null reference behavior, and argument-count mismatch. This does not cover
Descriptor lookup or a cross-context VM call. The existing upstream positive
SIO CAP remains same-applet/same-context and does not exercise the new path.
Next, design a cross-AID integration test using an unchanged upstream CAP if
available, then implement safe non-null reference translation and
cross-context exception restoration. Keep CTX-01 open.
