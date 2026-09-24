# Script para atualizar o yt-dlp.exe e recompilar o Freenamp com a nova versao/hash
$ErrorActionPreference = "Stop"

$rootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$binDir = Join-Path $rootDir "compile/bin"
$ytdlpBin = Join-Path $binDir "yt-dlp.exe"

New-Item -ItemType Directory -Force -Path $binDir | Out-Null

if (Test-Path $ytdlpBin) {
    $oldVer = (& $ytdlpBin --version | Select-Object -First 1).Trim()
    Write-Host "[Freenamp] Versao atual do yt-dlp: $oldVer"
    Write-Host "[Freenamp] Verificando atualizacao via yt-dlp -U..."
    & $ytdlpBin -U
} else {
    Write-Host "[Freenamp] Baixando yt-dlp.exe mais recente do GitHub..."
    curl.exe -L -o $ytdlpBin "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"
}

$newVer = (& $ytdlpBin --version | Select-Object -First 1).Trim()
$newHash = (Get-FileHash $ytdlpBin -Algorithm SHA256).Hash.ToLower()
Write-Host "[Freenamp] yt-dlp pronto: v$newVer (SHA256: $newHash)"

# Invocar o script que compila apenas o Freenamp com a nova versao/hash embutida
$buildOnlyScript = Join-Path $PSScriptRoot "build_freenamp.ps1"
if (Test-Path $buildOnlyScript) {
    & $buildOnlyScript
}
