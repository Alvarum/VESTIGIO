<# Entrada estable desde PowerShell. No modifica PATH del usuario ni el MSYS2 global. #>
param([ValidateSet('debug','release','analyze','ubsan')][string]$Preset='debug', [switch]$Test, [switch]$Package)
$ErrorActionPreference='Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolBin = Join-Path $projectRoot '.tools\msys64\ucrt64\bin'
if (!(Test-Path -LiteralPath (Join-Path $toolBin 'cmake.exe'))) { throw 'Ejecuta primero tools/bootstrap.ps1' }
$env:PATH = "$toolBin;$env:PATH"
$buildTemp = Join-Path $projectRoot 'build\temp'
New-Item -ItemType Directory -Force -Path $buildTemp | Out-Null
# GCC crea ensamblador temporal. Mantenerlo dentro del proyecto hace el build
# reproducible también en sandboxes o cuentas con TEMP restringido.
$env:TEMP = $buildTemp
$env:TMP = $buildTemp
Push-Location $projectRoot
try {
    & "$toolBin\cmake.exe" --preset $Preset
    if ($LASTEXITCODE) { throw 'Fallo configurando CMake' }
    & "$toolBin\cmake.exe" --build --preset $Preset --parallel 4
    if ($LASTEXITCODE) { throw 'Fallo compilando' }
    if ($Test) {
        & "$toolBin\ctest.exe" --preset $Preset
        if ($LASTEXITCODE) { throw 'Fallo de pruebas' }
    }
    if ($Package) {
        if ($Preset -ne 'release') { throw 'El paquete requiere -Preset release' }
        & "$toolBin\cmake.exe" --install "build/$Preset" --prefix 'dist/RetroForge' --component Runtime
        if ($LASTEXITCODE) { throw 'Fallo empaquetando' }
        Compress-Archive -Path 'dist/RetroForge/*' -DestinationPath 'dist/RetroForge-Windows.zip' -Force
    }
} finally { Pop-Location }
