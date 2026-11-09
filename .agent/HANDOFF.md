# Current handoff — RESOURCE-01 storage exhaustion and deletion probe — 2026-11-09 02:50 CET

The unexpected `2e8a:0009` PacketRF unit is excluded from all physical tests.
No hardware was accessed during this latest run.

An unchanged JCAlgTest 1.8.4/jc305 PC VPCD run with periodic inventory is at
`artifacts/results/jcalgtest-support-vpcd-20261109T010939Z/`. It reached RSA
KeyPair probing with inventory at 1,691 owner records / 43,697 payload bytes;
global 1,825 / 46,829; 1,485 unreachable tags / 39,293 bytes. RSA-768 reported
`SystemException_NO_RESOURCE`; RSA-896, 1024 and 1280 succeeded; RSA-1536
returned `F205` after 44.678 seconds. Persistent serialization failed writing
45 bytes for tag
`03013894FFFFFE000000420303FFFE000000470606FFFE000002F30101FFFE000002F80303`.
Afterward I sent Ctrl-C to the still-running test session; that caused the host
emulator's ptrace wrapper panic and empty APDU. The F205 preceded the
interruption and is valid PC-backend exhaustion evidence; the panic is not a
product failure or Pico reset. The official support comparison did not
complete.

A second unchanged-client run with the PC-only deletion request probe reached
DES CBC and was also interrupted by my Ctrl-C. It is incomplete. The bounded
control `PICO2_HOST_USB_ACCESS=1 ./tools/test-object-deletion-vpcd 8 rsa-public`
passed 19/19; eight request cycles erased 80 applet-owned records (10 each).
Log: `artifacts/logs/object-deletion-vpcd-20261109T014621Z.log`, SHA-256
`47b5265a5f02338d9e5d6e5e1b2c408d5a80ccae58e1ead8f802580bdbd51db5`.
This remains PC VPCD test-gate evidence, not firmware support or physical
acceptance.

RESOURCE-01 remains unresolved. Normal persistent reclamation is disabled.
Next: reproduce the 45-byte persistent-reference failure with a bounded client
workload and inspect backend storage capacity accounting. Then finish
PERSIST-01 root/alias, multi-owner, transaction and physical power-cut gates
before enabling any production collector. Allow long client runs to terminate
without sending Ctrl-C. Do not use PacketRF. See newest RESOURCE-01 entries in
`.agent/QKB.md` and `progress.md`.

# Current handoff — filter unrelated PacketRF from the PC/SC helper — 2026-11-08 11:18 CET

The user confirmed the fourth connected `2e8a:0009` device is an unexpected
PacketRF device and must not be used. Updated `tools/pico2-pcsc-reader-root`
to require VID/PID plus sysfs manufacturer `Pico2` and product
`Pico2 JCVM CCID`; `bash -n` passes. Host inspection finds three matching Pico2
boards at paths 1-2, 1-3, 1-4, and PacketRF at 1-5. The installed
`/usr/local/sbin/pico2-pcsc-reader` remains old and still counts all four, so it
must be refreshed by running `sudo ./tools/install-pcsc-sudoers` before the
next reader check. This is a repository helper fix; PacketRF was not accessed.

# Current handoff — distinguish RAM GC from persistent-resource growth — 2026-11-08 11:28 CET

Confirmed the runtime path: Pico invokes RAM heap collection after every
completed APDU; the production persistent reclaimer remains disabled. On PC
VPCD, `PICO2_HEAP_GC=1` enables equivalent per-APDU RAM collection only. Five
repeated RSA-1024 public KeyBuilder APDUs under this diagnostic reclaimed 3
arrays and 1 instance per request, while inventory reached 345 global records /
8,889 payload bytes, including 41 unreachable tags / 1,617 bytes. A subsequent
RSA-1024 KeyPair APDU passed. Thus RAM handle reclamation does not address the
persistent orphan records. This short sequence does not reproduce the full
1.8.4 client's empty-response failure. Repeated `F206 exception=16 opcode=93`
is consistent with the known unsupported `requestObjectDeletion()` behavior,
but its connection to the empty response is unproven.

Next: continue RESOURCE-01 and PERSIST-01 without enabling persistent deletion
by default. Instrument the failing full-run boundary and implement deferred
deletion only behind the existing safety requirements. Physical testing remains
blocked until the updated Pico-only helper is installed; do not access PacketRF.
The latest helper source fix is committed (`f552b6e8`), and the user must run
`sudo ./tools/install-pcsc-sudoers` before another physical reader check.
The RAM/persistent-resource diagnosis and this handoff update are recorded in
commit `0bf69acd`.

# Current handoff — PC VPCD matches Pico RAM collection cadence — 2026-11-08 12:21 CET

PC VPCD now performs RAM collection after each APDU for the selected applet,
rotates one inactive context every 32 APDUs, and reclaims transient global
arrays each APDU, matching the Pico runtime. `PICO2_HEAP_GC=1` remains an
additional all-context diagnostic with telemetry. Persistent record deletion
is still disabled. The earlier 5-call RSA Builder run (when PC collection was
opt-in) remains evidence that RAM GC does not reclaim serialized persistent
records.

Validation: PC, ARM max_crypto, and RISC-V max_crypto repository builds pass.
The unchanged JCAlgTest 1.8.4 VPCD smoke passes 30/30 across two passes; a
clean no-inventory pass passes 15/15 in 83.084 s. The VPCD multipackage suite
passes 47/47 with a 300-second APDU deadline. The default 90-second run timed
out only while deleting the 18-instance package; it passed with more time. The
suite also exposed stale inventory regex assertions, corrected in its test.
No hardware was accessed, and PacketRF remains excluded. Changes are committed
as recorded in the latest `progress.md` entry.

Next: continue RESOURCE-01 from deferred JCSystem deletion and full
1.8.4 KeyPair failure diagnosis. Keep PERSIST-01 physical multi-owner and
interruption-safety gates open. Hardware reader validation remains pending the
user running `sudo ./tools/install-pcsc-sudoers` to install the Pico-only helper.
Collector-parity implementation and validation are committed as `a01808de`.

# Current handoff — key resource accumulation confirmed; full run still fails — 2026-11-08 11:13 CET

# Current handoff — RAM-GC parity did not fix persistent exhaustion — 2026-11-08 12:55 CET

The unchanged JCAlgTest 1.8.4/jc305 full support run was repeated after PC VPCD
was changed to match firmware RAM-GC cadence. It still failed while generating
an RSA-768 key pair: APDU `B0 75 19 00 03 01 03 00 00` returned
`F205` / `SystemException_NO_RESOURCE` after 442,532 ms. The emulator logged
`PERSIST_EXCEPTION id=20` on persistent writes (the filesystem OutOfFlash
mapping). A following KeyPair APDU received an empty response and the client
terminated with `IllegalArgumentException: apdu must be at least 2 bytes long`.
Artifacts:
`artifacts/results/jcalgtest-support-vpcd-20261108T112336Z/`.

This was VPCD only. It does not establish physical token behavior or a physical
reset cause. RAM-GC parity did not fix the long-run resource failure. Persistent
object reclamation is still disabled; do not enable it until the PERSIST-01
multi-owner, transaction, alias and interruption-safety gates pass. Next inspect
the GC/reclaimer roots and existing interruption tests, then implement a
context-scoped deferred `requestObjectDeletion()` service at the safe post-APDU
boundary, starting with focused tests and keeping production reclamation
disabled until all safety gates pass. Preserve unrelated dirty worktree files.

# Current handoff — persistent GC experiment exposed root-coverage gap — 2026-11-08 13:45 CET

RESOURCE-01 remains unresolved. The full unchanged JCAlgTest 1.8.4/jc305 VPCD
run failed at RSA-768 with `F205` after 442,532 ms and logged persistent
OutOfFlash (`PERSIST_EXCEPTION id=20`). RAM-GC parity did not fix it.

A temporary, host-only experiment invoked the existing persistent reclaimer
after each VPCD APDU. It failed the multipackage 47-check suite at
`SELECT lookup applet after card reset` (`6A82`) after one or more owner records
had been erased. The exact deleted tag was not logged. The hook and all
experimental source changes were removed. The same current-worktree suite with
the hook off passed 47/47, and `./tools/test-persistent-transaction` passed
with `fail=0`. This is a confirmed safety gap in the current root coverage for
routine per-APDU reclamation; don't wire the primitive in or report object
deletion support until exact tags and inactive/native reference roots are
covered. Logs:
`artifacts/logs/host-vpcd-multipackage-apdu-20261108T122812Z.log` (failure) and
`artifacts/logs/host-vpcd-multipackage-apdu-20261108T123244Z.log` (control).

The approved Pico helper found zero `2e8a:0009` devices on refresh, so all
validation remained on VPCD/host-model. Next add tag-level reclaimer telemetry
and a regression fixture for retained-context/cross-owner aliases, then fix
root enumeration before retrying the diagnostic. Preserve all unrelated dirty
files.

The latest full unchanged JCAlgTest 1.8.4 VPCD run passed the algorithm and
KeyBuilder checks through EC transient reset/deselect, then entered KeyPair.
RSA 512/736 passed; RSA 1024 returned an empty APDU. The CSV has only 196 rows
(134 yes, 62 no); 25 of 159 expected capability identities were not emitted
before the client failed. This is not a complete support acceptance.

Fresh-card isolation: RSA 1024 KeyPair APDU
`B0 75 19 00 03 01 03 00 00` passes in 49.281 s with a 120 s timeout. A
separate ten-call sequence of RSA 1024 public KeyBuilder APDUs also passed, but
left 91 unreachable persistent tags / 3,622 payload bytes; global inventory
was 395 active records / 10,894 serialized bytes. No persistent records were
reclaimed. This confirms orphan accumulation and supports the RESOURCE-01
lead, but the full-run empty APDU cause is not yet proven. Keep reclaimer
disabled pending PERSIST-01 safety acceptance.

The EC redundant-zero-write optimization is committed as `54eb08c2`; PC, ARM
and RISC-V max_crypto builds passed. Its full-suite validation remains
incomplete. Logs and more exact observations are in `.agent/QKB.md` and the
latest progress entry.

Physical status: the access helper now refuses because it counts four devices
with VID:PID `2e8a:0009`, one of which enumerates as PacketRF; `lsusb` shows
three Pico2 JCVM devices plus that PacketRF device. `pcsc_scan -r` sees only
Virtual PCD 00 00/01. No physical test/flash was performed. Continue offline;
resolve VID:PID filtering in the host helper before retrying board access.

# Current handoff — JCAlgTest EC KeyBuilder latency investigation — 2026-11-08 08:41 CET

The active priority remains RESOURCE-01. A full unchanged JCAlgTest 1.8.4
VPCD support run installed the official CAP, passed RSA key-builder rows through
4096-bit CRT and EC private rows through 640-bit, then failed on the next APDU
with `SCARD_F_COMM_ERROR`. The trace files are under
`artifacts/results/jcalgtest-support-vpcd-20261108T071233Z/`. It is not a
complete support comparison. The emulator also logged repeated F206 exceptions
on deselect/reset commands; causality is not established.

The failing next APDU (`B0 75 20 00 03 1E 01 00`, EC-FP private transient-reset
KeyBuilder) completes on a fresh VPCD card with an extended host-test timeout:
31.343 s in one run and 41.289 s on a later run; a repeated call in the latter
run took 4.349 s. It exceeds the focused harness's 20 s default timeout. A
384-bit persistent EC key builder APDU completed in 38.020 s. These are host
VPCD timings, not Pico timings, and were not controlled benchmarks.

Removed redundant EC component byte-zero loops in `choupi/src/native_key.cpp`;
`JCVMArray` already value-initializes backing arrays. This eliminates needless
per-byte writes for persistent EC keys. PC, ARM and RISC-V max_crypto wrapper
builds pass, and the 384-bit and 1e transient APDUs complete under 90/50-second
timeouts. Full JCAlgTest suite still failed with `SCARD_F_COMM_ERROR`; investigate
slow first-use and filesystem paths before claiming acceptance. The 60.564 s
earlier full-suite 384-bit row is suggestive only, not a matched baseline.

The user reports all three physical tokens present. The authorized access helper
granted three USB nodes and `lsusb -d 2e8a:0009` saw three devices, but a fresh
`pcsc_scan -r` listed only Virtual PCD 00 00/01. Do not run physical APDUs or
flash until a Pico reader appears; continue offline meanwhile. Preserve other
pre-existing dirty files. Implementation and test logs from this step are
recorded in the latest progress entry and QKB section.

# Current handoff — persistent resource accounting diagnostics — 2026-11-08 08:05 CET

Extended the opt-in PC VPCD `PICO2_PERSISTENT_INVENTORY=1` hook to count
active records and serialized payload bytes across the full filesystem, in
addition to applet-owner unreachable records and heap handles. The owner scan
alone missed the RSA key graph; the global scan exposed it. With three retained
512-bit key types, observed active-record/payload totals were baseline `6/34`,
after public `17/264`, after plain private `27/483`, and after CRT private
`43/800`. Each checkpoint includes test/CAP/APDU records and is not an isolated
per-key or physical flash delta. The focused three-key VPCD run passes 5/5;
ARM, RISC-V and PC max_crypto builds pass. Log:
`artifacts/logs/resource-accounting-rsa-vpcd-newdiag.log`. Commit: `04c862a9`.

No physical reader is PC/SC-visible: the helper grants three USB nodes, but
current `pcsc_scan -r` output exposes only Virtual PCD. Continue offline.

# Current handoff — RSA key component allocation optimization — 2026-11-08 07:52 CET

RESOURCE-01 implementation stage: `buildNativeKey()` now allocates only
components required by the RSA type. Public/plain private RSA allocate
modulus/exponent; CRT private RSA allocates P/Q/DP1/DQ1/PQ. `clearKey()` skips
the absent fields. Both ARM and RISC-V max_crypto builds passed. The rebuilt
max_crypto VPCD suite passed 102 checks, including 3 new public/private/CRT
component cases and 16 transient AES/RSA lifecycle cases. Log:
`artifacts/logs/symm-crypto-kat-vpcd-20261108T064804Z.log`.

At 3072 bits, the structure saves 960 array payload bytes per public/plain
private key and 768 bytes per CRT key; a public+CRT pair avoids 1,728 bytes and
seven persistent array records before metadata. This is a capacity calculation,
not a measured physical flash/resource delta. A separate local 512-bit CRT
KeyPair sign/verify diagnostic returned a generic test failure after key
generation and component checks; its cause is unknown and remains open.

The authorized USB helper still grants three nodes, but the latest refresh
observed only two runtime Pico USB devices and `pcsc_scan -r` listed only the
two Virtual PCD readers. No physical APDU or flash was performed. Continue
RESOURCE-01 and retry hardware when a Pico reader appears.
Implementation commit: `508a043c`.

# Current handoff — VPCD crypto transient lifecycle regression — 2026-11-08 07:35 CET

Added VPCD lifecycle vectors for applet-static AES and RSA `Cipher` instances
backed by `CLEAR_ON_DESELECT` keys. The tests initialize a transient key and
Cipher, deselect to the card manager, reselect, verify use fails after the key
and native workspace clear, then repeat across a VPCD reset. AES and RSA
variants pass within the existing symmetric crypto VPCD suite. The test harness
now builds `max_crypto` because the RSA transient-key variant needs RSA support.
Validation log: `artifacts/logs/symm-crypto-kat-vpcd-20261108T062536Z.log`;
the run reported `TEST:SUMMARY pass=99 fail=0` and
`transient_lifecycle=16 reset_after=2 total=97 fail=0`. Commit: `7525b67b`.

This is VPCD evidence only; the physical PC/SC check after the user's latest
"tokens present" update refreshed the helper to three USB nodes, while a
separate `lsusb` snapshot showed two runtime `2e8a:0009` devices and
`pcsc_scan -r` listed only the two Virtual PCD readers. No physical APDU or
flash was attempted. Continue with the highest-priority RESOURCE-01 work and
retry physical tests when a Pico reader is actually published.

# Current handoff — saved RESOURCE-01 implementation plan — 2026-11-08 07:09 CET

Saved the proposed six-stage plan in docs/resource-01-implementation-plan.md
and linked it from RESOURCE-01 in TODO.md at the user's request. It covers
resource accounting, safe persistent reclamation, deferred JCSystem deletion,
replacement/failure cleanup, bounded key reuse and long-run/reset acceptance.
This is documentation only; implementation remains a separate task. Existing
PERSIST-01 physical safety gates and all prior handoff evidence remain in force.

# Current handoff — PERSIST-01 graph roots across overlay cuts — 2026-11-08 06:57 CET

Expanded the RP2350 overlay fake-flash power-cut scenario. It now constructs
and serializes a persistent `Object[]` root with a byte-array child, serializes
ten orphan byte arrays, and captures the candidate list from production
`PersistentInventory`. The production `PersistentReclaimer` deletes exactly
ten inventory tags. Across 1,000 injected COW snapshot cuts, each reboot
restores a fresh Heap and verifies the root graph and a neighboring owner's
record; old-snapshot outcomes are retried through the production reclaimer.
`./tools/test-rp2350-fs-overlay` passes:
`TEST:METRIC persistent_reclamation_orphan_tags=10 cuts=1000` and
`TEST:SUMMARY fail=0`. Log: `/tmp/rp2350-fs-overlay-root-graph.log`.

Host fake-flash only: physical multi-owner/interrupted erase and USB power-loss
acceptance remain open. Keep normal persistent reclamation disabled. The test
target now supplies inert CAP metadata stubs for its wildcard `Object[]` type
check; firmware sources were not changed, so ARM/RISC-V builds and hardware
tests were not run. Commit reference will be added in the progress follow-up.
Implementation and progress record: commit `4a1cc450`.

# Current handoff — three USB boards still absent from PC/SC — 2026-11-08 06:44 CET

The user reports all three tokens connected. I refreshed the authorized
`pico2-pcsc-reader` helper, which granted three USB nodes. `lsusb -d 2e8a:`
showed three runtime `2e8a:0009` devices with CDC and CCID interfaces. A bounded
`pcsc_scan -r`, including one run immediately after refreshing the helper,
listed only VPCD readers and no Pico reader. Device numbers changed between
separate helper/scan operations. No physical APDU, flash, or other card access
was attempted; do not infer a PCSC-usable reader from USB presence. Continue
offline or retry after reader enumeration returns. Details are in the
2026-11-08 QKB entry. Observation record commit: `2bb4f9cc`.

# Current handoff — user prioritizes long-run resource lifecycle — 2026-11-08 06:29 CET

Latest PERSIST-01 host validation: `tests/unit/rp2350_fs_overlay` links the
production persistent reclaimer, Heap and persistent-reference restore code.
Its 1,000-cut COW snapshot test stages ten unreachable owner tags through
`PersistentReclaimer`, recovers the filesystem after each injected cut,
restores the rooted array through a fresh Heap, and verifies that root plus a
second owner's record. If the old snapshot wins, retry also uses the production
reclaimer and confirms all ten candidates are removed. The
`./tools/test-rp2350-fs-overlay` run passes with `TEST:SUMMARY fail=0`; full log:
`/tmp/rp2350-fs-overlay-persist01.log`.
Implementation and progress record: commit `426d6030`.

This is host fake-flash evidence only. Physical multi-owner/interrupted erase
and actual USB power-loss acceptance remain open; keep normal persistent
reclamation disabled. ARM/RISC-V builds and physical tests were not run because
only host test files and tracking documents changed. Do not infer device
availability from the user's latest message; refresh the authorized host-access
helper before any USB operation. Preserve unrelated dirty files.

The user explicitly made long-run NO_RESOURCE/resource reclamation and reset
investigation the highest priority. TODO.md now starts with RESOURCE-01 and
contains the findings, proposed native changes and measurable acceptance gates.
It coordinates existing PERSIST, STORE, API and JC-RESET tasks. The user asked
for analysis and backlog prioritization only; no runtime implementation was
made or authorized by this update. Preserve PERSIST-01 safety gates and the
unchanged 1.8.4 CAP. Prior handoff and all unrelated worktree changes remain.

# Current handoff — reclaimer batch interruption regression — 2026-11-08 06:24 CET

Extended `./tools/test-persistent-transaction` to inject erase failures after
0, 1, 7, 8, and 9 successful deletions from ten orphan records. Each test
restarts reclaimer inventory with a fresh Heap, completes the remaining
deletions, and verifies a neighboring owner's record survives. The suite passes
with `TEST:SUMMARY fail=0`. This exercises production reclaimer logic with the
host filesystem stub; it does not establish flash durability or physical
power-cut behavior. PERSIST-01 remains open and normal persistent reclamation
remains disabled.

HMAC VPCD KATs pass 14/14; full 1.8.4 JCAlgTest support still previously failed
on persistent storage exhaustion during RSA CRT key probes. Physical devices
remain runtime-only and PC/SC lists only VPCD. Continue PERSIST-01 and capacity
work offline.

# Current handoff — current-worktree native HMAC KAT — 2026-11-08 06:21 CET

`PICO2_HOST_USB_ACCESS=1 ./tools/test-hmac-vpcd` rebuilt the PC `max_crypto`
VM and HMAC fixture, then passed all 14 checks: ATR/SELECT plus 12 one-shot and
split-update known-answer vectors across HMAC-SHA1, SHA-256, SHA-384, SHA-512,
MD5 and RIPEMD160. Log: `artifacts/logs/hmac-kat-vpcd-20261108T051915Z.log`.
The pinned 1.8.4 fixtures still verify (`tools/fixture-path --verify` passed
81 files). Full JCAlgTest support acceptance remains open: the prior complete
client attempt exhausted persistent storage during RSA CRT KeyBuilder probes.

The authorized USB refresh still found runtime boards only; PC/SC lists VPCD
but no Pico reader. No physical HMAC test or flash was performed. Next, continue
PERSIST-01 work offline; do not enable automatic persistent reclamation until
the multi-owner and power-loss gates pass.

# Current handoff — rebuilt cross-AID VPCD acceptance — 2026-11-08 06:13 CET

Rebuilt the PC VM from the current worktree with `./tools/build-pc --profile
max_crypto`, then ran `PICO2_HOST_USB_ACCESS=1
./tools/test-sio-cross-aid-vpcd`. The complete three-package SIO suite passed
35/35, including owner-tagged argument/reference checks, stale proxy behavior
after reset/restart/DELETE/same-AID reinstall, and fresh proxy dispatch. Log:
`artifacts/logs/sio-cross-aid-vpcd-20261108T050858Z.log`. The PC build returned
zero; it emitted the known Logback hostname socket warning and existing C++ /
Rust warnings. No source changes from the unrelated dirty worktree were staged.

Physical status is unchanged: the authorized helper granted three USB nodes,
which enumerate in runtime mode rather than BOOTSEL; `pcsc_scan -r` lists only
the two VPCD readers. Do not claim this VPCD pass as physical acceptance.
Continue CTX-01/03 and storage priorities offline until a Pico CCID reader or
BOOTSEL device becomes available.

# Current handoff — foreign-static replacement inventory regression — 2026-11-08 05:55 CET

Extended the persistent transaction/inventory unit test for foreign static
references. Replacing a static wrapper with one pointing to a different applet
owner now verifies that the old owner's deterministic target and nested record
become unreachable candidates, while the replacement owner's target remains
rooted. Clearing the replacement wrapper then exposes its subtree. This checks
inventory only; the reclaimer remains disabled in normal runtime and assignment
does not eagerly erase old records. `./tools/test-persistent-transaction`
passes, including the new `replacing_foreign_static_wrapper_orphans_old_owner_only`
case.

The latest authorized device refresh granted three USB nodes, but `lsusb`
shows runtime `2e8a:0009` devices, no BOOTSEL device is present, and bounded
`pcsc_scan -r` lists only VPCD readers. No physical flash or APDU was attempted.
Continue offline until at least one board is in BOOTSEL or a Pico reader is
published. Preserve the existing unrelated worktree changes.

# Current handoff — SIO rollback and refreshed max_crypto image — 2026-11-08 05:46 CET

The CTX-01 proxy root rollback is implemented and committed as `104d1845`
(`Fix SIO root rollback on proxy failure`); progress SHA reference follow-up is
`c438b3ad`. `./tools/test-rmi-registry` passes, including
`shareable_root_lease_rollback=1 owner=1 existing=1`. ARM and RISC-V builds
pass, and the max_crypto VPCD cross-AID suite passed 35/35 at
`artifacts/logs/sio-cross-aid-vpcd-20261108T034906Z.log`.

Refreshed the external-test ARM `max_crypto` empty-card image after the USB
recovery firmware change. Path:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`, size
4,079,104 bytes, SHA-256
`d5837c6fc890c4298b18cbc028e2a8a5448cf3f7ccbdff01e8ee1153da7320dd`. It is
RP2350 ARM Secure, release `0.1.1` (`0101`), profile `max_crypto` (159/162),
with all 4,096 storage blocks erased and no CAP/app installed. Firmware ends
at `0x100F1D60`, leaving 58,016 bytes; build marker is `FF735A76` from a dirty
worktree based on HEAD `735A76`. ARM/RISC-V max_crypto builds and packaging
pass. The image is not physically tested or flashed. Details are in
`artifacts/firmware/README-crypto-arm-empty-card-current.md` and the REL-01
entry in `TODO.md`.

Three Pico 2 boards currently enumerate as runtime `2e8a:0009` at stable USB
paths `1-3`, `1-4`, `1-5`, but their sysfs `serial` files are absent and
`pcsc_scan -r` lists only the two VPCD readers. Commit `971887e4` adds
`--usb-path` selection to
`tools/flash`, `tools/cycle`, and `tools/monitor`; a bounded monitor correctly
mapped path `1-3` to `/dev/ttyACM1`. Both architecture builds passed with the
new firmware BOOTSEL hook, and the updated `cycle choupi --usb-path 1-3` build
completed.

Failover checks then mapped `1-4` to `/dev/ttyACM2` and `1-5` to
`/dev/ttyACM0`; 8-second captures on both produced no serial payload. Direct
1200-baud CDC reset requests on those two paths returned `Input/output error`.
A fresh `pcsc_scan -r` still listed only VPCD. The authorized refresh now sees
USB nodes 117-119, all runtime `2e8a:0009`; none entered BOOTSEL. Runtime USB
enumeration is present, but there is no serial or CCID response from any board
in this VM. Whether the firmware loop is stalled is not established. This
failover check is recorded in commit `139a921e`.

Physical flash is still blocked: the deployed runtime rejects picotool reset
(`Unable to locate reset interface`), and the old firmware did not enter
BOOTSEL after CDC 1200 baud. The new firmware now requests
`reset_usb_boot(0, 0)` from a deferred CDC line-coding callback on 1200 baud,
but cannot be installed until a board is already in BOOTSEL. No physical flash
or runtime log is claimed. Failed attempt log:
`artifacts/logs/cycle-choupi-20261108T041557Z.log`.

Next: preserve unrelated dirty worktree files. When a board is actually in
BOOTSEL, run
`PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --usb-path 1-3` through the
required elevated host-access path, verify finite serial `TEST:PASS` output,
then verify 1200-baud reset/reflash using that new image. If one board is
unavailable, use another USB path. Refresh the authorized PC/SC helper before
APDU tests and do not treat USB presence as reader availability. The refreshed
max_crypto image metadata and handoff are committed as `a8a94dbf`; the second
and third board reset failover findings are in `6ad6b969`.

---

# Current handoff — CTX-01 revalidation and PC/SC reader discovery — 2026-11-08 04:28 CET

The offline reference-consumer audit found no new unchecked bytecode/native
reference path: typed stack pops and local reads enforce owner metadata, array
access uses package-checked pops, and JNI reference arguments use the checked
stack path. It did identify a failure-cleanup edge in SIO proxy creation: the
server reference root is retained before client proxy allocation/registration.
If a later step fails, that root may remain until reset or deletion. This is
listed in CTX-01 in `TODO.md`; implement rollback that only releases a newly
inserted root and test exhausted proxy capacity/allocation failure.

The full cross-AID VPCD suite passes 35/35 after rebuilding the PC VM explicitly
with `./tools/build-pc --profile max_crypto`. A first run against the stale PC
binary failed Signature SIO with `6F00`; the focused Signature run passed after
the rebuild, and the full passing log is
`artifacts/logs/sio-cross-aid-vpcd-20261108T032140Z.log`. The suite wrapper does
not rebuild the PC VM, so always build its profile after source/profile changes.

All three USB nodes are visible as Pico `2e8a:0009` devices (USB devices 098,
099, 100), and their descriptors include CCID interfaces. The authorized helper
granted access, but `pcsc_scan -r` still lists only the two VPCD readers.
`pcscd.socket` is active; `pcscd.service` is configured to auto-exit and was
inactive between scans. The session cannot read system journal logs, so the
driver-level cause remains unknown. No physical APDU or flash was attempted in
this validation pass. Physical cross-AID cleanup/registry checks remain open;
the three cards may still have temporary test AIDs from prior runs.

Next: implement safe SIO retained-root rollback for proxy creation, then run
unit and full profile tests/build gates. Resume physical validation only when a
Pico is listed by `pcsc_scan -r`; use another board if one drops out. Preserve
all unrelated uncommitted worktree changes.

# Previous handoff — physical cross-AID suite and reader drop — 2026-11-08 04:07 CET

Persistent applet-delete alias scanning now uses the same strict decoder as
unreachable-tag inventory for foreign-reference wrappers in package-static
records. It verifies wrapper size, owner/package fields, the deterministic
target tag length and the complete embedded source-static tag. A malformed
wrapper therefore blocks DELETE through an `InvalidTag` result instead of
being mistaken for a valid or unrelated reference. Valid aliases still block
deletion of their target applet owner.

`./tools/test-persistent-transaction` passes with new valid-alias and malformed
wrapper cases; `./tools/test-persistent-inventory` passes 22/22. `./tools/build
arm` and `./tools/build riscv` pass. The updated ARM image was flashed to Pico 2
`D92E743D5F640C0F`; finite monitor log
`artifacts/logs/cycle-choupi-20261108T021446Z.log` reports
`TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. The helper grants
USB access to three nodes, but `timeout 6s pcsc_scan -r` exits 124 without
readers, so physical DELETE/APDU validation remains open. The UF2 is 1,762,816
bytes, SHA-256
`bbface7f7640d49290eee1815b36fcf219eba56a669a533041cadef517763a98`.
Implementation commit: `0daec692`.

Follow-up audit found that the decoder rejected reserved package IDs in the
wrapper but accepted reserved applet-owner IDs `0xFE` and `0xFF` in the
embedded target tag. The shared decoder now rejects those values. A regression
confirms DELETE alias scanning fails closed and persistent inventory yields no
reclamation candidates. `./tools/test-persistent-transaction` and
`./tools/test-persistent-inventory` pass; ARM/RISC-V builds pass. D92 was
reflashed with the current ARM image and the finite monitor reports
`TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0` at
`artifacts/logs/cycle-choupi-20261108T022447Z.log`. The helper grants access to
three USB nodes, but bounded `pcsc_scan -r` still lists no Pico readers, so
physical alias/APDU validation remains open. The built UF2 SHA/size are recorded
in `progress.md`: 1,762,816 bytes, SHA-256
`b454377c522a1ee3a85c8b71122af12b801e1502b0b592f44279d9bb95bfae5d`.
Implementation commit: `7802769d`.

The strict foreign-static wrapper decoder rejects reserved target applet-owner
IDs `0xFE` and `0xFF`. Unit suites pass, ARM and RISC-V builds pass, and the
35-case cross-AID VPCD suite passes at
`artifacts/logs/sio-cross-aid-vpcd-20261108T023459Z.log`. Code commit:
`7802769d`; its progress reference is resolved in `c6568855`. Three boards
(D92, 2BED, 8E46) were flashed with the current ARM image and each passed the
finite runtime monitor:

- D92: `artifacts/logs/cycle-choupi-20261108T022447Z.log`
- 2BED: `artifacts/logs/cycle-choupi-20261108T024440Z.log`
- 8E46: `artifacts/logs/cycle-choupi-20261108T024555Z.log`

The physical cross-AID suite's APDU assertions pass on all three boards. Its
old `6B43` expected result was stale; the current all-success result is `6B40`.
The physical probe is updated and cleanup order now removes the reinstalled
service package before the client, since its package-static object may retain a
client object. On 8E46, cleanup lost the PC/SC reader with
`SCARD_E_NOT_TRANSACTED` after service-package deletion. D92 and 2BED stopped
publishing as readers afterward; the last scan showed only VPCD readers and one
unnamed USB device. Final cleanup and empty physical registries remain
unverified. Do not retry hardware until a Pico reader is published; then use
another board if the first one drops out.
Harness correction and result record: `ac567a94`.

D92 was flashed before the progress-only handoff commit and has UF2 SHA-256
`b454377c522a1ee3a85c8b71122af12b801e1502b0b592f44279d9bb95bfae5d` with CPLC
source ID `FF1E8418`. Later D92 rebuilds for 2BED and 8E46 use the same firmware
logic and UF2 SHA-256 `924f019205924fcce1924c72d02db0c6c24cc0626231238e1db11587e4b5ccd3`
with CPLC source ID `FFA7938E`. Both images are 1,762,816 bytes.

Relevant physical logs:
`artifacts/logs/sio-cross-aid-physical-20261108T025342Z.log` (D92),
`artifacts/logs/sio-cross-aid-physical-20261108T025924Z.log` (2BED), and
`artifacts/logs/sio-cross-aid-physical-20261108T030435Z.log` (8E46). D92's
temporary client and contract AIDs and 2BED's client/contract AIDs may remain;
8E46 cleanup likely removed the service but also needs a registry check. These
are test fixtures `A000000151000002`, `A000000151000004`, and
`A00000015100000401`; do not treat the boards as clean.

Continue the CTX-01 reference-consumer audit offline and keep persistent
reclamation disabled until PERSIST-01 multi-owner and physical power-loss gates
pass. Preserve unrelated worktree edits.

---

## Previous handoff — retain persistent foreign-static targets during inventory — 2026-11-08 02:31 CET

The preceding entry's latest CTX update remains below.

### Enforce owner checks in `athrow` — 2026-11-08 03:01 CET

CTX-01's reference-consumer audit found that `athrow` preserved an owner tag
but `doThrow` resolved the compact object handle in the active heap before
checking that owner. It now rejects non-local, non-global owners before heap
lookup and dispatches a catchable Java `SecurityException`. The cross-AID
fixture creates a `RuntimeException` in the caller and verifies that the
service catches the denial. Full VPCD suite passes `TEST:SUMMARY pass=35
fail=0` at `artifacts/logs/sio-cross-aid-vpcd-20261108T015026Z.log`.
Implementation commit: `b93fb66a`.

Both `./tools/build arm` and `./tools/build riscv` pass. The current ARM image
was flashed to Pico 2 `D92E743D5F640C0F`; finite monitor log
`artifacts/logs/cycle-choupi-20261108T015917Z.log` reports
`TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. The authorized
reader helper grants USB access to three nodes, but `timeout 8s pcsc_scan -r`
exits 124 without publishing a reader; the new exception case has emulator
coverage only. One initial VPCD attempt used a stale `build/choupi-pc/choupi`
binary and failed the new regression; explicitly rebuilding with
`./tools/build-pc --profile max_crypto` before rerunning made all 35 cases
pass. The ARM UF2 is 1,762,816 bytes, SHA-256
`8f219752eaa501691fb741d8c0883db561968be91cb29cf8a325f92942aaec0a`.

Next: continue the CTX-01 reference-consumer audit and persistent lifecycle
coverage. Re-run physical APDU coverage when PC/SC publishes a reader. Preserve
all unrelated worktree changes, including `bc_invoke.cpp`, `heap.cpp` and
`heap.hpp` edits noted below.

---

## Previous handoff — retain persistent foreign-static targets during inventory — 2026-11-08 02:31 CET

Persistent inventory now scans package-static records for the foreign-reference
wrapper format and protects the wrapper's owner-applet target tag and all
nested records beneath it from the unreachable-tag candidate set. Clearing the
wrapper makes the target subtree eligible for the existing reclaimer. This is
inventory-only: `PersistentReclaimer` remains disabled in normal post-APDU GC
until the PERSIST-01 recovery and multi-owner safety gates pass. Malformed or
unreadable wrappers fail the inventory closed.

The persistent-transaction suite passes with two new regressions: a live
foreign-static wrapper protects its target and nested records, and clearing the
wrapper exposes both as unreachable candidates. Its test target now links the
real `context.cpp` required by retained-context references; test cursor fault
injection is scoped to the applet namespace. ARM and RISC-V current-profile
builds pass. The final ARM CHOUPI image was flashed to Pico 2
`D92E743D5F640C0F`; its finite monitor reports `TEST:PASS choupi_runtime` and
`TEST:SUMMARY pass=1 fail=0` at
`artifacts/logs/cycle-choupi-20261108T011442Z.log`. The image is a startup
validation only; no APDU exercised the new inventory path. Refreshed USB helper
access sees three attached Pico nodes, but `timeout 8s pcsc_scan -r` exits 124
without readers. After rebuilding PC artifacts with `max_crypto`, the complete
cross-AID VPCD regression passes `TEST:SUMMARY pass=35 fail=0` at
`artifacts/logs/sio-cross-aid-vpcd-20261108T012703Z.log`. Its first run had the
known profile mismatch (`current` PC artifacts omitted AES-MAC Signature and
returned `6F00`); this was corrected by rebuilding PC with `max_crypto`.
Implementation and focused inventory regression are committed as `51624281`;
the validation follow-up is committed as `5535eb8f`.

Next: continue CTX-01 reference-consumer and PERSIST-01 lifecycle coverage.
Keep normal persistent reclamation disabled. Retry physical APDUs when PC/SC
lists a reader. Preserve all unrelated worktree changes; in particular,
`heap.cpp`/`heap.hpp` still include the unrelated `releaseGlobalAIDObject`
removal.

---

# Previous handoff — collect inactive heaps before applet DELETE — 2026-11-08 01:54 CET

CTX-01 static foreign object references are committed in `d91ed195`. A
follow-up lifecycle regression found that clearing all foreign references
could still leave an unrooted volatile reference array in the shared registry,
causing a false-positive `6985` on DELETE. The DELETE preflight now collects
unreachable objects using roots from every selected and inactive context before
checking live foreign edges. Persistent alias inventory remains fail-closed.

Validation: `./tools/build-pc --profile max_crypto`, `./tools/build arm`, and
`./tools/build riscv` pass. Focused static cleanup VPCD test passes, and the
full cross-AID suite passes `TEST:SUMMARY pass=35 fail=0` at
`artifacts/logs/sio-cross-aid-vpcd-20261108T005034Z.log`. An initial full-suite
attempt used PC artifacts regenerated for the `current` profile by the ARM
wrapper and failed only when AES-MAC Signature support was absent; rebuilding
PC with max_crypto fixed this profile mismatch.

Current ARM image was flashed to identified Pico 2 `D92E743D5F640C0F`; cycle
log `artifacts/logs/cycle-choupi-20261108T004356Z.log` records
`TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. UF2 is 1,761,280
bytes, SHA-256
`e182e1fc014e753b3f831c180646affefb3ceeca3da9f729518bc1f975e97fe0`.
After access refresh, three runtime `2e8a:0009` devices were enumerated:
`8E464F4E2BBC0A19`, `2BED26ED20556535`, and `D92E743D5F640C0F`. `timeout 8s
pcsc_scan -r` exits 124 with no readers, so physical applet/APDU validation
remains open.

Next: commit this GC-before-DELETE fix and its focused regression, preserving
unrelated dirty worktree changes. Then continue CTX-01 reference-consumer and
persistent owner-lifecycle acceptance; retry physical APDUs when PC/SC lists a
reader. `heap.cpp`/`heap.hpp` still include unrelated uncommitted
`releaseGlobalAIDObject` removal; do not stage that deletion. Other unrelated
edits remain in `AGENTS.md`, review probes, `bc_invoke.cpp`, `cap_load.cpp`,
crypto tests/scripts, and the RMI registry test.

---

# Current handoff — persistent foreign static object roots — 2026-11-08 01:08 CET

CTX-01 now supports an ordinary foreign applet object reference in a package
static field. `getstatic_a`/`putstatic_a` preserve the referenced package
identity; persistent storage uses a wrapper that points to an owner-applet
scoped target record. GC roots are routed to the package that owns the object,
and persistent inventory follows the target when checking applet deletion.
Foreign static arrays, global/private proxy values, and object graphs that
contain foreign field edges remain fail-closed. Clearing or replacing a
foreign static reference may leave an orphan target record; audit that cleanup
before widening support.

Validation passed: `./tools/build-pc --profile max_crypto`, `./tools/build arm`,
`./tools/build riscv`, and `PICO2_HOST_USB_ACCESS=1
./tools/test-sio-cross-aid-vpcd` (`TEST:SUMMARY pass=35 fail=0`). The VPCD
regression verifies static object identity after process restart and normal
service-applet deletion. Logs are
`artifacts/logs/sio-cross-aid-vpcd-20261107T234807Z.log` and
`artifacts/logs/cycle-choupi-20261108T000027Z.log`.

The ARM image was flashed to serial `D92E743D5F640C0F` using the authorized
host-access cycle wrapper. The finite monitor reported
`TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. The image is
1,761,280 bytes, SHA-256
`3ea91142d76f8c1443701f48bc5431a92f96bf418cc740d1b585a2d8723e3056`.
Physical applet/APDU validation remains incomplete: after access refresh,
`timeout 10s pcsc_scan -r` showed no readers. Three runtime boards had been
enumerated earlier: `2BED26ED20556535`, `8E464F4E2BBC0A19`, and
`D92E743D5F640C0F`.

Next: append the progress record and commit only this static-reference change,
then audit persistent alias cleanup when static references are cleared or
replaced. Preserve unrelated dirty changes, especially review probes,
`AGENTS.md`, the `bc_invoke.cpp`/`cap_load.cpp` edits, crypto tests, RMI test,
and the `releaseGlobalAIDObject` removal in `heap.cpp`/`heap.hpp`.

---

# Current handoff — volatile foreign instance-field owner roots — 2026-11-07 23:44 CET

CTX-01 now supports storing an ordinary foreign applet reference in a volatile
instance field. Each reference field carries package provenance without
increasing `jc_field_t` size; bytecodes preserve the owner tag. Local heap walks
skip foreign edges, while retained package contexts root the referenced object
in its actual owner heap. Package deletion clears these edges before owner IDs
can be reused. Persistent foreign fields and static foreign references remain
fail-closed.

Validation on the latest source: `./tools/build arm` and `./tools/build riscv`
pass with the current support profile; the ARM firmware ends at `0x100d6878`
with 169,864 bytes remaining. `./tools/build-pc --profile max_crypto` passes.
`PICO2_HOST_USB_ACCESS=1 ./tools/test-sio-cross-aid-vpcd` passes 35/35; log:
`artifacts/logs/sio-cross-aid-vpcd-20261107T223831Z.log`. The new regression
stores a foreign array reference in a runtime-created volatile holder, verifies
that service-side access is denied, then confirms the reference survives GC.

The three connected boards enumerate as runtime `2e8a:0009` devices after the
authorized helper grants nodes `/dev/bus/usb/001/062`, `/001/064`, and
`/001/066`. A fresh `timeout 8s pcsc_scan -r` still lists no readers (exit 124),
so no physical applet APDU test or flash was performed. Resume with static-field
owner handling and then persistent owner encoding/restoration; retry physical
acceptance only when PC/SC publishes a Pico reader. Preserve unrelated dirty
worktree edits in `AGENTS.md`, review probes, `bc_invoke.cpp`, `cap_load.cpp`,
crypto tests, and the RMI registry test. `heap.cpp`/`heap.hpp` also contain
pre-existing unstaged `releaseGlobalAIDObject` deletions; do not include them
in this implementation commit.

---

# Current handoff — three-board monitor check and PC/SC state — 2026-11-07 22:54 CET

The user confirmed all three Pico 2 tokens are connected. The authorized
host-access helper granted nodes `/dev/bus/usb/001/066`, `/001/062`, and
`/001/064`; elevated `lsusb` identifies all three as runtime `2e8a:0009`
Pico2 JCVM CCID devices. Bounded `pcsc_scan -r` still exits 124 without listing
a Pico reader, so no SmartcardIO/GPPro APDUs can be sent through PC/SC.

Finite serial monitor checks were run against each known board identity via
`PICO2_HOST_USB_ACCESS=1 ./tools/monitor --serial <serial> --seconds 8`.
All three returned `TEST:PASS choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`.
D92 reported CCID APDU `84F220020A` -> `9000`; 8E46 reported a prior
`80CA00E000` -> `6A88` with runtime status `9000`; 2BED showed zero transfer
count and no fresh APDU in telemetry despite the monitor's runtime PASS. No
firmware was flashed. The monitor output was not archived separately.

Next: continue CTX-01 reference-consumer and foreign-owner lifecycle work. In
particular, ordinary foreign object/static field retention and owner-aware GC
roots remain fail-closed/unimplemented; persistent foreign reference formats
must remain fail-closed. Before hardware APDUs, refresh PC/SC and use a reader
that is actually published. Preserve the unrelated dirty worktree edits listed
by `git status`; do not build/flash a newly modified image until both required
architecture builds pass.

---

# Current handoff — selected-context transient guard and reader state — 2026-11-07 22:11 CET

The native JCSystem `CLEAR_ON_DESELECT` guard and regression are committed as
`4492a973`. `SIO_CROSS_AID_TEST_TRANSIENT_CONTEXT=1
./tools/test-sio-cross-aid-vpcd` passes 2/2. All five array types reject
allocation from a foreign, non-selected SIO target with `ILLEGAL_TRANSIENT`,
while CLEAR_ON_RESET remains allowed. ARM and RISC-V max_crypto wrapper builds
pass; ARM has 62,132 bytes remaining in the firmware window.
The same-CAP sibling VPCD regression passes 6/6 after the change; its
SIO-invoked writer now allocates CLEAR_ON_DESELECT successfully in the shared
package context, while preserving shared-static behavior. Full cross-AID VPCD
regression passes 35/35, including foreign-reference, native array-copy,
proxy deletion/reinstall, and reset cases.
`./tools/test-transient-allocation-failures` passes in int-disabled and
int-enabled configurations (five focused cases in each).
`./tools/fixture-path --verify` confirms all 81 pinned fixtures, including the
unchanged JCAlgTest 1.8.4 baseline and 48 API CAPs.

Refreshed empty-card max_crypto ARM external-test image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`,
4,070,912 bytes, SHA-256
`51509530b150d5c5f04dcaa23fa82becf16a0e0f2f661c0eb2f9ca165789d564`. It
contains 3,854 firmware blocks and 4,096 erased storage blocks. Build source
ID is `FF4492A9`; `FF` marks the other uncommitted worktree edits, so this is
not a clean-tree release candidate.

The latest authorized helper grants access to three USB nodes. Elevated
`lsusb -d 2e8a:` identifies all three as runtime `2e8a:0009 Pico2 JCVM CCID`,
not BOOTSEL. Bounded `pcsc_scan -r` publishes no reader, so no smartcard APDUs
were attempted. Retry physical work when a Pico reader is listed; preserve the
other dirty worktree edits.
`tools/doctor` confirms required host/build tools. System `pcscd` journal
entries are not visible to this account, so the reader-registration cause is
unknown. The daemon is socket-activated with `--auto-exit`; direct start exits
when idle, while a terminal scan keeps it active, but the ACL refresh plus
active scan still lists no Pico reader.

PERSIST-01 host-model recheck after `4492a973`: persistent inventory passes 22
cases; persistent transaction/reclaimer and RP2350 filesystem-overlay suites
finish with `fail=0`, including 1,000-cut simulated reclaimer recovery. This
does not prove physical flash interruption or multi-owner reclaimer safety.

# Previous current handoff — RV-11 physical rerun and device state — 2026-11-07 21:54 CET

REL-01 is complete (`1c2028bc`): physical board D92 reports release `0101`,
CPLC date `2026-11-07`, and equipment ID `FF1DC424` in GPPro 26.06.04; its
finite monitor passed. External image refresh commit: `de960d02`.

The current max_crypto empty-card external image is
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`
(4,070,912 bytes, SHA-256
`93f020578c53fae98c78233f5f511ee566c603f21548cd05742bc28e19fda618`). It is
RP2350 ARM Secure, profile max_crypto (159/162), CPLC release `0101`, source ID
`FF1DC424` (dirty-tree marker plus HEAD prefix), firmware end `0x100F0D4C`,
with 62,132 bytes before storage. It contains 3,854 firmware blocks and 4,096
erased storage blocks. It has not been flashed; it includes current
uncommitted worktree content.

RV-11 remains open for physical acceptance. The VPCD Cipher lifecycle suite
passes 31/31. The physical test installed its disposable CAP and passed applet,
default-ISD, and explicit-ISD SELECT, then public GP GET DATA unexpectedly
returned `6982`; Cipher APDUs were not reached and cleanup succeeded. A first
attempt used a stale reader suffix and sent no APDUs. The physical probe now
requests `Card.disconnect(true)` and reconnects after GPPro installation to
clear any preceding SCP03 state; it compiles but is unverified on hardware.
Do not change production secure-channel policy until the reset-isolated run
establishes whether this is a probe setup issue.
The reset-isolated probe and evidence update are committed as `09f2a56d`.

The authorized helper granted all three USB nodes and `lsusb` still shows
three `2e8a:0009` devices, but subsequent bounded PC/SC scans publish no Pico
readers. Firmware-only image `build/arm/pico2_choupi.uf2` was flashed to D92
and failover board 2BED; both passed finite serial monitors and their card
storage areas were preserved. Board 8E46 was not reflashed. Logs:
`artifacts/logs/cipher-lifecycle-pcsc-20261107T204148Z.log` (GET DATA `6982`)
and `artifacts/logs/cipher-lifecycle-vpcd-20261107T204652Z.log` (31/31 pass).

The full JCAlgTest 1.8.4 support sweep remains incomplete. The first
900-second attempt timed out during RSA CRT 3072-bit probing. The extended run
completed APDU `B0 75 20 00 03 06 0C 00 00` in 797,682 ms and returned
`SystemException_NO_RESOURCE` (`F205`) while serializing a persistent
reference. Source maps the filesystem `OutOfFlash` status to this exception;
`F206` remains the expected optional `B0 E2` memory-query status. This confirms
persistent-storage exhaustion in the VPCD run. The exact reason it takes so
long, and whether the same allocation sequence exhausts physical storage,
remain open. VPCD uses the host Rust FS and a 512 KiB simulated flash; RP2350
uses the separate C++ COW FS with a 96 KiB data arena, so simulator capacity
and timing are not physical evidence. Partial output is under
`artifacts/results/jcalgtest-support-vpcd-20261107T191819Z/`. Trace the
storage-tag accumulation and defragmentation cost before resuming the long
sweep; do not enable production persistent reclamation before PERSIST-01
acceptance. The latest authorized helper granted all three USB nodes, but
bounded `pcsc_scan -r` timed out without publishing a Pico reader. HMAC-01
native code is committed as `558d0ccf`; its six RFC KAT vectors
pass. Preserve the pending and unrelated dirty changes. No physical hardware
test was run in this step.

---

---

# Current handoff — RV-54 and refreshed max_crypto image — 2026-11-07 17:50 CET

RV-20261106-54 is committed as `b3bc3f37`; the commit-reference follow-up is
`08acd740`. Transient backing overflow and local/global handle-table exhaustion
map to `SystemException.NO_TRANSIENT_SPACE` (reason 2), with negative lengths
and persistent `NO_RESOURCE` unchanged. The ROM-mask generator routes all five
`makeTransient*Array` results through `Heap::addTransientArrayResult()`;
generator commit: `79e18fd`.

`./tools/test-transient-allocation-failures` passes in int-disabled and
enabled builds, including negative lengths, int backing boundaries, and local
and global handle exhaustion. After commit, `./tools/build arm --profile
max_crypto` passed and regenerated the PC ROM mask. Earlier in this step the
RISC-V max_crypto VM-core/smoke build passed. The broad legacy
`jcarray_transient_test` aborts before reaching its assertions and is not counted
as passed. No hardware flash was performed.

Fresh max_crypto empty-card ARM image for external testing:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`, 4,047,360
bytes, SHA-256
`23f1977f6ee2328ec94722e75da3ac8a1b9748fb54c081e8262c37763d238834`. It is
RP2350 ARM Secure, release 0.1.1, profile max_crypto (147/150), source ID
`FF08ACD7`, firmware end `0x100EDFC0` (73,792 bytes remain), 3,808 firmware
blocks, and 4,096 erased storage blocks. It has an empty card (no CAP/app applet
preinstalled). It has not been flashed or tested on hardware. Source ID has the
dirty-tree marker; preserve unrelated working-tree edits.

Next: continue with the next unchecked high-priority item in `todo.md`; refresh
PC/SC access before any physical test and do not flash until both architecture
builds pass.

---

# Current handoff — R52 native int-array return fix and max_crypto build — 2026-11-07 16:21 CET

The focused R52 failure is fixed at its source in the ROM-mask generator.
`MethodComponentFromJCA` previously checked the base return type `INT` before
checking `isArray()`, so a native `[I` method ended with `IRETURN` even though
the generated JNI dispatcher pushes an array reference. The VM trace mapped
the failing PC to ROM package `A0000000600001`, method
`NativeImplementation.makeTransientIntArray(SB)[I`, at the return following
native method `0x0091`. The generator now selects `ARETURN` for array types
first. Submodule commit: `45acedf`.

`PICO2_HOST_USB_ACCESS=1 CHOUPI_VM_TRACE_EXCEPTIONS=1
SIO_CROSS_AID_TEST_TRANSIENT_CONTEXT=1 ./tools/test-sio-cross-aid-vpcd` passes
(`TEST:SUMMARY pass=2 fail=0`), including all five rejected
CLEAR_ON_DESELECT types, successful CLEAR_ON_RESET int-array allocation, and
recovery. The JCSystem APDU fixture now also verifies selected-context
allocation during `select()` (case 94), installation-time allocation,
same-context allocation and invalid event `ILLEGAL_VALUE`. Its run passed all
cases through 56, including case 94, then stopped at known separate R36 case
57 (reset/deselect callback count). R52 is complete offline. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261107T151333Z.log`
(SHA-256 `bbbb0c17db436c4b397a22939fdfe7b7e308262a71306ebea960dc3a5efaf997`)
and `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261107T152758Z.log`.

After the generator change, ARM max_crypto and the RISC-V max_crypto VM-core
and smoke builds pass. Fresh empty-card ARM image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`
(4,046,848 bytes; SHA-256
`a37bfddd3308243f74513d267029932b96051d593809d535d995541c4ce92202`). It is
RP2350 ARM Secure, profile `max_crypto` (147/150 capabilities), release 0.1.1,
source ID `FFD12B61`, firmware end `0x100EDEA4` with 74,076 bytes remaining,
and 4,096 erased storage blocks (empty card). No board was flashed. This image
is built from the dirty worktree, so it is for external testing rather than a
clean release.

REL-01 CPLC implementation remains committed as `971272f3`. Physical GPPro
`--info` acceptance remains open because the last PC/SC reader scan found no
Pico reader. Continue RV-20261106-36 reset/power callback semantics, beginning
with the case-57 mismatch. Preserve all other dirty edits.

---

# Current handoff — REL-01 image and latest reader check — 2026-11-07 15:48 CET

REL-01 CPLC source behavior is already committed as `971272f3`; the release
field is 0.1.1 (`0101`) and each build records a clean/dirty source identity.
This run revalidated the CPLC GET DATA unit suite (25/25), rebuilt ARM
max_crypto (147/150 capabilities; firmware end `0x100EDEA4`, 74,076 bytes
remaining), and passed the repository RISC-V max_crypto VM-core/smoke build.
The authorized PC/SC helper granted USB nodes `001/054`–`001/056`, but a
bounded 8-second `pcsc_scan -r` published no readers (exit 124). No board was
flashed and GPPro hardware verification could not run, so REL-01 hardware
acceptance remains open.

Fresh ARM max_crypto empty-card UF2 for external testing:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-FFF9BE81.uf2`
(4,046,848 bytes; SHA-256
`4cf059a39a26abaf94d8776b6f0d5fa1442a6eb0a678025bf020f0e03037ee9a`). Local
picotool inspection identifies RP2350 ARM Secure. The package contains 3,807
firmware blocks and 4,096 erased storage blocks, with no applet preinstalled.
The CPLC source ID is `FFF9BE81` (dirty marker plus HEAD prefix `F9BE81`); the
release is `0101`. This is built from the dirty worktree and includes the
unfinished R52 transient-context changes; it is an external test image, not a
clean release candidate.

R52 (`RV-20261106-52`) remains open. The selected-context guard rejects all
five CLEAR_ON_DESELECT array types in the focused path, but the VPCD case
returns 6F00 when it then successfully allocates a CLEAR_ON_RESET int array.
An isolated test without that successful int-array allocation passes. Exact
root cause is not yet confirmed; preserve the current fixture and production
changes and continue diagnosis. Latest logs before this rebuild are
`artifacts/logs/sio-cross-aid-vpcd-20261107T143654Z.log` and
`artifacts/logs/sio-cross-aid-vpcd-20261107T143258Z.log`.

Next: continue R52 diagnosis without disturbing unrelated dirty worktree
changes. On completion, rebuild both architectures and repackage the external
image. For REL-01 hardware acceptance, refresh the authorized reader helper and
run GPPro `--info` when a physical Pico reader is available.

# Current handoff — REL-01 offline acceptance and max_crypto external image — 2026-11-07 14:51 CET

REL-01 source implementation remains committed as `971272f3`: the CPLC release
field is 0.1.1 (`0101`), and builds embed a clean/dirty source identity. The
CPLC unit suite passes 25/25. Two consecutive ARM max_crypto builds used the
same source identity `FF7391D8` and the same firmware end `0x100EDE5C` (74,148
bytes remaining), establishing that ordinary builds preserve the selected
release field. RISC-V max_crypto VM-core/smoke build also passes. No hardware
reader was published: the authorized helper granted three USB nodes, but the
bounded `pcsc_scan -r` returned no readers. GPPro physical CPLC acceptance is
still open; do not claim it passed.

Fresh ARM max_crypto empty-card image for external testing:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-FF7391D8.uf2`
(4,046,848 bytes, SHA-256
`035b0373419ee90991af6d8ef60706ed4c10f3b8559854a8887118ef6d46fc22`). It is
RP2350 ARM Secure, profile max_crypto (147/150), firmware end
`0x100EDE5C`, and includes 4,096 erased storage blocks with no applet
preinstalled. Embedded CPLC bytes confirm release `0101` and source `FF7391D8`.
The image was built from the dirty worktree and includes uncommitted R52
transient-context changes; that focused VPCD test is currently failing and
must be resolved before treating this image as fully validated. No board was
flashed.

Next: complete R52 diagnosis without discarding its worktree changes. When a
physical Pico is published by PC/SC, query GPPro `--info` and verify release
and source ID for REL-01. Preserve unrelated modifications.

# Current handoff — R53 and max_crypto external image — 2026-11-07 14:24 CET

RV-20261106-53 is implemented and committed as `bf686b03`. Native
`JCSystem.isAppletActive(AID)` searches matching registered active/installing
instance state by full AID, checking the executing Context first because
installation can run in a short-lived Context outside the retained registry.
Focused VPCD SIO passes 4/4 across client/sibling calls before and after reset;
JCSystem callback cases 76/77 pass. The broad JCSystem suite passes through
case 56 but fails at the separately tracked RV-20261106-36 reset/deselect
expectation at case 57. No physical hardware was flashed.

Current external-testing firmware:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-FFBF686B.uf2`
(4,046,848 bytes; SHA-256
`e814c8f9b23b617f300ded3c0068cad552c9fec7b7d4f44d9dc96b1b5cebcae8`). It is
RP2350 ARM Secure, profile `max_crypto` (147/150 capabilities), release 0.1.1,
source ID `FFBF686B`, and empty-card storage (4,096 erased blocks; no applet
installed). Firmware ends at `0x100EDE30`, leaving 74,192 bytes. The dirty
source-ID prefix reflects unrelated preserved worktree changes. No token was
flashed. The image is ignored by Git.

Next: continue with RV-20261106-52 or RV-20261106-54; HOST-02 reconnect timing
still needs the external Windows host. Before physical testing, refresh the
authorized USB/PCSC helper and reader state. Preserve unrelated dirty edits,
including the pending HMAC TODO addition.

# Current handoff — R24 persistence retry regression verified — 2026-11-07 13:41 CET

RV-20261106-24 is closed with production-handler tests. Fault injection covers
failure before the first object header, between serialized child fields, and
at root-alias publication. Each case retries the same in-memory object, checks
that the root alias points to its stable body tag, and confirms field values,
canonical repeated restore, and restored body identity. A transaction-abort
case confirms the staged root/body are removed, the same preexisting live
object can be retried, and a fresh heap restores the same durable identity.

`./tools/test-persistent-transaction` passes (`TEST:SUMMARY fail=0`), and the
ASan/UBSan build/run passes with `ASAN_OPTIONS=detect_leaks=0`. Only test and
filesystem-stub code changed; no architecture rebuild or hardware run was
needed. R24 is marked complete in `todo.md`.

Next backlog priority is HOST-02 Windows reconnect timing. Its Windows-only
measurements remain externally dependent; then continue CTX-01/03 SIO context
and firewall implementation. Preserve all other dirty worktree changes.

# Current handoff — REL-01 build check and max-crypto image — 2026-11-07 13:33 CET

REL-01 source behavior is already committed as `971272f3`: CPLC release level
is `0101` (0.1.1), and the build wrapper embeds a source identity. This turn
revalidated two consecutive ARM max_crypto builds with the same source marker
`0xFFA77DD3`; the RISC-V max_crypto build also passed. The CPLC GET DATA unit
suite passes 25/25.

Physical GPPro acceptance remains open. The authorized PC/SC helper granted
three USB nodes, but `pcsc_scan -r` listed only `Virtual PCD 00 00` and
`Virtual PCD 00 01`; no physical Pico reader was available. No flash or
physical test was performed.

Current external-testing ARM image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-FFA77DD3.uf2`
(4,046,336 bytes; SHA-256
`b12cb6a2ea51927b2bfdefbbe6199d7bb1fb5502c066e95c5490c9173e49e543`). It is
RP2350 ARM Secure, max_crypto enables 147/150 capabilities, release is `0101`,
source ID is `FFA77DD3`, and all 4,096 storage blocks are erased. Firmware end
is `0x100EDDF8`, leaving 74,248 bytes. This artifact is ignored by Git and was
built from HEAD `a77dd3fe` plus preserved dirty worktree changes (dirty marker
`FF`). No token was flashed.

The REL-01 revalidation and external image record are committed as `6f68396a`.
The image itself remains outside Git per the artifact ignore rules.

Next: when a physical Pico is available in CCID mode, query GPPro `--info` and
verify CPLC release/source ID. Continue with the next actionable implementation
item meanwhile; preserve the unrelated dirty worktree files.

# Current handoff — R22/R23 persistence identity verified — 2026-11-07 13:19 CET

RV-20261106-22 is implemented and committed as `4554327c`. Persistent ordinary
objects now store bodies under unique tags independent of the first reference
field; aliases point to those bodies. Legacy direct-body records migrate to a
distinct identity marker (`FF FD`) so a transaction abort can recover the
original root, invalidate stale cached fields, reload the restored values, and
retry without splitting aliases. New bodies use `FF FE`. A diagnostic null
handle dereference that broke deep nested serialization is guarded.

`./tools/test-persistent-transaction` passes (`TEST:SUMMARY fail=0`), including
first-root replacement/clear, sibling and diamond sharing, cycles, migrated
object abort/retry, shared-alias mutation, GC, and fresh restore. ASan/UBSan
passes with `ASAN_OPTIONS=detect_leaks=0`. `./tools/build arm --profile
max_crypto` passes (firmware end `0x100EDDF8`, 74,248 bytes remaining);
`./tools/build riscv --profile max_crypto` passes. No physical test was
possible because PC/SC published no Pico reader.

RV-20261106-23 is also verified against the R22 implementation. Same-value
assignment to persistent instance and static references preserves the
separate object body during normal writes and transaction commit/abort. Fresh
restore returns the same canonical reference through the parent field, static
root, and direct root; subsequent child mutation persists. The persistent
transaction unit and ASan/UBSan suites pass. The TODO/progress status record is
committed as `a3b2c2ef`; the R23 test source is in a separate scoped
follow-up commit.

Fresh max-crypto ARM empty-card image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-FF455432.uf2`
(4,046,336 bytes, SHA-256
`f95c9bf99ee8d6094cc3eab67acd87da73aa978bbf5dba66dce3c423fee0eda2`). It is
RP2350 ARM Secure, enables 147/150 profile capabilities, contains 3,806
firmware blocks and 4,096 verified erased storage blocks, and reports source
ID `FF455432`; CPLC release level is `0101`. Firmware ends at `0x100EDDF8`,
leaving 74,248 bytes. No token was flashed. The image reflects the current
dirty worktree; preserve unrelated worktree changes, including the separate
HMAC TODO edit. The image record is committed as `8c07a4fb`, and its
SHA-resolution record is `2b45ff9b`; a UF2 byte scan confirms the CPLC release
sequence is `0101` and the source marker is `FF455432`.

# Current handoff — REL-01 reader check blocked; max-crypto image built — 2026-11-07 12:46 CET

REL-01 CPLC source remains committed as `971272f3`. The release level is
hard-coded as `0.1.1` (`0101`). Two consecutive ARM max-profile builds produced
the same dirty-tree source marker `FF8611E9`; the RISC-V shared-core build also
passed. The authorized USB helper granted three device nodes, but
`pcsc_scan -r` returned no readers (exit 124 after the bounded 8-second scan).
The VPCD GET DATA wrapper built its fixtures but did not reach reader-ready;
it was stopped after making no progress. No GPPro `--info` query or physical
flash was performed, so the REL-01 hardware acceptance remains open. Its
validation record is committed as `bb14d126`.

Max-crypto ARM empty-card image for external testing:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-FF8611E9.uf2`
(4,045,824 bytes, SHA-256
`8ade169f6785319993182045c8e46e84d1c8bb398b74da24a406b3bdcc3ae932`). It is
RP2350 ARM Secure, enables 147/150 profile capabilities, and has all 4,096
storage blocks erased. Firmware ends at `0x100EDCAC`, leaving 74,580 bytes.
Build provenance records `0xFF8611E9`; this was built from the current dirty
worktree, so review its uncommitted source changes before treating it as a
release artifact. No token was flashed.

Next: when a Pico is published by `pcsc_scan -r`, use GPPro `--info` to verify
CPLC release `0101` and the source ID. Preserve the currently dirty source
changes; do not stage them as part of this acceptance record.

# Current handoff — R26 and max-crypto external image — 2026-11-07 12:14 CET

RV-20261106-26 is implemented and committed as `a811c6f3`; its SHA follow-up
record is `cd9796ab`. Production-handler persistent boolean tests, ASan/UBSan,
and ARM/RISC-V builds pass. No physical test was performed.

Fresh max-crypto ARM empty-card image for external testing:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-cd9796ab.uf2`
(4,044,288 bytes, SHA-256
`3a0cdf63a310d369768e372b06dc0ddff3c053a5dc45a1bbe4a2fc3e5d795c02`). It
targets RP2350 ARM Secure, enables 147/150 profile capabilities, reports
release 0.1.1, and has 4,096 verified erased storage blocks, so no applet is
preinstalled. Firmware ends at `0x100ED9E8`, with 75,288 bytes remaining.
CPLC source ID `FFCD9796` reflects the preserved unrelated worktree edits. No
device was flashed. Build provenance is `build/arm/build-provenance.txt`.

Image details are recorded in commit `40f7729e`. Preserve unrelated dirty
files; the external-testing image is ready at the path above.

# Current handoff — REL-01 CPLC and max-crypto image — 2026-11-07 11:49 CET

REL-01 implementation is committed. CPLC reports release `0101` (the
requested firmware version 0.1.1), with all synthetic dates updated to 2026-11-07.
Every wrapper firmware build generates a four-byte
`ICPersonalizationEquipmentID`: `00` plus six commit hex digits for a clean
tree, or `FF` plus six digits for a dirty tree. CMake rejects malformed IDs;
the hardware probe validates the source marker and reports its four bytes.
The CPLC unit suite passes 25/25. The GPPro hardware check remains pending.

REL-01 is committed as `971272f3`. Post-commit ARM max-crypto build passes,
ends at `0x100ED9B0`, and leaves 75,344 bytes in
the firmware region. RISC-V max-profile shared-core/smoke build passes. The
empty-card external-testing UF2 is
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1.uf2` (4,044,288
bytes; SHA-256 `f9185dd5a32c4525bb1fabf5843c920d07a2371e6faf82ca10d6a69970f88a5a`;
firmware payload is 1,947,136 bytes). UF2 scan confirms version `0101`, date
`6311`, and source ID `FF971272`. The source ID marks a dirty tree based on
HEAD prefix `971272`; no exact clean-commit claim. No physical flash was done.

RV-20261106-11 transient global-array reclamation is committed as `7a77d943`
and passes the 80-round
VPCD stress test: 643/643 APDU checks and 20,817 reclaimed handles across 81
batches. Remaining R11 coverage is reset behavior and the older global AID
cache / package deletion lifetime audit; no physical test was performed. Stress
log `artifacts/logs/global-array-gc-vpcd-20261107-final.log` (SHA-256
`95007353b93ebcb39d7c08bf98cfcbfcc97134633434eb3cc0e5f90953b908ff`).

Next: continue with the next priority from `todo.md`. Physical GPPro display
acceptance for REL-01 remains pending. Preserve other dirty changes; prior
concurrent edits remain in the worktree.

# Previous handoff — global-array reclamation stress complete — 2026-11-07 11:35 CET

RV-20261106-11 transient global-array reclamation is committed as `7a77d943`
and passes the 80-round
VPCD stress test: 643/643 APDU checks and 20,817 reclaimed handles across 81
batches. ARM/RISC-V max-profile builds pass. Remaining R11 coverage is reset
behavior and the older global AID cache / package deletion lifetime audit;
no physical test was performed. Stress log:
`artifacts/logs/global-array-gc-vpcd-20261107-final.log` (SHA-256
`95007353b93ebcb39d7c08bf98cfcbfcc97134633434eb3cc0e5f90953b908ff`).

Next: finish and commit REL-01, then continue with the next priority from
`todo.md`. Preserve other dirty changes; prior concurrent edits remain in the
worktree.

# Current handoff — constrain ISO9797 method-2 unpadding — 2026-11-07 10:55 CET

RV-20261106-10 is implemented in `choupi/src/native_cipher.cpp`. Method-2
unpadding now searches only the final cipher block for its `0x80` marker, so a
marker in an earlier block, a missing marker, or an all-zero tail raises
`CryptoException.ILLEGAL_USE`. AES and DES VPCD lifecycle vectors cover those
malformed inputs, then reinitialize the same method-2 Cipher and verify valid
full-block padding still decrypts to zero bytes.

Focused lifecycle VPCD passes 13/13, and the complete symmetric suite passes
99/99 (38 Cipher, 47 MAC, 6 IV, 16 transient lifecycle vectors). Both
`./tools/build arm` and `./tools/build riscv` pass. Logs:
`artifacts/logs/cipher-workspace-vpcd-20261107T094913Z.log` (SHA-256
`faaaf7ac4c2e6412d1daf557b3a3ab2266afcfcbb45a2bf0d76deab56921c4c6`) and
`artifacts/logs/symm-crypto-kat-vpcd-20261107T095429Z.log` (SHA-256
`44da27bbafc34c3534a0756c9930149bdd39d431e6e5805d54061a8c4f2c1b55`).
No physical validation was performed; PC/SC still publishes no physical Pico
reader. Existing external build warnings remain.

Commit: `c354ce69`.

Next: investigate/test RV-20261106-11 global-array lifetime and reclamation.
R08's legacy persistent-workspace orphan-tag audit also remains open.

# Current handoff — accept empty final after aligned Cipher updates — 2026-11-07 10:40 CET

RV-20261106-09 is implemented in `choupi/src/native_cipher.cpp`: after
block-aligned updates emit all data, an empty `doFinal()` now succeeds for
NOPAD and encrypt-side ISO9797 method 1, clears the workspace and restores the
initial IV. Other padding modes retain their existing final-block behavior.

The local lifecycle applet tests AES/DES CBC NOPAD and ISO9797 M1, two full
blocks across separate updates, empty finalization, chained ciphertext against
known answers, and reuse/reset behavior. VPCD passes 12/12. During test
construction, two DES fixture expectations were corrected: key bytes now match
`133457799BBCDFF1`, and CBC's second block uses the chained ciphertext vector.
These were test-data defects; native DES one-shot vectors also passed in the
99/99 symmetric suite. Saved 512 KiB emulator image scan finds all five
sensitive workspace sentinels absent. Log
`artifacts/logs/cipher-empty-final-fixed.log`, SHA-256
`caabfd6209e897d7d18e59dad287025378816b927be2248dc637996d2852eb29`; image
SHA-256 `70f8caf48422e59e61814d7b33b156bf58c9bc755827bc461602c3231012a9f3`.

`./tools/build arm` and `./tools/build riscv` pass. ARM image ends at
`0x100d59e4`, leaving 173,596 bytes. Fixture verification passes (81 files,
JCAlgTest 1.8.4, 48 API CAPs). Build wrappers emitted existing upstream
compiler/converter warnings. No physical test: PC/SC publication remains
unavailable per the QKB entry; do not claim hardware acceptance.

Implementation commit: `a873e6d5`.

Next: continue with RV-20261106-10, strict ISO9797 method-2 final-block
padding validation. Keep R08's legacy persistent-workspace orphan-tag audit
open; its source-level risk remains unreproduced.

# Current handoff — scan all Cipher sensitive workspace payloads — 2026-11-07 10:00 CET

The chunked symmetric update fix is committed as `ff86c5b6`; SHA-resolution
records are `7deb776d` and `c2f0e350`. A further focused VPCD extension now
tests that transient AES-CTR operational bytes are not serialized: it leaves a
CTR Cipher active after 13 bytes with a transient reset-cleared AES key, a
distinct IV, buffered input and a live keystream block. The wrapper scans the
complete saved 512 KiB emulator image for those four patterns plus the
existing pending-CBC-plaintext sentinel. Its two-process run passes 11/11 and
all five patterns are absent. Log
`artifacts/logs/cipher-workspace-vpcd-20261107T085829Z.log`, SHA-256
`781838b36bf1695d30d088b6ce817607eaa3a43b9f88a23c190f80e589c6f35e`.

The Java Card API Cipher documentation was checked for same-array overlap:
the caller must avoid output overwriting unconsumed input. The current chunked
path consumes each chunk into its workspace before writing that chunk, so the
documented caller contract still applies; no separate overlap implementation
or test was added.

Physical PC/SC remains unavailable despite three runtime USB devices; see the
QKB entry “Runtime Pico USB nodes are present but PC/SC publishes no readers.”
No board commands or flash were attempted. R08 remains open for a complete
tag-level persistence audit and physical acceptance. Source inspection shows
`FlashMemory_Handler::writeArray()` writes only type, size, and clear-event
metadata for transient arrays, and restored transient arrays get zero-filled
backing storage. However, an older persistent Cipher workspace may have a
separate body tag after buffer growth. Rebinding its Cipher field writes a new
transient reference but does not obviously erase that former body tag. This
legacy-upgrade residue is a source-level risk, not yet reproduced; add a
legacy-seeded regression before deciding whether targeted cleanup is needed.
The audit/QKB/TODO update is committed as `93b2c7e0`.

Step commit: `91c20d2c`.

Next: build a legacy-seeded regression for any orphaned Cipher body records;
then resume physical validation when PC/SC publishes a Pico reader.

# Current handoff — chunk symmetric Cipher updates without transactional buffer replacement — 2026-11-07 09:46 CET

R08 coverage now includes the transaction-abort case where symmetric
`update()` input exceeds the original Cipher workspace capacity. In
`choupi/src/native_cipher.cpp`, `updateSymmetric()` processes input in chunks
bounded by the current buffer capacity, so CBC/CTR update calls do not replace
the persistent Cipher object's buffer reference mid-transaction. The buffered
length sidecar and transient buffer then remain aligned after abort.

The lifecycle VPCD test adds CBC and CTR update/abort vectors and passed
10/10, including the earlier two-process RSA/Cipher restore checks and the
persisted-flash plaintext sentinel scan. Log
`artifacts/logs/cipher-workspace-vpcd-20261107T083715Z.log`, SHA-256
`1b1695eada42d38a9709e25517a42a0ea387b30b73b4c4786fdbfebe212fe919`.
The broad symmetric VPCD suite passed 99/99 (38 Cipher, 47 MAC, 6 IV, 16
transient lifecycle vectors); log
`artifacts/logs/symm-crypto-kat-vpcd-20261107T084258Z.log`, SHA-256
`a25aefb211442fce20b01e89164350867e29affc1c849c567f798c23b1f53337`.
Both `./tools/build arm` and `./tools/build riscv` passed. The ARM full image
ends at `0x100d59b4`, leaving 173,644 bytes in its configured flash range.
`./tools/fixture-path --verify` passed (81 files, JCAlgTest 1.8.4, 48 API
CAPs).

Physical validation is blocked at PC/SC publication, not USB enumeration:
`lsusb -d 2e8a:` shows three runtime `2e8a:0009 Pico2 JCVM CCID` devices and
the permitted helper grants pcscd node access, but `pcsc_scan -r` lists only
`Virtual PCD 00 00` and `Virtual PCD 00 01`. No physical card command or flash
was attempted. The repeated symmetric suite initially found a stale hardcoded
input snapshot path; creating its expected empty generated input image allowed
the unchanged test to pass. Do not stage unrelated concurrent edits.

Step commit: `ff86c5b6`.

Next: keep R08 open; review overlapping input/output buffer behavior for the
chunked update path, inspect remaining workspace serialization cases, and
retry hardware validation when a physical PC/SC reader is published.

# Current handoff — scan persisted Cipher workspace payload — 2026-11-07 09:20 CET

RV-20261106-08 now has an end-to-end two-process VPCD regression under the
migrated `tests/fixtures/local/cipher_lifecycle_applet` source layout. The
first process tests AES CBC and RSA no-padding update/abort/final. The resumed
process verifies the old RSA Cipher returns `InvalidInit` (`449000`), then
reinitializes that retained Cipher with its restored key and checks a 64-byte
RSA no-padding known result (`019000`). `./tools/fixture-path --verify`
passed (81 fixture files; JCAlgTest 1.8.4; 48 API CAPs). VPCD passed 7/7;
log `artifacts/logs/cipher-workspace-vpcd-20261107T080444Z.log`, SHA-256
`d2d17cdd8d7df374a180121025ea3b66e6757af8c2369ce281c74f3cd5010280`.

The additional reinitialization probe and wrapper APDU are committed as
`c647f11c`; the targeted payload scan/test update is committed as `c38dcc5d`.
The SHA-resolution progress record follows. Earlier native
implementation is `c7458354`; the first process-resume wrapper was included in
concurrent commit `ab63e21b` and its attribution is documented in progress.md.
Source inspection shows transient array data is cleared and only transient
type/size/clear-event metadata is represented when a persistent object
references it. The expanded regression additionally leaves a unique 15-byte
CBC plaintext tail pending in the Cipher at emulator shutdown. A raw scan of
the saved 512 KiB persistent flash image finds no copy of those bytes, and the
sentinel is absent from the loaded test CAP. The complete run passes 8/8; log
`artifacts/logs/cipher-workspace-vpcd-20261107T081658Z.log`, SHA-256
`94b11c3f18c289f26024f6a807123fa2705a8589f2028c42982cbc4e4165af37`.

This is a targeted payload check, not a full tag-by-tag flash inventory. It
does not exercise AES-CTR or all buffer-growth boundaries. No physical board
was tested. Keep R08 open for those cases and physical acceptance.

Next: add transaction/reset coverage for CTR and symmetric buffer-growth
boundaries, then continue the remaining prioritized implementation and
acceptance work.

# Current handoff — testing-data migration complete — 2026-11-07 08:32:21 CET

User authorized full migration and final checks/commit.
Committed migration: `54174a489e648bd4495a1300122f909727bb7cb7`;
fixture catalog input commit: `21415dc6`. Final closeout preserves the
fixture-check wrapper executable permission; direct catalog tests pass (2/2). Test code is under
`tests/unit`, `tests/integration`, `tests/hardware`; local fixture sources under
`tests/fixtures/local`. Active baseline is supplied JCAlgTest 1.8.4/jc305,
with its versioned client and adjacent libraries; independent API suite retains
48 CAPs and its distinct client. Resolve paths using tools/fixture-path.
Manifest verifies 81 files; original 388-file tree is preserved/hash-checked
under artifacts/backups/testing-data-migration-20261107/AlgTest_JavaCard.
Fresh simulator reference and loose historical card CSVs live under
 tests/fixtures/reference. Legacy 1.8.2 evidence is explicitly historical.

Migration validation completed: JCAlgTest emulator/restart 30 checks; API
Core/Multi smoke (three installed applets) and extended Crypto 3 checks pass.
Early emulator timeouts remain in logs. Native batch 26/28 passed; the user
explicitly accepted allocation-test missing rollback-generation link dependency
and VPCD partial-header send failure as documented follow-ups MIG-20261107-01/02
in todo.md. No runtime correction or physical acceptance claimed. See
 docs/testing-data-migration.md and artifacts/results/testing-data-migration-20261107.

Pre-existing runtime, review-probe and test edits remain unstaged. Migration-only
index content preserved seven unrelated edited files; do not discard their
working changes at their new paths. Current source submodule dirtiness also
predates this migration. No further implementation authorized by migration
closeout; address semantic follow-ups separately.

# Current handoff — full testing data migration in progress — 2026-11-07 08:17:41 CET

User explicitly authorized the full migration, superseding review-only scope
for this task. Fixture catalog/resolver now pin provided JCAlgTest 1.8.4 and
separate API suite (48 CAPs, distinct client). Hash verification and 2 integrity
tests passed. Test files have been relocated in the working tree and consumers
are being updated/validated; migration is not complete. Preserve all prior
dirty source/test changes. Baseline current/HEAD bytes and migration-only index
content are under build/testing-migration/; do not stage pre-existing edits.
Do not run hardware operations for this filesystem/test migration. Next:
validate moved native wrappers, v1.8.4 emulator/client and final instructions,
then archive original supplied tree only after dependency scans pass.

# Current handoff — review summary and concurrent-source reconciliation — 2026-11-07 07:21:44 CET

User requested a summary. Report/todo now contain 60 historical findings/tasks,
including R60 dup_x analysis checked against Oracle JCVM 3.0.5. After a pause,
HEAD advanced through concurrent fixes. Added prominent 7 November status
reconciliation for R01/R02/R03/R05/R06/R07/R08/R60; do not describe all 60 as
unchanged open defects or reimplement corrected paths. R60 software closure
is reported by the implementing session; this review inspected corrected
source but did not independently rerun its new test suite. Other acceptance
scope remains open as recorded in the status table.

Pass12 preserves pre-update hashes and refreshed current hashes. Initial hash
verification failed because source changed during the pause; this correctly
triggered reconciliation rather than a false validation claim. All 60 mapping,
link/anchor checks and refreshed hashes now pass. No implementation, test or
CAP changed by this review session. Continue review-only under active goal;
stage only own continuity hunks and preserve all concurrent work.

# Current handoff — Cipher workspace fresh-restore test — 2026-11-07 07:21 CET

RV-20261106-08 has a partial native change in `choupi/src/native_cipher.cpp`
and `choupi/src/native_rsa_cipher.cpp`. Cipher workspaces (key snapshots,
IVs, buffered bytes, AES-CTR stream state and RSA input) use reset-cleared
transient arrays. Symmetric init rebinds old/persistent or undersized
workspaces; RSA init reserves one full modulus-sized input buffer so a
transactional object-reference rollback cannot desynchronize its
nontransactional native length sidecar. This preserves the established
transient key-workspace behavior.

The focused CAP lifecycle probe and `./tools/test-cipher-workspace-vpcd`
pass: AES CBC update/abort/final after partial and full block inputs; RSA
no-padding update/abort/final; then a second emulator process resumes the
flash image and confirms the retained Cipher is uninitialized (`449000`). The
two-process VPCD summary is `TEST:SUMMARY pass=6 fail=0`; log
`artifacts/logs/cipher-workspace-vpcd-20261107T061911Z.log`, SHA-256
`7622fb838721a67fd7bd3f706975a078f23a7f568802de0ffc1234208134992c`.
`./tools/build arm` and `./tools/build riscv` pass. ARM image ends at
`0x100d58ac`, leaving 173,908 bytes. Builds emit existing Rust/CAP warnings.
This was simulator-only: process-restart restore passed, but no physical board
test or persistent-record inventory was run. Source inspection confirms the
current workspace allocations use transient-array constructors; inspect the
saved flash records to independently establish that no Cipher workspace
records are written and assess whether pre-existing persistent records need
migration or cleanup. R08 remains open for those checks. Native implementation
is committed as `c7458354`; the process-restart acceptance wrapper is being
committed separately.

Next: continue R08 acceptance offline with persistent-record inspection; keep
this issue open until that gate and physical validation are addressed.

# Current handoff — bound superclass traversal in VM runtime — 2026-11-07 05:57 CET

RV-20261106-07 has a partial runtime hardening change in
`choupi/src/jc_handlers/jc_class.cpp`, `jc_class.hpp`,
`choupi/src/jc_bytecodes/bc_object_field.cpp`, and `bc_invoke.cpp`. The
class-to-class cast, Object-class lookup, instance-field sizing, inherited
field-token flattening and shareable-interface superclass walk now stop after
256 links and fail closed. Field-size accumulation and flattened tokens are
checked against `UINT16_MAX`.

Validation passed: PC `max_crypto` build; isolated SIO Signature test; full
cross-AID VPCD suite (`TEST:SUMMARY pass=35 fail=0`); ARM current build; RISC-V
current build; and scoped whitespace check. The first Signature run failed
because `build/choupi-pc/choupi` was stale; rebuilding through
`./tools/build-pc --profile max_crypto` made the isolated case and complete
suite pass. Build output still has existing packed-CAP-member alignment
warnings. No Pico was flashed, and no malformed superclass-cycle CAP test was
run.

RV-20261106-07 remains open: there is no load-time superclass-cycle validation,
and no hostile self/two-class-cycle or deep-chain regression proving bounded
recovery for allocation, casts, fields, method resolution and manager use.
Implementation is committed as `bcfa97ae`; see `progress.md` for the full
validation record.
Next: continue remaining P1 findings while preserving unrelated changes.

# Current handoff — validate instance field receiver classes — 2026-11-07 05:06 CET

RV-20261106-06 now has a native runtime assignability guard in
`choupi/src/jc_bytecodes/bc_object_field.cpp`. The shared field-token resolver
checks that the receiver's concrete class is the field's declaring class or a
subclass before using its flattened field offset. The guard is wired through
all 24 byte/short/int/reference get/put opcode forms, including wide and
`_this` variants. Hierarchy inspection is bounded to 256 links and rejects
when assignability cannot be established.

Validation passed: PC `max_crypto` build; ARM and RISC-V wrapper builds under
the `current` support profile; and full `./tools/test-symm-crypto-vpcd`
acceptance, `TEST:SUMMARY pass=99 fail=0` (38 cipher vectors, 47 MAC vectors,
and 16 transient lifecycle vectors; two emulator resets). Log:
`artifacts/logs/symm-crypto-kat-vpcd-20261107T040241Z.log`, SHA-256
`c93d49f54d982f152a197c3889e9f762853189667c021e737381e91909ccd796`.
An initial direct invocation used pre-existing generated probe artifacts and
failed APDU 3; rerunning through the full wrapper regenerated the matching
probe and passed. No physical test was attempted.

R06 remains open for a targeted negative CAP regression proving unrelated
same-layout receiver rejection, explicit imported/inherited/API receiver
coverage, and Java Card access-control semantics beyond class assignability.
The review source also cites access checks; this change only establishes
receiver-class compatibility. Next: add the focused negative runtime test or
advance another independently reproducible P1 while keeping R06 acceptance
open. Implementation commit is `f0fc469b`; its SHA-resolution record follows
in `progress.md`.

# Current handoff — persistent field cache remains canonical on write failure — 2026-11-07 04:44 CET

Persistent `JC_Instance` setters for byte, short, int and reference fields now
update the in-memory cache only after the persistent storage helper returns
successfully. Reference assignment resolves the incoming array/object type
before changing the cached value. Fault-injected tests cover byte, two-word
integer and array-reference failures inside an applet transaction; the old
cache is visible before abort and the original serialized record remains after
abort. `./tools/test-persistent-transaction` passes with `TEST:SUMMARY fail=0`.
ARM and RISC-V wrapper builds pass; ARM firmware ends at `0x100d5690` with
174,448 bytes remaining. No hardware test was run.

RV-20261106-05 remains open for caught failure followed by applet commit, GC and
cold restore. The stack-bounds SHA-resolution commit is recorded as
`81eb4d21`. This persistent-cache step is committed as `a343db91`; the SHA-resolution record follows in progress.md.

Next: examine RV-20261106-04 OwnerPIN reset and transaction-exempt state. Keep
implementation in native C++ VM paths; do not modify ROM API Java sources or
CAP fixtures. Preserve remaining concurrent worktree changes.

# Current handoff — validate stack local ranges before access — 2026-11-07 04:33 CET

`Stack` now validates physical stack indices, local-variable count and complete
slot width before forming local pointers or touching packed `ValueKind`
metadata. `Frame` local reads/writes also validate before pointer formation.
Nested `push_Frame()` checks caller argument availability and total frame
capacity before shifting the caller TOS or forming the new frame pointers; it
updates caller/root state only after frame insertion succeeds. The ASan/UBSan
RMI registry fixture covers high invalid local indices at a near-end frame,
rejects a two-word write across a one-word local without changing that local,
and verifies failed frame capacity leaves the caller operand intact.

`./tools/test-rmi-registry` passes with `local_bounds=1`. LeakSanitizer cannot
run in this VM under ptrace, so the wrapper turns off leak detection while
keeping ASan and UBSan active. ARM and RISC-V wrappers pass. ARM image ends at
`0x100d5658` with 174,504 bytes left in the reserved region. No physical test
was performed. RV-20261106-03 remains open for additional local/operand
boundary combinations. This implementation and test step is committed as `b36e9cf1`; its SHA-resolution record follows in progress.md.

Next: continue with the remaining P1 review findings. Keep the provided dirty
CAP/API review and crypto-probe changes unstaged unless their owner requests
otherwise.

# Current handoff — nested frame unwind direction — 2026-11-07 04:20 CET

`Stack::unwindToDepth()` now removes frames from the front of the frame list,
which matches `push_Frame()` (`emplace_front`) and normal `pop_Frame()`
(`pop_front`). A three-frame regression unwinds to depth two and then one and
checks that each retained caller's PC and local value remain intact. The
`./tools/test-rmi-registry` wrapper passes with `unwind_callers=1`; ARM and
RISC-V wrapper builds also pass. No physical device was used.

RV-20261106-02 remains open for end-to-end nested Java/ISO exception,
allocation-failure and timeout coverage. This first correction is committed as
`5e7dcb00`; its progress SHA-resolution record follows.

Next: continue through the P1 runtime findings, keeping all pre-existing
uncommitted review and implementation changes isolated from commits.

# Current handoff — interpreter opcode fetch recovery — 2026-11-07 04:13 CET

`Interpretor::run() noexcept` now contains exceptions from current-frame lookup
and opcode fetch. A method falling through its validated body returns `6F00`
and unwinds its active interpreter frames instead of allowing the C++ exception
to terminate the process. `tools/tests/cap_malformed_vpcd_test.py` adds a
fall-through mutation that changes the final `BC_RETURN` in the upstream
JCAlgTest `process()` method to `NOP`. The end-to-end VPCD test passes 6/6 and
then successfully selects the card manager, reselects the applet, and selects
the manager again. The neighboring truncated-`SIPUSH` case also passes 6/6.
The malformed-CAP test now uses a bounded 60-second APDU timeout because the
first cold faulting APDU took 19.5 seconds and exceeded the old 20-second bound
under variable emulator startup load.

Validation: `./tools/build-pc --profile current`, both malformed VPCD cases,
`./tools/build arm`, and `./tools/build riscv` passed. ARM firmware ends at
`0x100d5514` with 174,828 bytes left in the reserved region. No physical test
was possible: attached runtime USB devices still did not appear in PC/SC. VPCD
logs and hashes, plus full validation results, are in the newest `progress.md`
entry. RV-20261106-01 remains open for additional caller-state and malformed
entry coverage. The implementation and regression are committed as `a5a400c4`; the
commit SHA-resolution record follows in progress.md.

Next: inspect RV-20261106-02 (`Stack::unwindToDepth()` removes the wrong end of
the frame list) and build focused multi-level caller-state tests. Preserve all
other uncommitted source, review, fixture and test changes in the worktree.

# Current handoff — HOST-02 Windows reconnect timing harness — 2026-11-07 03:40 CET

Added `tools/measure-windows-reconnect.ps1` and documented its Windows
workflow. With serial/COM clients closed, it waits for physical unplug, listens
for Windows `Win32_DeviceChangeEvent` arrival, polls `SCardListReaders`, then
uses WinSCard to connect and send a read-only ISD SELECT. CSV rows include the
image SHA-256, reader, driver-install state, OS, PnP event timestamp, reader
latency, APDU latency and response. Run it on the Windows host for at least
three cycles each with the current and original images. The PnP event is a
host-side timing origin and does not include operator replug reaction time.

This Linux VM has no `pwsh`/Windows PowerShell runtime, and Windows timing has
not been collected. Static consistency assertions and `git diff --check` pass;
the embedded C# and PowerShell script still require execution on Windows.
Normal/diagnostic firmware behavior and the no-DTR physical deadline test are
already recorded in TODO/progress. Do not close HOST-02 until Windows CSV
measurements and image comparison are reviewed. No firmware was rebuilt or
flashed for this documentation/harness step. Harness and workflow are committed
as `f65c9f8e`; the progress SHA-resolution record follows separately.

CTX-01 continuation: Oracle Java Card 3.0.5 removable-applet guidance confirms
DELETE must fail when a target-owned object is referenced by an object owned by
another applet. Thus the cold-start persistent-array blocker is consistent with
the documented deletion rule while its stored child-object record remains; do
not change the fail-closed guard based on the dropped-field experiment. The
reclamation path is a separate PERSIST-01 task: `requestObjectDeletion()` is
currently unsupported, and its safety acceptance remains open. This
specification check is recorded in TODO/QKB; implementation work can continue
on other offline PERSIST-01 gates.
This spec-alignment update is committed as `6c7721cd`; its progress SHA
resolution follows separately.

PERSIST-01 offline revalidation on the current worktree: `./tools/test-persistent-inventory`
passes 22/22 and `./tools/test-persistent-transaction` passes all cases,
including preserved other-owner records, fail-closed incomplete/corrupt roots,
transaction guards and reclaimer interruption/retry. These do not satisfy
physical multi-owner or actual power-loss validation. Current host access sees
three USB nodes but no Pico PC/SC reader; keep object deletion disabled.
The evidence update is committed as `efeb94c9`; its progress SHA-resolution
record follows separately.

The current-worktree RP2350 filesystem overlay suite also passes. Its named
`persistent_reclamation_1000_power_cuts_recover_old_or_clean_state_and_retry`
case models 1,000 snapshot cuts and succeeds. This strengthens simulated
recovery evidence but does not replace physical flash interruption testing.
The update is committed as `b75f057c`; its progress SHA-resolution record
follows separately.

Latest USB/PC/SC refresh at 2026-11-07 03:51-03:53 CET: three runtime
`2e8a:0009` Pico devices are visible on USB (`001/054`-`001/056`), and
`pcscd.service` is active, but `pcsc_scan -r` returns no reader. No physical
APDU/reclaimer test can run in this state. The driver-level reason is unknown;
journal entries were not visible. Keep all prior source and review worktree
changes unstaged.
This diagnostic record is committed as `e1639b97`; its progress SHA resolution
follows separately.

# Previous handoff — persistent sibling object and array aliases block DELETE — 2026-11-07 03:20 CET

Cold-start VPCD now exercises persistent package-static aliases, a direct
object reference in an unselected sibling applet's instance field, and a
sibling-owned persistent `Object[]` containing the target object. The instance
and array aliases each make DELETE return `6985` before either client applet is
selected. Clearing the instance field or explicitly nulling the array element
permits DELETE `9000`; the surviving sibling remains usable. Dropping the
array field without nulling its element leaves the serialized alias present,
and DELETE remains blocked. Focused runs pass 2/2. The full `symm_crypto`
cross-AID suite passes 35/35. Full-suite log:
`artifacts/logs/sio-cross-aid-vpcd-20261107T020718Z.log`, SHA-256
`aecca8167aaf44ddd64c08ff6a92a19d4d76893557dee0862720684eb87b4d6c`. Final
full-suite log after the array case:
`artifacts/logs/sio-cross-aid-vpcd-20261107T021701Z.log`, SHA-256
`874e6d6b8e60da3a8542c4193988f19cf3ea8092b07044b1aed8b6e2072f1108`. Focused
array log:
`artifacts/logs/sio-cross-aid-vpcd-20261107T021545Z.log`, SHA-256
`15b1aebe5c66f13a89494450c33d2bb674bac6a09ce629b87b76bccfb53bd423`. Focused
object log:
`artifacts/logs/sio-cross-aid-vpcd-20261107T020312Z.log`, SHA-256
`32fd54335cd25bde61db7c067723ea0650438e12ec3204d3ae5880def3032cfd`.
No physical device was accessed; no firmware implementation changed. The
array-coverage fixture and documentation updates are committed as `91b934c8`;
its progress SHA-resolution record follows separately. Preserve unrelated
worktree changes.

Next follow-up: determine whether a sibling persistent array with a non-null
target element remains reachable after its containing applet field is cleared,
or whether the inventory guard sees an orphan because normal persistent
reclamation is disabled. Keep the guard fail-closed until persistent root
enumeration and interrupted cleanup are proven complete and safe. This finding
is tracked in CTX-01/TODO and QKB; no runtime change has been made.
The follow-up note is committed as `d8d9cef0`; its progress SHA resolution is
recorded in `progress.md`.

Physical check on 2026-11-07: `sudo -n /usr/local/sbin/pico2-pcsc-reader`
granted access to USB nodes `001/054`, `001/055`, and `001/056`, but bounded
`pcsc_scan -r` returned no reader listing within 12 seconds. No physical CAP
test was possible; retry only after PC/SC publishes a Pico reader.
This hardware-availability record is committed as `0359b647`; the progress
SHA-resolution record follows separately.

# Previous handoff — cold-start persistent alias DELETE regression — 2026-11-07 02:58 CET

A focused same-package card-manager APDU regression exercised persistent alias
protection with no applet Context materialized at DELETE time. The client
fixture stored its applet object in a package static (namespace `0x02`), the
emulator restarted, and DELETE returned `6985` before selecting either client
instance. Clearing the static reference through the sibling permitted DELETE
`9000`; the sibling remained usable. Focused VPCD passed 2/2 and the full
`symm_crypto` cross-AID suite passed 35/35. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261107T015250Z.log` (SHA-256
`836425861b6b8632245c41d663062faccb79b883b8e6b4da9007e3f63135a6f9`) and
`artifacts/logs/sio-cross-aid-vpcd-20261107T015414Z.log` (SHA-256
`5f41fc9206ff9fee85b79c99501c4ebfe72c29718ac4ebb86bb3865b97d76eff`). A
foreign service-array attempt did not persist an ordinary cross-package
reference, which intentionally serializes as null. No physical device was
accessed. This prior step is committed as `766b572e`.

# Current handoff — persistent alias protection for applet DELETE — 2026-11-07 02:40 CET

Applet-instance DELETE now runs a persistent inventory scan at the top-level
retained-reference API before checking materialized Context heaps. This catches
object (`0xFE`) and array (`0xFD`) aliases in persistent package/app instance
namespaces even when their sibling owner has never been selected since boot.
Alias chains of up to 32 records are followed; a 33-record chain, malformed
or unreadable inventory blocks DELETE fail-closed. Inventory unit tests cover
both depth boundaries. The helper is covered by `./tools/test-persistent-inventory`.
A dedicated APDU fixture for an unselected sibling's persisted alias remains
open. Keep CTX-01 open along with same-package DELETE APDU coverage.

Validation this step: all 22 `./tools/test-persistent-inventory` cases and
`./tools/test-rmi-registry` pass; final `./tools/build arm` and
`./tools/build riscv` pass after depth-boundary coverage was added. ARM image ends at `0x100d54e0`, leaving 174,880 bytes in the reserved
firmware region. Focused retained-reference VPCD DELETE passes 2/2, and the
full `symm_crypto` cross-AID suite passes 35/35. Log:
`artifacts/logs/sio-cross-aid-vpcd-20261107T012537Z.log` (SHA-256
`df320b6ada9ef9f404af3743faed2de056dd61377b38e80ddaf5a2ebee521a30`). No
physical test was run. The first full-suite attempt used the `current` profile
and stopped at the fixture's Signature expectation; after rebuilding with
`symm_crypto`, the full suite passed.

Implementation, tests, QKB and handoff are committed as `722cf6d7`; the
progress SHA-resolution record follows separately. Preserve the unrelated
source/review changes in the worktree. Prior handoffs remain below.

# Current handoff — applet DELETE retained-root scan — 2026-11-07 02:02 CET

The DELETE preflight now covers two volatile-array cases: cross-package edges
routed through the foreign-array registry, and same-package sibling edges found
by scanning the target package's shared heap. Arrays owned by the target applet
are excluded from the local scan so its own object graph does not block its
removal. It now also scans sibling-owned object reference fields, persistent
reference arrays, and cached package static roots. Commit `9282315a` contains
this scan extension and its regression. Native unit coverage
verifies sibling-array rejection, target-owned-array exemption, slot-clear
release, cached static-root rejection/release, and a sibling instance-field
reference blocking until cleared. The latest field/static tests pass in the
native registry fixture. Source/build scan extension is committed as
`9282315a`; its SHA resolution is `7e85cd39`. The sibling-field regression
follow-up is `f4f6195b`. Package-static objects tagged `0x02` are excluded
from applet-owned persistent blockers; a synthetic native case checks this.
The correction is committed as `13c2848c`.
The expanded `symm_crypto` cross-AID VPCD suite passes 35/35, including service
applet DELETE, stale proxy rejection, and same-AID reinstall. The log is
`artifacts/logs/sio-cross-aid-vpcd-20261107T004502Z.log` (SHA-256
`7a06e0358e9f147a0a28942672fc8cfaa789c68527fe939d3afa21bf07c5f113`).
The focused retained-object lifecycle APDU case also passes 2/2: DELETE returns
`6985` while the service array holds the target object, returns `9000` after
the slot is cleared, and the sibling applet remains usable. Log:
`artifacts/logs/sio-cross-aid-vpcd-20261107T010001Z.log` (SHA-256
`7a1578d28e3163aa0abc6f7fb9606b0444b3f3a1a93041b2904ee8b7d5e41701`). This
still tests a cross-package retaining array; same-package sibling array APDU
coverage is not complete. The validation record is committed as `d0698e85`.

ARM and RISC-V builds pass after this correction. ARM image ends at
`0x100d5358`, leaving 175,272 bytes in the reserved firmware region. No
physical run completed: the
reader helper found three USB nodes, but `pcsc_scan -r` timed out without
listing readers. Full VPCD log:
`artifacts/logs/sio-cross-aid-vpcd-20261107T000029Z.log` (SHA-256
`ef75708c9d200aa09107912c03ad3029738dd113ac9f0ccb422117bbd83645bd`).

The scan only sees heap objects and roots materialized in the current runtime;
persisted state for an unselected sibling applet may not be loaded. Remaining
acceptance: persistence-backed coverage proving how such roots are loaded or
covered, plus a same-package card-manager DELETE APDU case. Keep CTX-01 open.
The previous handoffs remain below as historical context.

# Current handoff — applet DELETE retained-reference guard — 2026-11-07 00:41 CET

Implemented a DELETE preflight for volatile cross-package reference arrays. It
resolves tracked handles in the target package heap and compares each object's
allocation applet owner with the applet being deleted; matching edges reject
DELETE with `6985`, and unresolvable registered references fail closed. The
three-package VPCD fixture covers two instances from one package: retain an
object from instance A in a service `Object[]`, reject DELETE A, clear the slot,
retry DELETE successfully, then confirm instance B still selects and calls the
service.

Validation: ARM and RISC-V wrappers pass; `./tools/test-rmi-registry` passes;
focused VPCD passes 2/2 and full `symm_crypto` cross-AID VPCD passes 35/35.
Full-suite log is `artifacts/logs/sio-cross-aid-vpcd-20261106T232906Z.log`
(SHA-256 `4e957c70c45dafd72a25d5987a16ddda6f9b9e2c9a03020bd2a5456a62cad3a0`).
No physical test completed: authorized helper found three USB nodes, but
bounded `pcsc_scan -r` timed out without listing readers.

Scope remains deliberately partial. Same-package cross-instance references and
persistent/static reference roots still need an audit and targeted tests before
CTX-01 deletion semantics can be closed. Prior handoff entries below are
historical and preserved.

Implementation and VPCD regression are committed as `3b4e3804`.

# Current handoff — positive-depth dup_x validation complete — 2026-11-06 23:55 CET

Commit `6fb521d58aabd98bb29593f70f9b14e4817b6896` fixes positive-depth
`dup_x`: only the top M words are copied, the top N words are shifted, and
word-kind/reference-owner metadata is preserved. Operand validation now uses
the specification's inclusive N bounds. The follow-up regression enumerates
every legal M/N pair for this build and validates a 32-bit int-pair case.
`./tools/test-rmi-registry` passes with `stack_dup_x=1`; ARM and RISC-V builds
pass.

The attempted `./tools/test-rmi-arrays-vpcd` run fails during applet install:
`stage=5 sw=6A80 vm=6F00`, before VPCD connects. A debug run confirms the
currently generated ROM `Dispatcher.addService` bytecode does not contain
`dup_x`, so this is not opcode execution evidence. It is an unrelated applet
instance persistence failure and must not be attributed to this change. The
first VPCD attempt was sandbox-blocked at localhost socket creation; the same
test was rerun with host access. No USB device was accessed or flashed.

The stack implementation is committed in `6fb521d5`; legal-form/int-pair unit
coverage in `7141905d`; and the CAP fixture/wrapper in `c41e89f7`. Unit tests
enumerate all legal M/N pairs and all 256 encoded nibble pairs, preserve the
stack on every invalid pair, test values/order, exact bounds, capacity,
reference owners, and 32-bit int pairs. `./tools/test-dup-x-vpcd` passes both
positive and negative interpreter cases: operand `0x12` on `[10,20]` yields
10, installs/selects successfully, and operand `0x17` reaches the handler and
fails closed with `RuntimeException`, opcode `0x3F`, and `6F00`. Test logs and
hashes are in `progress.md`. ARM, RISC-V, PC, and focused native tests pass.
No physical board was accessed; this validation used a test-only CAP and the
PC VPCD emulator.

The exhaustive invalid-nibble and runtime rejection update is committed as
`c5d16e53`; its detailed progress entry resolves the latest pending SHA.

RV-20261106-60's requested software semantics and interpreter checks are
complete. Preserve unrelated review/worktree changes. Continue with the
applet-instance DELETE / retained foreign-array root issue documented under
CTX-01 in `TODO.md` and `.agent/QKB.md`; build a VPCD regression before
changing lifecycle cleanup. Keep package DELETE and per-instance DELETE
coverage distinct.

# Current handoff — foreign-array GC review R59 — 2026-11-06 22:51:33 CET

Review-only active goal made progress: report/lowercase todo totals now 59.
Added source-derived cross-heap cycle retention from unconditional incoming
foreign-array roots. Existing ./tools/test-rmi-registry passed normal rooting,
overwrite/delete cleanup and owner slot reuse. The new unreachable-cycle case
was not run; CAP-path reachability needs nested SIO argument handoff and must
not assume direct return of a server-local array bypasses wrapping rules.

Pass11 evidence/hashes under `artifacts/reviews/review_20261106/pass11/`.
Raw log is local/ignored; committed JSON contains both PASS lines and its hash.
All 59 mappings/links/anchors, JSON and 10 source hashes checked. Suspected
GC dangling registry pointer excluded because collection unregisters first.
No implementation/test/CAP change or hardware action. Current wrapper used
its existing host fixture; no firmware build or performance measurement.
Continue review-only; stage only own shared-log hunks and preserve unrelated
worktree edits and prior handoffs below.

# Current handoff — asymmetric key coverage review R57–R58 — 2026-11-06 22:46:42 CET

Review-only active goal made progress: report and lowercase todo now contain
58 findings/tasks. Added unsupported transient EC private-key ECDSA signing
and the unbound existing-key KeyPair constructor. RSA transient private-key
Signature support is present; it was not reported missing. Extended R28's EC
matrix to allocatable versus usable lengths/domains without duplicating it.

Pass10 hashes/scope: `artifacts/reviews/review_20261106/pass10/`. All 58
finding/task mappings, links/anchors, JSON and 12 inputs checked. No runtime
acceptance, builds, implementation/test/CAP/generated changes or hardware
operations. Profile flags and existing generated JNI were inspected, not
regenerated. Runtime combination/constructor checks remain unexecuted.
Shared continuity files have concurrent edits from another session; stage only
this review's own hunks. Preserve those edits and other dirty files. Continue
review-only; implementation remains prohibited.

# Current handoff — applet-instance DELETE foreign-root audit — 2026-11-06 22:54 CET

Read-only CTX-01 audit found a possible retention gap: VM cleanup calls
`clearForeignArrayReferencesForPackage()` only for whole-package DELETE.
Applet-instance DELETE forgets that AID's runtime context and invokes GC, while
foreign array edges are tagged only by package ID and continue to route into
the package heap. This may retain objects from a deleted applet as long as a
foreign-reference array remains live. Source evidence only; runtime impact and
the safe invalidation granularity are not yet verified. Do not claim handle
aliasing or a reproduced bug.

`./tools/test-rmi-registry` passes package-delete edge cleanup and owner-slot
reuse; it does not cover applet-instance DELETE with a live foreign array edge.
Next: add a VPCD case that deletes one applet while a sibling service retains a
reference into it, then validate edge clearing and GC/resource behavior. AID
and package DELETE must remain distinct. No source implementation changed in
this audit; hardware was not accessed.
Audit record: commit `c79a8808`; RMI registry test output is in the preceding
progress entry.

# Current handoff — typed foreign-array regression committed — 2026-11-06 22:44 CET

Commit `21051098` adds a negative `aastore` regression for an incompatible
foreign applet object passed through SIO. The test passes it via an `Object`
parameter and stores it into a runtime `Shareable[]` through an `Object[]`
alias, expecting catchable `ArrayStoreException`. Compatible foreign
Shareable proxy storage remains positive coverage. No runtime algorithm or
firmware implementation changed.

Both full VPCD modes pass 35/35: Descriptor omitted log
`artifacts/logs/sio-cross-aid-vpcd-20261106T211815Z.log`; Descriptor included
log `artifacts/logs/sio-cross-aid-vpcd-20261106T212814Z.log`. The Python syntax
check and scoped diff check passed. An initial full test failure was caused by
an INS collision with KeyBuilder; moving the new command to `0x3F` fixed it.
No device was flashed. Last refreshed USB enumeration showed three Pico CCID
nodes, while PC/SC only exposed the two Virtual PCD readers. Refresh the helper
and PC/SC before physical validation.

Next: continue auditing foreign-reference consumers, roots and lifecycle
boundaries under CTX-01. Preserve the parallel review work and all unrelated
dirty source, test, submodule and artifact changes.

# Current handoff — CAP dependency and cleanup review R55–R56 — 2026-11-06 22:41:58 CET

Review-only goal made progress: report/lowercase todo totals now 56. Added
source-confirmed library dependency-closure validation gap and repeated
full-store orphan cleanup work for clean free package IDs. Confirmed import
version matching, loaded-uninstalled importer deletion guards and lifecycle
recovery already exist; did not report those as missing. Extended R29 raw
resource allocation-failure audit to RSA CRT helpers without duplicating it.

Pass9 source provenance: `artifacts/reviews/review_20261106/pass9/`. All 56
finding/task mappings, links/anchors, JSON and 11 source hashes verified.
No new runtime cases, build, hardware operation, CAP/test/generated/source
implementation changes. Dependency setup and scan/latency measurements remain
unexecuted. Review-only scope remains mandatory; preserve unrelated dirty
worktree edits and previous implementation handoffs below.

# Current handoff — typed foreign-array rejection regression — 2026-11-06 22:40 CET

Added a negative typed-array store case to the three-package SIO fixture. It
passes an incompatible client applet object through an `Object` argument and
stores it into a runtime `Shareable[]` via an `Object[]` alias; the VM raises
catchable `ArrayStoreException`. The positive foreign `Shareable` proxy store
continues to pass. This directly covers both outcomes of the `aastore`
component-type check.

Validation: `python3 -m py_compile tools/tests/sio_cross_aid_vpcd_test.py` and
scoped `git diff --check` passed. The full VPCD suite passed 35/35 with
Descriptor omitted and 35/35 with Descriptor included. An initial test run
exposed an APDU INS collision with the existing KeyBuilder test; the new case
now uses INS `0x3F`. Logs: `artifacts/logs/sio-cross-aid-vpcd-20261106T211815Z.log`
and `artifacts/logs/sio-cross-aid-vpcd-20261106T212814Z.log`. No firmware
source/build or hardware validation was involved. At the last device check,
three Pico USB CCID nodes were visible but PC/SC listed only two Virtual PCD
readers; refresh PC/SC before physical testing.

The test-source commit is pending. Preserve unrelated review, crypto-test,
submodule and source edits in the worktree. Resume CTX-01 by auditing remaining
foreign-reference consumers/lifecycle roots; keep physical acceptance open.

# Current handoff — JCSystem transient/activity review R52–R54 — 2026-11-06 22:38:03 CET

Review-only active goal made progress: report and lowercase todo now contain
54 findings/tasks. New source-confirmed mismatches: deselect-transient
allocation lacks selected-context check, isAppletActive queries only the
executing applet, and transient backing-limit errors map to NO_RESOURCE
instead of NO_TRANSIENT_SPACE. Extended R11 retention scope to historical
runtime AID cache entries rather than creating a duplicate retention task.

Two candidate defects were excluded: raw Object stack pops already reject
foreign owners; AID bytes use retained nontransient buffers and survive reset.
Pass8 source/generated hashes and scope under
`artifacts/reviews/review_20261106/pass8/`. All 54 mapping/link/anchor checks,
JSON and 11 input hashes passed. No new runtime tests, builds, CAP/generated
edits, implementation changes or hardware operations. Continue review-only;
preserve unrelated worktree edits and prior handoffs below.

# Current handoff — ArrayLogic native API review R48–R51 — 2026-11-06 22:34:30 CET

Review-only active goal made progress: report and lowercase todo now contain
51 findings/tasks. Four additions: cross-type repacking rejected, atomic
generic calls lack a standalone transaction/capacity boundary, NonAtomic
calls use rollback-participating setters, and native UtilException lacks
Java dispatch/reason plumbing. Native utility code and checked-in API contract
establish these findings; no runtime acceptance cases were executed.

Pass7 provenance under `artifacts/reviews/review_20261106/pass7/` hashes 12
sources/generated inputs. Existing JNI dispatch was inspected, not rebuilt
or edited. All 51 mappings/links/anchors and hashes checked. No implementation,
CAP, test source, build or hardware operation changed. Generic copy scratch
vector cost was linked to existing performance tasks, without measurements.
Continue review-only under the active goal. Preserve unrelated worktree edits
and prior implementation/physical-blocker handoffs below.

# Current handoff — transaction recovery review R44–R47 — 2026-11-06 22:30:54 CET

Review-only goal made progress: report and lowercase todo now contain 47
findings/tasks. Added failed rollback journal disposal/partial sync, snapshot
capacity charge before throwable publication, persistent element validation
allocations (reference owner-vector initialization per access), and missing
transaction-created object reference invalidation on abort.

Pass6 is source-confirmed only; proposed fault injection and runtime cases
were not executed. Source hashes and scope are under
`artifacts/reviews/review_20261106/pass6/`. All 47 finding/TODO mappings,
links/anchors, JSON and 14 source hashes passed verification. No implementation,
test source, generated JNI, CAP, firmware build or hardware operation changed.
Performance complexity was inferred from loops/allocations, not measured.
Continue review-only; implementation is explicitly prohibited. Preserve all
unrelated dirty files and preceding continuity records below.

# Current handoff — APDU and invocation review R41–R43 — 2026-11-06 22:27:05 CET

Review-only active goal made progress: three additional source-confirmed
findings, with report/lowercase todo totals of 43. APDU.receiveBytes bypasses
state/offset checks, outgoing length checks use response buffer capacity and
lose BAD_LENGTH reasons, and method entry's noexcept package-list insertion
can terminate on allocation failure after a frame is pushed. Native boundary
implementation remains mandatory for any later fix; none was implemented.

Pass5 source provenance: `artifacts/reviews/review_20261106/pass5/`. Source
inspection and 43 finding/TODO/link/hash checks only; no new runtime tests,
full builds, USB/PCSC, flash, CAP edits or performance measurements. Existing
parser/APDUUtil tests do not establish these APDU state/error contracts.
Preserve all unrelated worktree edits and previous handoffs below. Continue
review-only under the active goal; do not implement any listed TODO.

# Current handoff — native API contract review R37–R40 — 2026-11-06 22:23:01 CET

Review-only goal made progress: four additional source-confirmed findings and
matching TODOs; report and lowercase todo now contain 40 each. No runtime,
API implementation, tests, generated JNI or CAP changed. This pass inspected
native APIs and the checked-in API contracts: optional EC K is required by
initialization/ECDSA, genKeyPair overwrites preinitialized exponent/domain,
CBC MAC finalization retains nonzero IV, and reset drops initialization for
persistent-key Cipher/Signature objects. Extended R29's resource audit to EC
key-pair curveA across vector allocation rather than duplicating the finding.

Pass4 provenance is in `artifacts/reviews/review_20261106/pass4/`. No new
runtime case/build/hardware operation was run; do not claim runtime reproduction.
All 40 finding/task mappings, file links, anchors and source hashes checked.
Prior pass's two emulator failures remain the latest runtime observations.
Continue review-only under the active goal; implementation remains prohibited.
Preserve all unrelated dirty files and earlier handoffs below.

# Current handoff — review follow-up R32–R36 — 2026-11-06 22:19:12 CET

Review-only scope remains mandatory: no implementation is authorized. Report
`docs/review_20261106.md` and lowercase `todo.md` now contain 36 findings/tasks.
This pass added emulator failure-status loss (observed twice), shared build
publication without standalone PC locking, serial-only flash target fallback,
mixed-reader power-up evidence and physical/VPCD power-off callback mismatch.

Two local-socket attempts used the existing hashed emulator binary, with no
fresh build, USB/PCSC, flash or saved image. Storage startup failure and VPCD
connection refusal both incorrectly exited 0. These are failed acceptance
checks, not passing startup tests. See committed pass3 manifests for commands,
errors and binary/source hashes; raw logs are local and ignored. R33–R36 are
source-confirmed, with proposed validation unexecuted. All 36 finding/task
mappings and documentation links were checked. No performance measurements.

Preserve all unrelated dirty source, fixture, submodule and artifact changes.
Continue review-only if requested; do not fix listed defects under this task.
Earlier implementation handoffs and physical blockers remain below.

# Current handoff — review follow-up R22–R31 — 2026-11-06 22:11:12 CET

Review-only goal remains active. User explicitly prohibits implementation.
The report `docs/review_20261106.md` and lowercase `todo.md` now contain 31
findings/tasks after ten source-confirmed additions. New high priorities are
ordinary object durable identity and self-aliasing, retry after failed first
persistence, persistent boolean-array reads and new-object value loss when
first promoted after an unrelated rollback. JNI argument-conversion failures,
typed-array CP namespaces, EC metadata/resource cleanup and hardware exception
diagnostics are also covered.

The unchanged historical review runner `--iteration2` failed to link against
current static-field validation dependencies; no runtime cases ran. Do not
claim reproduced persistence outcomes from that attempt. Fixture alignment
assumptions also need updating before its output is usable. No fixture was
changed. Manifest and source hashes: `artifacts/reviews/review_20261106/pass2/`.
No hardware/build acceptance was attempted. All 31 documentation links/tasks
were checked. Previous goal turn was progress (committed review/evidence);
this turn extends authoritative review findings and TODOs.

Next review pass can inspect remaining OS/host boundary and lifecycle edges;
do not implement the listed fixes under this request. Preserve the unrelated
source/fixture/submodule changes and prior implementation handoffs below.

# Current handoff — review only completed — 2026-11-06 22:00:36 CET

The user requested a thorough current-code review and explicitly said not to
implement anything. `docs/review_20261106.md` records 21 prioritized findings;
lowercase `todo.md` has 21 matching follow-ups and links to the existing
uppercase `TODO.md` owners. No implementation or CAP was changed.

Existing offline checks covered 32 targets: 31 passed, one failed to link
(`test-jcvmarray-allocation`, missing rollback-generation dependency). The
protocol socket test passed with host permissions after restricted-context
failure. Test manifests/provenance are in
`artifacts/reviews/review_20261106/`; raw logs are local and gitignored. No
firmware build, USB/PCSC, flashing, serial or physical power-cycle work was
performed. Source-confirmed defects have proposed reproductions, not new
end-to-end passing evidence.

Highest priorities for a separately authorized implementation task: contain
opcode-fetch exceptions inside the noexcept interpreter, correct nested
unwinding direction, validate locals before metadata reads, implement native
PIN lifecycle/transaction behavior, and make failed persistent setters cache
consistent. Full details and acceptance cases are in the report. Preserve all
pre-existing working-tree changes and the prior implementation handoffs below.
Review commit SHA is recorded in the appended progress linkage.

# Current handoff — owner-aware typed foreign arrays — 2026-11-06 21:42 CET

## CTX-01/03 typed foreign reference-array implementation

Applet-owned volatile reference arrays can retain ordinary foreign references
with owner provenance. `aastore` resolves the foreign instance through the
retained Context for its package, so equal compact handles in the active Heap
cannot make the wrong class pass. Persistent foreign arrays and foreign
static/instance field stores remain fail-closed.

The typed `Shareable[]` test exposed an additional class/interface resolution
bug: generic CAP constant-pool classrefs carry the imported token but no
separate kind bit, and JC305 tokens must use separate class and interface
remapping tables. `tools/javacard-class-tokens.py` now emits source-token
interface flags and rejects overlapping class/interface token values in the
mapped exports. Dynamic classref resolution uses those flags. The VPCD fixture
retains a foreign Shareable proxy in `Shareable[]` and checks identity.

Validation passed:

- `./tools/test-rmi-registry` (`RMI_REGISTRY:PASS`, including unique/ambiguous
  retained-package lookup and owner-root checks).
- `SIO_CROSS_AID_TEST_TYPED_ARRAY_ONLY=1 SIO_CROSS_AID_INCLUDE_DESCRIPTOR=1`
  focused VPCD typed-array case: pass.
- Full VPCD cross-AID SIO suite with Descriptor included: `34/34`.
- Full default Descriptor-free VPCD suite: `34/34`.
- `./tools/build-pc --profile max_crypto`, `./tools/build arm --profile
  max_crypto`, `./tools/build riscv --profile max_crypto`: pass. ARM image
  ends at `0x100ecf2c`, leaving `78,036` bytes in the 1 MiB firmware window.
- No physical token was flashed or tested during this step. The last physical
  observation remains the previous handoff’s PC/SC state; refresh the helper
  before any hardware work.

VPCD logs: focused `artifacts/logs/sio-cross-aid-vpcd-20261106T202558Z.log`
(SHA-256 `91b36bc55d1d0f051ed785195a813a6880ea9ebb27fe1febb22022e703252c52`),
Descriptor-present full suite `...T202739Z.log` (SHA-256
`2842239a1222db66537c5bd0d1bd048469b0a4c23fabf5f021e8f4fd0dd09f19`),
Descriptor-free full suite `...T203319Z.log` (SHA-256
`3aadbcf21a81fc782cc6c7306637625272fdf78c57e437117df2f4db9c6bee68`). ARM
image SHA-256 `cd3ca51f771785e819cd227ba2174d13fee4da8b9c141d9684710a437afb88da`.

Next: continue the open CTX-01/03 owner-aware reference audit, especially
remaining foreign-reference consumers and lifecycle/root edges. Do not broaden
persistent or static/instance foreign-reference stores without durable owner
identity, restoration, and GC-root coverage. Follow root `AGENTS.md` for
validation and commits. Implementation commit:
`9e99b9d35cb5924e36e4db41dc8682903dce5ec0`.

# Previous handoff — loaded package import deletion policy — 2026-11-06 20:02 CET

## Latest GP-01 dependency validation

The package dependency preflight now has VPCD evidence for both installed and
merely loaded importers. An installed service/client importing a library blocks
library DELETE with `6985`, remains usable, then both importer packages can be
deleted and the library DELETE succeeds with `9000` (`2/2`). A separate case
loads the service CAP without installing its applet and confirms that this
loaded importer still blocks library DELETE (`6985`); deleting it permits
library DELETE (`9000`, `1/1`). Latest log SHA-256
`1780394f19841a4e8b2e9acc425c53a4e979e6da1197981995c610b9cccfa1b2`.
Loaded-uninstalled coverage is committed as `e52ceb3d`.

PC/SC refresh shows only Virtual PCD 00/00 and 00/01, no Pico. Physical package
DELETE remains open. Next: continue offline GP-01/CTX work and repeat these
scenarios on hardware once a Pico reader appears.

# Previous handoff — GP GET DATA VPCD validation and wrapper repair — 2026-11-06 18:05 CET

## Completed offline GP-01 GET DATA validation

Repaired `tools/test-gp-get-data-vpcd`: it now starts from a zero-byte flash
image (the old `algtest-v182-empty-flash.bin` was stale and contained package
56 without its registry sidecar), performs a bounded terminal-attached
`pcsc_scan -r` before starting the emulator to activate/keep the VPCD backend,
and waits for the named reader's card presence. The host-side
`GPGetDataApduProbe` now explicitly selects the default ISD before GET DATA.

The complete VPCD flow passed. Open GP `80CA` Card Recognition and CPLC
responses returned `9000`; ISO `00CB` returned the value-only BER objects;
both GP/ISO `2F00` list queries returned the ISD AID. SCP03 authentication
succeeded and authenticated GP `80CA` returned matching objects. Companion
`./tools/test-gp-get-data` passed 25/25. No physical reader was available for
hardware tests. APDU log SHA-256
`54ecabd9a94cabbef79356c1c2e3de2f219349145035a861279ad08f0ea77f23`;
emulator log SHA-256
`43d0517b24ab4d1b131dd76c38cadb38bb4cdd80c09afc1542c05732a55ea04a`.
Results are under `artifacts/results/gp-get-data-vpcd-20261106T170234Z/`.
Implementation/test-tool commit: `2a3ca092` (`Repair GP GET DATA VPCD acceptance`).
The multipackage VPCD integration also passes 47/47 with `--timeout 120`,
including GET DATA `61xx`/GET RESPONSE for 21 installed AIDs, restart registry
reconstruction, and lifecycle cleanup. A 60-second timeout expired on the
18-instance cascade DELETE, which completed under the 120-second bound.
Acceptance evidence and timeout note are committed in `2e94869d`.

## Latest GP-01 dependency-check follow-up

Extended the imported-library DELETE regression to remove importers after the
expected `6985`: dependent client and service package DELETEs both return
`9000`, then the library DELETE returns `9000`. Focused VPCD summary is 2/2;
log SHA-256 `f26cda758522bb0504a6bbf5040dec9b496c9e161a367e6b9ea537e8136079e0`.
No Pico reader is listed by PC/SC, so this is emulator-only. Next continue with
offline GP-01/CTX work; repeat the deletion sequence physically when a reader
appears.
This regression extension is committed as `dce22b92`.

Prior scoped implementation: commit `e91f9d65` rejects deletion of packages
imported by another loaded CAP (`6985`), validated in VPCD; its handoff and
progress SHA are in the next commit `c8bfc576`. Physical DELETE and GET DATA
acceptance remain open. The physical refresh found only Virtual PCD 00/00 and
00/01, no Pico.

# Previous handoff — reject deletion of imported library packages — 2026-11-06 17:25 CET

## Completed offline GP-01 follow-up

Added a fail-closed package dependency scan before full package DELETE. It parses
the Import component of every other loaded dynamic package and rejects deletion
when any package imports the target AID (`6985`); malformed package/import
metadata returns `6581` without starting deletion. This keeps an installed
client/service pair usable. Added an opt-in VPCD regression to
`tools/tests/sio_cross_aid_vpcd_test.py`.

Validation: PC build `./tools/build-pc --profile max_crypto` passed; focused
imported-package VPCD check passed (`6985`, dependent client SELECT `9000`,
service invocation `45679000`, `TEST:SUMMARY pass=1 fail=0`); default VPCD suite
passed `TEST:SUMMARY pass=34 fail=0`; ARM and RISC-V max_crypto wrappers passed.
Python syntax and scoped `git diff --check` passed. This work used emulator
only; PC/SC still has no enumerated Pico, so no flash or physical APDU test was
run. Focused log SHA-256 `150a4a652ab0ee59ce5899cc141e4511cf3230a3c0b96cccf22bca2aad5ceb30`;
default suite log SHA-256
`c4731899e42c4f3e9d5f1c055a5deeef8cf296ccb25aaf04a938655ba09d4800`.

The unrelated pre-existing edits in `src/cap_load.cpp` (filesystem erase
adapter and expanded LOAD diagnostics), crypto tests, artifacts, and submodule
trees were not part of this step. Stage the DELETE dependency hunks in
`src/cap_load.cpp` selectively. The scoped implementation is committed as
`e91f9d65` (`Reject deletion of imported CAP packages`). Next: proceed with the
next open GP-01 item. Physical package DELETE validation remains open.

# Previous handoff — package DELETE invalidates cross-AID SIO routes — 2026-11-06 14:22 CET

## Latest: CAP-REINSTALL-01 defect reproduced and fixed in VPCD

The new package-delete regression exposed that a client retained proxy could
revive after successful server package DELETE and same-AID package reinstall;
the old route returned `45679000`. Proxy entries now store the server package
ID. Package DELETE invalidates matching routes across selected and inactive
contexts. The final focused regression passes: after delete, old proxy returns
`6A0A`, fresh lookup is null; after same-AID reinstall, old proxy still returns
`6A0A` and fresh lookup reaches the service (`45679000`). Unit coverage
confirms a route to another package remains intact.

Validation: `./tools/test-rmi-registry`; focused
`PICO2_HOST_USB_ACCESS=1 SIO_CROSS_AID_TEST_PACKAGE_DELETE=1
./tools/test-sio-cross-aid-vpcd`; default SIO VPCD 34/34; same-CAP sibling
6/6, restart 7/7, and delete/reinstall 9/9. ARM and RISC-V max_crypto wrapper
builds pass. Full ARM image SHA-256
`eaf0fda21b394d6dc55a886be8ee19ac0abdb71eab1fbd8424f627e9d90de470` was
flashed to `8E464F4E2BBC0A19`; finite monitor log
`artifacts/logs/monitor-full-choupi-8E464F4E2BBC0A19-20261106.log` SHA-256
`a0a213ab9aacf5a482c748e3f3ff2e49d52afb02c4ab3bff9e81074bc0d8b4d2`,
`TEST:SUMMARY pass=1 fail=0`. One `tools/cycle arm` invocation selected the
smoke image; I immediately restored the full firmware with `tools/flash` and
verified the full image separately. Do not use that cycle log as firmware
evidence.

Physical APDU validation remains unavailable. The authorized helper grants
three USB nodes, but `timeout 8s pcsc_scan -r` exits 124 without listing a Pico.
The VPCD scenario still lacks an independent unrelated-client applet. After
runtime C++ edits, rebuild the PC emulator with
`./tools/build-pc --profile max_crypto` before running VPCD tests; wrappers reuse
the existing executable. Next: close unrelated-client integration and repeat
the package-delete/reinstall APDUs on physical hardware once PC/SC enumerates a
Pico. `TODO.md` keeps CAP-REINSTALL-01 open for these acceptance gates.

# Previous handoff — card-manager recovery after malformed CAP — 2026-11-06 13:43 CET

## Reader refresh — 2026-11-06 13:46 CET

The authorized helper granted USB nodes `001/039`, `001/040`, and `001/049`.
`timeout 8s pcsc_scan -r` emitted no reader list and exited 124. Do not attempt
physical GPPro/SmartcardIO until a Pico reader appears and a bounded
read-only transaction works. This updated outage is recorded in `.agent/QKB.md`.

Extended the opt-in empty-ConstantPool negative case to install the separate
valid client package after the malformed service package is rejected. The valid
LOAD and INSTALL succeed in the same VPCD session after the `6F00` failure,
with Descriptor both omitted and included. Focused summaries pass 1/1 in each
mode. Descriptor-free log
`artifacts/logs/sio-cross-aid-vpcd-20261106T124223Z.log`, SHA-256
`b1a61117741e48f1fd3d3e7ed494adb5aaebc19b9eb95c8812ca4d62ec4f84cd`;
Descriptor-included log
`artifacts/logs/sio-cross-aid-vpcd-20261106T124249Z.log`, SHA-256
`c643efc054a20ebedd79a92d70c2664331509a7dbc872e49d6d6a952878bb0c3`.

No runtime code or upstream CAP changed. PC/SC enumeration still times out
despite three USB nodes granted by the authorized helper; no physical testing
was attempted. Next: continue offline CTX-01/03 work; repeat physical GPPro
Descriptor-free validation once a Pico reader is registered.

# Previous active handoff — empty CAP ConstantPool rejected at INSTALL — 2026-11-06 13:38 CET

Added opt-in `SIO_CROSS_AID_TEST_EMPTY_CONSTANT_POOL=1`. The generated service
CAP keeps a valid ConstantPool component with `count=0`, updates the component
and Directory sizes, and leaves all other CAP bytes intact. CAP LOAD succeeds;
service Applet INSTALL rejects with `6F00` both when Descriptor is omitted and
included. The default Descriptor-free three-package cross-AID VPCD suite still
passes 34/34. The first focused run caught the wrong Directory size-field
offset in the fixture mutator; the corrected index passes all runs.

Logs: first failed fixture-metadata attempt
`artifacts/logs/sio-cross-aid-vpcd-20261106T123424Z.log`, SHA-256
`8349280326b29babab22495be551e919c09e29e5e74baccda3deed8319db18d5`;
Descriptor-free focused
`artifacts/logs/sio-cross-aid-vpcd-20261106T123520Z.log`, SHA-256
`dc2779d90d517795d60b8caff14319e6bc44ad2d6b54ba678ace6bbeca0e69b2`;
Descriptor-included focused
`artifacts/logs/sio-cross-aid-vpcd-20261106T123550Z.log`, SHA-256
`5abf8e10263f16047253c1be3a4a1a651554f92de13d0493ca9de7624f38218b`;
default suite
`artifacts/logs/sio-cross-aid-vpcd-20261106T123619Z.log`, SHA-256
`2db5a4f9ab556370d01fa58d6fcccb923a43d8bd48d5344433a8a83fa308d259`.

No runtime code or upstream CAP was modified; no firmware build or physical
test was needed. Previous Class omission case remains committed as `b5230539`.
Next: continue the remaining CTX-01 work and retry physical Descriptor-free
GPPro validation when PC/SC enumerates a Pico reader.

# Previous active handoff — empty Class method tables fail closed at INSTALL — 2026-11-06 13:30 CET

Added opt-in `SIO_CROSS_AID_TEST_EMPTY_CLASS_TABLES=1`. The mutator removes
all public/package method-offset entries from the generated service CAP's
Class component, preserves class/interface records, and updates the Directory
Class size. In Descriptor-free and Descriptor-included modes, CAP LOAD
succeeds but service Applet INSTALL rejects the incomplete tables with
`6F00`; both focused VPCD summaries pass 1/1. The first run caught a fixture
assumption that every class has a nonempty table; the mutator now preserves
already-empty tables while removing populated ones. The default Descriptor-free
cross-AID integration remains 34/34 with cleanup.

Logs: Descriptor-free
`artifacts/logs/sio-cross-aid-vpcd-20261106T122638Z.log` SHA-256
`43cd74626fb19efb9e10d1aa7e6614ae2607d1d1de5466a75c611e08a151c183`;
Descriptor-included
`artifacts/logs/sio-cross-aid-vpcd-20261106T122717Z.log` SHA-256
`954d928a7e4656fb20e1de0427a78015cfeb29302843b6df5340183f134a170f`;
default suite
`artifacts/logs/sio-cross-aid-vpcd-20261106T122752Z.log` SHA-256
`37510c80de31f742dd23659f2398a3dd3dcca1664191f43dad9f9c9fcb7190c5`.

The authorized reader helper granted all three USB nodes at 13:30 CET, but a
bounded `pcsc_scan -r` timed out without listing a Pico. No physical operation
was attempted. Next CAP metadata gap is complete ConstantPool method-entry
omission; physical Descriptor-free GPPro revalidation remains pending.

# Previous active handoff — zero-entry Export table case passes VPCD — 2026-11-06 13:15 CET

Added opt-in `SIO_CROSS_AID_TEST_EMPTY_EXPORT_TABLE=1`. It removes the service
CAP's sole Export class record (which has no fields or methods), sets Export
`class_count` to zero and updates the Directory size. CAP LOAD and service
Applet INSTALL both pass (`TEST:SUMMARY pass=1 fail=0`). The generated fixture
is test-only; upstream CAPs and runtime source are unchanged. Focused log
`artifacts/logs/sio-cross-aid-vpcd-20261106T121242Z.log`, SHA-256
`b13d09f269e687cd6c59bd41261e6a085c979e2e93b5ce01580f97d8b83b0c51`.

The normal Descriptor-free three-package cross-AID VPCD suite also passes
34/34 at `artifacts/logs/sio-cross-aid-vpcd-20261106T121319Z.log`, SHA-256
`409c417d789dfbfca12045844d268396b75010172607c55e36caf90e1cd8df85`.
The focused empty-Export case also passes with Descriptor present:
`artifacts/logs/sio-cross-aid-vpcd-20261106T121812Z.log`, SHA-256
`36ff47e521377d934ef13a7eedf5be23b67000962e45d9217e48a48418e19cc7`.
Remaining method-table work is whole-entry omission for Class/ConstantPool;
physical GPPro revalidation requires a Pico reader in `pcsc_scan -r`.

# Previous active handoff — diagnostic no-DTR deadline passed; normal empty-card image restored — 2026-11-06 13:04 CET

Fresh ARM/RISC-V `max_crypto` diagnostic builds with a 30,000 ms CDC wait
passed. Diagnostic image SHA-256:
`805ff85b5335704b8d3317ae76c9c79c0376086c86e1aabbfb34c9a1edee4300`.
Flashed only D92 (`D92E743D5F640C0F`) using
`tools/flash --serial D92E743D5F640C0F`; picotool could not locate the reset
interface, and the wrapper's CDC 1200-baud fallback entered BOOTSEL and
completed flash verification. Kept all serial clients closed for 32 seconds
after the flash command completed (therefore longer than the configured
deadline), then opened the finite monitor. Runtime passed repeatedly with
`TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. This confirms the
deadline path without DTR; no serial log was saved separately.

Restored D92 using the same empty-card image prepared for Windows testing,
SHA-256 `2578fa07aa5d6dc92e1eaed947774964bdde40c3eb4f596f8841e387b9058a9c`.
The flash wrapper verified the full image and storage; a fresh eight-second
monitor passed. ARM and RISC-V were rebuilt at `serial_wait_ms=0` after the
diagnostic run; both provenance files confirm normal mode. Current empty-card
max_crypto image remains at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`.

Latest reader refresh still grants three USB nodes, but a bounded
`pcsc_scan -r` times out without any Pico reader. PC/SC applet validation is
not available. HOST-02 has the no-DTR diagnostic deadline check; remaining
gate is at least three Windows cold reconnect timings with no serial terminal.
Preserve unrelated dirty files.

# Previous active handoff — max_crypto Windows image rebuilt; three boards pass runtime monitor — 2026-11-06 12:53 CET

The old CTX-01 note that Descriptor-free service INSTALL still fails is stale.
The runtime fallback already present in `Method_Handler::callMethod` gathers
callable starts from the Applet, Class method tables, internal static method
references and Export component, then bounds execution at the next known start
or Method-component end. Unknown/malformed offsets still fail closed.

Validated through the authorized VPCD wrapper using its default mode, which
omits Descriptor like standard GPPro `CAPFile.getCode()`: the three-package
cross-AID suite passes 34/34, including service and client INSTALL, call
coverage and cleanup. The separate Descriptor-included 34/34 result is recorded
in a companion run immediately afterward. Descriptor-included log:
`artifacts/logs/sio-cross-aid-vpcd-20261106T111004Z.log`, SHA-256
`7c4d8b53dd82cc0fcb03f01a4a4f7cb227725cc0eb90b90a77454eb8e26c2aba`. The
Descriptor-omitted log:
`artifacts/logs/sio-cross-aid-vpcd-20261106T110214Z.log`, SHA-256
`cf16e9c3786b41220256e712701e76b172a9b8bcb0c59b7a3710fd7f6e298910`.

No runtime source change was made in this continuation. The older physical
`D92E743D5F640C0F` INSTALL failure occurred before the structural-offset
fallback and does not validate the fix. Next: add hostile-CAP regression for
incomplete Class/constant-pool/Export entry metadata, then repeat standard
GPPro Descriptor-free LOAD/INSTALL on Pico hardware when PC/SC registers a
reader. Preserve unrelated worktree modifications.

## Latest continuation — unlisted install-body offset rejected

Added opt-in VPCD negative case `SIO_CROSS_AID_TEST_UNLISTED_INSTALL_OFFSET=1`.
It redirects the generated service Applet entry to a zero-valued interior byte
inside the real install method body; the following bytes resemble a method
header, but the offset is not any Descriptor-listed method start. Descriptor-free LOAD
succeeds and INSTALL fails closed with `6A80`. Repeating with Descriptor
included yields the same `6A80`. Both runs preserve the emulator after failure.

Descriptor-free log `artifacts/logs/sio-cross-aid-vpcd-20261106T111814Z.log`,
SHA-256 `58adf2a276978085e41cc40075ceab0494fb3a039eddaef6942dc2282dfaa306`;
Descriptor-included log `artifacts/logs/sio-cross-aid-vpcd-20261106T111843Z.log`,
SHA-256 `fda0b3e4da852740e6cc65edcb76a00d0f9f5afe250007e478ed7094be44dab2`.
The generated fixture is test-only; upstream CAP bytes are unchanged. Python
syntax and scoped diff checks pass. No firmware change, target build or
physical operation occurred. Next test missing/incomplete method-entry table
metadata and retry hardware only once PC/SC is responsive.

## Latest continuation — invalid Class method-table offset rejected

Added opt-in `SIO_CROSS_AID_TEST_BAD_CLASS_METHOD_OFFSET=1`. It zeroes the first
Class public/package method-table offset in the generated service CAP while
preserving component sizes. CAP LOAD succeeds; Applet INSTALL returns `6A80`,
as the descriptor-free method-bound scan rejects the invalid structural entry.
Focused VPCD summary: `TEST:SUMMARY pass=1 fail=0`. Log
`artifacts/logs/sio-cross-aid-vpcd-20261106T112657Z.log`, SHA-256
`ca66d9a84d4c94fc4877b2d82de63fc6e0c983aa228303d8328676aaff74af46`.
Python syntax and scoped diff checks pass. Firmware and upstream CAPs were not
changed. Next: add ConstantPool/Export malformed-entry coverage and repeat the
standard GPPro hardware install once PC/SC registers a Pico reader.

## Latest continuation — invalid ConstantPool static-method offset rejected

Added opt-in `SIO_CROSS_AID_TEST_BAD_STATIC_METHODREF_OFFSET=1`. It zeroes the
first internal `CONSTANT_StaticMethodref` target in the generated service CAP.
CAP LOAD succeeds, but INSTALL is rejected with `6F00` at package-import
validation stage 9, before applet bytecode runs; no Java Card exception is
reported. The focused test passes and the card manager/emulator remains
available for later operations. Log
`artifacts/logs/sio-cross-aid-vpcd-20261106T113216Z.log`, SHA-256
`f7c7094bab23f6339fc74e2bd3147f36044f45d64d288f8b1b22e350e882da5a`.
Next: cover omitted structural entries and continue physical retry when PC/SC
responds.

## Latest continuation — malformed Export offset rejected

The generated service CAP fixture has no Export static-method entry, so the
negative test now injects one zero-offset method record into its Export
component and updates Directory's tag-10 payload size. CAP LOAD succeeds and
Applet INSTALL rejects the invalid structural entry with `6A80`. The focused
VPCD run passes (`TEST:SUMMARY pass=1 fail=0`) at
`artifacts/logs/sio-cross-aid-vpcd-20261106T114248Z.log`, SHA-256
`9f272a603d610a51df1a8ab76b76372f04c6e4e555ff0080b07b546ff4941c6f`.
No upstream CAP or runtime source was modified. The fixture mutation changes
only its generated CAP input; original archives remain intact.

Together, the focused negative cases now cover an unlisted Applet interior
offset, an invalid Class method-table offset, an invalid internal
ConstantPool `StaticMethodref` (`6F00` at install validation stage 9), and an
invalid Export offset. Next: test omitted table entries and repeat GPPro
Descriptor-free installation on physical hardware once PC/SC responds.

---

# Active handoff — SensitiveResult.reset across SIO — 2026-11-06 11:56 CET

## Latest continuation — caller reset lifecycle tested

Added APDU `803E000000`: the client receives a `Util.arrayCopyNonAtomic`
SensitiveResult through SIO, asserts it, calls `SensitiveResult.reset()`, then
verifies another assertion throws `SecurityException`. Focused VPCD passes
1/1 and full cross-AID VPCD passes 34/34. Test commit: `17950d66`.

The physical exact-reader probe now also includes `803E`, but physical
validation is still blocked before card traffic: USB enumerates three Pico
devices with CCID descriptors while `pcsc_scan -r` lists only VPCD readers.
The probe compiled with `./tools/env javac`; no firmware code changed.

Focused log `artifacts/logs/sio-cross-aid-vpcd-20261106T105304Z.log`, SHA-256
`99f2f9819ec209e3b31e7dd9fb970fafe6502fe6be2c268b19f902113652cec3`; full log
`artifacts/logs/sio-cross-aid-vpcd-20261106T105336Z.log`, SHA-256
`032aa65b9ad2f4b01c5a30f12147e08d5d7f218c639bd3ea3f593dbd588cc1c3`.

Next: continue high-priority native runtime work offline. Retry physical APDUs
after a Pico appears in `pcsc_scan -r`.

---

# Previous handoff — atomic Util SensitiveResult SIO — 2026-11-06 11:50 CET

## Latest continuation — atomic Util results asserted across SIO

Added an interpreted service method covering `Util.arrayCopy()`,
`Util.arrayFill()`, and `Util.arrayCompare()` SensitiveResult values. It checks
success results in the service, verifies an invalid atomic copy leaves the
result unassigned, and the client asserts the final comparison result after
SIO returns. The focused VPCD case passes, and full cross-AID VPCD passes
34/34. The physical SmartcardIO probe now includes APDUs `803B` (AID,
including RIDEquals) and `803D` (atomic Util).

Test commit: `d37ca778`. The first focused attempt exposed an incorrect test
expectation: comparing `{1,2}` with a destination filled with `{1,1}` correctly
returns `+1`; the fixture was fixed to assert that result, then focused and
full VPCD reruns passed. The physical probe source compiles through
`./tools/env javac`; hardware was intentionally not retried because the Pico
USB devices remain absent from `pcsc_scan -r`.

Focused log `artifacts/logs/sio-cross-aid-vpcd-20261106T104555Z.log`, SHA-256
`a6d4af2d1f7f1f4abbd25209293cfde06c3e5d292a06f71fb10b5c2cd778e943`; full log
`artifacts/logs/sio-cross-aid-vpcd-20261106T104628Z.log`, SHA-256
`c95c5b8220d1f3671a6ced468e750023e257debb582ce3106ad625ed8ad7209d`.

Next: keep advancing CTX/other high-priority native work offline. Resume the
physical probe only when a Pico reader is registered in `pcsc_scan -r`.
Preserve unrelated changes.

---

# Previous handoff — AID.RIDEquals SensitiveResult — 2026-11-06 11:40 CET

## Latest continuation — RIDEquals true/false results covered through SIO

Extended the interpreted AID SIO case to assert true for
`AID.RIDEquals(previous)` and false for `AID.RIDEquals(null)`. The focused AID
case and full cross-AID VPCD suite pass on the rebuilt PC `symm_crypto` VM
(34/34). Test commit: `cc9a072f`.

Focused log `artifacts/logs/sio-cross-aid-vpcd-20261106T103631Z.log`, SHA-256
`662946299d973675114e4c6d828a6ee798a7e6297cd057f09d56a183450bb5ae`; full log
`artifacts/logs/sio-cross-aid-vpcd-20261106T103721Z.log`, SHA-256
`bfa8b510557c54e5e3795bbbd03badda3533d270cedc498822906583735925c4`.

The physical SmartcardIO probe now sends `803B000000` expecting `6A319000`,
but no hardware result is available yet: the latest run stopped before card
traffic because `pcsc_scan -r` listed only the two VPCD readers despite three
USB-enumerated Pico devices with CCID descriptors. QKB and the previous
handoff section record the host registration diagnosis. Retry the physical
probe only when a Pico appears in `pcsc_scan -r`.

Next: continue the high-priority native runtime backlog offline while physical
PC/SC registration is absent. Do not change upstream CAPs or implement
cryptographic behavior inside test CAPs.

---

# Previous handoff — Physical AID SensitiveResult probe — 2026-11-06 11:32 CET

## Latest continuation — AID physical check added, reader registration dropped

Added an exact-reader physical APDU check (`803B000000`, expected
`6A319000`) to cover AID boolean SensitiveResult assertions through SIO. The
updated physical harness built the fixture and compiled the probe, but GPPro's
initial read-only `--list` failed before any package LOAD/INSTALL because the
physical PC/SC reader was no longer registered. `lsusb` still showed all three
Pico 2 devices with CCID interface descriptors; `pcsc_scan -r` showed only the
two virtual readers after the authorized access helper ran. This is recorded
in QKB; no pcscd restart or configuration change was attempted.

Previous physical suite passed with JCSystem result `6A8B9000`, including
cleanup, at `artifacts/logs/sio-cross-aid-physical-20261106T102553Z.log`
(SHA-256 `1290201a5f4f78b558ac32cb76703808a45cd16542c1dc38dcddaa0dedd7952f`).
Updated attempt stopped before card traffic; GPPro log:
`build/sio-cross-aid-fixture/physical-gp-list-20261106T102843Z.log` (SHA-256
`a1db974ee9a6a906729879ff24ea6431c5e0bd52fe0561b1697d37a417e11d09`).

Next: continue offline/emulator implementation. Retry the updated physical
probe only when a Pico reader reappears in `pcsc_scan -r`; use its exact
current name. Preserve all unrelated worktree changes.

---

# Previous handoff — SensitiveResult GC roots — 2026-11-06 11:23 CET

## Latest continuation — object result is the sole native GC root

Extended `tools/tests/rmi_registry/rmi_registry_test.cpp` with a real heap
array. The test sets it as the VM-wide object SensitiveResult without adding
the handle to any VM stack roots, collects, verifies it remains reachable, then
resets SensitiveResult and verifies collection reclaims it and stale access is
rejected. `./tools/test-rmi-registry` passes with
`sensitive_result_gc_root=1`.

Test commit: `b2a72fbd`. No firmware source changed, so target builds and
physical testing were intentionally skipped for this test-only native heap
step. Combined with the interpreted SIO tests, ownership and propagation are
covered, but a single CAP cannot force synchronous GC through the public API;
there is no Java Card method to request it. See CTX-02 in `TODO.md` for that
scope and the remaining broader physical acceptance.

Next: broaden physically validated SensitiveResult coverage, then continue the
remaining high-priority plan items. Preserve unrelated dirty files.

---

# Previous handoff — JCSystem SensitiveResult through SIO — 2026-11-06 11:20 CET

## Latest continuation — JCSystem shareable object result fixed and verified

Fixed the intercepted `JCSystem.getAppletShareableInterfaceObject()` static
call path: it now clears stale SensitiveResult before validation, publishes the
returned proxy or null only after the helper completes successfully, and then
pushes the same return value. Exceptions remain unassigned. The interpreted
three-package CAP test asserts successful proxy and denied/null results, and
also checks `lookupAID()` identity.

Source/test commit: `d9503b8e`; source implementation and test had been
committed before this documentation/progress commit. On Pico 2
`D92E743D5F640C0F`, the physical SIO probe returned `6A8B9000` and cleanup
removed the temporary applets/packages. ARM and RISC-V builds passed; a finite
ARM cycle reported `TEST:SUMMARY pass=1 fail=0`. The rebuilt PC `symm_crypto`
VPCD focused JCSystem test passes 1/1, isolated AES-MAC Signature passes 1/1,
and the full cross-AID suite passes 34/34.

The prior isolated Signature failure was caused by running the fixture against
a stale `current` PC VM binary. The fixture wrapper builds its CAPs but does not
build the VM; AES-MAC is unavailable in `current`. Rebuild first with
`./tools/build-pc --profile symm_crypto` (or `max_crypto`). QKB records the
reader-channel suffix re-enumeration issue: refresh the reader helper and use
the exact name from `pcsc_scan -r` before hardware APDUs.

Next: continue CTX-02 with interpreted SIO object-result GC rooting after local
references are dropped. Existing object-result tests cover ownership and GC
roots at native/context level; no new Java provider or CAP implementation is
allowed. Keep unrelated worktree changes intact.

---

# Previous handoff — AID SensitiveResult through SIO — 2026-11-06 10:15 CET

## Latest continuation — AID boolean results verified

Extended the existing interpreted service method that checks
`JCSystem.getAID()` and `getPreviousContextAID()` to also assert the boolean
SensitiveResult values from `AID.partialEquals()`, `AID.equals(byte[],...)`,
and `AID.equals(AID)`. This confirms the AID results survive the SIO return;
the VM implementation already handled these calls correctly. The focused AID
VPCD test and full cross-AID VPCD suite pass (34/34). No runtime source change,
target build, or physical hardware was involved.

Test commit: `e938db26`; progress record follows. Full log:
`artifacts/logs/sio-cross-aid-vpcd-20261106T091117Z.log`, SHA-256
`08f6f65cf48d37097c0d640b2dd9f6b25d4534c15551cb727c53a9c0ef3177d2`.
Focused log: `artifacts/logs/sio-cross-aid-vpcd-20261106T091027Z.log`, SHA-256
`e69811575add0b5749532da8af7850ec2eb5fd02ed03b1bb2de58a5081e4053a`.

Next: continue CTX-02 by inspecting the other currently enabled key API result
producers, then add interpreted SIO coverage for object SensitiveResult rooting
after ordinary local references are dropped. Checksum and AEAD remain excluded
because those providers are unavailable/disabled. Preserve unrelated dirty
files and rebuild the PC VM before native VPCD tests.

---

# Previous handoff — SensitiveArrays SensitiveResult through SIO — 2026-11-06 10:00 CET

## Latest continuation — unsupported array capability results now recorded

Native `SensitiveArrays.isIntegritySensitive()` and
`isIntegritySensitiveArraysSupported()` previously returned `false` without
updating the SensitiveResult channel, despite their API contract. They now
clear stale state at entry and publish boolean false after successful calls;
exceptions still leave the channel unassigned. The interpreted three-package
fixture asserts each result in the service and checks the final one after SIO.

Focused SensitiveArrays VPCD passed; full cross-AID VPCD passed 34/34 on a
rebuilt `symm_crypto` PC VM. ARM `current` and RISC-V `current` builds pass.
ARM image ends at `0x100D4B48`, leaving 177,336 bytes. No physical device was
used. Python syntax and scoped CRLF-aware diff checks pass.

Source/test commit: `4234bf7f`; progress record is at the end of `progress.md`.

Current API Checksum factory branches both throw `NO_SUCH_ALGORITHM`, and no
AEAD provider is enabled; leave those out of runtime result coverage until the
algorithms are implemented and selected. Next: cover remaining enabled key
results and the object-result GC-rooting case after ordinary locals are
released. Preserve unrelated dirty files.

---

# Active handoff — RandomData SensitiveResult through SIO — 2026-11-06 09:41 CET

## Latest continuation — RandomData result producers covered

Added framework `RandomData` virtual-return capture for `getAlgorithm()` and
`nextBytes()`. A three-package interpreted fixture creates the currently
supported secure-random provider, checks the algorithm byte and returned end
offset in the service, and asserts both results after SIO.

Focused RandomData VPCD passed; the complete cross-AID suite passed 34/34 on a
freshly rebuilt PC `symm_crypto` VM. ARM `current` and RISC-V `current` builds
pass. The ARM image ends at `0x100D4B20`, leaving 177,376 bytes. Python syntax
and scoped CRLF-aware diff checks pass. No physical hardware was used.

Source/test commit: `f2169c73`; progress record is at the end of `progress.md`.

Next: cover Checksum and enabled AEAD/key result producers, then test object
SensitiveResult GC rooting after ordinary local references are dropped through
interpreted SIO. Preserve unrelated worktree changes. All PIN and RandomData
evidence remains emulator-only.

---

# Active handoff — concrete PIN SensitiveResult calls — 2026-11-06 09:23 CET

## Latest continuation — concrete OwnerPIN invocation covered

Extended SensitiveResult capture to Java Card `OwnerPIN` calls dispatched by
`invokevirtual`, in addition to the prior PIN/OwnerPINx `invokeinterface`
path. The interpreted three-package fixture checks concrete
`getTriesRemaining`, successful and failed `check`, and `isValidated`, then
asserts those results after returning through SIO.

Focused PIN/OwnerPINx/OwnerPIN VPCD passes; the full cross-AID regression passes
34/34 with the rebuilt PC `symm_crypto` VM. ARM `current` and RISC-V `current`
builds pass; the ARM image ends at `0x100D4AB8`, leaving 177,480 bytes. Python
syntax and scoped CRLF-aware diff checks pass. No physical device was used.

Source/test commit: `4f3be8c3`; progress record is at the end of `progress.md`.

Next: continue CTX-02 with RandomData, checksum, AEAD and key API result
producers, then object-result GC rooting after ordinary locals are dropped.
Interface and concrete OwnerPIN assertions are currently emulator-only.
Preserve unrelated dirty files.

---

# Active handoff — PIN SensitiveResult through SIO — 2026-11-06 09:03 CET

## Latest continuation — interface-based PIN result propagation

`invokeinterface` now marks interpreted Java Card framework PIN result frames.
On successful `sreturn`, the VM publishes the returned byte/boolean as
SensitiveResult. The service fixture and three-package SIO caller check
`PIN.getTriesRemaining`, `PIN.check` true and false, `PIN.isValidated` true and
false, `OwnerPINx.getTryLimit`, and
`OwnerPINxWithPredecrement.decrementTriesRemaining`.

Validation passed: focused PIN VPCD and the full cross-AID VPCD suite (34/34)
on a freshly rebuilt `symm_crypto` PC emulator; ARM `current` and RISC-V
`current` wrappers pass. ARM firmware ends at `0x100D4A30` with 177,616 bytes
remaining. No physical hardware test was run. The first OwnerPINx attempt used
an old PC emulator and failed; rebuilt PC passed. QKB already records that the
VPCD wrapper consumes an existing PC binary, so rebuild with
`./tools/build-pc --profile symm_crypto` after native edits.

Source/test commit: `35bbbfaf`; progress record is at the end of `progress.md`.

Next: add equivalent coverage for PIN methods called through concrete-class
`invokevirtual` (current fixture calls the sensitive methods through
interfaces), then continue with RandomData/checksum/AEAD result producers and
GC rooting of returned objects. Preserve unrelated dirty worktree changes and
do not claim physical acceptance without a real board test.

---

# Active handoff — SIO SensitiveResult profile validation — 2026-11-06 06:11 CET

## Latest continuation — KeyBuilder SensitiveResult results

Both native `KeyBuilder.buildKey()` overloads in `choupi/src/native_key.cpp`
now clear stale SensitiveResult state before validation/construction and publish
the returned key object only on success. The interpreted three-package SIO
fixture proves that the caller can assert the created key identity and that an
invalid key type leaves the result unassigned. Focused VPCD passes 1/1; full
cross-AID VPCD passes 34/34. PC `symm_crypto`, ARM `current`, and RISC-V
`current` builds pass. Python syntax and CRLF-aware scoped diff checks pass.
No physical device was used.

Implementation/test commit: `9ffe4da05a9893dede9da4e2e89a378a41f7b81a`.
Progress record is in `progress.md`. Logs: focused
`artifacts/logs/sio-cross-aid-vpcd-20261106T052220Z.log` (SHA-256
`46d7ce6ffaf930e1ab52ac72ae49685d3a151ab5df2c2edc07b373a04e70dc99`), full
`artifacts/logs/sio-cross-aid-vpcd-20261106T052256Z.log` (SHA-256
`def9f6dbb967b3df15eb2a6ea1679e18ab6c2b897a4aabadc48b0d34ad1af4bf`).

Next: continue CTX-02 with other native result producers (PIN, random-data,
checksum, AEAD and remaining key APIs), then test object-result GC rooting
through interpreted SIO after ordinary locals are dropped. Preserve unrelated
dirty files; do not claim physical validation without flashing and a finite
serial log.

`JCSystem.makeGlobalArray()` now clears stale `SensitiveResult` state and
publishes the returned global array as an object result in native
`choupi/src/jni.cpp`. The interpreted three-package fixture returns a global
byte array from its service applet and asserts the same object after the SIO
call. Its isolated PC VPCD test passes (`TEST:SUMMARY pass=1 fail=0`). A first
fixture draft used an `Object[]` containing a nested byte array; the VM
correctly rejected that unsupported global-array nesting. The test now uses a
global `byte[]`.

Builds: PC `symm_crypto`, ARM `current`, and RISC-V `current` pass. ARM reports
177,888 bytes remaining. The full VPCD suite passes 34/34 when run against the
rebuilt `symm_crypto` PC VM, including AES-MAC Signature and the new global-array
case. The isolated Signature case passes too. Running the same suite with PC
`current` correctly fails because that profile disables AES-MAC; the VM reports
`CryptoException.NO_SUCH_ALGORITHM` (reason 3). This was a test-profile mismatch,
not a runtime regression. The test harness now documents the required profile
and includes an isolated Signature mode.

The `makeGlobalArray` implementation/test commit is `438e8581`; its metadata
follow-up is `fc268448`. Profile diagnosis and harness clarification are
committed as `e13071af`. No physical device was used.

Next: continue CTX-02 with the remaining native result producers (PIN,
random-data, checksum, AEAD and key APIs), then extend GC-rooting coverage for
object results after locals are dropped. Preserve unrelated dirty files.

# Active handoff — exception table protected ranges — 2026-11-06 05:20 CET

## Latest continuation — active exception range validated

Exception-table `start_pc` conversion now accounts for the one-based CAP coordinate after the handler table. `doThrow()` checks Method bounds and validates instruction alignment/frame containment only for the range active at the current throw PC. The exclusive end is required to stay within the frame, but is not required to be an opcode boundary. A malformed range starting inside a `BSPUSH` operand fails with `6F00`; recovery checks pass 6/6. Catch CP and handler destination malformed cases also pass 6/6. The unchanged JCAlgTest positive VPCD suite passes 15/15, and ARM/RISC-V/PC builds pass. Unit PC bounds checks pass 24/24. Source commit: `9951d0ac`; progress/QKB/TODO metadata follows it. No physical hostile-CAP test was run.

## Latest continuation — range overflow fixture added

The malformed suite now includes an active 32,767-byte catch-all range whose end exceeds the Method image. VPCD rejects process with `6F00` and confirms recovery (6/6). Regression commit: `5a4abd65`; its progress record and metadata follow. No source/build/hardware change in this test-only step.

## Latest continuation — exception range start bounds covered

An active handler range with `start_pc=0xFFFF` is now tested: VPCD rejects the process with `6F00` and recovery passes 6/6. Test commit: `3e20fbb4`; progress/QKB/TODO metadata follows. No production source/build/hardware change in this test-only step.

Next: audit malformed Method exception-table framing/counts and continue separating emulator evidence from physical recovery acceptance.

# Active handoff — exception-handler target boundary — 2026-11-06 04:51 CET

## Latest continuation — handler target into an operand rejected

The runtime previously accepted an active catch-all handler whose offset pointed into an INVOKESTATIC operand. Its process APDU returned `9000`. `Bytecodes::doThrow()` now transfers through the current frame PC bounds and decoded instruction-start check. The generated regression now returns `6F00` for the malformed process and confirms ISD/applet reselection recovery (6/6). The neighboring out-of-range catch CP case also passes 6/6. ARM, RISC-V and PC builds pass; unchanged upstream JCAlgTest VPCD passes 15/15. Instruction-boundary sanitizer helper passes 13/13 with 100,000 malformed inputs. No physical hostile-CAP test was run. Source/test commit: `52f51fe1`; progress and knowledge-base metadata are recorded after it.

Next: continue reviewing malformed exception table range/entry semantics; keep physical hostile-CAP recovery explicitly open.


## Critical continuation — extended method header was one byte overestimated

Corrected `src/cap_method_bounds.hpp` and `src/cap_method_body_bounds.hpp` to
use a four-byte extended header (normal headers remain two bytes). JCVMS 3.0.5
§6.10.3 specifies `flags/padding`, `max_stack`, `nargs`, and `max_locals`, and
CHOUPI's packed header struct and `callMethod()` bytecode pointer agree. The
old five-byte assumption moved each Descriptor-derived extended frame start
one byte into its bytecode. Also corrected the deployed-CAP scanner, malformed
fixture header helper, and extended-header tests; a unit `static_assert` ties
the expected size to `sizeof(jc_cap_extended_method_header_info)`.

Evidence: scanned 3,973 concrete Descriptor methods across deployed CAPs and
found eight extended methods across versions. The 3.0.5 deploy suite contains
three; those were exactly the three “one-byte overrun” cases previously
reported. After correction, all 978 3.0.5 concrete method bodies end exactly
at the next method. Branch scan checks 8,720 destinations: zero out-of-range,
zero misaligned, zero clamps. Sanitizer helper suites pass: method header 7/7,
method body 11/11, PC bounds 19/19, instruction boundaries 13/13 including
100,000 deterministic malformed inputs.

Validation: ARM, RISC-V, and PC builds pass. Unchanged upstream JCAlgTest VPCD
install and APDU run passes 15/15; its full log is
`artifacts/logs/rv28-four-byte-extended-header-jcalgtest-full-vpcd-20261106.log`.
An upstream 3.0.5 Crypto API CAP containing extended methods loads/selects
successfully (2/2), but that does not prove a specific extended method ran.
Malformed `impdep1` runtime rejection and manager recovery pass 6/6. The ARM
UF2 is 1,741,824 bytes, SHA-256
`69f64c673ce1158b56449027454f5114406cab0a39ac3d1adf3a6fa86c3e17bb`.

Physical check: authorized enumeration sees three Pico 2 CCID USB devices:
`2BED26ED20556535`, `8E464F4E2BBC0A19`, and `D92E743D5F640C0F`. A bounded
8-second monitor on `D92E743D5F640C0F` repeatedly reports
`TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`. This only validates
the firmware currently on that device; no new image was flashed, so it does
not establish the header fix on hardware. `pcsc_scan -r` timed out without
output despite the helper granting access; PC/SC reader status remains
unconfirmed.

Commit: `618d1f1a` (`Correct extended CAP method header size`), with metadata
follow-up `913eb1d2`. New repeatable runtime test
`tools/test-api-crypto-extended-method-vpcd` verifies the upstream Crypto CAP
hash and sends the official API dispatch for AES-CBC-NOPAD `Cipher.crypt()`.
The bundled `ApiTestCryptoCipher.crypt(byte, byte[])` has 20 local slots. The
CAP applet returned `000020019000` (32-byte result, known-answer pass), and the
VPCD harness passed 3/3. Log:
`artifacts/logs/rv28-extended-crypto-crypt-api-vpcd-20261106.log`.

The runtime-regression step is committed as `c82c2be2` (`Exercise extended-header crypto method in VPCD`). Continue RV-28 malformed interpreter paths and linkage validation. Physical extended-method
execution remains open; the currently installed board firmware passed its
runtime monitor but the new header-fix image was not flashed. Keep unrelated
dirty files untouched.

## Latest RV-28 runtime cases — JSR/RET, conditional and switch targets / impdep2

Extended `tools/tests/cap_malformed_vpcd_test.py` with `impdep2`, taken short
and wide conditional branches into their own operands, and ITABLESWITCH,
SLOOKUPSWITCH, and ILOOKUPSWITCH targets into their operand tables. All six
cases pass 6/6: malformed applet SELECT returns `6F00`, direct process returns
`6999`, and ISD FCI/`9000` selection remains available before and after the
rejected applet reselect. The upstream CAP remains unchanged at SHA-256
`b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
Added JSR +1 targeting its own displacement operand and RET reading process
local 0 (a receiver reference rather than a return address). Both VPCD cases
pass 6/6 with the same rejected applet SELECT/direct APDU sequence and
successful ISD recovery. The RET case initially exposed a wrong test
expectation: SELECT itself returns `6F00`; after matching that observed
behavior, recovery passed. Logs and hashes are in the latest progress record.
Per-case log hashes are in the latest progress record. No firmware build or
physical CAP test was needed; this modifies only the offline fixture.

The conditional/switch fixture step is committed as `d3597a27` and recorded in
metadata commit `25b58492`. Next: inspect the remaining malformed
ConstantPool/interpreter call edges and consider a bounded sanitizer runtime
recovery path. Physical hostile-CAP recovery remains open.

The JSR/RET fixture step is committed as `11669af0` (`Test malformed JSR and
RET execution`). Its progress record includes both VPCD log hashes. Preserve
the other dirty worktree files; they were not part of these commits.

## Latest RV-28 call edge — out-of-range ConstantPool invocation operand

Added `invoke-static-cp-out-of-range` to `tools/tests/cap_malformed_vpcd_test.py`.
It replaces the generated process method start with complete INVOKESTATIC,
INVOKEVIRTUAL, INVOKESPECIAL, or INVOKEINTERFACE instructions using CP index
`0xFFFF`; the source JCAlgTest CAP is still hash-verified and unchanged. All
four PC VPCD variants pass 6/6 with SELECT `6F00`, process `6999`, and ISD
recovery before and after applet reselect. Initially the active
`JCVM_ARRAY_SIZE_CHECK` configuration made `JCVMArray::at()` reject the
out-of-range CP slot. The source now also performs an unconditional
declared-count check in `getCPEntry()` and throws `SecurityException`; its CP
accessor chain no longer has conditional `noexcept` specifications. All four
opcode variants were rerun against the changed source. Their logs were
overwritten by those reruns, so use the replacement hashes in the latest
progress entry rather than the earlier per-case hashes.

Next: audit the other method-call operands and catch-type CP lookups for
runtime range protection; retain the distinction between validation from the
current array-check configuration, no-check builds, sanitizers, and physical
card behavior.
The initial INVOKESTATIC case and audit are committed as `d4e13a32`; all four
invocation opcodes are covered in `00165ee1` with complete operands and
recovery checks. The source-level CP guard and validation summary are committed
as `6e751a98` with metadata commit `a59b998d`. All four VPCD cases pass against the rebuilt PC VM;
the unchanged upstream JCAlgTest VPCD regression passes 15/15. ARM, RISC-V,
and PC builds pass. The full ARM image was flashed to Pico `8E464F4E2BBC0A19`
and the 18-second monitor passed `TEST:SUMMARY pass=1 fail=0`; the monitor does
not exercise malformed CP behavior. Cycle log SHA-256:
`238b7d76ec36311326991e21e47f64b37c5a54add9f600283006da643b698644`.

The commit SHA, build hashes, physical log and latest PC log hashes are in the
progress record.

## Latest RV-28 exception-dispatch case — malformed active catch CP index

The generated `exception-catch-cp-out-of-range` case changes active handler
row 39's catch-type index to `0xFFFF` and replaces the first instruction in its
protected process range with complete `INVOKESTATIC 0xFFFF`. The upstream CAP
hash remains unchanged. PC VPCD passes 6/6: initial SELECT `9000`, process
returns `6F00`, ISD returns FCI/`9000`, applet reselect returns `9000`, and a
second ISD selection succeeds. The first run failed only because the fixture
incorrectly expected SELECT to execute the process method; correcting that
expectation produced the full passing recovery sequence. Log SHA-256:
`e5c72c2245565b7709c74f707fa7f6c92dc53fddbea4c6c0a03eebf7e8fa5f70`.

Historical note: the catch-index fixture and knowledge-base step was committed
as `0443d57e`; its progress metadata follow-up is complete. The operand-target
runtime fix and adjacent invalid-target regression are recorded above.

## Previous handoff — RV-28 malformed switch runtime rejection — 2026-11-06 02:45 CET

## Latest continuation — dynamic STABLESWITCH operand target rejected

Added `switch-into-operand` to the generated malformed CAP fixture. It starts
JCAlgTest `process()` with `BSPUSH 0` and a one-entry `STABLESWITCH`; both the
default and selected case target byte 1 of the switch, inside its first
displacement operand. The upstream CAP remains unchanged.

PC VPCD passes 6/6: applet SELECT `6F00`, process APDU `6999`, ISD FCI/`9000`,
reselect applet `6F00`, and ISD FCI/`9000`. Log:
`artifacts/logs/rv28-malformed-switch-into-operand-vpcd-20261106.log`, SHA-256
`1f221dd2e441cf5422cf751a5cdc2248a0216de1af1107ff7aff426b9bdf80d9`.
The PC-bound and instruction decoder helper suites pass under ASan/UBSan
(19/19 and 13/13 with 100,000 fuzz inputs); method-body bounds also pass
11/11 under ASan/UBSan. No physical device or firmware build was involved.

Next: decide whether any remaining switch form or conditional-taken path adds
distinct acceptance; then perform an exact focused sanitizer sweep and review
runtime exception containment. Physical recovery and Descriptor-free exact
method-start proof remain open.

## Previous handoff — RV-28 bytecode-bound sanitizer coverage — 2026-11-06 02:43 CET

## Latest continuation — run PC and method bound helpers under sanitizers

Enabled AddressSanitizer and UndefinedBehaviorSanitizer in
`tools/test-cap-bytecode-pc-bounds` and
`tools/test-cap-method-body-bounds`, using the repository wrappers. Both pass:
PC bounds 19/19 and method ranges 11/11. `tools/test-cap-instruction-boundaries`
also passes 13/13 under ASan/UBSan, including 100,000 deterministic malformed
bytecode inputs. Sanitizer environment disables LeakSanitizer because its
initialization is restricted in this VM; ASan and UBSan remain active.

No production C++ was changed, so platform builds and flashing were not run.
Prior offline malformed VPCD cases pass for `impdep1`, truncated `SIPUSH`,
`GOTO +1` into its operand and out-of-frame `GOTO_W`. Next assess whether a
distinct malformed switch/conditional path adds coverage, then validate runtime
exception containment under the host VPCD if practical. Keep physical recovery
and Descriptor-free exact method-start acceptance open.

## Previous handoff — RV-28 out-of-method branch runtime rejection — 2026-11-06 02:41 CET

## Latest continuation — dynamic `GOTO_W` beyond frame is rejected

Added the `branch-outside-method` fixture, replacing the first three bytes of
JCAlgTest `process()` with `GOTO_W +32767` (`A8 7F FF`), which exceeds the
Descriptor-bounded method frame. The unchanged upstream CAP SHA is still
`b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.

Offline PC VPCD passes 6/6: applet SELECT `6F00`, direct process `6999`, ISD
selection FCI/`9000`, applet reselect `6F00`, and ISD selection FCI/`9000`.
Log: `artifacts/logs/rv28-malformed-branch-outside-method-vpcd-20261106.log`,
SHA-256 `46d1376d3ce64f8710ef231d0bfec6f441e0fb85158c060ec045274ea6cd7b92`.
No C++ source changed; no build, sanitizer, physical test, or flash was done.

Next: continue malformed runtime coverage only where it can distinguish a
separate guard; then run the focused interpreter-related sanitizer tests and
review runtime exception containment. Keep physical recovery and Descriptor-
free exact method-start assurance open. Preserve unrelated worktree edits.

## Previous handoff — RV-28 malformed branch runtime rejection — 2026-11-06 02:38 CET

## Latest continuation — dynamic `GOTO` into its operand is rejected

The malformed fixture supports `branch-into-operand`: it replaces the first
two bytecode bytes in JCAlgTest `process()` with the valid two-byte JCVM
instruction `GOTO +1`, whose target is the displacement operand itself. A
first bad fixture encoded a 16-bit displacement for JCVM's 8-bit GOTO and
made a self-loop; the 20-second VPCD APDU timeout caught that fixture error.
The corrected fixture completed normally.

PC VPCD passes 6/6: target applet SELECT returns `6F00`, process APDU returns
`6999`, ISD selection returns its FCI/`9000`, applet reselect returns `6F00`,
and ISD selection succeeds again. Log:
`artifacts/logs/rv28-malformed-branch-into-operand-vpcd-20261106.log`, SHA-256
`8cda8aa982a28cfa74acd94955fed3a79ef052604a789e013c8dedb1439ff7d8`. The
upstream CAP hash remains unchanged. Emulator-only; no firmware build, physical
test, or sanitizer run was done.

Next: consider invalid out-of-method branch targets and improve fail-closed
runtime coverage. Keep Descriptor-free exact method starts and physical/
sanitizer hostile-CAP recovery as open items. Preserve unrelated changes.

## Previous handoff — RV-28 truncated operand runtime bounds — 2026-11-06 02:32 CET

## Latest continuation — truncated `SIPUSH` operand fails closed and recovers

Extended `tools/test-cap-malformed-vpcd` with a `truncated-sipush` fixture.
It reuses the unchanged JCAlgTest CAP and replaces the final `BC_RETURN` in
the Descriptor-bounded `process()` body with `BC_SIPUSH`; that bytecode's two
operands would cross the method end. The generated LOAD byte is at Method file
index `0x4798`; upstream CAP SHA-256 remains
`b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.

Offline VPCD passes 6/6: applet SELECT `9000`, malformed process APDU `6F00`,
ISD SELECT returns FCI/`9000`, applet reselection `9000`, and another ISD
selection returns FCI/`9000`. Process APDUs took 15.110 and 15.461 seconds;
connect took 18.347 seconds. Log:
`artifacts/logs/rv28-malformed-truncated-sipush-vpcd-20261106.log`, SHA-256
`2bdda893892a1056bbc2558083ccab2e5f0299a35669e1fc55c2b24c46274456`.
Validation is PC VPCD only; no physical device or sanitizer was used.

Next: add a runtime branch target into an instruction operand and verify the
applet/manager recover. Keep malformed runtime sanitizer and physical recovery
acceptance open. Preserve unrelated worktree changes.

## Prior handoff — RV-28 malformed dynamic opcode recovery — 2026-11-06 02:24 CET

## Latest continuation — dynamic-package `impdep1` rejection and valid-CAP regression

Added `tools/test-cap-malformed-vpcd`, which builds a generated LOAD-code
fixture from the unchanged upstream `AlgTest_v1.8.2_jc305.cap`. Its SHA-256 is
`b5a7711c6d1992d38b5dd2c529a41498df3a167c643ca358424e4a1c1fa05169`.
The fixture confirms the target is JCAlgTest's public `process()` method at
Method offset `0x3E63`, then changes its first body opcode at assembled LOAD
index `0x467F` from `0x19` to dynamic-package `impdep1` (`0xFE`). Only ignored
`build/` output is mutated; the upstream CAP stays unchanged.

Offline PC VPCD result: malformed applet SELECT returns `6F00`, direct applet
processing returns `6999`, and selecting the ISD after both failures returns
its FCI and `9000`; malformed test summary is 6/6. The unchanged upstream
JCAlgTest regression passes 15/15, including select, GETVERSION, JCSystem,
SHA-256, AES, RSA, secure-random and CRC paths. Logs:
`artifacts/logs/rv28-malformed-impdep-vpcd-20261106.log` and
`artifacts/logs/rv28-normal-vpcd-after-status-harness-20261106-rerun.log`.
Restricted sandbox execution returned EPERM before connecting; the offline
VPCD wrapper completed under escalated process permissions. This is not USB
access, a hardware check, or a firmware failure. No physical flash occurred.

Next: add malformed operand/truncated-instruction and branch-target runtime
cases, then run under sanitizers if supported. The Descriptor-free exact
StaticMethodref-start assurance gap remains open. Physical validation is still
blocked: earlier only one of three JCVM USB boards appeared in PC/SC, and its
bounded GPPro read-only listing timed out. Preserve unrelated worktree edits.

## Prior active handoff — RV-28 Descriptor-backed StaticMethodref validation — 2026-11-06 01:40 CET

## Latest device visibility refresh — 2026-11-06 01:47 CET

The authorized `sudo -n /usr/local/sbin/pico2-pcsc-reader` helper granted
pcscd access to three USB nodes. `lsusb -v -d 2e8a:0009` identified three
runtime JCVM CCID devices by unique serials `8E464F4E2BBC0A19`,
`2BED26ED20556535`, and `D92E743D5F640C0F`; none are in BOOTSEL. However,
`pcsc_scan -r` exposes only D92 plus the two VPCD readers. Thus the boards are
USB-visible, while two are not currently usable through PC/SC. No APDU, flash,
or firmware attribution was performed. This matches a prior unresolved
multiple-reader enumeration symptom; do not call it firmware failure. Continue
the RV-28 Descriptor-free method-target investigation offline and refresh the
helper before any later hardware action.

Follow-up at 01:54 CET: a read-only exact-reader GPPro `--list` against D92
produced no output and exited 124 at the 30-second timeout. Therefore D92's
appearance in pcsc_scan does not currently establish a working APDU path. No
firmware was flashed. Treat the earlier “no APDU” statement as applying only
to the initial enumeration command; the GPPro attempt may have reached PC/SC
but returned no observable protocol trace. Continue offline until the PC/SC
transaction path is diagnosed.

## Latest continuation — match StaticMethodref offsets to Descriptor — 2026-11-06 01:40 CET

When Descriptor is present, every internal StaticMethodref now must match
exactly one method descriptor's Method-component offset. Its range also follows
the handler-table base and one-based methods coordinate used by
`Method_Handler`. Descriptor-free LOADs retain range/header-fit checking only;
the exact-start gap remains open there. Instance fields use declared layout
bounds; virtual and super methods resolve through `Class_Handler`.

Validation: helper sanitizer suite 35/35; PC, ARM, and RISC-V max_crypto
builds; unchanged JCAlgTest VPCD install/APDU suite 15/15. ARM image ends at
`0x100ec798`, leaving 79,976 bytes in the firmware window. VPCD log:
`artifacts/logs/rv28-descriptor-static-method-vpcd-20261106.log`, SHA-256
`444284a42b9c84b8153618cf91b06d3adf81428f783a1fda3ea7ac09320c3fa2`.
No physical flash was performed.

Next: find a safe authoritative method-start map for Descriptor-free CAPs, then
add malformed interpreter-execution coverage. Preserve unrelated dirty changes.
This slice's commit is `5836a12f88cdea04cd5bbdc8e09e66d0aad666e8` (Validate
Descriptor static method targets).

Descriptor-free investigation: standard GPPro LOAD omits Descriptor. Class,
Applet, and Export tables independently identify listed callable methods, but
private/internal static targets are introduced through the ConstantPool offsets
under validation. Those offsets cannot self-authenticate an exact method
header. Current enforcement therefore stays at component/header bounds and
next enumerated target bounds; no exact-start claim is made. Stronger assurance
requires preserving Descriptor or an independently verifiable stream format.

## Previous continuation — bound external class tokens in Export tables — 2026-11-06 00:42 CET

## Previous continuation — bound external class tokens in Export tables — 2026-11-06 00:42 CET

Install preflight now validates external ConstantPool class tokens against the
target package's Export table. It resolves Import package tokens, applies
JC305 class-token mapping for uploaded packages, and rejects a token outside
`Export.class_count`. For ambiguous `CONSTANT_Classref`, either the class or
interface mapping may satisfy the bound; runtime dispatch checks the actual
class/interface use. Exported member tokens, internal targets, and malformed
interpreter execution remain open.

Tests: focused ASan/UBSan suite passes 16/16; PC emulator max_crypto build
passes; unchanged JCAlgTest VPCD install/APDU suite passes 15/15; ARM and
RISC-V max_crypto wrappers pass. VPCD log:
`artifacts/logs/rv28-export-token-vpcd-20261106.log`, SHA-256
`5401b45d0df8495fc31c5dca2127cc9dc51a4517bc3e4ee1cdf2bb78a36e622c`.
No physical card was flashed.

Next: validate imported static/instance field and method tokens using their
Export records; then check internal ConstantPool targets and construct
malformed interpreter runtime coverage. Preserve unrelated dirty worktree
changes. This slice's commit is `0fb196af` (Validate external CAP export class tokens).

## Previous continuation — validate external ConstantPool import tokens — 2026-11-06 00:31 CET

## Previous continuation — validate external ConstantPool import tokens — 2026-11-06 00:31 CET

RV-20261103-28 has a second validation slice. `validate_cap_imports` now
checks all six ConstantPool entry kinds: when the reference discriminator marks
an external link, its low-seven-bit package token must be below the CAP's
Import count. The existing pass still resolves each imported AID/package and
checks remote-interface references. This is not full semantic linkage
validation; exported class/member indices and internal class/member offsets
remain open.

Tests: `./tools/test-cap-constant-pool-validation` passes 11/11 under
ASan/UBSan; parser corpus passes 4,208 checks over 48 files and 100,000 fuzz
inputs; `./tools/build-pc --profile max_crypto`, ARM max_crypto and RISC-V
max_crypto wrappers pass. The unchanged upstream JCAlgTest VPCD run passes
15/15 (install, restore, selection, JCSystem, SHA-256/AES/RSA/random/CRC).
Log: `artifacts/logs/rv28-import-token-vpcd-20261106.log`, SHA-256
`23249161c70c958e3dfd53aded4c340cf73e13f100d4abaaa1ea087fececbb09`.
No physical card was flashed.

Next: validate external class/export/member token targets and internal class
and member references against structural metadata, preserving all deployed CAP
fixtures. Then add malformed interpreter execution coverage. Keep controlled
physical failure/recovery acceptance open. Preserve unrelated worktree changes.
This slice's commit is `a76989ec` (Validate external CAP import tokens).

## Previous continuation — reject undefined CAP ConstantPool tags — 2026-11-06 00:25 CET

RV-20261103-28 remains open. The CAP component parser now rejects ConstantPool
entry tags outside the six defined CAP 2.1/2.2 reference kinds, as well as
retaining its existing component count/length checks. The shared helper test
passes 7/7 under ASan/UBSan. `./tools/test-cap-parser` passes 4,208 checks
across 48 CAP files and 100,000 fuzz inputs; the deployed 3.0.5 scan passes
978 methods and 8,720 branch targets with three known descriptor clamps and
no invalid/misaligned targets. Native-opcode policy test passes 5/5. ARM and
RISC-V max_crypto wrapper builds pass.

LeakSanitizer itself fails to initialize in this VM because it runs under
ptrace restrictions. `tools/test-cap-constant-pool-validation` sets
`ASAN_OPTIONS=detect_leaks=0` by default while leaving ASan and UBSan enabled.
No hardware flash or malformed-CAP execution on an interpreter was done for
this parser-only slice.

Implementation/docs/progress commit: `9f71e640` (Reject undefined CAP constant
pool tags).

Next: implement/verify semantic ConstantPool linkage checks (internal class
offsets, imports, exports, fields and methods) without rejecting the valid
deployed CAP corpus; then add malformed bytecode interpreter execution under
sanitizers. Keep physical failure/recovery acceptance open. Preserve unrelated
dirty worktree changes.

## Previous continuation — VPCD session restore integration validated — 2026-11-06 00:15 CET

## Latest continuation — close RV-27 with current VPCD runtime evidence — 2026-11-06 00:15 CET

RV-20261103-27 is complete. The session unit suite passes 29/29, including
two known AIDs, different-AID switch, same-AID reselection, unknown AID,
rejected applet `select()`, corrupted restore, and distinct `6A82` versus
`6F00` outcomes. After `./tools/build-pc --profile max_crypto`, the bounded
VPCD JCAlgTest production callback integration passes 15/15. It installed and
restored JCAlgTest, selected it (`9000`), returned GETVERSION
`1.8.2_jc305`, JCSystem info, SHA-256 support/doFinal/static tests, AES/RSA,
random, CRC16 and expected unsupported-algorithm status. Connect took
18.601 s and full pass took 54.888 s. The prior pre-connect timeout is
superseded. Log:
`artifacts/logs/rv27-pc-vpcd-20261106.log`, SHA-256
`c35ad52fd4b3d85e7de8c143abbb32952713b7afcc8e547693ecb07117435d72`.
This was PC/VPCD-only; no physical device was used. ARM/RISC-V builds were
already validated with the implementation.

Next: RV-20261103-28, uploaded-CAP execution trust boundary. Preserve unrelated
worktree changes. RV-27 progress/TODO/QKB/handoff records are updated; this
validation commit `2ada2e15` (Validate VPCD applet restore integration).

# Prior continuation — CCID extension-error recovery validated — 2026-11-06 00:09 CET

## Latest continuation — preserve response across failed CCID extension — 2026-11-06 00:09 CET

RV-20261103-26 is implemented and marked complete. `driver_xfer` now treats
an asynchronous failed Bulk-IN time extension separately: it clears the
extension-in-flight flag but retains a final response that may already be
queued behind that extension. Bulk-OUT stays unarmed until final-response
completion. The shared recovery suite passes 21/21, including failure,
response retry, bounded-abort behavior, and subsequent slot-status/APDU
dispatch.

Both `./tools/build arm --profile max_crypto` and
`./tools/build riscv --profile max_crypto` pass. The updated ARM image was
flashed to Pico 2 `D92E743D5F640C0F`; finite serial monitor passed
`TEST:SUMMARY pass=1 fail=0`. PC/SC re-enumerated that board and the physical
lifecycle/routing test passed 36/36, then removed its disposable package.
No physical USB transfer error was injected; error behavior is covered by the
same helper called by the production callback. Cycle log:
`artifacts/logs/cycle-choupi-20261105T230607Z.log`; PC/SC log:
`artifacts/logs/cipher-lifecycle-pcsc-20261105T230800Z.log`.

Next: RV-20261103-27, stale VPCD session tests/reselection restore behavior.
Physical RV-11/12 lifecycle follow-ups remain open. USB helper most recently
granted three nodes; D92 is the PC/SC-visible reader. Preserve unrelated dirty
files. RV-26 source/test files:
`src/ccid_progress_flow.hpp`, `src/ccid_device.cpp`,
`tools/tests/ccid_progress_flow_test.cpp`; TODO/progress/QKB/handoff updated,
commit `8a90e71c` (Preserve pending CCID response on extension error).

# Prior continuation — CCID APDU routing and physical dispatch — 2026-11-05 23:52 CET

## Latest continuation — CCID APDU routing and physical dispatch — 2026-11-05 23:52 CET

RV-20261103-25 is implemented and its main physical acceptance passed. The
shared production CCID route now sends CLA 00 GET RESPONSE to the card manager
only while a segmented manager response is pending; otherwise it can reach
the selected applet. CLA 84 GET RESPONSE stays on the secure manager path.
The route unit suite passes 15/15; ARM and RISC-V `max_crypto` builds pass.

Pico 2 `D92E743D5F640C0F` ran the physical lifecycle probe on the current
ARM firmware. It passed default and explicit ISD SELECT while an applet was
selected, manager GET DATA while selected, unauthenticated protected GP GET
STATUS returning `6982`, then applet APDUs; complete suite `36/36`, cleanup
removed package `A000000151000203`. Log:
`artifacts/logs/cipher-lifecycle-pcsc-20261105T225050Z.log`, SHA-256
`9fefb8de1f2dbc0622797b2cecdc4e890498875f6e0e89ba0b12af448dee86f8`.
GPPro `--list` returned `ISD: A000000151000000 (OP_READY)`. The physical
router check does not inject nonzero slots or APDUs after power-off; those
states pass the shared router tests. Elevated USB sees three Pico boards, but
only D92 is currently registered by PC/SC.

Use the pinned GPPro build at
`.local-toolchain/globalplatformpro/tool/target/gp.jar` with
`--reader=<exact reader name>`. Passing a separate `--reader` argument to the
older deploy JAR stalled before APDU dispatch; do not treat that as a card
failure. Prior finite serial monitor showing stage 2 had no CCID traffic yet;
later telemetry showed stage 7 and repeated GetSlotStatus polling, and the
physical GPPro operations succeeded.

Next: continue with RV-20261103-26 (lost final CCID response recovery).
RV-11/12 physical lifecycle follow-ups remain open. Preserve unrelated dirty
worktree changes. RV-25 commit: `10a3ab66` (Fix CCID GET RESPONSE routing and
validate on Pico 2). Active implementation/test files for RV-25 are
`src/ccid_apdu_route.hpp`, `src/ccid_device.cpp`,
`tools/tests/ccid_apdu_route_test.cpp`, and
`tools/tests/CipherLifecyclePcscProbe.java`; root TODO/progress and QKB are
updated.

# Prior handoff — foreign references in applet-owned Object[] — 2026-11-05 22:51 CET

## Latest continuation — verify restored RSA transient key capacity — 2026-11-05 23:27 CET

RV-20261103-13 is resolved. Native `./tools/test-jcarray` passes restored
byte/boolean/short/int/reference logical lengths, zero initialization, clear
events, final valid elements, maximum backing capacity, and overflow rejection.
Extended `tools/test-cipher-lifecycle-vpcd` to set the retained AES transient
key and both RSA modulus/exponent components after a fresh emulator-process
restore. After a rebuilt PC max_crypto VM, the lifecycle suite passes 31/31;
the previously reported RSA `ArrayIndexOutOfBoundsException` is not reproduced
and its historical cause is unknown. No physical run was performed. RV-11/12
physical validation remains pending because PC/SC currently exposes only VPCD
readers.

Next highest implementation review item: RV-20261103-25, the production CCID
dispatch contract. Preserve unrelated worktree files and check the QKB before
changing shared routing. VPCD lifecycle log:
`artifacts/logs/cipher-lifecycle-vpcd-20261105T222334Z.log`.

## Latest continuation — VPCD reset must skip applet deselect — 2026-11-05 23:20 CET

The RV-20261103-12 transient test exposed a regression in VPCD: tracking the
selected AID had routed POWER_OFF, POWER_RESET and disconnect through
`clear_or_deselect()`, invoking applet `deselect()` before reset cleanup. The
session now clears selection bookkeeping without applet code, then invokes the
VM clear callback and card-manager reset callback. Applet SELECT transitions
still call `deselect()` as before. Standalone session tests pass 29/29; the
same-CAP VPCD lifecycle passes 6/6, including direct active reset with no
deselect count change and reset clearing a cached inactive writer applet's
transient arrays. Host VM max_crypto build passes. No firmware source changed
and no flash was done.

Physical status: elevated USB enumeration sees three `2e8a:0009` devices with
CDC and CCID descriptors, but PC/SC exposes only Virtual PCD 00/01. RV-12's
physical reset check remains pending. RV-11's corrected physical AES/RSA
lifecycle run is also pending for the same reader-enumeration issue.

Next: continue with RV-20261103-13, restored transient-array logical lengths,
using the same-CAP and native heap coverage. Revisit physical checks when a
Pico reader appears in PC/SC. Preserve all unrelated dirty files. VPCD log:
`artifacts/logs/same-cap-transient-vpcd-20261105T221737Z.log`.

## Latest continuation — Cipher lifecycle reset expectations — 2026-11-05 23:09 CET

Reconciled the RV-20261103-11 reset-status mismatch. Deselect preserves the
Cipher initialized flag and clears the key workspace, so retained AES and RSA
Ciphers return `42` (`UNINITIALIZED_KEY`). Reader reset clears native session
fields first, so they return `44` (`INVALID_INIT`); fresh emulator-process
restore also returns `44`. Updated both PCSC and VPCD test expectations. VPCD
passes 29/29, including deselect (`42`), reset (`44`) and fresh restore (`44`).
An earlier physical D92 run observed AES reset `44`, then aborted on the stale
`42` expectation and cleaned the probe package. The corrected full physical
rerun is still pending: the latest helper granted access to three USB nodes,
but GPPro enumerated only the two VPCD readers. No firmware source changed and
no flash was performed.

Next: when a physical Pico reader is again listed by PC/SC, rerun
`PICO2_HOST_USB_ACCESS=1 ./tools/test-cipher-lifecycle-pcsc '<exact reader>'`
with elevated host access. The test must complete AES and RSA deselect and
reader-reset paths, then report `TEST:SUMMARY pass=31 fail=0` and cleanup pass.
Preserve unrelated dirty files. Validation log:
`artifacts/logs/cipher-lifecycle-vpcd-20261105T220558Z.log`.

Implemented cross-package GC roots for foreign references held by applet-owned
volatile wildcard/Object arrays. Per-slot owner provenance routes each edge to
the target package collector; source local-GC skips foreign handles, preventing
equal compact handles from retaining the wrong object. Overwrite, package
deletion and compact-slot reuse are covered. A volatile array containing
foreign entries may remain in a live persistent applet field, but persistence
stores null so the unstable reference is not serialized. The VPCD regression
now verifies the field restores as null after emulator process restart. A
PC/SC card reset does not reload the runtime's persistent fields on physical
hardware; that warm-reset path must not be used as a cold-restore assertion.
Static/instance foreign-object stores and persistent arrays remain fail-closed.

Validation: `./tools/test-rmi-registry` passes with owner-slot reuse;
`./tools/build-pc --profile max_crypto` passes; Descriptor-free VPCD SIO passes
34/34 including cold-restore null (`artifacts/logs/sio-cross-aid-vpcd-20261105T214508Z.log`). Both
`./tools/build arm --profile max_crypto` and `./tools/build riscv --profile
max_crypto` pass. Final ARM image
`build/arm/pico2_choupi.uf2` (SHA-256
`59dc8f5335c7c6b24cee3a79012412181cd88b31f7904eb4de05f12febd6d4f9`) was
flashed to Pico 2 `D92E743D5F640C0F`; the finite cycle passed
`TEST:SUMMARY pass=1 fail=0`. Physical cross-AID SIO passed, including retained
array result `6B439000`, post-GC result `6A2E9000`, DELETE/reinstall and cleanup
(`artifacts/logs/sio-cross-aid-physical-20261105T214741Z.log`).

Next: continue with the highest remaining project TODO after RV-20261103-08;
read `.agent/QKB.md` for existing failure resolutions before touching related
code. Preserve unrelated dirty files and skip the pre-existing
`releaseGlobalAIDObject` removal when staging heap files. Implementation commit:
`ec00a265`. Cold-restore regression commit: `2f50699b`. Next highest open P1
is RV-20261103-11: Cipher key copies across transient-key clear events.

## Previous handoff — SHA-256 reset benchmark units — 2026-11-05 22:13 CET

User-supplied reset 6.20 ms vs doFinal 3.78 ms is consistent with a host
client units bug. Local JCAlgTest 1.8.2 bytecode subtracts doFinal's
ms/operation once from a 50-operation batch; missing subtraction is
49 * 3.78 = 185.22 ms/batch. Corrected reset differential is 2.50 ms,
including VM/dispatch/loop overhead. The applet reset loop calls doFinal then
reset; native doFinal already clears input, so that reset wipes zero bytes.
See `docs/sha256-reset-timing-analysis-20261105.md` for arithmetic, bytecode
provenance and limitations. No source/CAP changes, builds or hardware actions.
Actual user's client/CAP/firmware identity remains unverified. Host fix would
scale the subtraction by the matching batch count; no client fix was requested
or implemented. Prior firmware continuation remains below.

---

# Active handoff — signed branch offsets and physical SIO validation — 2026-11-05 20:49 CET

Fixed the physical cross-AID `JCSystem.getAppletShareableInterfaceObject`
failure. Short and wide branch handlers subtracted `sizeof(branch)` (an
unsigned `size_t`) from signed CAP displacements; a backward `goto` such as
`-64` became positive `4294967229` on 32-bit ARM. The bounds check correctly
rejected that wrapped target, so the applet returned null (`6A04`). A shared
signed offset helper now adjusts the displacement before branch dispatch.

The clean ARM max_crypto image passed `./tools/cycle choupi` on Pico 2
`D92E743D5F640C0F`. The complete descriptor-free physical fixture passed on
that board: SIO `45679000`, the firewall/exception checks, reset stale-proxy,
service DELETE and reinstall, and cleanup. The foreign reference into an
applet-owned array returned `6B479000`, matching the current CTX-08 fail-closed
policy and VPCD test; this probe's stale `6B43` expectation was corrected.
Descriptor-free Method bounds are reconstructed from CAP structural entry
tables, with invalid/unknown offsets still rejected. Tests: branch PC bounds
19/19, method body bounds 11/11, all 11 deploy 3.0.5 CAP scans (978 concrete
methods, no out-of-range or misaligned branch targets), ARM/RISC-V max_crypto
builds, the full physical SIO fixture, and Descriptor-free VPCD SIO 34/34.
Post-fix VPCD log: `artifacts/logs/sio-cross-aid-vpcd-20261105T195820Z.log`.
Other logs:
`artifacts/logs/cycle-choupi-20261105T194437Z.log` and
`artifacts/logs/sio-cross-aid-physical-20261105T194640Z.log`.

Continue from `TODO.md` priority 2 (remaining CTX-01/03 context isolation,
proxy/firewall lifecycle semantics); do not revisit the resolved signed
offset or Descriptor-free boundary work without new evidence. The D92 card was
left cleaned of fixture packages. Other physical reader visibility varied
after reset; refresh PC/SC names before any further physical work.

---

# Prior handoff — HOST-02 acceptance and available boards — 2026-11-05 15:42 CET

HOST-02 implementation and max_crypto Windows test image are complete locally.
Normal ARM startup removes the CDC/DTR wait and 500 ms sleep; bounded
`PICO2_DEBUG_SERIAL_WAIT_MS` supports 0..120000 ms and services USB every 1 ms.
Build wrapper always passes the value, writes provenance, and preserves
separately named diagnostic UF2 plus sidecar. ARM/RISC-V max_crypto normal,
diagnostic 30000 ms, then normal again passed; both CMake caches end at zero.
Physical normal firmware passed `tools/cycle choupi` on Pico 2
`D92E743D5F640C0F` (`TEST:SUMMARY pass=1 fail=0`); targeted GPPro SELECT,
CPLC and Card Data returned `9000`. The 30000 ms diagnostic build was also
flashed and its finite serial cycle passed, validating DTR-triggered capture;
no-DTR deadline behavior remains untested. The user reports that the normal
image became visible under Windows certutil almost instantly after replug.
Exact timing, image hash used, and reader-list method were not supplied, so the
three-run Windows acceptance remains open. Implementation commit: `1d3b5f95`.

Latest elevated USB refresh at 2026-11-05 15:38 CET found runtime JCVM devices
`D92E743D5F640C0F` and `2BED26ED20556535`, not BOOTSEL. `lsusb -v` confirmed
both provide the CCID interface. `pcsc_scan -r` listed only D92; 2BED is
USB-visible but not registered by PC/SC, and the third expected board was not
visible in this guest. Do not attribute that difference to firmware until the
USB/PCSC handoff is isolated. Logs from the recent cycles are under
`artifacts/logs/cycle-choupi-20261105T143119Z.log` and
`artifacts/logs/cycle-choupi-20261105T143418Z.log`.

After both current ARM and RISC-V `max_crypto` wrappers passed, the physical
cross-AID fixture ran on D92. Contract and service CAP LOAD returned success;
service applet INSTALL failed with `6A80`, matching the known Descriptor-free
dynamic method-body bounds limitation. The cleanup trap reported a failure
while trying to delete an applet AID that was not installed; an independent
GPPro `--list` afterward showed only the ISD, so no fixture package remains.
The reproducible blocker and logs are in `.agent/QKB.md` under the
Descriptor-free GPPro finding. Continue by deriving safe Method body bounds
without Descriptor; do not expand execution to the whole Method component.

Current Windows empty-card image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`, 4,025,856
bytes, SHA-256 `09d0a1ee76b3512314795eea796148141fc2e22fb7d7a28d21c97045aee61d15`;
contains 4,096 verified erased storage blocks and no preinstalled applet. See
`artifacts/firmware/README-crypto-arm-empty-card-current.md`.

Flash recovery note: two initial runtime-mode cycles failed because picotool
could not find the reset interface and CDC 1200-baud reset did not re-enumerate
in time. A third attempt hit a sysfs disappearance race during re-enumeration.
After the devices appeared in BOOTSEL, an identified D92 board flashed and
passed. Keep this distinct from the HOST-02 boot delay. Unrelated pre-existing
worktree changes were left unstaged.

---

# README refreshed — 2026-11-05

Applied the user-approved concise README with five parts: educational warning,
functional token/PCSC/Java Card/GlobalPlatform overview and CHOUPI attribution,
usage, compilation, and architecture links. Local links, shell snippet syntax,
section numbering, and diff whitespace checks passed. No firmware or hardware
work was needed; existing source/submodule changes remain unrelated. Resolve
the documentation commit with `git log -1 --format=%H -- README.md`.

---

# Cleaned history published — 2026-11-05

GitHub accepted cleanup commit `f6ec7b18a35fb43f9041b322bffb9bfb3e54b0cf`;
`git ls-remote` verified the same main tip. Local main now tracks origin/main.
This publication record follows that verified implementation commit. Continue
normal commits/pushes from this same folder. Existing unrelated worktree and
submodule changes remain uncommitted. Recovery backup is local and ignored.

---

# Repository history cleaned — 2026-11-05

The authorized artifact cleanup retained all 2,038 commits and preserved local
files and unrelated work. Generated logs, result output, metrics, UF2 images,
and backups are now excluded from main history and future tracking. Continue
working in this same repository; do not merge pre-cleanup history back into it.

See `.agent/history/2026-11-05-history-cleanup.md` for validation and recovery
instructions, and `.agent/history/2026-11-05-commit-map.tsv` for old/new IDs.
Current document hashes have been translated. Packaging reference data now
lives under `reference/jcalgtest/`. The recovery bundle and temporary rewrite
repository are ignored under `artifacts/backups/history-cleanup-20261105/`.
No unrelated source changes or dirty submodules were included in cleanup.
Firmware builds/hardware tests were skipped because firmware source is unchanged.
The previously open VPCD work remains described below.

---

# Current release-preparation outputs — ARM max and symmetric-only empty-card UF2s — 2026-11-05 11:22 CET

Built and packaged two current-source ARM profiles for external/GitHub release
use. Both images are empty-card images (no preinstalled CAP/applet) and include
4,096 `0xFF` storage blocks spanning `0x10100000`–`0x101FFFFF`:

- `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2` —
  `max_crypto`, 147/150 declared capabilities, 4,026,368 bytes, SHA-256
  `0868a0ec81b13bd5a5a7c9e42f2ac64fe061ade69d12ac93bded6aa852ec5873`.
- `artifacts/firmware/pico2-jcvm-symm_crypto-arm-empty-card-current.uf2` —
  `symm_crypto`, 46/150 capabilities, 3,837,440 bytes, SHA-256
  `40032e23306d73d4c095659b41ecb9b90441c447accaf870a88048b2bc501571`.

Both `./tools/build arm --profile ...` invocations passed. `picotool info -a`
identified each as RP2350 ARM Secure. A UF2 validator checked block sequence,
unique addresses, full erased storage, exact sizes and hashes. Firmware ends
at `0x100EB604` for max and `0x100D45A8` for symmetric-only. These artifacts
were not flashed to hardware and have not been uploaded to GitHub. Flashing
clears the card's persistent storage. See
`artifacts/firmware/README-crypto-arm-empty-card-current.md` for release notes
and rebuild commands. UF2s remain ignored by `.gitignore`; the README is
tracked. Release-preparation commit: `8960602b`.

The attempted PC-only VPCD storage trace from the previous continuation was
reverted before these builds. It did not enter either firmware image. The
VPCD harness process-group cleanup fix remains committed as `d2282b90` in the
cleaned history.

---

# Current continuation — prevent timed-out VPCD tests from orphaning emulator processes — 2026-11-05 10:35 CET

Removed `start_new_session=True` from the two emulator launches in
`tools/tests/sio_cross_aid_vpcd_test.py`. The harness's own `finally` block
still terminates an emulator after an internal timeout; external `timeout`
signals now also reach the emulator and its traced child through the inherited
process group. This addresses the accumulating CPU use from timed-out VPCD
tests. An orphan from an earlier diagnostic was terminated explicitly.

Validation: `python3 -m py_compile tools/tests/sio_cross_aid_vpcd_test.py`
passed. A fixture-reuse test killed after 8 seconds returned status 124 and
left no process in `ps -C choupi`. A separate 12-second connect-timeout run
failed before `VPCD:CONNECTED` as before, but its cleanup left no emulator
process. Log:
`artifacts/logs/sio-cross-aid-vpcd-20261105T093238Z.log`, SHA-256
`c6a3dd61bcc6d4ca65444cf190e65045be7e9b7af607660455da7998b55eea10`.
The underlying VPCD startup/runtime stall is still open; the VM progress trace
did not reach its configured interval. No firmware build or physical USB
operation was needed. The user reports all three devices are now in BOOTSEL;
that state has not been refreshed from inside the VM during this continuation.

Next: retain this cleanup change, then isolate the pre-connect stall. Check
whether `starting_jcre()` is blocked in host storage initialization or in the
interpreter before changing VM behavior. The CPUs were released after each
bounded test. Implementation commit: `d2282b9`.

---

# Current external-test artifact — max_crypto empty-card UF2 — 2026-11-05 10:02 CET

Refreshed `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`
from the current ARM `max_crypto` build. It is 4,026,368 bytes, SHA-256
`0868a0ec81b13bd5a5a7c9e42f2ac64fe061ade69d12ac93bded6aa852ec5873`, contains
3,767 firmware blocks and all 4,096 `0xFF` storage blocks
(`0x10100000`–`0x101FFFFF`), and has no preinstalled CAP/applet. The same bytes
are at `build/arm/pico2_choupi_max_crypto_empty_card.uf2`. `picotool info -a`
reports RP2350 ARM Secure; the block-map validator confirmed unique addresses
and fully erased storage. Regenerate after `./tools/build arm --profile
max_crypto` with `./tools/package-empty-card-uf2`. This combined image was not
flashed in this session; it intentionally erases card data when used.
Implementation commit: `2856556`; detailed record is in `progress.md`.

---

# Current continuation — Descriptor-aware physical JCSystem validation — 2026-11-05 09:52 CET

Implemented a test-only GPPro loader at `tools/load-cap-with-descriptor` and
`tools/LoadCapWithDescriptor.java`. It uses the pinned GPPro `GPSession` and
secure-channel handling while loading `CAPFile.getCode(true)`, which retains
optional Descriptor data. Updated `tools/test-jcsystem-transaction-physical`
to call this helper; upstream CAP contents and firmware Java Card boundary are
unchanged. `javac` compilation passed.

Both `./tools/build arm --profile max_crypto` and
`./tools/build riscv --profile max_crypto` passed. The ARM image was flashed to
Pico 2 `D92E743D5F640C0F`; the cycle passed flash verification and its finite
serial monitor (`TEST:SUMMARY pass=1 fail=0`, no `FAULT`). Image SHA-256:
`e520925668f07bd722f20643e462e558fb8eee7021d6c1d6498c611cddeb6360`.
Cycle log: `artifacts/logs/cycle-choupi-20261105T083813Z.log`, SHA-256
`4cae53d3d986215facdd36e5150bf41ac084053733518c475ec771a8b950663e`.

With the Descriptor-aware loader, a clean package install and complete physical
JCSystem probe passed `TEST:SUMMARY pass=13 fail=0`: transaction abort/commit,
persistent state, 32,767-byte array length, `BUFFER_FULL`, CCID power-cycle
persistence, commit capacity, and new global-reference validation. APDU output
was returned directly by the command tool and was not saved as a durable log.
Initial case 83/84 `6F00` results disappeared after deleting the applet and
package and reinstalling cleanly; this recovers the test but does not establish
the precise cause. The physical script was updated afterward, but its next
automated rerun failed at GPPro reader discovery because the Pico reader had
disappeared from PC/SC.

Latest host observations: `lsusb -d 2e8a:` still listed three runtime CHOUPI
devices; `pcsc_scan -r` showed only the two virtual readers. The authorized
reader-access helper granted all three USB nodes, but GPPro could not select a
physical reader. Do not assume BOOTSEL from the user's earlier report; the
enumerated devices were runtime `2e8a:0009`. Resume with `pcsc_scan -r`; if one
Pico returns, continue on it, otherwise keep working offline and do not wait
on a single board.

Current test package was deleted by applet AID and package AID, and GPPro
confirmed the card empty before the temporary reader-discovery loss. A new
empty-card listing after reconnect is still needed. The original GPPro-based
physical test script is now Descriptor-aware, but the new integrated rerun is
unverified. Implementation commit: `fee45c0`; the detailed progress record is
being committed separately. Preserve unrelated dirty files, including heap
and symm-crypto changes.

---

# Current continuation — Descriptor-backed field-kind checks — 2026-11-05 09:02 CET

Implemented Descriptor-backed first-store type checks for dynamic CAPs when the
optional Descriptor component is present. `Static_Handler` maps internal
static offsets to declared field types; all 24 normal, wide and `_this`
instance field bytecodes map their field token and declaring class to the
Descriptor type. Byte access accepts both byte and boolean. Malformed or
ambiguous Descriptor records fail closed. Descriptor-free packages keep the
existing runtime tag/layout checks; declared-kind guarantees cannot be claimed
for them because standard GPPro LOAD omits Descriptor.

Validation: `./tools/build arm --profile max_crypto`,
`./tools/build riscv --profile max_crypto`, and
`./tools/build-pc --profile max_crypto` passed. ARM image ends at
`0x100eb604`, leaving 84,476 bytes in the firmware window. Python fixture test
syntax and diff whitespace checks passed. Descriptor-present cross-AID VPCD
passed `TEST:SUMMARY pass=34 fail=0`; a static reference slot falsely declared
int failed closed (`1/1`), and an instance reference field falsely declared int
failed closed (`1/1`). Logs:
- `artifacts/logs/sio-cross-aid-vpcd-20261105T074812Z.log`, SHA-256
  `a6c85f12b1f3ebe9b6ff9629ddfed8884321a78c481eb9e1cfb03049d1c0b45e`
- `artifacts/logs/sio-cross-aid-vpcd-20261105T075613Z.log`, SHA-256
  `e4fb5aec92eb4fee606f912ee05b302a5261d0348d920e41f218379b97379bc8`
- `artifacts/logs/sio-cross-aid-vpcd-20261105T075840Z.log`, SHA-256
  `ff64fb3e1cd24989e3d55be821ce86169ef948bfb1768e6e914fbd3514d7e956`

The user reported three boards in BOOTSEL, but elevated USB enumeration found
three runtime `2e8a:0009` CHOUPI readers. Their USB serials were
`8E464F4E2BBC0A19`, `2BED26ED20556535`, and `D92E743D5F640C0F`. The last
identified board was flashed with the current `max_crypto` ARM image using
`PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile max_crypto --chipid
D92E743D5F640C0F`. The first picotool force-load could not find a reset
interface; the wrapper recovered through CDC 1200-baud BOOTSEL, detected ROM in
3 seconds, wrote and verified the image, and rebooted. Its 18-second monitor
passed repeatedly (`TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, no
`FAULT`). The board returned as a PC/SC reader; GPPro `--list` reported only
`ISD: A000000151000000 (OP_READY)`, with no loaded packages/applets. The other
two boards remain unchanged. This validates firmware boot/reader visibility,
not Descriptor-backed field bytecodes through a physical CAP: standard GPPro
LOAD omits Descriptor, while the SIO fixture execution pass used VPCD.
Physical cycle log: `artifacts/logs/cycle-choupi-20261105T081020Z.log` (SHA-256
`aede2b7c1a48993a6d73ef8a6dc8ce1d8f7a7330807c6c105618fedc8466f782`); flashed
image SHA-256 `e520925668f07bd722f20643e462e558fb8eee7021d6c1d6498c611cddeb6360`.

Commits: `c245264` (field checks), `30081ae` (field progress SHA), and `b2d2f97` (physical flash validation).
Next: inspect Descriptor parser/type metadata edge cases and add hostile-CAP
coverage. Keep the Descriptor-omitted dynamic method-boundary failure fail-
closed and open. Preserve unrelated worktree modifications (including
`heap.cpp`, `heap.hpp`, `cap_load.cpp`, fixture/review changes and supplied
SDK trees); stage only the declared-field implementation and its focused
fixture/test/docs records.

# Current session — fix uploaded CAP Descriptor lookup and multipackage INSTALL — 2026-11-05 01:21 CET

Fixed a runtime bug in `choupi/src/jc_handlers/jc_method.cpp`: per-method
Descriptor bounds were queried using the bytecode-start offset, but the
Descriptor records the method-header offset. The corrected calculation checks
the method pointer is in the Method component and uses its header offset plus
the handler-table and method-count prefix. Also corrected the VPCD
multipackage fixture to include optional `Export.cap` before `Descriptor.cap`.
The upstream CAP files were not modified.

Validation: `./tools/build arm --profile max_crypto`,
`./tools/build riscv --profile max_crypto`, and
`./tools/build-pc --profile max_crypto` passed. The focused command
`PICO2_HOST_USB_ACCESS=1 PYTHONUNBUFFERED=1 timeout --signal=TERM
--kill-after=5s 360s ./tools/test-vpcd-multipackage --timeout 180
--install-smoke` passed: Core and Multi packages LOADed and three applet
instances INSTALLed (`TEST:SUMMARY pass=1 fail=0`). The complete
multipackage/persistence/JCSystem transient suite now passes
`TEST:SUMMARY pass=47 fail=0`. A previous full run exceeded its time limit
because cleanup deleted 16 pagination instances individually. The test now
verifies all 21 instances and uses package cascade for those cleanup instances;
individual DELETE remains tested separately. Added PC diagnostic value
`PICO2_PERSISTENT_INVENTORY=once` to record one inventory/heap scan per owner
ID and avoid per-APDU diagnostic rescans; value `1` still means scan each APDU.
`tools/doctor` passed. The latest elevated `./tools/env lsusb` still showed
only USB root hubs and the VirtualBox tablet; no Pico devices enumerated
despite the user reporting all three in BOOTSEL. No flash or hardware test
occurred.

The current combined, applet-free max_crypto image has now been regenerated:
`build/arm/pico2_choupi_max_crypto_empty_card.uf2`, 4,022,784 bytes, SHA-256
`370a360f39bdbefdaf955406a8865b88b6542f34cd170b1a04b1153bf7b5cd75`. It
contains the just-built firmware plus 1 MiB of `0xFF` storage blocks. The
firmware-only `build/arm/pico2_choupi.uf2` is 1,925,632 bytes, SHA-256
`78d70491e088bd0e3159282f131714091e279d4d405c2b853d2a8403e6b27e98`.
`picotool info` and a UF2 block-by-block verification passed. Metadata and
Windows clean-card instructions are in
`artifacts/firmware/README-max_crypto-current-20261103.txt`.

Root `progress.md` has the 81-minute code-fix record (commit `ca72f14`), the
9-minute image-packaging record (commit `8ea6191`), and the current full-suite
record (commit `ce92b0c`). SHA bookkeeping is commit `5546a2c`;
`.agent/QKB.md` documents the reusable cause and fix; `TODO.md` records the
resolved bug and current physical-device visibility.

Current implementation/test files in this follow-up are `choupi/src/main.cpp`
and `tools/tests/pc_vpcd_multipackage_test.py`; the Descriptor offset fix is
committed. Build outputs and logs are ignored. Preserve all unrelated
pre-existing dirty files, especially
`choupi/src/heap.cpp`, `choupi/src/heap.hpp`, and `src/cap_load.cpp`; stage
only this step plus TODO/progress/QKB/handoff files.

Next: when a Pico enumerates, reconfirm chip identity and flash the combined
empty-card ARM image only after both target builds pass, then inspect finite
serial output and explicit PC/SC/GPPro behavior. If unavailable, continue the
remaining TODO priorities offline.

---

# Current session — CAP fixture completeness and VPCD diagnosis — 2026-11-04 23:50 CET

The reusable `tools/build-algtest-v182-load-code` now extracts the unchanged
upstream AlgTest 1.8.2 CAP components in load order, including Descriptor, and
writes the ignored input at `build/algtest-v182-load-code.bin` (30,836 bytes;
SHA-256 `b1967af560baa602a1330704d98589fe2cc85f32c25a2595efe7639e087314e7`).
Several probe CAP builders now include Descriptor in their LOAD inputs. Six
generated fixtures were byte-for-byte checked against the complete component
sequence. The Python VPCD applet harness now polls the emulator process and log
while waiting for its VPCD connection, so early install failures can surface
without waiting out the whole connect timeout.

Validation this session: `./tools/test-cap-method-body-deploy-caps` passed 11
packages / 978 methods / 8,720 branch targets, and the decoder sanitizer passed
13/13 checks over 100,000 malformed spans. ARM, RISC-V, and PC max_crypto
builds passed. A new VPCD integration attempt timed out after 120 seconds before
connection, without process exit or INSTALL failure marker; startup/runtime
hang remains unresolved. Do not claim the helper fixes JCAlgTest installation.

The user reported all three tokens in BOOTSEL, but this VM did not enumerate
them: authorized `PICO2_HOST_USB_ACCESS=1 ./tools/env lsusb -d 2e8a:` returned
no devices. `tools/flash --chipid D92E743D5F640C0F ...` waited for enumeration
and was stopped before writing. The image remains the previously verified
applet-free max_crypto UF2 (4,022,784 bytes, SHA-256
`9de981869926e22a9062052f4f408859d3ee1d8f5529ae389bdb0975b439f584`); no
hardware runtime test was done. Continue offline until guest USB enumeration
returns, and resume JCAlgTest startup diagnosis using the corrected fixture.

This worktree still has unrelated pre-existing modifications; stage only the
fixture builders, LOAD helper, VPCD harness changes, and their progress/QKB/
handoff record. Relevant dirty runtime files include `choupi/src/heap.cpp`,
`choupi/src/heap.hpp`, and `src/cap_load.cpp`; do not discard them.

# Current session — flash applet-free max_crypto to three identified boards — 2026-11-04 22:20 CET

The temporary PC-only branch-alignment A/B edit was restored; `git diff --
choupi/src/jc_bytecodes/bc_branching.cpp` is empty. `./tools/build-pc --profile
max_crypto`, `./tools/build arm --profile max_crypto`, and
`./tools/build riscv --profile max_crypto` all passed on the restored source.
The VPCD test with branch-start checks disabled still timed out before
`VPCD:APPLET_INSTALLED` or `VPCD:CONNECTED`, so this check alone is not the
JCAlgTest stall cause.

I enumerated three BOOTSEL chips and individually flashed the current combined
empty-card image to all three by chip ID:

- `D92E743D5F640C0F`
- `8E464F4E2BBC0A19`
- `2BED26ED20556535`

Each `tools/flash --chipid ... build/arm/pico2_choupi_max_crypto_empty_card.uf2`
reported full flash verification `OK` and `FLASH:PASS`. The monitor then
reported `ERROR: no USB serial device found`; subsequent `lsusb` showed no
Pico and `pcsc_scan -r` showed only the two virtual readers. The VM has
`VBoxClient` but no `VBoxManage`, so host-side VirtualBox reattachment is
currently unavailable from this guest. Treat these as flash-verified but
runtime-unverified. QKB has the detailed finding. Once attached back to the
VM, run chip/reader inventory, finite monitor, and PC/SC empty-card checks.

The image SHA-256 is
`9de981869926e22a9062052f4f408859d3ee1d8f5529ae389bdb0975b439f584`; exact
path and size remain in `artifacts/firmware/README-max_crypto-current-20261103.txt`.

---

# Current session — isolate JCAlgTest VPCD startup timeout — 2026-11-04 21:55 CET

The previous JCAlgTest VPCD attempt timed out before `VPCD:CONNECTED` or
`VPCD:APPLET_INSTALLED`. I regenerated the ignored diagnostic input
`build/algtest-v182-load-code.bin` with the unchanged upstream CAP's Descriptor
component included (30,836 bytes; SHA-256
`f9b0b23a0caa6adce7867d16feaa6c379d680254cf88eed08768b905d0366e5b`). The
retry still timed out before install/socket connect. This rejects neither the
Descriptor parser nor RV-28; it only means Descriptor omission was not enough
to explain the stall.

The restricted direct PC emulator command fails at `PTRACE_ATTACH` with
`Operation not permitted`; elevated repository VPCD commands get through MPU
and filesystem initialization and into remote contexts. Recent division-probe
VPCD logs connected, so the current failure is specific to the JCAlgTest
install/startup path. Logs:
`artifacts/logs/rv28-jcalgtest-vpcd-20261104.log`,
`artifacts/logs/rv28-jcalgtest-vpcd-retry-20261104.log`, and
`artifacts/logs/rv28-jcalgtest-vpcd-cpu-20261104.log` (all ignored artifacts).

A same-shell process snapshot during another bounded attempt showed the PC
emulator at 27.8% CPU and its guest at 39.1% CPU after roughly one second;
they were executing rather than waiting on the VPCD socket. This single sample
does not prove a loop. Dynamic branch checks decode from method start to each
candidate target, so profile that path against the actual AlgTest methods.
Latest short run log: `artifacts/logs/rv28-jcalgtest-vpcd-live2-20261104.log`.

Next: instrument or bisect the bounded JCAlgTest install path using the
required elevated route; keep all upstream CAP bytes unchanged. The current
max_crypto applet-free Windows image remains
`build/arm/pico2_choupi_max_crypto_empty_card.uf2`, 4,022,784 bytes, SHA-256
`9de981869926e22a9062052f4f408859d3ee1d8f5529ae389bdb0975b439f584`; it has
not been flashed.

---

# Architecture documentation refreshed — 2026-11-04 21:43 CET

- Updated `docs/FIRMWARE_JCVM_ARCHITECTURE.md` (3,660 words) from current
  source and development records through the RV-28/VPCD diagnostic follow-up.
  The document now distinguishes implemented changes, historical validation,
  and remaining acceptance/refactoring work; L1–L10 retain their original IDs.
- Replaced obsolete claims about board-owned filesystem implementation,
  interpreted Cipher/Signature/digest providers, absent lifecycle recovery,
  zero transaction capacity, and single-package host storage. Covered retained
  contexts/SIO ownership, volatile native crypto state, CCID routing/retries,
  immutable deadlines, and partial uploaded-CAP execution checks.
- Preserved the requested `docs/` location and corrected source links relative
  to it. The existing root-to-docs relocation is included with this update;
  unrelated source/probe changes and other sessions' records are preserved.
- Documentation validation passed: 20 local links resolve, all ten findings
  are represented, code fences balance, and document whitespace checks pass.
  No builds, runtime tests, USB/PCSC operations, or hardware validation were
  performed for this documentation-only step. Historical test results are
  labelled by scope; RV-25/26/27/28 acceptance gaps remain explicit.
- Remaining priorities: malformed-CAP runtime/linkage/access validation,
  unified production card-session policy, explicit storage durability/HAL
  contracts, residual interpreted API behavior, and build/profile isolation.

---

# Current session — RV-28 method body bounds — 2026-11-04 20:29 CET

Current max_crypto image for external Windows testing is freshly rebuilt from
the RV-28 source and remains applet-free:

- Empty-card UF2: `build/arm/pico2_choupi_max_crypto_empty_card.uf2`,
  4,022,784 bytes, SHA-256
  `9de981869926e22a9062052f4f408859d3ee1d8f5529ae389bdb0975b439f584`.
- Firmware-only UF2: `build/arm/pico2_choupi.uf2`, 1,925,632 bytes, SHA-256
  `d484379d46065ddaf32fb1360a48f528e1059e24568250ab30f81fc67dd73d0e`.
- Firmware end `0x100eaf84`; 86,140 bytes remain before storage at
  `0x10100000`. `picotool info -a` reports RP2350 ARM Secure. An address-map
  check confirmed all 3,761 firmware blocks are preserved and the full
  4,096-block `0x10100000`–`0x101fffff` storage region is `0xFF`. It has not
  been flashed. Image details are in
  `artifacts/firmware/README-max_crypto-current-20261103.txt`.

RV-20261103-28 has a partial native-opcode-origin fix in the worktree. Added
`src/cap_execution_policy.hpp` with the dynamic package range 56–63; the range
is now also used by `FlashMemory_Handler`. The interpreter throws
`SecurityException` instead of dispatching `impdep1` or accepting `impdep2`
when the current method belongs to an uploaded CAP. Added
`docs/CAP_EXECUTION_TRUST_MODEL.md` to state that GP authentication does not
make uploaded bytecode trusted. ROM/framework CAPs remain permitted.

Added `src/cap_method_bounds.hpp` and guarded `Method_Handler::getMethodFromOffset`
against offsets outside the Method component and truncated normal (2-byte) or
extended (5-byte) headers before later code reads packed fields. The helper
test covers exact-fit and truncated headers at several offsets.

Bounded bytecode reads and relative PC updates to each method's exact body,
using its Descriptor `method_offset` and `bytecode_count`. Uploaded CAP calls
fail closed when no valid descriptor range is found. The trusted ROM path
retains Method-component fallback where descriptor metadata is absent or
unusable. Exception diagnostics now check bounds before reading adjacent
`invokevirtual` bytes. The next descriptor method offset caps each body. In
the deployed 3.0.5 suite, 975/978 concrete method counts match the next start;
three counts exceed it by one byte and are clamped. Focused tests pass: method
body 7/7, PC bounds 14/14, method header 7/7, native opcode policy 5/5. An
offline scan confirmed ranges for 978 concrete methods across 11 deployed
3.0.5 API CAPs.

Validation: `./tools/test-cap-native-opcode-policy` passes 5/5,
`./tools/test-cap-method-bounds` passes 7/7,
`./tools/test-cap-bytecode-pc-bounds` passes 14/14 and
`./tools/test-cap-method-body-bounds` passes 7/7. `./tools/build arm` and
`./tools/build riscv` pass; the ARM wrapper also rebuilt the PC emulator. A
valid upstream JCAlgTest PC VPCD integration retry timed out before
`VPCD:APPLET_INSTALLED` or socket connect. The startup log is
`artifacts/logs/host-vpcd-jcalgtest-20261014.log`. Existing Rust and
packed-member warnings remain. No malformed CAP, sanitizer execution, firmware
flash, or physical test was performed. RV-28 stays open: branch targets are
bounded to the method body but are not verified as instruction boundaries.
Nested linkage, heap field-kind/access checks, and malformed input
runtime/sanitizer tests also remain required. Do not claim the interpreter
fully validates uploaded code.

The native opcode boundary step is committed as `b6b4302` (metadata follow-up
`654b3fe`). The method-header bounds step is committed as `9246f28`. Preserve
all pre-existing worktree changes, including the unrelated `src/cap_load.cpp`
changes; it was not edited for these steps. Continue offline unless authorized
physical runtime access becomes available.

The Method-component bytecode fetch bounds and focused test are committed as
`a556c2d`; progress/handoff SHA metadata follow-up is `20f54c7`. Per-method
body bounds, hard clamping at the next descriptor method offset, and guarded
exception diagnostics are committed as `12afae6`; progress/handoff SHA
metadata follow-up is `0ab09ff`. Added `tools/test-cap-method-body-deploy-caps`
to repeat the offline check against all 11 deployed 3.0.5 API CAPs; it passes
978 concrete methods with 3 descriptor clamps. This script and its records are
committed as `8499891`; metadata follow-up is `e8422f5`.

RV-28 dynamic branch instruction-boundary validation is implemented in the
current worktree. Taken short, wide, and switch branches in dynamic package IDs
56–63 use checked PC offsets and require the target to be a decoded instruction
start. Trusted ROM branches keep the existing range-only behavior. The width
decoder bounds variable switch payloads and rejects unknown/truncated
instructions; CHOUPI handlers confirm `checkcast` and `instanceof` are four
bytes (opcode, atype, 16-bit constant-pool index). Tests pass: boundary 12/12,
PC bounds 17/17, deployed scan 11 packages / 978 methods / 8,720 branch and
switch targets with zero out-of-range or misaligned targets (three one-byte
Descriptor overcounts clamp to the next method). ARM and RISC-V max_crypto
builds pass. No physical or malformed-CAP interpreter/runtime test was run.
The boundary helper now also builds through `tools/env cmake` with ASan/UBSan
and passes 100,000 deterministic malformed spans (0–32 bytes), 13/13 checks;
this does not exercise CAP installation or interpreter execution. The
implementation is committed as `b4cc795`. Continue RV-28 with malformed CAP
runtime/sanitizer coverage, nested linkage checks, and heap field-kind/access
validation.

Latest runtime validation attempt: `./tools/build-pc --profile max_crypto`
passes, but the one-pass VPCD run timed out before connection. A fresh-memory
attempt reported INSTALL stage 5 / SW `6A80` / VM `6F00` / exception 29
(`SecurityException`), and a later host-emulator attempt panicked at
`choupi/os/src/arch/host/emulator.rs:683` on an unknown ptrace stop (status
639). A temporary PC-only branch-target trace produced no alignment-failure
marker and was removed. The unchanged JCAlgTest CAP's offline scan reports
1,303 aligned in-range targets. Cause remains unresolved; don't rerun this
VPCD command unchanged. No hardware or malformed-CAP runtime test was run.

---

# Prior session — RV-28 uploaded-CAP native opcode boundary — 2026-11-04 19:15 CET

The first RV-28 slice rejects `impdep1` and `impdep2` in dynamic package IDs
56–63. It is committed as `b6b4302`, with the progress metadata follow-up
`654b3fe`.

---

# Prior session — RV-27 VPCD selection and restore correctness — 2026-11-04 19:02 CET

RV-20261103-27 has a first implementation pass in the working tree. The stale
session test now builds against `Session(CardCallbacks)`. `Session` tracks the
selected applet AID and only identifies a same-applet reselection when the
requested AID matches it. Restore results distinguish missing AIDs (`6A82`)
from restoration failures (`6F00`). The PC restore callback checks installed
AID presence separately from CAP/instance restoration failures. The session
test covers two known AIDs, switching in both directions, same-AID reselection,
unknown AID, ISD selection, rejected VM `select()`, missing AID and corrupted
restore.

Validation: `./tools/test-vpcd-session` passes `TEST:SUMMARY pass=25 fail=0`;
`./tools/build arm` and `./tools/build riscv` pass. The PC VPCD applet test was
attempted with elevated localhost access and failed by timeout before VPCD
connected; the appended log ends during emulator startup before
`VPCD:APPLET_INSTALLED`. This is not evidence against the changed restore path,
which is entered only after SELECT, but leaves production runtime acceptance
unverified. No physical token or USB/PCSC was used.

The implementation and first progress entry are committed as `233f522`.
Preserve unrelated worktree changes. Keep RV-27 open until a PC production
runtime test exercises the restore callback. Continue offline until physical
reader access returns.

---

# Prior session — RV-26 CCID response recovery first pass — 2026-11-04 20:43 CET

RV-20261103-26 first pass is implemented. `send_response` no longer blocks for
100 ms waiting for an extension and then discards the final response. It
retains the response in `tx_buffer`, retries nonblocking from the main USB task
after extension completion, and throttles retries to 1 ms. Thirty-two
synchronous final IN submission failures trigger a recoverable abort and
Bulk-OUT rearm so the host can retry. Bulk-OUT submission failures also retain
and retry the rearm request. ZLP submission uses the same retry service.

`./tools/test-ccid-progress-flow` passes `TEST:SUMMARY pass=15 fail=0`; ARM and
RISC-V max_crypto wrappers pass. Production callback recovery is unverified.
The authorized helper briefly saw a USB node and `lsusb` showed
`2e8a:0009 Pico2 JCVM CCID`, but repeated `pcsc_scan -r` only showed both
Virtual PCD readers. No physical runtime test was performed. RV-25 production
CCID route runtime validation is also still open.

Latest applet-free max_crypto ARM binary for external Windows testing:

- Empty-card image: `build/arm/pico2_choupi_max_crypto_empty_card.uf2`,
  4,016,640 bytes, SHA-256
  `7112f8f52a14ef60027904648338f85c7e7dee8e02739c9a52de8046fa6b5d2e`.
- Firmware-only image: `build/arm/pico2_choupi.uf2`, 1,919,488 bytes,
  SHA-256 `692c1fb42f8c7dd2aa92a37b6b23aa999f93c657eeb7144f63e66e2cbd72dbed`.
- Firmware endpoint `0x100EA3BC`, 89,156 bytes remain before storage at
  `0x10100000`; combined image contains 4,096 all-`0xFF` blocks through
  `0x101FFFFF`. Picotool reports RP2350 ARM Secure. The image has no bundled
  CAP/applet and was not flashed.
- Details: `artifacts/firmware/README-max_crypto-current-20261103.txt`.

RV-24 source commit `5b0b5e5f4611abd6c256a79c89899509f9a712bc`, metadata
follow-up `c224b8c`. RV-25 first-pass source commit
`832489c4eb748e0544c90a9afe43d557e3fb7942`, metadata follow-up `7fc34fe`.
RV-26 first-pass source/docs are committed as
`0d4b271fd338bb8aa76230de74fdf03c2ffe7692`; the progress SHA metadata
follow-up is pending. Preserve unrelated files, including AGENTS.md, heap/CAP
loader changes, review artifacts, crypto probes and upstream submodules. Next
offline priority after the metadata commit is RV-27 test/session correctness;
return to RV-25/26 runtime tests if a physical CCID reader becomes available.
No CAP or Java Card API source was changed.

---

# Current session — RV-25 production CCID routing first pass — 2026-11-04 20:23 CET

RV-20261103-25 is in progress. Added `src/ccid_apdu_route.hpp`, a shared
route decision used directly by `src/ccid_device.cpp`. It distinguishes
default/explicit ISD SELECT, applet SELECT, public/protected GP manager
commands, applet/no-applet APDUs, authentication failure, invalid CCID slot,
and powered-off slot. The CCID driver rejects invalid/inactive XfrBlock
requests, returns GP responses before applet dispatch, returns `6982` for
failed SCP authentication, handles explicit ISD AID selection with its FCI,
and echoes the request slot in CCID responses.

`./tools/test-ccid-apdu-route` passes `TEST:SUMMARY pass=12 fail=0`.
`./tools/build arm --profile max_crypto` and `./tools/build riscv --profile
max_crypto` pass. The RISC-V wrapper builds the VM core; the full production
CCID device build is ARM only. No physical CCID/PCSC runtime validation has
been performed. RV-25 must remain open until route behavior and CCID status
are exercised through production runtime, including failed authentication and
powered-off APDUs.

Latest applet-free max_crypto ARM binary for external Windows testing:

- Empty-card image: `build/arm/pico2_choupi_max_crypto_empty_card.uf2`,
  4,015,616 bytes, SHA-256
  `6a4e25626a465b125ecd89a40a442c78a8111984f9876c1685acce57c6c2e50e`.
- Firmware-only image: `build/arm/pico2_choupi.uf2`, 1,918,464 bytes,
  SHA-256 `689bbc0985df4e3096012c42cbabe3c7198199109f0c9f70acfbf80c909565e7`.
- Firmware endpoint `0x100EA1D4`, 89,644 bytes remain before storage at
  `0x10100000`; combined image has all 4,096 erase blocks through
  `0x101FFFFF`. Picotool reports RP2350 ARM Secure. The image has no bundled
  CAP/applet and was not flashed or runtime-tested.
- Binary details: `artifacts/firmware/README-max_crypto-current-20261103.txt`.

RV-24 is committed as `5b0b5e5f4611abd6c256a79c89899509f9a712bc`, with
metadata follow-up `c224b8c`. RV-25 first-pass source/docs/tests are committed
as `832489c4eb748e0544c90a9afe43d557e3fb7942`; the progress SHA metadata
follow-up is pending. Preserve all unrelated dirty files, especially AGENTS.md,
heap and CAP loader edits, review artifacts, crypto probes and upstream
submodules. The next work is physical/runtime CCID validation when a board is
available, plus fixes uncovered there. No user-provided CAP or Java Card API
source was modified.

---

# Current session — RV-24 non-atomic persistent writes — 2026-11-04 19:10 CET

RV-20261103-24 is implemented in native C++ and validated. `Util` copy/fill
uses a persistent byte setter that does not add an atomic transaction snapshot
or consume commit capacity. If an existing array record already has a before
image from an atomic mutation, non-atomic writes patch the corresponding
payload byte in that image; abort therefore restores atomic bytes while keeping
the non-atomic result. A transaction-created record remains deleted on abort.
Ordinary transaction accounting was not changed.

Validation passed: PC build (after implementation), ARM max_crypto build,
RISC-V max_crypto build, and focused VPCD transaction tests
(`TEST:SUMMARY pass=7 fail=0`, 231.098 seconds). The tests include commit
capacity overflow, non-atomic mutation of a record larger than capacity, mixed
atomic/non-atomic writes to the same and different arrays, and atomic-after-
non-atomic ordering. No physical board was accessed. VPCD tests do not cover
power-loss durability during an active filesystem transaction.

Latest applet-free max_crypto ARM artifacts for Windows testing:

- Empty-card image: `build/arm/pico2_choupi_max_crypto_empty_card.uf2`,
  4,015,104 bytes, SHA-256
  `9c712067e835f61231c1b70556acfee15fa77cf9b06cd44d31f6dc7f70d45000`.
- Firmware-only image: `build/arm/pico2_choupi.uf2`, 1,917,952 bytes,
  SHA-256 `2457caba615ceec3c8d2979c0a76825533e06ed7f856621776c09bcf04fef3e4`.
- Firmware ends at `0x100EA064`, leaving 90,012 bytes before `0x10100000`.
  Combined image contains 4,096 all-`0xFF` blocks over
  `0x10100000`–`0x101FFFFF`; picotool identifies RP2350 ARM Secure.
- VPCD log: `artifacts/logs/rv24-nonatomic-vpcd-20261104.log`, SHA-256
  `58cdfabe3db6561fa8236e50e126c6467437c26bf15b84a88fd09c244a2d0f66`.
- Image details: `artifacts/firmware/README-max_crypto-current-20261103.txt`.

RV-24 is marked complete in TODO. Implementation commit:
`5b0b5e5f4611abd6c256a79c89899509f9a712bc`. A metadata follow-up commit for
the progress SHA and this handoff remains. Preserve unrelated dirty files,
including AGENTS.md, heap and CAP loader changes, review artifacts, crypto
probes and upstream submodules. After the metadata commit, continue at RV-25:
unify CCID/card-session command routing. Do not use physical hardware unless
explicitly requested and present.

---

# Current session — prevalidate non-atomic Util arguments — 2026-11-04 17:43 CET

RV-20261103-23 is implemented in native C++ `invokestatic` dispatch. Calls to
`Util.arrayCopyNonAtomic` and `arrayFillNonAtomic` now clear SensitiveResult,
validate all null and range arguments before any destination write, preserve
overlap-copy behavior, and assign SensitiveResult only on success. Public Java
Card API sources and upstream CAPs were not changed.

Validation passed: PC, ARM, and RISC-V `max_crypto` builds; the full cross-AID
VPCD regression (`TEST:SUMMARY pass=34 fail=0`); and the focused CAP-to-native
Util test covering negative/overrun ranges, null arrays at zero length,
zero-length end boundaries, valid and invalid overlap, exact exceptions,
unchanged data after failures, and SensitiveResult state. The VPCD test needed
90-second connection and 120-second APDU deadlines because package LOAD is
slow in this VM. No physical device was used.

Current applet-free max_crypto ARM image for external Windows testing:

- `build/arm/pico2_choupi_max_crypto_empty_card.uf2`, 4,014,592 bytes, SHA-256
  `041144a8a5116111aef9f4e501f87355e90a4e1e9731cdf7969a75da9ac2e328`.
- Firmware-only `build/arm/pico2_choupi.uf2`, 1,917,440 bytes, SHA-256
  `803997081387ed53fa4fad5f4afafa1d93447666e5b6341d886838e0fa6f3d7e`.
- Firmware end `0x100E9F08`, leaving 90,360 bytes before `0x10100000`. The
  combined UF2 has 4,096 all-`0xFF` blocks over `0x10100000`–`0x101FFFFF`.
- Details: `artifacts/firmware/README-max_crypto-current-20261103.txt`.
- Full regression log: `artifacts/logs/sio-cross-aid-vpcd-20261104T163028Z.log`.
- Focused Util log: `artifacts/logs/sio-cross-aid-vpcd-20261104T163909Z.log`.

RV-23 was committed as `10e3af45531da9b1e493fceab7acf29404141599`. Preserve
unrelated worktree changes, including `AGENTS.md`, heap,
CAP loader, review probes, crypto fixtures and upstream submodules. Physical
validation was not requested. Next priority is RV-20261103-24: route
non-atomic persistent array writes outside the transaction journal while
preserving mixed atomic/non-atomic abort semantics.

---

# Current session — fix signed minimum division — 2026-11-04 14:05 CET

RV-20261103-19 is implemented. `bc_sdiv()` and `bc_idiv()` now push the
minimum dividend unchanged for `MIN_VALUE / -1`. Added a standalone Java Card
CAP test fixture that executes `sdiv`, `idiv`, `srem`, `irem`, signed quotient
and remainder combinations, and catches divide-by-zero exceptions. The
generated JCA was inspected to confirm those exact bytecodes are present.

`PICO2_HOST_USB_ACCESS=1 ./tools/test-division-vpcd` passed
`TEST:SUMMARY pass=5 fail=0`. VPCD setup took 114.859 seconds to connect and
about 27 seconds each to select the target and install; the custom arithmetic
APDU itself completed in 2 ms. The fixture log is
`artifacts/logs/division-vpcd-20261104T125922Z.log`, SHA-256
`1311ba904a36128b897cb62acb95434bf11d19975f241d8480e07b5b512768ba`.
ARM and RISC-V `max_crypto` builds passed. No physical token was tested.

The max empty-card image was refreshed: `build/arm/pico2_choupi_max_crypto_empty_card.uf2`,
4,010,496 bytes, SHA-256
`ee1978b2748dcc10f1bd5ec24bfe6d2938441aa8653e26435c878f27eca5cf21`;
firmware-only image: 1,913,344 bytes, SHA-256
`6bc3ec2b0d379a6a4f8d6d37c2fc27bcfcdf269a1e3a46aaf675adadfd1be99c`.
Firmware endpoint is `0x100E9764`, leaving 92,316 bytes. The combined image
was recognized by picotool as RP2350 ARM Secure and verified to erase the full
`0x10100000`–`0x101FFFFF` storage range.

Implementation commit: `b7cd206`; progress SHA follow-up remains. Next priority
is RV-20 defined arithmetic wrapping, shifts,
and word reconstruction. Preserve unrelated worktree changes.

---

# Current session — reject array backing-size overflow — 2026-11-04 13:53 CET

RV-20261103-18 is implemented in the current worktree. `JC_Array` checks
element-count × element-width in 32-bit arithmetic and accounts for persistent
array record headers before narrowing to the 16-bit backing length. Overflow
now raises `SystemException.NO_RESOURCE` at global/transient factories and
`newarray`/`anewarray`; persistent record serialization also computes its size
wide and rejects values above 65,535 bytes.

`./tools/test-jcarray` passed, including `int[16383]` accepted, `int[16384]`
rejected, and byte/short/int record limits. PC, ARM and RISC-V `max_crypto`
builds passed. Existing dependency/packed-member warnings remain. No physical
token validation was performed. The refreshed empty-card Windows image is
`build/arm/pico2_choupi_max_crypto_empty_card.uf2`, 4,010,496 bytes, SHA-256
`b30953d6e4ce191638150af647992089a14f6d99f5bb3c167bd31574f8cb49fd`; current
firmware-only image is 1,913,344 bytes, SHA-256
`32bdc2bb486a05ef8730b47cbafa4b0662b16bb686684b5fd2dc4e493f6f75fb`. It
ends at `0x100E9764`, leaving 92,316 bytes before storage. The combined image
was recognized by picotool as RP2350 ARM Secure and verified to erase the full
`0x10100000`–`0x101FFFFF` storage span.

RV-17 was closed using its accepted documentation branch: the compatibility
profile states the present 2,046-byte digest input cap and `ILLEGAL_USE`; a
transaction-safe incremental context remains planned. Documentation commits
are `e5ed1ac` and metadata follow-up `ae8a249`.

RV-18 implementation commit is `046afbc`; progress SHA follow-up remains. Next:
review RV-19 signed minimum division, preserve unrelated
worktree changes, and continue using the current empty-card image instructions
in `artifacts/firmware/README-max_crypto-current-20261103.txt`.

---

# Current session — document the MessageDigest input limit — 2026-11-04 13:32 CET

RV-20261103-17 took the contract-narrowing branch allowed by its acceptance
criteria. `docs/supported-profile.md` now explicitly limits cumulative
`MessageDigest.update()` plus `doFinal()` input to 2,046 bytes and records
`CryptoException.ILLEGAL_USE` above that limit. No runtime code changed and no
build or test was run; the source was inspected to confirm the fixed transient
buffer and error path.

Native incremental contexts remain open work. A context must roll back if an
applet transaction aborts after `update()`, while reset clears intermediate
state. Existing RV-15 VPCD tests cover transaction abort and reset behavior for
the current buffer model. Before removing the limit, add transaction-aware
snapshot/rollback for native contexts, then test >2,046 bytes across APDUs and
hash block boundaries, empty final input, reuse after completion, reset, and
transaction abort. No hardware test was run. The current max empty-card image
still matches the prior RV-16 build because firmware source is unchanged.

Documentation commit: `e5ed1ac`. Continue with RV-20261103-18 checked `int[]`
length allocation, preserving unrelated worktree changes.

---

# Current session — RV-16 CBC post-final IV state fixed — 2026-11-04 13:20 CET

RV-20261103-16 is implemented in the current worktree. After successful AES or
DES CBC `doFinal()`, native Cipher now zeros the active chaining IV and keeps
the original initialization IV for an explicit later `init()`. AES-CTR retains
its previous counter reset path.

Validation passed: `PICO2_HOST_USB_ACCESS=1
./tools/test-symm-crypto-vpcd` reports `TEST:SUMMARY pass=99 fail=0`, including
AES/DES vectors for first use with a nonzero IV, repeated use against a
zero-IV reference, and a new `init()` against the supplied IV. ARM and
RISC-V `max_crypto` builds passed; the PC VPCD build and test CAP conversion
passed. No physical token was flashed or tested.

For a clean Windows test card, the current ARM `max_crypto` combined UF2 is
`build/arm/pico2_choupi_max_crypto_empty_card.uf2`, 4,009,472 bytes, SHA-256
`7f7538c0b83faf826682848f39d4148cb4530f2fc11c95731f52bba33e03357a`. It
contains current firmware plus `0xFF` storage coverage at
`0x10100000`–`0x101FFFFF` and erases CAPs/filesystem state when flashed. The
firmware-only UF2 is 1,912,320 bytes, SHA-256
`34e95e0e1504add5130f5a0d7d11c102f939708ce3fa82699e33e7d6d7a8680e`; it does
not erase card storage. The firmware ends at `0x100E9590`, leaving 92,784
bytes before the storage region.

The latest passing suite log is
`artifacts/logs/symm-crypto-kat-vpcd-20261104T120640Z.log` (SHA-256
`1e67eb5eec3a133139518b2b10f4f83feacc038b431e60037ad12112162a6a4f`). The
refreshed image details are in
`artifacts/firmware/README-max_crypto-current-20261103.txt`.

Continue with RV-20261103-17, incremental digest behavior beyond the current
2,046-byte cumulative input cap. Preserve unrelated dirty worktree changes.
In particular, `choupi/src/heap.cpp`, symmetric probe sources and associated
test scripts contain other in-flight changes. Stage only the RV-16 native CBC
change and the new CBC test additions from shared files. RV-16 source commit is
`2edc1b2aeafcb47393351c362ebd21632798465d`. `progress.md` has two identical
RV-16 entries from an accidental append; both are preserved per the no-delete
history rule and carry the source commit SHA.

---

# Current session — RV-15 crypto session-state lifecycle closed — 2026-11-04 12:51 CET

RV-20261103-15 is implemented and validated in the current worktree. Native
MessageDigest, Cipher and Signature operational scalar state now lives in a
lazy, nonpersistent per-instance sidecar. `CLEAR_ON_RESET` cleanup discards
that sidecar; persistent CAP fields still retain algorithm identity, keys and
object configuration. RV-15 is checked off in `TODO.md`; physical-token
validation was not run.

Validation: `./tools/test-jcarray` passed. The full
`PICO2_HOST_USB_ACCESS=1 ./tools/test-asymm-crypto-vpcd` run passed 107/107
phases (102 asymmetric vectors), including digest state across aborted
transactions, warm reset, cold process restore, and ISO9796-2 Signature
recovery across an aborted transaction. `./tools/build arm --profile
max_crypto`, `./tools/build riscv --profile max_crypto`, and
`./tools/build-pc --profile max_crypto` passed. The PC build profile is restored
to `max_crypto`.

For Windows testing with an empty card, use the freshly regenerated
`build/arm/pico2_choupi_max_crypto_empty_card.uf2`: 4,009,472 bytes, SHA-256
`247b0d30bd2c0dab5fdfd5f4360a086932e5868ac82deb1993bfa26b624a5c5d`. It
contains current ARM max_crypto firmware plus an erased 1 MiB storage region;
flashing it removes installed CAPs and filesystem data. Firmware-only
`build/arm/pico2_choupi.uf2` is 1,912,320 bytes, SHA-256
`5087cb3352b61752f9c4e68a25aeb6f8ac3f59d700987a7bd3d1e04e6aec3bfc`; it
does not erase existing card storage. The firmware ends at `0x100E9564`, with
92,828 bytes left before `0x10100000`.

The latest VPCD log is
`artifacts/logs/asymm-crypto-keypair-vpcd-r15-20261104T1245CET.log` (SHA-256
`d9baa259fc0afeaab65eb8e0d778b442a8d3f27bd8df9c6ee607a1f93be12629`). The
profile README was refreshed at
`artifacts/firmware/README-max_crypto-current-20261103.txt`.

Continue with RV-20261103-16, checking CBC post-`doFinal()` IV behavior.
Preserve all unrelated dirty files from other sessions. In particular,
`choupi/src/heap.cpp` contains an unrelated deletion of
`releaseGlobalAIDObject`; stage only the reset-sidecar addition. The shared
`tools/tests/symm_crypto_applet/SymmCryptoProbe.java` contains other in-flight
test changes; stage only the RV-15 digest/recovery additions. The implementation
commit is `4b4345e9a756e74d1f2c8ca0f774ed34936b8e70`; the progress record's
commit SHA is recorded by its metadata follow-up commit.

---

# Current session — max_crypto empty-card UF2 prepared — 2026-11-04 11:45 CET

For Windows validation use
`build/arm/pico2_choupi_max_crypto_empty_card.uf2` (4,008,448 bytes, SHA-256
`dc3fdc634a0d06eb699e990ccbbdea19912cbc1795d65a04c91bdb64acebed04`). It is
the current ARM `max_crypto` firmware plus `0xFF` blocks covering all RP2350
application storage from `0x10100000` through `0x101FFFFF`. Copying it to the
Pico 2 BOOTSEL drive loads JCVM and clears previous CAP packages and persistent
filesystem data. No applet is bundled; load the unchanged CAP later with
GPPro. This is destructive to stored card data. No physical board was flashed.

The standard current firmware is `build/arm/pico2_choupi.uf2` (1,911,296
bytes, SHA-256 `9820f9dcee53120f0fca18b9f63bc2cabfbc8890cbeabbda4a9ddf7d68ef77f8`);
it contains only the firmware image and does not erase prior persistent card
storage. Use the explicit `*_empty_card.uf2` artifact for a clean Windows
test. Build validation passed for ARM `max_crypto`; firmware end is
`0x100e932c`. UF2 inspection confirmed the 1 MiB erased storage span with no
overlap. Continue with RV-20261103-15 crypto intermediate-state lifetime.

---

# Current session — Signature workspace transient lifetime — 2026-11-04 11:30 CET

RV-20261103-14 implementation is complete in the current worktree. Initial
Signature IV, message and recovery-hash buffers plus expanded RSA/ECDSA
message buffers now use explicit `Heap::addTransientArray()` methods. The
ambiguous `addArray(..., ClearEvent)` overload is deleted, preventing the
unscoped enum from being interpreted as an array component token. Native
heap tests include compile-time rejection of the old call shape and runtime
checks that 64-byte and 128-byte workspaces are `CLEAR_ON_RESET` transients.
VPCD ECDSA and RSA-SHA1 cases now send 80-byte inputs to exercise growth.
Implementation commit: `c6a38d4` (`Allocate Signature workspaces as transients`).

Validation passed: `./tools/test-jcarray`; the complete asymmetric VPCD suite
(`PICO2_HOST_USB_ACCESS=1 ./tools/test-asymm-crypto-vpcd`) with
`TEST:SUMMARY pass=99 fail=0`, including 97 asymmetric vectors; PC, ARM and
RISC-V `max_crypto` builds. The PC profile was restored to `max_crypto` after
the suite. Latest suite log:
`artifacts/logs/asymm-crypto-keypair-vpcd-20261027.log` (SHA-256
`438416fff71be921de3c1705125dedccd2b2b6e52d5b5f275d26dc18a59748bb`). No
physical reader was visible, so no hardware validation occurred.

Continue with RV-20261103-15 crypto intermediate-state lifetime in `TODO.md`.
Preserve unrelated dirty files. In particular, `choupi/src/heap.cpp` and
`heap.hpp` already contained unrelated AID-release changes before this step;
the RV-14 helper additions must be committed without including those unrelated
removals. Physical verification remains open.

---

# Current session — restore transient array shape and zero data — 2026-11-04 12:52 CET

RV-20261103-13 implementation is committed as `2ff86a1` (`Restore transient
array lengths and zero values`). Restored tagged transient
arrays now store their logical element count separately from serialized tag
bytes, zero initialize their new data buffers, and reject backing allocations
above 65,535 bytes before narrowing. Native tests cover byte, boolean, short,
int and reference types, including zero defaults, reset clear, last valid index,
maximum backing capacity and oversized rejection.

`./tools/test-jcarray` passed. The extended
`SAME_CAP_CONNECT_TIMEOUT_S=300 SAME_CAP_APDU_TIMEOUT_S=120
PICO2_HOST_USB_ACCESS=1 ./tools/test-same-cap-transients` passed
`TEST:SUMMARY pass=6 fail=0`; a restarted emulator restored the installed
writer applet and reported both transient array lengths as four, with zeroed
contents. PC, ARM and RISC-V `max_crypto` builds passed; ARM image endpoint is
`0x100e926c` with 93,588 bytes remaining. The authorized USB helper reported
one node but `pcsc_scan -r` exposed only Virtual PCD 00 00/01. RV-13 and
physical verification of RV-11/RV-12 therefore remain open.

The successful fresh
restore log is `artifacts/logs/same-cap-transient-vpcd-20261104T094553Z.log`
(SHA-256 `56a58e457b16886c6db8a963761c738e2cfbfef43fd5e5221beb3f6abfdc0ec5`).
The first two restart attempts correctly failed because the test omitted VPCD
`--restore-applet` metadata and then passed a CAP archive instead of contiguous
CAP components; the final run uses explicit applet records and the component
image. Preserve unrelated changes from the user/review work: `AGENTS.md`, docs
and review artifacts, SDK submodules, `choupi/src/heap.*`, `src/cap_load.cpp`,
physical/symmetric probes, and AlgTest logs/device data.

Continue with the next open priority in `TODO.md`; keep physical tests pending
until `pcsc_scan -r` lists a real Pico.

---

# Current session — reset clears both transient event classes — 2026-11-04 12:16 CET

RV-20261103-12 reset cleanup is implemented in the worktree. The reset
callback clears both `CLEAR_ON_RESET` and `CLEAR_ON_DESELECT` arrays in the
selected and cached inactive applet heaps. PC VPCD POWER_OFF, POWER_RESET and
disconnect now run reset cleanup directly without applet `deselect()`; normal
applet switching still calls `deselect()`.

`SAME_CAP_CONNECT_TIMEOUT_S=300 SAME_CAP_APDU_TIMEOUT_S=120
PICO2_HOST_USB_ACCESS=1 ./tools/test-same-cap-transients` passed
`TEST:SUMMARY pass=5 fail=0`. The added active-reset test checks both transient
arrays were zeroed and a persistent `deselect()` counter remained unchanged.
The sibling-switch case still checks that only the deselect array clears.
`./tools/build-pc --profile max_crypto`, `./tools/build arm --profile
max_crypto`, and `./tools/build riscv --profile max_crypto` passed. The PC and
ARM builds emitted existing dependency/packed-member warnings; no new source
warning was attributed to this change.

The authorized USB helper reported one attached node, but `pcsc_scan -r`
listed only Virtual PCD 00 00/01. Physical validation is therefore pending.
Regression log: `artifacts/logs/same-cap-transient-vpcd-20261104T091128Z.log`
(SHA-256 `a543c2f51609e067cb02e16fc5b9f1dca338f47d3badbf5ba5fe15b6bae2f65d`).
The initial regression attempts timed out during emulator startup/CAP LOAD at
the fixture defaults; the bounded 300-second connect and 120-second APDU run
passed.

Source changes are committed as `70248fa` (`Clear deselect transients during
card reset`). Preserve unrelated pre-existing dirty files, including
`AGENTS.md`, docs/review artifacts, SDK submodules,
`choupi/src/heap.*`, `src/cap_load.cpp`, physical and symmetric-crypto probes,
and the untracked AlgTest logs/device data. RV-11 remains open for physical
validation even though its PC regression and builds passed. Next priority:
RV-20261103-13 transient array restore logical lengths/storage, beginning with
the restored transient RSA array `ArrayIndexOutOfBoundsException` recorded in
TODO/QKB. Keep the hardware validation pending until an actual Pico reader is
listed.

---

# Current session — transient Cipher key workspace lifecycle — 2026-11-04 09:51 CET

RV-20261103-11 implementation is committed as `c619063` (`Clear Cipher key
copies with transient lifetime`). AES key copies and RSA
private Cipher modulus/exponent workspace now use transient arrays matching the
source key's CLEAR_ON_RESET/CLEAR_ON_DESELECT event. A transient marker is
checked by Cipher update/doFinal so a cleared workspace returns
`CryptoException.UNINITIALIZED_KEY` instead of using stale copies. The native
key interface cast path now checks actual NativeKey type before resolving the
class-token namespace; this is required for the dedicated RSA transient-key
fixture.

The repeatable `./tools/test-cipher-lifecycle-vpcd` passes `TEST:SUMMARY
pass=29 fail=0`: AES and RSA operations, deselect, reader reset, and a fresh
emulator process restoring the installed applet. PC, ARM and RISC-V
`max_crypto` builds pass. ARM image endpoint is `0x100e9200`, leaving 93,696
bytes. No physical board was available: the authorized helper granted one USB
node but `pcsc_scan -r` listed only Virtual PCD 00 00 and 00 01. Physical
validation remains open.

A probe APDU omission was corrected: `800A0000` had no Le although the fixture
responded with data; valid short case-2 framing is `800A000000`. The fresh
process test also found a separate RV-20261103-13 issue: directly using restored
Cipher copies returns `42 9000`, while repopulating the restored transient RSA
key gives `ArrayIndexOutOfBoundsException` (`62 9000`). Track that under the
existing transient-array restore-length item; it does not invalidate the
workspace clear assertion. Current regression CAP source is in
`tools/tests/cipher_lifecycle_applet/`; runner is `tools/test-cipher-lifecycle-vpcd`.

The PC/ARM/RISC-V builds and Java/Python/shell syntax checks passed. Preserve
unrelated dirty worktree files, including heap/persistence review work,
architecture/docs changes, JCSystem physical probes, and symmetric-crypto
scripts. The exact physical PC/SC lifecycle test is ready as
`PICO2_HOST_USB_ACCESS=1 ./tools/test-cipher-lifecycle-pcsc '<exact reader>'`;
the board must be enumerated by `pcsc_scan -r` first. Next: continue with the
RV-20261103-12 reset clearing behavior, then RV-13 transient array restore
length; keep physical validation open until an actual Pico PC/SC reader is
visible.

---

# Current session — refresh current max_crypto empty-card image — 2026-11-04 08:46 CET

Rebuilt the current worktree for PC, ARM and RISC-V using repository wrappers;
all three `max_crypto` builds passed. The ARM image is
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`,
1,912,320 bytes, SHA-256
`b964b6cf96fe9989d0740ff3fe9783d2a9c54ab9f92241282ba71ed6e7b71d57`, with
92,780 bytes remaining in the firmware window. It enables 147/150 declared
JCAlgTest capabilities, including RSA and ECC. Its bytes match
`build/arm/pico2_choupi.uf2`. No applet/CAP is bundled. This rebuild was not
flashed or runtime-tested on hardware. Firmware flashing does not remove CAPs
already persisted on a card; Windows GPPro empty-card steps are in
`artifacts/firmware/README-max_crypto-current-20261103.txt`.

The separate in-progress RV-20261103-11 transient Cipher-key lifecycle
investigation remains unresolved. Continue its VPCD failure tracing only after
this external image handoff; do not claim that this image passed hardware tests.

---

# Current session — enforce an immutable execution deadline — 2026-11-04 05:13 CET

RV-20261103-10 is implemented. `ExecutionBudget` records the time and limit
when the outermost RP2350 interpreter scope starts; nested interpreter runs
share it. Native RSA RNG callbacks update a separate service timestamp and
continue watchdog/CCID servicing, but return failure after the hard deadline.
Applet budget is now 600 seconds to preserve the measured RSA-CRT-3072
operation; INSTALL remains at 5 seconds. A fake-clock test covers wraparound,
progress, repeated nested/SIO scopes, expiration/unwind and a fresh next scope.

The first broad VPCD attempt hit the fixture's default 60-second setup connect
timeout before an APDU. The slow setup took 77.145 seconds, so the asymmetric
VPCD runner now uses a finite 300-second connect timeout and 600-second APDU
timeout. It passed `TEST:SUMMARY pass=99 fail=0` across 97 vectors. PC, ARM and
RISC-V `max_crypto` builds passed. Pico 2 `2BED26ED20556535` passed final
flash verification and finite serial. Its physical RSA-CRT-3072 generation
and components APDU returned `019000` in 72,612 ms; GPPro cleanup deleted the
temporary test package.

VPCD log:
`artifacts/logs/asymm-crypto-keypair-vpcd-20261027.log` (SHA-256
`1061bf7665519fd6024bbe41eca55844feacf872f00454d9bc582415acc98d23`). Physical
APDU log: `artifacts/logs/asymm-crypto-pcsc-20261104T040227Z.log` (SHA-256
`08b7397d42936035d7042e834495fa5d3ecc33e05e55b7875c83a02995ba9f2f`). Cycle
log: `artifacts/logs/cycle-choupi-20261104T040104Z.log` (SHA-256
`52689d2718db2a4d79917c1f9dfee35ea2ea9a3b0d02407b5218ce8caaf2ce1c`). Current
ARM UF2 `build/arm/pico2_choupi.uf2`: 1,909,248 bytes, SHA-256
`61a8f2462d80871942dd8b62000cf646b54cdb5c6647c0563d49d96f383eb5d6`.

Validation limit: the 600-second expiration was tested using the production
budget manager with an injected fake clock, not by waiting for a real hardware
timeout. The broad VPCD suite exercises RSA but does not force its interpreter
clock to expire. Source commit is `4251243` (`Enforce immutable applet
execution deadlines`); TODO/progress/QKB/HANDOFF metadata follow-up remains.
Next priority: RV-20261103-11, transient-key lifetime retained by Cipher.

---

# Previous session — preserve SensitiveResult object ownership — 2026-11-04 04:43 CET

Completed RV-20261103-09. VM-wide object `SensitiveResult` now stores a
`Stack::ReferenceValue` containing package owner and handle. Assertions compare
normalized owner+handle identity. `Context::visitRootReferences()` roots
package-local result objects only in their owning heap; global arrays/instances
are handled by global registries. The existing public Java Card API is
unchanged; only native VM state and test fixture CAP sources changed.

`./tools/test-rmi-registry` passed the owner-collision/root regression. The
interpreted SIO test uses service and client applet objects that both have
compact handle `0002`, owned by packages 57 and 58; client assertion now catches
`SecurityException`. The cross-AID VPCD suite passed `TEST:SUMMARY pass=32
fail=0`. PC, ARM and RISC-V `max_crypto` builds passed. Pico 2
`2BED26ED20556535` was reflashed; finite serial reported
`TEST:SUMMARY pass=1 fail=0`. The cycle recovered from a missing reset interface
via CDC BOOTSEL and verified the UF2.

VPCD log:
`artifacts/logs/sio-cross-aid-vpcd-20261104T033440Z.log` (SHA-256
`dc83725821ea28cad699e0ed59a52641f06f3c4821edef227e12b2a7f365b930`). Cycle
log: `artifacts/logs/cycle-choupi-20261104T034116Z.log` (SHA-256
`cd60290d67e2b46bceeb5bd2c41f1d7fdfe14551f0ce91fe1331cc2a743bb28e`). Current
ARM UF2 `build/arm/pico2_choupi.uf2`: 1,908,736 bytes, SHA-256
`b3c521ea19a8cb1832d97f99faa8a4f4817d136798a50d10d076b2675041e617`.

Implementation source commit: `7057014` (`Preserve SensitiveResult object
ownership`). TODO, progress, QKB and HANDOFF metadata are now updated; metadata
follow-up commit is `598ae54` (`Record RV-09 owner provenance validation`).
Preserve all unrelated pre-existing changes noted below. Next priority is
RV-10, non-renewable top-level execution deadlines.

---

# Previous session — reject unsafe foreign references in applet arrays — 2026-11-04 04:23 CET

Completed RV-20261103-08 implementation; source commit is `24a24ba` and
metadata follow-up is in progress. `JC_Array::setReferenceEntry()` now fails closed with
`SecurityException` before mutation when an applet-owned array is given a
foreign-package reference. Runtime-global Object arrays remain owner-tagged;
their per-package root enumeration already routes entries to the owner heap.

`./tools/test-rmi-registry` passed the production heap regression: equal and
non-equal compact handles were both rejected, receiver entries remained null,
the owner collector reclaimed both arrays, and both slots were safely reused.
`python3 tools/tests/sio_cross_aid_vpcd_test.py` passed all 31 cases, including
same-AID service package deletion/reinstall and stale proxy rejection after the
failed retention attempt. VPCD required elevated localhost network access (no
USB). PC, ARM and RISC-V `max_crypto` builds passed. Pico 2
`2BED26ED20556535` flashed the current ARM image and its finite serial monitor
passed. Cycle log `artifacts/logs/cycle-choupi-20261104T032120Z.log` (SHA-256
`7da2144f5187c2080738f303f4995f89b3075581c5b84be13708dd37aa5f7ea3`). Current
UF2: `build/arm/pico2_choupi.uf2`, 1,908,736 bytes, SHA-256
`7f0eeba6e0af85d859e4d5c7a5a777a97f2d23b1e15f9addd8344acc465d8e13`, end
`0x100e8e30`, 94,672 bytes remaining. It bundles no CAP and does not erase
packages already stored on a token.

VPCD transcript:
`artifacts/logs/sio-cross-aid-vpcd-20261104T031510Z.log` (SHA-256
`8d31156959a9222acdbff0d34eeaa889442cb13131d665fa05b7bffaeb833c0a`). The
R08 workaround deliberately rejects foreign references in applet-local arrays
instead of tracing owner provenance across heaps. Next: RV-20261103-09,
object-sensitive result roots with missing owner provenance. Preserve all
unrelated worktree changes, especially `AGENTS.md`, architecture docs/review
artifacts, SDK submodules, `choupi/src/heap.*`, physical probe tooling and
`src/cap_load.cpp` changes.

# Current session — applet transaction lifecycle abort — 2026-11-04 04:00 CET

Completed RV-20261103-07 and committed the implementation as `8ece8ec`
(`Abort applet transactions at lifecycle boundaries`). Runtime-owned CAP INSTALL
transactions now have explicit ownership and are not exposed to applet JCSystem
commit/abort calls. Applet transactions are aborted after `process`, `select`,
and `deselect`; implicit reader reset cleanup aborts applet-owned state without
calling `deselect`, and reports rollback failure. A normal `process` return
with an open transaction returns `6F00`; rejected selection remains `6999`;
deselection with an open transaction returns `6F00`.

VPCD APDU regression cases 85–88 pass for normal return, uncaught transaction
exception, deselect and rejected selection. Each checks persistent rollback;
the rejected select is followed by a successful select, and the deselect path
selects the card manager before reselecting the test applet. The persistent
transaction unit test confirms the applet abort guard leaves a runtime-owned
INSTALL transaction intact. Local VPCD required elevated network access due
restricted-loopback `EPERM`. Test result: `TEST:SUMMARY pass=6 fail=0`, elapsed
151.984 s. This slow time includes PC CAP install/setup; no CPU profile was
captured. `--light-install` skips the 32,767-byte test array for lifecycle-only
runs, but did not materially reduce setup time.

`./tools/test-persistent-transaction`, `./tools/build-pc --profile
max_crypto`, ARM and RISC-V wrappers passed. Pico 2 `2BED26ED20556535` flashed
the final ARM `max_crypto` CHOUPI image and finite serial passed
`TEST:SUMMARY pass=1 fail=0`; cycle log:
`artifacts/logs/cycle-choupi-20261104T025837Z.log` (SHA-256
`058a5e226ba0a33fffbcc414cfc130806c6e68650741baf2f1ea7004d4c6449b`). The
current UF2 is `build/arm/pico2_choupi.uf2`, 1,908,736 bytes, SHA-256
`d451ce0482558b4eed6e09470c4ea69fc7bc3707c4f30e05c885634c3b51ddce`, firmware
end `0x100e8e34`, with 94,668 bytes remaining. It bundles no CAP; flashing does
not clear packages already stored on the token.

Timeout and reset while bytecode is actively executing were not injected. All
interpreter return/error statuses pass through the process cleanup guard, and
the synchronous VM cannot receive a warm-reset command while executing one
APDU. Keep this validation limitation explicit. Next TODO is RV-20261103-08:
foreign retained-reference owner provenance. Preserve the extensive unrelated
dirty worktree: `AGENTS.md`, deleted root architecture doc/new docs, review
artifacts, SDK submodules, `choupi/src/heap.*`, physical probe tooling, and
unrelated `src/cap_load.cpp` edits. RV-07 implementation and TODO are committed;
its progress and handoff metadata follow-up is pending.

# Current session — INSTALL preflight — 2026-11-04 03:09 CET

Completed RV-20261103-06 and committed implementation as `7f572a4`
(`Preflight applet installation before applet code`). `cap_store_preflight` now
rejects invalid/duplicate instance AIDs, missing packages and full per-package
instance tables before the applet INSTALL method executes. Registry invariants
are checked again before journal publication, with transaction rollback on a
failed recheck. Duplicate/full conflicts map to `6A80`; pending-install
recovery also verifies that the owner ID matches its journal record. PC and
RP2350 emulation share the 40-instance package limit.

Validation: `./tools/test-rp2350-fs-overlay` passes same-package duplicate,
cross-package duplicate and 40-instance full-table checks;
`./tools/test-install-status` passes all status mappings;
`./tools/test-persistent-transaction` passes including the counter tag ABI
regression. `./tools/build-pc --profile max_crypto`, ARM and RISC-V wrappers
pass. Protected PC VPCD integration completed `TEST:SUMMARY pass=47 fail=0`,
including duplicate rejection without registry mutation, clean fresh retry,
restart, lifecycle and transient cases. On Pico 2 `2BED26ED20556535`, the
current ARM image flashed and finite startup passed (`TEST:SUMMARY pass=1
fail=0`); real GPPro duplicate INSTALL returned `6A80`, GET STATUS showed one
original instance, a fresh instance installed successfully, and GET STATUS
pagination passed (`TEST:SUMMARY pass=2 fail=0`). The temporary Multi package
was fully removed. Physical cycle log:
`artifacts/logs/cycle-choupi-20261104T020103Z.log`; physical APDU/cleanup log:
`artifacts/logs/physical-get-status-multipage-20261104T020406Z.log`.

Counter tag correction was independently committed as `7cd2490`; metadata is
`109eda6`. RV-06 metadata/progress commit is pending. Preserve unrelated dirty
worktree changes, especially `AGENTS.md`, review artifacts, `choupi/src/heap.*`,
physical probe sources, and unrelated `src/cap_load.cpp` PC wrapper,
whitespace, CAP-load diagnostic and comment hunks. Next: continue with
RV-20261103-07, automatic transaction abortion across applet lifecycle and
reset boundaries.

# Current session — cross-store CAP lifecycle recovery — 2026-11-04 02:07 CET

Implemented and committed RV-20261103-05 as `0914836` (`Recover interrupted
CAP lifecycle publication`). Recovery reconciles the CAP store with persistent
filesystem state before package validation, JCVM restore, LOAD retry, INSTALL,
and DELETE. It removes CAP slots left disabled by incomplete LOAD/package
DELETE, clears and disables stale package bits without CAP slots, and removes
instance registry entries whose persistent applet root is gone. PC emulator
startup runs recovery before validating bitmap/registry consistency and fails
closed on recovery errors. The exact tested ARM image is available at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2` (1,907,712
bytes, SHA-256 `a3fa5e7ff07c166b8220b72caef3871215d57710695be7e1c3f47eae0a7eeb8e`);
it bundles no applet and leaves 95,020 bytes in the firmware window.

Validation passed: `./tools/test-rp2350-fs-overlay` (all five lifecycle
publication/retry tests, `TEST:SUMMARY fail=0`),
`./tools/test-persistent-transaction` (`TEST:SUMMARY fail=0`), PC emulator,
ARM and RISC-V `max_crypto` builds, plus CHOUPI flash verification and finite
serial on Pico 2 `2BED26ED20556535` (`TEST:SUMMARY pass=1 fail=0`). The board
needed CDC BOOTSEL recovery after reset-interface discovery failed. Final
cycle log: `artifacts/logs/cycle-choupi-20261104T005831Z.log`. Overlay tests
seed durable state at each publication cut point and repeat recovery; they do
not cut power during a live on-device CAP-slot sector write.

Next: implement RV-20261103-06, checking duplicate AIDs and instance-registry
capacity before invoking applet INSTALL. The dirty worktree still contains
unrelated edits; preserve them, especially `AGENTS.md`, architecture docs,
review artifacts, `choupi/src/heap.cpp`/`.hpp`, physical JCSystem probe files,
and the unrelated `src/cap_load.cpp` PC filesystem wrapper, whitespace and
CAP-load diagnostics. RV-05 source/artifact commit is `0914836`; this metadata
follow-up records that SHA.

---

# Current session — transactional DELETE namespace erase — 2026-11-04 01:34 CET

Implemented RV-20261103-04. `FlashMemory_Handler::erasePersistentPrefix()` and
`erasePersistentPackageState()` require an active persistent transaction, walk
the read-only filesystem cursor to snapshot all matched values, then invoke the
backend bulk erase. `clear_applet_persistent_state`, package static cleanup,
orphan cleanup and package-state cleanup now call these wrappers. A failed
enumeration/snapshot fails before destructive erase starts. Package-wide
enumeration uses a non-null pointer for the zero-length prefix; the actual
RP2350 `fs_next_tag` implementation rejects null even for an empty prefix.

Validation passed: persistence transaction tests, plus the production RP2350
overlay test linked with `flashmemory.cpp`. Injected backend failure after one
actual erase restored the old records; `fs_init()` from committed flash found
both values intact. The package-level test also retained static, applet,
lifecycle and unrelated package data and verified CAP package lookup, applet
instance count and owner mapping. ARM/RISC-V wrapper builds and PC JCVM build
passed. The exact ARM image flashed and finite startup monitor passed on Pico 2
`2BED26ED20556535` (`TEST:SUMMARY pass=1 fail=0`). Current max artifact: UF2
1,907,200 bytes, SHA-256
`b4f1120c7dfec2c653176eb5298b55de1f6d9b2e9a5a56d078b3109c9d7ab1d5`, end
`0x100e8b14`, 95,468 bytes remaining. It contains no applet but preserves any
packages already stored on a token. Targeted DELETE failure behavior was proven
in the production overlay harness, not by an injected hardware DELETE.

Implementation and metadata commits are pending. Pre-existing `src/cap_load.cpp`
changes must be preserved: its PC filesystem wrapper and CAP load diagnostic
hunks are unrelated; selectively stage only the four DELETE cleanup call-site
hunks from this step. Then continue RV-20261103-05, cross-store DELETE/LOAD
publication recovery. Preserve all other worktree edits.

# Current session — static reference rollback invalidation — 2026-11-04 01:10 CET

Implemented RV-20261103-03. Heap static-reference cache entries now record the
persistent-write rollback generation. Reads and GC root enumeration ignore a
root cached before a rollback or failed commit; the next read resolves the
restored record and recaches its canonical reference. Regression coverage in
`tools/tests/persistent_transaction/persistent_transaction_test.cpp` checks a
null substitution followed by abort, an object substitution followed by an
injected failed commit, canonical original-array restoration, and GC root
retention.

Validation passed: `./tools/test-persistent-transaction` with
`TEST:SUMMARY fail=0`, both ARM/RISC-V `max_crypto` builds, and hardware cycle on
Pico 2 `2BED26ED20556535`. The wrapper first hit reset-interface discovery
failure, recovered via CDC BOOTSEL, verified flash, and finite serial ended
`TEST:SUMMARY pass=1 fail=0`. Current max ARM artifact was refreshed to the exact
output: 1,905,664 bytes, SHA-256
`1bcfd83bb1660c9122b8419e65c76846a4716cc16c4453a1a33999eef2cb41da`, end
`0x100e89b8`, 95,816 bytes remaining. The image has no bundled CAP; flashing
does not clear persistent token contents.

Implementation and metadata commits are pending. Preserve pre-existing heap
changes: `choupi/src/heap.cpp` has a pre-existing removal hunk for
`releaseGlobalAIDObject`, and `heap.hpp` has its matching removal. Stage only the
new generation-field/cache hunks from those files. The persistence test file's
RV-02 changes are already committed; stage its new RV-03 block only as usual.
After commit, continue with RV-20261103-04: make DELETE prefix erasure rollback
aware at the storage layer, beginning with current implementation mapping and a
failure-injection regression.

# Current session — persistent array identity and alias preservation — 2026-11-04 01:01 CET

Implemented RV-20261103-02 in `choupi/src/jc_handlers/flashmemory.cpp`.
Persistent array body keys retain the owning static-field prefix but append a
reserved `0xffff` marker and a durable 32-bit identity. A versioned counter
record is saved using the transactional persistent-tag writer. This prevents a
replacement array from overwriting data that is still reachable through an
older alias. The production persistence harness now tests reassignment,
fresh-heap restore, collection, rollback, and identity non-reuse.

Validation passed: `./tools/test-persistent-transaction`
(`TEST:SUMMARY fail=0`), the review real-heap probe
(`ARRAY_RETARGET old=0003 new=0005 old_byte=11 alias_byte=11 expected=11/11`),
and ARM/RISC-V `max_crypto` builds. The physical cycle
`PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile max_crypto --serial
2BED26ED20556535` flashed and verified the image on 2BED and the finite monitor
reported `TEST:SUMMARY pass=1 fail=0`. No CAP-level persistent-alias scenario
was exercised on hardware. Updated current max ARM UF2: 1,905,664 bytes, SHA-256
`96d1f3486ba9fd910d58c0ede2e5dfc863f562700e44a34037cd93e50e1a0f26`, ending at
`0x100e8990` with 95,856 bytes remaining. It contains no bundled applet;
reflashing does not erase applets already persisted on the token.

Implementation commit and its metadata follow-up are still pending. Stage only
RV-02 source/tests and its TODO/QKB/HANDOFF/progress/artifact records, preserving
all other dirty changes. Then begin RV-20261103-03 (aborted static-reference
cache restoration) as a new logical step. Physical access must continue using
`PICO2_HOST_USB_ACCESS=1` and elevated host access on the first USB command.

# Current session — CCID RX bounds and physical failover — 2026-11-04 00:43 CET

Implemented RV-20261103-01 in the production CCID receive path. New
`src/ccid_rx_framing.hpp` compares payload length to the available limit before
addition, requires a valid 10-byte header and slot 0, requires active slot
state for XfrBlock, rejects overlong frames, and provides bounded next-transfer
sizes. `queue_out()` requests at most the endpoint size, remaining frame size,
and remaining receive-buffer capacity; `driver_xfer()` checks the reported
byte count against the armed transfer. Focused host shim
`./tools/test-ccid-rx-framing` passes all wrapped 32-bit lengths, exact and
oversize boundaries, fragmented headers/payloads, slot/state and buffer-bound
cases. Both `./tools/build arm --profile max_crypto` and RISC-V builds pass.

Hardware recovery: cycle by 8E's runtime serial first failed reset-interface
discovery. The board later appeared in BOOTSEL and `picotool info` confirmed
chip ID 8E; `cycle ... --chipid 8E...` flash verification passed, but its
serial monitor found no CDC port and PC/SC did not list 8E afterward. USB
descriptor checks showed the remaining runtime board was 2BED, so no results
were attributed across identities. Failover to 2BED passed the ARM cycle and
finite monitor (`TEST:SUMMARY pass=1 fail=0`); after authorized reader refresh,
PC/SC saw 2BED and read-only GPPro `--info` completed ATR, SELECT, CPLC, Card
Data and GET DATA. Transcript: `artifacts/logs/ccid-rx-framing-gppro-2bed-20261103.log`.

The current firmware alias
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2` now
matches the changed build: 1,905,664 bytes, SHA-256
`86081613e6ab0e2ee88be2c679d6f2005672d283b51dfd5d0f6747de7205b2b9`, ARM
image end `0x100e88b8`, 96,072 bytes remaining. This updated hash has passing
builds and verified flash on 2BED, plus valid physical CCID APDUs. It contains
no applet; firmware updates still do not erase persistent packages. The
previous hash's full API Security/Crypto sweeps do not verify this changed
hash. Runtime state of 8E remains unconfirmed, separately from RV-01, which
passed on 2BED. Continue with RV-02 or another P1 offline item. Preserve
unrelated worktree changes.

# Previous session — current max firmware and physical API sweeps — 2026-11-04 00:14 CET

Rebuilt the current `max_crypto` profile using `./tools/build arm --profile
max_crypto` and `./tools/build riscv --profile max_crypto`; both passed. ARM
image ends at `0x100e8820` (96,224 bytes remain). Its freshly built UF2 matches
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2` exactly:
1,905,664 bytes, SHA-256
`756fc23ca143f93625752855f2389eca201c6da11cd630b779f93d898f3d4bc6`. This
firmware bundles no CAP/applet. It does not erase installed applets retained in
the token's persistent store. The updated artifact README provides Windows
GPPro cleanup commands and records the exact image's physical testing.

Physical testing already completed on the same image hash: ARM cycle and
finite serial passed on 2BED (`2BED26ED20556535`) and 8E
(`8E464F4E2BBC0A19`). On 2BED, API 3.0.5 Security completed 901 calls (397 OK,
504 expected unsupported, no unexpected results). Crypto completed all 466
calls on both boards (304 OK, 161 expected unsupported, one expected upstream
SHA-512 PSS `ILLEGAL_VALUE`); both matched VPCD. Test CAP cleanup was not
reliably confirmed: the Crypto runner reported reader-not-found after the test;
2BED still listed its test package, and two direct GPPro DELETE attempts timed
out after sending DELETE. A reflashing cycle restored PC/SC without deleting
the package. The 8E registry did not list Crypto afterward, but runner cleanup
still reported reader-not-found. Per-board data/logs and hashes are in
`.agent/QKB.md` and `artifacts/results/deploy/physical-crypto-*/`.

The external Windows image is ready at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`. Confirm
`gp --list` has no `APP:` or `PKG:` rows before installing any CAP; BOOTSEL
flashing alone never empties the persistent card store. Do not use the stale
non-current max artifact. Existing unrelated shared-worktree edits remain
untouched. Next: investigate/reproduce the physical DELETE/CCID timeout under
HOST-01/STORE-01 without deleting existing user packages; otherwise hand the
current UF2 and explicit Windows cleanup flow to the user.

# Previous session — API 3.0.5 Security VPCD sweep — 2026-11-03 23:18 CET

## Full API 3.0.5 Security VPCD sweep — 2026-11-03 23:17 CET

Completed the unchanged `AlgTestApiSecurity_305.cap` on fresh PC JCVM storage
through `Virtual PCD 00 00`. All 901 calls completed in 3:21:12 (upload at
19:56:27 CET, cleanup at 23:17:39 CET): 397 `OK`, 504 expected
`UNSUPPORTED_PER_SPEC`, no unexpected result or resource error. GPPro cleanup
removed the CAP. The run did not reproduce cumulative memory pressure in the PC
emulator; API-01 remains open for physical Security/Crypto validation and
allocator-state capture if hardware failures recur. Large RSA/EC API calls
occasionally took multiple minutes but did advance. Keep a live `pcsc_scan`
poller running when starting VPCD; otherwise its TCP endpoint may refuse the
connection. Full result artifacts and checksums are in `.agent/QKB.md` and
`progress.md`; raw client log is archived under
`artifacts/results/deploy/vpcd-full-security-current-max-20261103/`.

Next: continue API-01/03 acceptance with physical devices when enumerated;
otherwise pick the next offline implementation/verification item from TODO.
Current authorized reader state was last zero physical readers; refresh the
host-access reader helper before any hardware test.

## Full API 3.0.5 Crypto VPCD sweep — 2026-11-03 18:46 UTC

Closed EMU-01 using the unchanged Crypto CAP on fresh PC JCVM storage through
`Virtual PCD 00 00`. `AlgTestApiCrypto_305.cap` completed all 466 calls; upload
to cleanup took 1:45:54 under a finite 14,400-second area deadline. Outcomes:
304 `OK`, 161 expected `UNSUPPORTED_PER_SPEC`, and one known
`EXC_CARDRUNTIME reason=0x0001` on `ALG_RSA_SHA_512_PKCS1_PSS`. The QKB explains
that the upstream vector's 1024-bit RSA-CRT key cannot fit SHA-512's default
64-byte PSS salt, so `ILLEGAL_VALUE` is correct. No `WRONG_RESULT`, transport
error, or missing result occurred. GPPro cleanup removed the test CAP.

The first emulator starts failed because the VPCD endpoint refused connection;
a short `pcsc_scan -r` had exited. Keeping a live `pcsc_scan` poller running
while starting the emulator made the reader present (`VpcdPresenceProbe
present=true`) and enabled the complete sweep. Artifacts and hashes are in the
new QKB section and `progress.md` record. Full CSV:
`artifacts/results/deploy/vpcd-full-crypto-current-max-20261103/crypto.csv`.
This is emulator evidence only; API-03 physical acceptance remains open.

## Verify max_crypto build artifact — 2026-11-03 16:48 UTC

The exact `build/arm/pico2_choupi.uf2` the user referenced is generated with
`JC_SUPPORT_PROFILE "max_crypto"` and enables RSA and ECC selectors (including
RSA-2048, RSA cipher/signature, EC-256, ECDSA, and EC key-pair generation). Its
UF2 block structure is valid. Rebuilt via repository wrappers:
`./tools/build arm --profile max_crypto` and
`./tools/build riscv --profile max_crypto`; both passed. ARM ends at
`0x100e8820`, leaving 96,224 bytes before the 1 MiB firmware boundary. The
rechecked ARM hash is
`756fc23ca143f93625752855f2389eca201c6da11cd630b779f93d898f3d4bc6`.
No physical Pico reader was present (authorized helper found zero), so this
exact image has not been runtime-validated on hardware. The packaged
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2` is a different
hash/image; do not conflate its prior acceptance state with this build output.
Next: when a physical reader is enumerated, flash this exact ARM image and run
finite serial plus the unchanged upstream JCAlgTest support check; retain the
upstream CAP unchanged.

The current build output was also copied without modification to
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2` for
external testing. Its checksum and Windows empty-card steps are in
`artifacts/firmware/README-max_crypto-current-20261103.txt`. Do not direct the
user to the older `pico2-jcvm-max_crypto-arm-empty-card.uf2` artifact for this
current build request. On Windows, always provide a firmware image with no
bundled applet and include steps to remove persistent applets/packages before
the explicit CAP upload; BOOTSEL flashing alone preserves card contents.

## CTX-01 owner-aware array-store audit — 2026-11-03 16:36 UTC

Source review confirms `bc_aastore()` preserves reference owner provenance,
but `JC_Array::setReferenceEntry()` rejects foreign values for typed arrays.
Removing that guard would be unsafe: the assignability check resolves the
compact handle through the active service heap, and compact values can collide
across package heaps. `putfield_a` and `putstatic_a` use current-package-only
reference pops, so foreign values are rejected before storage. Persistent
instance/static/array support also needs a serialized owner identity and
owner-aware GC roots. Details are in TODO CTX-01 and QKB. No code changed and no
build or test was run for this source audit. Next implementation prerequisite:
add safe retained-Context/Heap resolution by owner package and test typed
`aastore` with equal compact handles; keep fail-closed guards until then.
Audit record commit: `f3c49c4`.

## Follow-up: verify false Signature results across SIO — 2026-11-03 16:34 UTC

Extended the interpreted Signature SensitiveResult fixture with a corrupted
AES MAC. Native `verify()` returns `false` without throwing; the service result
now survives SIO and the client successfully calls `SensitiveResult.assertFalse()`.
CAP conversion passed and the loopback VPCD suite passed `TEST:SUMMARY
pass=31 fail=0`; log:
`artifacts/logs/sio-cross-aid-vpcd-20261103T163223Z.log` (SHA-256
`61c7955c8b267f8fed7859a1c3f49aba65c76a114c609275de2efddee8eecc7a`). No
firmware code changed in this step; the prior native fix has PC, ARM, and
RISC-V build coverage. No physical reader was available. The suite's printed
summary count is 31 even though it now includes the new check; keep the exact
reported count rather than inferring an increment from the assertion count.
Commit: `fb8e2be`. Next: continue CTX-01/03 foreign-reference lifecycle and
consumer audit.

## Follow-up: native Signature SensitiveResult propagation — 2026-11-03 16:25 UTC

Implemented SensitiveResult propagation for native `Signature.sign()` and
`verify()` methods, including message-recovery and precomputed-hash variants.
Handlers clear stale state on entry and set short/boolean values after
successful completion. The interpreted three-package fixture performs AES MAC
sign/verify: the service asserts the signature length and the client asserts
the boolean verification result after SIO. PC, ARM, and RISC-V builds passed;
the cross-AID loopback suite passed `TEST:SUMMARY pass=32 fail=0` in
`artifacts/logs/sio-cross-aid-vpcd-20261103T162028Z.log`. No physical Pico
reader was attached, so hardware validation remains outstanding. A first
2-byte AES MAC NOPAD input was invalid; block-aligned 16-byte input passes.
Implementation commit: `98df2d1`. Next CTX-02 targets include PIN, random-data, checksum,
AEAD/key APIs, object-result GC rooting, then physical validation.

Follow-up: extended the native SensitiveResult audit to `Cipher.update()` and
`Cipher.doFinal()`. Both handlers clear previous state on entry and publish the
short result only after the RSA or symmetric operation succeeds. The
interpreted three-package fixture creates a persistent AES-128 key, calls AES
CBC update/final, asserts update's result inside the service, then asserts the
final output length after SIO returns. This extends the existing VPCD suite to
`TEST:SUMMARY pass=31 fail=0`; PC, ARM, and RISC-V builds passed. A transient
AES key fixture initially failed with uncaught `SecurityException`; using the
same persistent AES key form used by the symmetric crypto probe passes.
Authorized reader refresh still reports zero Pico devices, so hardware
validation remains outstanding. Commit `09eb9b6` contains the Cipher change and
tests. Next CTX-02 audit targets additional declared producers (PIN, signature,
random data, checksum, AEAD and enabled key APIs), then object-result GC rooting.

Implemented the next CTX-02 gap after the Java-level SIO tests exposed a native
API omission. `MessageDigest.doFinal()` returned the right SHA-256 output length
but did not update SensitiveResult, so the caller's assertion after SIO failed.
`choupi/src/native_message_digest.cpp` now resets prior state at method entry
and records the output length only after digest completion and buffered-input
cleanup succeed. The Java Card fixture checks successful SHA-256 output bytes,
asserts the length in the caller after SIO, and verifies an invalid input range
leaves a prior object result unassigned.

Validation passed: `./tools/build-pc`, `./tools/build arm` and
`./tools/build riscv`; test CAP conversion; Python syntax check; and elevated
loopback VPCD integration with `TEST:SUMMARY pass=31 fail=0`. Final log:
`artifacts/logs/sio-cross-aid-vpcd-20261103T154309Z.log`. The authorized reader
helper found zero physical devices, so no firmware flash or physical API check
was possible. The code change needs a physical rerun when USB passthrough
returns. QKB records the cause and viable fix; TODO CTX-02 tracks remaining
producers, GC rooting and physical coverage.

---

# Current session — extend SensitiveResult object-result SIO coverage — 2026-11-03 15:35 UTC

Extended the current cross-AID fixture with two additional interpreted SIO
checks. First, the client saves its object SensitiveResult from
`JCSystem.getAID()`, calls a passive service method that invokes no sensitive
API, then asserts the same object result after returning. Second, the service
asserts object results from its own `JCSystem.getAID()` and
`getPreviousContextAID()` calls. The fixture converted cleanly; the elevated
loopback VPCD suite passed `TEST:SUMMARY pass=29 fail=0`, including all previous
short-result, exceptional-result, firewall, restart, DELETE and reinstall
checks. Log: `artifacts/logs/sio-cross-aid-vpcd-20261103T153220Z.log`.

No runtime firmware or upstream CAP changed. The physical reader helper still
reports zero Pico devices in this VM. CTX-02 remains open for GC-rooting an
object-valued result after ordinary Java locals are dropped, additional
producers (PIN and enabled crypto APIs), and physical validation. TODO reflects
the 29/29 coverage; previous step `2f55cab` covered the initial two SIO checks.

---

# Current session — Java-level SensitiveResult across SIO — 2026-11-03 15:30 UTC

Added cross-AID CAP fixtures that exercise Java Card `SensitiveResult` from an
interpreted client applet after a service applet returns. The service performs
successful `Util.arrayCopyNonAtomic`; the client then asserts the returned short
after the SIO context switch. A second case has the service call array-copy with
an invalid range, catch the resulting `ArrayIndexOutOfBoundsException`, and
verify that the caller cannot assert a stale sensitive result. Both pass in
VPCD. The full cross-AID suite passes 27/27, including old firewall, exception,
restart, DELETE, and same-AID reinstall cases. No runtime firmware or upstream
CAP was changed.

Validation: `./tools/build-pc` passed; `./tools/build-sio-cross-aid-fixture`
converted all 3 test CAPs with zero conversion errors (expected int-support
warnings); Python syntax check passed; elevated local VPCD test passed
`TEST:SUMMARY pass=27 fail=0`. The initial converter internal error was caused
by enlarging the client's already large `process()` method; moving the check to
a private helper fixed conversion. No physical Pico reader is currently
available in this VM (`pico2-pcsc-reader` found zero), so no hardware run was
attempted. Exact log: `artifacts/logs/sio-cross-aid-vpcd-20261103T152735Z.log`.

CTX-02 remains open: add Java-level assertions for other sensitive-result
producers (including AID/JCSystem, PIN and enabled crypto paths), object-result
GC rooting through actual interpreted SIO, and physical coverage. Preserve
unrelated dirty files. See latest `TODO.md` CTX-02 entry and progress record.

---

# Current session — classify Windows GPPro and certutil results — 2026-11-03 15:16 UTC

The latest Windows 10 report changes the earlier MUTE diagnosis. GPPro 20.01.23
sees both `Pico2 JCVM CCID 0` and `Pico2 JCVM CCID 1`; both power/connect at T=1
with ATR `3B800181`, select the ISD, and successfully read CPLC and Card Data
(`9000`). Optional IIN/CIN, Card Capabilities and Key Info Template return
`6A88`. This confirms Windows reader enumeration, activation and APDU traffic
work in this run.

`certutil -scinfo` separately sees reader 0 as `PRESENT | UNPOWERED`, says the
card is available, and reads the ATR, then fails at
`SCardGetCardTypeProviderName` with `ERROR_FILE_NOT_FOUND (0x2)`. This is a
missing Windows card-type/provider/minidriver association for the ATR, not a
firmware APDU failure. No firmware change is indicated unless Windows
certificate-store/minidriver integration is an explicit product requirement.
The earlier Windows MUTE/USBPcap investigation is superseded for the tested
session; retain capture instructions only if the failure recurs.

The CPLC `6304` date values are unchanged. Repository-pinned newer GPPro treats
them as year digit 6, day 304 (2026-10-31); the older Windows GPPro 20.01.23
prints 2016-10-30. This is a date-epoch interpretation difference. Details and
Microsoft references are in the newest section of `.agent/QKB.md`.

No firmware, test CAP, or applet state changed. No hardware was accessed from
this VM for this report; results are user-provided Windows evidence. Continue
with the open combined-image full JCAlgTest static rerun only when a physical
reader is passed through to this VM.

---

# Current session — JCAlgTest 1.8.4 static failures fixed on 2BED — 2026-11-03 15:10 UTC

Completed a full upstream `ALG_PERFORMANCE_STATIC` run with
`AlgTest_JavaCard/deploy/client/AlgTestJClient_v1.8.4.jar` and unchanged
`AlgTest_v1.8.4_jc305.cap` on Pico 2 `2BED26ED20556535`. It took 2,209 seconds,
with 1,847 expected `NO_SUCH_ALGORITHM` cases, four `CANT_BE_MEASURED`, zero
reconnects, and zero manual resets. The first two prior `6400` results
(`Util_arrayCopy_EEPROM`, `Util_arrayCopy_RAM2EEPROM`) passed under cooperative
progress and a 60-second guard; their averages were 496.33 and 472.22 ms/op.
`SWAlgs_AES()` still hit the 60-second guard at 60,497 ms. The non-install
budget is now bounded at 120 seconds and interpreted bytecode services the
watchdog/CCID every 4,096 instructions without refreshing that deadline. The
exact 50-iteration SW AES APDU returned `AA9000` on hardware with that limit.
SHA-256 doFinal measured 5.79 ms/op.

The three DES `clearKey()` `6F00`s came from the JC305 DESKey interface map:
class token 10 swapped set/get but omitted the inherited Key-method remap.
`choupi/src/jc_handlers/jc_cp.cpp` now maps methods as `{1,3,2,0,5,4}`. Focused
exact clearKey sequences all return `AA9000` for DES 64/128/192 bits on the
final ARM image. PC/ARM/RISC-V builds pass; native-progress tests pass 6/6 and
CCID flow tests pass 9/9. Final firmware hash:
`07605231943cf52dbfaeeb3e17ff7e7b621cf5b904fc5e63a74ca7d886e16010`. Verified
flash and finite serial runtime monitoring passed on 2BED.

The full combined-image rerun did not start: the authorized reader helper found
zero physical readers, and `pcsc_scan -r` listed only the two VPCD readers.
This remains the only acceptance gap for this work: rerun the complete unchanged
1.8.4 static operation on the combined image when a physical Pico re-enumerates.
Do not treat the reader disappearance as a test or firmware failure. JCAlgTest
1.8.4 remains installed on 2BED; no CAP/app content changed. Existing SIO
fixture packages were preserved.

Files changed for this work: `choupi/src/interpretor.cpp`,
`src/choupi_pico2.cpp`, `docs/NATIVE_PROGRESS_CALLBACKS.md`, and
`choupi/src/jc_handlers/jc_cp.cpp`; progress/QKB/TODO are updated. Cooperative
progress commit: `74a5fd9`. DES mapping and final knowledge/progress update are
committed as `22a6255`. Preserve other dirty changes (notably AGENTS.md,
`choupi/src/heap.*`, `src/cap_load.cpp`, and the physical JCSystem probe).

Evidence:
- Full run: `artifacts/results/jcalgtest-184-static-2bed-cooperative-20261103/`
  (log SHA-256 `c333e47f33e4833938f4f079e36eba3debf7341b6e032f9f2227c0f57a913867`).
- DES focused run: `artifacts/logs/jcalgtest-184-des-clear-2bed-20261103.log`.
- Final flash/runtime: `artifacts/logs/cycle-choupi-20261103T150345Z.log`.
- Support files and exact details: latest `progress.md` records and `.agent/QKB.md`.

---

# Current session — refresh physical SIO test availability — 2026-11-03 14:04 UTC

Authorized refresh (`PICO2_HOST_USB_ACCESS=1 sudo -n
/usr/local/sbin/pico2-pcsc-reader`, followed by `pcsc_scan -r`) sees one
physical device: Pico 2 `2BED26ED20556535`. Exact-reader GPPro `--list`
confirms the SIO client and service fixture packages `A000000151000004` and
`A000000151000003` are already installed (along with JCAlgTest and API applets).
The physical same-AID delete/reinstall regression cannot run safely on this
board because its harness requires absent fixture AIDs and will not overwrite
them. No card state was changed. Continue with offline SIO/reference audit;
retry physical coverage only on another identified token with no conflicting
fixture AIDs. The Windows MUTE trace remains separately pending.

# Current session — classify JCAlgTest 1.8.4 static timeout results — 2026-11-03 14:07 UTC

Analyzed the exact static-performance run. Three `6400` cases
(`Util_arrayCopy_EEPROM`, `Util_arrayCopy_RAM2EEPROM`, `SWAlgs_AES`) terminate
at 15,074–15,421 ms, matching `Interpretor::run()`'s 15,000,000-us non-install
applet execution budget. This is an explicit firmware workload cutoff. Three
DES `clearKey()` cases instead return `6F00` in 715–750 ms; they are a separate
unresolved cause. TODO and QKB now distinguish these classes. Next: design
cooperative interpreter progress (watchdog and CCID extension servicing) for
long interpreted applet work before changing the budget, and separately
instrument the DES persistent-key path. The upstream CAP/client remain unchanged.
The only physical reader is still 2BED and contains SIO fixture packages; do
not overwrite them. Windows USBPcap remains pending.

# Previous session — Windows certutil MUTE reproduced in report — 2026-11-03 13:59 UTC

User reports a Windows `certutil -scinfo` run taking several minutes, then
listing one reader `Pico2 JCVM CCID 0`, state `SCARD_STATE_PRESENT |
SCARD_STATE_MUTE`, and “The card is unrecognized or not responding.” CertUtil
then says its diagnostic completed successfully. This means the enumeration
command completed; it is not evidence that card activation succeeded. Windows
board identity and flashed UF2 hash are still unknown. The current CCID source
and Linux results do not isolate the failure. Obtain USBPcap covering the first
`PC_to_RDR_IccPowerOn` and following messages, paired with the Pico CDC log,
exact board ID, and UF2 hash. Keep CCID behavior unchanged until this exchange
is captured. Details are in `.agent/QKB.md`.

# Previous session — static CCID review of Windows MUTE report — 2026-11-03 13:54 UTC

Reviewed the CCID descriptor against USB-IF Rev 1.1. Advertised one slot,
T=1, short APDU, and `dwFeatures=0x00020042` (automatic ATR parameter
configuration, automatic parameter negotiation, short APDU); ATR `3B800181`
also identifies T=1. The PowerOn handler emits DataBlock with the ATR. There
is no static mismatch explaining Windows `PRESENT | MUTE`. A concrete
unproven risk is the default response for unsupported CCID messages: it always
returns `RDR_to_PC_SlotStatus`, which may be wrong if Windows sends
`PC_to_RDR_GetParameters`. Do not modify CCID behavior without USBPcap evidence.
Next capture first PowerOn and any subsequent requests plus same-run CDC serial
log, exact Pico ID and UF2 hash. 2BED is currently reachable and passed the
post-flash probe; 8E disappeared after its verified flash. Details/sources are
in `.agent/QKB.md`.

# Current session — physical failover after 8E re-enumeration failure — 2026-11-03 13:51 UTC

Flashed current ARM `max_crypto` image SHA-256
`13f39ce7139297e423c63bf3b6ecd15386c2e550edd932e23efb612df082d863` to 8E
and 2BED after ARM/RISC-V builds passed. Both flash verifications passed. 8E's
cycle then failed finite monitoring (`no USB serial device found`) and the
authorized reader refresh showed only 2BED. Per failover instructions, used
2BED: its finite monitor passed `TEST:SUMMARY pass=1 fail=0`, and its
read-only JCAlgTest 1.8.4 power-up probe returned ATR `3B800181` plus successful
SELECT/GETVERSION/JCSystem responses. Logs and SHA details are in QKB and
`progress.md`.

Extended `tools/test-sio-cross-aid-physical` and its Java probe with an
install-only same-AID reinstall phase and checks for stale-proxy rejection and
fresh lookup. `bash -n` and `./tools/env javac` passed. The hardware case was
not run: 2BED already has the service/client fixture package AIDs, so the
wrapper correctly refuses to replace them; 8E is currently absent. Next run
the wrapper on a re-enumerated board where those fixture AIDs are absent. Do
not remove existing packages. 2BED's applets remain intact.

# Current session — cross-check Windows MUTE report on Linux readers — 2026-11-03 13:44 UTC

Refreshed authorized Linux PC/SC access and found Pico 2
`8E464F4E2BBC0A19` and `2BED26ED20556535`. The read-only power-up probe passed
on both: ATR `3B800181`, JCAlgTest 1.8.4 SELECT/GETVERSION, and JCSystem info
all returned `9000`. Logs are
`artifacts/logs/pico-powerup-persistence-20261103T134305Z.log` and
`artifacts/logs/pico-powerup-persistence-20261103T134321Z.log`. This is Linux
evidence only; Windows board identity and flashed UF2 hash are unknown. Continue
without CCID changes until Windows USBPcap and same-run serial capture show the
power-on command and ATR response. The same-AID SIO proxy lifecycle fix remains
committed as `0c69bdb` (progress metadata `b72ef52`).

# Current session — invalidate stale SIO proxies across DELETE/reinstall — 2026-11-03 13:40 UTC

Fixed a real stale-owner lifecycle defect. `Context::forgetAppletInstance()`
now invalidates proxy routes matching the deleted server AID in every cached
applet instance, including client contexts that do not own the server. The
cross-AID VPCD regression now sends GP INSTALL from the ISD on a fresh SCP03
channel, verifies the reinstalled service is selectable, confirms its old
proxy fails closed (`6A0A9000`), and confirms a fresh lookup succeeds
(`45679000`). Summary: `TEST:SUMMARY pass=25 fail=0`.

Validation passed: `./tools/build-pc`, `./tools/build arm`,
`./tools/build riscv`, and the elevated loopback-only
`SIO_CROSS_AID_REUSE_FIXTURE=1 python3 tools/tests/sio_cross_aid_vpcd_test.py`.
No physical token was used. A first failing run sent INSTALL while the client
applet was selected; VPCD correctly routed it to the client, which returned
`6A04 9000`. The test now selects ISD and reauthenticates SCP03 before INSTALL.
Next: continue auditing foreign-reference consumers and other stale-owner
lifecycle cases; separately the Windows `certutil -scinfo` MUTE report still
needs a paired USBPcap and Pico serial trace. Preserve existing uncommitted
user changes, especially `src/cap_load.cpp`, `choupi/src/heap.*`, and the
physical JCSystem probe files.

# Current session — prove SIO equal-handle owner collision — 2026-11-03 13:20 UTC

Closed CTX-01's deterministic equal-compact-handle integration gap. Added
opt-in PC-only trace output in `Stack::referencesEqual` and made the VPCD
cross-AID test require the trace. The unchanged fixture produced handle
`0002` for both sides, with owners 58 and 57 (`current_package=57`); the
comparison remained unequal. ARM and RISC-V builds, PC build and the VPCD
cross-AID suite passed (`TEST:SUMMARY pass=20 fail=0`). The fixture build emits
the existing converter warning that each package requires int support; it
completed with zero conversion errors. No firmware was flashed because the
diagnostic is compiled only for PC builds. Log:
`artifacts/logs/sio-cross-aid-vpcd-20261103T131843Z.log` SHA-256
`036b857c46f57bd0170880d63379b2ba7175f302ab7e24b9c39b3c877a8c2dd2`.

Next CTX-01 work: stale-owner lifecycle after applet deletion/reinstallation
and continue auditing all foreign-reference consumers/retention paths. The six
JCAlgTest 1.8.4 static `CANT_BE_MEASURED` cases are separately open under
API-03. Windows still needs a paired USBPcap and Pico serial trace for the
reported `PRESENT | MUTE` state.

# Current session — JCAlgTest 1.8.4 static sweep and Windows MUTE report — 2026-11-03 13:08 UTC

The unchanged upstream JCAlgTest 1.8.4 applet/client completed
`ALG_PERFORMANCE_STATIC` on Pico 2 `8E464F4E2BBC0A19`. Client-reported duration
was 1,976 seconds; 1,847 `NO_SUCH_ALGORITHM` outcomes are expected, and six
`CANT_BE_MEASURED` cases remain to isolate (DES clearKey at 64/128/192 bits,
EEPROM array copy, RAM-to-EEPROM array copy, and SWAlgs AES). No reconnects or
manual resets occurred. The exact-reader post-run probe passed ATR, SELECT,
GETVERSION `1.8.4_jc305`, and JCSystem info. Results/logs:
`artifacts/results/jcalgtest-184-static-8e-20261103/`; client log SHA-256
`88372e0723cc91a3b9d928c8f8016c18702aeec095e51be199beec44b24204ce`, CSV SHA-256
`c269c460b2822d8a8bd9184113081283e27ebd456bc38d5852b5ee799c7762a3`.

Windows `certutil -scinfo` reports `PRESENT | MUTE` after several minutes.
Linux on this board returns ATR `3B800181` and successfully runs the applet, so
Windows has not reached a working card session; the exact host/CCID failure is
not yet known. Next, request/capture a Windows USBPcap trace through the first
`PC_to_RDR_IccPowerOn`, paired with the Pico serial log and flashed UF2 hash.
Do not change CCID/ATR behavior without this evidence. Separately isolate the
six static-performance failures using exact upstream tests. Broader next
implementation remains CTX-01 persistent foreign-reference identity.

## Current session — recheck installed JCAlgTest after firmware flash — 2026-11-03 12:32 UTC

After the current ARM image was flashed to Pico 2 `8E464F4E2BBC0A19`, the
read-only `tools/test-pico-powerup` probe passed against the existing upstream
JCAlgTest 1.8.4 applet. PC/SC saw ATR `3B800181`; exact-reader GPPro listing
retained the ISD and three installed packages/applets. SELECT, GETVERSION
(`1.8.4_jc305`), and `JCSystem` info all returned `9000`. Log:
`artifacts/logs/pico-powerup-persistence-20261103T123155Z.log` (SHA-256
`54fc6a4c7ffe003d98e80c09d3712d8521263c684f61883487c2d8062ef16232`). This is
a warm reader check and does not establish cold-power persistence. Continue
CTX-01 persistent foreign-reference encoding; preserve the applets.

## Current session — physical foreign-reference retention check — 2026-11-03 12:28 UTC

Flashed and verified current ARM image on Pico 2 `8E464F4E2BBC0A19`; finite
serial monitor passed `TEST:PASS choupi_runtime` and
`TEST:SUMMARY pass=1 fail=0`. A retained `WATCHDOG_RECOVERY cause=other`
startup marker was also printed; QKB records this existing marker as retained
reset context, not proof of a current watchdog failure.

The physical three-package SIO suite passed on the exact reader. The new
ordinary foreign-reference retention APDU returned `6B439000` (volatile
transient Object[] retention and identity pass; static/instance field stores
remain denied). Existing cross-AID, reset, stale proxy, exception, and firewall
checks passed; GPPro deleted all temporary fixture packages. Log:
`artifacts/logs/sio-cross-aid-physical-20261103T122634Z.log` (SHA-256
`5fb0036a5b871c8ebad9a2e3e6ef431e9a334c9a96f4bc744f5b21b96e52bda4`). The
cycle log is `artifacts/logs/cycle-choupi-20261103T122420Z.log`; flashed UF2
SHA-256 `d1d3f5d9a54368905644cbbbe45da8c781b1a9d2ddf083cf00a8fb5b538be8cd`.

Continue with owner-preserving static/instance field and persistent-array
storage. No other user packages were changed.

## Current session — retain foreign refs in volatile Object[] — 2026-11-03 12:17 UTC

Verified against the official Java Card RE 3.0.5 §6.2.8 that ordinary
foreign applet references may be stored in static fields, fields of an
active-context-owned object, and accessible reference arrays. Only temporary
JCRE entry-point objects and global-array references are prohibited store
values. This corrects the earlier over-broad TODO wording.

Added per-slot owner metadata for applet-owned reference arrays and allowed
foreign references in volatile wildcard `Object[]` arrays. The Descriptor-free
VPCD cross-AID test passed 20/20. Result `6B43` proves transient array
store/read and owner-preserving identity work; static and instance field
stores remain denied. ARM and RISC-V wrappers and `./tools/build-pc` passed.
Descriptor-included and Descriptor-free VPCD runs both pass 20/20. Logs:
`artifacts/logs/sio-cross-aid-vpcd-20261103T121552Z.log` (Descriptor-free) and
`artifacts/logs/sio-cross-aid-vpcd-20261103T121825Z.log` (Descriptor included).
`./tools/test-rmi-registry` also passes with `global_object_array_owner=1` and
the stack owner/type checks enabled.

Persistent field/array storage still needs a stable cross-package reference
identity encoding, owner-aware restoration and GC roots. Typed foreign-array
component checks also need implementation. No physical hardware operation was
attempted.

## Current session — enforce global-array non-retention rule — 2026-11-03 11:57 UTC

Confirmed the old `RESTORE_READ_FAIL` from storing a global transient array in
a static field came from a spec-forbidden operation. Java Card 3.0.5 explicitly
forbids retaining a global-array reference in class fields, instance fields,
or array components. The current VM has guards in static storage, instance
field storage, and `JC_Array::setReferenceEntry`; no persistence format should
be added for this reference.

Extended the cross-AID fixture to attempt all three invalid stores and catch
`SecurityException`, then pass the same global Object[] through the permitted
SIO call. VPCD passed 20/20 with Descriptor omitted and included. Physical Pico
2 `8E464F4E2BBC0A19` returned `6B339000` for all three retention denials plus
the cross-context identity/firewall check, and passed reset, stale-proxy,
DELETE, and cleanup checks. Log:
`artifacts/logs/sio-cross-aid-physical-20261103T115608Z.log`.

Next: continue CTX-01 lifecycle and native-reference audit; the deterministic
equal-compact-handle integration proof remains open. Do not treat the
global-array retention rule as evidence that the full firewall is complete.

## Current session — cross-owner SIO reference identity regression — 2026-11-03 11:47 UTC

Extended the existing three-package SIO fixture. The service now compares a
client-owned applet object read from global `Object[]` against its own local
`this` before virtual dispatch; the expected behavior is unequal, followed by
a caught `SecurityException` when trying to invoke `equals` on the foreign
object. This covers cross-owner identity separately from comparisons against
null or the same foreign reference.

The VPCD suite passed 20/20 both with Descriptor omitted and included. Physical
Pico 2 `8E464F4E2BBC0A19` passed GPPro LOAD/INSTALL, SELECT, the global Object[]
case (`6B319000`), reset/stale-proxy and exception checks, GP DELETE/stale-proxy
checks, and cleanup. Log:
`artifacts/logs/sio-cross-aid-physical-20261103T114515Z.log`. The first VPCD
attempt lacked loopback permission and failed `EPERM`; the approved elevated
rerun passed. The first generic serial monitor selection also refused to guess
between two CDC devices; an exact chip-ID selection worked.

Limit: these integration runs do not log the two compact reference values, so
they do not prove that this particular client/service object pair collided in
the compact handle. A deterministic equal-handle cross-owner integration case
remains open. Continue CTX-01 audit after this regression.

## Current session — Windows MUTE Linux cross-check — 2026-11-03 11:35 UTC

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

A paired repeat on `8E464F4E2BBC0A19` succeeded: exact-reader GPPro `--list`
completed while `./tools/monitor --serial 8E464F4E2BBC0A19 --seconds 8`
captured ongoing CCID traffic, successful APDU responses (`9000`), and
`TEST:PASS choupi_runtime`. The first monitor invocation without `--serial`
refused to guess because two CDC devices were present; this is selection
behavior, not a firmware failure. These command outputs were not archived.

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
`220d37e`; fixture regression follow-up `1dcfd3d`. ARM firmware ends at `0x100d06a8`,
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

Committed cross-context private-reference owner enforcement as `352f2ad`.
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
  `1e38a43`). Replaced the root command-heavy readme with `README.md`, moved
  its physical workflow into `docs/`, and archived the superseded STATUS
  snapshot (commit `59f6912`). The active backlog remains root `TODO.md`.
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
cross. Implementation commit: `aa0df70`. Preserve unrelated edits in `src/cap_load.cpp`, heap,
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
resumed. Artifact refresh commit: `f5877b9`.

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
coverage. TODO and QKB are updated; selective implementation commit is `9bd4c26`. Preserve unrelated edits in AGENTS.md, cap_load/import
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
Source/test commit: `1500003`; append-only progress and this handoff update are
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
committed as `c366c72`; the progress SHA reference is in the metadata commit.

## CTX-03 second-board transient replication — 2026-11-02 14:56 UTC

Repeated the same physical same-CAP transient test on Pico 2
`2BED26ED20556535` after it passed on `8E464F4E2BBC0A19`. Results matched
exactly: set arrays `5A6B9000`, sibling deselect `006B9000`, PC/SC reset
`00009000`; GPPro cleanup passed. Log SHA-256:
`b340ba2d91faba8ee42ad49df14b0929221c61731b897c3439941cb5ecd83be5`.
This comparison is committed as `2e7181c`; the progress SHA reference is in
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
Harness and record are committed as `6fe7513`; the progress SHA reference is
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
`c3f49dc`; the progress SHA reference is recorded in the follow-up metadata
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
The test script/probe changes were committed as `4090cf8`. Remaining physical
same-CAP gaps: reset/transient, GC and cold-power persistence.

## Same-CAP package DELETE/LOAD/INSTALL — 2026-11-02 14:46 UTC

Extended the physical harness to delete both instances and the package, verify
the package is absent, LOAD the CAP again, and CREATE both siblings. On Pico 2
`8E464F4E2BBC0A19`, sibling SIO identity again returned `019000`, and initial
package static state returned `000012340056123456789000`. Cleanup passed. Log
SHA-256: `76a13511ff8b972d0b46e52d485be7c12b02d6878a5b2597a88aa1ca92957e7b`.
This harness change was committed as `2dbb16b`. Remaining physical same-CAP
gaps: reset/transient, GC and cold-power persistence.
Harness and progress record are committed as `2dbb16b`; the progress SHA
reference is in the metadata commit.
Harness and progress record are committed as `4090cf8`; the progress SHA
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
Implementation commits: `4fa2ec4` (owner retention), `7dde071` (reclamation
regression). Progress metadata commit: `23eaa5e`.
Latest authorized USB refresh found two runtime CCID devices at USB nodes
`/dev/bus/usb/001/003` and `/dev/bus/usb/001/004`; no BOOTSEL device, so no
physical flash was attempted. Preserve unrelated worktree changes, especially
the user's `releaseGlobalAIDObject` removal in `heap.cpp/.hpp`.

The focused `tools/test-same-cap-transients` VPCD regression passes 5/5: a
same-package sibling switch clears `CLEAR_ON_DESELECT` but retains
`CLEAR_ON_RESET`, and card reset clears both. Log:
`artifacts/logs/same-cap-transient-vpcd-20261102T120128Z.log`. Test-only CAP and
wrappers are committed in `f3dc7d3`.

`tools/test-same-cap-restart` now passes 7/7. After clean emulator shutdown and
restart, both same-CAP applet registrations restore, the reader sees the same
static scalar and array content, SIO resolves the restored writer array by
identity, and subsequent writer changes remain visible to the reader. Log:
`artifacts/logs/same-cap-restart-vpcd-20261102T121015Z.log`. The wrapper and
test are committed in `0524531`; this is process-restart VPCD evidence, not
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
progress record are committed in `42ed41c`; the storage exhaustion test is
committed in `e7b46b4`.

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
physical API, and USB-enumeration evidence in commit `5b7f7fb`.

ARCH-09 production progress/dispatch coverage is implemented in the latest
worktree. `src/ccid_device.cpp` calls shared production helpers in
`src/ccid_progress_flow.hpp` for pending-message dispatch, guarded USB progress,
time-extension waits, and final-response ordering. `tools/test-ccid-progress-flow`
passes 9/9 focused host checks; PC, ARM, and RISC-V builds pass. Implementation
commit:
`9302f15` (`Exercise production CCID progress flow`); `progress.md` records the
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

ARCH-07 is implemented and validated in commits `6a4030f` and `c51dbe0`.
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

The focused repro documentation is in commit `9419f20`; the preceding v4
capacity rerun is recorded in `6155e9a` and linked in `234c08a`.

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

Documentation/progress commit: `6155e9a` (`Record v4 API capacity rerun`).

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
`4f836bd` (`Verify native progress path on Pico 2`).

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
Root validation/tracker commit: `221b1b3` (`Verify Pico startup and registry
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
Root physical install/rollback commit: `f0ec574` (`Verify physical GPPro install
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
not run. Root documentation commit: `323ab79` (`Document PC emulator
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
backends`); root integration commit: `2fe8396` (`Align filesystem status codes
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
Pico`); root integration commit: `5adcdfe` (`Align OS filesystem tag capacity
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
Root implementation commit: `12f3770`; OS test commit: `c3ccaa5`.

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
rollback validation is open. Implementation commit: `fe6b8d9` (`Move CAP static
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
open in `TODO.md`. Implementation commit: `0283efa` (`Extract Pico filesystem
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
`8f3d2ff` (`Guard native progress callback reentry`).

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
header. Root implementation commit: `67ad1b0` (`Centralize VM and transport
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
reads`). Root implementation commit: `df6dcff` (`Specify filesystem ABI and
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
Test code commit: `2aaa1ae`.

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
and tests are committed as `4258113`.

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
Commit `6bd3569` expands this regression across all three getters. The PC
`max_crypto` build and ARM/RISC-V `max_crypto` wrapper builds pass. No physical
test was possible: the approved PC/SC helper granted access to two USB nodes,
but bounded `pcsc_scan -r` found no reader. The bug fix and regression are in
commit `3ef1a0e`; the implementation and validation record are in
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
`artifacts/logs/cycle-choupi-20261031T022105Z.log`. Commit `d5f62a8` contains
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
`40312f2`, and its 26 cases plus the 250,000-input parser fuzz run pass.
The related JC array test now supplies `JC_Instance::hasLoadedFields()` and
uses `0x3ffe` for an invalid local handle; `0x7ffe` is in the global-handle
namespace. `./tools/test-jcarray` passes all 27 cases; commit `270b409`.

The full current-source JCSystem transaction CAP run passed 85/85 VPCD checks
in 87.427 seconds, including restart restore, API/exception behavior,
`lookupAID()`, lifecycle callbacks, and GC pressure. Evidence is in
`artifacts/logs/jcsystem-transaction-apdu-vpcd-20261031T015450Z.log` and
validation commit `2d6947d`. Physical PC/SC remains unavailable.

Refreshed the downloadable max crypto ARM image from the latest passing
`./tools/build arm --profile max_crypto` output after the APDU token-map fix.
Artifact: `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card.uf2`,
2,086,400 bytes, SHA-256
`c739e78a21763724596ffb81878447b69038f67f1ff63d83a7d548d0b5c980e9`.
The file compares byte-for-byte with `build/arm/pico2_choupi.uf2`; the expected
support CSV hash is unchanged. Commit `e387fcc`. No physical flash was done.

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
`005bf4e`; the global EPO implementation and validations are committed at
`e36b613`. Next, isolate the APDU incoming-length mismatch, then continue the
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
passes on hardware. Profile declaration commit: `9fd1469`; its progress-SHA
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
Regression record commit: `deb1ff9`.

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
Implementation commit: `33cea90`.

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
Implementation commit: `3498569`.

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
Recovery evidence record commit: `21f975d`.

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
Implementation commit: `5ac9c8a`.

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
No physical reader was registered, so this was not a board test. Implementation is committed as `30ab432`; preserve unrelated dirty changes, especially `src/cap_load.cpp`.

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
Implementation is committed as `996d17f`; preserve unrelated worktree
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
record are committed as `a38c64a`, separately from unrelated dirty files.
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
The refreshed artifact and progress record are committed as `1025e7b`.
The CAP 2.2 public method-token checks, converter fixture regressions, and
refreshed current-source UF2 are committed as `ad2ba38`.

Current-source PC VPCD vector reruns also pass: `symm_crypto` 73/73 crypto
vectors (75 checks), `asymm_crypto` 97/97 (99 checks), and
`symm_crypto_ctr` 8/8 CTR vectors (10 checks). The wrappers build focused
profiles, not one combined `max_crypto` run. Hashes and logs are in QKB. These
are emulator checks; no Pico was visible through PC/SC for physical validation.
The evidence and progress record are committed as `35b0e5e`.

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
`e028cca`.

## Fresh VPCD multipackage lifecycle — 2026-10-30

After rebuilding the PC JCVM for `base`, `PICO2_HOST_USB_ACCESS=1
./tools/test-vpcd-multipackage` passed 25/25 checks: two packages and three
instances installed, GET STATUS and SELECT passed before/after process restart,
all three owner roots were inventoried, and delete/cascade-delete left an empty
registry. This exercises the current worktree, including the pre-existing
uncommitted `src/cap_load.cpp` changes; those edits were not committed here.
This is VPCD evidence only. Hardware enumeration and cold power-cycle recovery
remain to be checked separately. Log hashes and paths are in `.agent/QKB.md`.
The documentation record is committed as `8cce24a`.

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
The documentation and result record are committed as `9c13084`.

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
The AES-CTR source/profile step is committed as `8b0dbd9`; the max image/profile
step is committed as `7eac820`. See the latest progress record for the full
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
Documentation and evidence summary commit: `37d13fb`.

The supplied JCOP4 P71 symmetric matrix audit is complete: all 41 target
positive identities match the generated `symm_crypto` profile and the physical
2BED report; existing symmetric KAT coverage is 71/71. See QKB for the exact
comparison log/hash. Do not infer AES-CTR support from this matrix. Continue
with remaining backlog; persistent-record deletion remains disabled until
physical USB power-cycle recovery is validated.
Parity audit documentation commit: `a907deb`.

The `asymm_crypto` profile audit also matches all 136 shared JCOP4 target
identities. Its current generated set has 145 entries; the physical 1.8.2
report is 143/145 because the unchanged upstream CAP sends RSA/RSA-CRT 4096 on
two rows labeled 3072, while direct 3072 generation vectors pass. Regenerate
the expected set from current config for comparisons; the cached external-test
CSV is stale. See QKB for exact logs. API `/deploy` result corrections and
private-key hardening remain open.
Asymmetric profile audit documentation commit: `5d0e457`.

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
  snapshots (`9206494`, `2e2d014`). Focused EC errors and P-256 copies pass
  VPCD; physical retest pending.
- Signature digest continuation/state layout (`c28dec9`, `d35f0d2`) and
  malformed precomputed-hash reason (`126b152`) fixed. Upstream initial-digest
  group: 10 OK, eight expected unsupported, 448 filtered of 466. RSA and
  ECDSA SHA-1/256 continuation/precomputed cases pass VPCD. SHA-224/384/512
  continuation/precomputed coverage remains open: attempted upstream filters
  selected no such cases. Artifact directory:
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/`.
- MessageRecovery mode/parameter reasons fixed (`b29041a`, `231332a`);
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
  (`6485c87`, `14a2406`, `41d45b7`, `01a1ef1`, `220f4e9`); final-size reserves
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
  `90f3f72`; `tools/tests/rp2350_fs_overlay/rp2350_fs_overlay_test.cpp` has an
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
- Follow-up to `90f3f72`: a high-offset v3 interrupted-snapshot
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

## CTO implementation review — 2026-11-03 19:55 UTC

Completed the requested leadership-level retrospective in
`docs/CTO_IMPLEMENTATION_REVIEW.md`: approximately 3,650 words, eight numbered
page sections with seven explicit print page breaks. Covers original scope,
implemented architecture and deviations, dated delivery phases, runtime and
context correctness, persistence/capacity, crypto/transport, operating lessons,
and prioritized unfinished acceptance. Distinguishes historical physical passes,
latest emulator evidence, and current-image gaps; incorporates successful Windows
GPPro evidence and the completed 466-call Crypto VPCD sweep. All document links
and anchors resolve; the latest Crypto CSV counts were independently recounted.
Documentation only: no build, firmware, CAP, flash, USB or PC/SC changes/tests.
Pre-existing source changes, documentation moves and untracked artifacts were
preserved. Continue the active implementation using TODO task owners; this review
does not close or reprioritize their acceptance gates.

## Functional implementation review, first iteration — 2026-11-03 21:17 UTC

User requested a thorough current-code review and then explicitly requested
continued iteration and checks for similar problems elsewhere. The goal remains
active. Initial findings are in `docs/review_20261103.md`; 28 actionable
`RV-20261103-*` checklists were inserted in the existing root `TODO.md` under
existing task owners. Offline source/probe evidence is archived under
`artifacts/reviews/review_20261103/`, including a 143-file source hash manifest.
No firmware, CAP or existing test source changed. All pre-existing dirty files
and documentation moves remain untouched.

Validation: existing parser/overlay/transaction/inventory/RMI/APDU/GP/progress
checks pass. VPCD protocol passes 18/18 with elevated local socket access after
restricted send() EPERM. Existing VPCD session suite fails compilation; a copy
with only constructor compatibility adapted fails two semantic assertions.
The new review runner deliberately reports 10 failing contract cases / 2 pass;
its UBSan mode reports 2 failing diagnostic cases. These are outstanding defects.
No full target build, physical flash, USB/PCSC or cold-power test was performed.

Continue review, not runtime fixes: revisit persistent array alias replacement
in instance/object/reference-array paths, before-image bypass in delete/cleanup,
transaction caches outside statics, reset state in all native crypto objects,
width overflow in serialization/RMI and arithmetic helpers, and the CAP/native
opcode trust boundary. Append verified related findings and tighten any claims
whose integration trigger is not established. Preserve the first iteration's
source hashes/logs when adding later evidence.


## Same-AID CAP lifecycle review — 2026-11-05 09:05 CET

User requested analysis only. Findings are in `docs/cap-reinstall-review-20261105.md`; no firmware/CAP edits. Instance DELETE retains package code, duplicate package LOAD returns 6A80, and normal full-package replacement erases and verifies new code. Two un-reproduced cleanup risks remain: package DELETE skips cross-package SIO proxy invalidation; a post-persistent-commit DELETE storage failure skips runtime invalidation while later lifecycle recovery may finish durable cleanup. Existing same-CAP tests reload identical bytes. Next: reproduce package-cascade stale proxy and late-DELETE-failure cases, then add changed-version behavior coverage. Preserve all pre-existing dirty files. GP DELETE unit tests passed 26/26. Overlay suite exited 0 with summary fail=0 but emits an injected migration FAULT; do not claim fault-free hardware validation. No USB or hardware operations occurred.


## CAP reinstall review backlog — 2026-11-05 09:26 CET

Added open tasks CAP-REINSTALL-01/02/03 under GP-01 in root TODO.md at user request: package-cascade SIO proxy invalidation, runtime cleanup after late DELETE failure, and changed-version replacement with restart/hardware acceptance. The first two remain source-review risks requiring reproduction; no runtime fix or validation is claimed. Documentation only; preserve existing worktree changes.
# VPCD startup cleanup fixed; later cross-AID install still failing — 2026-11-05

The VPCD pre-connect stall was caused by PC orphan recovery erasing 256
potential static-field prefixes for every empty dynamic package slot. Each
prefix erase rescanned the host tag store. Replaced that with one package
static namespace erase per slot and restricted the two-byte prefix operation
to trusted Installer/RuntimeEnvironment contexts. The cause and fix are in
`.agent/QKB.md`.

Validation: `./tools/build-pc --profile symm_crypto` passes. The bounded
cross-AID harness reaches `VPCD:CONNECTED`; its Descriptor-free run fails on
the second fixture's INSTALL with `6A80`, exception 29, the previously
documented fail-closed limitation. With
`SIO_CROSS_AID_INCLUDE_DESCRIPTOR=1`, the full suite passed 34/34:
`artifacts/logs/sio-cross-aid-vpcd-20261105T130057Z.log`. No emulator process
remained after the run. No physical hardware used. Startup duration was not
measured. Temporary diagnostic prints were removed. Cross-target gate is also
now complete: `./tools/build arm` and `./tools/build riscv` both exited 0 with
the current profile; no board was flashed.

Next: continue with the next high-priority runtime item in `TODO.md`. Preserve
unrelated existing source, documentation, review artifact, and submodule
changes. In particular, `src/cap_load.cpp` still contains pre-existing
uncommitted lifecycle/install refactoring; do not stage it wholesale.

---


## README download links — 2026-11-05

README now links to the latest upstream JCAlgTest CAP/client release and the
latest GPPro gp.jar binary. The user explicitly requested no pinned CAP
release numbers; the install command uses a downloaded JC305 CAP placeholder.
Local links, shell snippet syntax, and whitespace checks passed; no firmware
or hardware work was performed. Resolve this documentation commit with
`git log -1 --format=%H -- README.md`.


## README firmware release link — 2026-11-05

README now names pico2-jcvm-max_crypto-arm__latest.uf2 and links to the
user-supplied project releases page. Existing BOOTSEL, data-loss, and build
guidance remains. This is a documentation-only change for publication to main;
no image or hardware validation is implied.

## CTO 48-hour review delivered — 2026-11-06 05:16 UTC

Documentation-only update: `docs/CTO_PROGRESS_REVIEW_20261106.md` compares
4 November 05:15 UTC–6 November 05:15 UTC with the previous CTO review.
Approximately two pages (1,164 words, one explicit print page break), with
material changes, prioritized missing outcomes and physical/emulator evidence
limits. Links and scoped whitespace checked; no new builds or hardware tests.
Implementation work and priorities remain as in the active handoffs above.
All unrelated pre-existing worktree changes were preserved. Resolve the review
commit with `git log -1 --format=%H -- docs/CTO_PROGRESS_REVIEW_20261106.md`.
# Current handoff — max_crypto Windows image rebuilt; three boards pass runtime monitor — 2026-11-06 12:53 CET

Built ARM and RISC-V max_crypto in normal mode with
`PICO2_DEBUG_SERIAL_WAIT_MS=0`; both repository wrappers passed. The fresh ARM
UF2 is `build/arm/pico2_choupi.uf2` (1,939,456 bytes, SHA-256
`fca53590c3631c8c69f529a8b4a861d63d621c85dab1dfcefb46963fed177654`). Its
firmware end is `0x100ECAD0`, leaving 79,152 bytes in the 1 MiB firmware
window. Packaged the external-test, empty-card image at
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`
(4,036,608 bytes, SHA-256
`2578fa07aa5d6dc92e1eaed947774964bdde40c3eb4f596f8841e387b9058a9c`). It
contains 3,787 firmware blocks plus 4,096 erased storage blocks covering
`0x10100000..0x101FFFFF`. No token was flashed.

On 2026-11-06 around 12:49 CET, the authorized host-access helper granted all
three Pico USB nodes; `lsusb` identified runtime JCVM boards
`2BED26ED20556535`, `8E464F4E2BBC0A19`, and `D92E743D5F640C0F`. Eight-second
finite serial monitors passed on all three. D92 included an observed CCID
APDU response with status `9000`; the other two reported `TEST:PASS
choupi_runtime` and `TEST:SUMMARY pass=1 fail=0`. `tools/doctor` passed. PC/SC
still listed no Pico reader: bounded `pcsc_scan -r` timed out after access was
granted. This is not a successful PC/SC/APDU or applet test.

For Windows testing use the empty-card artifact above. Its hash is the fresh
build to identify during external measurements. HOST-02 remains open for
three cold reconnect timings with no serial client and for exercising the
diagnostic deadline without DTR. See `TODO.md` and
`docs/usb-startup-diagnostics.md`; preserve the current user changes in the
worktree.

The preceding malformed Export-offset test commit is `89d1c8a7`; its appended
progress entry still says `pending` because the commit hash was not copied into
the earlier entry. No earlier progress records were rewritten here.

# Previous active continuation — Descriptor-free method bounds regression revalidated — 2026-11-06 12:05 CET

## Late DELETE failure context invalidation — 2026-11-06 15:25 CET

Reproduced the CAP-REINSTALL-02 risk with PC VPCD failpoints for both package
slot cleanup and instance-registry cleanup. Persistent DELETE commits before
those later cleanups; the resulting APDU status is `6581`. `CapLoadSession`
now exposes the commit state, and both CCID and VPCD invalidate selected and
inactive applet contexts if DELETE committed even when cleanup returns `6581`.
Package context restoration also refuses to restore a dynamic package whose
package record is gone.

Validation: rebuilt PC `max_crypto`, ARM `max_crypto`, and RISC-V `max_crypto`;
all wrappers passed. `./tools/test-rmi-registry` passed. Full cross-AID VPCD
suite passed 34/34. Both injected same-process cases passed: old SIO proxy
blocked after DELETE and same-AID reinstall, fresh lookup returned
`45679000`. Logs are `artifacts/logs/sio-cross-aid-vpcd-20261106T142352Z.log`
(package cleanup, SHA-256
`0bbed3993975ac642d200a921f3110de1fc9c2c4e40ec21bd1c786a82894bb42`) and
`artifacts/logs/sio-cross-aid-vpcd-20261106T142510Z.log` (instance cleanup,
SHA-256 `c06f3b1b1625496e721ac6da1608e77729ae4054ba8ee991d9b527a567d3efbf`).
No physical test was run; PC/SC reader registration is still absent. Restart
after each injected cleanup failure remains untested. Preserve pre-existing
unrelated worktree changes. Next: extend the injected-failure VPCD path to
restart before recovery/reinstall, then proceed with changed-version CAP
replacement (CAP-REINSTALL-03).

## CAP-REINSTALL-02 VPCD restart recovery verified — 2026-11-06 16:22 CET

Extended late-DELETE failure tests across emulator restart. Two additional
restart defects are fixed:

- VCR1 v2 sidecar restore skipped the serialized owner-ID byte when a package
  was disabled, corrupting all later package records. It now consumes and
  validates every owner ID before skipping disabled-package restoration.
- A persistent client proxy to a deleted service package could not be restored
  because its saved class metadata was gone, making the client unselectable.
  `JC_Instance` now restores that record as a fieldless stale proxy placeholder;
  dispatch still fails closed.

Package late-cleanup injection: `6581`, restart, client SELECT `9000`, old proxy
`6A0A`, missing service, same-AID package LOAD/INSTALL, fresh lookup
`45679000`; summary pass=1. Instance late-cleanup injection: `6581`, restart,
old proxy blocked, package DELETE `9000`, same-AID reinstall and fresh lookup
`45679000`; summary pass=1. The full cross-AID SIO suite passes 34/34. PC,
ARM, and RISC-V max_crypto builds pass; RMI registry unit passes. Logs and
hashes are in `progress.md` under `gp-delete-late-cleanup-restart-20261106`.
No physical PC/SC operation was run; the latest recorded host state has no Pico
reader in `pcsc_scan -r`.

The runtime invalidation fix is commit `3385f68e09d40fbb655f5560db93bade5230a77f`
(`Invalidate runtime state after committed DELETE failures`). The restart
hardening is commit `a1365c63ad514ba95905edea21a18ba46bc60fb3`
(`Recover deleted package proxies after restart`). Preserve unrelated dirty
worktree changes. Next: implement CAP-REINSTALL-03 changed-version package
replacement. Keep CAP-REINSTALL-02 open only for physical CCID/APDU acceptance.
# Current handoff — changed-version CAP replacement verified in VPCD — 2026-11-06 16:50 CET

## Completed

Implemented CAP-REINSTALL-03 emulator coverage using two test-only CAPs with
identical package/applet/instance AIDs. The V1 load image is 392 bytes
(`e70417f50bb64ea503a70599b0e55da236b8178577d41cbc4932b0f5e4c4cda7`); V2
is 479 bytes (`cc069da48ec7f881ada8c3ccee52326dbbcbcc0a604183bc8df1c045cdcdb01c`).
Both AIDs are package `A000000151000006`, applet/instance
`A00000015100000601`.

`./tools/test-cap-upgrade-vpcd` passes both directions: V1→larger V2 and
V2→shorter V1. Instance-only DELETE returns `9000`; duplicate LOAD of the
resident package returns `6A80` during LOAD data, not at LOAD initialization.
Full package DELETE returns `9000`; LOAD/INSTALL/SELECT all succeed. V2's
new static/instance layout and fresh defaults are observed; state persists
after restart. V2→V1 yields V1 defaults and behavior, then persists its
mutated state after a second restart. Full output:
`artifacts/logs/cap-upgrade-vpcd-run-20261106T154810Z.log`; emulator trace:
`artifacts/logs/cap-upgrade-vpcd-20261106T154927Z.log`.
Implementation commit: `cc00f4a1` (`Test changed-version CAP package replacement`).

The fixture exposed a valid CAP reference-field sentinel incorrectly rejected
at INSTALL. `valid_instance_field_token` now accepts `0xFF` only when the
reference field count is zero. Unit validation passes 38/38. PC, ARM, and
RISC-V `max_crypto` wrapper builds pass. Upgrade procedure and evidence are in
`docs/cap-reinstall-review-20261105.md`; TODO-REINSTALL-03 is complete for
offline emulator acceptance, but remains open for physical acceptance.

## Physical test blocker and next action

Refreshed access with `sudo -n /usr/local/sbin/pico2-pcsc-reader`; it granted
USB nodes `001/040`, `001/053`, and `001/049`. `timeout 8s pcsc_scan -r`
exited 124 without listing any reader. No physical APDU or flash operation
was attempted. When a Pico reader appears, flash the rebuilt ARM image only
after refreshing device identity, then repeat the changed-version GP DELETE,
LOAD, INSTALL, SELECT and restart checks on hardware. Continue offline while
PC/SC has no Pico reader.

# Previous current handoff — package DELETE invalidates cross-AID SIO routes — 2026-11-06 14:22 CET
# Current handoff — independent client survives package DELETE recovery — 2026-11-06 17:08 CET

## Completed

Closed CAP-REINSTALL-01's offline unrelated-client gap. Added a test-only
third package (AID `A000000151000007`) with an independent applet; SELECT plus
INS `10` returns `5A9000`. The focused package DELETE/reinstall scenario passes
with a normal `9000` DELETE and with injected late cleanup returning `6581`,
followed by emulator restart and recovery. In both paths the unrelated applet
remains selectable/executable after service deletion and same-AID service
reinstall. The stale proxy still returns `6A0A`, while fresh lookup reaches
the reinstalled service (`45679000`). The default cross-AID suite remains
`TEST:SUMMARY pass=34 fail=0`.
Implementation commit: `f036d20c` (`Test unrelated applet across package DELETE`).

Validation logs: normal opt-in run
`artifacts/logs/sio-cross-aid-independent-client-20261106T160101Z.log`,
late-cleanup/restart run
`artifacts/logs/sio-cross-aid-independent-client-restart-20261106T160123Z.log`,
default suite
`artifacts/logs/sio-cross-aid-default-after-independent-client-20261106T160237Z.log`.
All use localhost VPCD; no USB/PCSC or firmware build was needed.

## Physical blocker

CAP-REINSTALL-01, -02, and -03 remain open only for physical CCID/APDU
acceptance. Latest reader refresh granted three USB nodes but
`pcsc_scan -r` listed no Pico. Continue offline or refresh PC/SC when hardware
is available; do not claim physical validation.

# Previous handoff — changed-version CAP replacement verified in VPCD — 2026-11-06 16:50 CET
# Current handoff — persistent boolean arrays validated — 2026-11-07 12:06 CET

RV-20261106-26 is implemented in `choupi/src/jc_handlers/flashmemory.cpp`.
Persistent array decoding now validates the serialized length, reads ordinary
boolean payloads at the correct offset without a transient clear-event byte,
and allows the secure byte accessor to accept boolean arrays. Production
handler tests cover zero/one/multiple elements, static-reference restoration,
mutation and transaction abort, fresh-heap restore, and malformed records.

`./tools/test-persistent-transaction` and its ASan/UBSan build pass with
`TEST:SUMMARY fail=0`. ARM and RISC-V wrappers pass; RISC-V remains VM-core and
smoke only. ARM image ends at `0x100D5A80`, leaving 173,440 bytes. The suite's
first run exposed three fixture tag collisions, fixed by assigning unique tags.
The final progress entry records the build warnings and sanitizer limitation.
No physical test was performed.

R26 code/tests/TODO/progress are committed as `a811c6f3`; the SHA follow-up
record is commit `cd9796ab`.

The requested fresh max-crypto ARM image is built and packaged for external
testing. Path: `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-cd9796ab.uf2`;
size 4,044,288 bytes; SHA-256
`3a0cdf63a310d369768e372b06dc0ddff3c053a5dc45a1bbe4a2fc3e5d795c02`. It is
RP2350 ARM Secure, profile `max_crypto` (147/150 capabilities enabled), release
0.1.1. All 4,096 storage blocks were verified erased (`0xFF`), with no applet
preinstalled. The firmware end is `0x100ED9E8`, leaving 75,288 bytes. The CPLC
source ID is `FFCD9796` because unrelated edits remain in the worktree. No
physical flash/runtime test was performed. Build provenance:
`build/arm/build-provenance.txt`.

Next: record and commit these image details. Preserve unrelated dirty files.

# Previous current handoff — REL-01 CPLC and max-crypto image — 2026-11-07 11:49 CET
# Current handoff — R24 persistence retry regression verified — 2026-11-07 13:41 CET

# Current handoff — REL-01 offline checks and external image refreshed — 2026-11-07 16:45 CET

REL-01 source implementation is already committed as `971272f3`. Revalidated
release/source-ID stability against the current worktree with two consecutive
`PICO2_HOST_USB_ACCESS=1 ./tools/build arm --profile max_crypto` runs: both
reported CPLC source ID `0xFFBB1B86`, 147/150 profile capabilities, firmware
end `0x100EDE9C`, and 74,084 bytes remaining. The RISC-V max_crypto wrapper
build also passed its VM-core/smoke target. `gp_get_data_test` passes 25/25 and
the flash-boundary check passes. These builds include the current dirty tree;
`FF` marks that state and `BB1B86` identifies the first six characters of
HEAD.

Refreshed the authorized PC/SC access helper; it granted USB nodes
`001/054`, `001/055`, and `001/056`. Bounded `pcsc_scan -r` produced no reader,
so no GPPro query or hardware flash was possible. REL-01's physical display
check remains open.

Fresh empty-card image for external testing:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-0.1.1-FFBB1B86.uf2`,
4,046,848 bytes, SHA-256
`30c7a12912f853c157f79ff36424c3e3ceedf2060eb3bae723b6e33f6d40f8f4`. Picotool
identifies RP2350 ARM Secure; package validation confirms 3,807 firmware blocks
and 4,096 erased storage blocks. Release is `0.1.1` (`0101`). This image
includes current uncommitted worktree changes; do not describe it as a clean
release. `src/ccid_device.cpp` and the JCSystem reset/power test remain dirty
from the separate RV-20261106-36 investigation and are not part of this REL-01
record. No physical flash/runtime test is claimed.

Next: when a Pico CCID reader becomes visible, query CPLC with GPPro `--info`
and verify release `0101` and the source marker. Preserve unrelated dirty files.

# Current handoff — R36 logical power/reset semantics validated offline — 2026-11-07 17:12 CET

Physical CCID `PC_to_RDR_IccPowerOff` now follows the reset cleanup path via
`choupi_vm_clear_selected_applet()` instead of invoking Java `deselect()`. This
keeps CCID aligned with VPCD and clears applet transaction state, both transient
classes, applet/shareable session state, secure-channel state, pending APDU
response, and card-manager transfers without applet bytecode. The VPCD unit
suite passes 29/29. Focused JCSystem APDU case 57 passes power-off/on and reader
reset checks: deselect callback count stays unchanged, select count advances,
and both `CLEAR_ON_RESET` and `CLEAR_ON_DESELECT` arrays clear after each
boundary. Focused log:
`artifacts/logs/jcsystem-reset-power-focused-20261107.log`.

`./tools/build-pc --profile max_crypto`, `PICO2_HOST_USB_ACCESS=1 ./tools/build
arm --profile max_crypto`, and the corresponding RISC-V wrapper pass. The
RISC-V wrapper remains VM-core/smoke only. No board was flashed. The authorized
PC/SC helper again granted nodes `001/054`, `001/055`, `001/056`, but bounded
`pcsc_scan -r` published no readers, so physical CCID behavior and actual
supply loss remain unverified.

A light-install full JCSystem run is not valid for the capacity assertion at
case 83 (it substitutes a one-byte array). The normal-parameter full run
exceeded a 240-second external timeout without final test summary; emulator CPU
peaked at about 73% of one core and its log stopped after the exception
trace/write sequence. Logs:
`artifacts/logs/jcsystem-transaction-apdu-vpcd-r36-20261107.log` and
`artifacts/logs/jcsystem-transaction-apdu-vpcd-r36-full-20261107.log`. Do not
call the full JCSystem suite passed; its duration/last-case diagnosis remains
open independently from the focused R36 pass.

Next: when a Pico reader and board in BOOTSEL are actually enumerated, flash
only after confirming both architecture wrappers, then run physical POWER_OFF,
power-on/ATR, reader reset, SELECT/process and persistent deselect-counter
checks. Continue other offline priorities meanwhile. Preserve unrelated
worktree changes.

# Current handoff — REL-01 rebuild stability and external max_crypto image — 2026-11-07 18:01 CET

REL-01 ordinary-build stability is verified: `./tools/build arm --profile
max_crypto` ran twice consecutively and both reported CPLC source ID
`0xFFDCFCC7`. The CPLC/GP GET DATA unit suite passed 25/25 and
`./tools/test-firmware-flash-boundary` passed. ARM and RISC-V max_crypto
wrappers both passed; RISC-V remains the shared VM-core/smoke target only.
The verification record is commit `1d176d71`.

Fresh empty-card external-test image:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`, size
4,047,360 bytes, SHA-256
`9c40b757b67eb8dbada757edcf4355fd43e7c917f93a65b52e70a17bf6941505`.
Picotool identifies RP2350 ARM Secure, binary end `0x100EDFC0` (73,792 bytes
remain). Package validation matched all 3,808 firmware payloads against the
ARM build and confirmed 4,096 erased storage blocks (`0x10100000–0x101FFFFF`),
so the card has no installed applets or persistent data. Build source ID is
`FFDCFCC7`; profile is max_crypto (147/150); release is 0.1.1 (`0101`).

Physical GPPro acceptance remains open. Host helper granted USB access to
nodes `001/054`, `001/055`, and `001/056`, but `lsusb` reported all three as
`2e8a:0009` runtime CCID devices and `pcsc_scan -r` did not list a reader. No
physical query or flash was attempted. `.agent/EXT_REQUESTS.md` records the
Windows GPPro CPLC check needed to close REL-01. Preserve the user's unrelated
dirty worktree; the UF2 is ignored by Git.

Next: continue with the highest-priority unblocked TODO item after recording
this REL-01 partial acceptance. Do not mark REL-01 complete until GPPro reads
`OperatingSystemReleaseLevel=0101` and `ICPersonalizationEquipmentID=FFDCFCC7`
from a physical reader.

---

## Architecture review completed — 2026-11-07 21:10 CET

Documentation-only ARCH-REVIEW-20261107 replaces docs/FIRMWARE_JCVM_ARCHITECTURE.md
with the requested concise review. Exact prior report is in docs/backups/.
Source references, section/word limits and backup identity passed inspection.
No build, flash, USB, or runtime acceptance was performed. Existing technical
handoff priorities and all unrelated uncommitted changes remain applicable.
# Current handoff — persistent resource accounting diagnostics — 2026-11-08 08:05 CET
# Current handoff — key resource accumulation confirmed; full run still fails — 2026-11-08 11:13 CET

# Current handoff — persistent-root prefix regression; RESOURCE-01 open — 2026-11-08 14:27 CET

A focused safety fix now protects applet-instance root records when reachable
persistent objects have longer backing tags. Example: root `03043A00FF` must be
recognized as the prefix for a reachable backing record such as
`03043A00FF0000`. This corrected a reclaimer safety gap seen during temporary
VPCD testing. `./tools/test-persistent-transaction` passes with the new
`reachable_applet_root_prefix_protects_nested_backing_record` regression.

Temporary per-APDU collector diagnostics are removed. The experiment after the
prefix fix still failed transient cross-AID lifecycle with `6999` while logging
zero erasures for every APDU. The rebuilt-source normal VPCD multipackage suite
passes 47/47, including reset lookup and transient lifecycle. This means the
collector invocation itself can disrupt lifecycle state, and it is not ready
for production. Do not enable persistent reclamation or report object deletion
support.

RESOURCE-01 remains open: unchanged JCAlgTest 1.8.4 VPCD run failed on RSA-768
with `F205 / SystemException_NO_RESOURCE` after 442,532 ms and persistent
OutOfFlash (`PERSIST_EXCEPTION id=20`). This source fix only closes one
reclaimer root-coverage hole; it does not fix the long-run exhaustion. PC, ARM
max_crypto and RISC-V max_crypto builds pass. No device was accessed or flashed;
PacketRF remains excluded.

Next: investigate why invoking reachability collection changes transient
lifecycle even when no tags are erased; add regression coverage around mark-bit
state and transient global-array cleanup. Then continue resource accounting and
PERSIST-01 gates before retrying any persistent cleanup experiment.

# Current handoff — clarify transient VPCD failure attribution — 2026-11-08 14:52 CET

The targeted one-shot after-case-30 collector experiment did not actually reach
its gate: `artifacts/logs/host-vpcd-multipackage-apdu-20261108T133615Z.log` has
no `PERSISTENT_GC_CASE30` marker. Its `6999` cannot be attributed to the
reclaimer. Temporary source instrumentation was removed and the PC emulator was
rebuilt without it.

A fresh no-experiment VPCD multipackage control passed 47/47, including transient
lifecycle (`artifacts/logs/host-vpcd-multipackage-apdu-20261108T134445Z.log`).
One ordinary non-experimental run also failed transient lifecycle, so that check
is intermittent or test-state dependent. The earlier full per-APDU reclaimer
experiment did fail at the same assertion with zero logged erasures, but its
causality is not established. Keep production persistent reclamation disabled
because the broader PERSIST-01 proof is still incomplete.

RESOURCE-01 remains open: the unchanged 1.8.4 JCAlgTest VPCD run exhausted
persistent storage at RSA-768 (`F205`, 442,532 ms). Root-prefix guard commit is
`80e1f6f7`; unit regression and PC/ARM/RISC-V builds passed. No physical board
was accessed or flashed. PacketRF remains excluded.

Next: improve the transient lifecycle fixture to report which transient value
was lost and capture reset/deselect boundaries; then isolate full reclaimer
root tracing from tag enumeration/deletion. Continue persistent resource
accounting and defer production reclamation until PERSIST-01 gates pass.

# Current handoff — identify experimental cross-AID failure stage — 2026-11-08 15:15 CET

The cross-AID fixture previously allowed failed SELECTs to pass silently, then
reported the next `6999` as transient-state loss. The integration test now
asserts both SELECT responses. Clean normal PC VPCD passes 47/47 with the
stronger assertions (`artifacts/logs/host-vpcd-multipackage-apdu-20261108T135501Z.log`).

With the temporary per-APDU reclaimer enabled, the actual failure is
`transaction probe SELECT during transient switch returned 6A82`; the transient
APDU is never sent. The emulator logged zero persistent erasures. This is an
applet AID lookup failure at the SELECT/restore boundary, not proven transient
corruption. Log: `artifacts/logs/host-vpcd-multipackage-apdu-20261108T140352Z.log`.
The case-specific hook experiment was invalid and must not be used as evidence.

The temporary reclaimer hook has been removed; the PC emulator was rebuilt
without it. RESOURCE-01 remains open at persistent OutOfFlash / RSA-768 `F205`
after 442,532 ms. Root-prefix guard is committed as `80e1f6f7`. No physical
token was accessed or flashed; PacketRF remains excluded.

Next: inspect why `cap_store_find_instance` returns false after Core selection
only under the full collector experiment. Instrument CAP-store instance count
and AID comparison at SELECT restore, and keep SELECT failures separate from
transient payload assertions. Do not enable production reclamation.

# Current handoff — preserve applet root tags in reclaimer inventory — 2026-11-08 15:56 CET

The full temporary per-APDU VPCD reclaimer experiment produced exact evidence:
`PERSISTENT_RECLAIMER:ERASE owner=2 tag=03023900FF`. This is package 57,
class 0, registered applet instance AID `A000000062030101`. On restart, its
instance record count was zero and SELECT returned `6A82`. The earlier heap
prefix guard did not cover this root because the root itself was not loaded as
a reachable heap object.

`PersistentInventory::visitUnreachableTag` now excludes valid applet-instance
root tags directly. A focused regression confirms a root remains present while
an orphan record for the same owner is reclaimed. `./tools/test-persistent-transaction`
passes (`TEST:SUMMARY fail=0`), and `./tools/build arm` and `./tools/build riscv`
pass. No physical board access or flash was performed. The temporary VPCD
collector and tracing hooks have been removed; production persistent
reclamation remains disabled.

RESOURCE-01 is still unresolved: full JCAlgTest 1.8.4 VPCD previously failed
at RSA-768 with `F205` / persistent OutOfFlash after 442,532 ms. This step
closes a collector safety defect only. Continue resource accounting and PERSIST-01
multi-owner/transaction/interruption safety gates. Keep PacketRF excluded.

Artifacts: `artifacts/logs/host-vpcd-multipackage-restart-20261108T144059Z.log`.
Code/docs commit is pending at handoff creation.

# Current handoff — RESOURCE-01 collector experiment retested after root fix — 2026-11-08 18:45 CET

The registered applet-instance root guard is committed as `60148e50`. The
temporary PC-only reclaimer/request/trace hooks used for diagnostics have now
been removed, and `./tools/build-pc --profile max_crypto` passes on the clean
runtime path. No physical reader was accessed or flashed; the unexpected
PacketRF device remains excluded.

Evidence update: the request-gated experimental full JCAlgTest 1.8.4/jc305
run at `artifacts/results/jcalgtest-support-vpcd-20261108T162853Z/` made 187
successful collector calls and erased 1,580 tags. It reached RSA CRT private
4096 without `F205` or `PERSIST_EXCEPTION`, then hit the wrapper's 3,600-second
limit during KeyAgreement and failed with an empty APDU. It is partial VPCD
evidence, not full suite acceptance or a production fix. The collector-enabled
multipackage experiment after the root-tag fix passed 47/47; current clean
collector-disabled control also passed 47/47 at
`artifacts/logs/host-vpcd-multipackage-apdu-20261108T173555Z.log`.

Current firmware still reports object deletion unsupported and does not run
persistent reclamation. RESOURCE-01 is not fixed. `./tools/test-persistent-
transaction` passes `TEST:SUMMARY fail=0`. Continue with PERSIST-01 coverage of
all durable roots/aliases, retained applet contexts and cross-owner references,
then transaction/interruption recovery and physical acceptance before wiring
deferred per-context deletion into normal APDU processing. Do not use PacketRF.

# Current handoff — refreshed external max_crypto image — 2026-11-08 19:45 CET

The current ARM max_crypto empty-card artifact was rebuilt from the dirty
checkout and packaged without a CAP/applet:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2` (4,079,616
bytes, SHA-256 `02a879b36038b55c6c67f409969aab28574034b1e624bfcc06726c86f7584b1c`).
The build passed ARM max_crypto and RISC-V max_crypto wrapper validation; the
image parses as RP2350 ARM Secure, has 159/162 capabilities, release 0.1.1,
CPLC marker `FFEAFFDB` based on HEAD `EAFFDB`, and an empty 1 MiB card-storage
region. It has not been flashed or tested on a physical board. An unexpected
connected device was explicitly excluded; no physical-device command was run.
The artifact manifest and progress record were refreshed. Full JCAlgTest
acceptance remains open.

# Current handoff — focused request-latch regression — 2026-11-08 23:09 CET

A PC_VERSION-only `PICO2_TEST_OBJECT_DELETION=1` request-latch mode has been
added for a local disposable VPCD fixture. Eight persistent-array
allocate/clear/request cycles passed, with one applet-owned tag erased per
request and zero package-static tags erased. The harness passed 19/19. ARM and
RISC-V max_crypto wrapper builds passed; the default JCSystem behavior test
passed 3/3 and still reports deletion unsupported / `ILLEGAL_USE`.

This is a new focused host regression only. It does not reproduce the earlier
full JCAlgTest run that reclaimed 1,580 tags, cover all retained contexts/native
roots, or prove interruption recovery. Production firmware remains unchanged
and RESOURCE-01 remains open. The unexpected PacketRF device is excluded and
was not accessed. Continue offline with complete root coverage and
interruption-safety tests before considering production enablement.

Evidence log: `artifacts/logs/object-deletion-vpcd-20261108T220544Z.log`,
SHA-256 `9e09d434f15ae31e997eb74f66a1552397a61a7b3faafa16501cd597515cc3f9`.

# Current handoff — fail closed on retained applet contexts — 2026-11-08 23:21 CET

The PC-only request-latch collector now checks
`Context::isSoleRetainedAppletContext()` before scanning or erasing. It skips
and retains the deletion request whenever another retained Context or sibling
applet instance exists. The persistent-transaction suite passes with the new
single/second/destroyed/sibling context cases (`TEST:SUMMARY fail=0`); ARM and
RISC-V max_crypto builds pass; the exclusive VPCD fixture passes 19/19 and
reclaims one applet-owned tag on each of eight requests.

This is a conservative eligibility check, not full root enumeration. It does
not close PERSIST-01 for global/native roots or interruption recovery. Normal
firmware continues to report object deletion unsupported. Continue with typed
root provenance and complete root-domain auditing before broadening this test
path. The unexpected PacketRF device remains excluded and was not accessed.

Evidence: `artifacts/logs/object-deletion-vpcd-20261108T222047Z.log`, SHA-256
`678d0beb88222bebb5f988dcd7d3fb4f6acba55fdd6dea2dc49320fe31c5b1f9`.

# Current handoff — 64-cycle VPCD deletion stress — 2026-11-08 23:29 CET

The request-latch test wrapper now accepts 1–512 iterations (default 8) and
checks exact cleanup counts. A guarded 64-cycle VPCD run passed 131/131 APDU
checks. Each of 64 requests erased exactly one applet-owned tag; package-static
erasures were zero and there were no skipped requests. This is a repeated
single-owner byte-array fixture, not RSA/full JCAlgTest or physical-card
evidence.

Production firmware remains deletion-unsupported. Multi-context deletion still
fails closed; broader roots and persistent interruption/power-loss acceptance
remain. The unexpected device was not accessed. Next extend stress coverage to
key-object graphs and run alias/context-preservation controls before any full
JCAlgTest retry.

Log: `artifacts/logs/object-deletion-vpcd-20261108T222700Z.log`, SHA-256
`13b0053ed0e18a186759b65ec94c095f0bd86ba14be59898732bc19663847929`.

# Current handoff — repeated RSA key graph deletion in VPCD — 2026-11-08 23:37 CET

The local persistent-reclaimer probe now creates a 512-bit RSA public key,
sets modulus and exponent, clears the key's applet-instance reference, and
requests deletion. Two cycles passed 7/7 and each removed 10 owner tags. The
32-cycle stress run passed 67/67; every cycle erased exactly 10 applet tags
(320 total), and package-static erases were zero. VPCD logs are
`artifacts/logs/object-deletion-vpcd-20261108T223404Z.log` (2-cycle control) and
`artifacts/logs/object-deletion-vpcd-20261108T223514Z.log` (32-cycle stress).

This improves evidence over the byte-array fixture, but remains single-context
PC VPCD with no interrupted deletion, multi-owner roots, full JCAlgTest run, or
physical flash. Production firmware still reports deletion unsupported. Next
add live-alias preservation and erase-interruption coverage around this RSA
graph before expanding traffic or considering firmware wiring. Unexpected
PacketRF remains excluded.

# Current handoff — protect foreign alias chains during reclamation — 2026-11-08 19:50 CET

Uncommitted PERSIST-01 inventory changes now scan external package-static and
applet namespaces (`0x02`/`0x03`) for foreign object/array alias chains and
protect their target subtrees from owner-tag reclamation. The scan fails closed
on malformed/unreadable chains and is performed once per reclaimer call; each
erase batch still traces heap roots freshly. A regression proves that an
external array-to-object alias chain preserves the target and its nested body,
while an unrelated orphan is reclaimed; removing the chain makes the target
subtree reclaimable.

Validation in this turn: `./tools/test-persistent-transaction` (`fail=0`),
`./tools/test-persistent-inventory` (22/22), and `./tools/test-rp2350-fs-overlay`
(1,000 cuts, `fail=0`) pass. ARM, RISC-V, and PC builds already passed for this
source state during the current max_crypto image refresh. No physical board was
accessed; the unexpected device remains excluded. The normal persistent
reclaimer is still disabled and RESOURCE-01 remains open.

Next: commit the alias-root safety step, then continue PERSIST-01 by exercising
identified Pico2 JCVM devices only for multi-owner and interrupted-reclaimer
checks, refreshing the authorized reader helper first. Do not flash or use any
unidentified device. If physical safety acceptance cannot be completed, retain
the disabled production reclaimer and continue offline implementation/testing.

# Current handoff — malformed alias scan fails closed — 2026-11-08 19:57 CET

Added a transaction-suite regression for corrupt external alias metadata. It
proves the foreign-alias scan returns `InvalidTag` before erasing any records,
then reclaims the target and unrelated orphan after the malformed alias source
is explicitly removed. `./tools/test-persistent-transaction` passes with
`TEST:SUMMARY fail=0`. This is filesystem-stub evidence; no board was accessed.

Physical inspection is not ready: the installed PC/SC helper differs from the
repository's Pico-only filtered helper, and bounded `pcsc_scan -r` timed out
without output even though `pcscd.service` is active. Do not invoke the stale
helper or access the unexpected device. Continue offline until the approved
helper is installed and identified Pico2 JCVM readers are enumerated.

# Current handoff — current multipackage control passes after alias changes — 2026-11-08 20:08 CET

The unchanged host multipackage integration was rerun against the current alias
inventory changes. It passed `TEST:SUMMARY pass=47 fail=0`, covering duplicate
INSTALL rejection, restart restoration, cross-AID transient lifecycle, and
applet/package DELETE cleanup. Logs are under
`artifacts/logs/host-vpcd-multipackage-apdu-20261108T190022Z.log` and matching
restart/sidecar files. Normal persistent deletion remains disconnected, so this
is compatibility evidence, not reclaimer activation evidence.

Physical checks remain pending: the installed permission helper is stale, and
`timeout 8s pcsc_scan -r` produced no reader output. The local filtered helper
matches the approved Pico2 JCVM USB identity; it was not run with sudo because
only the stale installed helper is currently permitted. Do not interact with
the unexpected device. Continue offline resource lifecycle work.

# Current handoff — persistent write failure now logs exact tag — 2026-11-08 20:16 CET

Added failure-only telemetry in `FlashMemory_Handler`: failed persistent writes
emit status, payload length and the complete tag, and reference-serialization
exceptions emit the complete parent tag. This adds allocation-site evidence
without changing resource semantics. The persistent-transaction failure-injection
suite passes with `TEST:SUMMARY fail=0`; ARM and RISC-V max_crypto wrappers pass.
The current ARM image ends at `0x100F1EEC`, with 57,620 bytes before storage.

RESOURCE-01 remains unresolved: object deletion is still unsupported and the
production reclaimer remains disconnected. The current PC/SC enumeration timed
out; the installed helper differs from the repository's Pico-only filter, so
no physical operation was attempted. Next: use the new logs on the unchanged
JCAlgTest workload when safe physical/PC access is restored, and continue the
key-reuse/resource-lifecycle audit offline. Keep the unexpected device excluded.

# Current handoff — max_crypto artifact refreshed after write diagnostics — 2026-11-08 20:21 CET

The external-test ARM `max_crypto` empty-card UF2 was regenerated after the
persistent-write diagnostic change. Current file:
`artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`, 4,079,616
bytes, SHA-256 `b8e6829d8d102d57eba95e0af912e7e452afcf859510bc3023a84abe6f177d3c`.
Underlying firmware UF2 is 1,982,464 bytes with SHA-256
`a308b2f24bcc41d590a455991990dff014f45cd5c07de5d661cc859b0468b634`. Marker is
`FF2EB31F` (dirty tree based on HEAD `2EB31F`), release 0.1.1, profile
159/162, end `0x100F1EEC`, 57,620 bytes free. ARM and RISC-V wrappers passed;
package verification passed. The artifact is empty-card, has not been flashed,
and has no physical validation.


# Current handoff — RESOURCE-01 package-static key graph growth — 2026-11-08 20:43 CET

Added an opt-in PC VPCD inventory flag `PICO2_PERSISTENT_INVENTORY_TAGS=1`
that logs persistent tag and payload length (never record contents). PC max_crypto
build passed. Repeated the focused RSA public-key replacement probe for eight
APDUs; the VPCD probe passed 10/10 including ATR and SELECT. Each key replacement
added 10 records / 219 serialized payload bytes. Exact tag deltas are all under
`023808` (`path_static(package=56, offset=8)`). The applet-owner view remains
2 records / 17 bytes and reports zero unreachable records. This proves the
current applet-owner collector does not cover the package-static root graph.

This is a local probe diagnosis, not proof that the full JCAlgTest `F205` has the
same cause, and not physical flash evidence. Production persistent deletion
remains disabled. Next implement/test package-static-root-aware candidate
inventory/reclamation, accounting for retained contexts, aliases, transactions,
and interruption before wiring it into APDU processing. Keep the unexpected
PacketRF device excluded; do not claim RESOURCE-01 fixed.
Log: `artifacts/logs/resource-key-reuse-tags-vpcd-20261108.log`.


# Current handoff — package-static inventory and reclaimer primitive — 2026-11-08 21:08 CET

The read-only package-static inventory and a separate low-level package-static
reclaimer primitive are implemented. They are not called by normal APDU
processing, and object deletion is still unsupported. VPCD measurements show
why this matters: repeated RSA key replacement grows the static package graph
by 10 records / 219 serialized payload bytes per replacement after the first;
after eight replacements, the new package-static inventory reports 70 orphan
records / 1,533 payload bytes while the applet-owner inventory reports zero.
The full JCAlgTest `F205` relationship remains plausible but unproven.

The new collector preserves direct static slots and external alias chains,
fails closed for malformed aliases, refuses active transactions, preflights
before mutation, erases bounded batches and permits a fresh-heap retry after an
injected partial erase. `./tools/test-persistent-inventory` passes 22/22 and
`./tools/test-persistent-transaction` passes `fail=0`. ARM/RISC-V/PC max_crypto
wrapper builds pass. VPCD logs: `artifacts/logs/resource-package-static-
candidates-vpcd-20261108.log`. No physical device was accessed; the unexpected
PacketRF device remains excluded.

Next: audit roots across every retained applet/package context and durable
shareable/global roots; test filesystem recovery at power-cut boundaries on
hardware; then connect deferred `requestObjectDeletion()` only if those checks
pass. Re-run unchanged JCAlgTest 1.8.4/jc305 and verify a long-run plateau, no
`F205`, reset, FAULT or transport loss. Until then RESOURCE-01 is open and the
production reclaimer stays disabled.

# Current handoff — include active package static slots in inventory — 2026-11-08 21:15 CET

The PC-only VPCD inventory now loads installed static-reference slots for the
active package before tracing its roots. In the eight RSA public-key replacement
sequence, it found one package static root and still reported 70 unreachable
package-static records / 1,533 serialized payload bytes. The run passed
`TEST:SUMMARY pass=10 fail=0`. This reduces the chance that the observed orphan
count is caused by omitted active-package static references.

This remains a diagnostic, not production collection. It does not enumerate all
retained package contexts or prove the full JCAlgTest `F205` cause. Continue by
auditing the package-context/heap mapping and all durable roots before changing
runtime collection. The PC max_crypto build and VPCD probe passed; no physical
hardware was accessed. Unexpected PacketRF remains excluded.
Log: `artifacts/logs/resource-package-static-allroots-vpcd-20261108.log`.

# Current handoff — opt-in VPCD package-static reclaimer experiment — 2026-11-08 21:38 CET

Added a PC_VERSION-only `PICO2_RECLAIM_PACKAGE_STATIC=1` path. It hydrates
installed static references, adds the active Context roots and registered
applet root, then calls the bounded package-static reclaimer after an APDU. The
flag is off by default and not compiled into ARM/RISC-V firmware.

A disposable eight-replacement RSA public-key VPCD run passed 10/10 including
ATR/SELECT. Seven APDU passes erased 10 package-static records each. Pre-reclaim
inventory peaked at 27 records / 483 payload bytes, with 10 candidate records /
219 bytes; those counts stayed bounded. The no-reclaimer control reached 87 /
1,797. With the hook enabled, the multipackage VPCD suite passed 47/47 and all
44 observed reclaimer invocations returned `result=0`, `erased=0`; the fixture
did not create package-static orphans. Persistent inventory unit tests pass
22/22 and transaction tests report `fail=0`. PC max_crypto build passed.

This is host-model evidence only. The focused probe uses one package/context;
the multipackage fixture does not exercise package-static collection. Complete
retained-context/native-root accounting, interruption safety on physical flash,
and the full unchanged JCAlgTest 1.8.4 long run before considering firmware
wiring. Do not enable persistent collection in production. The unexpected
PacketRF device remains excluded and was not accessed.

# Current handoff — JCAlgTest with per-APDU package scan stopped early — 2026-11-08 22:00 CET

The full unchanged JCAlgTest 1.8.4/jc305 VPCD wrapper was run with the
PC_VERSION-only `PICO2_RECLAIM_PACKAGE_STATIC=1` hook. It was intentionally
interrupted after 76 support CSV result rows because of the slow per-APDU scan.
Before interruption, all 59 package-static reclaimer calls returned
`result=0 erased=0`; the applet logged 28 `F206` unsupported
`requestObjectDeletion` exceptions. No `F205` occurred before stopping.

SIGINT made the host emulator's ptrace wrapper panic on signal 2, so the harness
exited 1 and the client log ends with a short-APDU error. This is a cancelled
partial run, not proof of the original empty-response/NO_RESOURCE issue. Partial
results are in `artifacts/results/jcalgtest-support-vpcd-20261108T204110Z/`;
the CSV SHA-256 is
`a8393515ddafec3e215ce70e9d0e56b6c2c22f3599b1047292ac56a05bde7e20`.

Next, replace the all-APDU diagnostic approach with a request-gated host test
path: make `requestObjectDeletion` record intent, then service it at the safe
post-APDU boundary after the applet can clear cache references. Keep support
reported false in production and do not wire deletion to firmware until every
context/root and power-loss gate passes. No physical device was accessed; the
unexpected PacketRF remains excluded.

# Current handoff — preserve a live RSA graph through a static root — 2026-11-08 23:44 CET

The local VPCD fixture now roots a 512-bit RSA public key in both an applet
instance field and a package-static field. After clearing the instance field,
collection reports `static_roots=1` and erases zero tags; an APDU confirms the
static RSA key remains initialized. Clearing the static root then reports
`static_roots=0` and erases 10 applet tags. Two cycles passed 11/11; the stricter
one-cycle assertion rerun passed 7/7.

This validates the active package's static-root path in one Context only. It
does not cover foreign package roots, retained sibling contexts, native
registries or interrupted physical erasure. Keep production reclamation
disabled and continue those safety gates. The unexpected device remains
excluded and was not accessed.

Evidence: `artifacts/logs/object-deletion-vpcd-20261108T224233Z.log` and
`artifacts/logs/object-deletion-vpcd-20261108T224427Z.log`.

# Current handoff — single-request RSA batch reclamation probe — 2026-11-09 00:00 CET

The local request-gated VPCD probe now supports `rsa-batch`: build repeated
512-bit RSA key graphs, then issue one deletion request after the batch. A
2-key run passed 7/7 and reclaimed 20 applet-owner records. A 32-key run passed
37/37 and reclaimed exactly 320 applet-owner records; the final APDU confirmed
the fixture's retained key field was null. The probe now verifies deferred
cleanup after a larger sequence rather than one request per graph.

This is PC-only test-gate evidence in one fixture/context. Normal firmware still
has object deletion unsupported and persistent reclamation disabled. It does
not prove full JCAlgTest recovery, all-root safety, physical flash interruption
recovery, or an on-card capacity plateau. RESOURCE-01 remains unresolved.
Continue PERSIST-01 root and interruption-safety gates before any production
reclamation change. Unexpected PacketRF remains excluded and was not accessed.

Logs: `artifacts/logs/object-deletion-vpcd-20261108T225648Z.log` and
`artifacts/logs/object-deletion-vpcd-20261108T225803Z.log` (SHA-256
`ca1fc3ace73d2a73ebf9070054ddf453e270a24d9d29b0aa95e9dc08af459b2b`). The
fixture, wrapper and records are committed as `63f7cd17`.

# Current handoff — extend request-gated stress to RSA-4096 — 2026-11-09 00:11 CET

The local fixture and VPCD wrapper now select RSA public key sizes 512, 1024,
2048, or 4096 bits. Four 4096-bit create/request cycles passed 11/11 VPCD
checks, reclaiming exactly 10 applet-owner records per cycle (40 total). This
adds large key-material coverage to the focused test; it remains PC-only and
single-context evidence. Production deletion support remains false and the
normal persistent reclaimer remains disabled. RESOURCE-01 is not fixed or
accepted.

Validation command:
`PICO2_HOST_USB_ACCESS=1 ./tools/test-object-deletion-vpcd 4 rsa-public 4096`
Log: `artifacts/logs/object-deletion-vpcd-20261108T231016Z.log`, SHA-256
`0ebf558764dc3d8c95ea3757e62856362fb404774bd36a25c315d04d6cd21cf1`.
Next continue full-run-safe diagnosis and PERSIST-01 root/recovery gates; do not
access the unexpected PacketRF device. The fixture extension and records are
committed as `a0023fea`.

Current-tree revalidation: `./tools/test-persistent-transaction` passes
`TEST:SUMMARY fail=0`, including foreign alias safety, malformed input,
transaction refusal and interrupted cleanup/retry. This is unit-model evidence;
physical flash recovery remains open.

# Current handoff — physical reclaimer test image ready; helper refresh required — 2026-11-09 00:29 CET

Both current max_crypto architecture wrappers pass. An ARM diagnostic build
with `PICO2_DEBUG_PERSISTENT_GC=1` also passes; RISC-V wrapper passes its core
and smoke targets. Diagnostic image:
`build/arm/pico2_choupi_persistent_gc_test.uf2` (1,986,560 bytes,
SHA-256 `b1098c8e0ad7e07761e0d42bce24f65bc3bc451b42a07d42f0037cce4d27ebc8`).
The ELF includes the persistent-GC telemetry marker and is for focused
reclaimer validation, not release.

The checked-in `tools/pico2-pcsc-reader-root` uses exact Pico2 USB strings, but
the installed `/usr/local/sbin/pico2-pcsc-reader` has a different hash and is
stale. Do not execute it: it may grant access to the unexpected PacketRF device.
The user must run `sudo ./tools/install-pcsc-sudoers` from the repository root.
After that, use the safe host-access path and the existing
`tools/test-persistent-reclaimer-physical` on an identified Pico2 reader only.
No USB operation, physical flash, or token APDU was attempted during this
preparation. The full external request is in `.agent/EXT_REQUESTS.md`.
This preparation and its records are committed as `662aff8c`.

# Current handoff — RSA-4096 physical stress harness ready — 2026-11-09 00:38 CET

The physical reclaimer test now accepts 1..16 key-replacement cycles and RSA
sizes 512/1024/2048/4096 bits. Default is 8 x 4096. The local CAP fixture can
clear its RSA reference without calling unsupported JCSystem deletion; the
ARM diagnostic image's per-APDU reclaimer should collect orphaned graphs. The
wrapper bounds the host run and requires at least one successful nonempty
reclamation callback per replacement cycle.

CAP generation, host Java compile and shell syntax checks pass. A 4 x 4096-bit
request-gated VPCD control passes 11/11 and reclaims 40 owner tags. This is not
physical evidence. Do not use the stale installed helper: it differs from the
Pico-only repository helper and may include the unexpected PacketRF device.
Await the user running `sudo ./tools/install-pcsc-sudoers`; then use an exact
identified Pico2 reader name and chip ID, flash only the prepared diagnostic
image, and run `tools/test-persistent-reclaimer-physical '<reader>' 8 4096`.
The preflight and records are committed as `81af6f84`.
The harness and progress records are committed as `bb3cb62c`.

# Current handoff — reject stale physical-access helper — 2026-11-09 00:47 CET

`tools/test-persistent-reclaimer-physical` now compares the installed
`/usr/local/sbin/pico2-pcsc-reader` with `tools/pico2-pcsc-reader-root` before
calling it. The files currently differ; the harness will fail closed with an
instruction to run `sudo ./tools/install-pcsc-sudoers`. This prevents this test
from invoking the stale helper that may include the unrelated PacketRF device.
`bash -n` and `git diff --check` pass. No USB access was attempted. RESOURCE-01
still has no physical reclaimer evidence and remains unresolved.

# Current handoff — periodic inventory sampler validated — 2026-11-09 01:28 CET

The PC emulator supports `PICO2_PERSISTENT_INVENTORY_EVERY=N` for a full
persistent inventory/reachability sample every N completed APDUs. The strict
decimal parser rejects malformed intervals. `PICO2_PERSISTENT_INVENTORY=once`
and `=1` retain their previous behavior. The local 2-cycle RSA-public VPCD
fixture passed 7/7 at interval 2 and emitted three complete samples; the
legacy `PERSISTENT_INVENTORY: owner=...` prefix remains intact and sample
sequence is a trailing field. Multipackage VPCD regression passed 47/47 after
checking prefix compatibility. ARM and RISC-V max_crypto wrappers pass. No
physical device was accessed; the unexpected PacketRF device remains excluded.
This diagnostic does not enable firmware reclamation or prove long-run
stability; RESOURCE-01 remains open. See the latest progress record and logs
`artifacts/logs/object-deletion-vpcd-20261109T002617Z.log` and
`artifacts/logs/host-vpcd-multipackage-apdu-20261109T001659Z.log`. Commit:
`23318302`.

# Current handoff — persistent short-field failure regression — 2026-11-09 01:40 CET

Added `failed_persistent_short_write_keeps_cache_and_storage_canonical` to the
maintained persistent-transaction unit suite. An injected filesystem failure
leaves both the live cached short (`0x1234`) and durable record unchanged after
transaction rollback. `./tools/test-persistent-transaction` passes with zero
failures; persistent-inventory also passes with zero failures. ARM and RISC-V
max_crypto builds pass on the current worktree. No physical device was accessed;
PacketRF remains excluded. RESOURCE-01 remains open: host-only tests and builds
do not show that long-operation resource exhaustion or resets are fixed.

The archival review probe under `artifacts/reviews/review_20261103` is stale:
it uses old `tools/tests` fixture paths and its standalone target does not link
against current Stack/Context interfaces. Do not use it as an acceptance gate;
use `tools/test-persistent-transaction`. The recent probe edit is uncommitted
pre-existing work and was left untouched.
Implementation commit: `cbe14dd9`.

# Current handoff — integrated RP2350 filesystem power-cut simulation — 2026-11-09 00:21 CET

`./tools/test-rp2350-fs-overlay` now passes the complete suite with
`TEST:SUMMARY fail=0`. Its reclamation case tests 10 orphan records across
1,000 simulated flash cut points, preserving a live root graph and an unrelated
owner's record; restart must yield the complete old candidate set or clean
state, and retry must finish cleanup. This uses the actual RP2350 filesystem
implementation with fake flash, so it validates the COW recovery logic but is
not physical Pico power-cut evidence. Production object deletion remains
unsupported/disabled. Continue with a physical safe-boundary restart test and
the unchanged full JCAlgTest long-run plateau before declaring RESOURCE-01
fixed. The evidence record is committed as `a2f9e0ac`.

# Current handoff — physical reclaimer test image ready; helper refresh required — 2026-11-09 00:29 CET

Both current max_crypto architecture wrappers pass. An ARM diagnostic build
with `PICO2_DEBUG_PERSISTENT_GC=1` also passes; RISC-V wrapper passes its core
and smoke targets. Diagnostic image:
`build/arm/pico2_choupi_persistent_gc_test.uf2` (1,986,560 bytes,
SHA-256 `b1098c8e0ad7e07761e0d42bce24f65bc3bc451b42a07d42f0037cce4d27ebc8`).
The ELF contains the persistent-GC telemetry marker and is for focused
reclaimer validation, not release.

The checked-in `tools/pico2-pcsc-reader-root` uses exact Pico2 USB strings, but
the installed `/usr/local/sbin/pico2-pcsc-reader` has a different hash and is
stale. Do not execute it: it may grant access to the unexpected PacketRF device.
The user must run `sudo ./tools/install-pcsc-sudoers` from the repository root.
After that, use the safe host-access path and the existing
`tools/test-persistent-reclaimer-physical` on an identified Pico2 reader only.
No USB operation, physical flash, or token APDU was attempted during this
preparation. The full external request is in `.agent/EXT_REQUESTS.md`.
# Current handoff — RESOURCE-01 filesystem and bounded collector controls — 2026-11-09 04:28 CET

The CHOUPI OS host filesystem passed a new 100-cycle test with 16 applet-owned
512-byte records per batch, erase after each batch, 40 retained package roots,
and restart every ten cycles. The host FS suite passes 29/29 with two stress
tests ignored; running those existing 100,000-write/edit tests explicitly also
passes 2/2.

The PC-only request-gated RSA-512 deletion fixture passed 64 cycles / 131 APDU
checks, reclaiming 640 owner tags. Four periodic inventories returned to the
same 13 owner records / 287 bytes and 17 global records / 304 bytes, with zero
unreachable tags. Log:
`artifacts/logs/object-deletion-vpcd-20261109T032208Z.log`, SHA-256
`ed6f9427e8565084adfe3a9c1f063a69042f1be9fb5d8b186a9c26f3792d1c89`.

RESOURCE-01 remains unresolved. These controls do not explain the unchanged
JCAlgTest 1.8.4 full-suite F205 and do not test production Pico deletion or
hardware. Normal firmware still returns `ILLEGAL_USE` for
`requestObjectDeletion()`. PacketRF remains excluded. The host-only filesystem
regression is committed inside `choupi/os` as `a459209`; update the parent
submodule pointer with the accompanying root progress commit.

Next: run a denser, bounded JCAlgTest RSA KeyPair sequence with periodic
inventory around cache reset/free, identify which records remain reachable or
live at the F205 boundary, and finish retained-context / physical interruption
gates before considering production reclamation. The installed PC/SC helper
must still be checked against the Pico-only allowlist before hardware testing.

# Current handoff — RESOURCE-01 storage exhaustion and deletion probe — 2026-11-09 02:50 CET
