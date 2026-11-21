# Pico 2 Java Card VM

## 1. Educational project — read first

> [!WARNING]
> This project is **NOT production-ready. Do not use it for security-critical tasks.** It was almost completely coded by large language models, mostly GPT-6 Sol, GPT-6 Luna, and Claude Opus 5.5.
>
> This is a self-educational attempt to understand how to:
> 1. Perform complex agentic development with real physical hardware.
> 2. Keep agents working effectively day and night using skills, `/goal`, task lists, guardrails, acceptance gates, and repository cleanup.
> 3. Conduct effective functional, performance, and security reviews.
> 4. Give humans insight into what agents implemented over a day, week, or month.
> 5. Understand the architecture of the resulting system.

## 2. What this project does

This project turns a Raspberry Pi Pico 2 into a physical USB token that emulates a smart-card reader with an inserted Java Card. Your operating system sees it as a **PC/SC reader with a card**, accessible through standard smart-card tools.

The firmware implements a functional **Java Card virtual machine** and **GlobalPlatform applet management**. You can upload, install, and use Java Card applets supplied as CAP files, provided they use the supported bytecode and API subset. Applets execute on the Pico itself, with persistent data stored in flash.

The implementation is based on [CHOUPI, an open-source Java Card Virtual Machine](https://github.com/gavz/choupi), extended with Pico 2 hardware support, USB CCID transport, applet management, persistence, and native cryptography.

![Pico 2 Java Card VM firmware architecture](docs/pico2_jcvm_firmware_architecture_flat.png)

## 3. Try it

You need a Raspberry Pi Pico 2, a USB data cable, Java, and PC/SC support.

1. Download the following files.

   - The ARM firmware `pico2-jcvm-max_crypto-arm__latest.uf2` from the [latest Pico 2 JCVM release](https://github.com/mcubench/pico2-jcvm/releases/latest).
   - `gp.jar` from the [latest GlobalPlatformPro release](https://github.com/martinpaljak/GlobalPlatformPro/releases/latest).
   - The Java Card 3.0.5 applet CAP and `AlgTestJClient.jar` from the [latest JCAlgTest release](https://github.com/crocs-muni/JCAlgTest/releases/latest). Save the CAP as `jcalgtest.cap` beside `gp.jar`.

2. Hold **BOOTSEL** while connecting the Pico 2. Copy the image onto the **RPI-RP2** drive; the board reboots as a USB smart-card reader. An **empty-card image erases installed applets and persistent data**.
3. Install the latest JCAlgTest applet.

   ```bash
   java -jar gp.jar --install jcalgtest.cap
   ```

4. Run the JCAlgTest client and select the Pico reader when prompted.

   ```bash
   java -jar AlgTestJClient.jar
   ```

Unsupported algorithms are expected. A support report does not establish security or full Java Card conformance.

## 4. Build from source

Build setup is currently manual. Initialize the required submodules and provision the dependencies described in the [toolchain notes](.agent/TOOLCHAIN.md): Pico SDK, ARM toolchain, CMake/Ninja, Rust, Java/Ant/Maven, and Java Card development dependencies.

From the repository root, use the wrappers:

```sh
./tools/doctor
./tools/build arm --profile max_crypto
./tools/package-empty-card-uf2
```

Outputs:

- Firmware: `build/arm/pico2_choupi.uf2`
- Empty-card image: `artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2`

ARM Cortex-M33 runs the complete JCVM firmware; RISC-V (Hazard3) is out of scope.

## 5. Architecture and development

The Pico 2 runs a native C++ Java Card VM derived from CHOUPI. It interprets installed CAP files and dispatches Java Card APIs and cryptography to native C++ code.

```text
Host application → PC/SC → USB CCID → Card manager / JCRE
                                    → CAP interpreter → Native APIs
                                                      → Flash storage
```

The compatibility target is **Java Card Classic 3.0.5**. Full specification conformance remains unverified.

| Feature | Status | Coverage / limits |
| --- | --- | --- |
| CAP loading and bytecode interpretation | ![Supported](https://img.shields.io/badge/supported-brightgreen) | Imports must resolve to API packages present in the firmware ROM mask. |
| Applet lifecycle | ![Supported](https://img.shields.io/badge/supported-brightgreen) | Installation, selection, and deletion. |
| GlobalPlatform card management | ![Supported](https://img.shields.io/badge/supported-brightgreen) | Tested SCP03 authentication, LOAD/INSTALL, SELECT, DELETE, GET DATA, and GET STATUS flows. A physical regression confirms ordinary applet APDUs remain accessible after GPPro disconnects from SCP03. |
| Framework APIs | ![Partial](https://img.shields.io/badge/partial-yellow) | `Util`, `OwnerPIN` (try counter durable before comparison, validated flag in RAM), and implemented `JCSystem` transient-memory and transaction operations. See [JavaCard API coverage](#javacard-api-coverage) for detailed results. |
| Native cryptography | ![Partial](https://img.shields.io/badge/partial-yellow) | AES, DES and RSA ciphers, SHA digests, checksums, random number generation, MACs, digital signatures, and elliptic-curve key agreement. See [JavaCard API coverage for `javacard.security`](#javacard-security-coverage). |
| Garbage collection | ![Supported](https://img.shields.io/badge/supported-brightgreen) | `JCSystem.requestObjectDeletion()` queues reclamation of unreachable persistent objects after the APDU unwinds. Tested on Pico 2 in the external Windows run. Hardware power-loss validation remains open. |
| Persistent objects in flash | ![Partial](https://img.shields.io/badge/partial-yellow) | CRC-protected flash journal; writes limited to what Java Card atomicity requires (single elements, `Util` block updates, transactions). Power-loss acceptance remains open. |
| Cross-applet sharing and firewall | ![Partial](https://img.shields.io/badge/partial-yellow) | Targeted cross-AID sharing, proxy lifecycle, and firewall tests exist. Coordinated cross-context garbage-collection roots/cycles have host-unit coverage; full isolation semantics and device RAM bounds remain unestablished. |
| Optional packages absent from the ROM mask | ![Unsupported](https://img.shields.io/badge/unsupported-red) | Imports such as `javacardx.external` are rejected. |
| Logical channels | ![Unsupported](https://img.shields.io/badge/unsupported-red) | Not implemented. |
| Short APDUs and response chaining | ![Supported](https://img.shields.io/badge/supported-brightgreen) | Standard short command/response path. |
| Extended-length APDUs | ![Unsupported](https://img.shields.io/badge/unsupported-red) | Not implemented. |
| T=1 waiting-time extension | ![Unsupported](https://img.shields.io/badge/unsupported-red) | Not implemented. |

### Current crypto coverage

| Area | Implemented algorithms |
| --- | --- |
| Symmetric ciphers | AES in CBC, ECB, CTR, CFB128 and XTS modes, AES-CCM and AES-GCM through `AEADCipher`, and DES and Triple DES in CBC and ECB modes. |
| Asymmetric ciphers | RSA with PKCS #1 v1.5 or OAEP padding and raw RSA. |
| Signatures and MACs | RSA PKCS #1 v1.5, RSA-PSS, RSA ISO 9796, ECDSA, HMAC with SHA, MD5 or RIPEMD-160, AES MAC and CMAC, and DES MACs. |
| Digests and checksums | SHA-1, SHA-2, SHA-3, and ISO 3309 CRC16 and CRC32. `InitializedMessageDigest` supports SHA-1 and SHA-2 state import. |
| Keys and agreement | AES, DES, Triple DES and HMAC keys, RSA and RSA-CRT key pairs, prime-field EC key pairs, and elliptic-curve Diffie–Hellman variants. |
| Random data | Pseudo-random, secure random and preseeded DRBG, plus hardware TRNG on the Pico 2. |

`ALG_XDH` remains unavailable because its Java Card 3.1 named-parameter key API is outside the current ROM ABI. The legacy RSA ISO14888/ISO9796 **Cipher** IDs and the 192-/256-bit-block AES CBC/ECB and MAC NOPAD IDs are excluded. Standard 128-bit-block AES CBC/ECB and MAC support 192- and 256-bit keys.

### JavaCard API coverage

The table describes current Java Card 3.0.5 API support. The class lists include dependencies referenced by the [API test CAPs](tests/fixtures/external/javacard-api/provided-20261107/caps).

| API package | Included classes and interfaces | 3.0.5 coverage |
| --- | --- | --- |
| `java.rmi` | `Remote` | ![Partial](https://img.shields.io/badge/partial-yellow) Supports the `Remote` interface used by Java Card RMI. |
| `javacard.framework` | `AID`, `APDU`, `APDUException`, `Applet`, `AppletEvent`, `CardException`, `CardRuntimeException`, `ISO7816`, `ISOException`, `JCSystem`, `MultiSelectable`, `OwnerPIN`, `OwnerPINBuilder`, `OwnerPINx`, `OwnerPINxWithPredecrement`, `PIN`, `PINException`, `SensitiveArrays`, `Shareable`, `SystemException`, `TransactionException`, `UserException`, `Util` | ![Partial](https://img.shields.io/badge/partial-yellow) Supports applet lifecycle, short APDUs, transactions, transient memory, PIN operations and object deletion. `SensitiveArrays.makeIntegritySensitiveArray()` and integrity checks on such arrays are unavailable, as are `MultiSelectable.select(boolean)` and `deselect(boolean)` across logical channels. |
| `javacard.framework.service` | `BasicService`, `CardRemoteObject`, `Dispatcher`, `RMIService`, `RemoteService`, `SecurityService`, `Service`, `ServiceException` | ![Supported](https://img.shields.io/badge/supported-brightgreen) Supports service dispatch, security services and RMI service APIs. |
| <a name="javacard-security-coverage"></a>`javacard.security` | `AESKey`, `Checksum`, `CryptoException`, `DESKey`, `DHKey`, `DHPrivateKey`, `DHPublicKey`, `DSAKey`, `DSAPrivateKey`, `DSAPublicKey`, `ECKey`, `ECPrivateKey`, `ECPublicKey`, `HMACKey`, `InitializedMessageDigest`, `InitializedMessageDigest.OneShot`, `Key`, `KeyAgreement`, `KeyBuilder`, `KeyPair`, `KoreanSEEDKey`, `MessageDigest`, `MessageDigest.OneShot`, `PrivateKey`, `PublicKey`, `RSAPrivateCrtKey`, `RSAPrivateKey`, `RSAPublicKey`, `RandomData`, `RandomData.OneShot`, `SecretKey`, `Signature`, `Signature.OneShot`, `SignatureMessageRecovery` | ![Partial](https://img.shields.io/badge/partial-yellow) Supports key management, digests, checksums, random data, signatures and key agreement. `KoreanSEEDKey`, `DSAKey`, `DSAPublicKey`, `DSAPrivateKey`, `DHKey`, `DHPublicKey` and `DHPrivateKey` are unsupported. `HMACKey` accepts an 8-byte key and the signature API implements `ALG_HMAC_RIPEMD160`. `ALG_RSA_SHA_512_PKCS1_PSS` requires an RSA key large enough for the digest and salt. |
| `javacardx.apdu` | `ExtendedLength` | ![Unsupported](https://img.shields.io/badge/unsupported-red) Extended-length APDUs are unavailable. |
| `javacardx.apdu.util` | `APDUUtil` | ![Supported](https://img.shields.io/badge/supported-brightgreen) Supports the `APDUUtil` CLA helpers. |
| `javacardx.biometry` | `BioBuilder`, `BioException`, `BioTemplate`, `OwnerBioTemplate`, `SharedBioTemplate` | ![Unsupported](https://img.shields.io/badge/unsupported-red) This package is unavailable. |
| `javacardx.biometry1ton` | `Bio1toNBuilder`, `Bio1toNException`, `BioMatcher`, `BioTemplateData`, `OwnerBioMatcher`, `OwnerBioTemplateData`, `SharedBioMatcher`, `SharedBioTemplateData` | ![Unsupported](https://img.shields.io/badge/unsupported-red) This package is unavailable. |
| `javacardx.crypto` | `AEADCipher`, `Cipher`, `Cipher.OneShot`, `KeyEncryption` | ![Partial](https://img.shields.io/badge/partial-yellow) Supports AES, DES and RSA through `Cipher` and `Cipher.OneShot`, and `AES-CCM` and `AES-GCM` through `AEADCipher`. `Korean SEED`, the legacy wide-block AES factory IDs and encrypted key import through `KeyEncryption` are unavailable. |
| `javacardx.external` | `ExternalException`, `Memory`, `MemoryAccess` | ![Unsupported](https://img.shields.io/badge/unsupported-red) This package is unavailable. |
| `javacardx.framework` | `ClassicSIOProxy` | ![Partial](https://img.shields.io/badge/partial-yellow) Supports Shareable interface proxies. Full cross-context coverage remains open. |
| `javacardx.framework.math` | `BCDUtil`, `BigNumber`, `ParityBit` | ![Unsupported](https://img.shields.io/badge/unsupported-red) This package is unavailable. |
| `javacardx.framework.string` | `StringException`, `StringUtil` | ![Unsupported](https://img.shields.io/badge/unsupported-red) This package is unavailable. |
| `javacardx.framework.tlv` | `BERTLV`, `BERTag`, `ConstructedBERTLV`, `ConstructedBERTag`, `PrimitiveBERTLV`, `PrimitiveBERTag`, `TLVException` | ![Unsupported](https://img.shields.io/badge/unsupported-red) This package is unavailable. |
| `javacardx.framework.util` | `ArrayLogic`, `UtilException` | ![Unsupported](https://img.shields.io/badge/unsupported-red) This package is unavailable. |
| `javacardx.framework.util.intx` | `JCint` | ![Unsupported](https://img.shields.io/badge/unsupported-red) This package is unavailable. |
| `javacardx.security` | `SensitiveResult` | ![Supported](https://img.shields.io/badge/supported-brightgreen) Supports `SensitiveResult`. |

- [Architecture and layering](docs/FIRMWARE_JCVM_ARCHITECTURE.md)
- [Supported API profile](docs/supported-profile.md)
- [Persistence and filesystem](docs/FILESYSTEM_ABI.md)
- [Active tasks](TODO.md), [progress](progress.md), and [agent handoff](.agent/HANDOFF.md)

## Test organization

1. The [JCAlgTest applet CAP](tests/fixtures/external/jcalgtest/1.8.5) and its pinned host client.
2. The [Java Card API CAP suite](tests/fixtures/external/javacard-api/provided-20261107/caps) generated from the Java Card API, exercised with its own client.
3. Known-answer cryptographic test vectors, including initialization vectors for cipher modes that use them.
4. Custom [unit tests](tests/unit), [integration tests](tests/integration), and [hardware tests](tests/hardware).

See [tests/README.md](tests/README.md) for test commands. Verify supplied fixtures with `./tools/fixture-path --verify`.
