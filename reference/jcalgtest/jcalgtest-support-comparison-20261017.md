# JCAlgTest v1.8.2 support comparison — initial run 2026-10-17

## Inputs

- Pico 2 CHOUPI ARM, official `ALG_SUPPORT_EXTENDED` CSV:
  `jcalgtest-supported-arm-randomdata-oneshot-fix-20261017/`
- Official jCardSim `ALG_SUPPORT_EXTENDED` reference:
  `../../tests/fixtures/external/jcalgtest/legacy/1.8.2/reference/jcardsim-jcalgtest-v1.8.2-alg-support-extended.csv`
- Both files contain 4,525 algorithm result rows from JCAlgTest 1.8.2 / applet
  1.8.2_jc305.

## Results

| Source | Rows | `yes` | `no` | Other status |
| --- | ---: | ---: | ---: | ---: |
| Pico 2 CHOUPI, after the RandomData.OneShot fix | 4,525 | 15 | 4,510 | 0 |
| jCardSim reference | 4,525 | 1,469 | 2,935 | 121 |

The jCardSim's 121 other statuses are 116 `NullPointerException` and five
`error(ILLEGAL_VALUE)`. Of the algorithm identities with a binary status in
both files, jCardSim says `yes` where the Pico says `no` for 1,454 entries.
Those differences are expected capability differences; the simulator is a
cross-check, not the implementation contract. All 15 Pico `yes` entries also
appear as `yes` in jCardSim.

## Pico results checked against API source

The physical CSV reports these supported entries:

- `MessageDigest.ALG_SHA_256`
- `RandomData.ALG_SECURE_RANDOM`
- `Cipher.ALG_DES_CBC_NOPAD` and `Cipher.ALG_AES_BLOCK_128_CBC_NOPAD`
- the three DES and three AES key types/lengths used by those ciphers
- the old and JC 3.0.4 Cipher factory forms for AES-CBC/NoPadding and
  DES-CBC/NoPadding
- `OwnerPINBuilder.OWNER_PIN`, `OWNER_PIN_X`, and
  `OWNER_PIN_X_WITH_PREDECREMENT`

Source review confirms the digest, random, key, and cipher entries correspond
to implemented factory branches in the Java Card API. It also found two false
positive PIN entries: `OwnerPINBuilder.buildOwnerPIN()` returns `null` for
`OWNER_PIN_X` and `OWNER_PIN_X_WITH_PREDECREMENT`. JCAlgTest 1.8.2 marks any
non-throwing factory call supported and does not check the returned reference.
Those two rows therefore do not establish working PIN implementations.
Implement those documented PIN types, or make the applet/client report their
unsupported status without aborting the sweep, before claiming AC4 complete.

The previous physical run had six
`javacard.security.RandomData.OneShot` rows as `error(ILLEGAL_USE)`. Its
`open()` method is an unsupported factory and the API contract requires
`CryptoException.NO_SUCH_ALGORITHM`; the implementation now throws reason 3.
The new physical CSV records all six as `no` and contains no `error(...)`
rows.

## Follow-up — 2026-10-19

The initial source review found that the two `OwnerPINX` factories returned
`null`, which made their physical `yes` rows false positives. Both variants
have since been implemented; focused VPCD tests exercise their PIN try-limit
and predecrement behavior. The archived Pico CSV predates this implementation,
so AC4 still requires a fresh physical support sweep before it can close.

`tools/compare-jcalgtest-support.py` now validates both CSVs and checks the
CHOUPI `yes` identities against
`tools/jcalgtest-supported-v1.8.2.csv`, the reviewed declaration of the
implemented v1.8.2 support surface. Running it against the archived CSVs
matched all 4,525 `(section, algorithm_name)` rows, with no CHOUPI `error(...)`
statuses and exact agreement on the 15 declared supported entries. jCardSim's
1,454 additional `yes` results where CHOUPI reports `no` are capability
differences, not missing CHOUPI algorithms. This comparator validates the
archived data; it does not replace the post-OwnerPINX physical rerun.

Validation output: `artifacts/logs/jcalgtest-support-compare-20261019.log`.

## Artifact hashes

- Pico CSV:
  `f9c74aaa7d5b3a3e7fc30897a4fa167bd7cb70cb8c79c5ad1e948d9e0aa401bf`
- Pico official transcript:
  `0cb527b6fbd0886febc71e008b4a0f408f9af01f1ce54e3671d00409c563aa57`
- jCardSim reference CSV:
  `67d9feebf9756da69db034bab05cddd9acb38d31aa980888fa7b5db22ef1757b`
- First bounded physical serial capture:
  `9a5000f97f4e3d77ffe22fda70aca365f0fc34e4c0d72f331c0e95a1af5bcc01`
- Second bounded physical serial capture:
  `527578add128fe359e92637bafad9c4a162d25e730603acad6a1d563e1613d77`
