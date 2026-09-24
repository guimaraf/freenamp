# Script dedicado para compilar APENAS o software Freenamp (atualizando o hash/versao do yt-dlp no codigo)
$ErrorActionPreference = "Stop"

$rootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildDir = Join-Path $rootDir "build"
$distDir = Join-Path $buildDir "freenamp_portable"
$distCoreDir = Join-Path $distDir "core"
$distBinDir = Join-Path $distDir "bin"
$ytdlpBin = Join-Path $rootDir "compile/bin/yt-dlp.exe"

# Encerrar instancia em execucao se houver, para permitir sobrescrita do binario
Get-Process -Name freenamp -ErrorAction SilentlyContinue | Stop-Process -Force

if (Test-Path $ytdlpBin) {
    $hash = (Get-FileHash $ytdlpBin -Algorithm SHA256).Hash.ToLower()
    $ver = (& $ytdlpBin --version | Select-Object -First 1).Trim()
    Write-Host "[Freenamp] yt-dlp detectado: v$ver (SHA256: $hash)"
}

Write-Host "[Freenamp] Reconfigurando CMake com hash/versao atual do yt-dlp..."
cmake -B $buildDir -S $rootDir -DCMAKE_BUILD_TYPE=Release

Write-Host "[Freenamp] Compilando apenas os binarios do Freenamp (freenamp_core e freenamp)..."
cmake --build $buildDir --config Release --target freenamp_core freenamp

# Atualizar pacote portatil se existir
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
New-Item -ItemType Directory -Force -Path $distCoreDir | Out-Null
New-Item -ItemType Directory -Force -Path $distBinDir | Out-Null

$freenampExe = Join-Path $buildDir "frontend/freenamp.exe"
if (-not (Test-Path $freenampExe)) {
    $freenampExe = Join-Path $buildDir "freenamp.exe"
}
Copy-Item -Force $freenampExe $distDir

$coreDll = Join-Path $buildDir "frontend/core/libfreenamp_core.dll"
if (-not (Test-Path $coreDll)) {
    $coreDll = Join-Path $buildDir "frontend/libfreenamp_core.dll"
}
Copy-Item -Force $coreDll $distCoreDir

if (Test-Path $ytdlpBin) {
    Copy-Item -Force $ytdlpBin $distBinDir
}

Write-Host "`n[Freenamp] Compilacao exclusiva do Freenamp concluida com sucesso!"
Write-Host "Executavel atualizado em: $(Join-Path $distDir 'freenamp.exe')"
