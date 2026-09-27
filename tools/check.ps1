<# Verificaciones de formato y análisis. No formatea automáticamente. #>
param([ValidateSet('RetroForge','Vestigio','Both')][string]$Engine='RetroForge', [switch]$Full)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$toolBin=Join-Path $projectRoot '.tools/msys64/ucrt64/bin'
$env:PATH="$toolBin;$env:PATH"
Push-Location $projectRoot
try {
    $engines = if ($Engine -eq 'Both') { @('RetroForge','Vestigio') } else { @($Engine) }
    foreach ($selectedEngine in $engines) {
        $engineDirectory = if ($selectedEngine -eq 'Vestigio') { 'vestigio' } else { 'retroforge' }
        $engineRoot = Join-Path $projectRoot "engines/$engineDirectory"
        $sourceFiles=Get-ChildItem -Path "$engineRoot/include", "$engineRoot/src", "$engineRoot/tests" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.c','.h','.cpp','.hpp' }
        foreach ($sourceFile in $sourceFiles) {
            & "$toolBin/clang-format.exe" --dry-run --Werror $sourceFile.FullName
            if ($LASTEXITCODE) { throw "Formato incorrecto: $($sourceFile.FullName)" }
        }
        & "$PSScriptRoot/build.ps1" -Engine $selectedEngine -Preset analyze -Test
        & "$PSScriptRoot/build.ps1" -Engine $selectedEngine -Preset ubsan -Test
        if ($Full) {
            & "$PSScriptRoot/build.ps1" -Engine $selectedEngine -Preset debug -Test
            & "$PSScriptRoot/build.ps1" -Engine $selectedEngine -Preset release -Test
        }
    }
} finally { Pop-Location }
