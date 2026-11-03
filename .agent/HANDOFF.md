## Current session — Windows MUTE Linux cross-check — 2026-11-03 11:33 UTC

User tested the current max firmware in Windows `certutil -scinfo`: the reader
enumerates, but card status is `PRESENT | MUTE` and no card is recognized; the
command takes several minutes. This means Windows did not complete card
activation/ATR handling. It does not by itself establish an invalid ATR or an
applet failure.

Refreshed the authorized PC/SC path on Linux. Both physical reader IDs
enumerated: `8E464F4E2BBC0A19` (suffix `00 00`) and `2BED26ED20556535` (suffix
`01 00`). Bounded, read-only GlobalPlatformPro `--list` / PC/SC power-on
succeeded on each and returned ISD and installed applet/package listings. This
narrows the reported failure to a Windows-specific or host/USB-path interaction,
but does not isolate it because no Windows transfer trace or correlated
firmware serial log was captured, and the tested image hash is unknown.

Next: on Windows, capture a USBPcap trace covering enumeration through the
first `PC_to_RDR_IccPowerOn` response while capturing the Pico CDC serial log
for the same test; record the flashed UF2 SHA-256. Check whether CCID command
`0x62` reaches firmware and whether response `0x80` carries ATR `3B800181`.
Keep current CCID behavior unchanged until the trace distinguishes missing
command, missing/short USB response, and Windows ATR rejection. Linux checks
were read-only; no card contents were changed.

## Current session — cover foreign references at native Util entry — 2026-11-03 11:28 UTC

Added a test-only cross-AID Shareable method that passes a caller-owned byte
array directly to native `Util.arrayCopyNonAtomic`. The applet catches the
expected `SecurityException`; expected APDU result is `6A269000`. This verifies
the generated native dispatcher checks the owner tag before JNI code resolves
the handle with `Heap::getArray()`.

Validation passed: VPCD cross-AID suite 20/20 with Descriptor absent and 20/20
included; physical Pico 2 `8E464F4E2BBC0A19` passed the native Util call, reset
stale-proxy path, GP DELETE stale-proxy path, and fixture cleanup. The first
VPCD attempt intentionally reused stale generated fixture CAPs and returned
the default `45679000`; rebuilding the fixtures resolved that test setup
mismatch. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261103T112302Z.log`,
`artifacts/logs/sio-cross-aid-vpcd-20261103T112447Z.log`, and
`artifacts/logs/sio-cross-aid-physical-20261103T112637Z.log`.

Next: continue CTX-01's remaining native/reference-consumer, stale-owner,
GC-root, multi-package and channel-policy audit. This single JNI path does not
establish blanket firewall acceptance. Preserve unrelated changes.

## Current session — support explicit JCAlgTest version in power-up probe — 2026-11-03 11:19 UTC

The read-only `tools/test-pico-powerup` probe hardcoded applet version
`1.8.2_jc305`, so it falsely failed when both boards had the unchanged upstream
`1.8.4_jc305` applet installed. Added `JCALGTEST_EXPECTED_VERSION`, preserving
the 1.8.2 default. Sequential exact-reader runs passed on both physical boards:
`8E464F4E2BBC0A19` (ATR `3B800181`, SELECT/GETVERSION/JCSystem info `9000`) and
`2BED26ED20556535` (same results). Logs:
`artifacts/logs/pico-powerup-persistence-20261103T111821Z.log` and
`artifacts/logs/pico-powerup-persistence-20261103T111840Z.log`.

Two initial simultaneous attempts failed only because of the old version
assertion; one also hit a transient `SCARD_F_COMM_ERROR`. Re-running one board
at a time with the expected version set passed. The concurrent communication
error is not diagnosed. These are warm read-only checks, not proof of USB
power-loss persistence. No firmware or card contents were changed.

Next: continue CTX-01's remaining stale-owner/global-reference lifecycle and
reference-consumer audit. The project remains incomplete; preserve all other
worktree changes.

## Current session — fail closed on deleted SIO proxy — 2026-11-03 11:05 UTC

Fixed the stale-proxy GP DELETE case. Root cause: `shareableProxyCastTarget`
returned a boolean and conflated a true cast mismatch with a deleted/unavailable
server. `checkcast` turned both into `ClassCastException`. It now returns
`optional<bool>`: missing/invalid routing raises `SecurityException`, while an
incompatible cast still raises `ClassCastException`. The same fail-closed
distinction applies to `instanceof`.

Validation: `./tools/test-rmi-registry`; VPCD cross-AID suite 20/20 without
Descriptor and 20/20 with Descriptor; `./tools/build arm`; `./tools/build
riscv`. ARM image end `0x100d06e0`, 194,848 bytes remain. Physical Pico 2
`8E464F4E2BBC0A19` passed the full cross-AID probe, GP DELETE of service
`A00000015100000301`, retained proxy response `6A0A9000`, fresh lookup response
`6A049000`, and cleanup. Log:
`artifacts/logs/sio-cross-aid-physical-20261103T110344Z.log`. The physical
probe now expects `6A059000` for an opaque non-null reference whose use is a
null comparison, matching the VPCD test and existing owner-aware semantics.

Physical USB observations: 8E was flash-verified, but the immediate `cycle`
monitor timed out waiting for CDC; the board later returned as a CCID reader
and passed real APDUs. Pico `2BED26ED20556535` had existing JCAlgTest and SIO
packages, so its physical regression preflight correctly refused to overwrite
it. It was separately reflashed by stable chip ID and passed finite serial
`TEST:PASS choupi_runtime`; existing packages were preserved. This supports
continuing with another visible physical board when one is unavailable and
records the CDC re-enumeration gap.

Windows report remains unresolved: Windows sees PRESENT/MUTE in `certutil
-scinfo`, while `src/ccid_device.cpp` sends valid ATR `3B 80 01 81` on CCID
power-on. Need correlated Windows PC/SC trace and serial capture; do not infer
that the ATR bytes themselves are invalid. See `.agent/QKB.md`.

Next: continue remaining CTX-01 lifecycle, GC/root, stale owner, multi-package,
and channel policy acceptance. Preserve unrelated dirty worktree changes listed
in the prior handoff.

## Current session — owner-aware null and identity comparisons — 2026-11-03 04:35 UTC

Added `Stack::referencesEqual()` and switched narrow/wide `if_acmp*` plus
`ifnull`/`ifnonnull` to typed `ReferenceValue` pops. Comparisons do not resolve
handles in a heap; equality requires both compact handle and normalized owner.
The current owner marker resolves to the executing package, while null/global
references share a global identity domain. Updated the generated SIO fixture:
opaque foreign references compare safely, while global Object[] virtual
`Object.equals`, foreign `checkcast`, and foreign `instanceof` remain denied
with catchable security exceptions. Multi-reference comparisons also pass after
fixture CAP regeneration.

Validation: `./tools/test-rmi-registry`; `./tools/build arm` and
`./tools/build riscv` passed. Cross-AID VPCD suite passed 18/18 with Descriptor
omitted and 18/18 with Descriptor included. Comparison implementation commit
`1aaf7cd`; fixture regression follow-up `dc8769e`. ARM firmware ends at `0x100d06a8`,
leaving 194,904 bytes in the 1 MiB window. The authorized reader helper found
zero physical Pico 2 readers, so no flash/physical check occurred. The supplied
upstream AlgTest 1.8.4 client remains
`AlgTest_JavaCard/deploy/client/AlgTestJClient_v1.8.4.jar`; its completed full
BASIC run is recorded in the preceding handoff section.

An initial two-reference SIO probe returned `6F00` and logged a suspicious
second local `0x005A`. That did not reproduce after rebuilding all fixture CAPs
and adding a distinct two-array interface method: identical foreign references
and null pairs now pass in both Descriptor-free and Descriptor-included suites.
The earlier fixture anomaly remains unexplained, but multi-reference marshalling
is exercised and passing. Continue CTX-01 collision, storage/GC, stale-owner,
cast and DELETE audit. The explicit cast/instanceof regression is recorded
in the latest progress entry. Preserve all pre-existing user worktree changes.

## Current session — root return owner/GC regression — 2026-11-03 04:15 UTC

Added direct registry-suite coverage for the SIO root return metadata: the
reference owner survives root-result retrieval, a package-2 handle is not
enumerated as a root by a package-1 Context, and a current-context root still
is enumerated. `./tools/test-rmi-registry` passes. This was a test-only step, so
no ARM/RISC-V rebuild or hardware operation was run. The production change and
Descriptor-free/Descriptor-included SIO integration results are in the
preceding handoff section.

Next: continue CTX-01 foreign-reference lifecycle and comparison/cast audit;
physical validation remains pending until a Pico is enumerated.

## Current session — preserve private reference returns across SIO — 2026-11-03 04:12 UTC

Fixed cross-context `areturn` provenance. It now pops a `ReferenceValue` rather
than rejecting foreign owners, carries its owner through same-context frames,
and stores owner metadata with the root interpreter result. Root GC visits a
root return only if its owner is local/current/global, avoiding aliasing a
foreign compact handle in the server heap. On return from an SIO call, a
reference belonging to the original caller package is normalized back to the
caller-local marker; server-owned Shareable objects are proxied, and global or
third-party references retain their owner tag.

Extended the generated cross-AID SIO fixture with `byte[] echoArray(byte[])`.
The client verifies that the returned array is identical and readable in its
owning context. `./tools/test-rmi-registry` and ARM/RISC-V builds pass. The
VPCD SIO fixture passes 18/18 with Descriptor omitted and 18/18 with Descriptor
included. The first Descriptor-free run returned `6F00`; debug trace localized
the mismatch to applet-instance ID versus package ID during owner normalization.
After fixing the comparison, both runs pass. Current ARM image ends at
`0x100d05d8`, leaving 195,112 bytes; UF2 SHA-256
`98f4c036d6fe56b80cbcf0582ff6a8ec60bea6b3de83a2770220b59234190873`.

Physical validation is still outstanding. The authorized reader refresh and
USB inventory found no Pico after the previous BOOTSEL reset attempt. Do not
claim the latest changes are physically tested. The earlier full JCAlgTest
1.8.4 BASIC run used `AlgTest_JavaCard/deploy/client/AlgTestJClient_v1.8.4.jar`
and remains recorded in the prior handoff section.

Next: continue CTX-01 offline with GC/return pressure and third-party opaque
reference return tests, then continue the collision, storage, comparison/cast,
stale-owner and DELETE audit. Flash and capture a finite serial log when a Pico
is again enumerated. Preserve all pre-existing uncommitted changes.

## Current session — type-check reference receivers before dispatch — 2026-11-03 03:59 UTC

Extended CTX-01 after auditing invoke receiver reads. The generic stack accessor
returns raw words and only performs the owner check when the runtime word is
already tagged Reference. Added `Stack::get_Pushed_Reference()`, requiring both
a Reference tag and current/global ownership, and used it for virtual/special/
interface receiver dispatch plus the intercepted Shareable lookup client AID.
The raw accessor remains in argument decoding where the signature or runtime
kind tags establish the expected type. Regression tests verify primitive and
foreign-owner receiver words both raise catchable `SecurityException`.

Validation passed: `./tools/test-rmi-registry`; `./tools/build arm`;
`./tools/build riscv`; escalated VPCD multipackage 47/47; and escalated VPCD
RMI client 18/18. Firmware size ends at `0x100d050c`, leaving 195,316 bytes; UF2 SHA-256 is `3afb79ed6922bb7611bce5c4555ea9572ef64001de0f675aa6ebf130eebb644e`.
Physical validation did not complete. The flash wrapper could not access the
composite reset interface and requested CDC BOOTSEL; afterward the authorized
reader helper found zero Pico readers and `lsusb -d 2e8a:` returned no device.
The VM cannot currently identify a physical target, so this change is not
physically verified. The exact v1.8.4 JCAlgTest client remains
`AlgTest_JavaCard/deploy/client/AlgTestJClient_v1.8.4.jar`; its full BASIC run
on 2BED was already completed and recorded earlier in this handoff.

Next: continue CTX-01 audit offline. When a Pico is again visible, identify it,
flash the current ARM image, and inspect finite serial output before claiming
physical acceptance. Preserve unrelated worktree changes.

## Current session — preserve catchable foreign-reference rejection — 2026-11-03 03:43 UTC

`Stack::get_Pushed_Element()` checks owner metadata and throws
`SecurityException` for a foreign reference. Its old exception specification
was conditionally `noexcept` when optional stack bounds checks were disabled,
which would turn the intended applet exception into termination. Removed that
conditional from declaration and definition, and added a compile-time
regression translation unit that disables both checks and asserts the accessor
remains potentially throwing.

Validation passed: `./tools/test-rmi-registry`; `./tools/build arm`;
`./tools/build riscv`; escalated `PICO2_HOST_USB_ACCESS=1
./tools/test-vpcd-multipackage` (47/47); and escalated
`PICO2_HOST_USB_ACCESS=1 ./tools/test-rmi-client-vpcd` (18/18). On physical
Pico 2 `2BED26ED20556535`, the ARM image
`build/arm/pico2_choupi.uf2` was flashed from BOOTSEL and fully verified by
picotool. The 18-second finite monitor reported repeated
`TEST:SUMMARY pass=1 fail=0`; no failure/fault markers appeared. Firmware SHA-256
`1ca1093340e936276b52869c9637adf22c8627fe21598c8b6cf1d0379b85d963`. The first
cycle attempt failed to enter BOOTSEL promptly (`Unable to locate reset
interface`); delayed BOOTSEL enumeration appeared afterward and direct wrapper
flashing succeeded.

Next: continue CTX-01 by auditing all reference-consuming bytecodes and native
entry points for owner checks, aliasing/collision, return, storage, GC, stale
owner, and DELETE behavior. Keep unverified routes fail-closed. Preserve the
pre-existing dirty files listed in the worktree; commit only this stack fix and
its focused regression with its progress/handoff/TODO/QKB records.

## Current session — full 1.8.4 BASIC support completes on physical Pico — 2026-11-03 03:12 UTC

Used the supplied upstream `AlgTestJClient_v1.8.4.jar` with the unchanged
`AlgTest_v1.8.4_jc305.cap` on Pico 2 `2BED26ED20556535`. The official client
completed `ALG_SUPPORT_BASIC` in 24 minutes 20 seconds and exited 0. It
recorded 4,446 yes/no rows: 148 `yes`, 4,298 `no`, and no unexpected result
status. SELECT, GETVERSION `1.8.4_jc305`, JCSystem info, and the complete BASIC
matrix passed without PC/SC errors. This run passed the prior drop point of
2,680 rows, so the earlier `SCARD_E_NOT_TRANSACTED` did not reproduce; its
cause remains unknown.

Use the matching client path exactly as above. Client banner says 1.8.4 but the
generated CSV still says client version 1.8.3. The repository's standalone
`probe-jc-algtest` asserts applet version 1.8.2 and therefore exits nonzero on
the correctly responding 1.8.4 applet; do not interpret that assertion as a
card failure. The full official client run passed. No firmware was flashed.
Artifacts, hashes, and exact command are in the newest 2026-11-03 progress
entry. The earlier incomplete BASIC attempt remains in its separate directory.

Next: proceed with remaining JCAlgTest 1.8.4 operations only as separately
bounded runs if needed; keep source and upstream CAP/client untouched. Continue
the project backlog at CTX-01/03 when testing work is complete. Preserve the
pre-existing dirty changes listed in the earlier handoff sections.

## Current session — isolate final 1.8.4 mode APDU; no reproduction — 2026-11-03 02:38 UTC

The last request in the interrupted 1.8.4 BASIC log was
`B0752A000307081000` (SIG_CIPHER_HMAC / OAEP / SHA-512). Sent it twice in
isolation to Pico 2 `2BED26ED20556535`, each time after selecting the 1.8.4
applet. Both returned SW `9000` with JCAlgTest result status `03` (unsupported,
expected). The exact request does not reproduce the long-run disconnect.
The second run is archived at
`artifacts/logs/jcalgtest-184-single-hmac-mode-2bed-20261103.log`.

This narrows the failure toward cumulative/time-dependent behavior or PC/SC
transport handling; cause remains unknown. Do not claim the full BASIC matrix
passed. Keep the current 1.8.4 installation; the long sweep’s reader
rediscovery, post-failure version probe, and runtime monitor evidence are in
the preceding handoff section. The alternate 8E Pico still is not a PC/SC
reader, and no firmware or CAP changes were made.

## Current session — 1.8.4 physical sweep disconnect and reader recovery — 2026-11-03 02:32 UTC

The extended matching-client `ALG_SUPPORT_BASIC` attempt on Pico 2
`2BED26ED20556535` ended with `SCARD_E_NOT_TRANSACTED` after 2,680 result
rows (148 `yes`, 2,532 `no`); exit status 1. The run log and partial CSV are
under `artifacts/results/jcalgtest-184-basic-2bed-20261103-complete-attempt/`.
This is not a complete report. The log ends mid-APDU. No firmware was flashed.

After the failure, the authorized helper first reported one USB device and
`pcsc_scan -r` showed only VPCD. `lsusb` later showed two Pico devices, and
serial by-id identified both `8E464F4E2BBC0A19` and 2BED. A subsequent helper
refresh restored 2BED as a PC/SC reader; 8E still did not appear as a reader,
so there was no physical failover target. A post-failure 1.8.4 SELECT/version/
JCSystem probe passed, and a 10-second serial monitor showed repeated
`TEST:SUMMARY pass=1 fail=0`. Thus the applet/runtime remained responsive after
PC/SC rediscovery; the transport-drop cause is unknown.

The current card still has the freshly installed unchanged 1.8.4 CAP. Keep
using `AlgTest_JavaCard/deploy/client/AlgTestJClient_v1.8.4.jar`. Do not start
another long physical matrix until the reader-drop behavior is isolated or
another PC/SC board is available. The previous 900-second attempts and clean
install resolution are documented immediately below. Also investigate why the
old installed instance had a null version field; generic VPCD restore passes
do not close this CAP-specific issue.

## Current session — fresh install restores JCAlgTest 1.8.4 APDUs — 2026-11-03 02:06 UTC

Read the tidied repository instructions and ran the isolated VPCD persistence
regression: install/commit passed 3/3, restored state plus four GC-pressure
rounds passed 38/38. This confirms the generic persistent field/reference
restore fixture, but does not explain the JCAlgTest-specific physical failure.

On Pico 2 `2BED26ED20556535`, PC/SC and USB access were refreshed. GPPro listed
JCAlgTest plus the unrelated API applets. Deleted only the JCAlgTest applet AID
`4A43416C675465737431` and its package AID `4A43416C6754657374`, then installed
the unchanged upstream `AlgTest_v1.8.4_jc305.cap`. With the matching supplied
`AlgTestJClient_v1.8.4.jar`, SELECT returned `9000`, GETVERSION returned
`1.8.4_jc305`/`9000`, and JCSystem info returned `9000`. The former physical
GETVERSION `FF05` is therefore cleared by deleting/reloading the whole
JCAlgTest package. The precise cause remains unknown; do not treat this as a
general fix for instance-field restore.

The matching 1.8.4 `ALG_SUPPORT_BASIC` operation was attempted twice with a
900-second cap. Both runs made valid progress without PC/SC transport errors;
the second left a 2,617-result partial CSV (148 `yes`, 2,427 `no`) and timed
out mid-matrix with exit 124. The current CSV and complete APDU log are in
`artifacts/results/jcalgtest-184-basic-2bed-20261103/`. The new client banner
reports 1.8.4, while its generated CSV metadata says client 1.8.3. After the
timeout, a fresh exact-reader probe again passed version and JCSystem info.
No firmware was flashed; the only card mutation was replacement of the
JCAlgTest package. 8E remains USB-visible but absent from PC/SC; 2BED is the
only physical reader.

Next: investigate why the old installed instance returned a null version
field, then verify the freshly installed 1.8.4 instance across reset. For a
complete support report, rerun the 1.8.4 client with a longer explicit timeout
and a separate output directory; omitting `-fresh` restarts the run instead of
resuming rows. Keep using the 1.8.4 client for this CAP. Preserve the currently
installed 1.8.4 package and do not change upstream CAPs.

## Current session — trace JCAlgTest FF05 to null version instance field — 2026-11-03 01:20 UTC

Fixed CCID diagnostics so slot-status polling no longer overwrites the last
XfrBlock APDU response. Rebuilt and flashed the debug image to Pico 2
`2BED26ED20556535`; ARM/RISC-V builds and flash verification passed, and the
finite monitor reported `TEST:SUMMARY pass=1 fail=0`. The exact-reader physical
probe still returns SELECT `9000`, GETVERSION `FF05`.

The retained serial trace now correlates GETVERSION with status `FF05` and
shows `NullPointerException` (`id=28`) at `arraylength` opcode `0x92` in
package 56, Method PC 15865. Inspection of the unchanged CAP class shows
`GetVersion` reads instance field `ALGTEST_JAVACARD_VERSION_CURRENT` and then
executes `arraylength`; the field is null at runtime although the constructor
assigns its JC305 version array. This narrows the failure to instance-field
initialization/persistence/restore; why the value becomes null remains open.
Do not change the upstream CAP, erase storage, delete unrelated packages, or
run another long support sweep.

Next: follow `putfield_a_this` during constructor/install, inspect the
serialized field tag and `JC_Instance::getFields()` restore path, and create an
isolated reference-valued instance-field reset regression. Exact run logs and
firmware hash are in the 2026-11-03 progress entry and QKB. The 8E device is
still not a PC/SC reader; 2BED remains the only physical target.

## Current session — SIO boundary committed; physical GETVERSION still FF05 — 2026-11-03 01:06 UTC

Committed cross-context private-reference owner enforcement as `9165fc4`.
Current validation passes PC, ARM and RISC-V `max_crypto` builds, the RMI
registry suite, and the complete cross-AID VPCD suite 18/18 both with and
without Descriptor. The 8E board still enumerates at USB but does not appear
as a PC/SC reader; 2BED is the only physical reader available.

On 2BED, the exact-reader JCAlgTest probe again selected 1.8.2 with `9000` but
GETVERSION returned `FF05`. A finite serial capture reports the firmware
startup self-check passing. Its CCID status snapshot was overwritten by later
host slot-status polling, so the capture did not reveal the applet response.
Next implement/test a retained XfrBlock response snapshot, then rerun the
bounded GETVERSION probe. Do not run the long support sweep until GETVERSION is
healthy. In parallel, continue CTX-01 with an equal-handle collision fixture
and audit remaining reference consumers. See the 2026-11-03 progress entry and
QKB for the exact tests and limitations.

## Current session — USB failover board not enumerated by PC/SC — 2026-11-03 00:51 UTC

Host USB inventory sees both Pico 2 boards: `8E464F4E2BBC0A19` at bus 001
device 029 and `2BED26ED20556535` at device 031. Both enumerate as `2e8a:0009`
with the CDC and CCID interfaces. The authorized PC/SC helper grants `pcscd`
read/write access to both device nodes, but `pcsc_scan -r` lists only 2BED.
The 8E board cannot currently serve as APDU failover, despite USB enumeration.
No APDU was sent to 8E and it was not flashed. Next retry PC/SC discovery if
its reader appears; keep investigating 2BED's `FF05` applet failure offline.

## Current session — reflash did not restore JCAlgTest APDU behavior — 2026-11-03 00:45 UTC

After the 1.8.4 client run, the unchanged 1.8.2 CAP was reinstalled twice and
the card remained listed, but GETVERSION returned `FF05`. Both ARM and
RISC-V `max_crypto` builds pass. Reflashed Pico 2 `2BED26ED20556535` with ARM
UF2 SHA-256 `78e7fdbffa6654091963c81e8839b4034646db1d02047d4c268ff4f504e42e43`;
the finite monitor passed 18/18 `TEST:SUMMARY pass=1 fail=0` checks, with one
`WATCHDOG_RECOVERY cause=other` marker. After the flash, SELECT still returned
`9000` while 1.8.2 GETVERSION still returned `FF05`.

The current PC VPCD `max_crypto` integration test independently passes 15/15,
including 1.8.2 GETVERSION, JCSystem info, SHA-256 prepare/doFinal/static
performance, and the expected unsupported-algorithm case. Thus a clean current
PC image and CAP pass; the physical card/APDU path remains broken after
package replacement and firmware reflash. Do not clear other applet packages
or erase persistent storage. Diagnose package instance/static state and
physical PC/SC routing; retry on another identified board if one is available.
Logs and hashes are in the 2026-11-03 progress entry and `.agent/QKB.md`.

## Current session — selectable JCAlgTest client and 1.8.4 recheck — 2026-11-03

`tools/run-jcalgtest` now accepts `--client-jar PATH` (repo-relative or
absolute); its default remains the extracted 1.8.2 client. The physical runbook
documents selecting `AlgTest_JavaCard/deploy/client/AlgTestJClient_v1.8.4.jar`
for the unchanged 1.8.4 CAP.

Rechecked Pico 2 `2BED26ED20556535` with that exact 1.8.4 client and CAP. The
reader was listed at PC/SC index 3; SELECT succeeded, but applet GETVERSION
returned `FF05`. BASIC was stopped after 3,275 result rows, all labeled
`NullPointerException`. The finite monitor still passed the firmware runtime
check. The cleanup path uninstalled 1.8.4 and reinstalled unchanged 1.8.2;
GPPro lists the applet, but a subsequent SELECT/GETVERSION probe still gets
`FF05`. Treat the card as not verified restored, despite package inventory.
No firmware was flashed. Logs and hashes are in the 2026-11-03 progress entry
and `.agent/QKB.md`.

Next investigate the GETVERSION/uncaught-ISOException path on this card before
running another long support sweep. The earlier 2026-11-02 full matching-client
runs remain valid for their recorded board state. Preserve all unrelated
worktree changes.

## Current session — matching JCAlgTest 1.8.4 client runs — 2026-11-02 21:08–23:41 UTC

Used `AlgTest_JavaCard/deploy/client/AlgTestJClient_v1.8.4.jar` with the unchanged upstream 1.8.4 CAP. Runs on 8E and 2BED are recorded with hashes and row counts in the matching 2026-11-02 `progress.md` entry. Extended support on 8E ended with `SCARD_E_NOT_TRANSACTED` after 7,875 result rows and the reader disappeared; root cause is unknown. On 2BED, extended reached 5,836 rows in its 3,600-second bound, BASIC reached 1,421 rows in its 900-second bound, and static performance reached 170 timing values in its 900-second bound. No transport errors occurred on 2BED. The matching jar’s help/banner says 1.8.4, but generated CSV metadata says client 1.8.3; applet metadata correctly says 1.8.4.

2BED was restored via GPPro to the unchanged 1.8.2 CAP and verified by SELECT/GETVERSION, JCSystem info, and the SHA-256 physical probe (`TEST:PASS`). It is the only Pico reader currently visible; 8E remains absent. No firmware was flashed. Continue future performance/support tests from separate saved output paths without `-fresh` so completed rows can resume; respect the repository USB host-access gate.

## Current session — physical JCAlgTest 1.8.4 CAP test — 2026-11-02 21:08 UTC

On Pico 2 `8E464F4E2BBC0A19`, temporarily replaced only the installed upstream
JCAlgTest package with the unchanged `AlgTest_v1.8.4_jc305.cap` (SHA-256
`cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`). GPPro
LOAD/INSTALL and `--list` succeeded; the official local 1.8.2 host client
selected the applet, read version `1.8.4_jc305`, and completed
`ALG_SUPPORT_BASIC`. CSV: 362 algorithm rows, 148 `yes`, 214 `no`, zero other
statuses; the 20 remaining rows are section headers. This validates basic
1.8.4 CAP execution with the 1.8.2 client, not a matching 1.8.4 client or the
extended/performance suites. Logs and CSV hashes are in the 2026-11-02 progress
record. Restored the unchanged 1.8.2 CAP; GPPro inventory and SELECT/GETVERSION
confirm 1.8.2 is back. No firmware was flashed.

Next, if full 1.8.4 acceptance is required, obtain/use the matching 1.8.4
client and run extended support/performance separately; do not treat this
basic subset as full-version acceptance. Preserve the existing unrelated dirty
worktree changes.

## Initial plan PDF filing — 2026-11-02

- Moved the October 9 original port-plan PDF, unchanged, to
  `docs/initial-pico2-jcvm-port-plan-2026-10-09.pdf`; its SHA-256 is
  `a8d26af03fc0d3ccbfd64f1466e73ad814dc5d6ca258d9bb4d5189ca08d6b1cb`.
  The draft names JCAlgTest 1.8.4; current `plan.md` and `TODO.md` use 1.8.2.
  README and the relocation index now distinguish them. No build or physical
  operation was run for this documentation-only move.

## Markdown tidy-up — 2026-11-02

- Archived completed fixes and dated reviews in `artifacts/reviews/` (commit
  `ebc253f`). Replaced the root command-heavy readme with `README.md`, moved
  its physical workflow into `docs/`, and archived the superseded STATUS
  snapshot (commit `be0b2fc`). The active backlog remains root `TODO.md`.
- Moved development performance history into `docs/` with a small root
  compatibility link; archived an obsolete autonomous-firmware reference,
  removed the incomplete `HUMAN.md` stub, fixed the root skill's compilation
  link, and added `docs/documentation-moves-2026-11-02.md` for old paths.
- Documentation links and scoped whitespace checks passed. No firmware build,
  physical operation, or runtime test was performed for these documentation
  changes. Preserve unrelated pre-existing source and documentation edits,
  including the uncommitted firmware-architecture move and practical guide.

## Current session — private-reference SIO firewall boundary — 2026-11-02 20:06 UTC

Cross-context SIO argument marshalling now retains the caller package ID beside
non-null local reference handles. Generic and compact `aload`/`astore`
bytecodes preserve that owner, and array dereference bytecodes deny access
before resolving a foreign handle in the receiving heap. The regression forces
`aload_1; astore_2; aload_2; arraylength`; the service catches
`SecurityException` and returns `5A31`.

`./tools/test-rmi-registry`, PC/ARM/RISC-V max_crypto builds, VPCD with
Descriptor omitted (18/18) and included (18/18), and the expanded physical
GPPro LOAD, INSTALL, SELECT, SIO/private-array/global-Object[] firewall probe
and cleanup all passed. Cross-package global `aaload` now yields an opaque
reference retaining its source package tag; a later server-side use is denied
with `6B319000`. Pico 2
`8E464F4E2BBC0A19` runs the ARM image with UF2 SHA-256
`78e7fdbffa6654091963c81e8839b4034646db1d02047d4c268ff4f504e42e43`; finite
monitor passed repeatedly. Firmware ends at `0x100e83bc`, leaving 97,348 bytes
in the 1 MiB firmware window.

This covers private array parameter transport and array access plus opaque
cross-package global `Object[]` read/use only. Continue auditing all other
reference-consuming bytecode/native paths; define collision, comparison,
return, storage/GC, stale-owner, and deletion behavior. Implementation commit
and final logs are recorded in the 2026-11-02 progress entry; preserve
unrelated worktree edits in AGENTS, heap/import/cap loading, scripts and docs.

## Current session — reference-owner provenance sidecar — 2026-11-02 19:29 UTC

Added one byte of owner provenance alongside each VM operand-stack word.
Owner-aware APIs preserve it through reference pushes/pops, local reads/writes,
`aload`/`astore`, `dup`, `swap`, and typed nested-call locals. Null and
JCRE-global references are tagged global. Updated CTX-01 in TODO and added the
collision constraint and validation record to QKB. The added sidecar costs 256
bytes per 256-word stack, in addition to the existing 96-byte three-bit word
type tags.

`./tools/test-rmi-registry`, ARM `max_crypto`, and RISC-V `max_crypto` builds
pass. ARM firmware ends at `0x100e8160`, within the 1 MiB firmware window. The
user reported three BOOTSEL boards, but elevated `lsusb -d 2e8a:000f` returned
no device; no hardware flash was attempted.

This is only provenance plumbing. Existing marshalling still rejects private
reference arguments, and access-site firewall enforcement/equality semantics
are not implemented. Next: thread provenance through reference comparisons,
returns, and dereference operations, reject foreign-owner use as required, then
add collision and private-array SIO fixtures before allowing private refs to
cross. Implementation commit: `2c2cf93`. Preserve unrelated edits in `src/cap_load.cpp`, heap,
import handling, scripts, docs and AGENTS.md.

## Current session — refresh max_crypto ARM external-test image — 2026-11-02 19:18 UTC

Rebuilt `max_crypto` using `./tools/build arm --profile max_crypto` and
`./tools/build riscv --profile max_crypto`; both passed (147 of 150 declared
capabilities enabled). The ARM image ends at `0x100e7df8`, leaving 98,824 bytes
in the 1 MiB firmware window. Refreshed the tracked external-test artifact
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` (1,900,032 bytes,
SHA-256 `7456bf4826c0aa9578e989992f566fd58878c8f212d06ed23e9b27ac756b114d`) and
confirmed it matches the build output byte-for-byte. The generated support CSV
was also compared and copied; its existing tracked copy already matched.

The image has no applet preinstalled. `README-max_crypto.txt` now records the
current hash/size and explicitly says this refreshed hash has not been flashed
or runtime-tested. It was built from the shared worktree, including local
`src/cap_load.cpp` changes, so it is not reproducible from a clean checkout
until those changes are integrated. No device was touched. The user's external
test task is complete; next resume the open CTX-01/03 work after that task is
resumed. Artifact refresh commit: `ec395d9`.

## Current session — require physical reclaimer telemetry — 2026-11-02 19:01 UTC

Updated `tools/test-persistent-reclaimer-physical` to parse the stable 16-digit
chip ID from the current exact reader name, run a bounded exact-chip serial
capture during its APDU probe, and fail unless telemetry reports a completed
erase with at least one candidate. It also rejects `FAULT`/`TEST:FAIL` serial
output and logs the serial artifact path. `bash -n` and targeted whitespace
checks passed. The real hardware test passed 5/5 and cleanup after the script
verified `PERSISTENT_GC owner=4 result=0 erased=1 candidates=1`.

PC/SC renumbered 2BED from slot `00 00` to `01 00` when a second Pico appeared;
the first stale reader-name attempt stopped at GPPro listing before installation.
The rerun with the current slot passed. Refresh reader enumeration before tests;
retain chip IDs, not slot suffixes. Artifacts:
`artifacts/logs/persistent-reclaimer-physical-20261102T185839Z.log` (SHA-256
`65a060fd983ab98c408761abad656d9f5b0e52387c0de18973b1f2ab1704aa66`), serial
`artifacts/logs/persistent-reclaimer-serial-2BED26ED20556535-20261102T185839Z.log`
(SHA-256 `81c454dec8a9411846aac84e8698e8b5dd187543ec59437e073dabcb4a1f9258`).

Next continue the highest open CTX-01/03 item: cross-context reference ownership
and firewall semantics. Keep persistent reclamation disabled pending multi-owner
and power-interruption acceptance. Unrelated pending worktree edits remain
untouched.


## Current session — persistent reclaimer physical telemetry — 2026-11-02 18:54 UTC

Built both architectures with `PICO2_DEBUG_PERSISTENT_GC=1` and flashed the
hook-enabled current ARM image to Pico 2 `2BED26ED20556535`. The first picotool
attempt lacked a reset interface; the cycle wrapper recovered through its
CDC-1200 BOOTSEL path, then flash write/verify reached 100% and the 18-second
monitor passed. Firmware ends at `0x100d0138`, using 852,280/1,048,576 bytes
and leaving 196,296 bytes. UF2 SHA-256:
`38d585f20414648427011e5a21a78f3acfa13bce3bb442eeb7861ad38d267604`.

Ran `tools/test-persistent-reclaimer-physical` while capturing serial. All five
APDU assertions and temporary package cleanup passed. On the release APDU,
serial showed `CHOUPI:PERSISTENT_GC owner=4 result=0 erased=1 ... candidates=1`;
subsequent rooted-array read passed. This proves one live single-owner erase
case on hardware. It does not prove concurrent/multi-owner protection or
power-loss recovery during an erase; leave the reclaimer disabled in normal
collection. QKB/TODO now distinguish APDU behavior from tag-erasure evidence.

Logs: physical APDUs `artifacts/logs/persistent-reclaimer-physical-20261102T170042Z.log`
(SHA-256 `833d036b146cbb87723a0663e6b117be0a90245e62d0fbf656f5c1fad7cb5fd3`),
serial `artifacts/logs/persistent-reclaimer-serial-2BED-20261102T171000Z.log`
(SHA-256 `3e89318190f7a290db287e50ae9e0497c1fedb332e54fa032a68acf22bd86bff`),
cycle `artifacts/logs/cycle-choupi-20261102T165652Z.log` (SHA-256
`dea23a2e06ee37be165115c2f74e6034cec9c2f4a4fc0debbb0d284ea753c4e1`).

Next: make the physical harness require captured `PERSISTENT_GC erased>0`
telemetry so a non-hook image cannot be reported as reclaimer validation; then
continue CTX-01/03 cross-context reference semantics. Keep physical power-loss
acceptance open until an actual interruption can be performed and verified.

## Current session — package-owned references in global Object[] — 2026-11-02 16:50 UTC

Implemented per-slot owner tags for local object/array references stored in
volatile JCRE global `Object[]` arrays. Same-package `aaload` resolves the
original compact reference; cross-package reads throw `SecurityException`
before exposing a possibly colliding heap-local handle. References are roots
only in the owning package, reset clears slot data and tags, and package
removal clears slots before its ID is reused. JCTransactionProbe case 41 now
checks same-package store/read identity.

Validation in the resumed session: `./tools/test-rmi-registry` passed;
`./tools/build arm --profile current` and `./tools/build riscv --profile
current` passed. ARM image ends `0x100cff20` (851,744 / 1,048,576 bytes,
196,832 remain). Focused VPCD case 41 passed (`009000`, 71.496 s). Earlier in
this implementation step, physical Pico 2 `2BED26ED20556535` passed the full
transaction probe 10/10 and cleanup after flash/monitor. The first target
`8E464F4E2BBC0A19` flashed and verified but did not expose serial afterward;
work continued on 2BED. Its monitor contained one retained
`CHOUPI:WATCHDOG_RECOVERY ... cause=other` marker followed by repeated runtime
passes. Do not attribute that marker to this change without diagnosis.

The cross-package opaque-reference/proxy path remains unimplemented, so this
closes only same-package global-array retention, GC, reset, and package-delete
coverage. TODO and QKB are updated; selective implementation commit is `fffebb8`. Preserve unrelated edits in AGENTS.md, cap_load/import
handling, docs, test scripts, and the removal of `releaseGlobalAIDObject` from
heap.cpp/hpp. Next implementation target: add an owner-aware opaque reference
representation usable by `aaload` and normal firewall checks, then test
cross-package read/access, collisions, stale owners, and lifecycle behavior.

## Current session — current-profile reader recheck — 2026-11-02 16:04 UTC

Both required current-profile builds pass. ARM firmware end is `0x100cf988`;
850,312 of the 1,048,576-byte firmware window is used (198,264 bytes remain).
The user's latest note said three boards were in BOOTSEL, but elevated
`picotool info` found no BOOTSEL device and saw two runtime Pico 2 USB devices.
`pcsc_scan -r` confirmed readers `8E464F4E2BBC0A19` and `2BED26ED20556535`.
The bounded read-only `tools/test-pico-powerup` probe passed on both: ATR
`3B800181`, JCAlgTest 1.8.2_jc305, SELECT and JCSystem info returned `9000`.
Their installed package inventories differ. No firmware was flashed, no card
state was modified, and no cold-power removal was tested. Logs and hashes are
recorded in the latest `progress.md` entry.

Continue CTX-01 global Object[] ownership work. The setReferenceEntry guard in
`choupi/src/jc_types/jc_array.cpp` remains intentional: raw local handles alias
across package heaps, and safe support needs opaque owner-aware references,
GC roots, deletion, reset and stale-owner handling. Do not simply remove the
guard. Preserve all currently unrelated worktree changes.

## Current session — CTX-03 allocation pressure and exception token mapping — 2026-11-02 15:55 UTC

Fixed `bc_exceptions.cpp` to derive ISOException's embedded class token from the
generated JC305-to-ROM map. The previous constant 18 now resolves to
CardException in the current ROM mask and misreported its RMI type as
ISOException. The RMI VPCD wrapper now creates a fresh empty input image rather
than reusing a stale build artifact that already contains package 56. The
unchanged RMI fixture performs four consecutive allocation-pressure APDUs
(256 short-lived 64-byte arrays each) before the remaining array/exception
cases.

Validation passed: current ARM and RISC-V builds; VPCD RMI array suite 42/42
with first pressure collection `arrays=5/261`, `reclaimed_arrays=256`, and
RMI-reference interface suite 3/3. Current ARM firmware end is `0x100cf988`,
leaving 198,264 bytes in the 1 MiB firmware window. The UF2 flashed to both
physical boards has SHA-256
`5a105ffd8d4b5171c1ede2c8468bf62d86406983c585556979d3950a69db7e14`.
Exact-chip-ID cycles and finite serial checks passed on
`8E464F4E2BBC0A19` and `2BED26ED20556535`; four pressure calls and the complete
physical RMI array/exception probe then passed on each board with cleanup.
The initial serial-targeted cycle did not reach BOOTSEL; targeting the same
board by chip ID succeeded.

Logs: VPCD RMI suite
`artifacts/logs/rmi-array-invoke-vpcd-20261102T155109Z.log` (SHA-256
`8c450e4d6d5f885cffc20b5de0ad5d88d35695b0a244f5adf1f7cb50a9333857`), second
VPCD reference check `artifacts/logs/rmi-reference-interfaces-vpcd-20261102T155109Z.log`
(SHA-256 `cc8189d264a054ef1c31e53585d169d6eb402da05c765da2503a3b760b4778b1`),
physical probes `artifacts/logs/rmi-array-physical-20261102T155323Z.log` and
`...155426Z.log` (SHA-256 `3fa3283abaa57d6e503c92edb66730ce8d9bb88d717a52cf06da0c2977971c1a`
and `af4dfd07b721559e1af1b56b78f3ea2b5fcbcb2fd52731cdbfc0f4a107c90445`).
Source/test commit: `ea6a005`; append-only progress and this handoff update are
in the follow-up metadata commit.

Continue the remaining CTX-01/03 and PERSIST acceptance. In particular, design
ownership-safe global `Object[]` references across independently numbered
applet heaps before changing the current fail-closed behavior; keep cold-power
persistence separate from the APDU allocation-pressure result. Preserve all
other pre-existing dirty worktree changes.

## CTX-03 same-CAP lifecycle replicated on second Pico — 2026-11-02 15:01 UTC

Ran the extended physical sibling harness on `2BED26ED20556535`. SIO identity
(`019000`), initial and writer-updated static values, applet DELETE/recreate,
package DELETE/LOAD/CREATE, fresh static initialization, and cleanup all
matched `8E464F4E2BBC0A19`. Log SHA-256:
`c7ca0453911b758544a8a5dce5572a7df8f825f6f41ea2784de638affe6187db`.
The flashed firmware hash on 2BED was not captured; do not infer both devices
ran the same image. No status/result differences were observed. Comparison is
committed as `53cd1a2`; the progress SHA reference is in the metadata commit.

## CTX-03 second-board transient replication — 2026-11-02 14:56 UTC

Repeated the same physical same-CAP transient test on Pico 2
`2BED26ED20556535` after it passed on `8E464F4E2BBC0A19`. Results matched
exactly: set arrays `5A6B9000`, sibling deselect `006B9000`, PC/SC reset
`00009000`; GPPro cleanup passed. Log SHA-256:
`b340ba2d91faba8ee42ad49df14b0929221c61731b897c3439941cb5ecd83be5`.
This comparison is committed as `2c0c912`; the progress SHA reference is in
the metadata commit. Both are runtime readers. Continue CTX-01 global-reference
work offline or other persistence acceptance; no need to wait for BOOTSEL.

## CTX-03 physical same-CAP transient reset — 2026-11-02 14:52 UTC

Added `tools/test-same-cap-transients-physical` and a host SmartcardIO probe
for the existing unchanged test CAP. The physical run on Pico 2
`8E464F4E2BBC0A19` passed: writer set both arrays (`5A6B9000`), selecting the
sibling cleared `CLEAR_ON_DESELECT` and retained `CLEAR_ON_RESET`
(`006B9000`), and PC/SC reset cleared both (`00009000`). Cleanup passed. The
VPCD suite passed `TEST:SUMMARY pass=5 fail=0`. Physical log SHA-256:
`904aab987b954ef4846e7ba480d90704e8f79e567afc2c3a40c18aa53d4976b7`; VPCD
log SHA-256:
`ec92680ca2d78aa928e39d03b2787160867481eabf7f22cb72eef059efff662d`.
Harness and record are committed as `0a6110a`; the progress SHA reference is
in the metadata commit. Remaining same-CAP physical acceptance is GC and
cold-power persistence; the TODO also tracks global-reference ownership and
other CTX-01 gaps.

# CTX-03 physical same-CAP sibling selection/static state — 2026-11-02 14:34 UTC

The guarded physical harness installed the same test CAP once with GPPro, then
created writer and reader instances on Pico 2 `8E464F4E2BBC0A19`. Shareable
array identity returned `019000`; initial static values returned
`000012340056123456789000`; writer mutation returned
`000012350157123456799000`; the sibling reader observed those changed values.
Fixture cleanup deleted both instances and the package. The first probe got
`6D00` because it did not reselect the reader after SIO, unlike the existing
VPCD sequence; matching the sequence made the physical check pass. This is not
a firmware failure. Log SHA-256:
`0a8551584c163700174dd6fe767f09d146db1b30726df8e98f3027d207fca5fe`.
VPCD same-CAP regression rerun passed `TEST:SUMMARY pass=6 fail=0`.

Remaining CTX-03 physical acceptance: GC and persistence. Latest ARM image remains
948,368 bytes with 100,208 bytes left in the 1 MiB firmware window. Preserve
unrelated dirty files. Harness and validation record are committed as
`17b9e4a`; the progress SHA reference is recorded in the follow-up metadata
commit. Next inspect CTX-01/03 for a safe offline/high-value gap while physical
readers remain in runtime mode.

## Same-CAP reader applet DELETE/recreate — 2026-11-02 14:42 UTC

Extended `tools/test-same-cap-siblings-physical` to DELETE the reader instance,
verify it is absent from GPPro listing, CREATE it again from the retained CAP,
and SELECT/read its package state. On Pico 2 `8E464F4E2BBC0A19`, the recreated
reader returned `000012350157123456799000`, confirming package-level static
state stayed visible across instance removal and recreation. Final cleanup
passed. Log SHA-256:
`b1ba3eb64e4144fbd7e974bc1304a5168b584c2af23f8d90bdfb90d52480d7a0`.
The test script/probe changes were committed as `28a18f8`. Remaining physical
same-CAP gaps: reset/transient, GC and cold-power persistence.

## Same-CAP package DELETE/LOAD/INSTALL — 2026-11-02 14:46 UTC

Extended the physical harness to delete both instances and the package, verify
the package is absent, LOAD the CAP again, and CREATE both siblings. On Pico 2
`8E464F4E2BBC0A19`, sibling SIO identity again returned `019000`, and initial
package static state returned `000012340056123456789000`. Cleanup passed. Log
SHA-256: `76a13511ff8b972d0b46e52d485be7c12b02d6878a5b2597a88aa1ca92957e7b`.
This harness change was committed as `36a519b`. Remaining physical same-CAP
gaps: reset/transient, GC and cold-power persistence.
Harness and progress record are committed as `36a519b`; the progress SHA
reference is in the metadata commit.
Harness and progress record are committed as `28a18f8`; the progress SHA
reference is in the metadata commit.

# CTX-01 physical cross-AID SIO/reset acceptance — 2026-11-02 14:18 UTC

Implemented reset invalidation for runtime Shareable proxy routes. A clean
physical fixture using contract package `A000000151000002`, service
`A000000151000003`, and client `A000000151000004` was LOADed/INSTALLed on Pico
2 `8E464F4E2BBC0A19`. The client cross-AID call returned `45679000`; after
retaining the proxy and PC/SC reset, its call failed closed as `6A0A9000`.
Int argument/return returned `765432109000`; caught service exception
`6A079000`; denied lookup `6A019000`; private-array firewall `6A039000`; proxy
method firewall `6A099000`; cleanup passed. This closes the clean physical
channel-0 positive SIO and reset-staleness checks, not all CTX-01 acceptance.

Cause: card reset deactivated selected/inactive contexts and cleared transient
arrays but left runtime proxy routes and server object roots. Added
`Context::clearShareableSessionState()` and call it for every retained context
on card reset; persistent proxy markers remain, so unmapped placeholders throw
`SecurityException`. `tools/test-rmi-registry` and the updated SIO VPCD suite
pass. PC/ARM/RISC-V max_crypto builds pass. The ARM image ends at `0x100e7890`,
leaving 100,208 bytes in the 1 MiB firmware window. Current UF2 SHA-256:
`ea72d4ac21a28ccadcf6328d90f9d4e4ced47d58c4c4a5b027ad6e6abfce745e`.

Logs: `artifacts/logs/cycle-choupi-20261102T141341Z.log` SHA-256
`8a5a9287f124916fcbd302bb17f8b5ff2bff167b874ba71dec4b286cb6390e13`;
`artifacts/logs/sio-cross-aid-physical-20261102T141553Z.log` SHA-256
`4504e1371df260b2ddcea535d2ae9ddf546aab55d0ded385eb212aad1460cfa2`;
serial capture `artifacts/logs/sio-reset-monitor-8E-20261102.log` SHA-256
`e95eddc87adcc7b534d1d0635a1979d02daed4457149a9324d3ae0c75c0fe21c`.
It records the post-reset CCID power-on message, 120 successful runtime
monitor summaries, and no watchdog recovery, `TEST:FAIL`, or `FAULT`.
Fixture cleanup confirmed all test packages removed. Next: continue the
remaining same-CAP/multi-package physical lifecycle and global/reference
firewall coverage, then move to open STORE/API/PERSIST acceptance. Preserve the
other dirty worktree edits; this step has not been committed yet.

## CTX-03 same-CAP sibling Shareable identity — 2026-11-02 14:25 UTC

Validated the pending same-CAP sibling fixture extension. Writer now implements
the fixture's Shareable `Probe`; reader calls
`JCSystem.getAppletShareableInterfaceObject` and invokes `isShared` with the
package static array. This checks that sibling applets in one CAP package
receive the same static-array identity through SIO. The guarded
`PICO2_HOST_USB_ACCESS=1 ./tools/test-same-cap-siblings` passed 6/6: positive
sibling SIO identity, static primitive defaults, package static writes, and
retained-sibling visibility. Log:
`artifacts/logs/same-cap-sibling-vpcd-20261102T142314Z.log` SHA-256
`e0d86e0ebcaf3cae104caa73369816d6398cbf9516ed7ad51c92dcce07574f50`.
This is VPCD evidence only; no physical same-CAP install was run. The fixture
changes and this progress record are to be committed separately from other
pre-existing dirty files. Next is a guarded physical same-CAP lifecycle test,
then remaining CTX-01 global-reference and GC/delete behavior.

# ARCH-08 physical USB progress gate accepted — 2026-11-02 13:43 UTC

Instrumented `choupi_ccid_native_crypto_progress()` to record call duration,
TinyUSB service duration/event counts, and CCID time-extension submission and
completion counts. The summary is emitted from the main loop after the APDU.
Built ARM max_crypto and RISC-V smoke successfully, then flashed identified
Pico 2 `2BED26ED20556535` with `tools/cycle`; flash verification and finite
monitor passed.

The physical `tools/test-ccid-progress-physical` run sent standard USB
GET_DESCRIPTOR requests while the existing physical RSA-3072 generation and
signature CAP ran. APDU returned `019000` in 118,055 ms. Host sent 4,336
successful control requests during the 118,267-ms APDU interval, with zero
failures; all five post-APDU drain requests succeeded. Firmware telemetry:
5,795 progress calls, 3,301 us total, 117 us max; 169 USB service passes,
1,006 us total, 94 us max; 86 events processed, max two per pass; 85 time
extensions submitted and 85 completed. No watchdog-recovery marker,
`TEST:FAIL`, or `FAULT`; cleanup passed. `tools/test-ccid-progress-flow`
passed 9/9. The old 49,184-ms RSA-3072 timing remains a different run; the
new workload's comparison point is its 118,055-ms measured response under
sustained traffic.

ARM firmware end is `0x100e7804`, leaving 100,348 bytes in the 1 MiB firmware
window. UF2 SHA-256:
`50e9ed105bb3a933ce1dc44313a0b2987747524e1af3cb028690daab132b1334`.
Logs: `artifacts/logs/cycle-choupi-20261102T132946Z.log`,
`artifacts/logs/ccid-progress-monitor-2BED26ED20556535-20261102T133921Z.log`,
and `artifacts/logs/asymm-crypto-pcsc-20261102T133921Z.log`. ARCH-08 is now
closed; continue CTX-01/03 and the open storage, API, and persistence gates.

# JCAlgTest max_crypto physical support validation — 2026-11-02 13:18 UTC

The current ARM `max_crypto` image was rebuilt for ARM and RISC-V and reflashed
to Pico 2 `2BED26ED20556535` after its PC/SC reader disappeared during an
extended support run. `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile
max_crypto --chipid 2BED26ED20556535` recovered the board through its CDC
1200-baud BOOTSEL path; flash verification reached 100% and the finite monitor
passed. The startup log contained a retained `WATCHDOG_RECOVERY ... cause=other`
marker, but a targeted exact APDU from the interrupted extended test returned
`9000` in 163 ms after reflash. Exact-reader SELECT, GETVERSION and
JCSystem-info probes also passed.

The official JCAlgTest 1.8.2 `ALG_SUPPORT_BASIC` run completed on 2BED:
362 rows, 148 yes, 214 no, zero other. Comparator gaps are explained by two
known upstream RSA-3072/4096 mislabeled KeyPair queries and three
`Cipher.getInstance` aliases not covered by BASIC. The separate
`ALG_SUPPORT_EXTENDED` run timed out at 900 seconds after 2,094 CSV lines; do
not count it complete. Current result CSV SHA-256 is
`eb56504b226a92ac5f34a8ff97569e307211d189b079ed612bf26178343a4ade`; current
UF2 SHA-256 remains `16fd3afabc24b447a9eaf94989b676bdaa8e929b399cea5056b5b9be7b906b4c`.
Details are in TODO.md, QKB and progress.md. Continue with a bounded extended-
mode plan, then return to API-01/03 and remaining persistence, storage and
context acceptance. Refresh exact reader enumeration before the next physical
operation.

# API-02 physical Signature-IV acceptance — 2026-11-02 12:42 UTC

Flashed the current ARM `max_crypto` image to identified Pico 2
`2BED26ED20556535`. The first cycle targeted firmware serial and failed to
match BOOTSEL after the 1200-baud reset. `picotool info -a` read chip ID
`0x2bed26ed20556535` from the visible BOOTSEL interface; rerunning
`PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile max_crypto --chipid
2BED26ED20556535` succeeded. Both ARM and RISC-V build gates passed, flash
verification reached 100%, and the 18-second monitor passed repeated
`TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. Cycle log:
`artifacts/logs/cycle-choupi-20261102T123918Z.log`.

The unchanged API 3.0.5 Crypto CAP was then loaded, run through its official
client with only the three Signature MAC-IV calls enabled, and cleaned up.
All three returned `OK` with expected lengths: DES-MAC8-NOPAD 8, AES-MAC-128
16, DES-MAC8-ISO9797-M2 8 bytes. The CSV records `OK=3`, `SKIPPED_BY_USER=463`,
and successful cleanup:
`artifacts/results/Pico2_2BED_api305_signature-iv_maxcrypto_20261102.csv`
(SHA-256 `0c88385d492e08d0c1dd40c8328176b08fe86fc01abd9929695848635332cbba`).
API-02 is closed; API-03 full area coverage remains open. UF2 size is 1,895,936
bytes, SHA-256 `16fd3afabc24b447a9eaf94989b676bdaa8e929b399cea5056b5b9be7b906b4c`;
linked image uses 947,700/1,048,576 bytes and leaves 100,876 bytes.

Flash note: a serial-targeted run failed to select BOOTSEL, but chip-ID-targeted
cycle succeeded for the same board. The observed workaround and limits are in
`.agent/QKB.md`. Latest inventory before flash showed two runtime readers; one
was used and is now back in runtime mode. Continue API-03 / remaining project
acceptance with physical access refreshed before each operation.

# Physical read-only persistence probes on both runtime boards — 2026-11-02 12:30 UTC

The authorized PC/SC inventory found two runtime readers:
`8E464F4E2BBC0A19` and `2BED26ED20556535` (both firmware-mode CCID; no BOOTSEL).
`PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup '<exact reader>'` passed on
both. Each GPPro `--list` showed the JCAlgTest package/applet; each reader
returned ATR `3B800181`, SELECT `9000`, GETVERSION `1.8.2_jc305` / `9000`, and
JCSystem info `9000`. Timings (SELECT / GETVERSION / JCSystem info): 8E =
75/59/129 ms; 2BED = 47/73/121 ms. Logs:
`artifacts/logs/pico-powerup-persistence-20261102T122841Z.log` and
`artifacts/logs/pico-powerup-persistence-20261102T122908Z.log`.

This validates the currently deployed runtime cards, but the script did not
identify firmware image hashes and did not remove USB power. Cold-power
restoration remains open; no flash occurred. Next: continue open offline
acceptance and use these identified devices for physical follow-up when the
specific test is safe. Current max_crypto ARM build is available at
`build/arm/pico2_choupi.uf2`; it was not installed as part of these probes.

# CTX-03 shared owner-ID allocator and regression — 2026-11-02 12:25 UTC

The RP2350 CAP store and PC/VPCD backend now call the same bounded first-free
owner-ID selection helper in `choupi/src/applet_owner_allocator.hpp`. The RMI
registry test covers all 255 occupied IDs, preserves the caller's output when
allocation fails, chooses the lowest freed ID, and confirms live Heap objects
still block allocation after the persisted bitmap is cleared; after GC, ID 1
is reusable. The filesystem overlay test confirms the device-side metadata
allocator rejects ID 256 and reuses a deleted ID. The PC/VPCD
delete/reinstall test passes 9/9, including keeping owner 1 while its shared
array is live and reclaiming it after package deletion. A full 255-owner PC
VCR2 serialized-registry restore remains untested.

Validation passed: `./tools/test-rmi-registry`,
`./tools/test-rp2350-fs-overlay` (`TEST:SUMMARY fail=0`),
`./tools/build-pc --profile max_crypto`, `./tools/build arm --profile
max_crypto`, `./tools/build riscv --profile max_crypto`, and the elevated
`PICO2_HOST_USB_ACCESS=1 ./tools/test-same-cap-delete-reinstall --timeout 60`
(`TEST:SUMMARY pass=9 fail=0`). ARM firmware end remains `0x100e75f4`, with
100,876 bytes in the firmware window. The new broader JCAlgTest VPCD run also
passed twice, `TEST:SUMMARY pass=30 fail=0` (recorded in progress.md).

Latest authorized USB refresh found two runtime CCID readers and no BOOTSEL
board. No physical flash was attempted. Next: review remaining CTX-03
GC/reset edges, then pursue the open PERSIST-02 cold-power acceptance when a
board is actually available. Preserve all unrelated worktree changes.

# JCAlgTest VPCD regression — 2026-11-02 12:18 UTC

`PICO2_HOST_USB_ACCESS=1 ./tools/test-pc-vpcd-applet` completed with exit 0,
`TEST:SUMMARY pass=30 fail=0`, over two fresh emulator passes. Each pass
completed ATR, applet SELECT, JCSystem info, supported-algorithm probes,
SHA-256 final and static-performance calls, AES/RSA support, and the expected
`NO_SUCH_ALGORITHM` case. First-pass connect took 29.258 s and GETVERSION APDU
16.570 s; second-pass connect took 0.181 s and GETVERSION APDU 0.349 s. This is
host VPCD integration evidence, not physical acceptance. The log
`artifacts/logs/host-vpcd-jcalgtest-20261014.log` currently hashes to
`b612c9f275c898a73221d1694e21f7b8dda06d9d986b8cb54eff5918383ee7c0`.

Next: review remaining CTX-03 GC/reset edges and continue PERSIST-02 cold-power
acceptance when an identified board is available. Latest earlier authorized
inventory found two runtime CCID devices and no BOOTSEL; refresh via the
approved helper before any hardware operation. The project remains far from
complete: physical persistence, storage/API coverage, cross-applet semantics,
and external Windows workflow gates remain open.

# CTX-03 owner, transient, and restart lifecycle checks — 2026-11-02 12:10 UTC

Fixed owner-ID reuse while package-shared VM objects remain alive. A new
same-CAP VPCD regression installs writer and reader applets as owners 1 and 2,
checks the reader sees the shared static array, deletes the writer, reinstalls
it, and verifies the new owner is 3 while the reader remains owner 2 and can
still access the array. Two more instance DELETE/reinstall cycles produce
owner history `[1, 3, 3, 3]`: ID 1 stays reserved for the live array, while
object-free ID 3 is reclaimed. Full package DELETE/reinstall drops the heap
and makes ID 1 reusable. The PC emulator had a separate allocator in
`choupi/src/main_pc.cpp` that bypassed the VM owner-retention callback; it now
uses the same callback as the RP2350 allocator. In normal builds the callback
is required; it stays weak only for the standalone filesystem-overlay target.

Validation passed: PC max_crypto build; `tools/test-rmi-registry`;
`tools/test-rp2350-fs-overlay` (`TEST:SUMMARY fail=0`); same-CAP
delete/reinstall VPCD (`TEST:SUMMARY pass=9 fail=0`); multipackage VPCD
(`TEST:SUMMARY pass=47 fail=0`); ARM and RISC-V max_crypto builds. ARM image
ends at `0x100e75f4`, using 947,700/1,048,576 bytes and leaving 100,876 bytes.
UF2 is 1,895,936 bytes, SHA-256
`db74bd5a3efe95df2594957e3b9f5932456e40c883fade5319e23ddeb791c497`.
Implementation commits: `b33d401` (owner retention), `a4d5cfe` (reclamation
regression). Progress metadata commit: `6bd1b07`.
Latest authorized USB refresh found two runtime CCID devices at USB nodes
`/dev/bus/usb/001/003` and `/dev/bus/usb/001/004`; no BOOTSEL device, so no
physical flash was attempted. Preserve unrelated worktree changes, especially
the user's `releaseGlobalAIDObject` removal in `heap.cpp/.hpp`.

The focused `tools/test-same-cap-transients` VPCD regression passes 5/5: a
same-package sibling switch clears `CLEAR_ON_DESELECT` but retains
`CLEAR_ON_RESET`, and card reset clears both. Log:
`artifacts/logs/same-cap-transient-vpcd-20261102T120128Z.log`. Test-only CAP and
wrappers are committed in `c201188`.

`tools/test-same-cap-restart` now passes 7/7. After clean emulator shutdown and
restart, both same-CAP applet registrations restore, the reader sees the same
static scalar and array content, SIO resolves the restored writer array by
identity, and subsequent writer changes remain visible to the reader. Log:
`artifacts/logs/same-cap-restart-vpcd-20261102T121015Z.log`. The wrapper and
test are committed in `624f266`; this is process-restart VPCD evidence, not
physical power-loss validation.

`tools/test-rmi-registry` now allocates owner facades for IDs 1 through 255,
retains one live array for each, confirms all remain reserved, then collects
the arrays and confirms those owner states are reclaimable. It passes with
`owner_id_exhaustion=1`. The expanded fake-flash CAP storage test also assigns
IDs 1..255 across package slots, rejects the next allocation, then deletes an
instance and reuses ID 1. `tools/test-rp2350-fs-overlay` passes with
`cap_owner_allocator_rejects_exhaustion_at_255` and
`cap_owner_allocator_reuses_id_after_instance_delete`. The separate PC/VPCD
allocator has not been driven to full exhaustion. The Context regression and
progress record are committed in `6b20d05`; the storage exhaustion test is
committed in `8281870`.

Next: review remaining GC/reset edges and continue the open persistence
acceptance. Only flash after a fresh authorized inventory finds BOOTSEL and
both architectures build.

## CTX-03 applet-local RMI/SIO registries — 2026-11-02 11:13 UTC

The package Context no longer shares RMI exports, remote IDs, Shareable proxy
mappings, or exported service roots across sibling applets. These now live in
`AppletInstanceState`; root enumeration covers every sibling, instance DELETE
clears only its state, and deleting a service AID invalidates proxies in
remaining siblings. Same-package SIO reactivates the server state to retain
its object root, then restores the client before proxy allocation.

Validation: RMI registry regression passes with new sibling isolation and
delete assertions. PC, ARM and RISC-V max_crypto builds pass. Same-CAP VPCD
`pass=6 fail=0`; cross-AID SIO VPCD `pass=15 fail=0`; multipackage restart,
delete and transient suite `pass=47 fail=0`. Latest ARM image end is
`0x100e7380`: 947,072/1,048,576 bytes used, 101,504 bytes remaining. UF2
1,894,912 bytes, SHA-256
`af4101c0012773b366979ea85a6c05d0b31bf1e130326001b8e8cce4c321cba6`.

The latest authorized USB refresh still sees two runtime CCID devices at
`/dev/bus/usb/001/003` and `/dev/bus/usb/001/004`; no BOOTSEL board is visible.
Do not flash until one enumerates. Next investigate owner-ID reuse and
tombstoned Heap facade lifetime across delete/reinstall, then test the same-CAP
delete/reinstall and reset/transient cases. Preserve unrelated dirty files and
the user's removal of `Heap::releaseGlobalAIDObject` in `heap.cpp/.hpp`.

## CTX-03 package context routing and same-CAP SIO — 2026-11-02 10:54 UTC

Firmware now looks up one `Context` per package/CAP ID and activates the
requested per-AID `AppletInstanceState`. SIO lookup/invocation restores the
caller sibling after normal return and exceptions. Instance deletion forgets
only that AID and traces remaining package roots; package deletion drops the
whole package context. `context_registry.hpp` retains package-level lookup.

Validation passed: RMI registry test; PC, ARM, and RISC-V max_crypto builds;
same-CAP sibling VPCD `pass=6 fail=0`; cross-AID SIO VPCD `pass=15 fail=0`; and
VPCD multi-package install/restart/delete/transient regression `pass=47 fail=0`.
Latest ARM image ends at `0x100e70ec`: 946,412/1,048,576 bytes in firmware's
window, with 102,164 bytes remaining. UF2 is 1,893,376 bytes, SHA-256
`93b9f5770d349ba1b1d8a8f1ea8abea243e07b999d3317908589e0b9a0be6124`.

Physical test remains pending. Although the user reports three boards in
BOOTSEL, the approved host refresh found only two running CCID devices at USB
nodes `/dev/bus/usb/001/003` and `/dev/bus/usb/001/004`, with no BOOTSEL device.
Do not flash the new image until a BOOTSEL board is actually enumerated. Next:
continue review of tombstoned owner-facade growth and per-sibling reset,
transient, deletion/reinstall semantics, then flash and test when host access
shows a board. Preserve unrelated dirty worktree changes; the user-owned
`Heap::releaseGlobalAIDObject` removal in `choupi/src/heap.cpp/.hpp` is not part
of this change and must remain unstaged.

## CTX-03 shared heap owner facades — 2026-11-02 10:30 UTC

Per-owner Heap facades now share array/instance handle tables and package
reference-root storage. Each applet-instance record owns a stable facade; VM
objects allocated through it keep their original owner identity while sibling
facades resolve the same handle. `tools/test-rmi-registry` verifies that
identity and owner retention.

PC max_crypto, RMI registry test, ARM max_crypto, and RISC-V max_crypto pass.
ARM end `0x100e6d38` leaves 103,112 bytes in the 1 MiB firmware window. UF2
1,891,840 bytes, SHA-256
`8691a8141ecc8a7d123590cdd33770fedfe73eae48f97d310cbd305f259405c6`. The
firmware registry still keys Context by applet AID and does not activate sibling
state from one package context. Same-CAP SIO probe remains `6F00`/SecurityException
at `invokeinterface`; no hardware flash/test. Next: change registry matching to
package ID and safely restore/activate sibling AID records, then prove SIO object
identity, per-owner access, deletion, reset/transient and restart behavior.

## CTX-03 sibling state groundwork — 2026-11-02 10:13 UTC

`Context` now stores multiple `AppletInstanceState` records, can select or
create one by `(owner, full AID)`, and enumerates direct roots for every retained
instance. `tools/test-rmi-registry` covers independent active AID/registration
state and both sibling roots. This preserves the existing runtime because
firmware lookup still creates one Context per AID; it does not implement
package-shared firewall context or heap ownership.

Validation: `./tools/build-pc --profile max_crypto`,
`./tools/test-rmi-registry`, `./tools/build arm --profile max_crypto`, and
`./tools/build riscv --profile max_crypto` passed. ARM image end is
`0x100e6b10`: 944,912 bytes used and 103,664 bytes remain in its 1 MiB firmware
window; full UF2 is 1,890,816 bytes (SHA-256
`1b4f93c39c39b12df620ac9ce78db30ccc943cc4592dcc1428fc3eacb5e18407`). The
same-CAP identity diagnostic still fails with `6F00`, exception 29 at
`invokeinterface`: `artifacts/logs/same-cap-sibling-vpcd-20261102T101050Z.log`.
No physical flash/test occurred. Next: redesign package-level heap/handle storage
and per-object owner facades; do not relax local-reference rejection first.

## CAP StaticField primitive initial values — 2026-11-02 09:40 UTC

Static primitive getters now consult the retained CAP StaticField image when
no persistent field record exists. This lazily reproduces default-zero and
non-default byte/short/int initial values without requiring the optional
Descriptor component or writing every initial value to flash. Existing typed
persistent records still take precedence. The test-only same-CAP fixture now
checks default and non-default scalar values, writes, and sibling visibility
with Descriptor omitted; `TEST:SUMMARY pass=5 fail=0`.

Validation passed: PC max_crypto build; focused same-CAP VPCD test; ARM and
RISC-V max_crypto builds; `tools/test-rmi-registry`; and firmware-boundary
test. Current ARM binary is 943,592 bytes, ending at `0x100e65e8`, leaving
104,984 bytes in the 1 MiB firmware window. Current build UF2 SHA-256 is
`b5d240cc067b1cbd38a22bfdfb5f83dbb970027c64a2017730dd3c768bfc6daf`.
Fresh authorized USB inventory exposed two runtime CCID devices and no BOOTSEL,
so there was no physical flash/test. The external-test artifact was not
refreshed. Next: continue CTX-03 package-context/owner separation and extend the
fixture to prove shared object identity and per-object owner isolation. The
current registry creates a Context per `(owner, full AID)`; Context holds one
AppletInstanceState and its Heap has an immutable owner. A package-shared heap
therefore needs stable per-object owner facades and a shared handle/root store;
do not alias Context or Heap objects directly.

# Session handoff — updated 2026-11-02

## CTX-03 sibling state groundwork — 2026-11-02 10:13 UTC

`Context` now stores multiple `AppletInstanceState` records, can select or
create one by `(owner, full AID)`, and enumerates direct roots for every retained
instance. `tools/test-rmi-registry` covers independent active AID/registration
state and both sibling roots. This preserves the existing runtime because
firmware lookup still creates one Context per AID; it does not implement
package-shared firewall context or heap ownership.

Validation: `./tools/build-pc --profile max_crypto`,
`./tools/test-rmi-registry`, `./tools/build arm --profile max_crypto`, and
`./tools/build riscv --profile max_crypto` passed. ARM image end is
`0x100e6b10`: 944,912 bytes used and 103,664 bytes remain in its 1 MiB firmware
window; full UF2 is 1,890,816 bytes (SHA-256
`1b4f93c39c39b12df620ac9ce78db30ccc943cc4592dcc1428fc3eacb5e18407`). The
same-CAP identity diagnostic still fails with `6F00`, exception 29 at
`invokeinterface`: `artifacts/logs/same-cap-sibling-vpcd-20261102T101050Z.log`.
No physical flash/test occurred. Next: redesign package-level heap/handle storage
and per-object owner facades; do not relax local-reference rejection first.

## Same-CAP static state probe and default primitive fix — 2026-11-02 09:21 UTC

A test-only CAP containing a writer and reader applet from one package now
proves VPCD visibility of package static short and byte-array values after
AID switches. It passed four checks, including revisiting retained applet
contexts after a second write. The first probe exposed that an absent static
primitive record threw `IOException`, producing `6F00`; static byte/short/int
reads now return Java Card zero defaults when no stored value exists. The
current CAP initializer still validates but ignores `non_default_values`; add
that to CTX-03 acceptance. This fixture does not prove shared object identity,
heap identity, or package-scoped firewall context.

PC max_crypto build, ARM and RISC-V firmware builds, RMI registry regression,
and firmware boundary checks pass. ARM image end is `0x100e64bc`, leaving
105,284 bytes in the 1 MiB firmware window. The refreshed applet-free artifact
SHA-256 is
`f0187cc260839d2905a868b2babdad71f84f7819ffcaf190d437077296c76159`. Fresh
authorized USB inventory found two runtime CCID devices and no BOOTSEL device,
so no flash or physical runtime test occurred. Next: design a shared CAP
context/heap that preserves per-object applet owners; extend the fixture to
check object identity and owner isolation before changing runtime lifetimes.

## CTX-03 instance-state refactor — 2026-11-02 09:02 UTC

Grouped the existing per-applet owner, AID, registration/install state, restore
roots, and active flag into `Context::AppletInstanceState`. This is only a
behavior-preserving ownership boundary to prepare future separation of CAP
firewall context from applet-instance state; it does not yet introduce shared
package contexts or alter semantics. `tools/test-rmi-registry`, ARM max_crypto,
RISC-V max_crypto, and the synthetic firmware-boundary test passed. The ARM
image ends at `0x100e648c`, leaving 105,332 bytes in the 1 MiB firmware window.
The refreshed artifact is applet-free and has SHA-256
`029f4a7cc1dd38d0f77fc52cc733f90b7599e01606f934068318fd2a4a505d62`; physical
runtime test is still pending. A fresh authorized USB check found two runtime
CCID devices and no BOOTSEL device. Next: continue CTX-03 design and add tests
that distinguish same-CAP sibling state from firewall context.

## Current VPCD multipackage regression — 2026-11-02 08:56 UTC

The current shared source passed `PICO2_HOST_USB_ACCESS=1
./tools/test-vpcd-multipackage --timeout 60` with `TEST:SUMMARY pass=47 fail=0`.
It exercised three packages, 21 applet instances, restart recovery, lookupAID,
GET STATUS pagination, instance/cascade deletion, and transient lifecycle.
This remains a PC VPCD regression and does not establish shared context/state
for sibling applets inside one CAP. Logs and hashes are in `.agent/QKB.md`.

Next: proceed with CTX-03 context/owner separation, preserving source edits from
other sessions. ARM/RISC-V max_crypto builds and the new flash-window guard pass;
the refreshed size-fit ARM artifact remains physically untested, and USB
inventory still exposes only two runtime CCID readers.

## Max profile firmware partition fit — 2026-11-02 08:48 UTC

The previous max_crypto ARM image ended at `0x101000c4`, 196 bytes past the
1 MiB firmware boundary; a rebuild from the shared tree ended at `0x10103b28`,
15,144 bytes past it. LTO did not link (`offset out of range`) and was removed.
Firmware CMake targets now compile with `-Os`, reducing the current max image
to end at `0x100e64cc` (943,308 bytes from flash base), leaving 105,268 bytes in
the firmware window. ARM and RISC-V wrappers pass. The build wrapper now checks
the linker map against `src/flash_layout.hpp`; its synthetic below/at/above
boundary test passes. Current build output `build/arm/pico2_choupi.uf2` is
1,887,232 bytes, SHA-256
`76de3ba3f02bc5ac26d293f469071766dc4a2b36e96edebe248e865acd4ec828`.
The tracked external-test artifact now matches this size-fit build at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`. It is an external
test candidate, not a physically validated image; runtime and crypto throughput
still need testing.

Fresh authorized USB inventory again found only runtime CCID readers
`8E464F4E2BBC0A19` and `2BED26ED20556535`; no BOOTSEL device is VM-visible.
No image was flashed. Continue CTX-03 offline; on a future BOOTSEL enumeration,
flash this build only after reconfirming identity and run a finite monitor plus
focused/max crypto acceptance.

## Max profile build and live USB refresh — 2026-11-02 08:30 UTC

Both `./tools/build arm --profile max_crypto` and
`./tools/build riscv --profile max_crypto` passed; support selection is 147/150.
The current ARM output is 2,128,384 bytes (SHA-256
`7fc09911123bf8cfd8f596c45a810391ed1a4a385d1ed621267600acb5259caa`) and does
not match the tracked external-test artifact at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` (2,098,176 bytes,
SHA-256 `1dc6d486f49e9688eedcdf65785f1629cc6de93b23c7931e22d577ed6696d82d`).
Do not replace the artifact: current shared-tree changes are uncommitted and
belong to another session; the size difference has not been isolated. RISC-V
smoke output is 79,872 bytes, SHA-256
`7b46029078921f8f2fdd3cf35d07d21cbb3bf1f6984a08921e751b257a77a925`.

The user reported three devices in BOOTSEL, but the authorized live refresh
returned two runtime CCID devices (`8E464F4E2BBC0A19`,
`2BED26ED20556535`), no BOOTSEL device. No flash occurred. Next continue CTX-03
package-context ownership design offline; refresh the USB path again before any
physical operation and preserve all dirty source/doc files.

## Latest physical Multi CAP test — 2026-11-02 08:18 UTC

Both runtime cards passed `tools/test-get-status-multipage-physical` using the
unchanged upstream `AlgTestApiMulti_305.cap`. For each serial-qualified reader,
the script confirmed its test AIDs were absent, loaded the package, installed
two applet instances, authenticated SCP03, and sent Le=40 GET STATUS. First
page returned `6310` with 32 data bytes; continuation included the expected
three applet AIDs exactly once and ended in `9000`. Cleanup passed and removed
only the temporary Multi package. Logs and hashes are in `.agent/QKB.md`.
This validates package install and status pagination on both cards, not
same-CAP object access or package-context semantics. No firmware flash or
power-cycle occurred.

Continue CTX-03 with runtime ownership/context design. Do not call this
test a package-context acceptance pass: the probe checks registry behavior
only. Two token readers remain active; no BOOTSEL device has been enumerated.

## Latest current-source VPCD check — 2026-11-02 08:13 UTC

Rebuilt the host emulator using `./tools/build-pc --profile current`; it passed
with 58 existing Rust warnings. `./tools/test-rmi-registry` passed and reported
`shareable_cross_context_arguments=1`, `typed_local_access=1`,
`typed_operand_pop=1`, plus retained-context and proxy-root checks. The first
upstream multipackage VPCD attempt was manually interrupted while still
progressing through the transaction-fixture cleanup. A 15-second socket bound
was too short (it timed out at LOAD block 42). Rebuilt PC emulator plus
`PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage --timeout 60` then
passed `TEST:SUMMARY pass=47 fail=0`, including restart recovery, GET STATUS,
DELETE, and transient lifecycle across an AID switch. Logs and hashes are in
`.agent/QKB.md`. This was PC VPCD, not physical hardware validation. No source
change or CAP modification was made.

Next: proceed with CTX-03 package-scoped context design/implementation while
preserving per-instance owner and lifecycle semantics. Keep the owner's
uncommitted source/doc changes untouched. Physical inventory currently has two
runtime readers and no BOOTSEL device.

## Latest physical pool check — 2026-11-02 07:59 UTC

The user reports three boards in BOOTSEL. The authorized helper and `lsusb`
instead exposed two runtime CCID devices; `pcsc_scan -r` identified
`8E464F4E2BBC0A19` and `2BED26ED20556535`. Both finite serial monitors passed
`TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. The read-only
`tools/test-pico-powerup <exact-reader>` check passed for each: SELECT returned
`9000`, GETVERSION returned `1.8.2_jc305`, and JCSystem info returned `9000`.
Logs and hashes are recorded in `.agent/QKB.md`. Installed package registries
differ between the cards, but these tested outputs matched. No BOOTSEL device
was visible, and no flash, install, reset, or power-cycle was performed.

ARM and RISC-V wrappers both passed on the current shared tree. Current image
artifacts: ARM full CHOUPI UF2 1,854,464 bytes, SHA-256
`7a7fea7b44ff8ebbd11f0439f0870ee95c51d59d8e31116c388150197e5c481c`; RISC-V
smoke UF2 79,872 bytes, SHA-256
`7b46029078921f8f2fdd3cf35d07d21cbb3bf1f6984a08921e751b257a77a925`. These
images were built, not flashed. Continue CTX-01/03 offline; preserve unrelated
uncommitted source and user files.

## SIO parameter/access-rule clarification — 2026-11-02 07:53 UTC

Rechecked Java Card RE 3.0.5 §6.2.4.3 against its specific array access rules.
§6.2.4.3 says an SIO method can receive/access call parameters, while
§6.2.8.2 allows array access only when the array belongs to an applet in the
currently active context or is designated global. A private client array must
remain inaccessible in the server context; accepting a reference as an
argument must not copy its compact heap-local handle. The current call-boundary
rejection is memory-safe but may be stricter than the parameter contract.
CTX-01 now tracks an owner-tagged opaque argument and a fixture that attempts
server-side access and expects `SecurityException`, plus collision, return,
storage, GC and deletion checks. No source, test, or CAP changed.

The user's latest report says all three boards are in BOOTSEL. The authorized
USB refresh at 07:52 UTC instead granted two runtime readers at
`/dev/bus/usb/001/003` and `/dev/bus/usb/001/004` (`2e8a:0009`); no BOOTSEL
device was enumerated. Hardware remains untouched. Refresh the helper again
before any hardware action; continue offline in the meantime.

## Latest USB inventory — 2026-11-02 07:48 UTC

The authorized helper granted access to two USB nodes. `lsusb -d 2e8a:` shows
both as runtime `2e8a:0009 Pico2 Pico2 JCVM CCID` devices at bus 001 devices
003 and 004; no BOOTSEL device is visible. This supersedes the earlier
one-reader inventory, but does not confirm the third reported token. No flash
or PC/SC applet test was attempted. Re-run the helper and identify devices
before any hardware test; keep both visible readers available for failover.

## Latest host MPU correction — privileged region-update fault

Resumed the host MPU regression after PC SIO boot failed at RAM offset
`0x17e98`. Opt-in diagnostic `CHOUPI_LOG_HOST_MPU_PERMISSIONS=1` showed the
fault occurred with `privileged=true` while the MPU region registry was busy;
the old permission path attempted a nonblocking registry lock first and
incorrectly returned `PROT_NONE`. Both `uniform_page_protection()` and
`allows_addr()` now return RWX immediately for privileged accesses, before
consulting the unprivileged region registry. Unprivileged accesses continue to
fail closed if that registry is busy. Added a regression that holds the region
lock while forcing a privileged RAM page fault.

Validation: focused host MPU tests pass 11/11 (ptrace tests require the
elevated host execution path); `./tools/build-pc --profile current` passes;
the SIO cross-AID VPCD integration passes 15/15. Latest log:
`artifacts/logs/sio-cross-aid-vpcd-20261102T073510Z.log`. No hardware operation
was performed and no speedup was measured. The full ARM/RISC-V wrappers were
not rerun because this change is confined to the host emulator. The source is
committed in `choupi/os` as `041882e`; root progress, TODO, QKB, and handoff
tracking are ready for the separate root commit. Preserve all unrelated root
worktree changes.

## CTX-03 architecture prerequisite — package context versus applet owner

Read-only review found the runtime currently allocates/restores a separate
`Context` and `Heap` per installed applet AID: `findAppletContext()` matches
owner ID plus full instance AID, and `choupi_vm_find_or_restore_applet_context()`
constructs a new Context on a miss. Java Card RE 3.0.5 §§6.1.2–6.1.3 define one
firewall context per CAP file, shared by its applets, while object ownership
remains per applet instance. This conflation affects same-CAP object access,
static state, SIO routing and deletion. The finding is recorded as CTX-03 in
TODO/QKB. Design package-scoped heaps/contexts plus separate per-instance AID,
lifecycle, root and owner state before broadening foreign-reference transport.
No code, build, or physical test was performed in this review.

## Host emulator expected-fault logging

The host MPU emulator now suppresses warning formatting for SIGSEGVs whose
fault address lies within emulated RAM; these are expected page-protection
traps. Set `CHOUPI_LOG_HOST_MPU_FAULTS=1` to restore their detailed trace.
Addresses outside emulated RAM continue to be warned. PC build passed, the
focused transaction probe case 41 passed, and the cross-AID SIO VPCD regression
passed 15/15. The focused log shrank from 37,441 bytes / 322 warnings to 591
bytes / zero warnings. Runtime changed from 85.593 s to 83.362 s across two
single samples, so ptrace stepping remains the likely performance bottleneck;
no CPU profile was collected. SIO log:
`artifacts/logs/sio-cross-aid-vpcd-20261102T043717Z.log` (SHA-256
`a152f71bb608fc1af8ee132f71bda30a3685e8f67fa4174bdd39d28c7cae6a14`).

Follow-up `/usr/bin/time -v` sample for case 41 passed in 71.198 s test-reported
time / 87.56 s outer wall, with 26.84 s user, 59.61 s system, and 98% CPU. This
is about one logical CPU average for the test command and suggests syscall or
ptrace overhead; it does not explain an eight-core VM saturation report. The
VM restricts `perf stat` (`perf_event_paranoid=4`), so no kernel setting was
changed. Next capture per-process CPU during a real saturation reproduction
and instrument ptrace/fault counts before changing MPU behavior.

Latest authorized USB inventory still sees one running CCID and no BOOTSEL
devices, despite the user's report of three boards in BOOTSEL. Continue offline;
do not flash until BOOTSEL is visible and the required builds pass. The source
change is in the `choupi/os` submodule and requires its own commit before the
root progress/handoff commit. See `perf_tips.md` for measurement details.

## SIO reference argument boundary clarified

The user reports three physical Pico 2 boards in BOOTSEL, but the latest
authorized host refresh enumerates only one running CCID (`2e8a:0009` at
`/dev/bus/usb/001/055`); no BOOTSEL board is visible to this VM, so no flash
was attempted. Continue offline until passthrough exposes a BOOTSEL device.

Java Card RE 3.0.5 §6.2.4.3 allows SIO methods to receive parameters, while
§§6.1.4 and 6.2.8 still firewall accesses to client-private objects. Current
private-reference rejection is safe: applet heaps independently allocate
compact `jref_t` values, so raw cross-heap copying could resolve to a different
server object. A future implementation needs an explicit invocation-scoped
foreign-reference representation retaining source context and original
handle, with all unauthorized accesses denied. Do not weaken marshalling to
copy raw references. Details recorded in `.agent/QKB.md` and `TODO.md`.

The current worktree, including the other session's uncommitted changes, was
revalidated: `tools/test-rmi-registry` passes; ARM and RISC-V wrapper builds
pass; the SIO VPCD suite passes 15/15 with Descriptor omitted and 15/15 with
Descriptor included. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261102T040952Z.log` (SHA-256
`4f12fee93e4d7eb412620e0c0867f100ea2cfc802d267c2065ffc5d3c52d99f0`) and
`artifacts/logs/sio-cross-aid-vpcd-20261102T041201Z.log` (SHA-256
`013b6f75fbe8cbd24b627089f96ee78b9ab0a7557348df29a1bfe41b321f3332`). The
ARM UF2 remains 1,854,464 bytes (SHA-256
`7a7fea7b44ff8ebbd11f0439f0870ee95c51d59d8e31116c388150197e5c481c`); the
RISC-V smoke UF2 remains 79,872 bytes (SHA-256
`7b46029078921f8f2fdd3cf35d07d21cbb3bf1f6984a08921e751b257a77a925`).
The restricted VPCD run initially failed to bind its loopback listener with
`EPERM`; the same local-only test passed through the elevated execution path.
The follow-up remains foreign-reference design and collision/access
regressions. Preserve all unrelated worktree changes.

### Additional global Object[] firewall gap

Review of the same Java Card RE 3.0.5 spec found that §6.2.8.2 permits
ordinary applet-owned references to be stored in global Object[] arrays; only
temporary JCRE entry-point references and global-array references are excluded
from `aastore`. The current `JC_Array::setReferenceEntry` rejects every
non-null value when the destination array is in the global heap, and the
transaction probe case 41 expects that behavior. This is stricter than the
specification. Do not simply remove the rejection: another applet's heap may
have an unrelated object at the same compact handle. Correct support needs
owner-preserving reference indirection plus GC, deletion, persistence, reset,
and collision coverage. See `.agent/QKB.md` and CTX-01 in `TODO.md`.

Focused reproduction: `./tools/test-jcsystem-transaction-applet --case 41`
passes the current test in 85.593 s because the applet catches
`SecurityException` when writing itself to the global Object[]; APDU result is
`019000`. Log: `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261102T042104Z.log`
(SHA-256 `8cf6cb16df35f096ee1fa3c3befcc3a99adb48ef5e13e307aab1c1f2f114e1e4`).
This is a reproducible spec-compatibility failure, not a passing acceptance
result for global object references.

## Latest CTX-01 update: proxy markers survive persistent alias restore

`getPersistentField_ReferenceImpl()` rebuilt persistent object aliases without
copying the canonical object's internal Shareable-proxy marker. An aliased stale
proxy could therefore lose fail-closed recognition after process restart. The
alias restore path now reapplies the marker whether it resolves an already
loaded target or constructs the target from its canonical tag. Java-visible
field type remains ordinary object.

The test-only SIO client stores the same proxy in two persistent fields and
accesses the alias first after restart. It now catches the expected
`SecurityException`; the full cross-AID VPCD fixture passes 15/15 at
`artifacts/logs/sio-cross-aid-vpcd-20261102T032628Z.log` (SHA-256
`d6925d8315d57309e4f63673f5b8f8ff1fd106c28494ed7c411a1508b4532d49`).
`tools/test-rmi-registry`, PC, ARM and RISC-V wrapper builds pass. Latest ARM
UF2 is 1,853,440 bytes, SHA-256
`de5f11ad3059ac25903580132a8e41619204c0c3b7428963628a8611793bb0be`; RISC-V
smoke UF2 is 79,872 bytes, SHA-256
`7b46029078921f8f2fdd3cf35d07d21cbb3bf1f6984a08921e751b257a77a925`.

The proxy persistence policy remains fail-closed after VM process restart;
routing metadata is not rehydrated. This is now tested for both direct and
aliased persistent references. Physical positive cross-AID acceptance remains
open. Although the user reports three BOOTSEL boards, the latest authorized
helper plus elevated USB enumeration still expose only one runtime CCID
(`2e8a:0009`, `/dev/bus/usb/001/055`) and no BOOTSEL device. No flash was
attempted. Retry the physical fixture once BOOTSEL is guest-visible, then
continue with reference ownership/firewall cases and unchanged upstream Multi
CAP coverage. Preserve all unrelated worktree changes.

## Latest CTX-01 runtime-tag check: typed pops enforce value kinds

The static stack audit found that typed operand pops could bypass the runtime
kind tags: malformed bytecode could consume a reference through `pop_Short()`
and repush the raw word as a primitive. Typed byte, short, int, reference and
return-address pops now check the kind before consuming it. Int pops require a
low/high pair and read the words in explicit sequence. A mismatch throws
`SecurityException` without consuming the value; underflow is checked before
tag access. Untyped `pop` remains valid for `pop`/`pop2` bytecodes.

`tools/test-rmi-registry` passes valid and invalid typed-pop cases. PC current,
ARM and RISC-V wrapper builds pass. The focused cross-AID VPCD fixture passes
15/15 (`artifacts/logs/sio-cross-aid-vpcd-20261102T034712Z.log`, SHA-256
`624cc196eb4417cf1e6948bc28e0d61aacdc231a5d396b9f3077ccd9e20bb786`); the
multipackage lifecycle suite passes 47/47
(`artifacts/logs/host-vpcd-multipackage-apdu-20261102T035136Z.log`, SHA-256
`54a6fa69c91cc49c50db2abcfb2d0196d115018df3767224d678e1778f5f95d1`). Latest
ARM UF2 is 1,854,464 bytes, SHA-256
`7a7fea7b44ff8ebbd11f0439f0870ee95c51d59d8e31116c388150197e5c481c`; RISC-V
smoke UF2 is 79,872 bytes, SHA-256
`7b46029078921f8f2fdd3cf35d07d21cbb3bf1f6984a08921e751b257a77a925`.

The final authorized reader/USB refresh still shows one runtime CCID
(`2e8a:0009`, `/dev/bus/usb/001/055`) and no BOOTSEL boards despite the user's
report of three connected BOOTSEL devices. No flash was attempted. Continue
offline until the boards enumerate, then run physical cross-AID and stack-kind
validation on an identified card.

## Current CTX-01 state: descriptor-free typed arguments pass VPCD

The physical trace from a fresh two-package probe shows both packages installed
and the client selected, but `invokeinterface` returned `6F00` because the
interface package's runtime CAP had no Descriptor component. The generated CAP
archive did contain `Descriptor.cap`. GPPro's omission is confirmed in the
pinned local source: `GPSession.loadCapFile()` sends `cap.getCode()`, and the
local `capfile-26.02.22` implementation's `CAPFile.getCode()` delegates to
`_getCode(false)`, which explicitly excludes Descriptor and Debug. The physical
APDU stream was not separately captured. Trace and probe logs are
`artifacts/logs/sio-two-package-cdc-20261102T013345Z.log` and
`artifacts/logs/sio-two-package-probe-20261102T013345Z.log`.

The operand stack now carries compact three-bit word-kind tags (unknown,
primitive, reference, int-low, int-high, return-address) maintained by typed
pushes, local writes, frame-local initialization, `dup`, `swap`, and stack
element replacement. When Descriptor is absent, cross-context
`invokeinterface` uses these tags to construct server locals. Int pairs must
have low/high order; null and JCRE-global references may cross; unknown words
and applet-private references fail closed. Typed local reads now enforce the
stored kind and raise `SecurityException` if server bytecode uses an argument
as a different kind. `astore` preserves reference versus return-address so
`ret` can validate its local. Storage is 96 bytes per 256-word stack. Present
Descriptor signatures continue through the checked signature path; malformed
present data does not fall back to tags.

Validation: `tools/test-rmi-registry` passes. The three-package VPCD suite
passes 14/14 with Descriptor omitted and 14/14 with Descriptor present,
including a negative int-word-order regression. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261102T024802Z.log` and
`artifacts/logs/sio-cross-aid-vpcd-20261102T024959Z.log`. PC, ARM and RISC-V
wrapper builds pass. With local-kind checks, the SIO fixture passes 14/14 in
both Descriptor modes; the multipackage suite passes 47/47 and transaction
case 48 passes 3/3. Latest ARM image is 1,853,440 bytes, end `0x100e2224`,
SHA-256 `29d2b2dbc5149a47550194a62cb85acf904b7e595e57167dd79e45f3ed6f939f`;
RISC-V smoke image is 79,872 bytes, end `0x10009a78`, SHA-256
`7b46029078921f8f2fdd3cf35d07d21cbb3bf1f6984a08921e751b257a77a925`.

The user reports three boards in BOOTSEL, but the latest authorized helper and
USB enumeration still see one runtime CCID (`2e8a:0009`) and no BOOTSEL boards.
No flash or physical SIO validation was performed. The latest check was
`PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader` plus
`lsusb -d 2e8a:`; it returned one reader at `/dev/bus/usb/001/055`,
`2e8a:0009`. Refresh USB visibility before hardware work. Next, exercise
descriptor-free typed calls on a physical card when BOOTSEL becomes
guest-visible; continue checking stack-tag coverage and cross-context
ownership/firewall cases. Preserve unrelated uncommitted changes.

## 2026-11-02 persistent-index exhaustion diagnosis and reclaim-on-write

The original physical failure was confirmed: GPPro INSTALL returned stage `09`
and `6F00`, while CDC reported
`FS_WRITE_FAIL op=new tag_len=4 size=2 files=8192/8192 free_slots=0` for
lifecycle tag `02FF3D01`. The initial on-demand compaction change did not help
because the card had legacy parent-slot-encoded records and the old compactor
returned early when it found any. Compaction now retains ancestors required by
kept parent-tag records and remaps their parent slot references during rehash.
On-demand compaction retries the requested write and re-resolves its tag after
rehash; a fully live set still fails safely.

`./tools/test-rp2350-fs-overlay` passes with a full 8,192-slot fake filesystem
containing a parent/child tag pair, tombstone reclamation, remapped references,
and snapshot restart. ARM and RISC-V current builds pass. ARM firmware was
flashed to identified Pico 2 `2BED26ED20556535`; CDC-1200 recovered BOOTSEL,
flash verification passed, and the finite monitor reported
`TEST:SUMMARY pass=1 fail=0`. GPPro client INSTALL then succeeded; serial
reported `FS_COMPACT files=8192->696 bytes=82578->16749`. GPPro INSTALL of the
already-loaded service applet also returned `9000`.

The follow-up physical SIO APDU selected the client but returned `6F00`; serial
identified `SecurityException` at the client `invokeinterface`. The existing
package AIDs were loaded before this test, so their CAP code identity is not
verified against the newly generated fixture. Do not treat this as proof of a
current-source SIO defect or delete/replace the existing packages. Next build a
fixture with distinct package AIDs, load/install it package-scoped, and rerun
the physical probe. The VM presently exposes one runtime CCID reader; the other
reported BOOTSEL boards are not guest-visible. No flash erase was performed.

## Latest CTX-01 restart-safety update

Persisted Shareable proxy placeholders now carry an internal marker in their
persistent instance header. Restore reinstates the marker, while Java Card
field-type queries still expose an ordinary object. A marked placeholder whose
runtime routing entry is gone fails closed with `SecurityException` across
interface/virtual/special invocation, casts, and instance-field bytecodes. This
is a safety boundary, not proxy rehydration: using a saved proxy after emulator
process restart remains unsupported. The marker-only VPCD regression passed
14/14 (`artifacts/logs/sio-cross-aid-vpcd-20261101T233118Z.log`, SHA-256
`bde444b99091b7694caf8802b49c59468bf4bc4313e3897384a8bc25ed87e0a7`), and
`./tools/test-rmi-registry` passed. Generic persistence regression also passed
install/commit 3/3 and fresh-process restore 38/38, including four GC-pressure
rounds. PC, ARM and RISC-V current-profile builds pass; ARM image end is
`0x100e10ac`, RISC-V smoke image end is `0x10009a78`. Physical cycle on
`2BED26ED20556535` succeeded after CDC-1200 BOOTSEL recovery, and serial health
passed. The positive physical SIO probe is still blocked: pre-existing
contract/service packages caused the clean fixture guard to refuse replacement;
GPPro `--install` returned `6A80`, though GET STATUS listed the client package.
`--install-only --debug` returned stage `09`, VM status `0000`, and `6F00`; its
SELECT returned `6A82`. A read-only SELECT of the existing service AID
`A00000015100000301` also returns `6A82`; GET STATUS APP rows in this state
describe CAP applet descriptors, not installed instances. Deleting the
test-created loaded package returned
`6581`; do not retry cleanup or alter existing packages without a specific
safe path. Fresh `tools/test-rmi-external-link-vpcd` passes a valid exported
interface INSTALL and rejects an exported class and missing export (`3/3`), so
the physical stage-9 failure may be caused by stale dependency CAP content;
this is unconfirmed. Logs and hashes are in the top QKB entry. The authorized
PC/SC helper now sees one runtime CCID at `/dev/bus/usb/001/041`; no other
boards are visible in this VM. Preserve all unrelated worktree changes.

## Current CTX-01 Shareable status

Proxy `invokeinterface` supports typed cross-context primitive arguments and
returns. Null and JCRE-global array/instance references may cross; an array
parameter requires an array handle, while arrays may also satisfy `Object`.
Applet-local object/array arguments remain rejected because client-heap
ownership translation and firewall behavior are not implemented. Server-local
Shareable reference results are rooted in the server Context and returned as
client proxies. Same-context calls preserve the caller's complete operand
stack. Proxy casts validate both the requested Shareable interface and the
server class implementation.

The test-only three-package fixture now passes the cross-AID VPCD path end to
end: its client sends byte/short arguments, a JCRE-global AID reference and a
null array; the service checks `getPreviousContextAID()` and its own AID and
returns `0x4567`. The same fixture also verifies that a denied lookup returns
null, applet-local array arguments fail closed, and a 32-bit int argument and
return round-trip as `0x12345678` → `0x76543210`. A server-thrown
`SecurityException` is caught in the client context, and a subsequent SIO call
passes. A proxy saved in the client applet instance survives the VM's
end-of-APDU GC and works on the following APDU. This validates exception
unwinding for that case only; identity/status semantics for other server
exceptions remain open. The fixture also confirms direct `Object.equals` on a
proxy is rejected. Fresh lookup after PC emulator process restart still works,
but a proxy reference serialized in the client applet loses its native routing
metadata. The VM now rejects that unregistered foreign dynamic object for
interface/virtual/special calls, casts and field access rather than dispatching
it in the client context. VPCD confirms fail-closed behavior after process
restart and card reset; persistent proxy metadata rehydration remains open.
The unchanged upstream `AlgTestApiCore_305.cap` physical result below covers
only the same-applet route. On 2026-11-02 the full-index condition was fixed on
`2BED26ED20556535`; client and service applet INSTALLs returned `9000`. The
generated cross-AID probe selected the client but its first SIO call returned
`6F00` with `SecurityException` at client `invokeinterface`. Existing package
CAP identities are unknown, so the positive physical SIO result is still
unverified. Do not replace/delete the existing package AIDs. Use a fresh set of
distinct package AIDs for a clean physical probe.

The focused `tools/test-rmi-registry` regression and current-profile PC, ARM,
and RISC-V builds pass. The expanded VPCD fixture passes 14/14, including
stale-proxy rejection after emulator restart and card reset. Latest log:
`artifacts/logs/sio-cross-aid-vpcd-20261101T224601Z.log` (SHA-256
`ce85e4376191dab6f4d7a2e5b45cfd542fe97bf5bf71bbd2256998eb2ba2863f`). Never
alter upstream CAPs. The physical test script now
checks GET STATUS after loading the contract so it stops if GPPro reports LOAD
success without making the dependency resident. See the current QKB entry for
the unresolved physical install exception and existing partial test package.
Earlier same-applet physical result: full ARM CHOUPI firmware on
`8E464F4E2BBC0A19` passed finite serial output; unchanged
`AlgTestApiCore_305.cap` completed 291 Core calls and cleaned up. Its positive
Shareable sub-interface row returned `1/OK`.

Logs: `artifacts/logs/cycle-choupi-20261101T201412Z.log` (SHA-256
`b24d090271acf05a2b2cfa020394d116f25c4d68fb4efdb01d35f45b1252d4fe`) and
`artifacts/results/shareable-physical-core-8E464F4E2BBC0A19-20261101.csv`
(SHA-256 `6045b2d17131fcc7739e437c19bcab153f2164fa76dd8ffa15f7f89414880713`).

Next: build the SIO fixture with distinct package AIDs, load/install it on this
card or another identified clean card, and run the cross-AID APDU. Define whether
persistent Shareable references must be rehydrated after process restart or
must fail closed; if rehydration is required, persist proxy metadata by a stable
identity. The authorized reader helper most recently reported one runtime
reader (`/dev/bus/usb/001/047`, `2e8a:0009`); no additional BOOTSEL devices are
guest-visible. Keep CTX-01 open; VPCD success is not physical acceptance.

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

## JCRE-global references in cross-context SIO arguments — 2026-11-01

The cross-context argument marshaller now accepts null, JCRE-global array, and
JCRE-global instance references. These handles resolve via the shared global
heap and do not expose applet-private heap objects. Applet-local non-null
references (including proxy placeholders) still fail closed pending explicit
ownership translation. Non-null global references must match the method
descriptor category: array parameters require array handles, while arrays may
also satisfy `Object` parameters. `tools/test-rmi-registry` covers the accepted
global forms, array-parameter mismatch, and continued local-reference
rejection; it does not exercise descriptor lookup or cross-AID execution.

Validation: `tools/test-rmi-registry`, PC build, ARM build, and RISC-V build
passed. The user reports physical devices connected, but the VM's elevated
`lsusb -d 2e8a:` returned no devices and PC/SC lists only VPCD; no flash or
physical test was performed. Continue CTX-01 offline until an unchanged
upstream CAP exercises cross-AID invocation or a valid fixture is available.
