# Compiles the firmware for the ESP32-S3 (does not upload).
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\build_firmware.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cli = Join-Path $root 'tools\arduino-cli.exe'
# Libraries live inside the project (.arduino/libraries), not in your
# Documents folder, so the exact versions used here can't drift.
$env:ARDUINO_DIRECTORIES_USER = Join-Path $root '.arduino'

# ESP32S3 Dev Module, 16 MB flash, octal (OPI) PSRAM, our own partitions.csv.
$fqbn = 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom,CDCOnBoot=default'
$sketch = Join-Path $root 'firmware\chessMagnet'
$out = Join-Path $root 'build\firmware'

& $cli compile --fqbn $fqbn --library (Join-Path $root 'lib\ChessCore') --output-dir $out --warnings default $sketch
if ($LASTEXITCODE -ne 0) { throw 'firmware build failed' }
Write-Host "`nFirmware built: $out"
