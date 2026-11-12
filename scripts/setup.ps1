# One-time setup. Everything goes inside this project folder (nothing is
# installed system-wide):
#   tools/arduino-cli.exe          Arduino command-line tool
#   tools/llvm-mingw*/             portable clang for the desktop tests/preview
#   .arduino/libraries/            Arduino_GFX ("GFX Library for Arduino")
# The ESP32 Arduino core (esp32:esp32 3.3.12) is shared with Arduino IDE in
# %LOCALAPPDATA%\Arduino15; it's installed there if missing.
#
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\setup.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $root 'tools'
$tmp = Join-Path $root 'build\downloads'
New-Item -ItemType Directory -Force $tools, $tmp | Out-Null

function Get-Verified($url, $file, $sha256) {
  if (-not (Test-Path $file)) {
    Write-Host "downloading $url"
    Invoke-WebRequest -UseBasicParsing $url -OutFile $file
  }
  $actual = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()
  if ($actual -ne $sha256) { Remove-Item $file; throw "checksum mismatch for $file" }
}

# arduino-cli 1.6.0 (checksum from the GitHub release)
$cli = Join-Path $tools 'arduino-cli.exe'
if (-not (Test-Path $cli)) {
  $zip = Join-Path $tmp 'arduino-cli.zip'
  Get-Verified 'https://github.com/arduino/arduino-cli/releases/download/v1.6.0/arduino-cli_1.6.0_Windows_64bit.zip' $zip '2769b9b282ce597db9679d8ec14d726d57004ca4205ef4d7a795bf36b6cdc9d8'
  Expand-Archive $zip -DestinationPath (Join-Path $tmp 'cli') -Force
  Copy-Item (Join-Path $tmp 'cli\arduino-cli.exe') $cli
}

# llvm-mingw 20261020 (portable clang + MinGW headers for Windows)
if (-not (Get-ChildItem $tools -Directory -Filter 'llvm-mingw*' -ErrorAction SilentlyContinue)) {
  $zip = Join-Path $tmp 'llvm-mingw.zip'
  Get-Verified 'https://github.com/mstorsjo/llvm-mingw/releases/download/20261020/llvm-mingw-20261020-ucrt-x86_64.zip' $zip 'fd5462b7f5d93b2ff981fcd3f9c7e427e47f3203c64ea91c27bcc444eebd74cd'
  Expand-Archive $zip -DestinationPath $tools -Force
}

$env:ARDUINO_DIRECTORIES_USER = Join-Path $root '.arduino'
& $cli core update-index
& $cli core install esp32:esp32@3.3.12
& $cli lib install 'GFX Library for Arduino@1.6.9'
Write-Host "`nSetup complete."
