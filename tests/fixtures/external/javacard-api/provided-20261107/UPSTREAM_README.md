# JavaCard API coverage test: run on a card (deployment folder)

Calls every method and constant of the Java Card Classic API (2.2.1 to 3.0.5) on a physical card and reports what works, what is refused the way the specification allows,
and what is wrong. Use it on a card with a possibly incomplete implementation: the test is split into areas, so a card that cannot load one area still gets the others tested.

## Contents

| Path | What |
|---|---|
| `caps/` | the CAP files `AlgTestApi<Area>_<ver>.cap` for the API versions 221, 222, 303, 304, 305 (+ `Multi` and `Ext` scenario CAPs) |
| `gp.jar` | GlobalPlatformPro: uploads (`--install`), removes (`--uninstall`) the CAPs |
| `client/` | the test client `AlgTestJClient.jar` with its `lib/` (keep the folder together) |
| `api_card_runner.py` | the runner: for every area uploads the CAP, runs the client, removes the CAP, writes ONE csv |
| `run_api_test.sh`, `run_api_test.bat` | the runner with the paths of this folder (pass the options below to it) |
| `tools/`, `make_table.sh` | builds an interactive html table of the csv files in `results/` |
| `results/` | put the result csv files here (also `atr_names.csv`: card names by ATR for the table; add unknown cards to `atr_names_local.csv`) |

Needs: Java (`java` on the PATH, 8 or newer), Python 3.8+, a PC/SC reader with the card (Linux: `pcscd` running, `pcsc_scan -r` shows the reader; Windows/macOS: the system PC/SC).
No build tools are needed.

## 0. Safety

* Use a **test card**. The test installs applets, creates persistent objects (keys, PINs) and runs about 2600 APDUs. Cards without object deletion can lose EEPROM until the applets are deleted.
* The runner uses the **default GlobalPlatform keys**. If your card has other keys add `--gp-args "-d --key <key>"`. Several failed authentications can block a card, so first check
  `java -jar gp.jar --info` (read-only) works.
* Nothing touches other applets or the card manager. MIFARE access is only tried with a wrong key.
* 2048-bit RSA key generation makes some readers drop the card: see "reader drops the card" below.

## 1. Check the setup (no card needed)

    ./run_api_test.sh --list --api 304        # shows the CAP files, gp.jar and the client that were found

Several readers: pass the exact name from `pcsc_scan -r` with `--reader-name "<reader name>"`. The runner pins both GlobalPlatformPro and the Java client to that reader and resolves the Java index from SmartcardIO, including empty virtual terminals in its count. To distribute the API areas over three physical Pico readers, run `./tools/run-deploy-parallel --api 305` from the repository root. It requires three live PC/SC readers by default and writes a separate CSV and log per worker; use `--min-devices 1` to allow fewer. It excludes the known RSA-2048 keygen reader-drop case by default; pass `--exclude ''` to include it. A Pico in BOOTSEL is not a PC/SC card and cannot take a card test until it boots CHOUPI.
Linux, Java cannot see the reader: add `--pcsc-lib /usr/lib/x86_64-linux-gnu/libpcsclite.so.1`.
Give every run its own csv: `--out results/<card>_<stage>.csv --cardname <card>`.

## 2. Remove leftovers of earlier runs

The runner removes each CAP after its area, but a crash can leave one on the card (then the next upload fails or the card has no space). Remove all test CAPs of the version(s) used before;
errors for CAPs that are not installed are normal:

    for c in caps/AlgTestApi*_304.cap caps/AlgTestApi*_305.cap; do java -jar gp.jar -d --uninstall "$c"; done     # add -r "<reader>" for several readers

(Windows: `for %c in (caps\AlgTestApi*_304.cap caps\AlgTestApi*_305.cap) do java -jar gp.jar -d --uninstall %c`.)

## 3. Order of the areas (most important first)

Run the areas in stages. A stage that fails does not hide the others, and what blocks a card shows up where it happens.

| Stage | Area / CAP | Why this position | If the CAP is refused at LOAD (SW 6A80 / 6985) |
|---|---|---|---|
| 1 | **Core** (`AlgTestApiCore_<v>`, plus the Multi and Ext CAPs for the scenarios) | `javacard.framework` is the base of every applet: Applet lifecycle, APDU, Util, JCSystem, AID, PIN/OwnerPIN, exceptions, ISO7816. Decides the **API version**. | the card does not accept that API version: the runner tries the next lower one; if none loads the card cannot be tested |
| 2 | **Security** (`AlgTestApiSecurity_<v>`) | keys, KeyBuilder, KeyPair, MessageDigest, RandomData, Checksum: the base of everything cryptographic | an imported package is missing on the card |
| 3 | **Crypto** (`AlgTestApiCrypto_<v>`) | Signature, Cipher, KeyAgreement, AEAD: slowest area, most chance to crash the card, so after the cheap areas | same |
| 4 | **JavacardxExt** (`AlgTestApiJavacardxExt_<v>`, 4 KB) | imports only `javacardx.external`, so it loads even without the framework packages | `javacardx.external` is absent |
| 5 | **Javacardx** (`AlgTestApiJavacardx_<v>`) | `javacardx.framework` util / math / string / tlv, `javacardx.security`, `javacardx.apdu.util` | a framework package is absent: on a 3.0.5 card run **JavacardxSec** (`AlgTestApiJavacardxSec_305`, 84 calls, no framework import) instead |
| 6 | **JavacardxInt** (`AlgTestApiJavacardxInt_<v>`) | needs `int` support in the card | cards without int cannot load it: expected |
| 7 | **Service** (`AlgTestApiService_<v>`) | `javacard.framework.service` (imports `java.rmi`): rarely present | package absent |
| 8 | **Biometry** (`AlgTestApiBiometry_<v>`) | `javacardx.biometry`: rarely present | package absent |

A refusal at LOAD is information (the card has no such package), not a failure of the card. Fix first what is missing from the packages that a normal applet needs (stages 1-3).

**API version:** `--api auto` (default) loads the Core CAP of `305, 304, 303, 222, 221` in turn and keeps the first the card accepts. Force one with `--api 304`. A card that loads 3.0.4 but refuses 3.0.5 is normal;
members that exist only in 3.0.5 then show "not in CAP API version" (not called).

## 4. Commands, stage by stage

    ./run_api_test.sh --cardname <card> --areas Core                                                    --out results/<card>_1_core.csv
    ./run_api_test.sh --cardname <card> --areas Security                                                --out results/<card>_2_security.csv
    ./run_api_test.sh --cardname <card> --areas Crypto --exclude "genKeyPair.*LENGTH_RSA_2048"          --out results/<card>_3_crypto.csv
    ./run_api_test.sh --cardname <card> --areas JavacardxExt                                            --out results/<card>_4_ext.csv
    ./run_api_test.sh --cardname <card> --areas Javacardx                                               --out results/<card>_5_javacardx.csv   # 3.0.5 card without framework packages: --areas JavacardxSec
    ./run_api_test.sh --cardname <card> --areas JavacardxInt                                            --out results/<card>_6_int.csv
    ./run_api_test.sh --cardname <card> --areas Service,Biometry                                        --out results/<card>_7_service_bio.csv

(Windows: `run_api_test.bat` with the same options.)

* **Core** also installs the Multi CAP (two applets; needs logical channels) and the Ext CAP (extended-length APDUs, not on T=0) for the scenarios; `--no-scenarios` skips them.
* The runner writes after every call, so a partial csv survives a card loss. After 5 consecutive card failures an area is aborted (`TEST_ABORTED_CARD_FAILURES`).
* **Reader drops the card at one call:** note the last `CALL` row of the csv, re-plug the reader, and rerun the area with `--exclude "<regex on 'class; method; argument'>"` for that call. Excluded calls end as
  `SKIPPED_BY_USER` (support unknown); run them separately at the end.
* Other options: `--keep` (leave the CAPs on the card), `--retries` / `--pause` (reconnect attempts, default 4 x 20 s), `--gp-timeout` (240 s per gp.jar call), `--test-timeout` (3600 s per area),
  `--verbose`, `--dump-dir <dir>` (raw gp.jar output of unclassified failures, key-free).

## 5. Reading the result

One csv per run (delimiter `;`). `STAGE` rows say what happened to each step (`stage_result` OK / FAILED / SKIPPED, with a `failure_type`):

| failure_type | Meaning |
|---|---|
| `UPLOAD_REFUSED_LOAD` | the card lacks an imported package or the API version (6A80, 6985) |
| `UPLOAD_REFUSED_INSTALL`, `UPLOAD_NO_SPACE`, `UPLOAD_AUTH` | install refused, no EEPROM, wrong keys |
| `UPLOAD_NO_CONNECT`, `TEST_NO_APPLET`, `TEST_TIMEOUT`, `TEST_CLIENT_ERROR`, `TEST_ABORTED_CARD_FAILURES` | reader or card lost, no answer, client problem, card stopped answering |
| `CLEANUP_*` | the CAP could not be removed: use section 2 before the next run |

`CALL` rows have one status per API call: `OK`, `UNSUPPORTED_PER_SPEC` (allowed "not implemented"), `NOT_IN_CAP_API_VERSION` (not called), `WRONG_RESULT`, `EXC_CARDRUNTIME`, `EXC_RUNTIME`, `SW_ERROR`,
`CARD_ERROR`, `NO_MEMORY`, `SKIPPED_BY_USER`, `APPLET_NOT_INSTALLED`.

Interactive table of all csv files (colours and symbols, filters, explanation of every result on hover; the card name comes from its ATR via `results/atr_names*.csv`):

    ./make_table.sh                  # -> results/api_table.html

Summary per area and status of the csv files of one card:

    python3 - <<'X'
    import csv,glob,collections
    c=collections.defaultdict(collections.Counter)
    for f in glob.glob('results/<card>_*.csv'):
        for r in csv.DictReader(open(f,encoding='utf-8'),delimiter=';'):
            if r['row_type']=='CALL': c[r['area']][r['status']]+=1
    for a,s in c.items(): print(a,dict(s))
    X

## 6. What to fix first (priority for an incomplete implementation)

1. **Load / install problems of Core, Security, Crypto** (`UPLOAD_REFUSED_*`, `TEST_NO_APPLET`): nothing else can be judged until these work.
2. **Stability**: `CARD_ERROR`, `SW_ERROR`, `TEST_ABORTED_CARD_FAILURES`, a reader that drops at one call. A call that crashes or resets the card is the most serious defect; the last `CALL` row before the loss names it.
3. **`WRONG_RESULT` and `EXC_*` in Core** (APDU, Util, JCSystem, Applet lifecycle, PIN, exceptions): applets depend on them.
4. **`WRONG_RESULT` / `EXC_*` in Security and Crypto** (known-answer results, lengths, exception reasons).
5. **Optional features** (`UNSUPPORTED_PER_SPEC`, missing javacardx / service / biometry packages): the specification allows these; fix only what your use needs.
6. `NOT_IN_CAP_API_VERSION` and `SKIPPED_BY_USER` are not findings: another API version or a rerun covers them.

## Notes on this build

* The CAPs for API 304 were built from the current sources. The Crypto CAPs of 221, 222, 303 and 305 were built before the last source change (a diagnostic variant of the RSA PKCS1
  pre-computed hash test); they run everything else identically. Rebuild all from the repository with `AlgTest_JavaCard/api_dev.sh caps`.
* `client/AlgTestJClient.jar` was compiled from the repository sources with `javac -source 8 -target 8` (the repository uses `ant jar`; the class set is the same except one synthetic inner class of `CardMngr`).
* Background and the full description: `docs/API_COVERAGE.md` in the repository.
