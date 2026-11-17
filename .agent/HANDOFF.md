# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- QKB.md was reread: prior “no boards” meant no attached board was running JCVM, not no hardware. F42DD9D07D6D2B07 is currently enumerated as a JCVM reader after the verified recovery cycle.
- Commit `edb59d2b` records a direct physical APDU matrix on F42D with normal `max_crypto` firmware: RSA-1984 PKCS1 ×50 (48.775 s, 47/47 extensions, min available memory 70,120 B); RSA-1536 ISO14888 ×50 control (1.721 s); RSA-2048 PKCS1 ×50 (49.174 s, 47/47 extensions, min 69,488 B); RSA-2048 SHA-PKCS1 sign (`AA9000`, 2.278 s). All returned `AA9000`, no fault/reboot. Artifact: `artifacts/results/longop01-rsa1984-physical-20261117T170835Z/F42DD9D07D6D2B07/`.
- T6 remains open: direct APDUs are not unchanged-client resume-list acceptance or full option-2. Prior 1.8.5 resume-list attempt repeated RSA-1984 ×50 and timed out at 300 s before RSA-2048: `artifacts/results/longop01-rsa1984-physical-20261117/F42DD9D07D6D2B07/ALGTEST_log_1794932230136.log`.
- T5 source audit found E4 DELETE has no `NativeOperationScope`; persistent commit writes a full filesystem snapshot in one `flash_safe_execute` call. Physical preflight failed before E4: GPPro's initial ISD SELECT was followed by filesystem compaction and `Out of memory`; see `artifacts/results/longop01-delete-audit-20261117T1815Z/F42DD9D07D6D2B07/`. Do not treat it as DELETE evidence.
- Recovery after this panic: ARM `max_crypto` build passed with 35,404 B flash headroom; full `cycle choupi` passed (`TEST:SUMMARY pass=1 fail=0`, no `TEST:FAIL`/`FAULT`). Current image SHA-256 `6e645547bd70fbd8fad6fd65d7b9506ab7989964eaa1ab8af532b858e3e21f67`, normal, object deletion ON, debug OFF. Cycle log `artifacts/logs/cycle-choupi-20261117T171632Z.log`.
- LONGOP-01 T1/D2, T2, T5 reproduction, T6, and MEM-01 T7/T8 remain open. Periodic 250 ms free-memory samples miss brief peaks and largest contiguous block.

## Next actions
1. Diagnose the OOM during GPPro's ISD SELECT after the RSA matrix. Start serial capture before any client/GPPro command; inspect allocation/GC and compaction state before repeating DELETE.
2. Once preflight is stable, reproduce E4 DELETE with bounded serial timing, extensions, APDU status and watchdog evidence; reinstall unchanged JCAlgTest CAP if it is deleted.
3. Continue target RSA peak/fragmentation work, then pursue normal-image empty-card option-2 acceptance.
