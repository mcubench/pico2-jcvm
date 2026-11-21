# JCAlgTest physical runbook

Run commands from the repository root. Read the USB and PC/SC rules in
[AGENTS.md](../AGENTS.md) and check the [handoff](../.agent/HANDOFF.md)
before reserving a token.

## Identify the token and verify inputs

Before any PC/SC operation, check for live campaigns and monitors with
`pgrep -fa 'AlgTestJClient|jcalgtest-apdu-watchdog|tools/monitor'`.
Identify cards by chip ID using `PICO2_HOST_USB_ACCESS=1 lsusb -v -d 2e8a:0009`
with elevated execution. Every hardware command below also requires elevated
execution and the host-access environment marker as shown. Verify the exact
PC/SC reader name for the selected chip ID. Use bounded reader listing only
when necessary. Use the approved PC/SC helper only when a token is present on
USB, absent from PC/SC, and no campaign is live.

```sh
./tools/fixture-path --verify
cap=$(./tools/fixture-path jcalgtest.cap)
client_jar=$(./tools/fixture-path jcalgtest.client.1.8.5-20261121)
sha256sum "$cap" "$client_jar"
```

| Input | SHA-256 |
| --- | --- |
| JCAlgTest 1.8.5/jc305 CAP | `fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb` |
| JCAlgTest 1.8.5-20261121 client | `3cca70cbc243a773b998fa43792a135e26934d4bf616e7baa88c5369db18a703` |

Pass `--client-jar "$client_jar"` explicitly for every run. The wrapper's
default client and the embedded CSV version are unsuitable for identifying
the required JAR.

## Prepare a clean card for acceptance

Use a normal `max_crypto` build. The empty-card UF2 clears installed packages
and persistent objects on the selected token. Confirm that the token is
available for this use before flashing.

```sh
./tools/build arm --profile max_crypto
```

After a warning-free build, verify `mode=normal` in `build/arm/build-provenance.txt`. Set
`chip_id` to the confirmed 16-hex chip ID and choose a unique `result_dir`
for that token and campaign. Record both identities in the results.

```sh
mkdir -p "$result_dir"
cp build/javacard-support/jcalgtest-supported.csv "$result_dir/expected.csv"
image="$result_dir/max_crypto-empty-card.uf2"
./tools/package-empty-card-uf2 build/arm/pico2_choupi.uf2 "$image"
sha256sum "$image" "$cap" "$client_jar" > "$result_dir/input-sha256.txt"
PICO2_HOST_USB_ACCESS=1 ./tools/flash --chipid "$chip_id" "$image"
PICO2_HOST_USB_ACCESS=1 ./tools/monitor --serial "$chip_id" --seconds 8 > "$result_dir/startup.log"
```

Check the serial startup result for failures, then verify the re-enumerated
reader and set `reader_name` to its exact PC/SC name. Use GPPro to verify
only the ISD is present before installing the CAP.

```sh
PICO2_HOST_USB_ACCESS=1 ./tools/env java -jar .local-toolchain/globalplatformpro/tool/target/gp.jar \
  --reader "$reader_name" --list
PICO2_HOST_USB_ACCESS=1 ./tools/env java -jar .local-toolchain/globalplatformpro/tool/target/gp.jar \
  --reader "$reader_name" --install "$cap"
```

## Run and compare results

For a focused support report:

```sh
PICO2_HOST_USB_ACCESS=1 ./tools/run-jcalgtest --client-jar "$client_jar" \
  --reader-name "$reader_name" -op ALG_SUPPORT_EXTENDED -fresh -outpath "$result_dir"
./tools/compare-jcalgtest-support.py "$result_dir"/*ALGSUPPORT*.csv \
  tests/fixtures/reference/jcalgtest/1.8.5/jcardsim-alg-support-extended.csv \
  --expected-support "$result_dir/expected.csv"
```

Use a separate result directory and generated CSV for each image in a
development profile comparison. For long acceptance campaigns, prioritize
menu option `2 -> PERFORMANCE TEST` with 256-byte data and
`1 -> All algorithms`. Run the two campaigns on distinct identified tokens
when possible. Set `JCALGTEST_TIMEOUT_SECONDS` for the campaign because the
wrapper defaults to 900 seconds. Keep bounded serial telemetry, client logs,
results and image identity per device. Record the chip ID, exact reader name,
firmware hash, CAP hash and client JAR hash. Record any timeout, reset or
disconnect and use an available identified token if a board fails.

For the independent Windows run, use the current max_crypto release package
and its included capture instructions. [Active Windows request](../.agent/EXT_REQUESTS.md)
