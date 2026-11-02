# Pico 2 Java Card VM

CHOUPI is an experimental native Java Card VM for Raspberry Pi Pico 2 (RP2350).
The ARM firmware interprets installed CAP files; Java Card API behavior and
cryptography are implemented in native C++. The project targets a tested subset
of Java Card Classic 3.0.5 and does not claim certification or full conformance.
RISC-V is maintained as a build check; ARM Cortex-M33 is the physical target.

## Start here

- [Development rules](AGENTS.md): build wrappers, hardware access, safety, and commits.
- [Project plan](plan.md): original scope and AC1–AC5 acceptance criteria; early
  baseline descriptions may be historical.
- [Initial PDF draft](docs/initial-pico2-jcvm-port-plan-2026-10-09.pdf): dated
  2026-10-09 and names JCAlgTest 1.8.4. Use `plan.md` and `TODO.md` for the
  current 1.8.2 acceptance work.
- [Active backlog](TODO.md): current priorities, task IDs, acceptance gaps, and
  links to evidence. This is the source of truth for open work.
- [Session handoff](.agent/HANDOFF.md): last observed environment and next action.
- [Progress log](progress.md): append-only validation and commit history.
- [Issue knowledge base](.agent/QKB.md): recurring failures and viable fixes.
- [Declared profile](docs/supported-profile.md): supported Java Card surface and
  current exclusions.

## Build and verification

Use the repository wrappers from the repository root:

```sh
./tools/doctor
./tools/build arm
./tools/build riscv
```

Physical flashing and PC/SC operations require the host-access path described
in `AGENTS.md`. See the [JCAlgTest physical runbook](docs/jcalgtest-physical-runbook.md)
for a profile comparison workflow. Refresh device identity and current task
acceptance before using the guide.

## Documentation map

- [Filesystem ABI](docs/FILESYSTEM_ABI.md) and [flash layout](docs/flash-layout-migration.md)
- [Native progress callback contract](docs/NATIVE_PROGRESS_CALLBACKS.md)
- [Development performance notes](docs/development-performance.md)
- [Dated reviews](artifacts/reviews/)
- [Historical Markdown path index](docs/documentation-moves-2026-11-02.md)
- [Historical status snapshot](.agent/archive/2026-11-02-status-pre-tidy/STATUS.md)
