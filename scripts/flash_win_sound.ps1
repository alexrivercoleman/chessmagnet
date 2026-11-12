# Writes a WAV file into the board's "audio" flash partition, to be played
# when someone wins. Normal firmware uploads don't touch this partition, so
# you only need to do this once.
#
# The WAV must be 16-bit PCM (mono or stereo, 8-48 kHz) and at most 2 MB.
# 16 kHz mono is plenty for the small speaker: 20 s = 640 KB.
#
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\flash_win_sound.ps1 -Wav data\win.wav -Port COM5
param(
  [Parameter(Mandatory = $true)][string]$Wav,
  [Parameter(Mandatory = $true)][string]$Port
)
$ErrorActionPreference = 'Stop'
$partitionOffset = 0x410000   # must match firmware/chessMagnet/partitions.csv
$partitionSize = 0x200000

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $Wav))
if ($bytes.Length -gt $partitionSize) { throw "WAV is $($bytes.Length) bytes; the partition holds $partitionSize." }
$hdr = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4) + [System.Text.Encoding]::ASCII.GetString($bytes, 8, 4)
if ($hdr -ne 'RIFFWAVE') { throw 'Not a WAV file.' }

# esptool ships with the ESP32 Arduino core.
$esptool = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py" -Recurse -Filter 'esptool.exe' |
  Select-Object -First 1
if (-not $esptool) { throw 'esptool not found (is the ESP32 core installed?)' }

& $esptool.FullName --chip esp32s3 --port $Port write-flash ('0x{0:X}' -f $partitionOffset) (Resolve-Path $Wav)
if ($LASTEXITCODE -ne 0) { throw 'esptool failed' }
Write-Host 'Done. Reset the board; the serial log should show "win.wav: ...".'
