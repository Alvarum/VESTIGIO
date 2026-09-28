<# Entrada estable desde PowerShell. No modifica PATH del usuario ni el MSYS2 global. #>
param([ValidateSet('debug','release','analyze','ubsan')][string]$Preset='debug', [switch]$Test)
$ErrorActionPreference='Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$engineRoot = Join-Path $projectRoot 'engines/vestigio'
$buildDirectory = "build/vestigio/$Preset"
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
    $cmakeArguments = @('-S', $engineRoot, '-B', $buildDirectory, '-G', 'Ninja',
        "-DCMAKE_C_COMPILER=$toolBin/gcc.exe", "-DCMAKE_CXX_COMPILER=$toolBin/g++.exe", "-DCMAKE_MAKE_PROGRAM=$toolBin/ninja.exe",
        '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON')
    if ($Preset -eq 'release') { $cmakeArguments += '-DCMAKE_BUILD_TYPE=Release' }
    else { $cmakeArguments += '-DCMAKE_BUILD_TYPE=Debug' }
    if ($Preset -eq 'analyze') { $cmakeArguments += @('-DVG_ANALYZE=ON', '-DVG_BUILD_APPS=OFF') }
    if ($Preset -eq 'ubsan') {
        $cmakeArguments += @("-DCMAKE_C_COMPILER=$toolBin/clang.exe", "-DCMAKE_CXX_COMPILER=$toolBin/clang++.exe", '-DVG_BUILD_APPS=OFF',
            '-DCMAKE_C_FLAGS=-fsanitize=undefined -fsanitize-trap=all')
    }
    & "$toolBin\cmake.exe" @cmakeArguments
    if ($LASTEXITCODE) { throw 'Fallo configurando CMake' }
    & "$toolBin\cmake.exe" --build $buildDirectory --parallel 4
    if ($LASTEXITCODE) { throw 'Fallo compilando' }
    if ($Preset -in @('debug','release')) {
        $dotnetConfiguration = if ($Preset -eq 'release') { 'Release' } else { 'Debug' }
        & dotnet build 'engines/vestigio/studio/Vestigio.Studio.csproj' -c $dotnetConfiguration `
            --no-restore -p:RestorePackagesPath='.nuget/packages' -p:RestoreLockedMode=true `
            -o "$buildDirectory/bin"
        if ($LASTEXITCODE) { throw 'Fallo compilando VESTIGIO Studio' }
        if ($Test) {
            & dotnet build 'engines/vestigio/studio.tests/Vestigio.Studio.Tests.csproj' -c $dotnetConfiguration `
                --no-restore -p:RestorePackagesPath='.nuget/packages' -p:RestoreLockedMode=true `
                -o "$buildDirectory/bin"
            if ($LASTEXITCODE) { throw 'Fallo compilando las pruebas de VESTIGIO Studio' }
        }
    }
    if ($Test) {
        & "$toolBin\ctest.exe" --test-dir $buildDirectory --output-on-failure
        if ($LASTEXITCODE) { throw 'Fallo de pruebas' }
    }
} finally { Pop-Location }
