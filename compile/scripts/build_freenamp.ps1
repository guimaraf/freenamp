[CmdletBinding()]
param([string]$InstallDir, [switch]$SkipPackage)
$ErrorActionPreference = 'Stop'
$rootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildDir = Join-Path $rootDir 'build'
cmake -B $buildDir -S $rootDir -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
cmake --build $buildDir --config Release --target freenamp_core freenamp
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed.' }
if (-not $SkipPackage) {
    $arguments = @{ SkipBuild=$true }
    if ($InstallDir) { $arguments.InstallDir = $InstallDir }
    & (Join-Path $PSScriptRoot 'build_portable_release.ps1') @arguments
}
