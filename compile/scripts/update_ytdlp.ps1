# Update a portable installation independently of the application build.
[CmdletBinding()]
param([string]$InstallDir, [string]$StatusFile)
$ErrorActionPreference = 'Stop'
$rootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $InstallDir) { $InstallDir = Join-Path $rootDir 'build/freenamp_portable' }
$arguments = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'freenamp-update.ps1'),'-Action','UpdateExtractor','-InstallDir',([IO.Path]::GetFullPath($InstallDir)))
if ($StatusFile) { $arguments += @('-StatusFile',([IO.Path]::GetFullPath($StatusFile))) }
& powershell.exe @arguments
if ($LASTEXITCODE -ne 0) { throw 'Extractor update failed.' }
