param([switch]$NoLaunch, [string]$Settings = '')
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
    & $cmake --preset debug -B build/vestigio-studio -DRETRO_BUILD_APPS=ON
    if ($LASTEXITCODE) { throw 'Fallo configurando VESTIGIO Studio' }
    & $cmake --build build/vestigio-studio --target vestigio_gpu_host --parallel 4
    if ($LASTEXITCODE) { throw 'Fallo compilando el runtime de Studio' }

    $bin = Join-Path $projectRoot 'build/vestigio-studio/bin'
    $demo = Join-Path $bin 'assets/demo'
    New-Item -ItemType Directory -Force -Path $demo | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot 'assets/demo/atrium.level.json') -Destination $demo -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'assets/demo/atrium.gltf') -Destination $demo -Force

    & dotnet build src/studio/RetroForge.Studio.csproj -c Debug --no-restore `
        -p:RestorePackagesPath=.nuget/packages -p:RestoreLockedMode=true -o $bin
    if ($LASTEXITCODE) { throw 'Fallo compilando la aplicación WPF' }
    if (!$NoLaunch) {
        $arguments = @('--atrium')
        if ($Settings) { $arguments += @('--settings', $Settings) }
        & (Join-Path $bin 'retro_studio.exe') @arguments
        if ($LASTEXITCODE) { throw "Studio terminó con código $LASTEXITCODE" }
    }
} finally {
    Pop-Location
}
