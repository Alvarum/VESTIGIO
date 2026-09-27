# Ejemplo de puerta (180 frames por defecto):
# powershell -File tools/run-3d-demo.ps1 -SmokeDoor -ShowColliders -Capture build/door.png
param(
    [ValidateRange(0,100000)][int]$Smoke = 0,
    [string]$Capture = '',
    [switch]$ShowColliders,
    [string]$Level = '',
    [switch]$SmokeDoor,
    [ValidateSet('clean', 'retro')][string]$Visual = '',
    [string]$Settings = '',
    [switch]$NoAudio,
    [switch]$SmokeAudio,
    [switch]$SaveAudio,
    [ValidateRange(0, 1)][double]$VolumeMaster,
    [ValidateRange(0, 1)][double]$VolumeMusic,
    [ValidateRange(0, 1)][double]$VolumeSfx,
    [ValidateRange(0, 1)][double]$VolumeAmbience
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolBin = Join-Path $projectRoot '.tools/msys64/ucrt64/bin'
$cmake = Join-Path $toolBin 'cmake.exe'
if (!(Test-Path -LiteralPath $cmake)) { throw 'Ejecuta primero tools/bootstrap.ps1' }
$env:PATH = "$toolBin;$env:PATH"
$buildTemp = Join-Path $projectRoot 'build/temp'
New-Item -ItemType Directory -Force -Path $buildTemp | Out-Null
$env:TEMP = $buildTemp
$env:TMP = $buildTemp
Push-Location $projectRoot
try {
    & $cmake --preset debug -B build/vestigio-demo -DRETRO_BUILD_APPS=ON
    if ($LASTEXITCODE) { throw 'Fallo configurando la demo 3D' }
    & $cmake --build build/vestigio-demo --target vestigio_player --parallel 4
    if ($LASTEXITCODE) { throw 'Fallo compilando la demo 3D' }
    $player = Join-Path $projectRoot 'build/vestigio-demo/bin/vestigio_player.exe'
    $arguments = @()
    if ($Smoke -gt 0) { $arguments += @('--smoke', [string]$Smoke) }
    if ($Capture) { $arguments += @('--capture', $Capture) }
    if ($ShowColliders) { $arguments += '--show-colliders' }
    if ($Level) { $arguments += @('--level', $Level) }
    if ($SmokeDoor) { $arguments += '--smoke-door' }
    if ($Visual) { $arguments += @('--visual', $Visual) }
    if ($Settings) { $arguments += @('--settings', $Settings) }
    if ($NoAudio) { $arguments += '--no-audio' }
    if ($SmokeAudio) { $arguments += '--smoke-audio' }
    if ($SaveAudio) { $arguments += '--save-audio' }
    if ($PSBoundParameters.ContainsKey('VolumeMaster')) {
        $arguments += @('--volume-master', $VolumeMaster.ToString([Globalization.CultureInfo]::InvariantCulture))
    }
    if ($PSBoundParameters.ContainsKey('VolumeMusic')) {
        $arguments += @('--volume-music', $VolumeMusic.ToString([Globalization.CultureInfo]::InvariantCulture))
    }
    if ($PSBoundParameters.ContainsKey('VolumeSfx')) {
        $arguments += @('--volume-sfx', $VolumeSfx.ToString([Globalization.CultureInfo]::InvariantCulture))
    }
    if ($PSBoundParameters.ContainsKey('VolumeAmbience')) {
        $arguments += @('--volume-ambience', $VolumeAmbience.ToString([Globalization.CultureInfo]::InvariantCulture))
    }
    & $player @arguments
    if ($LASTEXITCODE) { throw "Player termino con codigo $LASTEXITCODE" }
} finally {
    Pop-Location
}
