<# MSYS2 aislado y raylib fijada. Sólo escribe .tools y .deps bajo este proyecto.
   Los SHA256 fijan los archivos base; pacman verifica firmas de sus paquetes.
   MSYS2 es rolling release: registramos cada versión instalada para auditoría. #>
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force '.tools','.deps' | Out-Null
    function Get-VerifiedArchive([string]$Url,[string]$Path,[string]$Sha256) {
        if (!(Test-Path -LiteralPath $Path)) {
            & curl.exe --fail --location --silent --show-error $Url -o $Path
            if ($LASTEXITCODE) { throw "No se pudo descargar $Url" }
        }
        if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Sha256) {
            throw "SHA256 incorrecto: $Path. No se ejecutara ni extraera."
        }
    }
    Get-VerifiedArchive 'https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-20260611.tar.xz' '.tools/msys2-base.tar.xz' 'A2D047E8EE213C3C6A49A8DE427EB1069DF12207C0422FF1B3CBB5C905C34221'
    if (!(Test-Path -LiteralPath '.tools/msys64/usr/bin/bash.exe')) {
        & tar.exe -xf '.tools/msys2-base.tar.xz' -C '.tools'
        if ($LASTEXITCODE) { throw 'No se pudo extraer MSYS2' }
    }
    $env:MSYSTEM='UCRT64';$env:CHERE_INVOKING='1'
    # Dos invocaciones: actualizar el runtime puede terminar la primera shell.
    & '.\.tools\msys64\usr\bin\bash.exe' -lc 'pacman -Syu --noconfirm'
    if ($LASTEXITCODE) { throw 'Fallo actualizando el runtime local' }
    & '.\.tools\msys64\usr\bin\bash.exe' -lc 'pacman -Syu --noconfirm && pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-clang-tools-extra'
    if ($LASTEXITCODE) { throw 'Fallo instalando herramientas locales' }
    # p11-kit lanza trust por una cadena sin comillas en algunas versiones.
    # Invocarlo directamente corrige su postinstalación cuando la ruta contiene
    # espacios, sin alterar paquetes ni relajar la comprobación TLS.
    $trustTool = Join-Path $projectRoot '.tools/msys64/ucrt64/bin/trust.exe'
    $trustBase = Join-Path $projectRoot '.tools/msys64/ucrt64/etc/pki/ca-trust/extracted'
    & $trustTool extract --format=openssl-bundle --filter=certificates --overwrite --comment "$trustBase/openssl/ca-bundle.trust.crt"
    if ($LASTEXITCODE) { throw 'Fallo preparando certificados OpenSSL' }
    foreach ($purpose in @('server-auth','email','code-signing')) {
        $bundle = switch ($purpose) { 'server-auth' {'tls'} 'email' {'email'} 'code-signing' {'objsign'} }
        & $trustTool extract --format=pem-bundle --filter=ca-anchors --overwrite --comment --purpose $purpose "$trustBase/pem/$bundle-ca-bundle.pem"
        if ($LASTEXITCODE) { throw 'Fallo preparando certificados PEM' }
    }
    & $trustTool extract --format=java-cacerts --filter=ca-anchors --overwrite --purpose server-auth "$trustBase/java/cacerts"
    if ($LASTEXITCODE) { throw 'Fallo preparando certificados Java' }
    & '.\.tools\msys64\usr\bin\pacman.exe' -Q | Set-Content '.tools/toolchain-packages.txt' -Encoding utf8
    Get-VerifiedArchive 'https://github.com/raysan5/raylib/archive/dbc56a87da87d973a9c5baa4e7438a9d20121d28.tar.gz' '.deps/raylib-6.0.tar.gz' '81B06CE7C19CF3B634B0271C23C361BA6AD8BF45FB8B036ABBFEB4260EC1E126'
    Get-VerifiedArchive 'https://github.com/raysan5/raygui/archive/020a61bebcbe288b4414de3416e219ef40af847a.tar.gz' '.deps/raygui-5.0.tar.gz' '8327EE8EC254ABABFD76908CF39857384AD311E4EF43F9C2C7D94BEC6E4A6389'
    if (!(Test-Path -LiteralPath '.deps/raygui-020a61bebcbe288b4414de3416e219ef40af847a/src/raygui.h')) {
        & tar.exe -xf '.deps/raygui-5.0.tar.gz' -C '.deps'
        if ($LASTEXITCODE) { throw 'No se pudo extraer raygui' }
    }
    Write-Output 'Herramientas listas. Siguiente: ./tools/build.ps1 -Preset debug -Test'
} finally { Pop-Location }
