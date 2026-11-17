# QKB incident entries archived on 2026-11-17

Verbatim sections moved out of `.agent/QKB.md` (original QKB line numbers in the migration table). Dated incident narratives whose fixes or conclusions are recorded in code, `TODO.md` or `progress.md`. Entries for still-open problems and the design/viability rules stay in QKB.md.

## BOOTSEL reset integration audit — 2026-11-12 (offline)

Confirmed firmware integration gap, not a proven new regression: `src/ccid_device.cpp`
configuration has three interfaces (CDC control/data and CCID), no
`TUD_RPI_RESET_DESCRIPTOR`, and its application-driver callback returns only CCID.
This descriptor layout dates to `dc76f431` (2026-10-11). `CMakeLists.txt`
removes the SDK descriptor source. SDK `pico/usb_reset.h` explicitly requires
adding the reset descriptor and registering `usb_reset_interface_driver` when
using custom TinyUSB. The plan promises this interface but implementation lacks it.

CDC fallback added in `971887e4` (2026-11-08) sets `usb_bootsel_requested`
in the line-coding ACK callback; only the outer loop in `src/choupi_pico2.cpp`
consumes it. Synchronous `choupi_ccid_task()` work can service USB via native
progress while feeding the watchdog without checking this flag. Thus a received
reset can remain pending until the APDU returns; 20-second recovery is not
guaranteed. An unserviced USB loop also prevents receiving the request.
Neither mechanism proves the exact D92 failure without existing run-specific
telemetry. VM passthrough and host CDC request delivery remain alternatives.

Implementation update — 2026-11-12: added the SDK vendor reset descriptor and
class driver as interface 3 while retaining CDC interfaces 0/1 and CCID
interface 2. CHOUPI's CDC-1200 callback now queues a request; main-loop USB
service and interpreter cooperative progress consume it after TinyUSB returns.
The SDK CDC reset callback is disabled to avoid a duplicate callback and direct
reset from inside TinyUSB. Offline evidence: reset/CCID unit tests 26/26,
identity-pinned flash-target-selection tests 10/10, and ARM/RISC-V wrapper
builds pass. The ARM image ends at `0x100d91b0`, leaving 159,312 bytes.

Physical limitation remains: after the user reported three devices in BOOTSEL,
elevated guest `lsusb -d 2e8a:` returned three runtime `2e8a:0009` devices and
no BOOTSEL `000f`. No reset or flash was attempted. Hardware confirmation of
vendor reset, CDC-1200 reset during idle and a long synchronous workload, and
request-to-re-enumeration latency remains open; no latency metric is measured.

Offline recheck on 2026-11-13: `./tools/test-ccid-progress-flow` passes
30/30, including deferred callback reset servicing during synchronous native
work; `./tools/test-flash-target-selection` passes 11/11, including chip-ID
pinned rediscovery after BOOTSEL re-enumerates at a new USB topology path.
Descriptor inspection confirms config interface count 4, CDC interfaces 0/1,
CCID interface 2, and SDK vendor reset interface 3, with the MS OS 2.0 function
subset also pointing to interface 3. These are offline checks only and do not
close the physical reset/re-enumeration acceptance gap.

## JCAlgTest option 2 PC/SC failures on 8E and D92 — 2026-11-12

**Symptom:** The unchanged JCAlgTest 1.8.4 `ALG_PERFORMANCE_STATIC` run on 8E
(`8E464F4E2BBC0A19`) completed 26 measurements, then failed at DES ECB NOPAD
`Cipher_init()` with `SCARD_F_COMM_ERROR`. Earlier in the same turn, the
authorized USB/PCSC helper was run while this live campaign was active. The
helper restarts `pcscd`; this is a confirmed way to tear down a live
SmartcardIO connection and is the likely cause of this first failure.

A failover attempt on D92 (`D92E743D5F640C0F`) completed 32 measurements before
DES ECB PKCS5 `Cipher_init()` returned `SCARD_E_NOT_TRANSACTED`. The client
skipped that case, then saw `SCARD_E_READER_UNAVAILABLE` twice on DES3 2-key CBC
NOPAD `Cipher_doFinal()`. No reader-helper refresh was run during this D92
attempt. After the client stopped at its retry prompt, `pcsc_scan -r` still
listed D92. D92 serial telemetry showed successful `9000` APDU responses and
`TEST:SUMMARY pass=1 fail=0`; neither serial log contained `FAULT`, `6F00`, or
an `OBJECT_DELETION` event. This does not establish a firmware fault or a
completed GC test. The campaign is incomplete.

**Viable response:** Never restart the PC/SC helper while any SmartcardIO
campaign is active. Preserve its logs and, after the active client is stopped,
check `pcsc_scan -r` before using another identified reader. If PC/SC remains
available after a transaction error, record that separately from board loss.
The exact cause of the D92 errors remains unknown. The user reported all three
boards in BOOTSEL, but the last guest `lsusb -d 2e8a:` enumeration showed three
runtime `2e8a:0009` devices and no BOOTSEL `000f`; PC/SC listed only D92. This
host/device-mode discrepancy must be resolved before flashing.

Evidence: `artifacts/results/jcalgtest-1.8.4-resource01-20261112/8E464F4E2BBC0A19/option2-gc-nodebug/`
and `artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option2-gc-nodebug/`.

Follow-up, 2026-11-12: On 4CDA (`4CDA5BC1B136E46A`), a watchdog-bounded
option-2 repro stalled for 45 seconds on SHA-384 `MessageDigest_update()`
measurement APDU `B041000016`. The VM logged its `9000` APDU response, but CCID
telemetry remained at stage 6 (response submission) rather than stage 7
(successful IN completion); the SmartcardIO client emitted no response before
the watchdog terminated it. This points to the CCID response-delivery boundary,
not specifically RSA-3072 or persistent-storage compaction. Afterward the
physical readers disappeared from `pcsc_scan -r`; the helper still granted
access to three USB nodes. D92 and 2BED were then identity-pinned through
`tools/cycle choupi`; both ARM/RISC-V builds, firmware flashes and finite
runtime checks passed, but `pcsc_scan` still listed only Virtual PCD readers.
An unprivileged PC/SC service restart was denied. A human service restart is
requested in `.agent/EXT_REQUESTS.md`. This is a reproduction of missing host
delivery after a successful VM result, not a confirmed cause or a complete
option-2 run.

The prior D92 storage failure is separate evidence: at `files=2134`,
`bytes=98292/98304`, compaction retained a record beginning at offset `91502`
while its output prefix had reached `91510`, then returned `FS_STATUS_IO_UNKNOWN`
and the applet returned `F205`. Diagnostic logging now also records the
previous retained record's fields, but that richer record has not yet been
captured during a fresh compaction failure. No RSA array change is justified.

Offline root cause confirmed, 2026-11-12 22:49 UTC: compaction's in-place
open-addressing rehash walked mapping paths from the lowest slot, which can
start in the middle of a path. That leaves a stale duplicate in a slot later
overwritten by its predecessor and loses a live record. A deterministic
96 KiB stress case with 2,400 64-byte compressed tags and 4-byte values
reproduced the loss at its first compaction (`id=856`, then
`FS_STATUS_NO_SUCH_TAG`). The fix moves path components from zero-incoming-edge
heads before processing remaining cycles. The new test passes 32 delete/replace
cycles, validates every live value after each compaction, and verifies snapshot
restore. This explains the class of corruption behind the earlier overlap
rejection, though the exact old D92 offset pair has not been reproduced after
the fix. ARM and RISC-V `max_crypto` builds pass. A fixed-image hardware cycle
is still pending: the approved elevated helper call failed with the
`no new privileges` restriction; see `.agent/EXT_REQUESTS.md`.

## 2026-11-12 External no-debug run: reset failures precede RSA-3072 disconnect

Investigation ID: RESOURCE-01-EXTERNAL-RESET-PREFIX-20261112.
User-reported evidence for `pico2-jcvm-max_crypto-object-deletion-no-debug-empty-card-580f3f40.uf2`; full new external log and serial telemetry are not available locally.

The first reported failure is AES-128 ECB ISO9797 M1 `Cipher_doFinal()` preparation, not RSA:
`B0 31 00 00 16 00 11 00 19 FF FF 00 0F 00 80 00 07 01 00 FF FF 00 02 00 32 00 01 00`
returned `6F00` in 1482 ms. The client then printed “Free unused card objects...”, “Reset applet didn't work, speed of algorithm couldn't be mesured”, and `CANT_BE_MEASURED`. User reports many repetitions before the final RSA-private-3072 PKCS1 preparation transport failure, and reset calls after every command to be measured.

The supplied client excerpt first sends the preparation APDU; on non-9000 it invokes `resetApplet(cla, resetIns)`. Only a true reset result permits retry of the preparation APDU. A false result prints the reset-failure message and raises CANT_BE_MEASURED. Thus this message identifies a failed recovery attempt, not proof of a slow RSA measurement. The reported applet INS_CARD_RESET branch invokes `JCSystem.requestObjectDeletion()`.

Important attribution limits: the shown `6F00` belongs to B0 31 preparation. No reset-command response/status or exception is supplied, so an exception specifically inside requestObjectDeletion is a hypothesis. In current source, the enabled native request merely sets a noexcept boolean and records its context; actual collection runs after APDU execution in `choupi/src/main.cpp`. With deletion disabled, the native API explicitly throws SystemException. Verify the tested binary configuration and distinguish request entry, post-APDU collection, reset response, and next prepare response. Retained deletion requests may be serviced again; repeated calls must be tested with live roots and failing preparation paths.

Next: locate the first failing reset in the complete trace, retain its exact APDU/status/elapsed time and preceding successful operation, correlate serial exception and OBJECT_DELETION markers, and reproduce an unchanged upstream 1.8.4 AES prepare/reset sequence repeatedly before attempting the RSA prefix. Measure USB-progress gaps across post-APDU collection as well as native RSA. RSA time-extension starvation remains possible but cannot explain away the earlier AES/reset failures. No robust fix established; no hardware accessed.

Physical follow-up, 2026-11-12 19:11 UTC: reader access returned for 8E
(`8E464F4E2BBC0A19`); two other runtime USB Picos were not published by PC/SC.
On 8E, SELECT JCAlgTest returned `9000` in 224 ms, then the exact client
`INS_CARD_RESET` (`B0E2000000`) returned `9000` in 1,560 ms, and the reported
RSA-3072 private PKCS1 `Cipher_doFinal()` preparation APDU (`B031...`) returned
`AA9000` in 1,758 ms. The full sequence passed; a subsequent bounded GPPro
`--list` showed ISD, JCAlgTest applet and package. A saved finite serial monitor
reported `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`.

This is evidence against a deterministic 3072-bit array-capacity defect: the
native key builder computes `(bits + 7) / 8` in `uint16_t`, yielding a valid
384-byte modulus/private-component capacity and 192-byte half-size components
for 3072-bit RSA; the exact client preparation succeeded on-card. It does not
exclude a state-dependent memory/GC bug or explain the external PC/SC loss.
The physical image has heap-GC diagnostics enabled and is not the external
no-debug image, so timing is diagnostic only. Next reproduce the preceding
failed AES prepare/reset and run repeated exact reset/prepare cycles or the
unchanged 1.8.4 client while correlating CCID response progress. No source fix
is established by this one successful sequence.

Repeated physical follow-up, 2026-11-12 19:22 UTC: 8E's diagnostic firmware
(`PICO2_DEBUG_HEAP_GC=1`) produced large per-record traces during reset. One
bounded repetition took 11.2 s for SELECT and 14.5 s for reset, then the host
probe timed out before a `B031` command appeared in serial telemetry. This is
inconclusive: instrumentation and output volume differ from the user's
no-debug firmware, and the host timeout expired while waiting for the next
APDU. Using D92 with no-debug firmware removed that confound. Four consecutive
reset/prepare cycles all passed: resets `9000` in 1.468–1.522 s; RSA-3072
prepare `AA9000` in 1.617–1.691 s. The exact preparation remained stable after
repeated deletion requests. This further argues against insufficient
3072-bit component arrays, but does not reproduce the earlier AES 6F00 or
Windows PC/SC disconnect. Continue with the unchanged upstream client on a
no-debug device and capture the first AES prepare/reset failure and transport
status. Logs: `artifacts/results/rsa3072-physical-20261112/8E-heap-debug/` and
`artifacts/results/rsa3072-physical-20261112/D92-no-debug/`.

AES follow-up, 2026-11-12 19:29 UTC: on no-debug D92, the exact reported
AES-128 ECB ISO9797 M1 `B031` preparation, preceded by `B0E2`, returned
`AA9000` once and then across four consecutive reset/prepare cycles. Repeated
reset responses were `9000` in 1.477–1.530 s; AES preparations returned
`AA9000` in 1.713–1.828 s. Serial remained healthy. This does not reproduce
the external `6F00` even for its exact AES config, and reduces the likelihood
that a basic AES or 3072-bit allocation failure is deterministic. The current
probe omits the measurement `B043` APDU and broad preceding suite history; do
not claim full performance acceptance. Next diagnostic should use the
unchanged 1.8.4 client over a longer no-debug campaign, retain the first
failure's APDU/reset result and correlate card serial and PC/SC events. Logs:
`artifacts/results/rsa3072-physical-20261112/D92-no-debug/apdu-aes-reset-prepare-20261112T1924Z.log`,
`.../apdu-aes-repeated-reset-prepare-20261112T1925Z.log` and corresponding
serial logs.

Full-client follow-up, 2026-11-12 19:49 UTC: the unchanged 1.8.4/jc305 client
ran `-op ALG_PERFORMANCE_STATIC -fresh` on D92 no-debug firmware whose CPLC
pre-personalization ID was `580F3F40`, matching the tested build. It completed
SHA, MD5, RIPEMD160, SHA-256, SHA-384 and SHA-512 tests, then failed during
`ALG_SHA_224 MessageDigest_reset()` at a `B041` APDU with
`SCARD_E_NOT_TRANSACTED`. No reader-helper restart occurred during the client.
Firmware serial state showed `rx_events=1774`, `xfr=207`, `stage=6`, last APDU
`B041000016`, and a recorded response `sw=9000`; no `CHOUPI:CCID_ERRORS`,
`FAULT`, or failed runtime summary appeared. A post-failure serial snapshot
still passed runtime health with the same stage-6 state. USB still enumerated
D92 as `2e8a:0009` and its CDC serial remained available, but PC/SC no longer
published a physical reader. After the client had exited, the approved helper
refreshed access; a line-buffered scan listed only the two Virtual PCD readers.

In `src/ccid_device.cpp`, `CCID_RESPONSE` is recorded before the final bulk-IN
transfer is known to have completed; the CCID diagnostic stage changes to 7
in the successful IN completion callback. Thus the stuck stage 6 plus client
transaction failure points to the final CCID response transfer not completing
(or its callback not being serviced), rather than to RSA buffer capacity. This
is a strong localization, not a proven exact electrical/USB cause: current
telemetry does not expose pending-response state or distinguish submission
from host ACK. Add focused response-submit/IN-completion diagnostics, then
reproduce on no-debug firmware. Full option-2 acceptance failed; it is not a
pass. Logs:
`artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option2-rsa-reset-followup/`.

## RESOURCE-01 follow-up — persistent-storage exhaustion during option 2

On physical D92 (`D92E743D5F640C0F`), the unchanged JCAlgTest 1.8.4/jc305
`ALG_PERFORMANCE_STATIC` follow-up passed the earlier SHA-224 reset point but
later failed at AES-192 setup after the persistent overlay reached 2,134
records and 98,292/98,304 bytes. The attempted persistent write returned
filesystem status 1 (`OutOfFlash`), mapped by the VM to
`SystemException.NO_RESOURCE` / `F205`; later client cases reported
`CANT_BE_MEASURED`. Object-deletion passes did run and eventually erased
applet records, but the last write-failure path produced no successful
`FS_COMPACT` telemetry. Firmware CDC/runtime health remained alive.

This confirms storage exhaustion in this run, not an undersized RSA-3072
component array. It does not establish the cause of the separate Windows
`SCARD_E_NOT_TRANSACTED` at RSA-3072. Exact RSA-3072 reset/prepare passed four
times before storage filled, and this full run failed earlier at AES-192.
Next inspect why `compact_deleted_overlay_records()` rejected compaction and
whether current applet persistent roots keep unexpectedly many algorithm
objects alive. Preserve the campaign artifacts and do not claim option-2
acceptance. Artifacts:
`artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option2-ccid-debug-followup/`.

## 2026-11-13 Windows reset campaign: distinguish transport from persistent resource failure

Evidence: `artifacts/results/windows-maxcrypto-20261112-with-reset/ANALYSIS.md`. First AES measurement transport loss precedes memory errors. After RSA-1984 reconnect F205 precedes 6F00; replug does not clear lasting failure. F205 can mean failed overlay compaction, not only true capacity exhaustion. Reported 580f3f40 image predates independently reproduced rehash fix 92c91943 and CCID recovery 49a390d5. Existing viable fix for reproduced rehash mapping loss: path-head-first rehash (unit validation previously recorded); **not proven to resolve this Windows run**. Eight-record/APDU backlog is still a hypothesis without deletion telemetry. No implementation in this step.

## 2026-11-13: CCID response retry remains unresolved; D92 compaction rejects tag reconstruction

On physical 2BED (`2BED26ED20556535`), the recovery-build option-2 campaign
stalled at xfr=1156 on `B043000016`: the VM returned `9000`,
`fs_sync()` took 1,023,217 us, and the CCID response remained pending with the
IN endpoint reported idle and no submit errors. The client watchdog expired
after 60 seconds; no `CCID_EXTENSION_RECOVER` was logged. Thus the deployed
100 ms stale-extension recovery is not validated and failed to complete this
second observed pending response. A new debug-only `CCID_RESPONSE_WAIT`
trace reports extension flag, endpoint busy state, native operation activity,
age, and retry failures; it has not yet been flashed or observed. The trace was
extended to report `CCID_RESPONSE_IN_FLIGHT` once per second after TinyUSB
accepts the final response but before its successful IN completion callback.
This distinguishes a response stuck in the endpoint from one blocked before
submission; stage 6 alone cannot distinguish these paths. Latest diagnostic
image is `build/arm/pico2_choupi.uf2`, SHA-256
`6147b04aea8abb3e90ac112347bb4fc331c54eff3de7bf10aea21955d92d55a7`; it has
not been flashed because the identified devices did not enter BOOTSEL.

### D92 tag reconstruction diagnostics

Added debug-only `FS_TAG_RECON_FAIL` logging inside the compaction tag
reconstructor. It reports the failing slot, flags, tag length, serialized
offset/header, parent slot and length, depth, root length, and precise local
failure reason; recursive failures are reported along the chain. This is meant
to distinguish stale parent indices from malformed compressed/plain record
metadata. `./tools/test-rp2350-fs-overlay` still passes, including full-index
compaction with a parent-encoded child. The new trace is build-validated only;
no physical capture identifies D92's failing record yet. Current diagnostic
image: `build/arm/pico2_choupi.uf2`, SHA-256
`898e968b8f2b03cfb6533b8c19d093451f1d957d49114daa96bda6b75b4ac7a0`.

Added `filesystem_parent_encoded_records_survive_repeated_compaction`: a
2,400-record near-capacity test using 29-byte legacy parent-encoded tags and
37-byte values, deleting/replacing 100 children twice, then snapshotting,
restarting, and verifying all live records. It passes, so this synthetic
pattern does not reproduce D92's rejection; preserve the physical failure and
wait for `FS_TAG_RECON_FAIL` output before changing compaction semantics.

Follow-up stress also tombstones all ten parent tags while 2,199 children
remain live, forces capacity compaction, then recreates the parents and runs a
second compaction. It still passes. The D92 snapshot cannot be v3 at the
reported 98,273 bytes because v3 validation limits `bytes_used` to 77,824;
that failure was reached in v4 state, so v3 migration alone is not a likely
explanation.

The separate D92 run hit `FS_COMPACT_REJECT status=131
reason=tag_reconstruction` repeatedly with 2,468 live overlay entries and
98,273/98,304 bytes. Persistent writes then returned `F205` /
`SystemException_NO_RESOURCE`. This is distinct from the 2BED response stall.
No failed slot or parent-chain detail is currently logged, so root cause is
unresolved. Preserve both physical logs. Do not conflate `F205` with the
transport failure or claim option-2 acceptance. Logs are under
`artifacts/results/jcalgtest-1.8.5-ccid-recovery-20261113/`.

## 2026-11-13: Validate stored overlay record bounds before tag reconstruction

`reconstruct_overlay_tag()` previously validated that the record offset was
inside `overlay_bytes_used`, but trusted the serialized key length and then
read/copy key bytes without checking the complete key and value span. A
malformed or inconsistent serialized header could therefore read beyond the
used overlay region during compaction. The function now verifies that
`key_length + file.size` fits in the remaining bytes before accessing the
payload. The regression corrupts a stored header to claim a 127-byte key and
confirms compaction returns `FS_STATUS_IO_UNKNOWN` without reading beyond the
record. `./tools/test-rp2350-fs-overlay` passes; ARM/RISC-V wrappers build.

This is defensive hardening, not a confirmed root cause for the physical D92
`tag_reconstruction` rejection: the exact slot/header is still unknown and the
near-capacity parent-encoded regression does not reproduce it. If a future
physical run rejects compaction, use `FS_TAG_RECON_FAIL` to capture the actual
record and verify whether this guard is implicated. Keep the CCID transport
stall as a separate failure class. Follow-up host coverage now also tests a
nested child -> parent -> root chain with both ancestors tombstoned through
compaction, re-created after remapping, and verified after snapshot restore;
the full filesystem suite passes. This still does not reproduce the D92 failure.

## 2026-11-13: D92 object deletion root scan is also incomplete

The preserved D92 option-2 serial log repeatedly reports
`OBJECT_DELETION:SKIP ... applet_roots_complete=0` and
`CHOUPI:HEAP_GC_SKIP ... applet_result=6`. The latter is a generic
`VisitResult::ReadFailure` assigned when `enumerate_applet_context_root()`
returns false; it does not prove the persistent read itself failed. The helper
could be failing because the restored reference is null, instance restoration
throws, or `visitAppletInstanceRoots()` does not find exactly one matching
owner/package/class root. Added debug-only once-per-boot
`CHOUPI:APPLET_ROOT_SCAN_FAIL` details for those branches. The ARM diagnostic
image contains this and the filesystem reconstruction trace, but is not on
hardware because the current approved host path finds zero Pico devices.
On the next physical run, capture the first failure reason before attempting a
long option-2 campaign. Keep this incomplete-root signal separate from the
compaction `tag_reconstruction` rejection and 2BED CCID transport stall.

Follow-up 2026-11-13: the D92 log's first reclaimer `applet_result=6` at
xfr=142 (after three applet erasures) precedes the incomplete applet-root scan
on the next APDU; the later scan failure is not established as the initiating
cause. `enumerate_applet_context_root()` now preserves exact inventory status
instead of converting every false result to `ReadFailure`, and maps missing
or mismatched root count/metadata to `IncompleteReachability` or `InvalidTag`.
The refreshed diagnostic package is `9188dcfc` (see EXT_REQUESTS); no physical
validation has occurred. After the user's PC/SC-restored report, the approved
VM host helper still found zero matching Pico devices, so do not attempt a
hardware cycle until USB passthrough exposes a reader/BOOTSEL device again.

## 2026-11-13 Windows d63877f6 loses communication on reset, without memory SW

Evidence: `artifacts/results/windows-maxcrypto-20261113-d63877f6/ANALYSIS.md`. Returned UF2 matches no-debug package hash. 1,920 responses all 9000; first error on B0E2 reset after successful AES-192 ECB NOPAD preparation, five baselines and first 50-iteration sample. Error 0x8010002F = SCARD_E_COMM_DATA_LOST. Subsequent B043 and SELECT also fail. Both rehash fix and CCID recovery are included; neither a new rehash failure nor exhaustion is demonstrated. 956 successful resets and per-sample reset pattern invalidate a once-per-method cleanup assumption for this client log. Root cause unresolved without serial/endpoint/collector state; no new fix claimed.

## 2026-11-14 MEM-01: archived 8E trace confirms alias-target deletion preceded heap read failures

In `artifacts/results/jcalgtest-1.8.4-resource01-20261112/8E464F4E2BBC0A19/option1-all-algorithms/serial.log`, body tag `03013894FFFFFE000000420007FFFF00000045` is classified `reachable=0` in the deletion scan (line 944), added to the batch (945), and erased (1004). Immediately afterward, the heap trace fails to read that same body while loading field `03013894FFFFFE000000420007` (lines 1012–1014). This proves the immediate chain: the old reclaimer erased a body named by a reachable serialized alias; the later heap trace encountered a missing target and became incomplete. The current reachable-alias guard covers this case and has a focused regression.

The tag formats explain why the old reachability test missed it: the alias source ends in `...0007`, while the body extends it by the six-byte `FFFF00000045` suffix. `Heap::hasReachablePersistentTagPrefix()` recognized exact object tags and direct two-byte child records; it did not resolve this alias encoding. The reclaimer therefore erased the body even though the serialized alias source was reachable. The current alias-chain guard resolves and protects the target, and `reachable_durable_alias_protects_body_subtree_from_incomplete_heap_walk` covers the missed-edge case. T6's historical root-cause finding is complete. The archived run is not clean post-fix evidence; zero/unexplained `alias-protect` runtime telemetry remains a separate T7 acceptance item.

## 2026-11-14 MEM-01: refuse automatic empty-store recovery when both COW copies are invalid

The RP2350 overlay's `fs_init()` checks for stored CAPs or any current/legacy snapshot header if no snapshot restores. If such evidence of prior persistent data exists, it logs `FAULT FS snapshot unavailable; preserve flash for recovery` and returns failure rather than initializing a blank store. `filesystem_restore_fails_closed_without_mutating_two_invalid_snapshots` corrupts both payload CRCs and asserts failure with the entire 2 MiB fake-flash image unchanged. The stronger `filesystem_restore_fails_closed_for_two_ambiguous_cow_copies_without_mutation` retains valid payload and header CRCs while making record intervals overlap in both copies, then asserts the same no-write failure. `./tools/test-rp2350-fs-overlay` passes. Physical recovery procedures remain outside this test.

## 2026-11-13 MEM-01: one bad overlay record becomes permanent exhaustion

**Symptom:** reclaimer logs `applet_result=6` once, then every later request
`OBJECT_DELETION:SKIP … applet_roots_complete=0`; later
`FS_COMPACT_REJECT reason=tag_reconstruction` repeats and writes return
`F205`; or RAM GC logs `HEAP_GC_SKIP edge_failures>0` every APDU until
`*** PANIC *** Out of memory`.

**Confirmed causes (code review, not yet fixed):** reclaimer returns
`ReadFailure` forever on a candidate that `fs_next_tag` enumerates but
`fs_exists` cannot resolve (M3); erase/append offset fix-up corrupts a second
slot sharing the same record offset (M6); snapshot load never validates index
consistency, so damage persists across reboots and firmware-only flashes
(M7); RAM GC is all-or-nothing on any trace edge failure (M4). Initial damage
came from the pre-`92c91943` compaction rehash and pre-`36fcfe69` alias-target
erasure.

**Viable response (until T1–T3 land):** treat such a board's store as
corrupt; re-flash with an empty-card UF2 before acceptance runs and record
the initial overlay file count. Do not chase new tracing on that state.
**Planned fix:** MEM-01 T1–T8 in TODO.md.

Evidence: `docs/review_memory_analysis_opus55_20261113.md`;
`artifacts/results/jcalgtest-1.8.5-ccid-recovery-20261113/D92E743D5F640C0F/serial.log`
lines 3527–3580 and 14062+; 8E OOM at line 12905 of
`artifacts/results/jcalgtest-1.8.4-resource01-20261112/8E464F4E2BBC0A19/option1-all-algorithms/serial.log`.

## 2026-11-14 MEM-01: prefix-wide alias keep fills the arena on a clean store

- Symptom: on the VPCD build with the RP2350 overlay backend
  (`CHOUPI_PC_RP2350_FS_OVERLAY`), option 2 stops after ~1,256 CSV lines.
  - The log shows `CHOUPI:FS_WRITE_FAIL … bytes=98287/98304`, then `6F00`, then
    repeated `F205`.
  - `OBJECT_DELETION:SCAN` always reports `candidates=0`, while
    `reachable_alias` keeps growing.
- Cause: the T4/T5 prefix keep (`hasTagPrefixRoot(tag, reachable_alias_targets)`).
  It retains superseded bodies under a live object prefix forever.
- Viable fix (implemented with focused offline coverage): resolve reachable
  aliases into heap roots with `extendReachableFrom`; let the heap trace decide
  which descendants remain live, never keep a whole historical tag prefix.
  Marked transient arrays that still have durable identity tags must also count
  as reachable during that trace. Otherwise alias resolution repeatedly adds
  the same array root and fails closed before reclaiming stale bodies.
- Evidence: the transaction suite passes a 500× persistent field reassignment
  regression with a stable 5 records / 121 tag-plus-payload bytes after every
  collection. The bounded VPCD run `artifacts/results/jcalgtest-support-vpcd-20261114T063317Z`
  reached 5 candidates at scan 400 and erased 564 records over 445 deletion
  calls, with no write failure in its 60-second window. It ended before option
  2 completed, so storage plateau/full-run acceptance is still outstanding.
- Diagnostics: `PICO2_GC_SCAN_DIAGNOSTICS=1` prints `ALIAS_ROOT` counts per
  prefix. The prefix with a growing count is the leak.
- Evidence: `artifacts/results/jcalgtest-support-vpcd-20261114T055811Z/emulator.log`.

## 2026-11-15 PC/SC service active but bounded reader listing times out

The approved `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper succeeded and
granted ACLs to three USB nodes. A read-only `systemctl status` showed both
`pcscd.service` and `pcscd.socket` active, but elevated
`PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --timeout 8` still exited 124
after its 8-second bound with `READERS:ERROR listing timed out after 8 s`.
No reader names were returned. This does not identify the USB nodes as JCVM
readers and does not justify repeating scans. VPCD/physical campaigns remain
blocked until bounded reader enumeration works; proceed with offline work.
Follow-up at 2026-11-15 01:39 CET: the approved helper again granted three USB
node ACLs, but the same 8-second `./tools/list-readers --timeout 8` invocation
timed out with no names. This repeats the enumeration blocker; no board identity
or PC/SC campaign can be inferred from the granted nodes.
Later on 2026-11-15, the helper again granted three USB-node ACLs (`001/074`,
`001/078`, `001/077`), but bounded elevated `PICO2_HOST_USB_ACCESS=1
./tools/list-readers --timeout 8` again timed out after 8 s with no readers.
The local VPCD regressions remain usable; the helper's ACL count establishes
neither physical token identity nor PC/SC reader availability.
Another read-only trace confirmed all three nodes enumerate as
`2e8a:0009 Pico2 JCVM CCID`. SmartcardIO successfully connected to
`/run/pcscd/pcscd.comm`, sent its context and reader-list requests, and blocked
on `poll(POLLIN)` awaiting the daemon reply until the 8 s timeout. `pcscd`
remained alive around 1:20 after the client timed out, with the main thread
observed at 1.6% CPU. This localizes the observed stall to the PC/SC request
response path; it does not establish whether the daemon is blocked in a USB
driver, a reader probe or another internal operation. Trace:
`artifacts/logs/smartcardio-list-readers-strace-20261115.log` (SHA-256
`2ec963a0dd001fc2f94f56b74bcf1db05ed9d4b78c6cf459c5d6f9f378356df3`).

## 2026-11-15 T7 heap inventory diagnostic filter excludes the overlay fixture

`PICO2_GC_SCAN_DIAGNOSTICS=1` produced no `OBJECT_DELETION:HEAP_INVENTORY` in
the high-occupancy overlay test. The test calls `Heap::traceReachableFrom` with
an explicit vector of 20 root references and reaches 300 arrays, but does not
populate `static_reference_roots`. The optional inventory is compiled only
under `PC_VERSION` and its guard requires both `arrays_reachable >= 100` and
`static_roots >= 20`; therefore the high-occupancy T7 fixture does not meet
the filter even when that diagnostic block is compiled in. The overlay test
passed (`TEST:SUMMARY fail=0`, 12 compactions, peak handles 309). Do not treat
missing inventory output as a successful count of object roots. Either adjust
the diagnostic predicate to cover vector-root graphs or obtain alias-root
telemetry through the unchanged-client VPCD run. Reader enumeration remains
blocked separately.

## 2026-11-15 Captured option-2 reader-loss sequence succeeds in VPCD replay

The complete 1,972-command APDU transcript from physical 2BED's option-2
session was replayed from an empty card through the PC emulator with object
deletion enabled and the RP2350 filesystem overlay backend. The first 1,968
responses matched the source except for the two known pre-selection GET DATA
differences. The four commands that had no source response completed as reset
`9000`, measure `AA9000`, reset `9000`, and prepare `AA9000`. Replay logged no
F205/NO_RESOURCE, filesystem failure, alias-protection warning, unexpected
status, or VM fault; all sampled overlay syncs succeeded. This means the
captured applet/storage sequence does not reproduce the physical failure in
the emulator. It cannot test USB bulk-IN completion, CCID endpoint recovery,
or Windows/Linux PC/SC publication, so it does not close HOST-01 or MEM-01
acceptance. Preserve the physical transport diagnosis as unresolved rather
than attributing the reader loss to those applet commands. Evidence:
`artifacts/results/t7-option2-aes192-full-capture-20261115/`.

The same persistent overlay was then exercised for another two captured
1,968-APDU loops plus a 200-command continuation. Every replay completed with
no resource/FS/VM failure. The observed pre-compaction peak was 81,775 bytes;
post-compaction restores returned to 367 files and 14,687–15,510 bytes across
sampled cycle boundaries (next process began at 367 files / 15,456 bytes).
This supports a stable live set across repeated synthetic cycles while raw
overlay use rises between compactions. It is still not a fresh live-client or
physical test. See `artifacts/results/t7-option2-aes192-post5904-loop2-20261115/`
and `artifacts/results/t7-option2-aes192-post5904-watermark-20261115/`.
