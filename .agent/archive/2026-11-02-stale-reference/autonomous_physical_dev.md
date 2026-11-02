---
name: pico2-autonomous-firmware
description: Run and diagnose autonomous build, USB BOOTSEL entry, verified flashing, serial testing, and reflashing loops on Pico 2/RP2350 hardware. Use when repeated firmware tests should work without unplugging the board, or when designing watchdog-assisted recovery.
---

# Autonomous Pico 2 firmware iterations

Use the connected physical board through one USB cable. A healthy Pico SDK USB
application can enter ROM BOOTSEL on a host request; a button press is not part
of the normal iteration. Preserve that capability in every candidate image.

This skill supplements the repository's `SKILLS.md`. Resolve repository paths
from its root. Read `AGENTS.md`, `.codex/SESSION_CONTINUITY.md`, and
`.codex/HANDOFF.md`; inspect unstaged/staged changes and acquire
`.codex/ACTIVE_SESSION` before editing. Preserve inherited work.

## Evidence and scope

The proven mechanism is `tools/flash`: SDK USB reset plus
`picotool load -f -u -v -x`, followed by strict CDC capture. See
[the operational how-to](references/usb-loop.md) for exact commands, recovery,
failure causes, and archived session evidence. Read it before the first flash.

For adding firmware recovery, read
[the watchdog design](references/watchdog-recovery.md). This is a proposed
extension, not an existing tested facility: current `src/main.c` uses watchdog
scratch registers for boot identity but never enables a watchdog timer.

## Normal loop

1. Run `./tools/doctor`; classify missing runtime serial separately from missing
   BOOTSEL access. Confirm permissions and VM capture of both USB identities.
2. Make one attributable firmware change. Use `./tools/build arm` and
   `./tools/build riscv`; fix every warning/error before flashing.
3. Run `./tools/cycle arm` or `./tools/cycle riscv` for the requested ISA.
   It rebuilds both, automatically enters BOOTSEL, loads/verifies/executes the
   UF2, and captures 45 seconds of strictly validated output by default.
4. Check actual picotool verification and expected fresh BOOT identity as well
   as command status. A wrapper's `FLASH:PASS` alone is insufficient.
5. Archive stdout **and stderr**, artifact hashes, source/configuration and
   failure class before the next attempt; `logs/<arch>-latest.log` is overwritten.
6. On failure, use the bounded recovery ladder in the how-to. Fix the cause and
   repeat autonomously while a working reset/recovery path remains.

Do not treat `./tools/monitor --seconds 8` as full validation: it is a diagnostic
capture and lacks `--require-pass`. A late attachment can miss one-shot startup
records. Obtain a fresh boot and use `cycle` for acceptance.

## Boundaries

Do not flash unless both current architectures build, except an explicitly
requested architecture-specific investigation. Do not invoke compilers directly,
use sudo, change host configuration, or operate on OTP, erase, boot security or
partition tables. Keep all edits inside this repository. This skill does not
itself authorize overclocking or overvoltage.

Stop repeated reset attempts when one deliberate BOOTSEL attempt and one
known-good recovery attempt cannot produce a real mode transition or verified
fresh boot. Diagnose host visibility first. Request physical BOOTSEL only after
software recovery is genuinely exhausted, with evidence and exact recovery
instructions. Never continue a campaign on the assumption that a no-op flash
installed the next image.

Update the handoff with the last confirmed running image (or unknown state),
log paths, both build results, recovery actions, and exact next step. Release
session ownership on completion.
