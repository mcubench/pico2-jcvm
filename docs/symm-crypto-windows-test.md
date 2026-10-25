# Windows test package: `symm_crypto`

This package tests only the `symm_crypto` ARM firmware profile. It contains no
asymmetric-profile firmware. The CAP is the unchanged upstream JCAlgTest 1.8.2
JC305 artifact.

## Flash and check PC/SC

1. Extract `build/external-test/symm-crypto-windows-test.zip` on Windows.
2. Hold BOOTSEL while connecting the Pico 2. When the `RPI-RP2` drive appears,
   open PowerShell in the extracted directory and verify/copy the firmware:

   ```powershell
   Get-FileHash .\firmware\pico2_choupi_symm_crypto.uf2 -Algorithm SHA256
   $boot = (Get-Volume -FileSystemLabel 'RPI-RP2').DriveLetter + ':\'
   Copy-Item .\firmware\pico2_choupi_symm_crypto.uf2 $boot
   ```

   Wait for the board to reboot.
3. Check Windows PC/SC. Start PowerShell as Administrator only if the service
   is stopped:

   ```powershell
   Get-Service SCardSvr
   Start-Service SCardSvr
   certutil -scinfo
   ```

   Confirm that a Pico2 JCVM CCID reader appears. Use the exact reader name
   shown by Windows in the commands below. If `SCardSvr` was already running,
   skip `Start-Service`.

## Ensure an empty card, then install JCAlgTest explicitly

Set `$reader` to the full name reported by Windows. Firmware updates preserve
CAPs previously written to the card's persistent filesystem. These commands
remove only the known JCAlgTest applet and package, then verify that no package
or applet remains before the explicit install:

```powershell
$reader = Read-Host 'Paste the exact Pico2 JCVM reader name shown by certutil -scinfo'
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
java -jar .\tools\AlgTestJClient.jar -op ALG_SUPPORT_EXTENDED -fresh -cardname CHOUPI_symm_crypto -outpath .\results\symm_crypto
if ($LASTEXITCODE -ne 0) { throw 'JCAlgTest client failed' }
```

At the client reader prompt, select the Pico entry. The applet is explicitly
installed by the command above and is not part of the firmware. Compare the
result CSV against the generated support list:

```powershell
$csv = Get-ChildItem .\results\symm_crypto\*ALGSUPPORT*.csv | Select-Object -First 1
$reference = '.\reference\jcardsim-jcalgtest-v1.8.2-alg-support-extended.csv'
py .\tools\compare-jcalgtest-support.py $csv.FullName $reference --expected-support .\expected\expected-support-symm_crypto.csv
```

The expected CSV lists the 46 capabilities selected for this profile. A
complete Windows enumeration and client run must be performed on the target
Windows system; this package itself does not claim that result.
