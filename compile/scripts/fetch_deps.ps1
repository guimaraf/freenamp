# Script portátil para baixar e restaurar dependências em compile/
$ErrorActionPreference = "Stop"

$baseDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$compileDir = Join-Path $baseDir "compile"
$binDir = Join-Path $compileDir "bin"
$libsDir = Join-Path $compileDir "libs"

New-Item -ItemType Directory -Force -Path $binDir | Out-Null
New-Item -ItemType Directory -Force -Path $libsDir | Out-Null

# 1. yt-dlp.exe portátil
$ytdlpDest = Join-Path $binDir "yt-dlp.exe"
if (-not (Test-Path $ytdlpDest)) {
    Write-Host "[Freenamp] Baixando yt-dlp.exe portátil..."
    curl.exe -L -o $ytdlpDest "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"
}

# 2. nlohmann/json.hpp
$jsonDir = Join-Path $libsDir "nlohmann"
New-Item -ItemType Directory -Force -Path $jsonDir | Out-Null
$jsonDest = Join-Path $jsonDir "json.hpp"
if (-not (Test-Path $jsonDest)) {
    Write-Host "[Freenamp] Baixando nlohmann/json.hpp..."
    curl.exe -L -o $jsonDest "https://github.com/nlohmann/json/releases/latest/download/json.hpp"
}

# 3. SDL2 MinGW 64-bit
$sdlDir = Join-Path $libsDir "SDL2-2.30.12"
if (-not (Test-Path $sdlDir)) {
    Write-Host "[Freenamp] Baixando SDL2 MinGW development package..."
    $sdlZip = Join-Path $libsDir "sdl2.zip"
    curl.exe -L -o $sdlZip "https://github.com/libsdl-org/SDL/releases/download/release-2.30.12/SDL2-devel-2.30.12-mingw.zip"
    Expand-Archive -Path $sdlZip -DestinationPath $libsDir
    Remove-Item -Force $sdlZip
}

# 4. libmpv MinGW 64-bit
$mpvDir = Join-Path $libsDir "mpv"
if (-not (Test-Path $mpvDir)) {
    Write-Host "[Freenamp] Baixando libmpv MinGW development package..."
    $mpv7z = Join-Path $libsDir "mpv.7z"

    $mpvUrl = ""
    $headers = @{ "User-Agent" = "Freenamp-Build" }
    if ($env:GITHUB_TOKEN) {
        $headers["Authorization"] = "Bearer $env:GITHUB_TOKEN"
    }

    # 1. Tentar resolver a URL do release mais recente de shinchiro
    try {
        $rel = Invoke-RestMethod -Uri "https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/latest" -Headers $headers -TimeoutSec 15
        $asset = $rel.assets | Where-Object { $_.name -match '^mpv-dev-x86_64-\d+.*\.7z$' -and $_.name -notmatch '-v3-' } | Select-Object -First 1
        if ($asset -and $asset.browser_download_url) {
            $mpvUrl = $asset.browser_download_url
            Write-Host "[Freenamp] URL dinâmica do libmpv resolvida (shinchiro): $mpvUrl"
        }
    } catch {
        Write-Host "[Freenamp] Aviso: Falha ao consultar releases/latest do shinchiro: $_"
    }

    # 2. Tentar zhongfly como fallback dinâmico
    if (-not $mpvUrl) {
        try {
            $releases = Invoke-RestMethod -Uri "https://api.github.com/repos/zhongfly/mpv-winbuild/releases" -Headers $headers -TimeoutSec 15
            foreach ($r in $releases) {
                $asset = $r.assets | Where-Object { $_.name -match '^mpv-dev-x86_64-\d+.*\.7z$' -and $_.name -notmatch '-v3-' } | Select-Object -First 1
                if ($asset -and $asset.browser_download_url) {
                    $mpvUrl = $asset.browser_download_url
                    Write-Host "[Freenamp] URL dinâmica do libmpv resolvida (zhongfly): $mpvUrl"
                    break
                }
            }
        } catch {
            Write-Host "[Freenamp] Aviso: Falha ao consultar releases do zhongfly: $_"
        }
    }

    # 3. Fallback estático direto
    if (-not $mpvUrl) {
        $mpvUrl = "https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20261021/mpv-dev-x86_64-20261021-git-e471496c56.7z"
        Write-Host "[Freenamp] Usando URL estática de fallback: $mpvUrl"
    }

    Write-Host "[Freenamp] Baixando: $mpvUrl"
    curl.exe -fL -o $mpv7z $mpvUrl

    if (-not (Test-Path $mpv7z) -or ((Get-Item $mpv7z).Length -lt 1000000)) {
        throw "Download do libmpv falhou ou arquivo incompleto: $mpv7z"
    }

    New-Item -ItemType Directory -Force -Path $mpvDir | Out-Null
    tar.exe -xf $mpv7z -C $mpvDir
    Remove-Item -Force $mpv7z
}

Write-Host "[Freenamp] Todas as dependências portáteis estão prontas em compile/!"
