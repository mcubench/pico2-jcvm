# JCAlgTest 1.8.6/jc32 (PQC support tests)

User directive 2026-11-25: keep the current JCAlgTest with the Java Card 3.2
PQC tests as a testing CAP/client pair, applet version bumped to 1.8.6, and keep
testing with the unchanged 1.8.5/jc305 CAP as well. Unlike the other
directories here, these files were **built in this workspace** from the user's
AlgTest sources (`~/projects/algtest`, not a git checkout); they are not
upstream releases.

| File | SHA-256 | Bytes |
|---|---|---|
| `AlgTest_v1.8.6_jc32.cap` | `b6940d26501bd0818d581d3bf8359f6cb498386006bd6388ced05285052847da` | 149969 |
| `client/AlgTestJClient.jar` | `c6af938e66a886f14fa2b5f4b130f414159e3b84017a892eac08d202f1b0581c` | 165537 |

- GETVERSION returns `1.8.6_jc32`; the client still embeds version string
  `1.8.5`. Identify both by hash.
- Package AID `4A43416C6754657374`, applet AID `4A43416C675465737431` (same as
  1.8.5, so only one of the two CAPs can be installed at a time).
- CAP format 2.3 (compact). Imports the 3.2 preview packages
  `javacardx.crypto.kem` and `javacardx.security.pqc`; needs a JCVM with the
  PQC API and CAP 2.3 support.
- `client/lib` links to the 1.8.4/1.8.5 dependency set (the jars match the
  client build's `dist/lib` byte for byte).
- `algtest-1.8.6-jc32-pqc.diff`: the complete source change against the
  user's 1.8.5 sources (applet, client, `preprocess_jcapi.py`, `jcbuild.xml`).

## What the new tests cover (ALG_SUPPORT_BASIC and _EXTENDED)

- `javacardx.security.pqc.PQCKeyFactory`: key pair generation for ML-KEM-512/
  768/1024 and ML-DSA-44/65/87, each with an expanded private key and with an
  `ATTR_SEED` private key that is then expanded (12 rows).
- `javacardx.crypto.kem.KEM ALG_ML_KEM`: `getInstance` for the three ML-KEM
  sets in encapsulation and decapsulation mode (3 rows).
- `javacard.security.Signature ML-DSA`: modular `getInstance` with
  `ALG_NULL`/`PAD_NULL` for the four `SIG_CIPHER_ML_DSA*` constants (4 rows).

No PQC performance tests. `tools/check-jcalgtest-pqc-support.py` requires all
19 rows to report `yes`.

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
4. Client: `ant jar` in `~/projects/algtest/AlgTest_JClient`.
