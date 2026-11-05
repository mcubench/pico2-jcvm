# USB startup and diagnostic builds for agents

**Implemented workflow.** The build wrapper consumes
`PICO2_DEBUG_SERIAL_WAIT_MS`, defaults it to zero on every invocation, and
rejects values outside `0..120000`. Diagnostic ARM UF2s are copied to a
filename containing the wait duration. Build provenance records profile,
wait, mode, image hash, and support-config hash.

## Behavior and setting

Normal firmware must start without a serial terminal. Remove the existing
30-second CDC/DTR wait and unconditional 500 ms sleep from normal startup.
Keep CDC logging available when a terminal connects later. Preserve one-context
USB/CCID handling; do not reintroduce competing background USB workers.

For agentic development, use an explicit diagnostic build only when early
startup output is needed. Wrapper input:

- `PICO2_DEBUG_SERIAL_WAIT_MS=0` (default): no console wait.
- A positive value, such as `30000`: wait at most that many milliseconds for
  CDC DTR, servicing USB frequently; continue when connected or at the deadline.
- Reject malformed, negative, or out-of-range values. The finite upper bound
  is 120000 ms; never allow an indefinite wait.

The build wrapper translates the input into an explicit firmware setting on
every invocation, including default zero. A diagnostic-to-normal build in an
existing directory resets the CMake cache value. The option works independently
of crypto profile. Normal output remains `pico2_choupi.uf2`; diagnostic output
is copied to `pico2_choupi_debug_serial_wait_<N>ms.uf2` with a matching
`.provenance.txt` sidecar.

## Build commands

Run from the repository root. Use these wrappers, not compiler binaries.

Normal/release build, including after a diagnostic session:

```sh
PICO2_DEBUG_SERIAL_WAIT_MS=0 ./tools/build arm --profile max_crypto
PICO2_DEBUG_SERIAL_WAIT_MS=0 ./tools/build riscv --profile max_crypto
```

Diagnostic build for agents capturing early boot output:

```sh
PICO2_DEBUG_SERIAL_WAIT_MS=30000 ./tools/build arm --profile max_crypto
PICO2_DEBUG_SERIAL_WAIT_MS=30000 ./tools/build riscv --profile max_crypto
```

The full JCVM target is ARM; RISC-V remains the required shared-code build
check. Keep diagnostic images distinguishable from normal/release artifacts.
Do not publish a diagnostic wait as the normal release default.

## Agent hardware workflow after implementation

All commands below that touch USB/PCSC require **host-access execution**:
`PICO2_HOST_USB_ACCESS=1` on that command **and**
`sandbox_permissions: "require_escalated"` on the execution tool call.
The environment marker alone is insufficient. Follow [AGENTS.md](../AGENTS.md),
refresh board identity, and stop hardware work if host access is unavailable.

With `board_serial` set to the freshly identified device, a diagnostic cycle
preserves the wait option through its internal builds and captures a finite
serial log:

```sh
PICO2_HOST_USB_ACCESS=1 PICO2_DEBUG_SERIAL_WAIT_MS=30000 \
  ./tools/cycle choupi --profile max_crypto --serial "$board_serial"
```

An additional bounded capture, if needed:

```sh
PICO2_HOST_USB_ACCESS=1 ./tools/monitor --serial "$board_serial" --seconds 8
```

The existing cycle builds both architectures before flashing and then opens
the serial monitor. Require `TEST:PASS`, the expected `TEST:SUMMARY`, and no
`TEST:FAIL`, `FAULT`, timeout, or nonzero status. Opening a monitor that asserts
DTR ends the diagnostic wait; this is intentional for early-log capture.

## Acceptance and return to normal development

1. Verify diagnostic startup both with DTR and without it: the wait must end
   on connection or its configured deadline. Capture startup phase timestamps.
2. Rebuild **both architectures with value zero** using the normal commands
   above, then flash the identified ARM normal image through the authorized
   host path and inspect its finite serial log.
3. Separately unplug/reconnect on Windows with **no terminal, serial monitor,
   or auto-reconnecting COM-port client**. Measure at least three cold
   reconnects from insertion to PC/SC reader listing and a successful read-only
   APDU. Record the UF2 hash, timing method, reader name, and whether driver
   installation was already complete. Keep first-install timing separate.
4. Compare against the original image under the same conditions. Distinguish
   USB enumeration, driver startup, PC/SC listing, and application readiness.
   Use a Windows USB trace if a substantial delay remains. Opening a serial
   monitor during this measurement can hide the original bug.
5. Record results and remaining limitations in progress/handoff. Keep HOST-02
   open until normal startup and diagnostic behavior pass their own gates.

This implementation removes the 30-second wait and 500 ms sleep from normal
startup. This document does not establish a particular build, physical
hardware result, or measured Windows startup time; check `progress.md` and
`.agent/HANDOFF.md` for current evidence.
