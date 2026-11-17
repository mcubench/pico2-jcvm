# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- ARM `max_crypto` normal build passes with object deletion ON, debug switches OFF, no C++ warnings, and 36,016 bytes remaining. UF2 SHA-256 `d5d914ce51a0c011904bb1213e151727a20aa194fea7bd33d78c83d4cbfe3794`.
- The focused T7 exception bug is fixed: `doThrow()` recognizes ISOException by framework AID in split ROM package IDs as well as package 0. The CAP catches unsupported Cipher algorithm 18, writes `03`, then throws ISOException `9000`; the applet rethrows it. The old path converted package-6 ISOException into SecurityException/`6F00`.
- Final max_crypto RP2350 overlay replay passed all 1,590 APDUs with `PICO2_TEST_OBJECT_DELETION=1`; the four formerly failing Cipher probes return `039000`. Two status differences are the known GET DATA differences before applet SELECT. Emulator SHA-256 `ad8b0f45c71dc3b3401e9fa4363e14620d786499bce1ef4b5618a14d2d62af94`; ROM mask `ab55f462bb535c9d90fb49b4aa9c40c6c45e26c8183ff2d4f22fdb1d1ff30925`. Inputs verified: 1.8.5 JAR SHA-256 `b6a753be280a225efc1b237fd53319c01946af15b4f2ae2e57adaad8e3eacec3`, unchanged 1.8.4/jc305 CAP SHA-256 `cac42cd3f83e91530f4fd50ae7c3412a2035f076945e3428238f9a84534dbfad`.
- T7's unchanged-client long VPCD run and physical T8 acceptance remain open. Approved helper found 0 Pico 2 devices; elevated reader listing returned `list() failed`, so no hardware cycle was run. Refresh before hardware work.
- Separate transaction case 89 still fails: bytecode-intercepted `Util.arrayCopyNonAtomic()` requests a 32,793-byte persistent write in a 65,536-byte sector already holding 32,793 bytes. See `artifacts/logs/jcsystem-transaction-apdu-vpcd-20261117T144708Z.log` and TODO.md.
- The RP2350 overlay CMake linker roots are committed in `3fa22bc8`; fixture verification passed.

## Next actions
1. Start unchanged-client option 2 from fresh state when PC/SC works, with telemetry gate and `PICO2_TEST_OBJECT_DELETION=1`; then physical option 2 and option 1 on identified tokens when available.
2. Continue separate case-89 / RV-20261106-50 investigation without weakening non-atomic semantics.
3. Refresh the approved helper and reader list before running `./tools/cycle choupi --profile max_crypto --chipid ID`.
4. External Windows action and host/token troubleshooting remain in `.agent/EXT_REQUESTS.md` and `.agent/QKB.md`.
