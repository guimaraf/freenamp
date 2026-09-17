# Script de empacotamento portátil final do Freenamp
$ErrorActionPreference = "Stop"

$rootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildDir = Join-Path $rootDir "build"
$distDir = Join-Path $buildDir "freenamp_portable"
$distCoreDir = Join-Path $distDir "core"
$distBinDir = Join-Path $distDir "bin"

Write-Host "[Freenamp] Compilando versao Release..."
cmake --build $buildDir --config Release

Write-Host "[Freenamp] Criando diretorio de distribuicao portatil: $distDir"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
New-Item -ItemType Directory -Force -Path $distCoreDir | Out-Null
New-Item -ItemType Directory -Force -Path $distBinDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $distDir "cache") | Out-Null

# Copiar executavel principal nativo (launcher) para a raiz
Copy-Item -Force (Join-Path $buildDir "frontend/freenamp.exe") $distDir

# Copiar DLLs para a subpasta core/
Copy-Item -Force (Join-Path $buildDir "frontend/core/SDL2.dll") $distCoreDir
Copy-Item -Force (Join-Path $buildDir "frontend/core/libmpv-2.dll") $distCoreDir
Copy-Item -Force (Join-Path $buildDir "frontend/core/libfreenamp_core.dll") $distCoreDir

# Remover qualquer versao legada de bat e DLLs da raiz
if (Test-Path (Join-Path $distDir "iniciar_freenamp.bat")) {
    Remove-Item -Force (Join-Path $distDir "iniciar_freenamp.bat")
}
if (Test-Path (Join-Path $distDir "SDL2.dll")) {
    Remove-Item -Force (Join-Path $distDir "SDL2.dll")
}
if (Test-Path (Join-Path $distDir "libmpv-2.dll")) {
    Remove-Item -Force (Join-Path $distDir "libmpv-2.dll")
}

# Copiar extrator yt-dlp.exe para a pasta bin/
Copy-Item -Force (Join-Path $rootDir "compile/bin/yt-dlp.exe") $distBinDir

# Copiar icone para a pasta assets/ da versao portatil
$distAssetsDir = Join-Path $distDir "assets"
New-Item -ItemType Directory -Force -Path $distAssetsDir | Out-Null
Copy-Item -Force (Join-Path $rootDir "assets/freenamp.ico") $distAssetsDir

Write-Host "`n[Freenamp] Pacote portatil standalone gerado com sucesso em:"
Write-Host "$distDir"
Get-ChildItem -Recurse $distDir | Select-Object Name, Length
