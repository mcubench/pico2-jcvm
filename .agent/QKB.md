# Issue knowledge base

## GPPro DELETE preflight can encounter an earlier JCVM out-of-memory panic — 2026-11-17

On F42DD9D07D6D2B07, after the focused RSA physical matrix, the bounded
`gp.jar --delete 4A43416C675465737431` check returned
`SCARD_E_NOT_TRANSACTED` after 16 seconds. The serial capture shows GPPro's
initial `00A4040000` ISD SELECT, repeated filesystem compaction messages and
`*** PANIC *** / Out of memory`; it contains no `E4` DELETE APDU. Therefore
this is not evidence that DELETE itself stalled or hit the watchdog. The exact
capture is `artifacts/results/longop01-delete-audit-20261117T1815Z/F42DD9D07D6D2B07/`.

**Recovery verified.** Rebuild the normal ARM `max_crypto` image, then use the
identity-pinned full JCVM wrapper `./tools/cycle choupi --profile max_crypto
--serial F42DD9D07D6D2B07`. The 2026-11-17 recovery passed with
`TEST:SUMMARY pass=1 fail=0`, no `TEST:FAIL`/`FAULT`, and re-enumerated the same
chip ID as a PC/SC reader. The image hash was
`6e645547bd70fbd8fad6fd65d7b9506ab7989964eaa1ab8af532b858e3e21f67` (normal,
object deletion ON, debug OFF). Storage was preserved. Before diagnosing this
as a DELETE regression, establish that the preceding focused RSA work and
filesystem compaction have not exhausted transient memory; retain serial from
before the first GPPro SELECT.

**Follow-up result (2026-11-17).** After the normal cycle, an unauthenticated
SmartcardIO `80 E4` correctly returned `6982` without mutation. GPPro then
completed authenticated applet and package DELETE on the same image and
reinstalled the unchanged 1.8.4/jc305 CAP. The first successful run started
with a 19,985-byte / 534-file filesystem snapshot and serial compaction showed
`534->134` files, then `134->2`; both encrypted E4 commands returned `9000`.
A second GPPro `--debug` measurement on the smaller post-reinstall state timed
applet E4 at 1.057 s and package E4 at 726 ms (whole command invocations 1.487 s
and 938 ms). The capture had no `FAULT`, `PANIC`, or `TEST:FAIL`, and no
`CHOUPI:USB_PROGRESS` time-extension record for E4. Artifacts:
`artifacts/results/longop01-delete-authenticated-20261117T1728Z/F42DD9D07D6D2B07/`
and `artifacts/results/longop01-delete-measure-20261117T1732Z/F42DD9D07D6D2B07/`.
This confirms the earlier panic was cleared by a normal cycle and authenticated
DELETE works at the observed data size. It does not validate a full 96 KiB
overlay or explain the earlier 15 s Windows observation; retain T5 for a
large-store measurement and the other synchronous card-manager flash paths.

## Physical token roster — 2026-11-17

Selection policy ("one working token, switch only when unavailable") is in
`reference/physical-token-access/SKILL.md`. Chip ID is stable across replugs and
equals the USB serial (verified with `picotool info -a`, `chipid: 0xee116f37c7e4a04e`
for serial `EE116F37C7E4A04E`); the USB bus/device number and reader index are not stable.

**Attached to the new machine (observed 2026-11-17 18:30 via `lsusb` and sysfs):**

| Nickname | Chip ID | State when first seen |
| --- | --- | --- |
| EE11 | `EE116F37C7E4A04E` | BOOTSEL (`2e8a:000f`), no program loaded |
| 07E7 | `07E74C5D7A038DEC` | BOOTSEL (`2e8a:000f`) |
| F42D | `F42DD9D07D6D2B07` | BOOTSEL (`2e8a:000f`) |
| 22A9 | `22A9DEAAA3D09795` | running a non-JCVM program (`2e8a:0009`, product "Pico", mfr "Raspberry Pi") |

Nicknames are the first four hex digits (proposed, not confirmed by the user). None of
these is a JCVM CCID device yet, so the approved helper reports `found 0`.

**Do not assume any attached board already runs the JCVM.** The attached Pico 2 boards
may be in BOOTSEL, running JCVM firmware, or running some other firmware (smoke test,
stock/other program). A board only appears in PC/SC after the full JCVM is flashed.
"No boards" / `list() failed` / helper `found 0` / `SCARD_E_NO_READERS_AVAILABLE` therefore
means "no board is running the JCVM", not "no hardware" or a broken pcscd.

**Viable solution (2026-11-17, verified).** Diagnose by layer: `lsusb` (`2e8a:000f` =
BOOTSEL, `2e8a:0009` = running; `lsusb -v -d 2e8a:0009` must show a CCID interface, class
11, for JCVM; CDC + vendor-only means other firmware), then `picotool info -a --bus N
--address M` for chip IDs of BOOTSEL boards. If no board runs the JCVM, flash one:
`PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --serial <CHIPID>` (works from BOOTSEL or
other firmware), then `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only` and the
approved helper (`count=1`). Verified: F42D flashed from BOOTSEL, reader
`Pico 2 CHOUPI CCID [JCVM CCID] (F42DD9D07D6D2B07) 00 00` listed. Do not debug
pcscd/libccid first when no board runs JCVM firmware.

**Fresh USB refresh (2026-11-17 18:35 UTC, elevated bounded `lsusb` + `picotool info -a`):** F42D is running the full JCVM (`2e8a:0009`) and is the single enumerated PC/SC reader. 07E7 (`07E74C5D7A038DEC`) and EE11 (`EE116F37C7E4A04E`) are identified in BOOTSEL (`2e8a:000f`), available to provision for a later campaign. A second runtime `2e8a:0009` product "Raspberry Pi Pico" was visible but its chip ID was not captured; do not assign it to the old 22A9 identity without checking. The live option-2 run is on F42D, so do not flash another board until it finishes; 07E7 is the identified candidate for the separate all-algorithms run.

**F42D USB-visible but absent from PC/SC (reconfirmed 2026-11-18 13:17 CET).** Bounded elevated `lsusb -v -d 2e8a:0009` identified 22A9 as generic Pico firmware, plus `F42DD9D07D6D2B07` and `07E74C5D7A038DEC` as `Pico2 JCVM CCID`. With no campaign live, `tools/list-readers --pico-only` socket-activated `pcscd` but did not return a reader. The approved helper then reported `ACCESS_GRANTED count=2` for the two JCVM USB nodes; the subsequent bounded reader list still contained only 07E7. Thus F42D is not available for SmartcardIO even though it is USB-visible and has a CCID interface. Do not claim it as a campaign token. No firmware was flashed. 07E7 is the only currently confirmed PC/SC token; keep using it for exploratory work until another exact reader appears. The `pcscd` refresh was performed with no campaigns live.

**Boards from the old machine (not attached here as of 2026-11-17):** 8E
`8E464F4E2BBC0A19`, D92 `D92E743D5F640C0F`, 2BED `2BED26ED20556535`, and an unnamed
`48E9A50BB405634E`. Old campaign results refer to these IDs; do not compare them with
runs on the new boards without noting the token.

**Helper:** `pico2-pcsc-reader` (source `tools/pico2-pcsc-reader-root`, installer
`tools/install-pcsc-sudoers`) is installed on this machine and accepts 1-4 matching
`2e8a:0009` "Pico2 JCVM CCID" devices. Running it with no JCVM firmware attached prints
`Expected 1 to 4 ... found 0`, which is expected, not a failure of the helper. The
2026-11-15 "refuses at four devices" observation was the old machine's 1-3 limit.

Do not infer a roster from this table without re-listing; devices change.

## Repository moved to a new host (agentdev, Debian 12) — build broke — 2026-11-17

**Viable solution (applied).** After moving the repo from `/home/claude/Projects/pico2-jcvm`
(newer distro) to `/home/agentdev/projects/pico2-jcvm` (Debian 12, glibc 2.36):
- `.local-toolchain/root` binaries (cmake 4.2.3, ninja, gcc 14.2.1) need glibc >= 2.38 and
  do not run. `tools/toolchain-path.sh` now probes the local cmake and otherwise uses the
  host cmake/ninja; install the Arm GNU Toolchain 14.2.rel1 tarball to
  `.local-toolchain/toolchain/arm-gnu-14.2` (preferred, self-contained newlib).
- Host needs a JDK >= 11 (`openjdk-17-jdk-headless`; capmap-free classes are version 55) and
  Apache Ant (unpacked to `.local-toolchain/ant`).
- Every CMake cache under `build/` embedded `/home/claude/...` and was rejected. Move such
  dirs aside (they were moved to `build/_stale-claude-paths/`) and rebuild; in
  `build/choupi-pc/generation.sha256` the path prefix was rewritten, hashes unchanged.
- `~/.bashrc` exports `PICO_SDK_PATH=$HOME/pico/pico-sdk` (unpatched TinyUSB, no
  `tud_task_budgeted`). The wrappers now ignore the ambient variable; override only with
  `PICO2_SDK_PATH`.
- **Flashing with several RP boards attached:** `tools/flash` never set picotool's
  `--bus/--address` selector (`set_target_selector` had no caller), so `picotool load`
  failed with "Command requires a single RP-series device to be targeted" as soon as a
  second RP-series board was present. Fixed: the selector is set before load and
  BOOTSEL requests, the file precedes the selectors, and `-x` is replaced by an explicit
  `picotool reboot --bus --address` (`-x` fails the same way with several devices).
  `picotool reboot -F -u` from a running JCVM can still fail the same way; the wrapper
  then falls back to the CDC-1200 touch, which works.
- **Cached Java 21 classes:** `build/jcalgtest-reader-index/*.class` was compiled by the old
  host's Java 21 and fails on Java 17 (`UnsupportedClassVersionError 65.0`). Delete such
  cached classes (moved to `build/_stale-claude-paths/java21-classes/`) to force a rebuild.
- **Reader not published (`SCARD_E_NO_READERS_AVAILABLE`):** this host's
  `/etc/libccid_Info.plist` (libccid 1.5.2) has no `0x2E8A:0x0009` entry, unlike the old
  machine's patched table. Install it once as root with `sudo ./tools/install-libccid-pico2`
  (edits a backup-protected copy, validates it, restarts pcscd). Debian runs `pcscd` as root,
  so there is no `pcscd` account and the helper's ACL grant is skipped (`acl=not-needed`).
- Compiler is now whatever `arm-gnu-14.2` provides; record compiler/host in provenance when
  comparing timings with pre-move campaigns.

## Ctrl-C aborts the VPCD emulator ptrace shim — 2026-11-16

Interrupting `tools/test-jcalgtest-support-vpcd` with Ctrl-C while the
JCAlgTest client is active can propagate SIGINT into the PC emulator, which
panics in `src/arch/host/emulator.rs:743` with `Unknown ptrace signal received`
(the current trace showed signal 2). The wrapper then reports a card removal
and the client may fail during its current APDU. Treat that as an interrupted
host run, not a card-side failure or a pass. Preserve the logs and inspect the
RP2350 overlay; do not resume unless the matching PC memory file and
`.cap-registry` are both present and validated. No graceful mid-suite shutdown
procedure has been verified. For acceptance, start from fresh state and allow
the bounded suite to finish rather than using Ctrl-C.

## Picotool aborts after verified flash — identity-pinned runtime recovery — 2026-11-14

Picotool can abort with `picoboot::connection_error` / status 134 after its
`Verifying Flash: 100%` milestone, while the board has already rebooted. Do not
accept a nonzero flash status from progress output alone. `tools/flash` now
accepts this narrow case only after full verification was reported and the
same pinned USB serial or RP2350 chip ID reappears as a runtime device within
10 seconds. The subsequent cycle monitor still checks finite firmware health.
Offline regression coverage rejects loading-only output, an unpinned runtime,
and a different chip ID. The recovery path was not triggered in the latest
physical run; a normal 8E flash/verify/reboot/monitor passed. The D92 board
verified its write but did not reappear as a serial device and is currently
not enumerated; continue on another identified board rather than retrying D92
without USB visibility.

The CDC-1200 idle reset path was also exercised on 8E on 2026-11-14: the
same chip ID appeared in BOOTSEL after 3,119 ms (9 short polling attempts).
The max_crypto image was then restored through `tools/cycle`; ARM/RISC-V
builds and the finite runtime monitor passed. This does not cover a reset
request during a long synchronous APDU.

The BOOTSEL polling loop previously ran `picotool info` once during USB target
identity validation and a second time in `is_bootsel()` for each poll. This
duplicate call increased contention and made the loop's counter-based timeout
longer than wall time. The poller now relies on the validated product ID and
uses a wall-clock deadline. `tools/test-flash-target-selection` covers
runtime-vs-BOOTSEL classification; 8E cycle passed after the change. The
identity-pinned loop no longer spends a second Picotool timeout on each probe.

## One damaged persistent candidate blocking object deletion — scoped fix 2026-11-14

Applet and package-static reclaimers now leave an enumerable but unresolved
candidate intact and continue reclaiming the other candidates across bounded
batches. They report the first bad tag once per invocation, avoiding a serial
log line for every later batch. A weak filesystem hook lets the RP2350 overlay
check its index and rebuild a pure probe-chain placement fault or a stored-hash
mismatch whose serialized tags and parent chains remain verifiable; after
repair, the reclaimer restarts enumeration. Other record and arena ambiguity
is not mutated and remains an integrity failure to investigate. `ReadFailure`
logs identify `alias_chain`, `root_scan`, or `candidate_scan`.

Regression evidence: the fake-flash overlay suite repairs an orphaned probe
slot and reclaims all 20 records; injected unresolved applet and package
candidates remain present while the other 19 in each namespace are reclaimed.
This does not establish long-run JCAlgTest behavior or physical acceptance.

## Stored overlay hash mismatch — safely repairable when the arena is intact

`overlay_integrity_valid()` now decodes the canonical tag before comparing its
stored index hash. Recompute hashes only for self-contained plain/compressed
tags, after all serialized bounds, contiguous non-overlapping intervals, and
unique canonical live tags verify; then remap parent-slot references. A
recovery test corrupts a self-contained parent record's hash and verifies its
parent-encoded child and an unrelated record survive sync, reload and
compaction. A hash mismatch on a parent-encoded record is ambiguous with a
stale/corrupt parent pointer, so reject that snapshot and try the previous COW
generation instead of recomputing the child identity. If arena layout is
ambiguous or canonical tags collide, do not mutate. This does not yet cover
dropping only individually undecodable records when no valid COW copy exists.

Compacted: 2026-11-11. Reusable diagnoses and fixes; current tasks live in
[TODO](../TODO.md), device/build state in [HANDOFF](HANDOFF.md), and run history in
[progress](../progress.md). A scoped fix does not close broader acceptance.

The [byte-for-byte backup](archive/2026-11-11-qkb-compaction/QKB.md),
[SHA-256 manifest](archive/2026-11-11-qkb-compaction/SHA256SUMS) and
[migration notes](archive/2026-11-11-qkb-compaction/MIGRATION.md) preserve all original text.
The [complete issue catalog](QKB_INDEX.md) indexes every original heading and the earlier historical index.
Catalog titles retain dated wording; read later corrections and the summaries below before reusing a diagnosis.
Evidence links identify archived sections; resolve original relative links from `.agent/`.

## Uploaded CAP could import the internal native bridge — fixed 2026-11-13

The ROM resolver searched every resident package by AID, including the
exported `fr/gouv/ssi/nativeimpl` package (`A0000000600001`). A hostile CAP
with a direct external `StaticMethodref` to
`NativeImplementation.getAID()` installed successfully before the fix. The
Export table and baseline result are recorded in
`artifacts/reviews/cap-import-policy/nativeimpl-export-table-20261113.txt`.

`Import_Handler::getPackageIndex` now denies unlisted ROM package AIDs for a
dynamic caller and is used by preflight and external reference resolution.
Keep the exact public Java Card / GlobalPlatform AID allow-list; do not replace
it with an AID prefix rule. Rejection maps to `6982`. The regression command is
`PICO2_HOST_USB_ACCESS=1 ./tools/test-vpcd-multipackage --cap-import-policy`
through elevated host access. It passes after the fix; public API installs and
the 35-case cross-AID SIO suite also pass.

## CAP Export member offsets must be checked before persistence — viable partial fix 2026-11-13

Checking only ConstantPool references leaves malformed or unused Export
entries unchecked. At LOAD, validate every static-field offset against the
StaticField image and every static-method offset against the Method component,
including complete method headers. When Descriptor is present, build a bounded
method-offset bitmap once and require exported offsets to occur in its method
records. Without Descriptor, the CAP format does not provide enough information
to prove an offset is an exact method start; retain the bounds/header check and
do not claim exact-start validation. The VPCD fixture appends a malformed,
unreferenced Export method and verifies final LOAD returns `6A80` before
persistence. Remaining: validate Export `class_offset` values against Class
record starts and tighten exported field width/type checks where metadata allows.

## BOOTSEL force-load failure and board failover — 2026-11-13

After one board accepted a diagnostic image, a later identity-pinned force-load
to that runtime board failed with picotool status 247; the wrapper could not
re-enumerate the selected device at its former bus/address. Do not keep probing
or retrying that same failing target. Enumerate BOOTSEL devices with the
approved elevated host-access path, obtain each chip ID (`picotool info -d
--bus N --address N`), and switch to another responding identified board.
On this occurrence, D92 flashed successfully after 2BED stopped responding;
its finite runtime monitor and PC/SC reader check passed. The approved reader
helper is for runtime CCID visibility and reports zero while boards are only in
BOOTSEL, so use picotool/USB enumeration for BOOTSEL diagnosis.

## USB visible, physical reader missing — unresolved / volatile

Runtime USB, PC/SC reader publication, card presence and APDU success are separate checks.
ACL access alone does not prove reader publication; libccid VID/PID support was present in captured failures.
Use the latest [handoff](HANDOFF.md) for device state, then refresh authorized access and enumerate the exact reader.
Reader slot suffixes change: match the 16-digit chip ID and refresh the full reader name before each test.
Keep board-specific logs; switch to another identified board if one disappears.
Host-only visibility with no guest USB device suggests passthrough trouble, not a proven firmware fault.
Do not start or claim a physical campaign until the client connects to that exact reader.
Do not restart PC/SC while another test is active; service restart can interrupt VPCD transactions too.

Evidence: [E002](QKB_INDEX.md#e002), [E032](QKB_INDEX.md#e032), [E425](QKB_INDEX.md#e425), [E006](QKB_INDEX.md#e006).

2026-11-14 HOST-01 code-level cause/fix for the D92 late-run queued response:
`service_pending_final_response()` used a zero retry timestamp to mean
"immediately due", then checked it with signed `time_us_32()` subtraction. At
uptime ≥2^31 µs (35m 47.483648s), zero compares as a future deadline. A newly
queued response therefore returns before submission; the response-wait trace
below the same guard also never runs. This matches D92's ≈45-minute failure
and `pending=1, in_busy=0, submit_errors=0`. The driver now tracks whether the
retry deadline is armed explicitly; tests cover long uptime and timer wraparound
(35/35 pass). This is a source-level diagnosis with strong trace correspondence,
not yet physical confirmation. Validate the fresh acceptance UF2 in
`EXT_REQUESTS.md` during a sustained option-2 run before closing HOST-01.

2026-11-14 21:53 CET: the approved helper granted ACL access to three nodes
(`/dev/bus/usb/001/074`, `/001/078`, `/001/077`), but the required bounded
`PICO2_HOST_USB_ACCESS=1 ./tools/list-readers` call timed out after 20 seconds
with exit 124 and printed no inventory. No PC/SC client campaign was started,
and no physical token was reset or flashed. This is the same distinction as
the earlier events: granted USB-node access does not establish PC/SC reader
publication. Continue offline and re-enumerate through the host path before
any physical or VPCD PC/SC run; do not repeatedly poll the hanging service.

2026-11-14 22:07 UTC follow-up: after the approved helper granted three USB
node ACLs (`/dev/bus/usb/001/074`, `/001/078`, `/001/077`), two bounded
`PICO2_HOST_USB_ACCESS=1 ./tools/list-readers` attempts (12 s then 15 s) both
timed out, including the retry after socket activation. No reader inventory
was returned. This does not prove the USB nodes are Pico readers or that
pcscd is healthy. Do not keep retrying the service; continue offline until a
fresh externally observed PC/SC state is available.

2026-11-14 sustained-run reproduction on 8E (`8E464F4E2BBC0A19`): the
physical option-2 client run lasted about 32 minutes and reached
`TYPE_DES LENGTH_DES3_3KEY ALG_DES_ECB_ISO9797_M2 Cipher_init()`. JCAlgTest
reported `SCARD_E_NOT_TRANSACTED`, then `SCARD_E_READER_UNAVAILABLE`; its
non-interactive prompt ended with `NoSuchElementException`. The supplied
1.8.5 client JAR was used (`b6a753be…e3acec3`) with the unchanged 1.8.4/jc305
CAP (`cac42cd3…dbfad`); the generated CSV labels its embedded client version
1.8.4, so retain both identities when reporting it. At the failure boundary,
the 8E serial telemetry showed no `CHOUPI:CCID_ERRORS`, and the final logged
`B0E2` response was `9000`; a finite CDC monitor still passed afterward.
Runtime USB `2e8a:0009` remained visible, but a later PC/SC scan listed only
the two virtual readers. Re-running the approved Pico-only ACL/service helper
did not restore physical reader publication. This points to a host CCID/PCSC
delivery or device-channel problem, not demonstrated persistent-storage
exhaustion; it does not prove the firmware CCID path healthy. During the run,
live heap counts stayed within 51–115 arrays and 27–28 instances over 833 GC
samples, with no `F205`, `6F00`, or `NO_RESOURCE`. Preserve the logs in
`artifacts/results/jcalgtest-1.8.5-physical-20261114/8E464F4E2BBC0A19/option2-performance/`.

## Host access and helper failures — viable execution path

Every USB/PCSC attempt must use elevated host execution from the first attempt.
For repository wrappers also set `PICO2_HOST_USB_ACCESS=1` on that command.
Only the approved `sudo -n /usr/local/sbin/pico2-pcsc-reader` exception is authorized;
do not reuse historical unrestricted sudo/service commands. Helper integrity/product-string failures
need the approved helper repair described in [external requests](EXT_REQUESTS.md).
Local VPCD sockets, AF_UNIX tests and ptrace can also fail under sandbox policy;
EPERM/bind/ptrace denial is a host-access failure, not a protocol regression.
Use the required execution path; if unavailable, stop hardware work and continue offline.

Evidence: [E417](QKB_INDEX.md#e417), [E029](QKB_INDEX.md#e029), [E367](QKB_INDEX.md#e367), [E413](QKB_INDEX.md#e413).

Host-access regression, 2026-11-12 22:49 UTC: the approved reader helper was
invoked through the required elevated host path, but `sudo` returned
`The "no new privileges" flag is set`. No USB work followed that failure;
do not retry in the restricted context. Continue offline until host execution
is restored.

## Targeted flashing and BOOTSEL — scoped viable fallback

Runtime `2e8a:0009` is not BOOTSEL `2e8a:000f`.
Use repository wrappers and a mandatory chip ID; build both architectures before flashing.
The targeted full-VM path is `tools/cycle choupi --profile <profile> --chipid <ID>`;
`cycle arm`/RISC-V smoke output is not full Java Card acceptance.
CDC 1200-baud reset has recovered boards when picotool reset lookup failed. On
2026-11-12, identity-pinned recovery entered BOOTSEL in 4/3/3 seconds on D92,
8E and 2BED; all three firmware-only flashes verified. Their finite monitors
passed and PC/SC listed all readers after the approved helper refreshed node
access. Refresh that helper after USB re-enumeration before treating a missing
PC/SC reader as firmware failure.
Close serial monitors before resetting; failure after doing so disproves monitor interference as a universal explanation.
If bounded reset fails, change identified board or request host passthrough recovery instead of repeatedly retrying.
Flashing preserves installed CAP storage unless an explicitly prepared empty-card image is used.
A flash success plus serial heartbeat proves runtime startup only.

On 2026-11-12 the VM reassigned two boards to a different USB topology path
during CDC-to-BOOTSEL re-enumeration. The selector now rediscovers a selected
board by the already pinned RP2350 chip ID when its old path disappears or is
occupied by another device; it still rejects a same-path identity mismatch if
the original chip cannot be found, and never falls back to a sole unrelated
board. `./tools/test-flash-target-selection` passes 11/11 synthetic cases.
The updated path was used to flash empty-card max_crypto to D92 and 8E; both
passed finite serial health. Physical registry verification is pending PC/SC
recovery.

Evidence: [E429](QKB_INDEX.md#e429), [E093](QKB_INDEX.md#e093), [E106](QKB_INDEX.md#e106), [E065](QKB_INDEX.md#e065).

## VPCD setup, stale state and process lifecycle — viable controls

Rebuild the selected PC profile after native changes; identify emulator/ROM/fixture hashes.
Use a freshly generated empty image for clean-card tests: stale images can retain apps, proxies or incompatible state.
Discover readers with bounded, logged retries and pin one reader through install, probe and cleanup.
A socket-connect timeout may hide an INSTALL failure; preserve emulator stdout and inspect pre-connect stages.
Terminate only the known test process group when a wrapper times out; a detached emulator can survive and consume CPU.
RV-20261106-32 now propagates tested startup, VPCD, install and persistence
failures to the parent exit code. `./tools/test-emulator-exit-status` covers
absent restored applet, refused connection, malformed frame, failed install,
image/sidecar save failures and clean close. Run it through host execution that
permits `ptrace`; continue checking diagnostics and results for new failure
paths beyond this matrix.

Evidence: [E360](QKB_INDEX.md#e360), [E089](QKB_INDEX.md#e089), [E288](QKB_INDEX.md#e288), [E297](QKB_INDEX.md#e297), [E342](QKB_INDEX.md#e342).

## RP2350 overlay snapshot semantic corruption — fallback is safe but repair is incomplete

`filesystem_slot_valid()` checks the snapshot envelope and CRC; semantic checks
in `overlay_integrity_valid()` catch index/arena inconsistencies that can still
exist in an otherwise self-consistent snapshot. The restore path rejects that
generation and tries the older copy-on-write slot. It emits one concise
`CHOUPI:FS_INTEGRITY_FAIL reason=... slot=... snapshot=...` line. If all arena
records still reconstruct and only the hash-table probe chain is broken, it
rebuilds table placement from the verified record metadata, remaps parent
indices and marks the repaired RAM image dirty for the next sync. The open-path
rehash must clear the vacated source slot; failing to do so left duplicate
entries and broke probe reachability. This clear is now present in both repair
and compaction rehash and is covered by compaction/reload checks with a
parent-encoded child. For hash mismatch or overlapping record bounds, startup
rejects the generation and republishes the valid prior COW generation. See the
2026-11-14 continuation below for the restricted per-record quarantine path;
ambiguous arena damage remains fail-closed.

The repair regression found why a probe rehash could itself remain invalid: the
open-path relocation copied the carried entry into its destination but left a
duplicate in the vacated source slot. Clear that source when a path terminates;
do not clear it when closing a permutation cycle. Apply this to both index
repair and the existing compaction rehash. Coverage with a parent-encoded child
now verifies the parent-slot remap and value survival after compaction/reload.

### 2026-11-14 — bounded quarantine after proving arena intervals

When no strict COW snapshot restores, a CRC-valid v4 snapshot can recover from
an undecodable non-tombstone record if every record's stored bounds are valid
and all intervals exactly cover the arena without overlap or gaps. Recovery
removes the bad record, packs the remaining serialized records, rebuilds the
hash table and remaps parent references. Valid unrelated records and a
parent-encoded child survive reload and compaction. Restore first tries both
strict COW generations, so quarantine cannot preempt a fully valid older copy.

Safety limits: undecodable tombstones fail closed because dropping one may
expose ROM contents. A parent-encoded record that remains undecodable after
known-bad ancestors are excluded is ambiguous with a bad parent pointer and is
not quarantined. Parent hash mismatches, out-of-bounds records, gaps, overlaps,
duplicate live identities and malformed parent links also remain fail-closed.
The fake-flash tests cover one undecodable self-contained record with no usable
older snapshot and an invalid parent pointer that must not be dropped. Do not
broaden quarantine based only on successful tag parsing; interval proof and
parent identity checks are required first.

Follow-up matrix: a malformed self-contained parent plus its undecodable
parent-encoded child is quarantined as a dependent chain while an unrelated
record survives. An undecodable tombstone with valid intervals causes restore
to fail closed (`unreconstructable_tombstone`); it is not dropped. This does
not justify recovery from overlapping, gapped or otherwise unprovable arena
intervals. If no strict COW copy remains in those cases, startup still fails
closed.

### 2026-11-14 — mixed-live overlay churn metrics

The enhanced fake-flash overlay run now holds 320 live index slots: 20
parent records, 280 parent-encoded children and 20 unrelated records, with
mixed 180–300-byte child values. Across 12 compactions and garbage batches it
used 71,274/98,304 arena bytes (72.5%), never dropped below 27,030 bytes
available, and retained every value. Integrity validation ran each cycle at
196 µs average and 240 µs maximum in this run; sync/reload every four cycles
passed. This isolates storage compaction at realistic occupancy. It does not
measure heap-handle growth or exercise the persistent reclaimer/alias graph
with this live set; the full JCAlgTest VPCD campaign remains unrun.

Interrupted-publish follow-up: five cuts at start, quarter, midpoint,
three-quarter and final-mutation offsets of a 157,184-mutation publish over
this mixed live set all rebooted into a complete old or new generation. Every
trial verified the full 300-record data set and passed integrity. This does
not replace reclaimer/heap-graph churn or JCAlgTest option 2.

The reclaimer/heap integration follow-up uses 20 persistent reference-array
roots with 280 aliased byte-array children in one long-lived Context. Twelve
cycles each create and reclaim eight unrooted arrays. The reclaimer erased all
96 candidates (eight per batch); conservative heap marking reclaimed the eight
RAM arrays per cycle while retaining all 300 rooted arrays. The initial live
set occupied 581 index slots and 67,458/98,304 bytes. Handle high-water stayed
at 308; minimum available capacity was 30,846 bytes. Twelve integrity checks
averaged 613 µs (1,024 µs maximum) in the latest run. Reload every four cycles
and child-value checks passed. This is direct fake-flash coverage of the real
reclaimer and heap graph, but does not report runtime `alias-protect` events or
constitute the unchanged JCAlgTest VPCD campaign.

Evidence: MEM-01 T1 regression and validation record in [progress](../progress.md).

High-occupancy follow-up now replaces one persistent child reference on each
of the 12 cycles while reclaiming eight unrelated arrays. The superseded
durable body is asserted absent after every reclaimer batch, the new child is
included in the persistent root graph, and the 581-slot overlay count remains
stable. Each pass erases exactly nine records; 67,458 bytes are initially
occupied, minimum available capacity is 30,752 bytes, peak handles are 309,
and integrity checks average 370 µs (1,296 µs maximum). This is synthetic
coverage of the observed historical-alias growth mechanism under mixed live
occupancy. It does not establish long-run plateau under the official client.

## CAP Class external references — validate Import token bounds before storage

External superclass references in Class records and external superinterface
references in Interface records also carry package tokens into the Import
component. Checking only Constant Pool references leaves malformed inheritance
metadata able to pass LOAD. Walk Class component records before persistence and
reject any external package token whose low seven bits are outside Import
count. Preserve the `0xFFFF` no-superclass sentinel. Regression uses unchanged
JCAlgTest 1.8.4 bytes with its applet superclass changed from `8203` to `FF00`;
the VPCD test confirms LOAD rejects before package storage. Keep separate
coverage for Import/Export consistency and transitive library dependencies.

## Late mixed-suite resource failures — unresolved; reproduce state

Capture exact ordered APDUs, initial storage, profile/build/backend and first terminal error.
Compare fresh versus accumulated state and cleanup enabled versus disabled; shorten the preceding sequence before broad reruns.
Track live RAM/handles, unreachable durable bodies and obsolete filesystem versions separately.
F205/NO_RESOURCE with persistent OutOfFlash is not proof of a RAM leak, unsupported algorithm or transport fault.
PC filesystem capacity/layout differs from the Pico overlay; do not transfer thresholds or timings between them.
An isolated RSA replay or a flat focused plateau does not close the full unchanged 1.8.4 workload.
Keep timeout, deliberate interruption, induced ptrace panic and genuine runtime failure distinct.
External Windows RSA preparation failed before the measurement APDU; the method label alone does not establish an arithmetic failure.
Continue RESOURCE-01 and offline-only JC-RESET-01 under [TODO](../TODO.md).

Evidence: [E012](QKB_INDEX.md#e012), [E409](QKB_INDEX.md#e409), [E368](QKB_INDEX.md#e368).

## Reachable same-owner alias target was reclaimed — root cause reproduced and guarded; physical acceptance open

`setPersistentField_Reference()` used to republish an alias to a persistent
array's cached body tag without confirming that the body still existed. A
focused regression deletes the body while retaining the heap handle; the old
path writes a dangling alias, while the current path throws `IOException` and
leaves the destination field unset. This protects against propagating a
corrupt edge but does not recover a body already lost.

The archived 2026-11-11 trace showed the first missing array-body read
(`03013894FFFFFE000000420003FFFE000000470000FFFE000005810001FFFF00000583`)
after two eight-record deletion batches, but lacked candidate-tag logging.
The 2026-11-12 candidate-tagged 8E run established the cause for a later
reproduction: reachable alias source `03013894FFFFFE000000420007` named body
`03013894FFFFFE000000420007FFFF00000045`, yet the same-owner reclaimer
selected that body for `stage=erase`. The heap graph had omitted that edge, and
candidate selection relied on heap reachability plus foreign aliases without
protecting a durable alias under a reachable same-owner prefix.

Commit `36fcfe69` adds that protection. A candidate body ending in the stable
array marker and identity is mapped back to its alias field prefix; if that
prefix is reachable and its serialized alias chain targets the body, the
candidate is retained. Invalid, cyclic, or unreadable alias chains abort the
inventory fail-closed. A regression with a live root and deliberately omitted
field-layout edge proves the body survives and strict restoration works. On
the corrected D92 physical run, the exact old target has appeared as
`stage=alias-protect`, not `stage=erase`; through the captured checkpoint there
were 1,092 alias protections, 184 unrelated erasures and zero
`HEAP_GC_FS_READ_FAIL` or deletion skips. The full unchanged 1.8.4 run is still
in progress, so this is early evidence rather than full RESOURCE-01 acceptance.

The earlier failing 8E run then reported `HEAP_GC_FS_READ_FAIL` for the erased
body and later skipped conservative heap reclamation, followed by handle
growth and `F205`. This confirms the chain from the missing durable body to
incomplete graph walk and retained handles for that reproduced case. Other
historical resource failures may still have additional causes.

Evidence: `progress.md`, entry `RESOURCE-01-REACHABLE-ALIAS-TARGET-GUARD-20261112`;
regressions `reachable_durable_alias_protects_body_from_incomplete_heap_walk`
and `persistent_array_handle_rejects_realiasing_missing_body`;
8E trace `artifacts/results/jcalgtest-1.8.4-resource01-20261112/8E464F4E2BBC0A19/option1-all-algorithms/serial.log`;
corrected D92 trace `artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option1-alias-guard-aa44cb12/serial.log`.

The host reclaimer now has a focused foreign durable-alias cycle regression:
two aliases owned outside the target applet point to each other. Alias
inventory returns `InvalidTag` before deleting the target or an unrelated
orphan; after removing the corrupt records, retry reclaims the now-unreachable
target records. This confirms bounded fail-closed behavior in the host stub,
not physical recovery or the cause of the archived missing-body incident.

Evidence: `progress.md`, entry `PERSIST-01-ALIAS-CYCLE-20261112`;
regression `persistent_alias_cycle_blocks_reclamation_then_retry_succeeds`.

## Request-gated deletion and batch budgets — implemented, physical acceptance open

Normal firmware reports object deletion unsupported. Diagnostic `PICO2_ENABLE_OBJECT_DELETION=1`
enables deferred servicing after the Java stack unwinds; it does not authorize production enablement.
An unbounded physical sweep erased 1,417 records before reply and coincided with transport loss;
its exact duration was unmeasured. Both PC and physical paths now share
`reclaimUnreachableObjectDeletionBatch()` and one total budget of eight records per APDU across applet/package namespaces.
Retain pending work and resume later; separate per-namespace limits would allow 16.
The VPCD batch fixture must drain with status APDU `00000D`, not new request `000008`.
One/four replacements pass with 2/6 collector calls and 10/40 records erased; assert both counts and per-call limit.
Physical response timing, actual interruption recovery and long-suite acceptance remain open.

Evidence: [E001](QKB_INDEX.md#e001), [E426](QKB_INDEX.md#e426), [E427](QKB_INDEX.md#e427), [E428](QKB_INDEX.md#e428).

## Persistent reclamation roots — fail closed; focused host evidence

RAM GC does not reclaim persistent records. Protect direct registered applet roots even if no heap object represents them,
including backing tags longer than the root prefix. Package-static graphs need their own inventory.
Trace live aliases across owner/package namespaces and cached peer statics, shareable server refs and RMI roots.
Malformed/unreadable edges, incomplete scans, active transactions or ambiguous duplicate package contexts must block erasure.
Tests demonstrate rooted peer objects survive while an orphan is erased, plus shared batches and retry behavior.
The production reclaimer participates in 1,000 modeled flash cuts; modeled cuts and a single-owner physical hook
are not physical multi-owner power-loss acceptance. PERSIST-01/02/03 gates still apply.

Evidence: [E372](QKB_INDEX.md#e372), [E373](QKB_INDEX.md#e373), [E412](QKB_INDEX.md#e412), [E414](QKB_INDEX.md#e414), [E384](QKB_INDEX.md#e384).

## Foreign-reference GC cycles — coordinated tracing, acceptance open

Foreign-reference registry edges alone must not become permanent roots.
The coordinated collector traces real roots, follows foreign instance/array edges from marked objects to a fixed point, then sweeps.
Host tests cover cycles/diamonds, repeated collection, cached static/proxy roots and production retained-context enumeration.
Ambiguous duplicate package contexts and incomplete target-context enumeration fail closed.
Physical reset/deletion, broader context edge cases and bounded actual RAM measurements remain RV-20261106-59 gates.

Evidence: [E418](QKB_INDEX.md#e418), [E419](QKB_INDEX.md#e419), [E421](QKB_INDEX.md#e421).

## Host filesystem edit relocation — viable fix for IO_LOCKED

Relocating an edited record through `write_impl()` indexed it in the defrag staging sector,
then attempted to erase that still-locked sector. Keep the old record indexed, build patched data in owned memory
and use normal `write()` allocation/replacement. Regression verifies reinitialization and no live staging entry.
`IO_LOCKED=129` is distinct from `OutOfFlash=1`; do not map both to NO_RESOURCE.
The 414-command current-profile replay passes; this fixes one failure, not RESOURCE-01 overall.

Evidence: [E011](QKB_INDEX.md#e011).

## Host compaction tracing overhead — confirmed fix and measurement

Per-word host flash programming and privilege checks caused 29,590 tracer traps / 40.032402 s per measured defrag.
Bulk programming plus removal of release-mode privilege traps reduced the measured PC VPCD defrag to 2,731 us / zero traps;
128 replacements pass 133/133 and APDU 124 took 0.314 s. FS tests pass 36/36 (two ignored), asymmetric VPCD 107/107.
Preserve flash AND/boundary behavior, failure injection and emulator MPU checks; embedded flash paths are unchanged.
The earlier “probable cause” and 43–102-second timings are superseded for this optimized host path,
not measurements of physical Pico flash.

Evidence: [E405](QKB_INDEX.md#e405).

## Persistent arrays and transaction performance — measured scope

Bulk 128-byte persistent copy reduced harness FS calls from **1,024 to 3**; 1,152 was corrected instrumentation.
Validate complete type/length/ranges, stage overlap safely and preserve non-atomic snapshot semantics.
Two physical boards passed 17/17 transaction checks; Util 19/19 and symmetric/asymmetric VPCD 102/102 and 107/107 pass.
Rollback-generation-keyed array metadata caching reduced 128 `size()` calls from 256 FS reads to 1.
It showed **no physical speedup**: cached arraylength 4.799–4.845 s versus 4.598–4.607 s;
mixed access 1.559–1.596 s versus 1.553–1.593 s. Invalidate on rollback and tag changes.
Do not equate fewer calls with lower end-to-end latency.

Evidence: [E406](QKB_INDEX.md#e406), [E407](QKB_INDEX.md#e407).

## Durable identity and rollback — reusable invariants

Persistent fields must alias stable object/array body identities instead of owning those bodies.
Replacing/clearing one field must preserve other aliases; failed first persistence and transaction abort must be retryable.
Keep cached scalar/reference/static values canonical with durable state after write failure and rollback.
Transient array identity must not make transient contents persistent. Missing-body recovery and rollback generation must agree;
reserve identities safely so abort does not reuse them, and encode both index bytes in persistent tags.
Focused RESOURCE-02 64-init AES/RSA plateaus are zero-growth; alias setup retains one 4-byte unreachable tag.
The full mixed workload and physical persistence gates remain open.

On rollback, `fs_sync()` must run with the persistent-transaction-active predicate temporarily false:
the Pico backend intentionally returns success without publishing while that predicate is true. Restore the active
guard and retain snapshots if this publication fails. Test doubles must model this deferral; a stub that always commits
on `fs_sync()` can conceal a rollback that reports success while leaving the durable snapshot unchanged. The R44
regression now checks failure quarantine and retry, plus simulated cold reset to the previous committed state.
This is host/integration evidence; physical failure/recovery remains unverified. See [R44](../TODO.md#open-review-acceptance--2026-11-06).

Evidence: [E061](QKB_INDEX.md#e061), [E060](QKB_INDEX.md#e060), [E397](QKB_INDEX.md#e397), [E401](QKB_INDEX.md#e401), [E402](QKB_INDEX.md#e402).

## Overlay capacity and package deletion — distinguish live data from tombstones

Deleted dynamic records formerly consumed all 8,192 index slots. Compaction now retains/remaps parent-slot references
and can recover tombstones on write pressure. A live full set still needs reclamation or a capacity change.
Track both payload bytes and index slots; old arena sizes are version-specific historical baselines.
Cross-store INSTALL/DELETE publication needs journal/replay/rollback at real durable boundaries.
Simulated cuts, warm resets and firmware reflash do not demonstrate actual USB power-loss recovery.
After failed DELETE, reconcile retained runtime contexts and stale routes as well as durable registry state.

Evidence: [E131](QKB_INDEX.md#e131), [E058](QKB_INDEX.md#e058), [E030](QKB_INDEX.md#e030), [E228](QKB_INDEX.md#e228).

## SIO/firewall ownership — implemented scope and limits

Firewall context belongs to the CAP; applet instances retain distinct lifecycle/owner state.
Compact handles require owner provenance across parameters, returns, arrays, statics and exceptions.
A client proxy must refer to a context-owned server route; never pass a raw server handle as a client object.
Check Shareable interface/class membership, argument kinds and typed stack/local metadata before use.
DELETE/reset must invalidate stale routes, including same-AID reinstall; persisted proxies require validated routing metadata or fail closed.
Foreign static/array/instance retention has scoped support, not blanket cross-context access permission.
Collect retained heaps before checking DELETE edges so unreachable registry entries do not falsely block deletion;
persistent alias scans must still protect real references. Complete CTX-01/03 acceptance before broadening support.

Evidence: [E113](QKB_INDEX.md#e113), [E116](QKB_INDEX.md#e116), [E044](QKB_INDEX.md#e044), [E337](QKB_INDEX.md#e337), [E359](QKB_INDEX.md#e359).

## SensitiveResult — publish only valid successful results

Clear prior sensitive state before a sensitive operation can fail, including JNI conversion paths.
On success, publish the specified boolean/short/object with owner provenance; passive calls must preserve lifetime.
Digest/Cipher/Signature and other enabled producers have focused interpreted SIO checks;
object results must remain roots until reset. Native simulation and applet execution are separate evidence.
Unsupported Checksum/AEAD providers do not count as covered producers. CTX-02 and RV-20261106-25 retain physical/conversion gaps.

Evidence: [E068](QKB_INDEX.md#e068), [E069](QKB_INDEX.md#e069), [E070](QKB_INDEX.md#e070), [E055](QKB_INDEX.md#e055).

## Crypto key/session lifecycle and dispatch — scoped fixes

Cryptographic intermediate bytes belong in reset/transaction-correct native session storage,
not persistent Java object fields. Transient key clearing must invalidate retained Cipher use.
CBC doFinal resets active IV to zero; explicit init can restore a supplied IV. Audit MAC and reset behavior separately.
JC305 inherited/redeclared Key methods require per-interface token maps; class index and CAP class token differ.
For CRT signatures, check component length before resolving a possibly null modulus; derive it from P/Q when absent.
Use native C++ dispatch; supplied upstream CAPs remain unchanged. Consult the catalog for algorithm-specific reasons,
key-size/cofactor limits, digest continuation and reference-vector controls.

Evidence: [E049](QKB_INDEX.md#e049), [E052](QKB_INDEX.md#e052), [E048](QKB_INDEX.md#e048), [E188](QKB_INDEX.md#e188), [E005](QKB_INDEX.md#e005).

## CAP parsing and interpreter hardening — defended paths, acceptance gaps

Standard GPPro LOAD omits Descriptor; Directory sizes describe original CAP components.
Do not infer arbitrary field kinds or cross-CAP types from numeric values when metadata is unavailable.
Validate complete component/header/Export/import and ConstantPool bounds; runtime instruction starts, branches,
handler ranges and method limits need checked coordinates and non-overflowing signed arithmetic.
Uploaded code must not invoke VM-internal native opcodes. Parser success is not proof of field access/firewall safety.
CAPIN tasks begin with a reproducer; keep local hostile inputs separate from unchanged upstream fixtures.
Known guards and focused VPCD recovery do not close all hostile-CAP/physical/sanitizer acceptance.

At INSTALL, validate the dynamic package's complete transitive Import closure,
not only the applet package. A library can import an absent package without
using it in its Constant Pool; a direct-only check then incorrectly accepts
the applet. LOAD may precede dependency packages, so defer closure resolution
until INSTALL. An eight-slot visited worklist handles cycles within the fixed
dynamic package table. `missing-library-dependency` in the RMI VPCD suite is
the regression; expected failure is `6A88`, while valid staged loading and
install remain supported.

Evidence: [E037](QKB_INDEX.md#e037), [E039](QKB_INDEX.md#e039), [E301](QKB_INDEX.md#e301), [E403](QKB_INDEX.md#e403).

## CCID routing, responses and reset — preserve recovery

Route each APDU once; pending responses belong to the originating applet/manager.
A failed time extension/final IN transfer must retain the result or explicitly abort/reset and rearm endpoints.
Logical power-off is a reset boundary: clear both transient classes, transactions and native session state without Java deselect callbacks.
An unauthenticated stale SCP03 session must not divert ordinary applet APDUs; protected manager operations still require authentication.
The post-GPPro-disconnect AES prepare regression passes on hardware. Keep broader power-loss/transport gates open.

Evidence: [E308](QKB_INDEX.md#e308), [E309](QKB_INDEX.md#e309), [E351](QKB_INDEX.md#e351), [E424](QKB_INDEX.md#e424).

## Build, converter and profile provenance — viable controls

Use repository wrappers. Keep generated ROM/JNI/profile artifacts coherent and rebuild after source/test relocation;
cached CMake paths and a stale PC emulator can invalidate a test result.
Do not infer full-VM support from a smoke build or capability manifest. Check firmware/CAP flash-window fit.
JC305 int signatures require converter `-i`; large test methods can trigger converter internal errors—split test methods instead.
Standalone allocation linkage was repaired during testing-data migration; separate old review persistence fixtures need aligned dependencies/slots.
The historical Rust FS workaround omits the default allocator feature to avoid obsolete `lock_api` nightly `const_fn`;
use the repository workflow rather than invoking compiler binaries directly.

Evidence: [E349](QKB_INDEX.md#e349), [E124](QKB_INDEX.md#e124), [E071](QKB_INDEX.md#e071), [E340](QKB_INDEX.md#e340), [E341](QKB_INDEX.md#e341), [E395](QKB_INDEX.md#e395).

Shared PC ROM/JNI/profile outputs are written by both `tools/build` and
standalone `tools/build-pc`. Both wrappers now serialize through
`build/.tools-build.lock`; nested PC generation reuses the parent's open file
descriptor. Keep build entrypoints behind these wrappers. The concurrent
ARM/PC/RISC-V profile regression passed and checked each architecture's
support-selection, ROM-mask and JNI provenance against its generated profile.
It does not cover external writers that bypass the wrappers.

Evidence: `progress.md`, entry `RV-20261106-33`; logs
`artifacts/logs/concurrent-builds-20261112T035503Z/`.

For VPCD parser regressions, rebuild the PC emulator with
`./tools/build-pc --profile max_crypto` after changing shared parser/VM sources
and before invoking `./tools/test-sio-cross-aid-vpcd`. The test wrapper can run
an existing emulator binary; a stale binary can accept malformed CAP data and
produce a false regression result. The Export class-offset negative case first
returned `9000` against a stale emulator, then returned the expected `6A80`
after rebuilding; it passed with Descriptor both omitted and included.

The default `pc_vpcd_applet_test.py` APDU deadline is 20 seconds, which can be
shorter than SELECT on the full JCAlgTest 1.8.4 CAP. Persisted-CAP corruption
restart runs measured applet SELECT from 18.7 to 30.7 seconds; a 20-second
deadline therefore produced a test timeout despite the valid SELECT eventually
completing. Use `--apdu-timeout 60` for full JCAlgTest restart/corruption runs
and distinguish this harness deadline from an emulator crash or card failure.
Evidence: `artifacts/logs/capin-missing-class-persisted-vpcd-20261113-retry60.log`
and `artifacts/logs/capin-missing-import-persisted-vpcd-20261113.log`.

## Startup and Windows diagnostics — implementation accepted, timings open

Normal firmware has zero console/DTR wait; bounded diagnostic startup capture is explicitly enabled.
Windows timing acceptance needs at least three cold reconnects on current and original images with serial clients closed;
record PnP, PC/SC and SELECT timings plus exact image/driver identity. See [workflow](../docs/usb-startup-diagnostics.md).
Windows PRESENT/MUTE and certutil card-type mapping are distinct from Linux APDU success; preserve exact traces before assigning cause.
CPLC OS release date is not a build timestamp: the current two equipment-ID fields encode a 64-bit source commit prefix
(see handoff/progress), superseding historical advice to rewrite release dates for each build.

Evidence: [E300](QKB_INDEX.md#e300), [E072](QKB_INDEX.md#e072), [E079](QKB_INDEX.md#e079), [E145](QKB_INDEX.md#e145).

## Acceptance evidence — version, backend and completion matter

Use the unchanged supplied JCAlgTest **1.8.4 / jc305 CAP** with the **1.8.5 host client** (explicit `--client-jar`; the catalog default is still 1.8.4) and `tools/fixture-path --verify`. Entries below that mention running "the unchanged 1.8.4 client" are historical; do not repeat them with that client.
The supplied Java Card API validation suite is separate. Legacy 1.8.2 reports stay historical.
Record exact image/profile/hash, chip/reader identity, ordered commands, exit status, duration and cleanup result.
An API heartbeat, isolated probe, incomplete CSV or timed-out matrix is not full-suite acceptance.
Do not classify expected NO_SUCH_ALGORITHM as a failure or silently relabel wrong results as unsupported.
Old “3072” rows that actually request 4096 are fixture/version-specific evidence; do not edit supplied CAPs to hide mismatches.
Archive all measurements and corrections; current remaining work belongs in [TODO](../TODO.md), not duplicate campaign diaries here.

Evidence: [E092](QKB_INDEX.md#e092), [E276](QKB_INDEX.md#e276), [E277](QKB_INDEX.md#e277), [E278](QKB_INDEX.md#e278).

## GPPro DELETE timeout followed by PC/SC loss — unresolved observation (2026-11-12)

After `GPPro --list` succeeded on all three newly flashed cards, deleting the
JCAlgTest applet on chip `2BED26ED20556535` exceeded a 30-second timeout. The
board remained USB `2e8a:0009` but lost its USB serial descriptor; its bounded
serial monitor had no output. PC/SC enumeration then stopped listing readers,
even though the approved access helper still granted access to all three USB
nodes. The other boards were not subjected to deletion. This does not establish
that DELETE caused the board symptom or that PC/SC itself caused the timeout.

After rebuilding the current empty-card UF2 and flashing D92/8E, refreshing
the approved helper restored PC/SC publication for those two boards. Their
GPPro lists now show only the ISD and no APP/PKG entries. This does not
establish whether the earlier GPPro DELETE caused the service or third-board
symptom. `2BED26ED20556535` still enumerates as `2e8a:0009` without a USB
serial descriptor and has no PC/SC reader; its bounded monitor had no startup
output and CDC-1200 did not enter BOOTSEL. Do not retry its write until its
chip ID is verifiable again. No viable recovery for that board is confirmed.
See the 2026-11-12 08:27 UTC handoff and progress entries
`MAXCRYPTO-THREE-PHYSICAL-BOARDS-20261112` and
`HOST-01-FLASH-PATH-RECOVERY-EMPTY-CARD-20261112`.

## Physical Pico reader disappears from PC/SC after a successful reset APDU — unresolved

**Symptom:** On corrected D92 image `1837533b…dde62`, the 1.8.4 all-algorithms
client ran about 13 minutes and produced 51 `ALG_` rows, then returned
`SCARD_E_NOT_TRANSACTED` for APDU `B0 75 12 00 01 16 00`. The preceding
`INS_CARD_RESET` completed with `9000` after erasing three applet records. The
next APDU did reach the applet: serial logs show `APDU_PROCESS stage=end
sw=9000 len=29`. However, there is no subsequent `CCID_RESPONSE` record for
that command; the last response snapshot is still the preceding `B0E2` reset.
No `HEAP_GC_FS_READ_FAIL`, deletion skip, `FAULT`, `F205` or `NO_RESOURCE`
appears on this corrected image.

**Observed cause status:** `choupi_vm_process_applet()` returns after the
`APDU_PROCESS stage=end` log, then `src/ccid_device.cpp` calls `fs_sync()` and
queues the CCID response. The current trace cannot tell whether it stalls in
that sync, response submission or USB completion. At exit the USB nodes still
enumerated as runtime `2e8a:0009`, while `pcsc_scan -r` listed only VPCD; the
approved helper granted two nodes and `pcscd.service` remained active. This is
not enough evidence to blame PC/SC alone or to call the firmware panic-free at
the response boundary. `dmesg --ctime` was denied with `Operation not
permitted`, so kernel USB reset events could not be checked. The earlier 8E
run is a separate reproduced heap/body failure: it emitted
`HEAP_GC_FS_READ_FAIL` and later `*** PANIC *** Out of memory`.

**Response:** Preserve the result/serial logs. Add gated stage telemetry around
applet return, `fs_sync()` begin/end, CCID response submission and IN completion;
then rerun on an identified physical board after PC/SC access returns. Do not
retry by busy polling. The alias-target fix has separate positive physical
evidence, but neither this post-applet response path nor RESOURCE-01 acceptance
is closed.

Evidence: `progress.md`, entry `RESOURCE-01-D92-PCSC-LOSS-AFTER-RESET-20261112`;
`artifacts/results/jcalgtest-1.8.4-resource01-20261112/D92E743D5F640C0F/option1-alias-guard-aa44cb12/client.log`,
`serial.log`, and `results/Pico_2_CHOUPI_CCID_[JCVM_CCID]_(D92E743D5F640C0F)_00_00_ALGSUPPORT__3b_80_01_81.csv`.

## CCID final response blocked by stale time-extension state — recovery deployed; long-run validation pending

On physical 2BED (`2BED26ED20556535`), the pre-recovery JCAlgTest option-2
campaign reached xfr=2037 and stalled on reset APDU `B0E2000000`. Object
deletion completed and erased eight applet records; persistent `fs_sync()`
returned status 0 after 1,379,916 us, and the VM response was `9000`. The
CCID trace then reported `response_submit_return pending=1 zlp=0 in_busy=0
submit_errors=0`; stage remained 6 and the client watchdog timed out after 60
seconds. There was no `F205`, persistent-write failure, or `FAULT` before the
stall.

Given `retry_pending_ccid_transfer()`'s gates, this state indicates the final
response was withheld because `native_extension_in_flight` remained set even
though the IN endpoint was idle. A time-extension completion callback had not
cleared that state. This explains why the final response was never submitted;
it does not establish why TinyUSB omitted/delayed the callback.

Recovery now records when a time-extension IN transfer is accepted. When a
final response is pending and the endpoint is idle for at least 100 ms, the
driver clears stale extension state and submits the response. An active native
operation is not affected because this recovery requires a pending final
response. `./tools/test-ccid-progress-flow` passes 30/30 and ARM/RISC-V builds
pass with object deletion plus CCID diagnostics enabled. This image is flashed
on D92 and 2BED, both of which passed finite startup checks.

The new option-2 run on D92 uses the supplied 1.8.5 client with the unchanged
1.8.4/jc305 CAP. At the captured checkpoint it had reached xfr=496 with normal
responses; it has not reached the old failure point or logged
`CCID_EXTENSION_RECOVER`. Do not mark this recovery proven until the event is
observed and later APDUs complete. Preserve the logs and keep broader RESOURCE-01
acceptance open. Artifacts:
`artifacts/results/jcalgtest-1.8.5-compaction-fix-20261113/2BED26ED20556535/`
and
`artifacts/results/jcalgtest-1.8.5-ccid-recovery-20261113/D92E743D5F640C0F/`.

The previously requested PC/SC restart has been completed by the user and the
physical readers are presently enumerated. Refresh the approved helper only
between campaigns; running it during a SmartcardIO session can restart PC/SC
and tear down that active connection.

## Maintenance

Add or update the matching diagnosis instead of prepending repeated availability checks.
Use **symptom → confirmed cause or hypothesis → viable response → validation scope/limit → evidence**.
Keep exact commands only when safe under current AGENTS.md; a historical command is not authorization.
Put new experiment logs/hashes in progress and link them. Keep stable issue IDs in the catalog;
add new catalog entries without renumbering existing ones. Do not upgrade an unresolved hypothesis to a viable fix without evidence.

## Archived incident entries (2026-11-17)

These dated 2026-11-12..16 narratives were moved verbatim to [archive/2026-11-17-qkb-compaction](archive/2026-11-17-qkb-compaction/QKB-archived-sections.md) ([migration table](archive/2026-11-17-qkb-compaction/MIGRATION.md), [SHA-256](archive/2026-11-17-qkb-compaction/SHA256SUMS)). Search there for old evidence; conclusions live in `TODO.md`, `progress.md` and the sections below.

- BOOTSEL reset integration audit — 2026-11-12 (offline) (archive L5)
- JCAlgTest option 2 PC/SC failures on 8E and D92 — 2026-11-12 (archive L48)
- 2026-11-12 External no-debug run: reset failures precede RSA-3072 disconnect (archive L117)
- RESOURCE-01 follow-up — persistent-storage exhaustion during option 2 (archive L209)
- 2026-11-13 Windows reset campaign: distinguish transport from persistent resource failure (archive L231)
- 2026-11-13: CCID response retry remains unresolved; D92 compaction rejects tag reconstruction (archive L235)
- 2026-11-13: Validate stored overlay record bounds before tag reconstruction (archive L290)
- 2026-11-13: D92 object deletion root scan is also incomplete (archive L312)
- 2026-11-13 Windows d63877f6 loses communication on reset, without memory SW (archive L340)
- 2026-11-14 MEM-01: archived 8E trace confirms alias-target deletion preceded heap read failures (archive L344)
- 2026-11-14 MEM-01: refuse automatic empty-store recovery when both COW copies are invalid (archive L350)
- 2026-11-13 MEM-01: one bad overlay record becomes permanent exhaustion (archive L354)
- 2026-11-14 MEM-01: prefix-wide alias keep fills the arena on a clean store (archive L381)
- 2026-11-15 PC/SC service active but bounded reader listing times out (archive L407)
- 2026-11-15 T7 heap inventory diagnostic filter excludes the overlay fixture (archive L437)
- 2026-11-15 Captured option-2 reader-loss sequence succeeds in VPCD replay (archive L452)

## Overlay parent-slot reconstruction integrity

Parent-encoded filesystem records store a hash and tag length in the overlay
index, plus a parent table slot in the serialized record. Bounds checks alone
cannot detect a stale slot that still points to a different valid parent of
the same length. `reconstruct_overlay_tag()` now hashes the reconstructed
canonical tag and compares it with `OverlayFile::hash`; mismatch rejects the
read/compaction path before compaction mutates the overlay. The RP2350 overlay
suite covers a valid-but-wrong parent slot and confirms compaction returns
`FS_STATUS_IO_UNKNOWN` without losing the child's value after restoring the
correct slot. This is fail-closed hardening, not a recovery mechanism and not
evidence that the old 2,468-record physical failure had this cause. When
debugging a repeat, capture `FS_TAG_RECON_FAIL reason=hash_mismatch` to
distinguish stale index/reference state from the existing bounds failures.

## Reclaimer diagnostic alias-list memory — bounded on firmware

`collectReachableAliasTargets()` previously retained a `std::vector<Tag>` for
every expanded alias target during collection. `Tag` is 129 bytes, and the
280-child high-occupancy graph therefore represents at least 36,120 bytes of
target elements, before vector capacity slack. On firmware this list did not
affect heap tracing or candidate deletion; it served diagnostics. The reclaimer
now populates it only when PC/VPCD scan diagnostics request the summary;
normal tests and ARM/RISC-V firmware pass a null list while retaining alias
resolution and `Heap::extendReachableFrom()`.

Validation: persistent transaction, persistent inventory and RP2350 overlay
suites pass; ARM and RISC-V `max_crypto` builds with object deletion enabled
pass. No physical heap delta was measured; 36,120 bytes is the element payload
for 280 fixed-size Tags, not runtime allocator usage or vector capacity.

Foreign alias targets are also packed into one byte vector as a sequence of
length-prefixed values. During insertion, a target whose entire prefix is
already covered by a retained shorter root is skipped. This preserves
`hasTagPrefixRoot` results while reducing target storage to `len + 1` bytes per
unique, non-covered prefix rather than 129 bytes per target. The foreign alias
cycle regression and all persistent inventory/transaction/overlay tests pass;
ARM/RISC-V object-deletion-enabled max_crypto builds pass. The actual target
count and physical free-heap delta remain unmeasured.

## 2026-11-14: D92 option-2 response remains queued with idle CCID IN endpoint

Physical D92 (`D92E743D5F640C0F`) exploratory option-2 run used the diagnostic
ARM max_crypto/object-deletion image (`079ae6883c60b2fe12013c3e84bac0f5a25f35554f1215ddb96304d67c01225a`),
the unchanged upstream JCAlgTest 1.8.4/jc305 CAP
(`cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`), and
the supplied 1.8.5 host JAR
(`b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`). The
client failed after 121 method groups / 289 CSV rows at
`TYPE_AES LENGTH_AES_192 ALG_AES_BLOCK_128_ECB_NOPAD Cipher_init()` with
`SCARD_E_NOT_TRANSACTED`.

The serial trace shows the `B043000016` applet execution returned `9000`; its
`fs_sync` took 1,269,927 us. Final submission then reported
`pending=1 zlp=0 in_busy=0 submit_errors=0`. The CCID diagnostic stage remained
6 at 4,448 RX events / 4,015 tasks / 2,056 XfrBlocks, with no later IN
completion. No `CCID_RESPONSE_WAIT`, `CCID_EXTENSION_RECOVER`, or
`CCID_ERRORS` appeared. The approved PC/SC access helper granted access to all
three USB nodes, but subsequent `pcsc_scan -r` emitted only its banner and
never enumerated a reader; all three `2e8a:0009` runtime devices remained in
`lsusb`.

This run produced 1,101 heap-GC samples. Live counts settled in bounded steps
at `(95,18)`, `(103,22)`, and `(105,24)` arrays/instances; there were no
`F205`, `NO_RESOURCE`, `FS_WRITE_FAIL`, `FS_COMPACT_REJECT`, heap trace skips,
or firmware faults. Treat it as partial memory evidence and a HOST-01 CCID
response-delivery failure, not memory exhaustion. Next, inspect why the
pending final response does not transition when the endpoint is idle; keep the
host transport failure separate from MEM-01.

Logs: `artifacts/results/jcalgtest-1.8.5-physical-20261114-ccid-debug/D92E743D5F640C0F/option2-performance/`.


## Direct PC VPCD tests need elevated localhost sockets — 2026-11-14

**Symptom:** The command `tools/test-cap-malformed-vpcd --mutation external-superclass-without-import`
in the restricted execution context failed
before VPCD startup with `[Errno 1] Operation not permitted`; the outer test
then tried to read a result log that had not been created. This was sandbox
loopback denial, not a CAP or VM failure.

**Viable response:** Run direct local VPCD socket integration wrappers with
elevated execution from the first attempt. The elevated focused test passed and
reported `ERROR: PC CAP LOAD data rejected` plus
`CAP_LOAD_VALIDATION:PASS missing_required_import_rejected_before_store`; no
CAP-registry sidecar was created. This test path is local VPCD and does not
require physical USB access, but its startup installation path does not prove
a GlobalPlatform LOAD APDU status or subsequent DELETE behavior.


## 2026-11-15 Windows RSA-1984 measurement loses PC/SC communication

Incoming report and artifacts were archived from `inbox/` to
`artifacts/results/windows-maxcrypto-20261114-805420e6/` (with `SHA256SUMS`).
Superseded diagnosis: see "RSA-1984 ×50 measure APDU" below. The run used the superseded no-debug, deletion-enabled image
`pico2-jcvm-max_crypto-object-deletion-nodebug-empty-card-ccid-deadline-805420e6.uf2`
(SHA-256 `5fa91117aeb6b2ac1c6ebb537447090016b27f0717bd14e528cd5d9437e4a6de`)
on Windows, reader `Pico2 JCVM CCID 0`, ATR `3b 80 01 81`. A one-operation
RSA-1984 NOPAD baseline returned `AA9000`; the subsequent 50-operation
`B043000016` measurement ended with PC/SC `0x8010002f`, followed by `0x1f`
on applet SELECT and `SCARD_E_SERVICE_STOPPED`. `0x8010002f` is a PC/SC
communication-loss code, not a card status word and not proof of a VM memory
failure.

The supplied `AlgTestJClient.jar` is not the pinned 1.8.5 artifact (pinned
SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`);
its SHA-256 is `2a0bcf839fee03fc66215e50ce5bba9fa6378bb20475b15d79f876a39b6d47ab`.
Both archives contain 66 entries, but 3 class files differ:
`AlgTestJClient`, `CardMngr`, and `PerformanceTesting`. PerformanceTesting
bytecode differs in repeated-measurement/statistics calculation, so do not
count this as the pinned-client acceptance run. The CSV is incomplete at
RSA-1984 (SHA-256 `5dfa1a7446b59e5aeaa94400739e77f008d6c1147d0eb5982fe945132329d3f2`);
the raw log SHA-256 is `39ac9f6762982741c3ebbb0c2fb46025555533a3a5a4b1a7aa49aaca187134fc`.
No chip ID or serial log was supplied. The log shows a successful single
operation (`AA9000`, about 2.5 seconds), then no card reply for the 50-operation
APDU. It does not show whether the board reset/faulted or Windows/CCID lost the
response.

Next: use the current empty-card image in
`WIN-OBJECT-DELETION-NORMAL-DIRTY-25216292` with the pinned 1.8.5 client,
capture serial telemetry and board ID, and retain the exact failing APDU. Only
attribute this to memory exhaustion if the new run yields a memory/storage
diagnostic or reproducible firmware fault; present evidence is transport-only.

## 2026-11-15 Defer object deletion during persistent transactions

`Context::collectHeaps()` rejects a non-empty Java stack, so coordinated
collection already runs only at an empty-stack safe point. The post-APDU
deletion service previously did not check whether the persistent-write
transaction was still active. Reclaiming then could erase an unreachable
record that a transaction rollback later restores as a reference. The service
now leaves the deletion request pending until both the stack is empty and
`FlashMemory_Handler::persistentWriteTransactionActive()` is false. Focused
policy coverage checks no request, live Java frames, active transaction, and a
safe boundary. This is a safety prerequisite for MEM-01 T10, not automatic
low-watermark reclamation or a retry of a failed write.

## Automatic object deletion must compact arena tombstones — 2026-11-15

The RP2350 overlay records `fs_erase()` as deleted index entries until the
arena is compacted. A safe-point low-watermark sweep that only erases object
records can therefore keep the live object count bounded while raw arena use
continues to climb. At an empty-stack APDU boundary, when no persistent-write
transaction is active, the automatic deletion path now compacts deleted
records while an automatic sweep is pending and free space remains at or below
20 KiB. The fake-flash overlay regression created 8,240 unreachable objects
in 1,030 APDUs; it reclaimed all 8,240 after allocation stopped and the pending
sweep drained over 695 boundaries. The live root survived; max raw use was
77,852/98,304 bytes (79.2%). This is not physical proof; still validate on
hardware with an unchanged long JCAlgTest run. Direct failed-write retry is
still unsafe from the live Java stack and remains a separate design task.


## Low-watermark sweep must compact using pre-sweep free-space — 2026-11-15

When an automatic deletion sweep erases unreachable records, `fs_erase()` can
raise the reported available byte count above the 20 KiB trigger before the
service decides whether to compact tombstones. Comparing the post-sweep value
therefore skipped compaction even though the sweep began under the watermark.
Capture available bytes at the safe point before the reclaim batch and use that
snapshot for both the trigger and post-batch compaction decision. Apply this to
both physical ARM and PC VPCD/RP2350-overlay paths. Focused auto-GC VPCD test
passed 512 array replacements (515/515 APDUs), with one automatic trigger and
one compaction, retaining the live root. This does not close unchanged-client
T7 or physical T8 acceptance.


## Applet-requested deletion also needs low-watermark compaction — 2026-11-16

The safe-point compaction guard was still gated by
`automatic_deletion_pending`. That missed tombstones created by explicit
`JCSystem.requestObjectDeletion()` batches: they could erase records while
free space was below 20 KiB without compacting the arena. In the pre-fix
unchanged-client VPCD run, 4,974 deletion calls erased 11,752 records but only
one compaction occurred; raw use reached 95,253/98,304 bytes (96.9%).

**Viable fix:** compact after any non-empty deletion batch when the free-space
snapshot taken before that batch is at or below the watermark. Keep the
transaction and empty-stack safety guards unchanged. Policy and RP2350 overlay
regressions, normal ARM build and 512-replacement VPCD stress pass. A clean
post-fix option-2 run recorded 31 successful compactions, max automatic
trigger use 79,308/98,304 bytes (80.7%), and no storage/runtime failure, but
the client process ended before suite completion and the last 1,000-sync
window still grew. Do not claim T7 acceptance from this partial run.


## PC/SC helper limit and direct VPCD socket sandbox — 2026-11-15

A fresh approved `sudo -n /usr/local/sbin/pico2-pcsc-reader` call found four
matching Pico CCID USB devices while its supported range is one to three, so it
refused to change ACLs. The bounded elevated
`PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --timeout 8` then timed out with
no reader inventory. Do not bypass the helper's device-count guard or flash an
unidentified device. Reattempt only after the count is in range and reader
enumeration returns an exact name. Separately, direct localhost VPCD replay
failed with `EPERM` in the restricted sandbox; invoking the same bounded replay
through the host-access/elevated path succeeded. Use that path for direct VPCD
socket tests in this environment.

## 2026-11-15 RSA-1984 ×50 measure APDU: deterministic reader loss / panic (LONGOP-01)

**Issue.** JCAlgTest option 2 loses the card at the first RSA-1984 private-key
`B043000016` with 50 repetitions. Windows reports `0x8010002f`, then `0x1f` or
`0x8010002f` on SELECT, and often `SCARD_E_SERVICE_STOPPED` as the reader
disappears. 3/3 runs: `805420e6` (NOPAD) and `25216292-dirty` (PKCS1, twice).
Attempt 2 failed from a fresh session (SELECT, `B031`, 5 baselines, ×50), so no
long-run accumulation is needed.

**Not the cause.**
- Persistent damage: after a replug, applet SELECT returns `9000` and later APDUs work.
- The `time_us_32` 2^31/2^32 uptime bug: that fix is in both images, and a wrap passed mid-run without errors.
- Keygen: 44.3 s keygen APDUs pass because the `randomBytes()` RNG callback
  (`choupi/src/native_ec_keypair.cpp`) calls `choupi_native_crypto_progress()`.
- Card status words: `0x8010002f` is PC/SC data loss.

**Known contributors.**
- No CCID time extension or USB servicing during modexp loops (D1, confirmed in code).
- Per-`doFinal` `heap.addArray` plus Mbed TLS temporaries against the ≈188 KB malloc
  arena with `PICO_MALLOC_PANIC` (D2, suspected).
- One serial excerpt shows `*** PANIC ***` right before the ×50 APDU, message lost.

**Pitfalls when reading the evidence.**
- An LED heartbeat or serial status after the failure only shows the firmware
  is running now. It does not show the firmware never rebooted.
- Status records that stop changing while the client still gets responses
  mean the serial capture is stale or belongs to another board. Verify
  `rx_events` keeps rising, and match COM port and reader by USB instance serial.
- `rx_events` (`src/ccid_device.cpp`) increments on every CCID OUT transfer
  without exception.

**Viable technique: reach the failing operation in minutes.** The JCAlgTest
client skips operations listed in
`<cardname>____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED__already_measured.list`
when you type the same card name and answer `y`. The archived list in
`artifacts/results/windows-maxcrypto-20261115-25216292-dirty/` stops right
before RSA-1984 PKCS1 `Cipher_doFinal()`. Keep a pristine copy, because the
client updates it. Results are partial and debug-only.

**Windows uninstall timing context (2026-11-17 recheck).** The archived
`artifacts/results/windows-maxcrypto-20261115-25216292-dirty/readme.txt` places
`gp --uninstall jcalgtest.cap` after the RSA-1984 ×50 failure, failed applet
reconnect/SELECT, and a successful `gp --list`. GPPro logs the E4 APDU, then
reports `SCardTransmit` failed with `0x1f` after 15 s 11 ms. No serial capture
covers this uninstall, so this is the Windows-side transmit wait, not proof that
E4 reached/completed in firmware or that the JCVM watchdog fired. Current Linux
physical E4 measurements, including near-capacity DELETE, completed in 0.7–2.7 s;
they do not reproduce the Windows condition. Keep T5 open and require a
synchronized serial/APDU capture to attribute the historical stall.

Tasks: TODO [LONGOP-01](../TODO.md#longop-01). External request:
`WIN-RSA1984-X50-SERIAL-20261115`.

## Persistent overlay tag-prefix storage (2026-11-15)

**Finding.** The option-2 overlay snapshot near exhaustion stored many related persistent tags as full canonical byte strings. Their common tag ancestry is long, and the old parent encoding only represented a repeated two-byte suffix. On the 1,393-record failure snapshot, modeling arbitrary suffixes under existing serialized prefixes predicted about 35.6 KiB of tag-byte savings without changing the 96 KiB / 8,192-slot INV-28 limit.

**Implementation and partial runtime evidence.** Snapshot schema v5 stores parent slot + suffix length + arbitrary suffix bytes; v2/v3 and v4 bitmap snapshots remain readable, and legacy three-byte repeated-suffix parent records remain supported. Fake-flash regression verifies canonical reads, parent deletion, compaction, sync and reload. The normal ARM `max_crypto` build passes. A fresh option-2 VPCD run with object deletion enabled passed the earlier `F205` point: RSA-CRT-private 2048 / ISO14888 `Cipher_doFinal()` and `Cipher_init()` returned `9000`. Four automatic compactions triggered at 77,865–78,075 / 98,304 bytes and erased eight records each. A quiescent saved overlay passed integrity with 1,847 slots, 1,514 live records, 73,245 bytes used and 25,059 free. The client run was intentionally interrupted at a later RSA-public-3072 case; the resulting reader-removal/client exception and PC-emulator ptrace shutdown panic are interruption noise, not memory evidence. Full option-2 plateau telemetry and physical T8 remain required.

**When rechecking.** Recompute peak and post-compaction usage from a quiescent overlay image (a copy taken during APDU writes can contain mismatched metadata/arena bytes). Continue with another available identified Pico 2 if one board disappears; VPCD evidence is diagnostic only.

## Picotool selector ordering and USB sysfs hot-unplug — 2026-11-17

`picotool load` 2.3.1 parses target selectors such as `--bus` and `--address`
before the positional UF2 path. Placing them after the image returns
`unexpected option: --bus`. `tools/flash` now constructs the command with
selectors first; the unit suite asserts the exact argv order. During CDC-1200
re-enumeration, sysfs attributes can disappear between reads, so
`tools/lib/flash_usb_target.sh` treats a partial attribute read as transient
absence instead of a shell error. `./tools/test-flash-target-selection`
passes 22/22. The failed hardware attempt did not transfer firmware, so retry
only after the authorized helper and reader enumeration, and inspect a fresh
finite serial log before claiming runtime success.

## No Pico 2 devices visible after access refresh — 2026-11-17

After `sudo -n /usr/local/sbin/pico2-pcsc-reader`, the approved helper reported
`found 0` matching Pico 2 JCVM CCID devices. The elevated
`PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only` then failed with
`Cannot enumerate PC/SC readers: list() failed`. This is a current host/board
availability observation, not a firmware result. Do not retry USB access in a
restricted shell; refresh the helper and enumerate again at the next hardware
session. Continue offline while no device is exposed.

## Safe retry point after persistent `NO_RESOURCE` — 2026-11-17

In `write_persistent_tag_or_throw`, `FS_STATUS_OUT_OF_FLASH` becomes a Java
Card `SystemException.NO_RESOURCE`. The interpreter then unwinds; after the
interpreter scope ends, the post-APDU path aborts any open applet transaction
and only then invokes safe-point heap/object reclamation. This ordering
provides a reclamation point but no way to resume the failed Java operation.
Replaying the entire APDU may repeat volatile applet side effects. Retrying a
single persistent write outside its enclosing Java Card transaction would
violate transaction atomicity. Do not implement either shortcut; define an
explicit replay contract first. Existing offline garbage-churn coverage
tests 1,030 cycles × 8 allocations without applet deletion requests.

## Rust OS filesystem tests in the local toolchain — 2026-11-17

Use `./tools/env` to activate the repository-local Rust toolchain before the
OS Make target or a focused cargo test. Plain `make -C choupi/os test` fails
because cargo is not on the environment PATH. The full host test suite also
aborted at an MPU access test in this environment; running the filesystem
group directly succeeded (37 passed, 2 ignored). Focused command:
`./tools/env cargo test --manifest-path choupi/os/Cargo.toml --no-default-features --features host fs::tests::`.
When rebuilding the RP2350 PC overlay after changing the Rust library, compare
the generated `choupi` executable and `libjavacard_os.a` timestamps. CMake may
report the target built while leaving the old executable because the library
is a custom-target byproduct; `./tools/env cmake --build build/choupi-pc-rp2350-overlay --target choupi --clean-first -j2` reliably relinks it. Confirm behavior with the focused VPCD case after relinking.

## OS patch wrapper pinned to prior submodule commit — 2026-11-17

After committing the range-write change in `choupi/os` as `f12c597`,
`./tools/build arm --profile max_crypto` initially stopped before compilation:
`tools/apply-choupi-os-patches` still required its parent commit `9efd14e`.
The wrapper's expected revision now matches the root gitlink (`f12c597`), and
its existing broken-tail patch is idempotently recognized as already applied.
The ARM build then passed in normal `max_crypto` mode. If a later OS commit
advances the gitlink, update and validate this wrapper pin in the same step.


## Host allocator telemetry in VPCD worker thread — 2026-11-17

`LD_PRELOAD` malloc/operator-new probes did not observe the VM worker thread reliably, and Valgrind Massif aborted on an unknown interpreter call before VPCD connection. For focused PC/VPCD diagnostics, sample `mallinfo2().uordblks` at the native RSA VM boundary and pair it with `Heap` live-array counts; this produced stable per-call observations. Treat it only as sampled host telemetry: it does not capture allocator intra-call high-water and cannot establish Pico arena usage. Do not use these host byte totals to close target RAM findings; collect embedded allocator/arena telemetry on a physical board.

## Board availability and JCAlgTest prompt routing — 2026-11-17

The earlier `found 0` / `list() failed` snapshot meant no attached board was
running JCVM firmware; it did not mean that the physical boards were absent or
that PC/SC itself was broken. Following the layer-by-layer procedure above,
the refreshed board F42DD9D07D6D2B07 was flashed from BOOTSEL with the full
JCVM and then appeared as `Pico 2 CHOUPI CCID [JCVM CCID]
(F42DD9D07D6D2B07) 00 00`. This confirms availability is volatile and that
firmware state must be checked before diagnosing reader enumeration.

For `tools/run-jcalgtest`, do not send a reader index when PC/SC has exactly one
terminal: the client auto-selects it, and the extra line is consumed as the
main menu choice. With multiple terminals, send the selected index. Also avoid
piping all menu/card/resume answers at once: separate Java `Scanner` instances
can buffer later lines. The runner now routes scripted lines through the
prompt-aware watchdog, and leaves an un-scripted TTY interactive. A fake
multi-prompt flow validated ordered delivery. The real pinned 1.8.5 session
reached RSA-1984 ×50 and logged four successful samples before the bounded
session timed out; it was partial diagnostic work, not option-2 acceptance.

## Near-capacity authenticated DELETE timing — 2026-11-17

The physical capacity fixture in `tests/fixtures/local/fs_capacity_applet/`
creates three rooted 30,000-byte persistent arrays and reports remaining
persistent/transient memory. On F42DD9D07D6D2B07 it reached 3,820 bytes
persistent free and 88,232 bytes transient free. A normal `max_crypto` full
JCVM cycle preserved the state and reset volatile RAM. Firmware UF2 SHA-256:
`fe9b190c073170a15d99626366872c89e2f6ca0e9a01c8da1fea0f199f808255`; fixture
CAP SHA-256: `34fbf10a49fa67da486640d9b323938e86c759597597f47e758c1873de64d816`.

Authenticated GPPro applet DELETE took 2.673 s at E4 (3.859 s whole command),
and package DELETE took 1.696 s at E4 (1.895 s whole command). Both returned
`9000`. Serial showed the initial 94,479-byte / 160-file overlay (96.1% of
98,304 bytes), compaction during deletion, and successful E4 responses with
no CCID time extension, panic, fault or `TEST:FAIL`. The unchanged JCAlgTest
package remained installed. Logs:
`artifacts/results/filesystem-capacity-physical-20261117T174318Z/F42DD9D07D6D2B07/`
and `artifacts/results/filesystem-capacity-delete-20261117T1748Z/F42DD9D07D6D2B07/`.

For board availability, reread the **Physical token roster** entry above:
`found 0` means no enumerated board was running JCVM firmware, not that no
physical boards were attached. Identify USB state/chip IDs and flash the full
JCVM to an identified BOOTSEL or non-JCVM board with the approved wrapper,
then refresh the helper and list readers. The 2026-11-17 run verified this on
F42D. Never diagnose PC/SC from an old roster; board state is volatile.


## Near-capacity GlobalPlatform INSTALL timing — 2026-11-17

Recreated the 94,479-byte / 160-file overlay on F42D using the filesystem
capacity fixture, then ran a normal `max_crypto` full JCVM cycle to clear
volatile RAM while preserving storage. GPPro created a second instance of the
same local applet with a distinct instance AID. The encrypted E6 INSTALL APDU
returned `9000` in 2.950 s; the complete GPPro operation took 4.155 s. Serial
showed the overlay at 94,479 bytes and repeated compaction records, then the
matching E6 response. No `USB_PROGRESS` time extension, `PANIC`, `FAULT`, or
`TEST:FAIL` appeared. Cleanup deleted the second instance, first instance and
local package; the unchanged JCAlgTest package remains installed. Firmware UF2
SHA-256 `9baf1cfcda50bfcc44cb4639fd83bf77ec0b087ca752c411a92e58f906d63e9d`;
fixture CAP SHA-256
`01cc0fefe6edc03a0a3e398fd2f5ab444cb5f42d6cf93c8030d7b9834696b6a1`.
Artifacts: `artifacts/results/filesystem-capacity-physical-20261117T175220Z/`
and `artifacts/results/filesystem-capacity-install-20261117T1757Z/`.

Source trace: card-manager E6 dispatch in `src/ccid_device.cpp` does not use
`NativeOperationScope`; it synchronously calls
`CapLoadSession::installAndMakeSelectable()`, including the applet install
method. Successful persistent installation commits the filesystem journal,
publishes CAP owner metadata through `cap_store.cpp`'s separate
`flash_safe_execute(..., 5000)`, then commits another filesystem snapshot to
clear the journal. The observed E6 is below the 15 s watchdog, but this path
has no cooperative CCID progress service and remains an audit candidate for
applets with expensive install methods or larger state.

## GPPro CAP inspection still initializes PC/SC — 2026-11-17

Invoking `gp.jar --cap <file> --info` in the restricted shell unexpectedly
attempted `SCardEstablishContext` and failed with `SCARD_E_NO_SERVICE`, before
reading card state or issuing APDUs. Do not use GPPro for offline CAP inspection
in a restricted context; inspect CAP ZIP/component contents with non-PCSC archive
utilities. If a GPPro operation might initialize SmartcardIO, use the approved
host-access execution path from its first attempt.


## Near-capacity GlobalPlatform LOAD timing — 2026-11-17

On F42DD9D07D6D2B07, recreated the near-capacity persistent overlay (94,479
bytes / 160 files, 3,820 bytes persistent free), then ran a normal full-JCVM
cycle to reset volatile RAM. GPPro loaded the unchanged supplied
`AlgTestApiCore_305.cap` (SHA-256
`9c6b348ef1206c45459bfe0899fcc74f755f7a946fc71f65508dd94dfb13026e`,
70,253-byte archive). It sent 42 E8 blocks; the final E8 block returned
`9000` in 2.534 s. The full GPPro LOAD operation took 2.803 s, returned status
0, and GET STATUS/list showed package AID `4A43416C6754657374417069` loaded.
The finite serial log showed `FS_COMPACT` at 94,479 bytes and matching E8
responses, with no `USB_PROGRESS`, `PANIC`, `FAULT`, or `TEST:FAIL`. Cleanup
removed the supplied API package and local filler instance/package; only the
unchanged JCAlgTest package remained. Firmware UF2 SHA-256
`a221bd40b8ed0059d934a25451988862061b23fcdfee1cf096f77e137a5930d6`.
Artifacts: `artifacts/results/filesystem-capacity-physical-20261117T180244Z/`
and `artifacts/results/filesystem-capacity-load-20261117T1807Z/`.
`./tools/fixture-path --verify` passed (83 files, JCAlgTest 1.8.4 CAP baseline,
48 Java Card API CAPs).

Source trace: only the final E8 block calls `finishLoad()`; card-manager E8
dispatch is synchronous and not wrapped in `NativeOperationScope`. It writes
the package CAP slot through `choupi_store_cap_package()`'s `flash_safe_execute`
callback, then commits a separate full filesystem snapshot to enable the
package. At this state the observed operation remained well below the 15 s
watchdog. Keep the earlier Windows timing discrepancy and interrupted lifecycle
recovery paths separate from this successful current-image result.


## API CAP package persistence across full JCVM cycle — 2026-11-17

On F42DD9D07D6D2B07, loaded the unchanged `AlgTestApiCore_305.cap` on the
normal `max_crypto` image (CAP SHA-256
`9c6b348ef1206c45459bfe0899fcc74f755f7a946fc71f65508dd94dfb13026e`). Then
ran `./tools/cycle choupi --profile max_crypto --serial F42DD9D07D6D2B07`; it
passed and preserved card storage. After helper refresh and exact reader
enumeration, GPPro GET STATUS/list showed package
`4A43416C6754657374417069` still loaded. Authenticated package E4 DELETE
returned `9000` in 881 ms. Final GPPro list contained only the pre-existing
JCAlgTest package. The finite cycle/serial and operation logs are under
`artifacts/results/cap-lifecycle-cycle-20261117T1813Z/F42DD9D07D6D2B07/`;
cycle log: `artifacts/logs/cycle-choupi-20261117T181422Z.log`.

This is package-level persistence over a normal full-JCVM cycle. It does not
exercise reset/power loss between install-journal publication, CAP owner-map
update and journal clear, so interrupted lifecycle recovery remains unverified.


## Install journal VPCD recovery expectation — 2026-11-17

The first local journal-recovery test failed before restart because
`tests/integration/pc_vpcd_applet_test.py` expected `6A82` after deliberately
skipping recovery once. `vpcd_restore_applet()` returns `RestoreStatus::Failed`
for that injection, which correctly maps to `6F00`; `6A82` means the applet is
not registered and was the wrong assertion. The test now expects fail-closed
`6F00`, terminates the emulator, then validates fresh-process recovery and
SELECT `9000`. No VM source change was needed.

All four bounded PC/VPCD cases pass with the local capacity applet (not the
JCAlgTest host client): pending journal before CAP registry publication,
pending journal after publication, metadata publication error, and journal-clear
sync failure. Each fresh process recovered and selected the applet. Results are
in `artifacts/logs/filesystem-install-recovery-local-20261117T1823Z.log`,
`artifacts/logs/filesystem-install-publication-recovery-local-20261117T1823Z.log`,
`artifacts/logs/filesystem-install-metadata-recovery-local-20261117T1823Z.log`,
and `artifacts/logs/filesystem-install-clear-recovery-local-20261117T1823Z.log`.
The unprivileged run returned `Operation not permitted` before creating a VPCD
log because the sandbox blocked loopback bind; elevated execution permitted the
local socket. This test path uses no USB or PC/SC.


## Interactive JCAlgTest runner stalls at menu under GNU timeout — 2026-11-17

When `tools/run-jcalgtest` was launched interactively with an exact reader, its
TTY menu printed but input was not consumed. The Java client entered stopped
state (`STAT=Tl`) when Scanner read from the terminal because GNU `timeout`
placed it outside the foreground process group. No test option had started.

**Viable fix.** In the TTY-preserving branch only, use
`timeout --foreground --signal=TERM --kill-after=5s`. This leaves the client in
the terminal's foreground group while retaining the finite suite timeout. The
change in `tools/run-jcalgtest` passed `bash -n`; the physical 1.8.5 client then
accepted the main menu choice `2`, the performance submenu choice `2`, and
started APDU measurements on the exact selected reader. Avoid sending scripted
answers in advance: separate Scanner instances can buffer later lines.

The initial stopped prompt was terminated before any performance option ran;
its partial log is kept separate from the active attempt under
`artifacts/results/option2-acceptance-20261117T1830Z/F42DD9D07D6D2B07/`.

## JCAlgTest watchdog must route reader selection by prompt — 2026-11-17

With multiple PC/SC readers, `tools/run-jcalgtest` resolves the supplied exact
reader name (including the chip ID) to the current SunPCSC reader slot. The
watchdog previously queued that slot before scripted main-menu inputs, although
the client asks for the main-menu choice first. This shifted every answer: an
intended option-2 run selected option 1 and, after another shifted answer,
connected to F42D instead of the requested 07E7.

**Viable fix.** Pass the identity-resolved slot separately as `--reader-index`
to `jcalgtest-apdu-watchdog.py`. The watchdog answers it only when the exact
reader-selection prompt appears; `--input-line` remains in order for menu
prompts. The physical confirmation log at
`artifacts/results/option2-acceptance-20261117T2113Z/07E74C5D7A038DEC/attempt3/client.log`
shows menu 2, reader 1, and the selected 07E7 card before performance submenu
2. The run ended early and is not acceptance. Use a fresh empty-card start for
the next acceptance run and verify the `Used reader` identity before counting
any results. `bash -n`, Python bytecode compilation, and `git diff --check`
passed for the wrapper change.

## Rising resource use in the full physical JCAlgTest option-2 run — 2026-11-18

A complete unchanged-client physical option-2 run on empty-card normal-image
Pico 2 `07E74C5D7A038DEC` completed in 4,660 seconds. Its serial log recorded
2,951 `OBJECT_DELETION:PRE_CLEANUP` samples. The headroom gate passed with a
peak of 689 files / 14,601 of 98,304 bytes (14.852%, against a 90% threshold)
and 24 explicitly logged `LOW_WATERMARK_COMPACT` events (all `status=0`). There were 4,826 captured APDU responses, all `9000`, and
no `TEST:FAIL`, `FAULT`, `PANIC` or `HEADROOM:FAIL`. These measurements show
bounded arena use over this campaign; they do not prove all future applet object
patterns plateau, and the run remains exploratory until T7 VPCD acceptance
passes. The client shell exit status was unavailable after its process handle
expired; the explicit `Testing complete.` marker, total time and final result
files are preserved. The parent identity file names an older firmware because
an intervening cycle rebuilt/flashed the latest image; use the per-attempt
`campaign-identity.txt` for actual campaign provenance.

Artifacts: `artifacts/results/option2-acceptance-20261117T2113Z/07E74C5D7A038DEC/attempt5/`;
headroom summary from `attempt4/serial.log`.


## Fixed-image physical option-2 completion and long AES enumeration APDU — 2026-11-18

The fixed normal `max_crypto` image (`853543d6`, deletion ON/debug OFF) completed
the unchanged-client option-2 performance run on chip `07E74C5D7A038DEC` after
resuming with the same-cardname `already_measured.list`. The 1.8.5 JAR hash was
`b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`; the
unchanged 1.8.4/jc305 CAP hash was
`cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`. The
client reported `Testing complete`, 610 seconds for the resumed portion, zero
errors, zero reconnects and zero human resets. Combined serial telemetry passes
the 90% headroom gate with 2,953 samples and peak use of 707 files / 15,148 of
98,304 bytes (15.409%). Over the last 100 samples, live arrays stayed at 115,
instances varied between 3 and 30, and raw storage alternated between 398 files
/ 11,379 bytes and 554 files / 12,527 bytes. No continued array growth was
observed after family setup. This is exploratory physical evidence, not T8
acceptance until T7 VPCD passes.

Several repeated `B0 A5 00 00 16` calls in the `SWALGS SWAlgs_AES()` portion
took 117.192–117.288 seconds and returned `9000`; serial telemetry showed CCID
time extensions (105 per observed long call) and the card stayed connected.
This was a legitimate long synchronous APDU, not a transport failure. A client
log line with no response for roughly two minutes must be correlated with the
complete serial capture before stopping the campaign. Keep the suite timeout at
the planned bound and do not use an APDU timeout shorter than this measured
latency. Artifacts: `artifacts/results/option2-exploratory-20261118T1049Z/07E74C5D7A038DEC/`.

## PC/SC helper refresh interrupts a live JCAlgTest run; VPCD reader is unavailable — 2026-11-18

The authorized `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper restarts
`pcscd.service`. Running it while option 1 was active on F42D interrupted the
client at a cleanup boundary: `SCARD_F_COMM_ERROR`, followed by
`SCARD_E_NO_SERVICE`, then an uncaught `NoSuchElementException` after the
client's reconnect/retry prompt. No firmware fault or memory failure appeared.

**Viable control (updated 2026-11-18, user directive):** do not refresh PC/SC
or list readers unless necessary.
- The helper source ends with an unconditional `systemctl restart
  pcscd.service`. On Debian pcscd runs as root, so the ACL step is skipped and
  the restart is its only effect.
- Never run the helper while any campaign is live (check
  `pgrep -fa 'AlgTestJClient|jcalgtest-apdu-watchdog|tools/monitor'`).
- Use `tools/list-readers` only when a reader name must be confirmed.
- Identify tokens with `lsusb -v -d 2e8a:0009` (`iSerial` = chip ID).
- pcscd hotplug picked up re-enumerated 07E7 after each reflash on 2026-11-18
  without the helper. Exception still open: F42D was visible on USB but not in
  PC/SC. If a session is interrupted, preserve its
`already_measured.list`, result and serial logs, diagnose exact token availability,
and resume on that same token when available. If it is unavailable, record the
switch and use another empty-card token.

The unchanged-client VPCD runner also failed before client startup because
bounded reader discovery listed only Pico tokens, not `Virtual PCD 00 00`. This
environment currently has no VPCD driver or `vpcd`/`vicc` executable in PATH or
`/usr/lib/pcsc`. The emulator wrapper cannot publish a SmartcardIO reader by
itself. Restore the host VPCD PC/SC service before retrying T7; see external
request `HOST-VPCD-PCSC-01`.

Evidence: `artifacts/results/option1-acceptance-20261117T2257Z/F42DD9D07D6D2B07/attempt1/client-output/ALGTEST_log_1794956298016.log`;
`artifacts/results/jcalgtest-support-vpcd-20261117T230259Z/reader-inventory-*.log`.


## Physical option-1 diagnostics overlap and invalidate telemetry gate — 2026-11-18

During the active 07E7 option-1 run, the original raw serial log contains
interleaved firmware records. For example, a `HEAP_GC` line ends with
`instances_live=OUPI:FS_COMPACT`, and the next line begins with a truncated
`LETION:PRE_CLEANUP`; another `CCID_COMMANDS` record overlaps `RSA_MEMORY`.
`tools/check-object-deletion-headroom.py` rejects three malformed PRE_CLEANUP
records (log lines 9848, 9982 and 16502). Since this reproduces in the live original,
it is not a snapshot-copy race. The current run cannot satisfy T10 even though
the complete parseable samples currently peak at 31,286/98,304 bytes (31.83%).

**Viable control:** none established yet. Preserve the client/serial logs and
finish the active campaign without restarting PC/SC. Before counting a later
run for T10, find and serialize all firmware diagnostic output paths (including
runtime cleanup, filesystem compaction, USB progress and CCID status), then
build/cycle and verify that telemetry lines remain whole under USB traffic.
Rerun the fail-closed gate on the entire capture. The client has four `SW=F105`
`CryptoException_ILLEGAL_USE` RSA Cipher results in this campaign; review them
against the support baseline separately from transport or memory failures. At
265 method groups, complete HEAP_GC records reached 201 live arrays / 72 live
instances; the final 100 complete samples still vary across 153–201 arrays and
48–72 instances as families continue loading. This is not yet a post-suite
plateau measurement.

Evidence: `artifacts/results/option1-acceptance-20261118T0009Z/07E74C5D7A038DEC/attempt4/serial.log` and `client.log`.


## JCAlgTest option 1 can read a stale working-directory resume list — 2026-11-18

The long all-algorithms performance path is top-level menu `2` then submenu
`1`; top-level `1` only runs the short supported-algorithms probe. Also, the
client looks for `*_already_measured.list` in its working directory and prompts
to reuse it. A stale list in the repository root caused a fresh client run to
exit with `NoSuchElementException` at that prompt, before performance work.

**Viable control:** run each client attempt from its own clean result directory,
use a unique `-cardname`, set `JCALGTEST_STDIN_LINES=$'2\n1'`, and confirm the
client log reports both prompt selections and the exact selected chip ID before
counting the campaign. Preserve any root-level list/log generated by a failed
attempt under that attempt's result directory; do not delete or reuse another
campaign's list.

Current clean failover attempt is `artifacts/results/option1-acceptance-20261118T0009Z/07E74C5D7A038DEC/attempt4/`.


### Option-2 retained-storage plateau and compaction-count correction — 2026-11-18

A fixed-window reanalysis of the complete 2,951-sample physical option-2
serial trace confirms the late-run retained-record/byte envelope is stable
after initial algorithm-family setup. Samples 1,501–2,000 averaged 513.5 files /
12,045 B; 2,001–2,500 averaged 510.9 files / 11,983 B; 2,501–2,951 averaged
501.8 files / 12,017 B. The window end values were respectively 546/12,179,
395/11,261 and 499/12,125. The total peak (689 files / 14,601 B) occurred in
the first of those late windows; the later two peaks were 660/13,451 and
661/13,433. This shows bounded sawtooth behavior over the measured campaign,
not monotonic growth; it does not establish all possible applet workloads.

The same 2,951-sample trace has 2,951 parseable `HEAP_GC` samples. Live arrays
rose from 30 at startup to 115 while new algorithm families were initialized;
the final 100 samples are exactly 115 arrays / 30 instances each. Thus the
initial increase is bounded and the final repeated cleanup window is stable.
This is a count of Java heap objects, not a byte-level RAM high-water measure.

Correction: the serial log contains **24**, not 25, explicit
`OBJECT_DELETION:LOW_WATERMARK_COMPACT` records; all 24 report `status=0`.
Earlier notes counted one extra. The campaign still has 2,951 telemetry
samples and passes the 90% headroom gate.

Source: `artifacts/results/option2-acceptance-20261117T2113Z/07E74C5D7A038DEC/attempt4/serial.log`.


### Early option-1 heap and storage checkpoint — 2026-11-18

A stable copy of the active 07E7 option-1 telemetry after 92 method groups
contains 329 parseable heap-GC samples. `arrays_live` rose from 95 to 103 in
initial setup, then remained exactly 103 over the last 100 cleanup samples;
`instances_live` alternated between 2 and 22 in that tail. Of 370 pre-cleanup
samples in the snapshot, storage peaked at 616 files / 12,613 of 98,304 bytes
(12.830%); the last 100 samples ranged 332–599 files and 9,943–12,501 bytes.
No error/fault marker was present. This is an early exploratory checkpoint, not
a full-run plateau or acceptance result; more algorithm families remain.

Snapshot was copied to `analysis-snapshot-20261118T0029CET-serial.log` and
`analysis-snapshot-20261118T0029CET-client.log`; live originals remain in
`artifacts/results/option1-acceptance-20261118T0009Z/07E74C5D7A038DEC/attempt4/`.


### Option-1 follow-up heap reuse checkpoint — 2026-11-18 01:40 CET

At 276 method groups, strictly complete records contain 643 HEAP_GC samples and
944 PRE_CLEANUP samples. Live counts reached 213 arrays / 78 instances as new
families initialized. The final 100 complete HEAP_GC samples include five
family levels (165/54 through 213/78), and repeated cleanup samples at the
latest level remain stable. This supports reuse within the current family; it
does not establish a plateau after the full option-1 suite. Storage peaked at
1,064 files / 35,520 of 98,304 bytes (36.13%); the last 100 samples range
740–1,064 files / 25,218–35,520 bytes. The fail-closed headroom check rejects
three malformed PRE_CLEANUP lines elsewhere in the original serial capture, so
this campaign remains invalid for T10. It has six `CryptoException_ILLEGAL_USE`
and two `CryptoException_ILLEGAL_VALUE` records so far; review the completed
results against the support baseline.


### Option-1 sustained family-level heap checkpoint — 2026-11-18 02:26 CET

At 304 method groups, strict whole-line parsing gives 686 HEAP_GC samples and
1,007 PRE_CLEANUP samples. The final 100 HEAP_GC records span five successive
family-level counts from 177 arrays / 60 instances to 225 / 84, with repeated
cleanup records at 225 / 84. This is materially stronger evidence that the
heap count is reused within a family while new families add retained objects;
the full option-1 suite has not completed, so do not call it a final plateau.
Storage has a 39,789/98,304-byte peak (40.48%, 1,097 files); the last 100
samples range 30,840–39,789 bytes. It is still rising gradually. T10 remains
unusable on this run due to malformed serial output at lines 9848, 9982 and
16502.

### Option-1 persistent-store growth despite stable heap count — 2026-11-18 02:35 CET

At 317 method groups, the last 50 strictly complete HEAP_GC records remain
225 live arrays / 84 live instances. Yet the last 300 complete PRE_CLEANUP
samples show store usage rising from 612 to 1,016 files and 17,374 to 38,809
bytes, with several compaction drops between those endpoints. The observed peak
is 1,137 files / 40,137 of 98,304 bytes (40.83%). Thus heap object reuse does
not imply persistent-store plateau; continue the run and determine whether the
store plateaus after the full algorithm suite or continues toward the T10 gate.
The three malformed serial records (lines 9848, 9982, 16502) still invalidate
this campaign for T10.

At 329 method groups, a fourth malformed PRE_CLEANUP record appeared at line
25115. Its expected `available=... files=... bytes=.../98304` telemetry was
replaced by `free=... end_free=... delta=... samples=... elapsed_us=...`, fields
from `RSA_MEMORY`. This confirms multiple firmware diagnostics interleave in
the original serial stream. The current run has 753 strictly complete heap
records whose last 50 remain 225 arrays / 84 instances, while the last 300
complete storage samples rose 804→918 files and 20,804→37,953 bytes with
compaction drops. Global peak is 40,137/98,304 bytes (40.83%).

At 395 method-name records (2026-11-18 04:12 CET), the same run has 1,327
strictly complete PRE_CLEANUP records and 921 complete HEAP_GC records. The
last 50 heap samples remain 225 arrays / 84 instances. Persistent storage
continues to rise between cleanup batches, but cleanup produces large drops:
the last 100 complete samples moved 1,039→897 files and 39,001→38,049 bytes;
the last 300 ended at 897 files / 38,049 bytes from 916 / 37,939. The peak is
still 1,139 files / 40,137 of 98,304 bytes (40.83%). Six malformed
PRE_CLEANUP lines now invalidate T10 (9848, 9982, 16502, 25115, 26890,
27673); an earlier partial OBJECT_DELETION record at line 1156 is also
interleaved with exception diagnostics. These results show reclamation during
the run but do not prove a final persistent-store plateau or acceptance.


## RSA-cache object-deletion pass starved the watchdog — fixed and A/B verified 2026-11-18

The old physical option-1 run on 07E7 failed at `TYPE_RSA_CRT_PRIVATE` 2048
NOPAD, on a `B0E2` request. Its serial capture ends inside the deletion pass and
the board re-enumerated itself. This reproduces the Windows reader-loss point:
the full object-deletion scan had no watchdog refresh, USB service or CCID time
extension. It is a long-operation responsiveness failure, not persistent-store
exhaustion.

**Viable fix:** commits `25c39cd7` and `853543d6` build a bounded 24 KiB sorted
index of marked persistent tags (linear fallback if the target RAM margin is
not available), reuse complete stable-root traces within one deletion batch,
and service watchdog/USB/CCID progress from the long scans. The automatic
post-APDU heap GC now runs only when the applet requested deletion. On 07E7 at
the reproduced cache state, A/B changed B0E2 from a 14.87 s worst measured old
pass / 10.2 s repeat to 3.05 s, with `OBJECT_DELETION:TIMING` at 2.18–2.54 s,
about 41 progress calls and two completed CCID extensions per pass. The new
image prepared and measured CRT-3072 beyond the old failure point; every probe
APDU returned `9000`, with no PANIC/FAULT/watchdog reboot. The index peaked at
21,870 B, close to its cap; larger indexes safely fall back to the slower
linear walk. This fixes the observed watchdog boundary; it does not prove the
full all-algorithm campaign or storage plateau.

A new normal-image exploratory option-2 run is now active on a freshly wiped
07E7 with unchanged 1.8.4/jc305 CAP and pinned 1.8.5 client. At 62 method
groups / 445 cleanup samples, storage reached 607 files / 12,517 of 98,304 B
(12.73%) and then dropped to 378 / 10,466. Live heap rose from 30 arrays / 16
instances to 103 / 22 during family setup and was stable at 103 / 22 over the
last 100 samples. This is an early checkpoint, not a suite-wide plateau or
acceptance. Do not list readers or restart PC/SC while the run is live.

Artifact: `artifacts/results/option2-exploratory-20261118T1049Z/07E74C5D7A038DEC/attempt1/`.
