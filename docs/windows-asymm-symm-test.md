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

## Ensure an empty card, install upstream applet, and run JCAlgTest

From the extracted archive directory, run:

```powershell
$reader = 'Pico2 JCVM CCID'
$appletAid = '4A43416C675465737431'
$packageAid = '4A43416C6754657374'
$listed = & java -jar .\tools\gp.jar --reader $reader --list 2>&1
if ($LASTEXITCODE -ne 0) { throw 'GPPro --list failed' }
$listing = $listed | Out-String
if ($listing -match "APP:\s+$appletAid") {
  & java -jar .\tools\gp.jar --reader $reader --delete $appletAid
  if ($LASTEXITCODE -ne 0) { throw 'Failed to delete the JCAlgTest applet' }
}
if ($listing -match "PKG:\s+$packageAid") {
  & java -jar .\tools\gp.jar --reader $reader --delete $packageAid
  if ($LASTEXITCODE -ne 0) { throw 'Failed to delete the JCAlgTest package' }
}
$listed = & java -jar .\tools\gp.jar --reader $reader --list 2>&1
if ($LASTEXITCODE -ne 0) { throw 'GPPro --list failed after cleanup' }
$listing = $listed | Out-String
$listing
if ($listing -match '(?m)^(APP|PKG):') {
  throw 'Other applications/packages remain; review them before continuing'
}

# Upload the unchanged upstream CAP only after the empty-card check.
java -jar .\tools\gp.jar --reader $reader --install .\applet\AlgTest_v1.8.2_jc305.cap
if ($LASTEXITCODE -ne 0) { throw 'GPPro LOAD/INSTALL failed' }
java -jar .\tools\AlgTestJClient.jar -op ALG_SUPPORT_EXTENDED -fresh -cardname "CHOUPI_symm_crypto" -outpath .\results\symm_crypto
if ($LASTEXITCODE -ne 0) { throw 'JCAlgTest client failed' }
```

If Windows reports a longer reader name, use that exact name for `--reader`.
The firmware update preserves packages already installed on the card; the
commands above delete only the known JCAlgTest applet/package, confirm the
registry is otherwise empty, then explicitly upload the unchanged upstream
CAP. At the JCAlgTest reader prompt, select the Pico entry.

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
- The CAP and applet installation survive reflashing. The clean-card commands
  above remove the previous JCAlgTest installation before explicit upload.
- These images passed repository ARM builds. A RISC-V wrapper build compiles
  the VM core only; it does not produce a full CHOUPI firmware image.
- This test checks Windows PC/SC enumeration and the standard client flow. It
  does not establish Windows driver behavior for other host configurations.
