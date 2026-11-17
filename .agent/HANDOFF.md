# Handoff — next steps only

Updated: 2026-11-17.

## Current state
- **Host moved 2026-11-17** from `/home/claude/Projects/pico2-jcvm` (newer distro) to `/home/agentdev/projects/pico2-jcvm` (Debian 12, glibc 2.36). Details and fixes: `.agent/QKB.md` "Repository moved to a new host". The wrappers (`tools/env`, `tools/common.sh`, `tools/toolchain-path.sh`) now use `.local-toolchain/toolchain/arm-gnu-14.2` (gcc 14.2.1), host cmake/ninja, `.local-toolchain/ant`, and the system OpenJDK 17. An ambient `PICO_SDK_PATH` is ignored (override: `PICO2_SDK_PATH`). Stale CMake build dirs with `/home/claude` paths were moved to `build/_stale-claude-paths/` (git-ignored, safe to delete).
- **Verified on the new host:** `./tools/build arm` (baseline profile, mode=normal, object deletion ON, 0 repo-source warnings, 110,480 bytes flash remaining) and the host suites `test-rmi-registry`, `test-rp2350-fs-overlay`, `test-persistent-transaction`, `test-persistent-inventory` pass. The normal `max_crypto` image also builds (36,024 bytes flash remaining, 0 warnings; `src/cxx_terminate.cpp` keeps the libstdc++ demangler out). **Not repeated here:** any flash, VPCD campaign, PC/SC reader listing or JCVM hardware run. Refresh access with the approved helper and `./tools/list-readers` before each hardware session.
- **choupi/os submodule** is intentionally "dirty": `tools/patches/choupi-os-fs-broken-tail.patch` is applied by `tools/apply-choupi-os-patches` (called from `tools/build`). All three submodules are registered; `.gitmodules` sets `ignore` so `git status` is clean.
- **Compiler changed** (pre-move host toolchain to Arm GNU 14.2.1): note it when comparing timings with earlier campaigns.
- Low-watermark deletion compaction (`98fcb151`), pre-cleanup telemetry and the 90% gate (`1a28fa98`) are committed; the `PRE_CLEANUP` block was guarded for plain-PC and firmware builds in `c26477ef`.
- The 1.8.5 option-2 VPCD run was interrupted at RSA private 1536 (user request); partial run `artifacts/results/jcalgtest-support-vpcd-20261116T071855Z/` (peak 56.37%, no `F205`/`NO_RESOURCE`/`FS_WRITE_FAIL`/`FAULT`). Not resumable (missing `.cap-registry`). Do not Ctrl-C a running campaign (QKB).
- **Tokens (2026-11-17):** PC/SC helper installed and working on this machine. Four boards attached, none running JCVM firmware yet: `EE116F37C7E4A04E`, `07E74C5D7A038DEC`, `F42DD9D07D6D2B07` (BOOTSEL, blank) and `22A9DEAAA3D09795` (other program). Working token: EE11 (`EE116F37C7E4A04E`), JCVM max_crypto flashed and running (serial heartbeat OK); PC/SC reader visible: `Pico 2 CHOUPI CCID [JCVM CCID] (EE116F37C7E4A04E) 00 00` (libccid plist patched, helper reinstalled). PC/SC smoke test PASSED 2026-11-17 (ATR 3B800181, ISD SELECT, GET DATA, SCP03 authenticated; `GPGetDataApduProbe`). `tools/probe-gp-get-data` compares the card CPLC ID to the *current* HEAD, so it fails after any new commit; pass the image's recorded id via `PICO2_EXPECTED_CPLC_COMMIT_PREFIX` (see `build/arm/build-provenance.txt`, `cplc_source_id`). Roster and rules: `.agent/QKB.md`, `reference/physical-token-access/SKILL.md`.
- External requests: `.agent/EXT_REQUESTS.md` (WIN-MEM01-MAXCRYPTO-C050A193 Windows test, built by `tools/package-maxcrypto-windows-test`; RSA-1984 x50 serial capture).

## Tidy-up status
- Done: all planned repo tidy-up (see `progress.md` steps `20261117-*`). **Deferred by user:** docs/reviews/root `.md` relocation (`SKILL.md`, `plan.md`, `perf_tips.md` stub, `docs/`).

## MEM-01 next actions
1. Flash the JCVM to the chosen working token and check the serial log (`./tools/cycle choupi --profile max_crypto --chipid ID`); the fresh Windows package (c050a193) is built and requested; rebuild it with `tools/package-maxcrypto-windows-test` after source changes.
2. Restart unchanged-client option 2 from fresh VPCD card/registry/overlay state to `JCALGTEST_VPCD:PASS` or its configured timeout; no Ctrl-C.
3. Run physical option 2 and option 1 on separate identified empty-card tokens when possible; enforce the telemetry gate and record per-device hashes/logs.
4. Continue T10 safe-point exhaustion replay/retry design and garbage-allocation tests without applet requests, plus HOST-01/LONGOP-01 and plateau evidence.
