# Windows max_crypto b4adbe13 (v0.3-rc1): JCAlgTest 1.8.6/jc32 support and performance

Supplied 2026-11-26 at `inbox/pico2-jcvm-maxcrypto-deletion-b4adbe13/`. The two
CSVs are byte-identical copies (`SHA256SUMS`); the inbox copy is left in place.
No client log, serial log, chip ID or client JAR hash was supplied.

## Identity

- Firmware: CPLC ends `b4 ad be 13` / `d4 69 4d f9` = `0xB4ADBE13D4694DF9`,
  the normal max_crypto image of package `b4adbe13` (tag `v0.3-rc1`;
  UF2 `9373ebfb…5ae23`, ZIP `a2b461c5…b66b6`).
- Applet `1.8.6_jc32`. Client reports `1.8.5`: package `b4adbe13` carries the
  1.8.6 client before the version-string re-pin (`0e98fd56…`), same tests as
  `a90fe5bb…`. Client hash is inferred from the package, not reported.
- Reader `Pico2 JCVM CCID 0`, ATR `3b800181`, T=1. Performance run started
  2026/11/25 23:28:07, support run 2026/11/26 11:48:48 (tester's clock).

## ALGSUPPORT (8,627 rows, 338 yes)

- All 19 PQC rows `yes` (PQCKeyFactory ML-KEM/ML-DSA fresh and seed, KEM
  ML_KEM 512/768/1024, Signature ML-DSA four variants). No error rows.
- Same yes/no set as the local VPCD jc32 run of the same firmware code
  (`artifacts/results/jcalgtest-support-vpcd-20261125T221109Z`) except
  `RandomData[.OneShot] ALG_TRNG`: yes on hardware, no in the simulator
  (documented: no RP2350 entropy peripheral there).
- The jCardSim 1.8.5 reference comparison does not apply (1.8.4 applet).

## PERFORMANCE (option 2 incl. PQC section)

- Completed through `PQC - END`; 1,933 methods, 1,028 measured,
  864 `NO_SUCH_ALGORITHM`, 1 reconnect / 1 human intervention.
- Error rows (41):
  - 19 `ILLEGAL_VALUE` AES with 512-bit key (ECB/CBC/MAC): known,
    spec-correct since AES-XTS enabled 512-bit keys (progress 2026-11-21).
  - 6 `CryptoException_ILLEGAL_VALUE` RSA-512 OAEP: known, spec-correct.
  - 10 `CryptoException_ILLEGAL_USE` RSA/RSA-CRT public NOPAD doFinal
    (512–4096): known input-dependent class (input ≥ modulus); more rows
    than earlier runs (4).
  - **6 `6f00` on `TYPE_RSA_CRT_PRIVATE LENGTH_RSA_2048`** NOPAD/PKCS1/OAEP
    init+doFinal. Not seen in recent local runs; the run recovered at
    RSA-CRT 3072. Open: needs a local repro with serial telemetry.
