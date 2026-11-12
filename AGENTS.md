# Pico 2 development instructions

The target is a Raspberry Pi Pico 2 with an RP2350 connected by its USB port.

Supported CPU architectures:

- ARM Cortex-M33: `rp2350-arm-s`
- Hazard3 RISC-V: `rp2350-riscv`

Use the repository wrappers; do not invoke compiler binaries directly.

Java Card runtime boundary:

- Pico 2 runs the native Java Card VM, which interprets installed CAP files.
  It does not run `.java` or `.class` programs as firmware.
- Implement Java Card API behavior and cryptographic algorithms in native C++
  VM code, exposed through native API dispatch.
- Do not add Java provider/adapter implementations to firmware or create or
  modify CAP files to implement algorithms. Use existing upstream CAPs only to
  exercise native API behavior.

- Build ARM: `./tools/build arm`
- Build RISC-V: `./tools/build riscv`
- Build both, flash, and capture logs: `./tools/cycle arm` or `./tools/cycle riscv`
- Capture finite serial output: `./tools/monitor --seconds 8`
- Diagnose the host setup: `./tools/doctor`

For the long JCAlgTest client campaigns, run `2 -> PERFORMANCE TEST` (1–3
hours) and `1 -> All algorithms` (5–8 hours) on different identified Pico 2
tokens when possible. Keep each token's result directory and serial telemetry
separate, record its chip ID and firmware hash, and continue on another
available token if one becomes unavailable or stalls.

After changing platform-independent source:

1. Build ARM and RISC-V.
2. Fix all errors and warnings.
3. Flash the requested architecture.
4. Inspect serial output.   
5. Treat `TEST:FAIL`, `FAULT`, a timeout, or any nonzero command status as failure.
6. Diagnose, edit, and repeat until the hardware test passes.

Runtime output uses `TEST:PASS`, `TEST:FAIL`, and `TEST:SUMMARY` for known-answer tests; `BENCHMARK:PASS` for measured Bitcoin double-SHA-256 throughput; `MINING:START`, `MINING:PROGRESS`, and `SHARE:FOUND` for mining work; and `FAULT` for runtime failures.

Safety constraints:

- Never run `picotool otp`, `picotool erase`, or commands that alter boot security, OTP, flash partition tables, or machine configuration.
- Run every USB/PCSC operation (`lsusb`, attached-device `picotool`, serial monitor, raw CCID, or physical SmartcardIO) through the host-access execution path from its first attempt. For repository wrappers, pass `PICO2_HOST_USB_ACCESS=1` on that single command and invoke it with `sandbox_permissions: "require_escalated"`; both are required, and the wrapper refuses to reach libusb/PCSC without them. For direct system commands, use `sandbox_permissions: "require_escalated"` on the command itself. Never probe one of these commands in the default restricted context or retry there after a failure. If host access is unavailable, stop hardware work and continue offline.
- Do not use `sudo` from an autonomous coding cycle except exceptions listed in section 'sudo exception'.
- Do not modify files outside this repository.
- Do not flash unless both architectures currently build, unless explicitly investigating an architecture-specific failure.

## sudo exceptions
```
sudo -n /usr/local/sbin/pico2-pcsc-reader
```
## Incremental commits and progress log

Work in small, logical steps. A logical step is one coherent change that can be
validated independently.

At the end of every logical step:

1. Validate the step with the relevant build, test, hardware cycle, or inspection.
2. Append a concise record to the repository-root `progress.md`; never rewrite or
   delete earlier records.
3. Create a new Git commit containing the step and its `progress.md` record. Do
   not amend or reuse the previous step's commit.
4. Commit only files that belong to that step; preserve unrelated pre-existing
   changes.

Each `progress.md` record should be structured as:

```markdown
## YYYY-MM-DD � <short step title>

- Step/experiment: `<identifier if applicable>`
- Change: <what changed and why>
- Validation: <commands/tests/hardware checks run and whether they passed>
- Result: <key measured or observed outcome; include failures or limitations>
- Decision: <what this establishes and the next action, if any>
- Commit: `<commit SHA>`
- Artifacts/logs: `<paths, hashes, or none>`
```

Include exact measurements, important runtime output, hashes, or archived log
paths when they materially explain how the step worked. State explicitly when a
metric was not measured or a validation was intentionally skipped.

## Session Continuity

This repository may be worked on by multiple Claude/Codex sessions/accounts.

For any substantial task, read and follow:
`.agent/SESSION_CONTINUITY.md`

Maintain `.agent/HANDOFF.md` as described there so another Claude/Codex
session can safely continue interrupted work.

Never discard existing uncommitted changes merely because they were
created by another session.

## Issues knowledge base

Maintain `.agent/QKB.md` file containing issues encountered (e.g., inaccessible token, failures to run lsusb, sudo related issues...). When robust fix for the issue is found, mark it as viable solution and try to use it later when issue is encountered again. 

## Validation suites and test layout

- For new full JCAlgTest integration campaigns, use the user-supplied **1.8.5 host client** at `tests/fixtures/external/jcalgtest/1.8.5/client/AlgTestJClient_v1.8.5.jar`, explicitly selected with `tools/run-jcalgtest --client-jar PATH` (or the runner's supported client override). Scripts and `./tools/fixture-path jcalgtest.client` still default to the historical 1.8.4 client; workers must override that default. Keep the unchanged supplied **1.8.4 / jc305 CAP**, resolved with `./tools/fixture-path jcalgtest.cap`. Record client and CAP versions separately. Do not fall back to 1.8.2. See `tests/README.md` for invocation and hash verification.
- The separate supplied Java Card API validation suite is under `tests/fixtures/external/javacard-api/provided-20261107/`; retain all version variants and use its own client.
- `tests/fixtures/manifest.json` pins inputs and hashes. Run `./tools/fixture-path --verify` before acceptance. Keep supplied CAP/JAR bytes unchanged; no Java algorithm providers or modified algorithm CAPs.
- Test code belongs in `tests/unit`, `tests/integration`, or `tests/hardware`; repository-owned test applet sources belong in `tests/fixtures/local`. Keep stable entrypoints in `tools/`.
- Generated tests/CAPs/images go under `build/`; logs/results go under `artifacts/`. SDKs/converters belong in `.local-toolchain/`, not external fixture directories.
- Legacy 1.8.2 inputs/results remain explicitly versioned historical evidence. Recheck imports, client compatibility, expected responses and support baselines for 1.8.4; never relabel old results as 1.8.4. See `tests/README.md` and `docs/testing-data-migration.md`.

### Long physical JCAlgTest campaigns
- Treat Windows-host testing as independent evidence; still run the unchanged 1.8.4 client on a physical Pico 2 in this environment when PC/SC access is available.
- Prioritize client option `2` (all JC API methods, 256-byte data; estimated 1–3 hours) and option `1` (all algorithms; estimated 5–8 hours). These runs exercise periodic `INS_CARD_RESET` / `JCSystem.requestObjectDeletion()` behavior.
- Use a different identified Pico 2 for the second campaign when possible. Keep per-device results, reader name, chip ID, firmware hash, and serial telemetry separate. If one device disappears or stalls, continue on another available device rather than waiting for it.
- Do not claim either run started until the physical reader is enumerated in PC/SC and the client is connected to that exact reader.
