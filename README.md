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

See the [physical test runbook](docs/jcalgtest-physical-runbook.md) and [Windows instructions](docs/windows-asymm-symm-test.md). Agents accessing hardware must follow [AGENTS.md](AGENTS.md).

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

| Area | Implementation and validation |
| --- | --- |
| Digests and checksums | Streaming SHA3-224/256/384/512, including `MessageDigest.OneShot`, and ISO 3309 CRC16/CRC32 passed simulator vectors. SHA-1 and SHA-2 state import remain available through `InitializedMessageDigest`. |
| AES ciphers | AES-CFB128, AES-XTS, AES-CCM, and AES-GCM passed simulator vectors. CCM and GCM use `AEADCipher` for tags and associated data. XTS accepts two AES-128 or two AES-256 keys and buffers one data unit of up to 32,767 bytes. Full physical vector validation remains open. |
| MACs | DES MAC4 NOPAD passed simulator vectors. The 128-bit-block AES MAC and CMAC accept AES-128/192/256 keys, with physical coverage for all three widths. |
| RandomData | Pseudo-random, secure random, and preseeded DRBG passed simulator and focused physical API checks. Both TRNG factories passed on the RP2350. TRNG is hardware-only. |
| EC and RSA keys | `max_crypto` enables 112-bit prime-field EC private key storage. On-card RSA and RSA-CRT 3072-bit generation passed focused physical checks. Both 4096-bit generation modes passed simulator tests. Focused physical 4096-bit generation remains deferred. |

`ALG_XDH` remains unavailable because its Java Card 3.1 named-parameter key API is outside the current ROM ABI. The legacy RSA ISO14888/ISO9796 **Cipher** IDs and the 192-/256-bit-block AES CBC/ECB and MAC NOPAD IDs are excluded. Standard 128-bit-block AES CBC/ECB and MAC support 192- and 256-bit keys.

### JavaCard API coverage

The table groups the API classes and interfaces referenced by the [supplied 3.0.5 CAPs](tests/fixtures/external/javacard-api/provided-20261107/caps). Badges reflect the November 21 `d7e90a18` `max_crypto` VM simulator rerun. Core results use the Pico 2 flash overlay with object deletion enabled. Class lists include fixture dependencies. See the [progress records](progress.md#2026-11-21-rerun-the-supplied-java-card-305-api-suite-on-d7e90a18) for case-level results.

| API package | Included classes and interfaces | 3.0.5 coverage |
| --- | --- | --- |
| `java.rmi` | `Remote` | ![Partial](https://img.shields.io/badge/partial-yellow) The Service suite exercised RMI through `Remote`. |
| `javacard.framework` | `AID`, `APDU`, `APDUException`, `Applet`, `AppletEvent`, `CardException`, `CardRuntimeException`, `ISO7816`, `ISOException`, `JCSystem`, `MultiSelectable`, `OwnerPIN`, `OwnerPINBuilder`, `OwnerPINx`, `OwnerPINxWithPredecrement`, `PIN`, `PINException`, `SensitiveArrays`, `Shareable`, `SystemException`, `TransactionException`, `UserException`, `Util` | ![Partial](https://img.shields.io/badge/partial-yellow) 273 of 291 Core and scenario cases passed, including `isObjectDeletionSupported()` and `requestObjectDeletion()`. Unsupported cases were `SensitiveArrays.makeIntegritySensitiveArray()`, `isIntegritySensitive()`, `assertIntegrity()` and `clearArray()` on integrity-sensitive arrays, and `MultiSelectable.select(boolean)` and `deselect(boolean)` across logical channels. Four extended-length cases were blocked at installation. |
| `javacard.framework.service` | `BasicService`, `CardRemoteObject`, `Dispatcher`, `RMIService`, `RemoteService`, `SecurityService`, `Service`, `ServiceException` | ![Supported](https://img.shields.io/badge/supported-brightgreen) 124 of 124 Service cases passed. |
| <a name="javacard-security-coverage"></a>`javacard.security` | `AESKey`, `Checksum`, `CryptoException`, `DESKey`, `DHKey`, `DHPrivateKey`, `DHPublicKey`, `DSAKey`, `DSAPrivateKey`, `DSAPublicKey`, `ECKey`, `ECPrivateKey`, `ECPublicKey`, `HMACKey`, `InitializedMessageDigest`, `InitializedMessageDigest.OneShot`, `Key`, `KeyAgreement`, `KeyBuilder`, `KeyPair`, `KoreanSEEDKey`, `MessageDigest`, `MessageDigest.OneShot`, `PrivateKey`, `PublicKey`, `RSAPrivateCrtKey`, `RSAPrivateKey`, `RSAPublicKey`, `RandomData`, `RandomData.OneShot`, `SecretKey`, `Signature`, `Signature.OneShot`, `SignatureMessageRecovery` | ![Partial](https://img.shields.io/badge/partial-yellow) 491 of 901 Security cases passed. All tested `KoreanSEEDKey`, DSA key and DH key cases were unsupported. On the historical test image, 8-byte `HMACKey` set/get and the `ALG_HMAC_RIPEMD160` and SHA-512 RSA-PSS signature probes raised exceptions. |
| `javacardx.apdu` | `ExtendedLength` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The extended-length fixture was refused at INSTALL (`6A88`). |
| `javacardx.apdu.util` | `APDUUtil` | ![Supported](https://img.shields.io/badge/supported-brightgreen) 62 of 62 `APDUUtil` cases passed. |
| `javacardx.biometry` | `BioBuilder`, `BioException`, `BioTemplate`, `OwnerBioTemplate`, `SharedBioTemplate` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The Biometry fixture was refused at INSTALL (`6A88`). |
| `javacardx.biometry1ton` | `Bio1toNBuilder`, `Bio1toNException`, `BioMatcher`, `BioTemplateData`, `OwnerBioMatcher`, `OwnerBioTemplateData`, `SharedBioMatcher`, `SharedBioTemplateData` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The Biometry fixture was refused at INSTALL (`6A88`). |
| `javacardx.crypto` | `AEADCipher`, `Cipher`, `Cipher.OneShot`, `KeyEncryption` | ![Partial](https://img.shields.io/badge/partial-yellow) 110 of 172 cases passed, including all five `Cipher.OneShot` cases. On that historical image, unsupported cases covered AES-CCM/GCM, Korean SEED CBC/ECB, legacy AES-192/256 CBC/ECB no-padding IDs, AES-CBC with `PAD_NULL`, DES-CBC ISO 9797-1 methods 1/2 algorithm 3 padding, DES-CBC and RSA-PKCS1 with external access, RSA ISO 14888/9796, RSA-PSS, RFC 2409 and OAEP with SHA-224/256/384/512 or SHA-3, and all three `KeyEncryption` cases. `Cipher.doFinal()` without data returned zero where the probe expected an exception. |
| `javacardx.external` | `ExternalException`, `Memory`, `MemoryAccess` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The external-memory fixture was refused at INSTALL (`6A88`). |
| `javacardx.framework` | `ClassicSIOProxy` | ![Partial](https://img.shields.io/badge/partial-yellow) The Core Shareable sub-interface case passed. Full cross-context coverage remains open. |
| `javacardx.framework.math` | `BCDUtil`, `BigNumber`, `ParityBit` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The full extension fixture was refused at INSTALL (`6A88`). |
| `javacardx.framework.string` | `StringException`, `StringUtil` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The full extension fixture was refused at INSTALL (`6A88`). |
| `javacardx.framework.tlv` | `BERTLV`, `BERTag`, `ConstructedBERTLV`, `ConstructedBERTag`, `PrimitiveBERTLV`, `PrimitiveBERTag`, `TLVException` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The full extension fixture was refused at INSTALL (`6A88`). |
| `javacardx.framework.util` | `ArrayLogic`, `UtilException` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The full extension fixture was refused at INSTALL (`6A88`). |
| `javacardx.framework.util.intx` | `JCint` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The integer extension fixture was refused at INSTALL (`6A88`). |
| `javacardx.security` | `SensitiveResult` | ![Supported](https://img.shields.io/badge/supported-brightgreen) 22 of 22 `SensitiveResult` cases passed. |

- [Architecture and layering](docs/FIRMWARE_JCVM_ARCHITECTURE.md)
- [Supported API profile](docs/supported-profile.md)
- [Persistence and filesystem](docs/FILESYSTEM_ABI.md)
- [Active tasks](TODO.md), [progress](progress.md), and [agent handoff](.agent/HANDOFF.md)

## Test organization

See [tests/README.md](tests/README.md) for unit/integration/hardware tests and immutable external fixtures. Verify supplied inputs with `./tools/fixture-path --verify`. JCAlgTest (1.8.5/jc305 CAP, 1.8.5-20261121 client) and the independent API CAP suite have separate pinned clients; historical 1.8.2 evidence is not current acceptance.
