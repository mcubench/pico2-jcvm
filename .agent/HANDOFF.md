# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- Latest commit `eb79e487` records the fresh APDU replay regression and current PC/SC blocker. The following CMake wrapper-root fix is in progress in `choupi/CMakeLists.txt`.
- ARM `max_crypto` rebuild passes: normal mode, object deletion ON, debug switches OFF, 36,024 bytes remain in the 1 MiB flash window. UF2 SHA-256 `20493fbb3028058d28ae10a39520fff55672a82a038e717a308e22e32dd39e8b`; provenance `build/arm/build-provenance.txt`.
- Latest `sudo -n /usr/local/sbin/pico2-pcsc-reader` reported 0 devices; elevated `PICO2_HOST_USB_ACCESS=1 ./tools/list-readers --pico-only` returned `list() failed`. No flash was attempted. Refresh both before any hardware work.
- T7 replay regression: option-2 APDUs 1584/1586/1588/1590 return `6F00` instead of `9000` on regular and overlay emulators. Trace and history are in `artifacts/results/resource01-apdu-replay-vpcd-20261117T142939Z/` and `/143248Z/`. Earlier Nov 15 replays reached 1,972 commands without unexpected statuses.
- Overlay build now links with empty `CMAKE_EXE_LINKER_FLAGS`; current binary hash is `4fd35962baf48a2a783a0305c6e8e19e3bb700b7e0ffda951b93d8c781e68024` and `nm` confirms `__wrap_fs_sync`, `__wrap_fs_erase_prefix`, `__wrap_fs_erase_package_state` are present.

## Next actions
1. Commit the in-progress CMake wrapper-root fix with its progress record. The ARM build passes; full JCVM cycle is unavailable until an identified board is enumerated.
2. Diagnose the APDU 1584 exception-handler rethrow using the unchanged transcript; compare CAP method bytecode/handler offsets and the Nov 15 passing binary/trace before changing VM behavior.
3. Restart unchanged-client option 2 from fresh VPCD state to its configured pass/timeout when PC/SC is available. Use only the supplied 1.8.5 client and unchanged 1.8.4/jc305 CAP. Physical options 2 and 1 remain acceptance gates after T7.
4. Windows external request and host/token troubleshooting are tracked in `.agent/EXT_REQUESTS.md` and `.agent/QKB.md`.
