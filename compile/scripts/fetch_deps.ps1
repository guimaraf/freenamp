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
    curl.exe -L -o $mpv7z "https://github.com/zhongfly/mpv-winbuild/releases/download/2026-09-16-0b7ed670f7/mpv-dev-x86_64-20260916-git-0b7ed670f7.7z"
    New-Item -ItemType Directory -Force -Path $mpvDir | Out-Null
    tar.exe -xf $mpv7z -C $mpvDir
    Remove-Item -Force $mpv7z
}

Write-Host "[Freenamp] Todas as dependências portáteis estão prontas em compile/!"
