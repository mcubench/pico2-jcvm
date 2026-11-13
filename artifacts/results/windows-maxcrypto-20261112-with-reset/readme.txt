Observations for pico2-jcvm-max_crypto-object-deletion-no-debug-empty-card-580f3f40.uf2 on Windows host
- AlgTestJClient.jar with fixed resetAPDU used - garbage collection is triggered
- option 2 -> PERFORMANCE TEST and 1 -> All algorithms (estimated time 5-8 hours)
 used 
- When the AlgTestJClient.jar run  will get stuck (in log see message "ACTION REQUIRED: Re-insert the card (or power-cycle it), then press:"), the smartcard stopped to be visible to windows pc/sc layer. 
```
> certutil -scinfo
The Microsoft Smart Card Resource Manager is not running.
WaitForSingleObject: Service is in an unknown state.
CertUtil: -SCInfo command FAILED: 0x80070102 (WIN32/HTTP: 258 WAIT_TIMEOUT)
```

- I reinserted token (no applet reuopload, no firmware reupload) and restarted AlgTestJClient.jar with continuation from previous run (see in logs this:)
```
Specify type of your card (e.g., NXP JCOP CJ2A081): File 'pico2_maxcrypto_20261112withReset____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED__already_measured.list' with already measured algorithms found. Do you like to use it and measure only missing algorithms? (y/n)
y
	Continue was selected. Only algorithms NOT present pico2_maxcrypto_20261112withReset____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED__already_measured.list in will be measured
Following algorithms will NOT be measured again (listed in pico2_maxcrypto_20261112withReset____PERFORMANCE_SYMMETRIC_ASYMMETRIC_DATAFIXED__already_measured.list file):
```

- The run eventually got stuck again. I reinserted and continued. After 3rd reinsertion, the run stopped measuting anything sensible and even after removal, the applet communicates, but is not abel to performn any performnace measurement:
```
method name:; TYPE_RSA_PRIVATE LENGTH_RSA_1984 ALG_RSA_PKCS1 Cipher_doFinal()
measurement config:;appletPrepareINS;31;appletMeasureINS;43;config;00 11 00 0a ff ff 00 05 07 c0 00 07 00 7c ff ff 00 01 00 32 00 01
UNKONWN_ERROR-card_has_return_value_6f00
```
and after some time 
```
method name:; TYPE_RSA_PRIVATE LENGTH_RSA_2048 ALG_RSA_ISO14888 Cipher_doFinal()
measurement config:;appletPrepareINS;31;appletMeasureINS;43;config;00 11 00 09 ff ff 00 05 08 00 00 07 00 80 ff ff 00 01 00 32 00 01
SystemException_NO_RESOURCE
```

- I decided to test removal of applet. When trying to unistall applet using `gp --uninstall AlgTest_v1.8.4_jc305.cap -d` I got this error:
```
>gp --uninstall AlgTest_v1.8.4_jc305.cap -d
GlobalPlatformPro v20.01.23-0-g5ad373b
Running on Windows 10 10.0 amd64, Java 1.8.0_202 by Oracle Corporation
# Detected readers from JNA2PCSC
[*] Pico2 JCVM CCID 0
SCardConnect("Pico2 JCVM CCID 0", T=*) -> T=1, 3B800181
SCardBeginTransaction("Pico2 JCVM CCID 0")
A>> T=1 (4+0000) 00A40400 00
A<< (0012+2) (3ms) 6F0A8408A000000151000000 9000
[TRACE] GPSession -  [6F]
[TRACE] GPSession -      [84] A000000151000000
[DEBUG] GPSession - Auto-detected ISD: A000000151000000
Warning: no keys given, using default test key 404142434445464748494A4B4C4D4E4F
[WARN] PlaintextKeys - Don't know how to calculate KCV, defaulting to SCP02
[WARN] PlaintextKeys - Don't know how to calculate KCV, defaulting to SCP02
[WARN] PlaintextKeys - Don't know how to calculate KCV, defaulting to SCP02
[INFO] GPSession - Using card master keys: ENC=404142434445464748494A4B4C4D4E4F (KCV: 8BAF47) MAC=404142434445464748494A4B4C4D4E4F (KCV: 8BAF47) DEK=404142434445464748494A4B4C4D4E4F (KCV: 8BAF47) for null
[TRACE] GPSession - Generated host challenge: 1A64136201DDD97A
A>> T=1 (4+0008) 80500000 08 1A64136201DDD97A 00
A<< (0029+2) (3ms) 0000000000000000000000030055CDE7E4626803CAB7B487F114656600 9000
[DEBUG] GPSession - Host challenge: 1A64136201DDD97A
[DEBUG] GPSession - Card challenge: 55CDE7E4626803CA
[DEBUG] GPSession - Card reports SCP03 i=00 with key version 0 (0x00)
[INFO] GPSession - Diversified card keys: ENC=404142434445464748494A4B4C4D4E4F (KCV: 504A77) MAC=404142434445464748494A4B4C4D4E4F (KCV: 504A77) DEK=404142434445464748494A4B4C4D4E4F (KCV: 504A77) for SCP03
[INFO] GPSession - Session keys: ENC=8341878507F87BABA302EEC90F40C933 MAC=6DCB44C6DF9A87149718EC82D8E4EC17 RMAC=4BA14852D010F342C2ABDC090D903CC8, card keys=ENC=404142434445464748494A4B4C4D4E4F (KCV: 504A77) MAC=404142434445464748494A4B4C4D4E4F (KCV: 504A77) DEK=404142434445464748494A4B4C4D4E4F (KCV: 504A77) for SCP03
[DEBUG] GPSession - Verified card cryptogram: B7B487F114656600
[DEBUG] GPSession - Calculated host cryptogram: 3F7FC26EBF917A65
A>> T=1 (4+0016) 84820100 10 3F7FC26EBF917A65E51C4F8B64AF152F
A<< (0000+2) (2ms) 9000
A>> T=1 (4+0010) 84F28002 0A 4F00F949C917979BB419 00
A<< (0019+2) (2ms) E3114F08A0000001510000009F700101C50100 9000
[TRACE] GPRegistry -  [E3]
[TRACE] GPRegistry -      [4F] A000000151000000
[TRACE] GPRegistry -      [9F70] 01
[TRACE] GPRegistry -      [C5] 00
A>> T=1 (4+0010) 84F24002 0A 4F007F16CCD015991C46 00
A<< (0032+2) (2ms) E31E4F0A4A43416C675465737431C4094A43416C67546573749F700107C50100 9000
[TRACE] GPRegistry -  [E3]
[TRACE] GPRegistry -      [4F] 4A43416C675465737431
[TRACE] GPRegistry -      [C4] 4A43416C6754657374
[TRACE] GPRegistry -      [9F70] 07
[TRACE] GPRegistry -      [C5] 00
A>> T=1 (4+0010) 84F21002 0A 4F0088C4CDFFED512AF9 00
A<< (0029+2) (1ms) E31B4F094A43416C67546573749F700101840A4A43416C675465737431 9000
[TRACE] GPRegistry -  [E3]
[TRACE] GPRegistry -      [4F] 4A43416C6754657374
[TRACE] GPRegistry -      [9F70] 01
[TRACE] GPRegistry -      [84] 4A43416C675465737431
A>> T=1 (4+0010) 84F22002 0A 4F00FD0C5C52DA29F6AE 00
A<< (0017+2) (1ms) E30F4F094A43416C67546573749F700101 9000
[TRACE] GPRegistry -  [E3]
[TRACE] GPRegistry -      [4F] 4A43416C6754657374
[TRACE] GPRegistry -      [9F70] 01
[DEBUG] GPRegistry - Registry already contains PKG: 4A43416C6754657374, 1
[TRACE] DMTokenGenerator - No private key for token generation provided
[TRACE] GPSession - TLV payload:
[TRACE] GPSession -  [4F] 4A43416C6754657374
A>> T=1 (4+0019) 84E40080 13 4F094A43416C6754657374E58CCC5C76D74C71
<< (15s9ms) SCardTransmit got response 0x1f (null: null)
Failed to communicate with card in apdu4j.terminals.LoggingCardTerminal@7fac631b: Transmit failed: SCardTransmit got response 0x1f (null: null)
SCardEndTransaction("Pico2 JCVM CCID 0")
PC/SC failure: SCARD_E_SERVICE_STOPPED
```
If reinserted, the card isi visible again, and gp --list lists applets with jaclagtest applet still present. => removal will fail, possibly due to memory cleanup when applet is to be uninstalled. 

- When firmware was reuploted (after bootsel), then jclagtest applet installed and then uninstalled, it worked correctly and applet was removed.


