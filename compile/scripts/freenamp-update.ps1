[CmdletBinding()]
param(
    [ValidateSet('CheckApp','StageApp','ApplyApp','StageExtractor','ApplyExtractor','UpdateExtractor')][string]$Action = 'CheckApp',
    [Parameter(Mandatory=$true)][string]$InstallDir,
    [string]$StatusFile,
    [int]$ParentPid = 0
)
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$script:Repository = 'guimaraf/freenamp'
$script:Lock = $null
$resumeApp = $false
function Write-Status($State, $Message, $Version = '', $Progress = 0) {
    $result = @{ state=$State; message=$Message; version=$Version; progress=$Progress } | ConvertTo-Json -Compress
    if ($StatusFile) {
        $parent = Split-Path -Parent $StatusFile
        [IO.Directory]::CreateDirectory($parent) | Out-Null
        $temp = $StatusFile + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
        [IO.File]::WriteAllText($temp, $result, (New-Object Text.UTF8Encoding($false)))
        if (Test-Path -LiteralPath $StatusFile) {
            $previous = $temp + '.previous'
            [IO.File]::Replace($temp, $StatusFile, $previous)
            [IO.File]::Delete($previous)
        }
        else { [IO.File]::Move($temp, $StatusFile) }
    }
    Write-Output $result
}
function Api-Headers {
    $headers = @{ 'User-Agent'='Freenamp-Updater'; Accept='application/vnd.github+json' }
    if ($env:GITHUB_TOKEN) { $headers.Authorization = 'Bearer ' + $env:GITHUB_TOKEN }
    return $headers
}
function Get-Release($Repository, $Tag = 'latest') {
    if ($Tag -ne 'latest') { $Tag = 'tags/' + [uri]::EscapeDataString($Tag) }
    Invoke-RestMethod -Uri "https://api.github.com/repos/$Repository/releases/$Tag" -Headers (Api-Headers) -TimeoutSec 60
}
function Download-Asset($Asset, $Destination) {
    if (-not $Asset -or $Asset.url -notmatch '^https://api\.github\.com/repos/[^/]+/[^/]+/releases/assets/\d+$') { throw 'Invalid GitHub release asset.' }
    $headers = Api-Headers
    $headers.Accept = 'application/octet-stream'
    Invoke-WebRequest -UseBasicParsing -Uri $Asset.url -Headers $headers -OutFile $Destination -TimeoutSec 300 | Out-Null
    if (-not (Test-Path -LiteralPath $Destination) -or (Get-Item -LiteralPath $Destination).Length -eq 0) { throw 'Empty download.' }
}
function Verified-Asset($Release, $Name, $Destination, $ChecksumName = 'SHA256SUMS') {
    $asset = @($Release.assets | Where-Object { $_.name -ceq $Name })
    if ($asset.Count -ne 1) { throw "Required release asset missing: $Name" }
    $expected = ''
    if ($asset[0].digest -match '^sha256:([a-fA-F0-9]{64})$') { $expected = $Matches[1] }
    else {
        $checks = @($Release.assets | Where-Object { $_.name -ceq $ChecksumName })
        if ($checks.Count -ne 1) { throw "No SHA256 verification available for $Name." }
        $checksumFile = $Destination + '.sha256'
        Download-Asset $checks[0] $checksumFile
        foreach ($line in [IO.File]::ReadAllLines($checksumFile)) {
            if ($line -match '^([a-fA-F0-9]{64})\s+\*?(.+)$' -and $Matches[2].Trim() -ceq $Name) { $expected = $Matches[1]; break }
        }
        Remove-Item -LiteralPath $checksumFile -Force
    }
    if (-not $expected) { throw "SHA256 entry missing for $Name." }
    Download-Asset $asset[0] $Destination
    if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -ine $expected) { throw "SHA256 mismatch: $Name" }
}
function Assert-NoLinks($Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Symbolic links and junctions are not supported in update paths.' }
        $next = Split-Path -Parent $cursor
        if ($next -eq $cursor) { break }; $cursor = $next
    }
}
function Install-FileAtomic($Source, $Target) {
    $temporary = $Target + '.freenamp-' + [guid]::NewGuid().ToString('N') + '.tmp'
    $previous = $temporary + '.previous'
    try {
        Copy-Item -LiteralPath $Source -Destination $temporary
        if (Test-Path -LiteralPath $Target) { [IO.File]::Replace($temporary, $Target, $previous) }
        else { [IO.File]::Move($temporary, $Target) }
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
        if (Test-Path -LiteralPath $previous) { Remove-Item -LiteralPath $previous -Force }
    }
}
function Expand-SafeZip($Archive, $Destination) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Directory]::CreateDirectory($Destination) | Out-Null
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\','/')
            if ($name -match '(^/|:|(^|/)\.\.(/|$))' -or (($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) { throw 'Unsafe ZIP entry.' }
            $target = [IO.Path]::GetFullPath((Join-Path $Destination $name))
            if (-not $target.StartsWith($Destination.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'ZIP entry escapes staging.' }
            if ($name.EndsWith('/')) { [IO.Directory]::CreateDirectory($target) | Out-Null; continue }
            [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $false)
        }
    } finally { $zip.Dispose() }
}
function Read-AppVersion($Directory) {
    $metadata = Join-Path $Directory 'release-info.json'
    if (-not (Test-Path -LiteralPath $metadata)) { throw 'This installation has no release-info.json; install a current portable package first.' }
    $info = Get-Content -LiteralPath $metadata -Raw | ConvertFrom-Json
    if ($info.version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid installed application version.' }
    return [version]$info.version
}
function Latest-App {
    $release = Get-Release $script:Repository
    if ($release.draft -or $release.prerelease -or $release.tag_name -notmatch '^v(\d+\.\d+\.\d+)$') { throw 'Latest release is not an official stable version.' }
    return @{ release=$release; version=[version]$Matches[1] }
}
function App-Files($Directory) {
    foreach ($file in Get-ChildItem -LiteralPath $Directory -File -Recurse) {
        $relative = $file.FullName.Substring($Directory.TrimEnd('\').Length + 1).Replace('\','/')
        if ($relative -match '^(cache/|bin/)') { continue }
        if ($relative -notmatch '^(freenamp\.exe|freenamp-update\.ps1|release-info\.json|README\.md|core/[^:]+|assets/[^:]+)$') { throw "Unexpected application payload: $relative" }
        [pscustomobject]@{ path=$relative; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
}
function Remove-StagedDirectory($Path) {
    $full = [IO.Path]::GetFullPath($Path)
    $stagePrefix = [IO.Path]::GetFullPath($stage).TrimEnd('\') + '\'
    if (-not $full.StartsWith($stagePrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Cleanup outside updater staging is forbidden.' }
    if (-not (Test-Path -LiteralPath $full)) { return }
    Assert-NoLinks $full
    foreach ($entry in Get-ChildItem -LiteralPath $full -Force -Recurse) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Unsafe link in staging cleanup.' }
    }
    Remove-Item -LiteralPath $full -Recurse -Force
}
function Stage-Extractor {
    Write-Status 'progress' 'Verificando versoes' '' 5 | Out-Null
    $yt = Get-Release 'yt-dlp/yt-dlp-nightly-builds'
    $deno = Get-Release 'denoland/deno'
    $installedYt = Join-Path $InstallDir 'bin/yt-dlp.exe'
    $installedDeno = Join-Path $InstallDir 'bin/deno.exe'
    if ($yt.tag_name -match '^\d{4}\.\d{2}\.\d{2}\.\d+$' -and
        $deno.tag_name -match '^v?(\d+\.\d+\.\d+)$' -and
        (Test-Path -LiteralPath $installedYt -PathType Leaf) -and
        (Test-Path -LiteralPath $installedDeno -PathType Leaf)) {
        $currentYt = & $installedYt --ignore-config --version 2>$null
        $ytText = [string]($currentYt | Select-Object -First 1)
        $ytOk = $LASTEXITCODE -eq 0 -and $ytText -match '^\d{4}\.\d{2}\.\d{2}\.\d+$' -and [version]$ytText -ge [version]$yt.tag_name
        $currentDeno = & $installedDeno --version 2>$null
        $denoText = [string]($currentDeno | Select-Object -First 1)
        $denoOk = $LASTEXITCODE -eq 0 -and $denoText -match '^deno\s+(\d+\.\d+\.\d+)(\s|$)' -and [version]$Matches[1] -ge [version]$deno.tag_name.TrimStart('v')
        if ($ytOk -and $denoOk) { return @{ current=$true; version=$yt.tag_name } }
    }
    $workName = 'extractor-' + [guid]::NewGuid().ToString('N')
    $work = Join-Path $stage $workName
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $manifest = Join-Path $stage 'extractor-plan.json'
    if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest -Force }
    try {
        Write-Status 'progress' 'Baixando yt-dlp' '' 20 | Out-Null
        Verified-Asset $yt 'yt-dlp.exe' (Join-Path $work 'yt-dlp.exe') 'SHA2-256SUMS'
        Write-Status 'progress' 'Baixando Deno' '' 55 | Out-Null
        Verified-Asset $deno 'deno-x86_64-pc-windows-msvc.zip' (Join-Path $work 'deno.zip') 'deno-x86_64-pc-windows-msvc.zip.sha256sum'
        Write-Status 'progress' 'Verificando arquivos' '' 80 | Out-Null
        Expand-SafeZip (Join-Path $work 'deno.zip') (Join-Path $work 'deno')
        $ytVersion = & (Join-Path $work 'yt-dlp.exe') --version
        if ($LASTEXITCODE -ne 0 -or -not $ytVersion) { throw 'Downloaded yt-dlp failed validation.' }
        $denoVersion = & (Join-Path $work 'deno/deno.exe') --version
        if ($LASTEXITCODE -ne 0 -or -not $denoVersion) { throw 'Downloaded Deno failed validation.' }
        $files = @(
            @{name='yt-dlp.exe'; source='yt-dlp.exe'; sha256=(Get-FileHash -LiteralPath (Join-Path $work 'yt-dlp.exe')).Hash},
            @{name='deno.exe'; source='deno/deno.exe'; sha256=(Get-FileHash -LiteralPath (Join-Path $work 'deno/deno.exe')).Hash}
        )
        $version = [string]($ytVersion | Select-Object -First 1)
        @{installDir=$InstallDir; payload=$workName; version=$version; files=$files} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest -Encoding UTF8
        return @{ current=$false; version=$version }
    } catch { Remove-StagedDirectory $work; throw }
}
function Apply-Extractor {
    $manifest = Join-Path $stage 'extractor-plan.json'
    $plan = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if ($plan.installDir -ine $InstallDir -or $plan.payload -notmatch '^extractor-[a-f0-9]{32}$' -or @($plan.files).Count -ne 2) { throw 'Invalid staged extractor manifest.' }
    $work = Join-Path $stage $plan.payload
    Assert-NoLinks $work
    foreach ($name in @('yt-dlp.exe','deno.exe')) {
        $record = @($plan.files | Where-Object { $_.name -ceq $name })
        $expectedSource = $name
        if ($name -eq 'deno.exe') { $expectedSource = 'deno/deno.exe' }
        if ($record.Count -ne 1 -or $record[0].source -cne $expectedSource) { throw 'Invalid staged extractor file.' }
        $source = Join-Path $work $expectedSource; Assert-NoLinks $source
        if ((Get-FileHash -LiteralPath $source).Hash -cne $record[0].sha256) { throw 'Staged extractor checksum changed.' }
    }
    $bin = Join-Path $InstallDir 'bin'; Assert-NoLinks $bin
    [IO.Directory]::CreateDirectory($bin) | Out-Null
    $replaced = New-Object Collections.ArrayList
    try {
        foreach ($file in $plan.files) {
            $source = Join-Path $work $file.source
            $target = Join-Path $bin $file.name; Assert-NoLinks $target
            $saved = Join-Path $work ($file.name + '.previous'); $existed = Test-Path -LiteralPath $target
            if ($existed) { Copy-Item -LiteralPath $target -Destination $saved }
            Install-FileAtomic $source $target
            [void]$replaced.Add(@{ target=$target; saved=$saved; existed=$existed })
        }
    } catch {
        foreach ($entry in $replaced) {
            if ($entry.existed) { Install-FileAtomic $entry.saved $entry.target }
            elseif (Test-Path -LiteralPath $entry.target) { Remove-Item -LiteralPath $entry.target -Force }
        }; throw
    }
    Remove-Item -LiteralPath $manifest -Force
    Remove-StagedDirectory $work
    return $plan.version
}
try {
    if (-not [IO.Path]::IsPathRooted($InstallDir)) { throw 'InstallDir must be absolute.' }
    if ($StatusFile -and -not [IO.Path]::IsPathRooted($StatusFile)) { throw 'StatusFile must be absolute.' }
    $InstallDir = [IO.Path]::GetFullPath($InstallDir).TrimEnd('\')
    Assert-NoLinks $InstallDir
    if (Test-Path -LiteralPath (Join-Path $InstallDir '.git')) { throw 'Use a portable installation separate from the source checkout.' }
    $hashBytes = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($InstallDir.ToLowerInvariant()))
    $key = ([BitConverter]::ToString($hashBytes)).Replace('-','').Substring(0,24)
    $stage = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) "Freenamp\updates\$key"
    Assert-NoLinks $stage
    [IO.Directory]::CreateDirectory($stage) | Out-Null
    $script:Lock = [IO.File]::Open((Join-Path $stage 'update.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
    $planFile = Join-Path $stage 'plan.json'
    switch ($Action) {
        'CheckApp' {
            $current = Read-AppVersion $InstallDir; $latest = Latest-App
            if ($latest.version -gt $current) { Write-Status 'available' 'Nova versao do Freenamp disponivel.' $latest.version.ToString() }
            else { Write-Status 'up-to-date' 'Freenamp atualizado.' $current.ToString() }
        }
        'StageApp' {
            Write-Status 'progress' 'Verificando versao' '' 5 | Out-Null
            $current = Read-AppVersion $InstallDir; $latest = Latest-App
            if ($latest.version -le $current) { Write-Status 'up-to-date' 'Freenamp atualizado.' $current.ToString(); break }
            if (Test-Path -LiteralPath $planFile) {
                $oldPlan = Get-Content -LiteralPath $planFile -Raw | ConvertFrom-Json
                if ($oldPlan.payload -match '^payload-[a-f0-9]{32}$') { Remove-StagedDirectory (Join-Path $stage $oldPlan.payload) }
                Remove-Item -LiteralPath $planFile -Force
            }
            $payloadName = 'payload-' + [guid]::NewGuid().ToString('N')
            $payload = Join-Path $stage $payloadName
            $archive = Join-Path $stage 'application.zip'
            Write-Status 'progress' 'Baixando Freenamp' '' 20 | Out-Null
            Verified-Asset $latest.release 'freenamp-windows-x64.zip' $archive 'SHA256SUMS.txt'
            Write-Status 'progress' 'Verificando pacote' '' 80 | Out-Null
            Expand-SafeZip $archive $payload
            foreach ($required in @('freenamp.exe','core/libfreenamp_core.dll','core/SDL2.dll','core/libmpv-2.dll','freenamp-update.ps1','release-info.json')) {
                if (-not (Test-Path -LiteralPath (Join-Path $payload $required) -PathType Leaf)) { throw "Incomplete application payload: $required" }
            }
            if ((Read-AppVersion $payload) -ne $latest.version) { throw 'Release tag and package version do not match.' }
            $files = @(App-Files $payload)
            @{ installDir=$InstallDir; version=$latest.version.ToString(); payload=$payloadName; files=$files } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $planFile -Encoding UTF8
            Remove-Item -LiteralPath $archive -Force
            Write-Status 'ready' 'Atualizacao verificada e pronta para instalar.' $latest.version.ToString()
        }
        'ApplyApp' {
            if ($ParentPid -le 0 -or $ParentPid -eq $PID) { throw 'ApplyApp requires the original application PID.' }
            $parent = Get-Process -Id $ParentPid -ErrorAction SilentlyContinue
            if ($parent) {
                if ($parent.Path -ine (Join-Path $InstallDir 'freenamp.exe')) { throw 'Parent PID does not belong to the target installation.' }
                if (-not $parent.WaitForExit(120000)) { throw 'Timed out waiting for Freenamp to exit.' }
            }
            if (Get-Process -Name freenamp -ErrorAction SilentlyContinue | Where-Object { $_.Path -ieq (Join-Path $InstallDir 'freenamp.exe') }) { throw 'Target installation is still running.' }
            $resumeApp = $true
            $plan = Get-Content -LiteralPath $planFile -Raw | ConvertFrom-Json
            if ($plan.installDir -ine $InstallDir -or $plan.version -notmatch '^\d+\.\d+\.\d+$') { throw 'Staged installation does not match.' }
            if ($plan.payload -notmatch '^payload-[a-f0-9]{32}$') { throw 'Invalid staging directory.' }
            $payload = Join-Path $stage $plan.payload
            Assert-NoLinks $payload
            if ([version]$plan.version -le (Read-AppVersion $InstallDir)) { throw 'Staged update is no longer newer than the installed application.' }
            $actual = @(App-Files $payload)
            if ($actual.Count -ne @($plan.files).Count) { throw 'Staged payload changed.' }
            foreach ($file in $actual) {
                $record = @($plan.files | Where-Object { $_.path -ceq $file.path -and $_.sha256 -ceq $file.sha256 })
                if ($record.Count -ne 1) { throw 'Staged payload checksum changed.' }
            }
            $backup = Join-Path $stage ('backup-' + [guid]::NewGuid().ToString('N'))
            [IO.Directory]::CreateDirectory($backup) | Out-Null
            $changed = New-Object Collections.ArrayList
            $resumeApp = $false
            try {
                foreach ($file in $actual) {
                    $destination = Join-Path $InstallDir $file.path
                    Assert-NoLinks $destination
                    $saved = Join-Path $backup $file.path
                    $existed = Test-Path -LiteralPath $destination
                    if ($existed) { [IO.Directory]::CreateDirectory((Split-Path -Parent $saved)) | Out-Null; Copy-Item -LiteralPath $destination -Destination $saved }
                    [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
                    Install-FileAtomic (Join-Path $payload $file.path) $destination
                    [void]$changed.Add(@{ target=$destination; saved=$saved; existed=$existed })
                }
                Write-Status 'success' 'Freenamp atualizado e reiniciado.' $plan.version
                Start-Process -FilePath (Join-Path $InstallDir 'freenamp.exe') -WorkingDirectory $InstallDir | Out-Null
            } catch {
                foreach ($entry in $changed) {
                    if ($entry.existed) { Install-FileAtomic $entry.saved $entry.target }
                    elseif (Test-Path -LiteralPath $entry.target) { Remove-Item -LiteralPath $entry.target -Force }
                }
                $resumeApp = $true
                throw
            }
            Remove-Item -LiteralPath $planFile -Force -ErrorAction SilentlyContinue
            try { Remove-StagedDirectory $payload; Remove-StagedDirectory $backup } catch { }
        }
        'StageExtractor' {
            $staged = Stage-Extractor
            if ($staged.current) { Write-Status 'up-to-date' 'YouTube ja esta atualizado.' $staged.version }
            else { Write-Status 'ready' 'yt-dlp nightly e Deno verificados e prontos para instalar.' $staged.version }
        }
        'ApplyExtractor' {
            $version = Apply-Extractor
            Write-Status 'success' 'yt-dlp nightly e Deno atualizados com SHA256 verificado.' $version
        }
        'UpdateExtractor' {
            $staged = Stage-Extractor
            if ($staged.current) { Write-Status 'up-to-date' 'YouTube ja esta atualizado.' $staged.version }
            else {
                $version = Apply-Extractor
                Write-Status 'success' 'yt-dlp nightly e Deno atualizados com SHA256 verificado.' $version
            }
        }
    }
} catch {
    if ($Action -eq 'StageApp' -and $payload -and (Test-Path -LiteralPath $payload)) {
        try { Remove-StagedDirectory $payload } catch { }
    }
    # Do not expose request headers or token-bearing exception details in GUI logs.
    Write-Status 'error' ('Atualizacao falhou: ' + $_.Exception.Message.Split([Environment]::NewLine)[0])
    if ($resumeApp) {
        try { Start-Process -FilePath (Join-Path $InstallDir 'freenamp.exe') -WorkingDirectory $InstallDir | Out-Null } catch { }
    }
    exit 1
} finally { if ($script:Lock) { $script:Lock.Dispose() } }
