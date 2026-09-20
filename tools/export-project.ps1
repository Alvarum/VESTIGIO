<# Exporta un proyecto dirigido por datos sin recompilar sus reglas.
   En el árbol fuente usa el player Release; en el paquete instalado usa el
   retro_player.exe situado junto a Studio. La salida es autocontenida. #>
param(
    [Parameter(Mandatory=$true)][string]$Project,
    [string]$Output='dist/Games'
)
$ErrorActionPreference='Stop'
$manifest=(Resolve-Path -LiteralPath $Project).Path
$projectRoot=Split-Path -Parent $manifest
$sourceRoot=Split-Path -Parent $PSScriptRoot
$player=Join-Path $sourceRoot 'retro_player.exe'
if (!(Test-Path -LiteralPath $player)) {
    $player=Join-Path $sourceRoot 'build/release/bin/retro_player.exe'
}
if (!(Test-Path -LiteralPath $player)) {
    throw 'No existe retro_player.exe Release. Ejecuta tools/build.ps1 -Preset release.'
}
$name=[IO.Path]::GetFileNameWithoutExtension($manifest)
$target=Join-Path $Output $name
New-Item -ItemType Directory -Force -Path $target | Out-Null
Get-ChildItem -LiteralPath $projectRoot -Force | Copy-Item -Destination $target -Recurse -Force
Copy-Item -LiteralPath $manifest -Destination (Join-Path $target 'project.retro') -Force
Copy-Item -LiteralPath $player -Destination (Join-Path $target "$name.exe") -Force
$sessionLibrary = Join-Path (Split-Path -Parent $player) 'retro_session.dll'
if (!(Test-Path -LiteralPath $sessionLibrary)) { throw 'Falta retro_session.dll junto al Player.' }
Copy-Item -LiteralPath $sessionLibrary -Destination $target -Force
foreach($notice in @('README.md','THIRD_PARTY.md')) {
    $candidate=Join-Path $sourceRoot $notice
    if(Test-Path -LiteralPath $candidate) { Copy-Item -LiteralPath $candidate -Destination $target -Force }
}
$licenses=Join-Path $sourceRoot 'licenses'
if(!(Test-Path -LiteralPath $licenses)) {
    $licenses=Join-Path $sourceRoot 'dist/RetroForge/licenses'
}
if(Test-Path -LiteralPath $licenses) {
    Copy-Item -LiteralPath $licenses -Destination $target -Recurse -Force
}
$zip=Join-Path $Output "$name-Windows.zip"
Compress-Archive -LiteralPath $target -DestinationPath $zip -Force
Write-Output "Juego: $target"
Write-Output "ZIP:   $zip"
