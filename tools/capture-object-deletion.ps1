param(
  [Parameter(Mandatory = $true)] [string] $Port,
  [int] $Seconds = 3600,
  [string] $OutFile = ".\object-deletion-serial.log"
)

$ErrorActionPreference = "Stop"
if ($Seconds -lt 1 -or $Seconds -gt 86400) {
  throw "Seconds must be in 1..86400"
}

$parent = Split-Path -Parent $OutFile
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$serial = [System.IO.Ports.SerialPort]::new(
  $Port, 115200, [System.IO.Ports.Parity]::None, 8,
  [System.IO.Ports.StopBits]::One)
$serial.ReadTimeout = 250
$serial.NewLine = "`n"
$stream = $null
$writer = $null
try {
  $stream = [System.IO.File]::Open(
    $OutFile, [System.IO.FileMode]::Create,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
  $writer = [System.IO.StreamWriter]::new(
    $stream, [System.Text.UTF8Encoding]::new($false))
  $writer.AutoFlush = $true
  $serial.Open()
  Write-Host "SERIAL_CAPTURE:START port=$Port seconds=$Seconds file=$OutFile"
  $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
  while ([DateTime]::UtcNow -lt $deadline) {
    try {
      $line = $serial.ReadLine().TrimEnd([char]13)
      $writer.WriteLine($line)
      Write-Host $line
    } catch [System.TimeoutException] {
      # Bounded blocking read; loop again until the requested deadline.
    }
  }
} finally {
  if ($serial.IsOpen) { $serial.Close() }
  if ($writer) { $writer.Dispose() }
  elseif ($stream) { $stream.Dispose() }
}

$lines = @(Select-String -Path $OutFile -Pattern '^OBJECT_DELETION:' -ErrorAction SilentlyContinue)
$positive = @($lines | Where-Object { $_.Line -match 'result=0' -and $_.Line -match 'erased_(applet|package)=[1-9][0-9]*' })
Write-Host "SERIAL_CAPTURE:SUMMARY object_deletion_records=$($lines.Count) positive_reclaims=$($positive.Count) file=$OutFile"
