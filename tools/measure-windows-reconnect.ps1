param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('normal', 'diagnostic', 'original')]
  [string]$BuildLabel,

  [Parameter(Mandatory = $true)]
  [ValidatePattern('^[0-9a-fA-F]{64}$')]
  [string]$ImageSha256,

  [string]$ReaderPrefix = 'Pico2 JCVM CCID',

  [ValidateRange(1, 20)]
  [int]$Runs = 3,

  [ValidateSet('yes', 'no', 'unknown')]
  [string]$DriverPreinstalled = 'unknown',

  [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

if (-not ('Pico2Pcsc' -as [type])) {
  Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

public static class Pico2Pcsc {
  private const uint ScopeSystem = 2;
  private const uint ShareShared = 2;
  private const uint ProtocolT0 = 1;
  private const uint ProtocolT1 = 2;
  private const uint Success = 0;
  private const uint NoReaders = 0x8010002E;
  private const uint InsufficientBuffer = 0x80100008;

  [StructLayout(LayoutKind.Sequential)]
  public struct IoRequest {
    public uint Protocol;
    public uint Length;
  }

  [DllImport("winscard.dll", EntryPoint = "SCardEstablishContext")]
  private static extern uint EstablishContext(uint scope, IntPtr reserved1,
    IntPtr reserved2, out IntPtr context);

  [DllImport("winscard.dll", EntryPoint = "SCardReleaseContext")]
  private static extern uint ReleaseContext(IntPtr context);

  [DllImport("winscard.dll", EntryPoint = "SCardListReadersW",
    CharSet = CharSet.Unicode)]
  private static extern uint ListReaders(IntPtr context, string groups,
    IntPtr readers, ref uint characters);

  [DllImport("winscard.dll", EntryPoint = "SCardConnectW",
    CharSet = CharSet.Unicode)]
  private static extern uint Connect(IntPtr context, string reader,
    uint shareMode, uint preferredProtocols, out IntPtr card,
    out uint activeProtocol);

  [DllImport("winscard.dll", EntryPoint = "SCardDisconnect")]
  private static extern uint Disconnect(IntPtr card, uint disposition);

  [DllImport("winscard.dll", EntryPoint = "SCardTransmit")]
  private static extern uint Transmit(IntPtr card, ref IoRequest sendPci,
    byte[] sendBuffer, uint sendLength, IntPtr receivePci,
    byte[] receiveBuffer, ref uint receiveLength);

  public static IntPtr OpenContext() {
    IntPtr context;
    uint result = EstablishContext(ScopeSystem, IntPtr.Zero, IntPtr.Zero,
      out context);
    if (result != Success) throw new InvalidOperationException(
      "SCardEstablishContext failed: 0x" + result.ToString("X8"));
    return context;
  }

  public static void CloseContext(IntPtr context) {
    if (context != IntPtr.Zero) ReleaseContext(context);
  }

  public static string[] Readers(IntPtr context) {
    uint characters = 0;
    uint result = ListReaders(context, null, IntPtr.Zero, ref characters);
    if (result == NoReaders) return new string[0];
    if (result != Success) throw new InvalidOperationException(
      "SCardListReaders(size) failed: 0x" + result.ToString("X8"));
    if (characters == 0) return new string[0];
    int capacity = checked((int)characters);
    IntPtr names = Marshal.AllocHGlobal(checked(capacity * 2));
    try {
      result = ListReaders(context, null, names, ref characters);
      if (result == InsufficientBuffer) return new string[0];
      if (result != Success) throw new InvalidOperationException(
        "SCardListReaders(data) failed: 0x" + result.ToString("X8"));
      List<string> readers = new List<string>();
      StringBuilder current = new StringBuilder();
      for (int index = 0; index < capacity; ++index) {
        char value = (char)(ushort)Marshal.ReadInt16(names, index * 2);
        if (value == '\0') {
          if (current.Length == 0) break;
          readers.Add(current.ToString());
          current.Clear();
        } else {
          current.Append(value);
        }
      }
      return readers.ToArray();
    } finally {
      Marshal.FreeHGlobal(names);
    }
  }

  public static uint SelectIsd(string reader, out string response) {
    response = "";
    IntPtr context = OpenContext();
    IntPtr card = IntPtr.Zero;
    try {
      uint protocol;
      uint result = Connect(context, reader, ShareShared,
        ProtocolT0 | ProtocolT1, out card, out protocol);
      if (result != Success) return result;
      IoRequest pci = new IoRequest();
      pci.Protocol = protocol;
      pci.Length = (uint)Marshal.SizeOf(typeof(IoRequest));
      byte[] command = new byte[] {
        0x00, 0xA4, 0x04, 0x00, 0x08,
        0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x00, 0x00
      };
      byte[] receive = new byte[258];
      uint receiveLength = (uint)receive.Length;
      result = Transmit(card, ref pci, command, (uint)command.Length,
        IntPtr.Zero, receive, ref receiveLength);
      if (result != Success) {
        response = "";
        return result;
      }
      byte[] exact = new byte[(int)receiveLength];
      Array.Copy(receive, exact, receiveLength);
      response = BitConverter.ToString(exact).Replace("-", "");
      return Success;
    } finally {
      if (card != IntPtr.Zero) Disconnect(card, 0);
      CloseContext(context);
    }
  }
}
'@
}

if (-not $OutputPath) {
  $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $OutputPath = Join-Path (Get-Location) "windows-reconnect-$BuildLabel-$stamp.csv"
}

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory -and -not (Test-Path $outputDirectory)) {
  New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$context = [Pico2Pcsc]::OpenContext()
$eventWatcher = $null
try {
  $rows = @()
  Write-Host "Windows: $([Environment]::OSVersion.VersionString)"
  Write-Host "Image: $BuildLabel SHA-256 $ImageSha256"
  Write-Host "Reader match: starts with '$ReaderPrefix'"
  Write-Host "Driver already installed: $DriverPreinstalled"
  Write-Host 'Close every serial terminal/monitor and any COM auto-connect tool.'
  Write-Host 'For each run, unplug the Pico. Press Enter after it is removed.'

  for ($run = 1; $run -le $Runs; $run++) {
    [void](Read-Host "Run $run/$Runs - unplug the Pico, then press Enter")
    $removalDeadline = [DateTime]::UtcNow.AddSeconds(30)
    while ([DateTime]::UtcNow -lt $removalDeadline) {
      $present = @([Pico2Pcsc]::Readers($context) | Where-Object {
        $_.StartsWith($ReaderPrefix, [StringComparison]::OrdinalIgnoreCase)
      })
      if ($present.Count -eq 0) { break }
      Start-Sleep -Milliseconds 100
    }
    if ($present.Count -ne 0) {
      throw "Pico reader did not disappear from PC/SC after unplug: $($present -join ', ')"
    }

    $query = 'SELECT * FROM Win32_DeviceChangeEvent WHERE EventType = 2'
    $eventWatcher = New-Object -TypeName System.Management.ManagementEventWatcher -ArgumentList $query
    $eventWatcher.Options.Timeout = [TimeSpan]::FromSeconds(60)
    $eventWatcher.Start()
    Write-Host 'Now reconnect the Pico. Do not open a serial client.'
    $arrivalDeadline = [DateTime]::UtcNow.AddSeconds(60)
    $arrival = $null
    while ([DateTime]::UtcNow -lt $arrivalDeadline -and -not $arrival) {
      $event = $eventWatcher.WaitForNextEvent()
      try {
        if ([uint32]$event.Properties['EventType'].Value -eq 2) {
          $created = $event.SystemProperties['TIME_CREATED']
          if ($created -and $created.Value) {
            $arrival = [DateTime]::FromFileTimeUtc([Int64]$created.Value)
          } else {
            $arrival = [DateTime]::UtcNow
          }
        }
      } finally {
        $event.Dispose()
      }
    }
    if (-not $arrival) { throw 'No Windows PnP arrival event within 60 seconds.' }
    $eventWatcher.Stop()
    $eventWatcher.Dispose()
    $eventWatcher = $null

    $dispatchDelayMs = ([DateTime]::UtcNow - $arrival).TotalMilliseconds
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $reader = $null
    $readerDeadline = [DateTime]::UtcNow.AddSeconds(30)
    while ([DateTime]::UtcNow -lt $readerDeadline -and -not $reader) {
      $reader = [Pico2Pcsc]::Readers($context) | Where-Object {
        $_.StartsWith($ReaderPrefix, [StringComparison]::OrdinalIgnoreCase)
      } | Select-Object -First 1
      if (-not $reader) { Start-Sleep -Milliseconds 25 }
    }
    if (-not $reader) { throw 'No matching PC/SC reader within 30 seconds of PnP arrival.' }
    $readerListedMs = $dispatchDelayMs + $clock.Elapsed.TotalMilliseconds
    $clock.Stop()

    $apduResponse = ''
    $apduResult = [uint32]0
    $apduDeadline = [DateTime]::UtcNow.AddSeconds(15)
    $apduClock = [Diagnostics.Stopwatch]::StartNew()
    do {
      $apduResult = [Pico2Pcsc]::SelectIsd($reader, [ref]$apduResponse)
      if ($apduResult -eq 0) { break }
      Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $apduDeadline)
    $apduClock.Stop()
    if ($apduResult -ne 0) {
      throw ('SCardConnect/SELECT failed for {0}: PC/SC 0x{1:X8}' -f $reader, $apduResult)
    }
    if (-not $apduResponse.EndsWith('9000', [StringComparison]::OrdinalIgnoreCase)) {
      throw "ISD SELECT returned $apduResponse, expected SW 9000."
    }

    $row = [pscustomobject]@{
      Run = $run
      PnpArrivalUtc = $arrival.ToString('o')
      Reader = $reader
      ReaderListedMsFromPnpArrival = [Math]::Round($readerListedMs, 1)
      ApduSuccessMsAfterReaderListed = [Math]::Round($apduClock.Elapsed.TotalMilliseconds, 1)
      TotalMsFromPnpArrivalToApduSuccess = [Math]::Round(($readerListedMs + $apduClock.Elapsed.TotalMilliseconds), 1)
      SelectResponse = $apduResponse
      BuildLabel = $BuildLabel
      ImageSha256 = $ImageSha256.ToLowerInvariant()
      DriverPreinstalled = $DriverPreinstalled
      OsVersion = [Environment]::OSVersion.VersionString
      TimingOrigin = 'Win32_DeviceChangeEvent EventType=2; PC/SC list polling every 25 ms'
    }
    $rows += $row
    $row | Export-Csv -NoTypeInformation -Append -Encoding UTF8 -Path $OutputPath
    Write-Host ('Run {0}: reader listed {1:N1} ms; ISD SELECT {2} after {3:N1} ms; response {4}' -f `
      $run, $readerListedMs, $reader, $apduClock.Elapsed.TotalMilliseconds, $apduResponse)
    Write-Host "CSV: $OutputPath"
  }
} finally {
  if ($eventWatcher) {
    $eventWatcher.Stop()
    $eventWatcher.Dispose()
  }
  [Pico2Pcsc]::CloseContext($context)
}

Write-Host "Completed $($rows.Count) reconnect(s)."
