# Session continuity

For substantial work, read [AGENTS.md](../AGENTS.md),
[SPEC.md](../SPEC.md), and [HANDOFF.md](HANDOFF.md) at startup. Use
[TODO.md](../TODO.md) for open work and [EXT_REQUESTS.md](EXT_REQUESTS.md)
for external dependencies. The [original port plan](../docs/archive/plan.md)
is historical. Search [progress.md](../progress.md) by task ID, Step-Id, or
commit and read the matching records. Consult [QKB.md](QKB.md) for relevant
troubleshooting and [TOOLCHAIN.md](TOOLCHAIN.md) for dependency setup.

Re-read the handoff before updating it. Keep it to 60 lines or fewer and
overwrite the current handoff with last-observed board state, active runs,
blockers, and immediate next steps. Put durable tasks in TODO and completed
work in progress. Preserve other sessions' work and verify that the pinned OS
and Java Card SDK submodules are populated before a full build.

## Progress record

Each `progress.md` record should be structured as:

```markdown
## YYYY-MM-DD <short step title> <total minutes of actual work>

- Step-Id: `<YYYYMMDD-HHMM-short-slug>` (same value as the commit trailer)
- Task: `<TODO identifier, e.g. MEM-01 T7, if applicable>`
- When: '<start date + time>' - '<end date + time>'
- Model: `<name of the model you are, e.g., gpt6-funny>`
- Change: <what changed and why>
- Validation: <commands/tests/hardware checks run and whether they passed>
- Result: <key measured or observed outcome; include failures or limitations>
- Decision: <what this establishes and the next action, if any>
- Artifacts/logs: `<paths, hashes, or none>`
```

Older records use `Commit: <SHA>` lines and separate backfill commits. Those are
historical; do not convert or "fix" them.

Include exact measurements, important runtime output, hashes, or archived log
paths when they materially explain how the step worked. State explicitly when a
metric was not measured or a validation was intentionally skipped.
