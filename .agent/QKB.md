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
