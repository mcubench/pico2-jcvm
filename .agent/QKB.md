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

## Current access snapshot — 2026-10-17

- Escalated read-only enumeration sees the running Pico as
  `Bus 001 Device 057: ID 2e8a:0009 Raspberry Pi Pico 2 CHOUPI CCID`. The
  authorized helper reports
  `PCSC:ACCESS_GRANTED node=/dev/bus/usb/001/057 service=pcscd.service`, and
  `systemctl is-active pcscd.service` reports `active` after running it.
- **New unresolved symptom:** Java SmartcardIO lists no readers even after the
  helper succeeds. `./tools/probe-jc-algtest` reports
  `PC/SC reader Pico 2 CHOUPI CCID not found`; the exact-name resolver likewise
  shows an empty reader list. The device node ACL includes `user:pcscd:rw-`
  and `user:claude:rw-`. PC/SC journal query returned no entries. USB
  visibility and node permissions are restored, but a CCID reader has not
  been published to clients; do not retry the static client until reader
  discovery works.
- Further bounded inspection confirms USB interface 2 advertises class
  `Chip/SmartCard` and CCID endpoints `03`/`83`. The installed libccid
  `Info.plist` maps VID/PID `0x2E8A`/`0x0009` to `Pico 2 CHOUPI CCID`. The
  service is socket activated; `pcscd.service` exited successfully after idle
  auto-exit while `pcscd.socket` remained listening. This normal idle exit does
  not explain the empty reader list observed while Java was querying it.
- No unbounded `pcsc_scan` process or active JCAlgTest client was found in the
  latest process inspection. Continue to avoid `pcsc_scan` because of the
  previously observed runaway case.
