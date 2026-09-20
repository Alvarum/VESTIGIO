<# Exporta un proyecto dirigido por datos sin recompilar sus reglas.
   En el árbol fuente usa el player Release; en el paquete instalado usa el
   retro_player.exe situado junto a Studio. La salida es autocontenida. #>
param(
    [Parameter(Mandatory=$true)][string]$Project,
    [string]$Output='dist/Games',
    [string]$ExecutableName=''
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
$gameName=if($ExecutableName){$ExecutableName}else{$name}
if($gameName -notmatch '^[A-Za-z0-9 _-]+$') {
    throw 'ExecutableName solo admite letras ASCII, numeros, espacios, - y _.'
}
$outputFull=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Output))
$target=[IO.Path]::GetFullPath((Join-Path $outputFull $gameName))
if(!$target.StartsWith($outputFull + [IO.Path]::DirectorySeparatorChar,
                       [StringComparison]::OrdinalIgnoreCase)) {
    throw 'La carpeta de salida calculada no pertenece a Output.'
}
if(Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $target | Out-Null
Get-ChildItem -LiteralPath $projectRoot -Force | Copy-Item -Destination $target -Recurse -Force
Copy-Item -LiteralPath $manifest -Destination (Join-Path $target 'project.retro') -Force
Copy-Item -LiteralPath $player -Destination (Join-Path $target "$gameName.exe") -Force
$sessionLibrary = Join-Path (Split-Path -Parent $player) 'retro_session.dll'
if (!(Test-Path -LiteralPath $sessionLibrary)) { throw 'Falta retro_session.dll junto al Player.' }
Copy-Item -LiteralPath $sessionLibrary -Destination $target -Force
$projectGuide=Join-Path $projectRoot 'README.md'
if(Test-Path -LiteralPath $projectGuide) {
    Move-Item -LiteralPath (Join-Path $target 'README.md') `
        -Destination (Join-Path $target 'GUIA-DEL-JUEGO.md') -Force
}
$engineReadme=Join-Path $sourceRoot 'README.md'
if(Test-Path -LiteralPath $engineReadme) {
    Copy-Item -LiteralPath $engineReadme -Destination (Join-Path $target 'RETROFORGE.md') -Force
}
$thirdParty=Join-Path $sourceRoot 'THIRD_PARTY.md'
if(Test-Path -LiteralPath $thirdParty) {
    Copy-Item -LiteralPath $thirdParty -Destination $target -Force
}
$licenses=Join-Path $sourceRoot 'licenses'
if(!(Test-Path -LiteralPath $licenses)) {
    $licenses=Join-Path $sourceRoot 'dist/RetroForge/licenses'
}
if(Test-Path -LiteralPath $licenses) {
    Copy-Item -LiteralPath $licenses -Destination $target -Recurse -Force
}
$instructions=@(
    "HAUNTED - CASA DE LA NIEBLA",
    "",
    "Haz doble clic en $gameName.exe para jugar.",
    "",
    "WASD: mover    Ratón: mirar    Click izquierdo: disparar",
    "E: interactuar    Espacio: saltar    Escape: pausa",
    "F5: guardado rápido    F9: carga rápida"
)
$instructions | Set-Content -LiteralPath (Join-Path $target 'LEEME.txt') -Encoding UTF8
$zip=Join-Path $outputFull "$gameName-Windows.zip"
Compress-Archive -LiteralPath $target -DestinationPath $zip -Force
Write-Output "Juego: $target"
Write-Output "ZIP:   $zip"
