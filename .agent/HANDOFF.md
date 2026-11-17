# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Latest firmware/tooling commit: `8fc710a2` fixes `picotool load` selector ordering and makes transient sysfs hot-unplug reads safe. Its regression suite passes 22/22. Later commits record T10 measurements and safe retry analysis.
- ARM `max_crypto` image rebuilt from this commit: normal mode, object deletion ON, debug switches OFF, 36,024 bytes remain in the 1 MiB flash window. UF2 SHA-256: `e469c2a40339f59962c3a9265f6ce30522e85a3d288ec55f90df8b6f3172fd28`. Build provenance: `build/arm/build-provenance.txt`.
- Hardware access refreshed with `sudo -n /usr/local/sbin/pico2-pcsc-reader`, but the helper reported 0 matching devices; `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only` could not enumerate readers. No flash was attempted. Physical board availability is volatile; refresh helper and reader list again before hardware work.
- Previous token EE11 was running the older UF2; do not attribute the newly built image to it. No result from the failed cycle log `artifacts/logs/cycle-choupi-20261117T141248Z.log` establishes a transfer or runtime.
- New replay failure: 1.8.5 option-2 transcript APDUs 1584, 1586, 1588, and 1590 return `6F00` instead of `9000` on both regular and RP2350 overlay emulators. Trace: CryptoException reason 3 matches package-56 handler, which then rethrows and exits as SecurityException. Earlier Nov 15 replays reached 1,972 commands with no unexpected status changes. Start from `artifacts/results/resource01-apdu-replay-vpcd-20261117T142939Z/` (overlay exception trace) and `/143248Z/` (regular emulator). Do not count T7 complete until this regression is resolved.
- `tools/test-jcalgtest-support-vpcd` did not start the 1.8.5 client: PC/SC enumeration failed (`list() failed`); approved helper and reader list also found no Pico 2.

## Next actions
1. When a board is enumerated, select its exact chip ID and run `PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile max_crypto --chipid ID` with elevated host access. Inspect its bounded serial log for `TEST:PASS choupi_runtime`, `TEST:SUMMARY pass=1 fail=0`, and absence of `FAULT`/`TEST:FAIL`.
2. Diagnose the APDU 1584 exception-handler rethrow using the unchanged transcript; compare method bytecode/handler offsets and the Nov 15 passing binary/trace before any runtime change.
3. Fix the PC VPCD overlay target's missing `__wrap_fs_*` link roots in CMake (the successful local CMake cache workaround is ignored build state), then rerun the focused replay and unchanged-client option 2 when PC/SC is available.
4. Restart unchanged-client option 2 from fresh VPCD state to its configured pass/timeout; use only the supplied 1.8.5 client and unchanged 1.8.4/jc305 CAP. Physical options 2 and 1 remain acceptance gates after T7.
5. Windows external request and token/host troubleshooting are tracked in `.agent/EXT_REQUESTS.md` and `.agent/QKB.md`.
