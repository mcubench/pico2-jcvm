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

$samples = @()
foreach ($line in Get-Content -Path $OutFile) {
  if ($line -notmatch 'OBJECT_DELETION:PRE_CLEANUP\b.*?files=(\d+)\s+bytes=(\d+)/(\d+)') {
    continue
  }
  $files = [UInt64]::Parse($Matches[1])
  $used = [UInt64]::Parse($Matches[2])
  $capacity = [UInt64]::Parse($Matches[3])
  if ($capacity -eq 0 -or $used -gt $capacity) {
    throw "Invalid pre-cleanup telemetry in ${OutFile}: files=$files bytes=$used/$capacity"
  }
  $samples += [PSCustomObject]@{ Files = $files; Used = $used; Capacity = $capacity }
}
if ($samples.Count -eq 0) {
  throw "No OBJECT_DELETION:PRE_CLEANUP telemetry in $OutFile"
}
$capacities = @($samples | Select-Object -ExpandProperty Capacity -Unique)
if ($capacities.Count -ne 1) {
  throw "Inconsistent storage capacities in $OutFile: $($capacities -join ',')"
}
$peakFiles = ($samples | Measure-Object -Property Files -Maximum).Maximum
$peakUsed = ($samples | Measure-Object -Property Used -Maximum).Maximum
$capacity = $capacities[0]
$peakPercent = [Math]::Round((100.0 * $peakUsed / $capacity), 3)
$headroomStatus = if ($peakUsed * 10 -gt $capacity * 9) { 'FAIL' } else { 'PASS' }
Write-Host "HEADROOM:$headroomStatus samples=$($samples.Count) peak_files=$peakFiles peak_bytes=$peakUsed/$capacity peak_percent=$peakPercent threshold=90%"
if ($peakUsed * 10 -gt $capacity * 9) {
  throw "Object deletion exceeded the 90% persistent arena headroom gate"
}
