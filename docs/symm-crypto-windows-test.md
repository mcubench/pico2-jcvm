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

   Confirm that a Pico 2 CHOUPI CCID reader appears. Use the exact reader name
   shown by Windows in the commands below. If `SCardSvr` was already running,
   skip `Start-Service`.

## Install JCAlgTest and run the standard client

Replace the example reader value with the full name reported by Windows:

```powershell
$reader = 'Pico 2 CHOUPI CCID'
java -jar .\tools\gp.jar --reader $reader --list
java -jar .\tools\gp.jar --reader $reader --install .\applet\AlgTest_v1.8.2_jc305.cap
java -jar .\tools\AlgTestJClient.jar -op ALG_SUPPORT_EXTENDED -fresh -cardname CHOUPI_symm_crypto -outpath .\results\symm_crypto
```

At the client reader prompt, select the Pico entry. If the package is already
installed, inspect `--list` output and skip the install command. Compare the
result CSV against the generated support list:

```powershell
$csv = Get-ChildItem .\results\symm_crypto\*ALGSUPPORT*.csv | Select-Object -First 1
py .\tools\compare-jcalgtest-support.py $csv.FullName --expected-support .\expected\expected-support-symm_crypto.csv
```

The expected CSV lists the 46 capabilities selected for this profile. A
complete Windows enumeration and client run must be performed on the target
Windows system; this package itself does not claim that result.
