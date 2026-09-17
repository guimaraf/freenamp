# Script de empacotamento portátil final do Freenamp
$ErrorActionPreference = "Stop"

$rootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildDir = Join-Path $rootDir "build"
$distDir = Join-Path $buildDir "freenamp_portable"
$distBinDir = Join-Path $distDir "bin"

Write-Host "[Freenamp] Compilando versao Release..."
cmake --build $buildDir --config Release

Write-Host "[Freenamp] Criando diretorio de distribuicao portatil: $distDir"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
New-Item -ItemType Directory -Force -Path $distBinDir | Out-Null

# Copiar executavel principal e DLLs de execucao
Copy-Item -Force (Join-Path $buildDir "frontend/freenamp.exe") $distDir
Copy-Item -Force (Join-Path $buildDir "frontend/SDL2.dll") $distDir
Copy-Item -Force (Join-Path $buildDir "frontend/libmpv-2.dll") $distDir

# Copiar extrator yt-dlp.exe para a pasta bin/
Copy-Item -Force (Join-Path $rootDir "compile/bin/yt-dlp.exe") $distBinDir

Write-Host "`n[Freenamp] Pacote portatil standalone gerado com sucesso em:"
Write-Host "$distDir"
Get-ChildItem -Recurse $distDir | Select-Object Name, Length
