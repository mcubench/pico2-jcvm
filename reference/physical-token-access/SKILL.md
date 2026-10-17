---
name: pico2-token-access
description: Diagnose Pico 2 USB, BOOTSEL, libusb, and PC/SC access failures from a development VM. Use when a physical token is missing, cannot be flashed, or is absent from PC/SC; exclude VPCD-only testing.
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
- If the configured helper is available, have the user run
  `sudo -n /usr/local/sbin/pico2-pcsc-reader`. It grants pcscd access to the
  current USB device node. Replugging changes the bus/device number, so an ACL
  applied to an old `/dev/bus/usb/...` path may no longer help.
- If PC/SC lists only `Virtual PCD` readers, the physical Pico is still
  unavailable. Do not treat VPCD visibility as hardware validation. Avoid
  `pcsc_scan`; it has previously remained running at high CPU after a timed
  invocation. Use a bounded repository Java PC/SC probe or the requested
  JCAlgTest command instead.
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

## Evidence

Record separately whether the VM enumerated the Pico, picotool confirmed the
expected mode, PC/SC found the physical reader, firmware was flashed, and a
bounded hardware response was observed. Never report a build, reader listing,
or attempted flash as proof that an on-device test passed.
