# JCAlgTest 1.8.6/jc32 (PQC support tests)

User directive 2026-11-25: keep the current JCAlgTest with the Java Card 3.2
PQC tests as a testing CAP/client pair, applet version bumped to 1.8.6, and keep
testing with the unchanged 1.8.5/jc305 CAP as well. Unlike the other
directories here, these files were **built in this workspace** from the user's
AlgTest sources (`~/projects/algtest`, not a git checkout); they are not
upstream releases.

| File | SHA-256 | Bytes |
|---|---|---|
| `AlgTest_v1.8.6_jc32.cap` | `5f19662dfa8c1941299423a6c4dcac44c931bb7c29e631d07abc82647b816f8f` | 156883 |
| `client/AlgTestJClient.jar` | `0e98fd5631d3027bf8c7b8b764b2cd101b473b46338dcbe34668959f336ed6c1` | 156762 |

- GETVERSION returns `1.8.6_jc32`; the client still embeds version string
  `1.8.5`. Identify both by hash.
- The applet version is 1.8.6 for every API variant (user directive
  2026-11-25): the sources define `1_8_6__JC222/JC304/JC305/JC32` and assign
  them in the constructor, so a jc305 build of these sources would report
  `1.8.6_jc305` without PQC tests. Only the jc32 CAP is pinned; the unchanged
  1.8.5/jc305 CAP stays the baseline.
- The client also fixes a duplicated CSV line: `CardMngr.PrintCPLCInfo` and
  `PrintGPInfo` wrote their last field (`CPLC.ICPersonalizationEquipmentID`)
  twice; they now end with a plain line break.
- Package AID `4A43416C6754657374`, applet AID `4A43416C675465737431` (same as
  1.8.5, so only one of the two CAPs can be installed at a time).
- CAP format 2.3 (compact). Imports the 3.2 preview packages
  `javacardx.crypto.kem` and `javacardx.security.pqc`; needs a JCVM with the
  PQC API and CAP 2.3 support.
- `client/lib` links to the 1.8.4/1.8.5 dependency set (the jars match the
  client build's `dist/lib` byte for byte).
- `algtest-1.8.6-jc32-pqc.diff`: the complete source change against the
  user's 1.8.5 sources (applet, client, `preprocess_jcapi.py`, `jcbuild.xml`).
  The original `PerformanceTesting.java` was not backed up before editing; its
  diff baseline was rebuilt by reversing the anchored edits, and reapplying
  them reproduces the current file byte for byte.

## What the new tests cover (ALG_SUPPORT_BASIC and _EXTENDED)

- `javacardx.security.pqc.PQCKeyFactory`: key pair generation for ML-KEM-512/
  768/1024 and ML-DSA-44/65/87, each with an expanded private key and with an
  `ATTR_SEED` private key that is then expanded (12 rows).
- `javacardx.crypto.kem.KEM ALG_ML_KEM`: `getInstance` for the three ML-KEM
  sets in encapsulation and decapsulation mode (3 rows).
- `javacard.security.Signature ML-DSA`: modular `getInstance` with
  `ALG_NULL`/`PAD_NULL` for the four `SIG_CIPHER_ML_DSA*` constants (4 rows).

`tools/check-jcalgtest-pqc-support.py` requires all 19 rows to report `yes`.

## PQC performance tests (ALG_PERFORMANCE_STATIC)

Run in performance options 1 and 3 (with the asymmetric algorithms), or alone
with option 4 ("Only post-quantum algorithms"; interactive only, e.g.
`JCALGTEST_STDIN_LINES=$'2\n4'` without `-op`). For each of ML-KEM-512/768/1024
and ML-DSA-44/65/87:

- `PQCKeyFactory_generateKeyPair()`, `_generateKeyPair_seed()`,
  `_expandSeed()` (1 operation per measurement, 3 measurements, freshly built
  keys).
- ML-KEM: `KEM_encapsulate()` and `KEM_decapsulate()`; ML-DSA
  (`SIG_CIPHER_ML_DSA`, `ALG_NULL`/`PAD_NULL`): `Signature_sign()`, `_verify()`
  and `_init()` (5 operations per measurement, 5 measurements).

Key pairs are cached in the applet the same way as RSA keys
(`m_cached_keyPair_ML_*_1/_2`, `getPQCKeyPairObject`, other parameter sets
dropped on out-of-memory). The client sends `INS_CARD_RESET` with
`P1_CARD_RESET_FREE_CACHE` after each parameter set. Signatures and ciphertexts
go to a 4627-byte buffer, transient if the card allows it, otherwise
persistent. The ML-DSA reference signature for verify is computed in prepare
because ML-DSA signing time varies (rejection sampling) and would otherwise
add noise to the baseline. New applet commands: `INS_PREPARE_TEST_CLASS_KEM`
(0x3a), `INS_PERF_TEST_CLASS_KEM` (0x4a); `TestSettings.keyType`
`Consts.KEYTYPE_PQC_PRIVATE` (0x70) with the NamedParameterSpec name in
`keyClass`. Against the 1.8.5/jc305 CAP these tests should end in card errors
(unknown INS or key type), not results; this was not run.

## Rebuild

The PQC lines are commented out in `src/` and tagged `//jc32`, like the
existing `//jc305` code, so the client and older applet builds never see them.

1. In `~/projects/algtest/AlgTest_JavaCard`, generate `src32/` with
   `process_file(..., {'//jc304': True, '//jc305': True, '//jc32': True})`
   from `preprocess_jcapi.py` (the script's own `build32` ant target needs
   ant-javacard and `ext/jc261_kit`, which are not available here).
2. Compile with JDK 8 against the SDK 26.1 API:
   `javac -source 1.8 -target 1.8 -bootclasspath <jc261>/lib/api_classic-preview.jar:<jdk8>/jre/lib/rt.jar -classpath <jc261>/lib/api_classic_annotations-preview.jar`.
   `rt.jar` comes after the Java Card jar only so javac can resolve the boxing
   classes that overload resolution needs; the preview API jar lacks them.
3. Convert with `<jc261>/bin/converter.sh -enable-preview -out CAP JCA -applet
   0x4a:0x43:0x41:0x6c:0x67:0x54:0x65:0x73:0x74:0x31 algtest.JCAlgTestApplet
   algtest 0x4a:0x43:0x41:0x6c:0x67:0x54:0x65:0x73:0x74 1.0` (JDK 17 runtime).
4. Client: `ant clean jar` in `~/projects/algtest/AlgTest_JClient` (without `clean`, stale classes from earlier builds can end up in the jar).
