[CmdletBinding()]
param(
    [string]$InstallDir,
    [switch]$SkipBuild,
    [switch]$RefreshExtractors
)
$ErrorActionPreference = 'Stop'
$rootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildDir = Join-Path $rootDir 'build'
if (-not $InstallDir) { $InstallDir = Join-Path $buildDir 'freenamp_portable' }
$InstallDir = [IO.Path]::GetFullPath($InstallDir)
if ($InstallDir.TrimEnd('\') -ieq $rootDir.TrimEnd('\') -or (Test-Path -LiteralPath (Join-Path $InstallDir '.git'))) { throw 'Choose a portable folder separate from the checkout.' }
# Never kill an instance: Windows file locking provides an actionable failure instead.
if (Get-Process -Name freenamp -ErrorAction SilentlyContinue | Where-Object { $_.Path -ieq (Join-Path $InstallDir 'freenamp.exe') }) { throw 'Close Freenamp in the target portable folder before packaging.' }
if (-not $SkipBuild) {
    cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw 'CMake build failed.' }
}
foreach ($folder in @('', 'core', 'bin', 'assets', 'cache')) { [IO.Directory]::CreateDirectory((Join-Path $InstallDir $folder)) | Out-Null }
function Copy-Required($Candidates, $Destination) {
    $source = $Candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $source) { throw ('Required package component missing: ' + ($Candidates -join ', ')) }
    Copy-Item -LiteralPath $source -Destination $Destination -Force
}
Copy-Required @((Join-Path $buildDir 'frontend/freenamp.exe'), (Join-Path $buildDir 'freenamp.exe')) (Join-Path $InstallDir 'freenamp.exe')
Copy-Required @((Join-Path $buildDir 'frontend/core/libfreenamp_core.dll'), (Join-Path $buildDir 'frontend/libfreenamp_core.dll')) (Join-Path $InstallDir 'core/libfreenamp_core.dll')
Copy-Required @((Join-Path $buildDir 'frontend/core/SDL2.dll'), (Join-Path $rootDir 'compile/libs/SDL2-2.30.12/x86_64-w64-mingw32/bin/SDL2.dll')) (Join-Path $InstallDir 'core/SDL2.dll')
Copy-Required @((Join-Path $buildDir 'frontend/core/libmpv-2.dll'), (Join-Path $rootDir 'compile/libs/mpv/libmpv-2.dll')) (Join-Path $InstallDir 'core/libmpv-2.dll')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'freenamp-update.ps1') -Destination $InstallDir -Force
foreach ($asset in @('freenamp.ico','frenamp.png','freenamp.png')) {
    $source = Join-Path $rootDir ('assets/' + $asset)
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $InstallDir 'assets') -Force }
}
Copy-Item -LiteralPath (Join-Path $rootDir 'README.md') -Destination $InstallDir -Force
$cmakeText = Get-Content -LiteralPath (Join-Path $rootDir 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(Freenamp\s+VERSION\s+(\d+\.\d+\.\d+)') { throw 'Cannot read application version.' }
@{ version=$Matches[1]; repository='guimaraf/freenamp'; platform='windows-x64' } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $InstallDir 'release-info.json') -Encoding UTF8
foreach ($name in @('yt-dlp.exe','deno.exe')) {
    $source = Join-Path $rootDir ('compile/bin/' + $name)
    $destination = Join-Path $InstallDir ('bin/' + $name)
    # Application packaging never replaces independently updated extractor binaries.
    if (-not (Test-Path -LiteralPath $destination) -and (Test-Path -LiteralPath $source)) { Copy-Item -LiteralPath $source -Destination $destination }
}
if ($RefreshExtractors -or -not (Test-Path -LiteralPath (Join-Path $InstallDir 'bin/yt-dlp.exe')) -or -not (Test-Path -LiteralPath (Join-Path $InstallDir 'bin/deno.exe'))) {
    & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'freenamp-update.ps1') -Action UpdateExtractor -InstallDir $InstallDir
    if ($LASTEXITCODE -ne 0) { throw 'Verified extractor provisioning failed.' }
}
Write-Host "Portable package ready: $InstallDir"
