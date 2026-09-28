param([switch]$NoLaunch, [string]$Settings = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
$bin = Join-Path $projectRoot 'build/vestigio/debug/bin'
& "$PSScriptRoot/build.ps1" -Preset debug
if (!$?) { throw 'Fallo compilando VESTIGIO Studio' }
if (!$NoLaunch) {
    $arguments = @()
    if ($Settings) { $arguments += @('--settings', $Settings) }
    & (Join-Path $bin 'vestigio_studio.exe') @arguments
    if ($LASTEXITCODE) { throw "Studio terminó con código $LASTEXITCODE" }
}
} finally { Pop-Location }
