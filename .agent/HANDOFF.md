# Session handoff — updated 2026-10-30

Read `AGENTS.md` and `.agent/SESSION_CONTINUITY.md` before work. The prioritized
opening section of `TODO.md` tracks active scope; later historical checklists
may be stale. `progress.md` is the append-only work record.

Full pre-compaction history: [archived handoff](archive/2026-10-29-pre-compaction/HANDOFF.md).
The archive is historical evidence, not current instructions or current board state.
[Issue knowledge base](QKB.md) contains current troubleshooting and a historical index.
Archive integrity is recorded in `archive/2026-10-29-pre-compaction/manifest.json`.

## Next concrete work

1. Continue safe persistent-object reclamation for repeated API workloads.
   Host emulation now exposes a bounded-memory Rust-OS tag cursor; Pico
   firmware uses a matching cursor over the mutable overlay in
   `src/choupi_pico2.cpp`. The physical cycle passed startup, but did not call
   the cursor. A native read-only visitor now pages applet-owned `0x03, owner`
   tags and rejects stalled/malformed cursor responses; it does not enumerate
   package-static roots or participate in GC yet. Next build the complete
   persistent graph inventory, then invoke this visitor from a diagnostic VM
   path and test an actual cursor call on hardware.
   No persistent records are deleted; preserve stable handles and keep loaded
   persistent records intact until reachability is proven.
2. Remaining acceptance/hardening: Windows validation, physical power-cycle
   checks where still open, and private RSA hardening.
   Available-memory accounting and JCOP4 P71 symmetric coverage are recorded
   investigation requests, not implemented features.

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

- 2BED: `2BED26ED20556535` was most recently flashed at 2026-10-30 01:11 UTC
  with the ARM `current` image SHA-256
  `e664415f23f3562905c5cc01a38a205d3c333be16a015268e091d38dd3bbdcfb`.
  Flash verification succeeded and the finite monitor passed repeatedly with
  `TEST:PASS choupi_runtime` / `TEST:SUMMARY pass=1 fail=0`; log:
  `artifacts/logs/cycle-choupi-20261030T011112Z.log`. The monitor did not call
  the filesystem cursor. The earlier `asymm_crypto` test outcomes below
  remain historical results for that profile, not the current flashed image.
  The cycle emitted no `TEST:FAIL` or nonzero exit. Its complete
  API 3.0.5 Security rerun completed 901 calls with 396 `OK`, 504
  `UNSUPPORTED_PER_SPEC`, one `NO_MEMORY`, zero `SW_ERROR`/wrong results, and
  successful CAP cleanup. D92 `D92E743D5F640C0F` did enter BOOTSEL and the
  corrected image was reported flash-verified, but it did not reappear in the
  subsequent PC/SC scan; current firmware/runtime state is unconfirmed. Do not
  infer D92 recovery or test results from flash verification alone. Latest
  scan found 8E and 2BED readers; use an identified available board as
  failover. RISC-V currently provides VM-core smoke coverage; it is not full
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
  snapshots (`4a178a2`, `b101b0a`). Focused EC errors and P-256 copies pass
  VPCD; physical retest pending.
- Signature digest continuation/state layout (`4b546aa`, `d6ebeeb`) and
  malformed precomputed-hash reason (`1aaf17c`) fixed. Upstream initial-digest
  group: 10 OK, eight expected unsupported, 448 filtered of 466. RSA and
  ECDSA SHA-1/256 continuation/precomputed cases pass VPCD. SHA-224/384/512
  continuation/precomputed coverage remains open: attempted upstream filters
  selected no such cases. Artifact directory:
  `artifacts/results/deploy/signature-initial-digest-vpcd-final/`.
- MessageRecovery mode/parameter reasons fixed (`be0ea69`, `2ae55a4`);
  NOPAD now permits both RSA directions, four selected VPCD calls pass.
  PKCS#1/OAEP mode restrictions remain. Physical retests pending.
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
  (`f7595d9`, `4ecd522`, `df2d2d9`, `bedf23a`, `da8ee60`); final-size reserves
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
