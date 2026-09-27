param(
    [ValidateRange(0,100000)][int]$Smoke = 0,
    [string]$Capture = ''
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
    & $player @arguments
    if ($LASTEXITCODE) { throw "Player termino con codigo $LASTEXITCODE" }
} finally {
    Pop-Location
}
