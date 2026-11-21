# Pico 2 Java Card VM

## 1. Educational project — read first

> [!WARNING]
> This project is **NOT production-ready. Do not use it for security-critical tasks.** It was almost completely coded by large language models, mostly GPT-6 Sol and Luna.
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

You need a Raspberry Pi Pico 2, a USB data cable, Java, PC/SC support, and these downloads:

- [JCAlgTest CAP files — latest release](https://github.com/crocs-muni/JCAlgTest/releases/latest): the repository acceptance baseline is the supplied **v1.8.5, Java Card Classic 3.0.5 (`jc305`)** CAP, pinned in `tests/fixtures/manifest.json`; run it with the v1.8.5 host client.
- [JCAlgTest PC client — latest release](https://github.com/crocs-muni/JCAlgTest/releases/latest): use client **v1.8.5** (`AlgTestJClient_v1.8.5.jar`, pinned in `tests/fixtures/external/jcalgtest/1.8.5/client/`). Older clients do not send the card-reset/object-deletion APDU correctly, so long runs exhaust card memory.
- [GlobalPlatformPro `gp.jar` binary](https://github.com/martinpaljak/GlobalPlatformPro/releases/latest/download/gp.jar).

1. Download [pico2-jcvm-max_crypto-arm__latest.uf2](https://github.com/mcubench/pico2-jcvm/releases), or build an ARM JCVM **`.uf2`** image below.
2. Hold **BOOTSEL** while connecting the Pico 2. Copy the image onto the **RPI-RP2** drive; the board reboots as a USB smart-card reader. An **empty-card image erases installed applets and persistent data**.
3. Install the unchanged upstream JCAlgTest v1.8.5 applet:

   ```sh
   java -jar gp.jar --reader "<Pico reader name>" \
     --install "<downloaded JC305 CAP file>"
   ```

4. Run the v1.8.5 client and select the Pico reader when prompted:

   ```sh
   java -jar AlgTestJClient_v1.8.5.jar \
     -op ALG_SUPPORT_EXTENDED -fresh -outpath results
   ```

Results are written under `results/`. Unsupported algorithms are expected; a support report does not establish security or full Java Card conformance.

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
| Short APDUs and response chaining | ![Supported](https://img.shields.io/badge/supported-brightgreen) | Standard short command/response path. |
| Persistent objects in flash | ![Partial](https://img.shields.io/badge/partial-yellow) | CRC-protected flash journal; writes limited to what Java Card atomicity requires (single elements, `Util` block updates, transactions). Power-loss acceptance remains open. |
| Framework APIs | ![Partial](https://img.shields.io/badge/partial-yellow) | `Util`, `OwnerPIN` (try counter durable before comparison, validated flag in RAM), and implemented `JCSystem` transient-memory and transaction operations. See [JavaCard API coverage](#javacard-api-coverage) for detailed results. |
| Native cryptography | ![Partial](https://img.shields.io/badge/partial-yellow) | Profile-dependent keys, digests, random numbers, ciphers, signatures/MACs, and key agreement. `max_crypto` declares **171 of 171** capability flags enabled, including AES, DES/3DES, RSA, elliptic-curve operations, SHA digests, and HMAC variants; `InitializedMessageDigest` and all five 3.0.5 `OneShot` classes (RAM-only). The default profile is `baseline`; profile flags do not establish full algorithm acceptance. |
| GlobalPlatform card management | ![Supported](https://img.shields.io/badge/supported-brightgreen) | Tested SCP03 authentication, LOAD/INSTALL, SELECT, DELETE, GET DATA, and GET STATUS flows. A physical regression confirms ordinary applet APDUs remain accessible after GPPro disconnects from SCP03. |
| Cross-applet sharing and firewall | ![Partial](https://img.shields.io/badge/partial-yellow) | Targeted cross-AID sharing, proxy lifecycle, and firewall tests exist. Coordinated cross-context garbage-collection roots/cycles have host-unit coverage; full isolation semantics and device RAM bounds remain unestablished. |
| MessageDigest input | ![Implemented](https://img.shields.io/badge/implemented-blue) | Streams across `update()` and `doFinal()` up to the selected SHA algorithm’s message-length limit. Each call reads directly from the applet array within Java Card’s array and `short` bounds. |
| Extended-length APDUs | ![Unsupported](https://img.shields.io/badge/unsupported-red) | Not implemented. |
| T=1 waiting-time extension | ![Unsupported](https://img.shields.io/badge/unsupported-red) | Not implemented. |
| Logical channels | ![Unsupported](https://img.shields.io/badge/unsupported-red) | Not implemented. |
| `JCSystem.requestObjectDeletion()` | ![Supported](https://img.shields.io/badge/supported-brightgreen) | Enabled in the normal build. Queues a reclamation pass after the APDU unwinds. |
| Persistent-object reclamation | ![Supported](https://img.shields.io/badge/supported-brightgreen) | Reclaims unreachable persistent objects. Exercised on a Pico 2 in the external Windows run. Hardware power-loss validation remains open. |
| Optional packages absent from the ROM mask | ![Unsupported](https://img.shields.io/badge/unsupported-red) | Imports such as `javacardx.external` are rejected. |
| RSA/RSA-CRT 4096-bit key-pair generation | ![Implemented](https://img.shields.io/badge/implemented-blue) | Enabled in `asymm_crypto` and `max_crypto`. Physical validation pending. |

### JavaCard API coverage

The table groups the API classes and interfaces referenced by the [supplied 3.0.5 CAPs](tests/fixtures/external/javacard-api/provided-20261107/caps). Badges reflect the November 21 `d7e90a18` `max_crypto` VM simulator rerun. Core results use the Pico 2 flash overlay with object deletion enabled. Class lists include fixture dependencies. See the [progress records](progress.md#2026-11-21-rerun-the-supplied-java-card-305-api-suite-on-d7e90a18) for case-level results.

| API package | Included classes and interfaces | 3.0.5 coverage |
| --- | --- | --- |
| `java.rmi` | `Remote` | ![Partial](https://img.shields.io/badge/partial-yellow) The Service suite exercised RMI through `Remote`. |
| `javacard.framework` | `AID`, `APDU`, `APDUException`, `Applet`, `AppletEvent`, `CardException`, `CardRuntimeException`, `ISO7816`, `ISOException`, `JCSystem`, `MultiSelectable`, `OwnerPIN`, `OwnerPINBuilder`, `OwnerPINx`, `OwnerPINxWithPredecrement`, `PIN`, `PINException`, `SensitiveArrays`, `Shareable`, `SystemException`, `TransactionException`, `UserException`, `Util` | ![Partial](https://img.shields.io/badge/partial-yellow) 273 of 291 Core and scenario cases passed, including `isObjectDeletionSupported()` and `requestObjectDeletion()`. Unsupported cases were `SensitiveArrays.makeIntegritySensitiveArray()`, `isIntegritySensitive()`, `assertIntegrity()` and `clearArray()` on integrity-sensitive arrays, and `MultiSelectable.select(boolean)` and `deselect(boolean)` across logical channels. Four extended-length cases were blocked at installation. |
| `javacard.framework.service` | `BasicService`, `CardRemoteObject`, `Dispatcher`, `RMIService`, `RemoteService`, `SecurityService`, `Service`, `ServiceException` | ![Supported](https://img.shields.io/badge/supported-brightgreen) 124 of 124 Service cases passed. |
| `javacard.security` | `AESKey`, `Checksum`, `CryptoException`, `DESKey`, `DHKey`, `DHPrivateKey`, `DHPublicKey`, `DSAKey`, `DSAPrivateKey`, `DSAPublicKey`, `ECKey`, `ECPrivateKey`, `ECPublicKey`, `HMACKey`, `InitializedMessageDigest`, `InitializedMessageDigest.OneShot`, `Key`, `KeyAgreement`, `KeyBuilder`, `KeyPair`, `KoreanSEEDKey`, `MessageDigest`, `MessageDigest.OneShot`, `PrivateKey`, `PublicKey`, `RSAPrivateCrtKey`, `RSAPrivateKey`, `RSAPublicKey`, `RandomData`, `RandomData.OneShot`, `SecretKey`, `Signature`, `Signature.OneShot`, `SignatureMessageRecovery` | ![Partial](https://img.shields.io/badge/partial-yellow) 491 of 901 Security cases passed. All tested `KoreanSEEDKey`, DSA key and DH key cases were unsupported. On this revision, 8-byte `HMACKey` set/get and the `ALG_HMAC_RIPEMD160` and SHA-512 RSA-PSS signature probes raised exceptions. |
| `javacardx.apdu` | `ExtendedLength` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The extended-length fixture was refused at INSTALL (`6A88`). |
| `javacardx.apdu.util` | `APDUUtil` | ![Supported](https://img.shields.io/badge/supported-brightgreen) 62 of 62 `APDUUtil` cases passed. |
| `javacardx.biometry` | `BioBuilder`, `BioException`, `BioTemplate`, `OwnerBioTemplate`, `SharedBioTemplate` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The Biometry fixture was refused at INSTALL (`6A88`). |
| `javacardx.biometry1ton` | `Bio1toNBuilder`, `Bio1toNException`, `BioMatcher`, `BioTemplateData`, `OwnerBioMatcher`, `OwnerBioTemplateData`, `SharedBioMatcher`, `SharedBioTemplateData` | ![Unsupported](https://img.shields.io/badge/unsupported-red) The Biometry fixture was refused at INSTALL (`6A88`). |
| `javacardx.crypto` | `AEADCipher`, `Cipher`, `Cipher.OneShot`, `KeyEncryption` | ![Partial](https://img.shields.io/badge/partial-yellow) 110 of 172 cases passed, including all five `Cipher.OneShot` cases. Unsupported cases covered AES-CCM/GCM, Korean SEED CBC/ECB, legacy AES-192/256 CBC/ECB no-padding IDs, AES-CBC with `PAD_NULL`, DES-CBC ISO 9797-1 methods 1/2 algorithm 3 padding, DES-CBC and RSA-PKCS1 with external access, RSA ISO 14888/9796, RSA-PSS, RFC 2409 and OAEP with SHA-224/256/384/512 or SHA-3, and all three `KeyEncryption` cases. `Cipher.doFinal()` without data returned zero where the probe expected an exception. |
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

See [tests/README.md](tests/README.md) for unit/integration/hardware tests and immutable external fixtures. Verify supplied inputs with `./tools/fixture-path --verify`. JCAlgTest (1.8.5 CAP, 1.8.5 client) and the independent API CAP suite have separate pinned clients; historical 1.8.2 evidence is not current acceptance.
