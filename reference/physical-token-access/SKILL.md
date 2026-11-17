---
name: pico2-token-access
description: Diagnose Pico 2 USB, BOOTSEL, libusb, and PC/SC access failures, and choose which of several attached physical tokens to use (identify by chip ID, stay on one working token, switch only when it is unavailable). Use when a physical token is missing, cannot be flashed, is absent from PC/SC, or when starting hardware work with more than one token attached; exclude VPCD-only testing.
---

# Pico 2 physical-token access

Use this when a Pico 2 repeatedly disappears between the VM, `picotool`, and
PC/SC. Diagnose each layer independently; visibility in one does not prove
access in another.

## Choose the execution context first

Every USB/PCSC command must start through the host-access execution path by
setting `sandbox_permissions: "require_escalated"` on that command. Do not
probe USB visibility, `picotool`, serial ports, or PC/SC from the default
restricted context first. For repository USB wrappers, also pass
`PICO2_HOST_USB_ACCESS=1` on that one escalated command; the wrapper checks
both the explicit marker and `NoNewPrivs: 0` before touching libusb or PC/SC.
If host access cannot be selected, stop hardware work and continue offline.

## Identify the failure layer

Run diagnostics through host access. `./tools/doctor` is safe in the default
context, but direct hardware checks must use host access:

```sh
./tools/doctor
# In the host-access execution context:
lsusb -d 2e8a:0009
lsusb -d 2e8a:000f
```

- `2e8a:0009` is the running CHOUPI CCID device; `2e8a:000f` is commonly the
  RP2350 BOOTSEL device. Confirm the actual mode with `./tools/env picotool
  info -a` when USB access works.
- A VirtualBox “attached” indicator is not proof that the guest owns the USB
  device. If guest `lsusb` cannot see it, detach and reattach it from the VM's
  USB device menu, then check guest `lsusb` again. Do not start flash retries
  while the guest cannot enumerate the Pico.
- If `lsusb` sees the running device but PC/SC does not, check pcscd and reader
  permissions. Restart the service (reload fails when it is inactive), then
  run the bounded reader scan again.
- If the configured helper is available, run it yourself with elevated
  execution, exactly as `sudo -n /usr/local/sbin/pico2-pcsc-reader` (no
  prefix or extra arguments; see `AGENTS.md` sudo exceptions). It grants pcscd access to the
  current USB device node. Replugging changes the bus/device number, so an ACL
  applied to an old `/dev/bus/usb/...` path may no longer help.
- If PC/SC lists only `Virtual PCD` readers, the physical Pico is still
  unavailable. Do not treat VPCD visibility as hardware validation. Never use
  `pcsc_scan` for physical readers; it has previously remained running at high
  CPU after a timed invocation. List readers with the bounded
  `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers [--pico-only]`.
- `libusb -99` or picotool initialization errors mean direct USB access is
  unavailable in the current host/VM context, even if another layer reported
  the device. Restore guest USB passthrough and recheck before flashing.

## Flash and recover

- Use `./tools/flash <image.uf2>` or the repository cycle wrapper. They can
  request BOOTSEL through the firmware reset interface or CDC fallback when
  USB is accessible. Require both architecture builds to pass before a normal
  shared-source flash.
- For manual recovery, hold BOOTSEL while connecting the board, then confirm
  `boot type: bootsel` with picotool before loading firmware.
- Do not infer BOOTSEL from PC/SC state: BOOTSEL is a bootloader mode, while a
  PC/SC reader is exposed by running firmware.
- Stop after a bounded failed visibility or reset attempt. Record guest
  `lsusb`, `picotool info`, a bounded Java PC/SC probe, and the wrapper log;
  fix that layer before repeating. Never retry the same command in the
  restricted context.

## Choosing and keeping a token

Several Pico 2 tokens may be attached at once (four at the time of writing).
Preferred use:

1. **Identify first.** Before hardware work, enumerate the attached tokens and
   record each one's 16-hex RP2350 chip ID (and USB serial) so it is
   recognised across replugs and bus renumbering. Select by identity, never by
   reader index or bus/device number: `./tools/cycle choupi --chipid ID`
   (or `--serial ID`, `--usb-path PATH`) and the equivalent flag on the other
   wrappers. The roster of known chip IDs is kept in `.agent/QKB.md`.
2. **Stay on one token.** Pick one identified token as the *working token* and
   use it for all development cycles and tests for as long as it stays usable.
   Do not rotate tokens for convenience. Name the working token in
   `.agent/HANDOFF.md`.
3. **Switch only when it is unavailable.** Move to another token only when the
   working token cannot be used: not enumerated, will not flash, stalled or
   lost from PC/SC and not recoverable with the steps in this file. Record the
   board-level failure and the switch (old ID, new ID, reason) in `progress.md`
   and do not retry the failed token until access returns.
4. **Use two tokens on purpose.** Run on two tokens only when that is the point
   of the experiment: (a) to compare results from two different tokens, or
   (b) to parallelise independent long runs (for example JCAlgTest option 2 and
   option 1). Say which reason applies before starting.
5. **Never mix evidence.** Tie every result, serial log and firmware hash to one
   chip ID and keep per-token result directories separate. A switch or a second
   token produces new evidence; do not splice runs across tokens. Do not assume
   one token's stored state matches another's; acceptance runs still start
   from an empty card (see `AGENTS.md`).
6. **Refresh, don't trust old notes.** Board availability is volatile. At the
   start of hardware work refresh access with the approved helper and list
   readers with the bounded `./tools/list-readers --pico-only` (both through
   the host-access path above), then confirm the working token is still the one
   enumerated.

## Evidence

Record separately whether the VM enumerated the Pico, picotool confirmed the
expected mode, PC/SC found the physical reader, firmware was flashed, and a
bounded hardware response was observed. Never report a build, reader listing,
or attempted flash as proof that an on-device test passed.
