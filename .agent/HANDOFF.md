# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- QKB.md was reread: prior “no boards” meant no attached board was running JCVM, not no hardware. F42DD9D07D6D2B07 is the working identified board; refresh helper and reader before next USB/PCSC run.
- Commit `edb59d2b` records the direct physical RSA matrix on normal `max_crypto`: RSA-1984 PKCS1 ×50 (48.775 s, 47/47 extensions, min available memory 70,120 B); RSA-1536 ISO14888 ×50 control (1.721 s); RSA-2048 PKCS1 ×50 (49.174 s, 47/47 extensions, min 69,488 B); RSA-2048 SHA-PKCS1 sign (`AA9000`, 2.278 s). All returned `AA9000`, no fault/reboot. Artifact: `artifacts/results/longop01-rsa1984-physical-20261117T170835Z/F42DD9D07D6D2B07/`.
- T6 remains open: direct APDUs are not unchanged-client resume-list acceptance or full option-2. Prior 1.8.5 resume-list attempt repeated RSA-1984 ×50 and timed out at 300 s before RSA-2048: `artifacts/results/longop01-rsa1984-physical-20261117/F42DD9D07D6D2B07/ALGTEST_log_1794932230136.log`.
- T5 source audit plus physical check: card-manager E4 has no `NativeOperationScope`. After a normal cycle, GPPro authenticated applet/package DELETE passed on a 19,985-byte / 534-file snapshot, compacting to 134 then 2 files; unchanged CAP reinstall passed. Repeat debug run measured applet E4 1.057 s and package E4 726 ms on a smaller 4,247-byte snapshot, with no E4 time-extension record or fault. Artifacts in `.agent/QKB.md`; detailed serial/APDU logs under `artifacts/results/longop01-delete-authenticated-20261117T1728Z/` and `.../longop01-delete-measure-20261117T1732Z/`.
- The pre-cycle GPPro attempt failed before E4: initial ISD SELECT followed by `Out of memory`. Normal ARM build and full JCVM cycle restored F42D; image SHA-256 `6e645547bd70fbd8fad6fd65d7b9506ab7989964eaa1ab8af532b858e3e21f67`, normal, object deletion ON, debug OFF. T5 remains open for full 96 KiB occupancy/other flash-heavy card-manager paths and the earlier 15 s Windows result.
- LONGOP-01 T1/D2, T2, T5, T6, and MEM-01 T7/T8 remain open. Periodic 250 ms free-memory samples miss brief peaks and largest contiguous block.

## Next actions
1. Measure E4 DELETE and INSTALL near the filesystem capacity limit using a cheaper faithful preloaded state or focused fixture; retain exact PC/SC APDU timing, serial telemetry and watchdog evidence.
2. Investigate the earlier transient OOM after RSA work; start serial capture before any GPPro/client command.
3. Pursue normal-image empty-card option-2 acceptance after the focused blockers are understood.
