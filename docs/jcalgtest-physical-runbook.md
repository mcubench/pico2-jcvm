# JCAlgTest physical profile comparison runbook

Run these commands from the repository root. This is an example workflow, not a
record of current board state. Identify each board and its installed packages
before flashing or installing a CAP. Every USB/PCSC command below must use the
host-access execution path from `AGENTS.md` (elevated sandbox execution plus
`PICO2_HOST_USB_ACCESS=1`); the marker alone does not grant access. The flash
wrapper builds both architectures before flashing the ARM CHOUPI image.

Replace the example serials and reader names below with the identities from
that authorized discovery. Keep per-board results separate.

`tools/run-jcalgtest` selects the supplied versioned JCAlgTest client and
JCAlgTest 1.8.4 / jc305 baseline through `tests/fixtures/manifest.json`.
The independent API suite client must not be substituted. The versioned client
reports its internal client version as 1.8.3; the applet reports 1.8.4_jc305.

```sh
# Verify the pinned CAP, clients, API corpus and simulator reference.
./tools/fixture-path --verify

# Build and flash CHOUPI with the `base` profile to a Pico 2. It enables the
# minimum crypto needed by JCAlgTest's eager startup allocations, plus all
# OwnerPINBuilder variants so disabled capabilities are reported as `no`.
base_serial='REPLACE_WITH_IDENTIFIED_PICO_SERIAL'
PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile base --serial "$base_serial"

# Save the expected support set before building another profile (the wrapper
# regenerates this file for each selected profile).
mkdir -p artifacts/results/base-alg-support
cp build/javacard-support/jcalgtest-supported.csv \
  artifacts/results/base-alg-support/expected.csv

# Grant pcscd access after USB re-enumeration, then confirm the physical reader.
PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader
PICO2_HOST_USB_ACCESS=1 pcsc_scan -r

# Use the exact Pico reader name printed above.
reader_name='REPLACE_WITH_EXACT_BASE_PCSC_READER_NAME'

# After a separately performed Pico reboot/power cycle, verify the CAP/applet
# and JCSystem APDU are available. The script does not initiate or prove USB
# power removal; its output explicitly leaves that check unverified.
PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup "$reader_name"

# Install the supplied JCAlgTest 1.8.4 JC305 CAP (skip if already installed).
PICO2_HOST_USB_ACCESS=1 ./tools/env java -jar .local-toolchain/globalplatformpro/tool/target/gp.jar \
  --reader "$reader_name" \
  --install tests/fixtures/external/jcalgtest/1.8.4/AlgTest_v1.8.4_jc305.cap

# Retrieve the extended algorithm support report.
PICO2_HOST_USB_ACCESS=1 ./tools/run-jcalgtest --reader-name "$reader_name" \
  -op ALG_SUPPORT_EXTENDED -fresh \
  -outpath artifacts/results/base-alg-support

# Build and flash the all-enabled baseline profile to a second Pico, then run
# the same JCAlgTest operation there for an apples-to-apples report comparison.
baseline_serial='REPLACE_WITH_SECOND_IDENTIFIED_PICO_SERIAL'
PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile baseline --serial "$baseline_serial"
mkdir -p artifacts/results/baseline-alg-support
cp build/javacard-support/jcalgtest-supported.csv \
  artifacts/results/baseline-alg-support/expected.csv
reader_name_baseline='REPLACE_WITH_EXACT_BASELINE_PCSC_READER_NAME'
PICO2_HOST_USB_ACCESS=1 ./tools/run-jcalgtest --reader-name "$reader_name_baseline" \
  -op ALG_SUPPORT_EXTENDED -fresh \
  -outpath artifacts/results/baseline-alg-support

# Compare each report to the same jCardSim reference, with the expected support
# set captured from the matching firmware build.
./tools/compare-jcalgtest-support.py \
  artifacts/results/base-alg-support/*ALGSUPPORT*.csv \
  tests/fixtures/reference/jcalgtest/1.8.4/jcardsim-alg-support-extended.csv \
  --expected-support artifacts/results/base-alg-support/expected.csv
./tools/compare-jcalgtest-support.py \
  artifacts/results/baseline-alg-support/*ALGSUPPORT*.csv \
  tests/fixtures/reference/jcalgtest/1.8.4/jcardsim-alg-support-extended.csv \
  --expected-support artifacts/results/baseline-alg-support/expected.csv

# Each comparison should end with COMPARE:PASS and no status other than yes/no.

# Show which expected capability rows changed between the two profiles.
diff -u artifacts/results/base-alg-support/expected.csv \
  artifacts/results/baseline-alg-support/expected.csv

# Run the static performance suite.
PICO2_HOST_USB_ACCESS=1 ./tools/run-jcalgtest --reader-name "$reader_name" \
  -op ALG_PERFORMANCE_STATIC -fresh \
  -outpath artifacts/results/physical-perf-static
```

Use the provisioned restricted PC/SC helper after each USB
re-enumeration. If it is unavailable, stop physical work and record the access
limitation; autonomous sessions must not run an installer with `sudo`. JCAlgTest records unsupported algorithms as
`NO_SUCH_ALGORITHM`; those rows are expected. Static performance may take
several minutes; the wrapper's default timeout is 900 seconds.
