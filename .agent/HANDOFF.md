# Handoff

Updated 2026-11-24. See [TODO.md](../TODO.md) for active work and
[EXT_REQUESTS.md](EXT_REQUESTS.md) for external dependencies.

## Last observed physical state

- No campaign was recorded live after the 2026-11-21 T8 completion. Check
  live processes and USB identities again before hardware or PC/SC work.
- 07E7 retained its campaign state and later exhausted storage during a
  broad API probe. Temporary probe removal failed. Use EE11 for development
  while it remains usable. Confirm identities and exact reader names at use.
- F42D was restored to a normal max_crypto image after reclaim fault injection,
  with about 83 KB of 98.3 KB used. Its last recorded reader suffix was
  `02 00`. Use the [QKB roster](QKB.md#physical-token-roster) for full chip IDs.
- EE11 runs the normal max_crypto primitive-array-read candidate
  (UF2 `fa0f3c77…2170`). Its unchanged JCAlgTest 1.8.5/jc305 applet
  responded on reader suffix `00 00`. Use EE11 for further development
  while available.

## Next work

- WIN-BE737F0E-6F00: the Windows be737f0e option-1 run reported 20 `6f00` in
  the RSA-CRT-4096 Cipher block (uncaught non-Crypto exception in JCAlgTest
  `prepare_Key`, likely SystemException twice). Needs the Windows CSV and
  serial log; the 2 RSA-CRT-4096 `ILLEGAL_USE` rows were the 600 s deadline,
  fixed in step `20261123-0620-rsa4096-exec-deadline`.
- EE11 runs the GP DELETE no-snapshot build (`93517fb2` code, storage kept).
  The JCAlgTest 1.8.5/jc305 CAP is installed on an otherwise near-empty store.
- GP DELETE after a perf run (uninstall OOM panic / reader drop) is fixed and
  hardware-validated on EE11 (`20261124-2205-gp-delete-no-snapshot`). Windows
  confirmation: WIN-GP-DELETE-WATCHDOG-20261124 (package `6d3bb526`).
- WIN-BE737F0E-6F00: fixed in `d65eb278` (runtime exception instances bypass the
  Java allocation reserve); validated on EE11, needs a new Windows package for
  acceptance. Open: ALGSUPPORT RSA 4096 KeyPair "no" rows not investigated.
- EE11 was wiped (empty card) on 2026-11-24 and runs the PQC max_crypto
  image (`20261124-1700-pqc-api`); no applets installed. Reinstall JCAlgTest
  before using it for JCAlgTest work.
- PQC (JC 3.2 preview ML-KEM/ML-DSA): hardware-validated on EE11. Open:
  ML-DSA on 07E7/F42D not run; a JCAlgTest-style perf campaign does not cover
  PQC; `KEM.ALG_DHKEM` and the XDH named-parameter keys are not implemented.

- If continuing VM performance work, reprofile the XOR control and the
  interpreter fetch/decode phases before changing dispatch or branch checks.
  See the primitive-array-read step in [progress.md](../progress.md).
- Await Windows option 1/2 results for `v0.2-rc1` under
  WIN-MAXCRYPTO-RC-V0.2-RC1. The tag is local. Publish only with user authorization.
- Verify CAPIN-07 LOAD rejection and recovery on hardware. Preserve the SIO
  SensitiveResult regression when revisiting the full-record bound in
  `resolveClassref`. Import-dependent virtual method linkage still runs at
  INSTALL so packages can load before their dependencies.
- Verify the CAPIN-04 frame boundary on hardware. VPCD covers the 64-frame
  limit and next APDU. Method-entry allocation failure has unit coverage,
  with APDU-level injection still open (RV-20261106-43).
- Validate the RAM-pressure collector from `20261121-2154-ram-pressure-reclaim`
  on an empty normal max_crypto EE11 image when the required host access path
  is available. Run the full symmetric KAT through cleanup with serial and
  flash telemetry. If the cutoff recurs, use `PICO2_DEBUG_HEAP_GC=1` for the
  allocation and collection samples. RSA/RSA-CRT 4096 physical runs remain
  deferred at user request.
- XDH: the SDK 26.1 export files (`toolchain.jc261`, `lib/tools.jar`) now
  provide the named-parameter key ABI requested in CRYPTO-XDH-ABI-20261121;
  a reference CAP and expected outputs are still missing.
