# Session continuity

This repository implements the work packages in `plan.md`. Read `AGENTS.md`,
`plan.md`, this file, and `.agent/HANDOFF.md` before making substantial changes.

## Change discipline

- Preserve every pre-existing worktree change, especially the provided
  `choupi/` and `AlgTest_JavaCard/` trees.
- Keep each implementation change to one coherent step. Validate it with the
  available build, test, or inspection, append to root `progress.md`, and make a
  new commit containing the step. Do not amend earlier commits.
- Do not claim hardware validation unless an attached Pico 2 was actually
  flashed and its finite serial log inspected. Physical pool availability is
  volatile: refresh the authorized reader helper immediately before hardware
  work, and treat the current HANDOFF as the source for the last observed
  state. On 2026-11-01 the helper reported zero Pico readers; continue offline
  until a board is enumerated, then follow root `AGENTS.md` build gates and
  record bounded logs.
- Treat multiple identified physical Pico 2 devices as a failover pool. If one
  becomes unavailable, fails to enumerate, or stops responding, record its
  board-level failure and continue on another usable board without waiting for
  recovery. Keep remaining workers active in parallel runs. Reconfirm identity
  before flashing or attributing results, and preserve per-board image,
  outcome, and failure logs. Retry the unavailable board only after access
  returns.
- For long physical JCAlgTest campaigns, prioritize the interactive client
  options `2 -> PERFORMANCE TEST` (all JC API methods with 256-byte data,
  estimated 1–3 hours) and `1 -> All algorithms` (estimated 5–8 hours). These
  runs exercise periodic `INS_CARD_RESET` / `JCSystem.requestObjectDeletion()`
  calls. Use a different identified Pico 2 for the second long run when
  possible, instead of consuming both runs sequentially on one token. If a
  token becomes unavailable or stalls, continue on another enumerated token;
  keep separate per-device result folders and serial logs and record the
  firmware hash and chip ID for each run.
- Do not invoke compiler binaries directly. Use the repository build wrappers
  once available.
- Keep `.agent/HANDOFF.md` current with completed work, blockers, and the next
  concrete action. Never replace or remove earlier `progress.md` records.

## Source and dependency handling

- Runtime cryptographic and Java Card API behavior belongs in native C++ VM
  code reached through CAP-to-API dispatch. Do not add Java providers/adapters,
  or implement algorithms in test or upstream CAP files. Existing CAPs may be
  run to test native API behavior.

- The planned target is RP2350 Cortex-M33. The plan lists RISC-V as out of
  scope, while repository `AGENTS.md` includes a RISC-V build workflow; follow
  the more specific plan when choosing implementation scope and report any
  unavoidable divergence.
- Preserve the user-provided source and CAP artifacts. Do not replace the
  supplied AlgTest version without inspecting and recording its provenance.
- The pinned CHOUPI OS and Java Card SDK dependencies are submodules declared
  in `choupi/.gitmodules`; verify that they are populated before attempting a
  full build.

## Active validation inputs

The user authorized full testing-data migration on 7 November 2026. JCAlgTest 1.8.4 / jc305 is now the selected baseline, superseding historical 1.8.2 instructions. Use `tools/fixture-path` and `tests/fixtures/manifest.json`. Test code is under `tests/`; the supplied original tree is archived, not discarded. Preserve legacy results as historical evidence and require new version-specific acceptance.
