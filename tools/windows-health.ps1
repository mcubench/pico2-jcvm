# Read and decode the Pico 2 JCVM DF7E health record (GET DATA, RAM and
# POWMAN scratch only; reading it never writes flash). Works without a serial
# capture. Shipped in Windows packages as tools\health.ps1.
#   powershell -ExecutionPolicy Bypass -File tools\health.ps1 -Reader 'Pico2 JCVM CCID 0'
# boot_count is 1 after the first boot following USB power-on; a higher value
# in the same power session means the card reset (watchdog, fault, reboot).
param(
  [Parameter(Mandatory = $true)][string]$Reader,
  [string]$OutFile,
  [string]$Gp = (Join-Path $PSScriptRoot 'gp.jar')
)
$raw = & java -jar $Gp --reader $Reader --apdu 00A4040008A000000151000000 --apdu 80CADF7E00 -d 2>&1 | ForEach-Object { "$_" }
$hit = $raw | Where-Object { $_ -match 'A<< .*\) (DF7E26[0-9A-Fa-f]{76}) 9000' } | Select-Object -First 1
$lines = @("health_time=$((Get-Date).ToString('o'))")
if ($hit -and $hit -match '(DF7E26[0-9A-Fa-f]{76})') {
  $v = $Matches[1].Substring(6)
  function U([int]$Offset, [int]$Bytes) { [Convert]::ToUInt32($v.Substring($Offset * 2, $Bytes * 2), 16) }
  $lines += @(
    "health_record=$($Matches[1])",
    "health_version=$(U 0 1)",
    "health_flags=0x$($v.Substring(2, 2))",
    "health_boot_count=$(U 2 4)",
    "health_chip_reset=0x$($v.Substring(12, 8))",
    "health_recovery_pc=0x$($v.Substring(20, 8))",
    "health_recovery_info=0x$($v.Substring(28, 8))",
    "health_uptime_ms=$(U 18 4)",
    "health_apdus=$(U 22 4)",
    "health_sw_error=$(U 26 4)",
    "health_last_error_apdu=$(U 30 4)",
    "health_last_error_cla_ins=$($v.Substring(68, 4))",
    "health_last_error_sw=$($v.Substring(72, 4))")
} else {
  $lines += 'health_record=unavailable'
  $lines += $raw | ForEach-Object { "# $_" }
}
if ($OutFile) { $lines | Set-Content $OutFile }
$lines | Where-Object { $_ -notlike '# *' }
