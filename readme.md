# Pico 2 JCVM + JCAlgTest

Run these commands from the repository root. The flash wrapper builds both
architectures as a safety gate and flashes the ARM CHOUPI image.

```sh
# One-time: extract the official JCAlgTest 1.8.2 client (archive already downloaded).
mkdir -p build/algtest-client-1.8.2
unzip -o build/downloads/AlgTest_dist_1.8.2.zip AlgTestJClient.jar \
  -d build/algtest-client-1.8.2

# Build and flash CHOUPI with the `base` profile to a Pico 2. It enables the
# minimum crypto needed by JCAlgTest's eager startup allocations, plus all
# OwnerPINBuilder variants so disabled capabilities are reported as `no`.
PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile base --serial D92E743D5F640C0F

# Grant pcscd access after USB re-enumeration, then confirm the physical reader.
PICO2_HOST_USB_ACCESS=1 sudo -n /usr/local/sbin/pico2-pcsc-reader
PICO2_HOST_USB_ACCESS=1 pcsc_scan -r

# Use the exact Pico reader name printed above.
reader_name='Pico 2 CHOUPI CCID [JCVM CCID] (D92E743D5F640C0F) 00 00'

# After powering/restarting the Pico, verify CAP/applet persistence and that
# the JCAlgTest applet can still be selected and process JCSystem APDUs.
PICO2_HOST_USB_ACCESS=1 ./tools/test-pico-powerup "$reader_name"

# Install the supplied JCAlgTest 1.8.2 JC305 CAP (skip if already installed).
./tools/env java -jar .local-toolchain/globalplatformpro/tool/target/gp.jar \
  --reader "$reader_name" \
  --install AlgTest_JavaCard/AlgTest_v1.8.2_jc305.cap

# Retrieve the extended algorithm support report.
./tools/run-jcalgtest --reader-name "$reader_name" \
  -op ALG_SUPPORT_EXTENDED -fresh \
  -outpath artifacts/results/base-alg-support

# Build and flash the all-enabled current profile to a second Pico, then run
# the same JCAlgTest operation there for an apples-to-apples report comparison.
PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi --profile current --serial 2BED26ED20556535
reader_name_current='Pico 2 CHOUPI CCID [JCVM CCID] (2BED26ED20556535) 00 00'
./tools/run-jcalgtest --reader-name "$reader_name_current" \
  -op ALG_SUPPORT_EXTENDED -fresh \
  -outpath artifacts/results/current-alg-support

# Compare each report against the capability rows generated for that profile.
./tools/compare-jcalgtest-support.py \
  artifacts/results/base-alg-support/*ALGSUPPORT*.csv \
  artifacts/results/base-alg-support/*ALGSUPPORT*.csv \
  --expected-support artifacts/results/base-alg-support/expected.csv

# Run the static performance suite.
./tools/run-jcalgtest --reader-name "$reader_name" \
  -op ALG_PERFORMANCE_STATIC -fresh \
  -outpath artifacts/results/physical-perf-static
```

If the PC/SC helper is not installed, install its restricted sudo rule once with
`sudo ./tools/install-pcsc-sudoers`. Re-run the ACL helper after each USB
re-enumeration. JCAlgTest records unsupported algorithms as
`NO_SUCH_ALGORITHM`; those rows are expected. Static performance may take
several minutes; the wrapper's default timeout is 900 seconds.
