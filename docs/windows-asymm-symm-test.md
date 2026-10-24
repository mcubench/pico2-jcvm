# Windows Pico 2 profile test

Use the supplied ARM UF2s to compare the `symm_crypto` and `asymm_crypto`
profiles on a Windows PC. The asymmetric profile contains the symmetric
profile's enabled capabilities plus the current asymmetric selections.

## Files

The generated archive `build/external-test/pico2-jcvm-windows-test.zip`
contains both UF2 images, the upstream AlgTest 1.8.2 CAP, GPPro, the official
JCAlgTest 1.8.2 client, expected support CSVs, a CSV comparator, and this
run sheet. Check `SHA256SUMS.txt` before use.

## Flash and check PC/SC

1. Extract the archive. Hold BOOTSEL while connecting the Pico 2, then copy
   `firmware/pico2_choupi_symm_crypto.uf2` to the `RPI-RP2` drive. Wait for the
   board to reboot. Run this test first; repeat with the asymmetric image for
   the second profile.
2. In PowerShell, check the Windows Smart Card service and reader:

   ```powershell
   Get-Service SCardSvr
   # In an elevated PowerShell only if the service is stopped:
   Start-Service SCardSvr
   certutil -scinfo
   ```

   Confirm that `Pico2 JCVM CCID` appears. If other readers are present,
   note the Pico's index in the JCAlgTest client's reader list.

## Install upstream applet and run JCAlgTest

From the extracted archive directory, run:

```powershell
java -jar .\tools\gp.jar --reader "Pico2 JCVM CCID" --list
java -jar .\tools\gp.jar --reader "Pico2 JCVM CCID" --install .\applet\AlgTest_v1.8.2_jc305.cap
java -jar .\tools\AlgTestJClient.jar -op ALG_SUPPORT_EXTENDED -fresh -cardname "CHOUPI_symm_crypto" -outpath .\results\symm_crypto
```

If Windows reports a longer reader name, use that exact name for `--reader`.
At the JCAlgTest reader prompt, select the Pico entry. Do not modify the CAP;
it is copied unchanged from upstream.

Compare the completed output CSV to the profile's expected support set. Find
the actual CSV under `results\symm_crypto`, then run:

```powershell
py .\tools\compare-jcalgtest-support.py .\results\symm_crypto\<actual-file>.csv --expected-support .\expected\expected-support-symm_crypto.csv
```

For the second profile, repeat BOOTSEL flashing with
`firmware/pico2_choupi_asymm_crypto.uf2`, then repeat installation and testing
with `asymm_crypto` in the output path and card name, and use
`expected-support-asymm_crypto.csv`.

## Notes

- Each firmware build is self-contained; flash only one profile at a time.
- The CAP and applet installation may survive reflashing. If GPPro reports
  that the package is already loaded, inspect it with `--list` before trying
  installation again.
- These images passed repository ARM builds. A RISC-V wrapper build compiles
  the VM core only; it does not produce a full CHOUPI firmware image.
- This test checks Windows PC/SC enumeration and the standard client flow. It
  does not establish Windows driver behavior for other host configurations.
