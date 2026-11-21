# Declared Java Card profile

## Compatibility target

CHOUPI targets the Java Card Classic 3.0.5 API and accepts CAP 2.1 and CAP 2.2
input formats where their components are implemented by the loader. The Pico 2
executes native JCVM code that interprets CAP bytecode. Java source and Java
`.class` files are build inputs for generating the host-side ROM mask; they are
not firmware programs and are never executed by the card.

This is a compatibility target, not a claim of complete Java Card 3.0.5
specification conformance. Applets must be converted against the API packages
present in the ROM mask, and any dynamically loaded package must have all of
its imports resolved at installation.

## Runtime surface

The currently exercised applet path includes the Java Card framework and
applet lifecycle, short APDUs and response chaining, `Util`, the implemented
`JCSystem` transient-memory and transaction operations, Security key classes,
and native `javacardx.crypto` operations. GlobalPlatform card management
supports the tested SCP03, CAP LOAD/INSTALL, SELECT, DELETE, GET DATA, and GET
STATUS flows. Package and applet AIDs can be loaded dynamically, subject to
available storage and supported imports. SCP03 supports S8 with an
eight-byte challenge and C-MAC security level. Command encryption and
response MAC are outside this subset. The card manager uses the well-known
test key. This project is for educational use.

Cryptographic capabilities are selected by `config/javacard-support.json`.
The `max_crypto` profile combines the baseline, symmetric, asymmetric, and
AES-CTR profiles. Its generated manifest is authoritative for the selected
JCAlgTest capabilities; it does not imply support for every algorithm defined
by Java Card.

The RP2350 max_crypto build exposes `RandomData.ALG_TRNG` through both
RandomData factories. The PC simulator reports this algorithm unavailable
because it has no RP2350 entropy peripheral.

## Known exclusions and limits

- Extended-length APDUs and T=1 waiting-time extension are not implemented.
- Logical channels are out of scope for the current firmware. The runtime
  implements retained applet contexts, owner-aware references and SIO
  dispatch. Focused VPCD and physical probes cover portions of these paths.
  General firewall enforcement and broader hostile multi-applet acceptance
  remain open under INV-19.
- Normal builds enable `JCSystem.requestObjectDeletion()`. The completed
  Linux physical option 1 and 2 campaigns satisfy the MEM-01 T8 long-run gate.
  Mixed-owner reachability and physical interruption recovery remain open.
- Buffered multipart `Signature` operations have a cumulative 32,767-byte
  message limit and require transient workspace for the accumulated message.
- Optional API packages absent from the ROM mask, including
  `javacardx.external`, are rejected when CAP imports are resolved.
- The asymmetric and maximum crypto profiles enable RSA and RSA-CRT
  4096-bit key-pair generation. Historical JCAlgTest runs sent length 4096
  under the 3072 label and length 64 under the 4096 label. Use the actual
  requested key length when interpreting those rows. Physical 4096-bit validation
  is deferred.
- The max crypto profile selects the Java Card 3.1 AES-CFB and AES-XTS
  identifiers through the existing Cipher factories. XTS accepts concatenated
  256- or 512-bit keys and buffers one data unit of 16 to 32,767 bytes for
  ciphertext stealing at finalization. These modes have simulator vector
  coverage. Physical algorithm validation is pending.
- `MessageDigest` and `InitializedMessageDigest` stream input directly from
  applet byte arrays across calls. Per-call offsets and lengths are Java Card
  `short` values. The cumulative message, including any initialized prefix,
  must be shorter than 2^64 bits for SHA-1/224/256 or 2^128 bits for
  SHA-384/512. State lives in a fixed-size transient buffer and clears on
  `reset()`, successful `doFinal()`, and card reset. Incremental SHA-256
  uses Mbed TLS so each instance can keep independent state across APDUs.

## Evidence and remaining work

New JCAlgTest runs use the unchanged 1.8.5/jc305 CAP and the supplied
1.8.5-20261121 host client, identified separately by SHA-256. The
[physical runbook](jcalgtest-physical-runbook.md) records how to select them.

The completed Linux campaigns apply to their recorded firmware and input
hashes. The active backlog tracks clean-card API coverage, physical recovery
and the Windows `v0.2-rc1` campaign. [Product invariants](../SPEC.md) ·
[Backlog](../TODO.md) · [Progress log](../progress.md)
