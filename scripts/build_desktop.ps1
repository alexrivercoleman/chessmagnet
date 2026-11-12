# Builds and runs the desktop programs:
#   build/desktop/test_perft.exe  - chess-library perft check
#   build/desktop/test_app.exe    - game/UI behaviour tests
#   build/desktop/preview.exe     - renders screens to build/preview/*.png and sounds to *.wav
#
# Compiler: the portable clang in tools/llvm-mingw (scripts/setup.ps1 fetches it).
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\build_desktop.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'build\desktop'
$preview = Join-Path $root 'build\preview'
New-Item -ItemType Directory -Force $out, $preview | Out-Null

$tc = Get-ChildItem (Join-Path $root 'tools') -Directory -Filter 'llvm-mingw*' -ErrorAction SilentlyContinue | Select-Object -First 1
$cxx = if ($tc) { Join-Path $tc.FullName 'bin\clang++.exe' } else { '' }
if (-not $cxx -or -not (Test-Path $cxx)) { throw "Compiler not found in tools\llvm-mingw* - run scripts\setup.ps1 first." }

$core = Join-Path $root 'lib\ChessCore\src'
$coreSrc = @(Get-ChildItem $core -Filter *.cpp | ForEach-Object { $_.FullName })
$flags = @('-std=c++17', '-O2', '-Wall', '-Wextra', '-static', "-I$core", "-I$root\desktop")

$targets = @(
  @{ name = 'test_perft'; src = @("$root\desktop\test_perft.cpp") },
  @{ name = 'test_app';   src = @("$root\desktop\test_app.cpp") + $coreSrc },
  @{ name = 'preview';    src = @("$root\desktop\preview.cpp") + $coreSrc }
)
foreach ($t in $targets) {
  Write-Host "== building $($t.name)"
  & $cxx @flags @($t.src) -o "$out\$($t.name).exe"
  if ($LASTEXITCODE -ne 0) { throw "build of $($t.name) failed" }
}

$failed = $false
Write-Host "`n== perft"
& "$out\test_perft.exe"; if ($LASTEXITCODE -ne 0) { $failed = $true }
Write-Host "`n== app tests"
& "$out\test_app.exe"; if ($LASTEXITCODE -ne 0) { $failed = $true }
Write-Host "`n== preview (output in build\preview)"
& "$out\preview.exe" $preview; if ($LASTEXITCODE -ne 0) { $failed = $true }
if ($failed) { Write-Host "`nSOME CHECKS FAILED"; exit 1 }
Write-Host "`nDesktop build OK."
