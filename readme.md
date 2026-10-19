# Pico 2 JCVM + JCAlgTest

Run these commands from the repository root. The flash wrapper builds both
architectures as a safety gate and flashes the ARM CHOUPI image.

```sh
# One-time: extract the official JCAlgTest 1.8.2 client (archive already downloaded).
mkdir -p build/algtest-client-1.8.2
unzip -o build/downloads/AlgTest_dist_1.8.2.zip AlgTestJClient.jar \
  -d build/algtest-client-1.8.2

# Build and flash CHOUPI to the Pico 2.
PICO2_HOST_USB_ACCESS=1 ./tools/cycle choupi

# Grant pcscd access after USB re-enumeration, then confirm the physical reader.
sudo -n /usr/local/sbin/pico2-pcsc-reader
pcsc_scan -r

# Use the exact Pico reader name printed above.
reader_name='Pico 2 CHOUPI CCID [JCAlgTest CCID] (D92E743D5F640C0F) 00 00'

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
  -outpath artifacts/results/physical-alg-support

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
