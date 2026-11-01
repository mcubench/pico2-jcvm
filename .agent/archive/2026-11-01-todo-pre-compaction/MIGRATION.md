# TODO compaction migration — 2026-11-01

The [original TODO](TODO.md) is an immutable byte-for-byte snapshot;
`manifest.json` records its SHA-256, byte/line counts, and checkbox counts.
The active backlog is [repository TODO](../../../TODO.md). Source line numbers
below refer to the archived snapshot, not the compacted file.

All 31 original unchecked items remain represented by open or conditional tasks.
No unchecked item was declared complete solely by this compaction. The 150
completed checkboxes and every detailed evidence record remain in the snapshot;
`checkboxes.json` indexes all 181 checkboxes, original indentation, and dispositions.
The active completed-baseline table is a summary, not a replacement for evidence.

## Unchecked-item map

| Original line | Original item (first line) | Retained task(s) |
| --- | --- | --- |
| 186 | ARCH-08 — Bound native USB progress work (ARCH-04 follow-up). | [ARCH-08](../../../TODO.md#arch-08) |
| 251 | Implement a native trusted-VM inventory consumer with explicit owner | [PERSIST-01](../../../TODO.md#persist-01), [PERSIST-02](../../../TODO.md#persist-02) |
| 337 | Add malformed serialized edge/alias tests and prove an incomplete walk | [PERSIST-01](../../../TODO.md#persist-01), [PERSIST-02](../../../TODO.md#persist-02) |
| 387 | Verify recovery after an actual USB power cycle; PC/SC card reset | [PERSIST-02](../../../TODO.md#persist-02) |
| 399 | Preserve stable Java Card handles and validate interrupted-update and | [PERSIST-01](../../../TODO.md#persist-01), [PERSIST-02](../../../TODO.md#persist-02), [PERSIST-03](../../../TODO.md#persist-03) |
| 497 | Implement safe persistent-object reclamation/GC for repeated API | [PERSIST-01](../../../TODO.md#persist-01) |
| 593 | Re-run the remaining affected deploy cases after each fix on VPCD; | [API-03](../../../TODO.md#api-03), [EMU-01](../../../TODO.md#emu-01) |
| 711 | Validate long API sweeps with package-delete compaction on hardware. | [STORE-01](../../../TODO.md#store-01) |
| 719 | Investigate API 3.0.5 `EXC_RUNTIME` regressions on the v4 max_crypto | [API-01](../../../TODO.md#api-01), [API-02](../../../TODO.md#api-02), [EMU-01](../../../TODO.md#emu-01) |
| 1008 | AC1: verify GPPro on Windows. Linux PC/SC and GPPro were verified on | [AC-01](../../../TODO.md#ac-01) |
| 1032 | AC1–AC5: run the plan's clean-flash integration sequence three times. | [AC-02](../../../TODO.md#ac-02) |
| 1100 | Make CAP installation and filesystem updates recover atomically. CAP | [PERSIST-03](../../../TODO.md#persist-03) |
| 1221 | Broaden garbage-collector root and persistence-alias coverage. The | [PERSIST-02](../../../TODO.md#persist-02) |
| 1414 | Complete deploy-folder API coverage on the Pico 2. Runtime loading | [API-03](../../../TODO.md#api-03), [API-01](../../../TODO.md#api-01), [API-02](../../../TODO.md#api-02) |
| 1583 | Complete general SensitiveResult coverage. The result slot is VM-wide | [CTX-02](../../../TODO.md#ctx-02) |
| 1618 | Audit the declared Java Card profile API contract. The compatibility | [PROFILE-01](../../../TODO.md#profile-01) |
| 1624 | Complete multi-applet contexts and firewall semantics if the declared | [CTX-01](../../../TODO.md#ctx-01) |
| 1671 | Add extended APDUs only if required by the selected profile. The | [PROFILE-02](../../../TODO.md#profile-02) |
| 1778 | Diagnose intermittent Pico PC/SC/USB transport loss. The package and | [HOST-01](../../../TODO.md#host-01) |
| 2138 | Validate failed-install rollback and status words on hardware, then close | [PERSIST-03](../../../TODO.md#persist-03) |
| 2152 | Reconstruct the persisted applet object after reboot using its stored | [PERSIST-02](../../../TODO.md#persist-02), [PROFILE-01](../../../TODO.md#profile-01) |
| 2161 | Dispatch SELECT, `select()`, `deselect()`, and `process()` through the | [PROFILE-01](../../../TODO.md#profile-01) |
| 2180 | Implement the remaining Java Card APDU buffer and incoming/outgoing | [PROFILE-01](../../../TODO.md#profile-01), [PROFILE-02](../../../TODO.md#profile-02) |
| 2234 | Complete the JCAlgTest v1.8.2-used native API surface, including the | [PROFILE-01](../../../TODO.md#profile-01), [CTX-01](../../../TODO.md#ctx-01) |
| 2269 | Verify that an installed applet instance and its persistent fields survive | [PERSIST-02](../../../TODO.md#persist-02) |
| 2295 | Complete remaining GET STATUS / GET DATA behavior, including accurate | [GP-01](../../../TODO.md#gp-01), [PERSIST-02](../../../TODO.md#persist-02) |
| 2400 | Add golden applet coverage for exceptions, transient arrays, statics, | [PROFILE-01](../../../TODO.md#profile-01) |
| 2424 | Complete hardware integration: the current ARM `current` profile was | [AC-01](../../../TODO.md#ac-01), [AC-02](../../../TODO.md#ac-02), [API-03](../../../TODO.md#api-03) |
| 2453 | Pass AC1–AC5 and the plan's clean-flash integration sequence three times. | [AC-02](../../../TODO.md#ac-02) |
| 2648 | Run the packaged Windows workflow in `docs/windows-asymm-symm-test.md`; | [AC-01](../../../TODO.md#ac-01) |
| 2794 | Complete deferred hardening and robustness work (plan WP9). CAP | [HARD-01](../../../TODO.md#hard-01) |

## Reconciliation decisions

- ARCH-08 remains open after event-budget implementation: sustained-traffic,
  per-invocation wall time, and physical long-operation acceptance are unmeasured.
- Inventory/root primitives and reclaimer tests are implemented; owner, alias,
  stable-handle, incomplete-walk, actual-power-loss and enablement gates remain.
- Current Crypto has no EXC_RUNTIME; preserve the unexplained older failures as
  history and track current memory pressure, IV retests and full Security coverage.
- Old lifecycle/API checkboxes are retained as focused coverage/recovery gates,
  rather than instructions to rebuild already implemented methods.
- The selected card-manager profile has evidence, while new GP/ISO GET DATA,
  dynamic 2F00 and physical cold-power reconstruction still require validation.
- Windows and three consecutive clean-flash integration requirements are each
  consolidated under one task without weakening either acceptance condition.
- Completed reports remain profile/date-specific. Two upstream KeyPair-3072
  labeling defects stay explicit comparator mismatches, not missing firmware.
- A card reset, firmware reboot, emulator restart, simulated cut and USB power
  removal establish different recovery properties. Their distinctions survive.

## Navigation and history

Task IDs in TODO are the maintained owners. Archive wording may describe
superseded priorities, implementations, device availability, or host limitations.
Use current TODO and HANDOFF before reusing an old command or resuming work.
No build, firmware, hardware, or conformance result was produced by compaction.
