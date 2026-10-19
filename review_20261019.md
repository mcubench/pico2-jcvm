# Functionality review: implementation handoff (2026-10-19)

This guide is based on a read-only review of the current worktree. It is a work
list, not a claim that the proposed fixes or hardware behavior have been tested.
The worktree already contains extensive unrelated changes; inspect `git status`
before each step and stage only the files for that step. Keep source changes in
small commits with the required `progress.md` record and `.agent/HANDOFF.md`
update. Do not alter boot security, OTP, or flash partition tables.

## 1. Restore an instance whose AID differs from its CAP applet AID

**Observed path.** `CapLoadSession::installAndMakeSelectable()` in
`src/cap_load.cpp` parses the CAP applet AID and the instance AID independently
(around lines 489–500), supplies the instance AID to Java install, and stores it
in CAP metadata (around lines 553–589). On restart,
`CapLoadSession::restoreInstalled()` compares that stored instance AID with the
CAP applet AID (around lines 673–680). A valid distinct instance AID therefore
causes `reset()` and hides the installed applet. Both AIDs must remain distinct:
the CAP applet AID identifies the install entry point; the instance AID
identifies the selected application.

**Implementation.** Remove the instance-versus-CAP-applet equality requirement
from `restoreInstalled()`. Retain `cap_store_instance_aid()` length/CRC checks,
the CAP code validation, and the lifecycle tombstone checks. Check the return
value of `cap_store_applet_class_index()` as well: the current `(void)` call
around line 684 can set `installed_ = true` with class index `0xffff`. Copy the
stored instance AID into `instance_aid_` and use it for SELECT and context
restoration. Reject malformed metadata rather than
silently substituting the CAP applet AID. Do not relax the separate check that
the INSTALL command's applet AID identifies an applet declared by the CAP.

**Regression.** Extend the host install/restart path used by
`tools/test-vpcd-persistence` with a valid instance AID that differs from the
CAP applet AID. First process: LOAD, INSTALL, SELECT by the instance AID, and
write a persistent field. Fresh process from the same flash image: SELECT by
that AID succeeds and the field reads back. Check GET STATUS on the physical
card-manager path separately; the current VPCD session has no GET STATUS
handler.
Verify that SELECT by the CAP applet AID alone returns `6A82` when it is not an
installed instance. Preserve the existing same-AID restart regression. A
malformed stored AID/class-index record must fail closed.

**Done when.** Both same-AID and distinct-AID restarts pass, with no change to
the CAP applet lookup used by INSTALL. Build PC, ARM, and RISC-V through the
repository wrappers before any requested physical flash.

## 2. Preserve selection across a `61xx` SELECT response

**Observed path.** `choupi_vm_process_applet()` in `choupi/src/main.cpp`
(around lines 896–909) queues surplus response bytes and returns `61xx` when
the SELECT command's Le is smaller than the applet's response. The physical
SELECT branch in `src/ccid_device.cpp` (around lines 479–500) deselects for
every status other than `9000`. `choupi/src/vpcd_session.cpp` (around lines
147–169) makes the same decision from the response's final two bytes.
`choupi_vm_deselect_applet()` clears the pending response queue, so a subsequent
GET RESPONSE cannot retrieve the rest.

**Implementation.** In both transports, treat `9000` and all `61xx` status
words as successful SELECT-process outcomes. Keep the applet selected and the
queued response available after `61xx`. Preserve failure behavior for a false
`Applet.select()`, malformed response, or non-success status such as `6A82`.
Use one explicit status predicate in each path; check the VM return status and
the emitted final SW for consistency before deciding whether to deselect.
GET RESPONSE itself should not call `Applet.process()` or clear the queue.

**Regression.** Add a `tools/tests/vpcd_session_test.cpp` case whose SELECT
process callback returns `61xx`; assert selection remains active and no
deselect callback runs. Add an applet-level VPCD case with SELECT Le smaller
than its response: SELECT returns the first chunk plus `61xx`, GET RESPONSE
returns the remaining bytes plus `9000`, and an ordinary APDU still reaches
the selected applet. Keep the existing failed-SELECT (`6A82`) assertion. Run
`./tools/test-vpcd-session`, `./tools/test-short-apdu`, then the focused applet
VPCD test. If physically tested, send the same sequence through CCID/PCSC.

**Done when.** Both transports have the same state transition for `61xx`, and
the end-to-end VPCD sequence proves that the pending bytes survive SELECT.

## 3. Make CAP and filesystem state recover as one installation

**Observed path.** `CapLoadSession::finishLoad()` writes the CAP slot before it
updates lifecycle markers in the filesystem (`src/cap_load.cpp`, around lines
430–466). `installAndMakeSelectable()` syncs persistent Java data and then
rewrites the CAP header before its transaction returns (around lines
530–583). The filesystem has two generation/CRC snapshots in
`src/choupi_pico2.cpp`, while `src/cap_store.cpp` rewrites one CAP slot/header.
The stores have no common durable commit identity. An interrupted LOAD or
INSTALL can leave a valid CAP paired with the wrong filesystem generation, or
erase an earlier valid CAP while its filesystem state survives. This is also
tracked as open power-loss atomicity work in `TODO.md`.

**Implementation design.** Treat this as a storage-format change, not a
reordering of the existing calls. Define an installation epoch shared by CAP
metadata and the filesystem snapshot. Stage the new CAP in an inactive bank,
validate its length/CRC and applet metadata, and retain the prior valid bank.
Stage the matching filesystem state without losing the previous committed
snapshot. Publish the new epoch only after both new records are durable; on
boot, choose the newest complete matching pair and ignore incomplete pairs.
The reserved CAP region is `0x1c0000..0x1fffff` (256 KiB) and the maximum CAP
code is 28 KiB; two banks are a possible layout, but verify erase boundaries,
image size, existing version-1/version-2 header migration, and filesystem
snapshot retention before selecting it. Apply the same recovery rule to DELETE
and replacement LOAD. Avoid an in-place header erase as the only record of a
committed applet.

**Regression.** Extend the host flash/power-cut harness used by
`tools/test-rp2350-fs-overlay` and `tools/test-persistent-transaction` to cut
after each CAP erase/program, filesystem snapshot program, and commit marker.
For every cut, a fresh process must recover either the complete old install or
the complete new install, never a mixed CAP/instance/registry state. Include
failed INSTALL rollback, replacement LOAD, applet DELETE, package DELETE, and
legacy-header migration. Only then run the VPCD restart regression and the
ARM/RISC-V build gates. The physical AC5 power-cycle check remains separate;
the current handoff reports that PC/SC does not expose the Pico reader.

**Done when.** Every injected cut has a deterministic old-or-new recovery
result, and a real Pico power-cycle test confirms the installed applet and its
persistent fields survive. Do not claim physical validation from a host restart.

## 4. SCP03 key provisioning (deferred hardening)

`src/scp03.cpp` embeds the public test key `40..4F` (lines 15–19). This is
adequate only for a controlled development card: anyone who knows the source
can authenticate card-management commands. The project plan explicitly defers
certification-grade hardening, so keep this separate from the AC1–AC5 fixes.
Before deployment outside that setting, define a per-device key-provisioning
and rotation workflow, store keys outside the public firmware image, remove the
test-key fallback, and verify that existing authorized GP operations still
work while the test key fails. Do not use OTP or change boot security as part
of an autonomous implementation cycle.

## Scope and validation rules for follow-up agents

Short APDUs only are intentionally supported (`src/short_apdu.hpp`); extended
APDUs, real T=1 wait-extension signaling, and multi-applet contexts are separate
feature work. Record them as limitations rather than silently broadening the
fixes above. For platform-independent source changes, run `./tools/build arm`
and `./tools/build riscv` and fix warnings. Flash only after both pass. Run any
USB/PCSC operation through the host-access execution path from its first
attempt, with `PICO2_HOST_USB_ACCESS=1` for wrappers and escalated execution.
Treat a nonzero status, timeout, `TEST:FAIL`, or `FAULT` as failure. Keep each
logical fix and its `progress.md` record in a separate commit; preserve all
pre-existing worktree changes.
