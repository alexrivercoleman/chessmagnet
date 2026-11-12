# Converts any audio file (MP3, etc.) into data\win.wav in the format the
# firmware plays: 16-bit PCM, mono, 16 kHz, peak-normalised, short fade-out,
# no metadata. data\ is ignored by git, so the song is never committed.
#
# Needs ffmpeg (e.g. `winget install Gyan.FFmpeg`).
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\make_win_sound.ps1 -In "$HOME\Downloads\song.mp3"
param(
  [Parameter(Mandatory = $true)][string]$In,
  [double]$FadeSeconds = 0.3
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'data\win.wav'
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null

$ffmpeg = (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source
if (-not $ffmpeg) {
  $ffmpeg = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter ffmpeg.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
}
if (-not $ffmpeg) { throw 'ffmpeg not found.' }

# Measure the length so the fade lands at the very end.
$ffprobe = Join-Path (Split-Path $ffmpeg) 'ffprobe.exe'
$duration = [double](& $ffprobe -v error -show_entries format=duration -of csv=p=0 $In)
if ($duration -gt 60) { throw "Clip is $duration s; keep it under a minute (the flash partition holds ~65 s)." }
$fadeStart = [Math]::Max(0, $duration - $FadeSeconds)

# ffmpeg writes its normal log to stderr; don't let PowerShell treat that as
# a failure (we check $LASTEXITCODE instead).
$ErrorActionPreference = 'Continue'

# Pass 1: find the loudest sample, to normalise the peak to -1 dBFS.
$stats = & $ffmpeg -hide_banner -nostats -i $In -af 'aresample=16000,pan=mono|c0=0.5*c0+0.5*c1,volumedetect' -f null - 2>&1
$peak = [double](($stats | Select-String 'max_volume: (-?[\d.]+) dB').Matches[0].Groups[1].Value)
$gain = -1.0 - $peak

# Pass 2: convert. Mono mix, 16 kHz, gain, fade-out, 16-bit PCM, no tags.
& $ffmpeg -hide_banner -loglevel error -y -i $In -map_metadata -1 -fflags +bitexact `
  -af "pan=mono|c0=0.5*c0+0.5*c1,aresample=16000,volume=${gain}dB,afade=t=out:st=${fadeStart}:d=${FadeSeconds}" `
  -ar 16000 -ac 1 -c:a pcm_s16le -flags +bitexact $out
if ($LASTEXITCODE -ne 0) { throw 'ffmpeg failed' }
'{0}: {1:N1} s, peak gain {2:+0.0;-0.0} dB, {3:N0} KB' -f $out, $duration, $gain, ((Get-Item $out).Length / 1KB)
