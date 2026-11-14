# Removed from .agent/EXT_REQUESTS.md on 2026-11-14

Superseded by WIN-OBJECT-DELETION-NODEBUG-0E67DF89; it shipped the 1.8.4 "matching client" (rule-conflict sheet item 16-A).

## Windows validation: fresh max_crypto empty-card image

Please validate the current RP2350 ARM `max_crypto` firmware on a Windows host.
The image has no pre-installed applet and clears the card's persistent storage
region; install the upstream JCAlgTest CAP explicitly during the test.

- [Download the complete Windows test package](../build/external-test/pico2-jcvm-max_crypto-windows-test.zip)
  (includes firmware, GPPro, pinned JCAlgTest 1.8.4/jc305 CAP and matching
  client, expected support CSV, comparator, and run sheet).
- [Download the standalone empty-card UF2](../artifacts/firmware/pico2-jcvm-max_crypto-arm-empty-card-current.uf2)
- Package SHA-256: `c872e23383daa298212caefafa24f3c860a3c1ea9c18e0a6b2f8db44124da804`
- UF2 SHA-256: `5f0b8383261b511c11c23f5cd9f96c93948fd66616527dc5f2fd48e733418502`
- UF2 size: 4,085,248 bytes (UF2 transport blocks include firmware and a full
  erased persistent-storage region).
- Build provenance: profile `max_crypto`, ARM `rp2350-arm-s`, serial wait
  `0 ms`, CPLC source ID `0xFF393005`; firmware end `0x100F2974`, leaving
  54,924 bytes before the configured flash limit.

The package [README](../build/external-test/max_crypto-package/README.md)
contains the complete PowerShell steps. In brief:

1. Hold BOOTSEL while connecting the Pico 2 and copy the packaged UF2 to the
   `RPI-RP2` drive.
2. Use `Get-Service SCardSvr` and `certutil -scinfo` to confirm Windows sees
   `Pico2 JCVM CCID`; send the exact reader name and ATR. If `certutil` reports
   a card-provider lookup error, include it and continue with the GPPro check.
3. Run `java -jar .\tools\gp.jar --reader $reader --list` and confirm there
   are no `APP:`/`PKG:` entries, then explicitly install
   `AlgTest_v1.8.4_jc305.cap` using the package's GPPro command.
4. Run the included `AlgTestJClient.jar` for `ALG_SUPPORT_EXTENDED`, then run
   `compare-jcalgtest-support.py` against the included max_crypto expected CSV.

Please return the Windows version, exact reader name/ATR, before/after GPPro
listings, JCAlgTest CSV, and comparator output. The CAP remains the unchanged
pinned upstream fixture; do not modify it.

