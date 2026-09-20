<# Verificaciones de formato y análisis. No formatea automáticamente. #>
param([switch]$Full)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$toolBin=Join-Path $projectRoot '.tools/msys64/ucrt64/bin'
$env:PATH="$toolBin;$env:PATH"
Push-Location $projectRoot
try {
    $sourceFiles=Get-ChildItem -Path include,src,tests -Recurse -File | Where-Object { $_.Extension -in '.c','.h' }
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
