# Deterministic offline coverage for the shipped PowerShell 5.1 updater.
$ErrorActionPreference = 'Stop'
$originalToken = $env:GITHUB_TOKEN
$env:GITHUB_TOKEN = 'offline-test-token'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testRoot = Join-Path $root ('build/updater-tests-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$global:FreenampTestDownloads = @{}
$global:FreenampTestRelaunched = $false
function Assert($Condition, $Message) { if (-not $Condition) { throw $Message }; Write-Host ('PASS: ' + $Message) }
function Invoke-RestMethod {
    param($Uri,$Headers,$TimeoutSec)
    if ($Uri -match '/repos/yt-dlp/yt-dlp-nightly-builds/') { return $global:FreenampTestYtRelease }
    if ($Uri -match '/repos/denoland/deno/') { return $global:FreenampTestDenoRelease }
    return $global:FreenampTestRelease
}
function Invoke-WebRequest { param($Uri,$Headers,$OutFile,$TimeoutSec,[switch]$UseBasicParsing) Copy-Item -LiteralPath $global:FreenampTestDownloads[$Uri] -Destination $OutFile }
function Get-Process { param($Id,$Name,$ErrorAction) return $null }
function Start-Process {
    param($FilePath,$WorkingDirectory)
    $global:FreenampTestRelaunched = $true
    $state = Get-Content -LiteralPath $global:FreenampTestStatusPath -Raw | ConvertFrom-Json
    Assert ($state.state -in @('success','error')) 'Status exists before relaunch'
}
function Run-Updater($Action) {
    & $script:Updater -Action $Action -InstallDir $script:Install -StatusFile $global:FreenampTestStatusPath -ParentPid 999999 | Out-Null
    $result = Get-Content -LiteralPath $global:FreenampTestStatusPath -Raw | ConvertFrom-Json
    Write-Host ($Action + ': ' + $result.state + ' - ' + $result.message)
    return $result
}
try {
    # Redirect OS-local staging into this disposable workspace fixture only.
    $text = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'freenamp-update.ps1') -Raw
    $text = $text.Replace('function Get-PublicTag($Repository) {', 'function Get-PublicTag($Repository) { if ($Repository -eq ''guimaraf/freenamp'') { return ''v1.1.0'' }; throw ''Fixture unavailable''')
    $text = $text.Replace("[Environment]::GetFolderPath('LocalApplicationData')", ("'" + $testRoot.Replace("'","''") + "'"))
    $script:Updater = Join-Path $testRoot 'updater.ps1'
    [IO.File]::WriteAllText($script:Updater,$text)
    $script:Install = Join-Path $testRoot 'portable with spaces'
    $global:FreenampTestStatusPath = Join-Path $script:Install '.freenamp-last-update.json'
    foreach($folder in @('','core','bin','cache')) { [IO.Directory]::CreateDirectory((Join-Path $script:Install $folder)) | Out-Null }
    '{"version":"1.0.0"}' | Set-Content -LiteralPath (Join-Path $script:Install 'release-info.json')
    'old executable' | Set-Content -LiteralPath (Join-Path $script:Install 'freenamp.exe')
    'saved playlist' | Set-Content -LiteralPath (Join-Path $script:Install 'cache/playlists.json')
    'newer extractor' | Set-Content -LiteralPath (Join-Path $script:Install 'bin/yt-dlp.exe')
    'local deno' | Set-Content -LiteralPath (Join-Path $script:Install 'bin/deno.exe')
    $fixture = Join-Path $testRoot 'fixture'
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'core')) | Out-Null
    foreach($file in @('freenamp.exe','freenamp-update.ps1','core/libfreenamp_core.dll','core/SDL2.dll','core/libmpv-2.dll')) { 'new payload' | Set-Content -LiteralPath (Join-Path $fixture $file) }
    '{"version":"1.1.0"}' | Set-Content -LiteralPath (Join-Path $fixture 'release-info.json')
    $archive = Join-Path $testRoot 'release.zip'
    Compress-Archive -Path (Join-Path $fixture '*') -DestinationPath $archive
    $assetUrl = 'https://api.github.com/repos/guimaraf/freenamp/releases/assets/123'
    $global:FreenampTestDownloads[$assetUrl] = $archive
    $global:FreenampTestRelease = [pscustomobject]@{ tag_name='v1.1.0'; draft=$false; prerelease=$false; assets=@([pscustomobject]@{name='freenamp-windows-x64.zip';url=$assetUrl;digest=('sha256:' + (Get-FileHash -LiteralPath $archive).Hash)}) }
    Assert ((Run-Updater 'CheckApp').state -eq 'available') 'CheckApp detects newer official stable version'
    Assert ((Run-Updater 'StageApp').state -eq 'ready') 'StageApp verifies and stages official ZIP'
    Assert ((Get-Content -LiteralPath (Join-Path $script:Install 'freenamp.exe') -Raw).Trim() -eq 'old executable') 'Staging leaves application untouched'
    Assert ((Run-Updater 'ApplyApp').state -eq 'success') 'ApplyApp installs staged application'
    Assert $global:FreenampTestRelaunched 'ApplyApp relaunches target installation'
    Assert ((Get-Content -LiteralPath (Join-Path $script:Install 'cache/playlists.json') -Raw).Trim() -eq 'saved playlist') 'Application update preserves cache'
    Assert ((Get-Content -LiteralPath (Join-Path $script:Install 'bin/yt-dlp.exe') -Raw).Trim() -eq 'newer extractor') 'Application update preserves independently updated yt-dlp'
    Assert ((Get-Content -LiteralPath (Join-Path $script:Install 'bin/deno.exe') -Raw).Trim() -eq 'local deno') 'Application update preserves Deno'
    Assert ((Run-Updater 'CheckApp').state -eq 'up-to-date') 'CheckApp recognizes current application'
    '{"version":"1.0.0"}' | Set-Content -LiteralPath (Join-Path $script:Install 'release-info.json')
    $global:FreenampTestRelease.assets[0].digest = 'sha256:' + ('0' * 64)
    Assert ((Run-Updater 'StageApp').state -eq 'error') 'SHA256 mismatch rejects staging'
    $global:FreenampTestRelease.assets[0].digest = 'sha256:' + (Get-FileHash -LiteralPath $archive).Hash
    Assert ((Run-Updater 'StageApp').state -eq 'ready') 'Valid package can be staged after failed verification'
    $planFile = Get-ChildItem -LiteralPath (Join-Path $testRoot 'Freenamp/updates') -Filter plan.json -File -Recurse | Select-Object -First 1
    $plan = Get-Content -LiteralPath $planFile.FullName -Raw | ConvertFrom-Json
    $stagedExe = Get-Item -LiteralPath (Join-Path $planFile.DirectoryName ($plan.payload + '/freenamp.exe'))
    'tampered' | Set-Content -LiteralPath $stagedExe.FullName
    Assert ((Run-Updater 'ApplyApp').state -eq 'error') 'ApplyApp rejects modified staged payload'
    Assert ((Run-Updater 'StageApp').state -eq 'ready') 'Restage before rollback test'
    'rollback original' | Set-Content -LiteralPath (Join-Path $script:Install 'freenamp.exe')
    $global:FreenampTestFailTarget = Join-Path $script:Install 'core/libmpv-2.dll'
    function Copy-Item {
        param($LiteralPath,$Destination,[switch]$Force)
        if ($global:FreenampTestFailTarget -and $Destination.StartsWith($global:FreenampTestFailTarget + '.freenamp-', [StringComparison]::OrdinalIgnoreCase)) {
            $global:FreenampTestFailTarget = ''
            throw 'Simulated locked destination'
        }
        Microsoft.PowerShell.Management\Copy-Item -LiteralPath $LiteralPath -Destination $Destination -Force:$Force
    }
    Assert ((Run-Updater 'ApplyApp').state -eq 'error') 'Copy failure reports rollback error'
    Assert ((Get-Content -LiteralPath (Join-Path $script:Install 'freenamp.exe') -Raw).Trim() -eq 'rollback original') 'Copy failure restores original executable'
    Assert ((Get-Content -LiteralPath (Join-Path $script:Install 'release-info.json') -Raw | ConvertFrom-Json).version -eq '1.0.0') 'Copy failure restores version metadata'
    $global:FreenampTestRelease.tag_name = 'latest'
    Assert ((Run-Updater 'CheckApp').state -eq 'error') 'Non-semver release rejected'
    $global:FreenampTestRelease.tag_name = 'v1.1.0'; $global:FreenampTestRelease.prerelease = $true
    Assert ((Run-Updater 'CheckApp').state -eq 'error') 'Prerelease rejected'
    $global:FreenampTestRelease.prerelease = $false
    $global:FreenampTestRelease.assets[0].digest = ''
    $checks = Join-Path $testRoot 'SHA256SUMS.txt'
    ((Get-FileHash -LiteralPath $archive).Hash + '  freenamp-windows-x64.zip') | Set-Content -LiteralPath $checks
    $checksUrl = 'https://api.github.com/repos/guimaraf/freenamp/releases/assets/124'
    $global:FreenampTestDownloads[$checksUrl] = $checks
    $global:FreenampTestRelease.assets += [pscustomobject]@{name='SHA256SUMS.txt';url=$checksUrl;digest=''}
    Assert ((Run-Updater 'StageApp').state -eq 'ready') 'Published checksum fallback verifies package without API digest'
    $evilZip = Join-Path $testRoot 'unsafe.zip'
    $zip = [IO.Compression.ZipFile]::Open($evilZip, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entry = $zip.CreateEntry('../escape.txt')
        $writer = New-Object IO.StreamWriter($entry.Open())
        $writer.Write('unsafe'); $writer.Dispose()
    } finally { $zip.Dispose() }
    $global:FreenampTestDownloads[$assetUrl] = $evilZip
    $global:FreenampTestRelease.assets[0].digest = 'sha256:' + (Get-FileHash -LiteralPath $evilZip).Hash
    Assert ((Run-Updater 'StageApp').state -eq 'error') 'ZIP traversal rejected after valid archive hash'
    Assert (-not (Test-Path -LiteralPath (Join-Path $testRoot 'escape.txt'))) 'ZIP traversal creates no external file'
    $fixtureExe = Join-Path $testRoot 'fixture-version.exe'
    Add-Type -TypeDefinition 'public class FreenampFixtureVersion { public static void Main() { System.Console.WriteLine("2026.11.25.000000"); } }' -OutputAssembly $fixtureExe -OutputType ConsoleApplication
    $denoFixture = Join-Path $testRoot 'deno-fixture'; [IO.Directory]::CreateDirectory($denoFixture) | Out-Null
    Copy-Item -LiteralPath $fixtureExe -Destination (Join-Path $denoFixture 'deno.exe')
    $denoZip = Join-Path $testRoot 'deno-fixture.zip'
    Compress-Archive -Path (Join-Path $denoFixture '*') -DestinationPath $denoZip
    $ytUrl = 'https://api.github.com/repos/yt-dlp/yt-dlp-nightly-builds/releases/assets/125'
    $denoUrl = 'https://api.github.com/repos/denoland/deno/releases/assets/126'
    $global:FreenampTestDownloads[$ytUrl] = $fixtureExe; $global:FreenampTestDownloads[$denoUrl] = $denoZip
    $global:FreenampTestYtRelease = [pscustomobject]@{tag_name='2026.11.25.000000'; assets=@([pscustomobject]@{name='yt-dlp.exe';url=$ytUrl;digest=('sha256:'+(Get-FileHash -LiteralPath $fixtureExe).Hash)})}
    $global:FreenampTestDenoRelease = [pscustomobject]@{tag_name='v2.9.8'; assets=@([pscustomobject]@{name='deno-x86_64-pc-windows-msvc.zip';url=$denoUrl;digest=('sha256:'+(Get-FileHash -LiteralPath $denoZip).Hash)})}
    Assert ((Run-Updater 'CheckExtractor').state -eq 'available') 'CheckExtractor identifies missing or outdated binaries'
    $combined = Run-Updater 'CheckUpdates'
    Assert ($combined.state -eq 'checked' -and $combined.app.state -eq 'available' -and $combined.extractor.state -eq 'available') 'Combined check reports independent update states'
    $global:FreenampTestDenoRelease.tag_name = 'invalid'
    $combined = Run-Updater 'CheckUpdates'
    Assert ($combined.app.state -eq 'available' -and $combined.extractor.state -eq 'error') 'Extractor failure preserves application result'
    $global:FreenampTestDenoRelease.tag_name = 'v2.9.8'
    Assert ((Run-Updater 'StageExtractor').state -eq 'ready') 'StageExtractor verifies both official release assets'
    Assert ((Get-Content -LiteralPath (Join-Path $script:Install 'bin/yt-dlp.exe') -Raw).Trim() -eq 'newer extractor') 'StageExtractor leaves active extractor untouched'
    Assert ((Get-Content -LiteralPath (Join-Path $script:Install 'bin/deno.exe') -Raw).Trim() -eq 'local deno') 'StageExtractor leaves active Deno untouched'
    Assert ((Run-Updater 'ApplyExtractor').state -eq 'success') 'ApplyExtractor installs verified staged pair'
    Assert ((Get-FileHash -LiteralPath (Join-Path $script:Install 'bin/yt-dlp.exe')).Hash -eq (Get-FileHash -LiteralPath $fixtureExe).Hash) 'Installed extractor hash matches verified stage'
    Assert ((Get-FileHash -LiteralPath (Join-Path $script:Install 'bin/deno.exe')).Hash -eq (Get-FileHash -LiteralPath $fixtureExe).Hash) 'Installed Deno hash matches verified stage'
    Assert ((Run-Updater 'ApplyExtractor').state -eq 'error') 'Consumed extractor plan cannot be reapplied'
    Assert ((Run-Updater 'UpdateExtractor').state -eq 'success') 'Combined CLI extractor action still works'
    $denoVersionExe = Join-Path $testRoot 'deno-version.exe'
    Add-Type -TypeDefinition 'public class FreenampDenoVersion { public static void Main() { System.Console.WriteLine("deno 2.9.8 (stable)"); } }' -OutputAssembly $denoVersionExe -OutputType ConsoleApplication
    Copy-Item -LiteralPath $denoVersionExe -Destination (Join-Path $script:Install 'bin/deno.exe') -Force
    $global:FreenampTestDownloads.Clear()
    Assert ((Run-Updater 'StageExtractor').state -eq 'up-to-date') 'Current yt-dlp and Deno skip downloads'
    '{"version":"1.1.0"}' | Set-Content -LiteralPath (Join-Path $script:Install 'release-info.json')
    $global:FreenampTestRelease.tag_name = 'v1.1.0'
    Assert ((Run-Updater 'StageApp').state -eq 'up-to-date') 'Current application skips downloads'
    $env:GITHUB_TOKEN = ''
    $publicBase = 'https://github.com/guimaraf/freenamp/releases/download/v1.1.0/'
    $global:FreenampTestDownloads[$publicBase + 'freenamp-windows-x64.zip'] = $archive
    $global:FreenampTestDownloads[$publicBase + 'SHA256SUMS.txt'] = $checks
    '{"version":"1.0.0"}' | Set-Content -LiteralPath (Join-Path $script:Install 'release-info.json')
    Assert ((Run-Updater 'CheckApp').state -eq 'available') 'Public latest release works without API token'
    Assert ((Run-Updater 'StageApp').state -eq 'ready') 'Public download validates published checksum without API token'
    Write-Host 'All offline updater tests passed.'
} finally {
    $env:GITHUB_TOKEN = $originalToken
    $resolved = [IO.Path]::GetFullPath($testRoot)
    if ($resolved.StartsWith(([IO.Path]::GetFullPath((Join-Path $root 'build')) + '\'),[StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
