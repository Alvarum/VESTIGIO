<# Verificaciones de formato y análisis. No formatea automáticamente. #>
param([switch]$Full)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$toolBin=Join-Path $projectRoot '.tools/msys64/ucrt64/bin'
$env:PATH="$toolBin;$env:PATH"
Push-Location $projectRoot
try {
    $engineRoot = Join-Path $projectRoot 'engines/vestigio'
    $sourceFiles=Get-ChildItem -Path "$engineRoot/include", "$engineRoot/src", "$engineRoot/tests" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.c','.h','.cpp','.hpp' }
    foreach ($sourceFile in $sourceFiles) {
        & "$toolBin/clang-format.exe" --dry-run --Werror $sourceFile.FullName
        if ($LASTEXITCODE) { throw "Formato incorrecto: $($sourceFile.FullName)" }
    }
    & "$PSScriptRoot/build.ps1" -Preset analyze -Test
    & "$PSScriptRoot/build.ps1" -Preset ubsan -Test
    if ($Full) {
        & "$PSScriptRoot/build.ps1" -Preset debug -Test
        & "$PSScriptRoot/build.ps1" -Preset release -Test
    }
} finally { Pop-Location }
