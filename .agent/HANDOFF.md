# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Latest commit: `8fc710a2` fixes `picotool load` selector ordering and makes transient sysfs hot-unplug reads safe. Its regression suite passes 22/22.
- ARM `max_crypto` image rebuilt from this commit: normal mode, object deletion ON, debug switches OFF, 36,024 bytes remain in the 1 MiB flash window. UF2 SHA-256: `e469c2a40339f59962c3a9265f6ce30522e85a3d288ec55f90df8b6f3172fd28`. Build provenance: `build/arm/build-provenance.txt`.
- Hardware access refreshed with `sudo -n /usr/local/sbin/pico2-pcsc-reader`, but the helper reported 0 matching devices; `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only` could not enumerate readers. No flash was attempted. Physical board availability is volatile; refresh helper and reader list again before hardware work.
- Previous token EE11 was running the older UF2; do not attribute the newly built image to it. No result from the failed cycle log `artifacts/logs/cycle-choupi-20261117T141248Z.log` establishes a transfer or runtime.

## Next actions
1. When a board is enumerated, select its exact chip ID and run `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile max_crypto --chipid ID` with elevated host access. Inspect its bounded serial log for `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, and absence of `FAULT`/`TEST:FAIL`.
2. Resume MEM-01 offline T10 safe-point exhaustion reclaim/replay design and garbage allocation tests without applet deletion requests.
3. Restart unchanged-client option 2 from fresh VPCD state to its configured pass/timeout; use only the supplied 1.8.5 client and unchanged 1.8.4/jc305 CAP. Physical options 2 and 1 remain acceptance gates after T7.
4. Windows external request and token/host troubleshooting are tracked in `.agent/EXT_REQUESTS.md` and `.agent/QKB.md`.
