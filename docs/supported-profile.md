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
available storage and supported imports.

Cryptographic capabilities are selected by `config/javacard-support.json`.
The `max_crypto` profile combines the current, symmetric, asymmetric, and
AES-CTR profiles. Its generated manifest is authoritative for the selected
JCAlgTest capabilities; it does not imply support for every algorithm defined
by Java Card.

## Known exclusions and limits

- Extended-length APDUs and T=1 waiting-time extension are not implemented.
- Logical channels and general multi-applet contexts/firewall semantics are
  not implemented.
- `JCSystem.requestObjectDeletion()` is unsupported; persistent-object
  reclamation remains disabled pending safe power-loss recovery validation.
- Optional API packages absent from the ROM mask, including
  `javacardx.external`, are rejected when CAP imports are resolved.
- The optional `max_crypto` profile does not enable RSA or RSA-CRT 4096-bit
  key-pair generation. The upstream JCAlgTest 1.8.2 CAP mislabels its 3072-bit
  generation probes as 4096-bit requests; this profile does not alter the CAP
  or claim those requests as supported.

## Evidence and remaining work

Current-source VPCD runs cover the JCAlgTest 1.8.2 applet path and the
unchanged Java Card 3.0.5 Core, Security, Crypto, Service, and
`JavacardxSec` deploy CAPs. Physical Pico 2 results are recorded in
`TODO.md`, `.agent/QKB.md`, and `progress.md`; they are not interchangeable
with VPCD results. The full API contract still needs broader per-method tests,
and acceptance still needs repeatable clean-flash runs, Windows PC/SC
validation, and actual USB power-cycle recovery.
