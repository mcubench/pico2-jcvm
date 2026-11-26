# Handoff

Updated 2026-11-26. See [TODO.md](../TODO.md) for active work and
[EXT_REQUESTS.md](EXT_REQUESTS.md) for external dependencies.

## Last observed physical state

- Development token `22A9DEAAA3D09795` (reader suffix `03 00`) runs normal
  max_crypto ExtendedLength firmware (UF2 `cebfc623…0897`). ECTester is
  installed alongside the JCMathLib and KeyPair probes. Five ECTester suites
  completed. Seven negative suites await explicit authorization. See
  `artifacts/results/ectester-22A9DEAAA3D09795/suite-summary.tsv`.
  Check live processes and USB identities before hardware or PC/SC work.
- EE11 holds the RSA key cache from the 2026-11-26 replay. 07E7 previously
  exhausted storage during an API probe. F42D retained ~83 KB of applet
  storage. Use the [QKB roster](QKB.md#physical-token-roster) for chip IDs.

## Next work

- WIN-B4ADBE13 RSA-CRT-2048 `6f00` (2026-11-26): not reproduced on the PC
  overlay simulator (APDUs 1-7110, deletion ON) nor on EE11 (APDUs 3007-7110
  over PC/SC, applet 1.8.5/jc305 already installed, ~72 KB store at the end).
  Leading hypothesis: `Cipher.getInstance` NO_RESOURCE (uncaught by JCAlgTest
  `prepare_class_Cipher`) on a fuller Windows store. EE11 now holds that
  RSA key cache in its 1.8.5/jc305 applet. See progress `20261126-1615-rsacrt2048-6f00-repro`.

- WIN-BE737F0E-6F00: `d65eb278` fixed the RSA-CRT-4096 watchdog-era
  `6f00` path by bypassing the Java allocation reserve for runtime exceptions.
  Windows confirmation and investigation of ALGSUPPORT RSA 4096 KeyPair
  "no" rows remain open. Earlier deadline fix: `20261123-0620-rsa4096-exec-deadline`.
- Await Windows GP DELETE confirmation (WIN-GP-DELETE-WATCHDOG-20261124,
  package `6d3bb526`).
- PQC: ML-DSA on 07E7/F42D remains open. `KEM.ALG_DHKEM` and the XDH
  named-parameter keys are unavailable. Use `JCALGTEST_VARIANT=jc32
  tools/test-jcalgtest-support-vpcd` for the supplemental 1.8.6/jc32 support
  test. The 1.8.5/jc305 pair stays the baseline.

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
