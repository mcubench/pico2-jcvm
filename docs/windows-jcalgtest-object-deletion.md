# Windows test: JCAlgTest object deletion enabled

This is a diagnostic firmware build of the `max_crypto` profile. It reports
`JCSystem.isObjectDeletionSupported() == true`, accepts
`JCSystem.requestObjectDeletion()`, and services a request after its APDU has
returned and the Java call stack is empty. It does **not** enable unconditional
reclamation after every APDU. Normal/release builds continue to report object
deletion unsupported until physical interruption and long-run acceptance are
complete.

The test uses the unchanged upstream JCAlgTest 1.8.4 / jc305 CAP and matching
client. Do not substitute or modify the CAP.

## Files

- `pico2-jcvm-max_crypto-object-deletion-diagnostic-empty-card.uf2`
- `AlgTest_v1.8.4_jc305.cap`
- `AlgTestJClient_v1.8.4.jar`
- `gp.jar`
- `capture-object-deletion.ps1`
- `SHA256SUMS.txt`

## Flash and install

1. Put the Pico 2 in BOOTSEL and copy the diagnostic UF2 to `RPI-RP2`. This is
   an empty-card image; it replaces applet and persistent card contents.
2. Reconnect the token and confirm the reader appears in Windows PC/SC.
3. In the package directory, install the upstream applet, replacing the reader
   name with its exact Windows PC/SC name:

   ```powershell
   java -jar .\gp.jar --reader "Pico2 JCVM CCID 0" --install .\AlgTest_v1.8.4_jc305.cap
   ```

## Run and capture

Find each Pico's USB serial COM port in Device Manager. Use separate identified
Pico 2 tokens for the two long runs when possible. In one PowerShell window per
token, capture firmware telemetry long enough for the whole run (adjust the
COM port; use 32,400 seconds for the 5–8-hour run):

```powershell
powershell -ExecutionPolicy Bypass -File .\capture-object-deletion.ps1 -Port COM5 -Seconds 32400 -OutFile .\results\performance-serial.log
```

On the first token, launch the upstream client interactively and choose
`2 -> PERFORMANCE TEST` (all JC API methods with 256-byte data, estimated
1–3 hours). Select that token's Pico 2 JCVM reader when prompted. Save its
result files under a dedicated directory. On another token, repeat the flash
and applet installation steps, then choose
`1 -> All algorithms` (estimated 5–8 hours) and capture a separate serial log
and result directory. These interactive choices exercise the periodic
`INS_CARD_RESET` path.

```powershell
java -jar .\AlgTestJClient_v1.8.4.jar
```

The serial log should contain `OBJECT_DELETION:` records following
`INS_CARD_RESET` APDUs. `result=0` means both persistent root scans completed;
`erased_applet` and `erased_package` show how many records were reclaimed.
Review the JCAlgTest log/result files for failed or incomplete rows and any
`6F00`, `F205`, reset, or PC/SC errors. Please return the full `results` folder
and matching serial log for each physical token, including partial files if a
run stalls. Include the selected reader name and chip ID for both.

This diagnostic build is for testing only. Passing one Windows run does not
establish power-loss safety or justify enabling deletion in release firmware.
