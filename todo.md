# JCVM implementation backlog

Updated: 2026-10-20

This is the current prioritized backlog. It tracks work needed for the
JCAlgTest 1.8.2 / Java Card 3.0.5 integration and the remaining VM runtime
gaps. It is not a Java Card certification checklist. Unsupported algorithms
may correctly report `NO_SUCH_ALGORITHM`; implementing every algorithm is out
of scope.

## Priority 1 — close the current integration criteria

- [ ] **AC4: refresh the physical support-results comparison after OwnerPINX.**
  The previous physical `ALG_SUPPORT_EXTENDED` run completed 4,525 rows with
  15 `yes`, 4,510 `no`, and no transport or row errors. The archived jCardSim
  comparison also matched all 4,525 row identities and the declared 15-item
  support list, but the physical CSV predates the OwnerPINX fix. Rerun the
  official client on the Pico, archive the CSV/log, and run the comparator.
  Keep unsupported algorithms and `NO_SUCH_ALGORITHM` out of the failure
  count. Evidence: `.agent/HANDOFF.md`, `.agent/QKB.md`,
  `artifacts/results/jcalgtest-support-comparison-20261017.md`.
- [ ] **AC5: prove installed Java applet state survives a real power cycle.**
  VPCD verifies a fresh process can restore instance fields and persistent
  aliases (7/7 checks). Physical `tools/test-pico-powerup` has verified the
  installed registry and SELECT / GETVERSION / JCSystem after firmware
  restarts, but a physical power-off/on test that reads back previously
  written applet state is still needed. Run the probe before and after a full
  power removal, then repeat the applet test.
- [ ] **AC1 and AC2: repeat the documented GlobalPlatform path as one clean
  install flow.** Confirm `gp --list`, install the supplied JCAlgTest 1.8.2
  JC305 CAP with `gp --install`, and select it on the current firmware. Linux
  has prior positive evidence; Windows reader/GlobalPlatform compatibility
  and a current clean-install transcript remain to be recorded.
- [ ] **AC1–AC5: run the plan's clean-flash integration sequence three times.**
  For each run, start from the documented clean state, install the CAP, run
  supported-algorithms mode, compare the CSV, power-cycle, and repeat. Record
  each run separately. The plan defines three consecutive passes as the exit
  criterion (`plan.md`).

## Priority 2 — finish correctness gaps in implemented card behavior

- [ ] **Make CAP installation and filesystem updates recover atomically.**
  The CAP slot header can be marked installed before the filesystem
  transaction's final `fs_sync()`. A sync failure can roll back filesystem
  snapshots without rolling back that header. Add failure-injection coverage
  and coordinate durable recovery before changing the commit protocol. See
  `.agent/QKB.md` for the recorded risk.
- [ ] **Complete GlobalPlatform lifecycle verification.** Exercise authenticated
  `DELETE` over APDU, then verify GET STATUS and reinstall behavior after
  restart. Current parser, tombstone, cleanup, and host tests pass, but the
  full DELETE APDU plus restart lifecycle is not covered end to end. Extend
  GET DATA / GET STATUS continuation cases where the selected profile requires
  them. CPLC (`9F7F`) currently returns `6A88` because no manufacturing-data
  source is available.
- [ ] **Broaden garbage-collector root and persistence-alias coverage.** The
  reachability collector passed the complete physical static workload
  (2,262 transfers; heap stabilized at 83 array slots / 28 instance slots;
  one pass reclaimed 50 arrays). Add regressions for static references,
  native-held handles, aliasing, and lazy restoration in applet shapes beyond
  JCAlgTest before relying on collection for general workloads. Keep persistent
  flash records available when their RAM objects are reclaimed.
- [ ] **Verify PC/SC discovery and reinsertion repeatedly.** A Pico 2 BOOTSEL
  insertion was recognized as `2e8a:000f`, and firmware mode as
  `2e8a:0009`. This confirms the observed insertion, but repeated remove/add
  event timing and reader rediscovery after firmware reboot have not been
  characterized. Capture bounded udev/PCSC logs and verify the same board
  serial reappears.

## Priority 3 — define and extend the VM profile

- [ ] **Declare the supported Java Card profile and audit the API contract.**
  State the supported Java Card Classic version and supported APIs, then add
  applet-level behavior tests for each implemented API. Current regression
  coverage is strong for JCSystem, APDU, lifecycle, exceptions, and persistence,
  but does not establish broad profile compatibility.
- [ ] **Implement multi-applet contexts and firewall semantics if required by
  the declared profile.** The runtime currently has a single applet context.
  General context switching, custom shareable-interface callbacks and
  ownership checks, cross-applet AID lookup, and logical channels are not
  implemented. `getAppletShareableInterfaceObject()` returning null only
  covers the current no-server/default case; `getAssignedChannel()` reports
  basic channel 0.
- [ ] **Decide and implement commit-capacity reporting consistent with
  transactions.** `beginTransaction` / commit / abort and rollback behavior
  have coverage, but `getMaxCommitCapacity()` and
  `getUnusedCommitCapacity()` currently return zero. Document zero as a
  deliberate platform capability or add real capacity accounting and tests.
- [ ] **Add extended APDU support only if the selected profile requires it.**
  The current transport rejects extended APDUs. `APDU.waitExtension()` checks
  API state, but the synchronous CCID path cannot send T=1 WTX while processing
  a command; actual WTX signaling is unsupported.

## Already implemented or validated — do not treat as open tasks

- ARM and RISC-V repository builds pass; the RP2350 ARM image has passed
  bounded runtime checks on hardware.
- JCAlgTest 1.8.2 `ALG_PERFORMANCE_STATIC` completed on the Pico: 2,262
  transfers, no panic or GC traversal error; expected totals were 1,772
  `NO_SUCH_ALGORITHM` and 95 `CANT_BE_MEASURED`. SHA-256 256-byte timings were
  3.10 ms/op (`doFinal`), 3.34 ms/op (`update`), and 6.11 ms/op (`reset`).
- SCP03, basic GlobalPlatform listing/loading/selection, SHA-256 execution,
  APDU short-response chaining, applet registration/selection/reselection,
  tested JCSystem methods, transaction rollback, and VPCD process-restart
  persistence have working implementations and recorded tests.
- `JCSystem.requestObjectDeletion()` reports `SystemException.ILLEGAL_USE`
  because `isObjectDeletionSupported()` is false. Object deletion is an
  explicit unsupported capability, not a silent no-op.

## Scope guard

Do not add algorithms solely because JCAlgTest reports `NO_SUCH_ALGORITHM`.
The immediate goal is correct VM, applet, JCSystem, persistence, card-manager,
and transport behavior for the declared profile. Security hardening and Java
Card certification are separate work.
