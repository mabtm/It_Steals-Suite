[CmdletBinding()]
param(
    [ValidateSet('Discover', 'Install', 'Uninstall')][string]$Mode = 'Install',
    [string]$GameDir,
    [string]$InstallDir,
    [string]$DiscoveryFile,
    [string]$PayloadDir = (Join-Path $PSScriptRoot 'payload')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:LoaderUrl = 'https://github.com/LavaGang/MelonLoader/releases/download/v0.7.3/MelonLoader.x64.zip'
$script:LoaderHash = '5b2b2f3d1cd42b59ec886c5bdc2663edae87a0097a4f4a8f58c0965a99dda416'

function Get-ContainedPath([string]$Root, [string]$Relative) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or $Relative -match '[:\x00]' -or
        $Relative -match '^[\\/]' -or ($Relative -split '[\\/]' | Where-Object { $_ -in '.', '..' })) {
        throw "Unsafe package path: $Relative"
    }
    $base = [IO.Path]::GetFullPath($Root).TrimEnd([char[]]'\/')
    $path = [IO.Path]::GetFullPath((Join-Path $base ($Relative -replace '[\\/]', [IO.Path]::DirectorySeparatorChar)))
    if (-not $path.StartsWith($base + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Package path leaves its destination: $Relative"
    }
    return $path
}

function Assert-NoReparsePoint([string]$Path) {
    $current = [IO.Path]::GetFullPath($Path)
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Choose a folder without junctions or symbolic links: $current"
            }
        }
        $parent = [IO.Path]::GetDirectoryName($current)
        if ($parent -eq $current) { break }
        $current = $parent
    }
}

function Test-GameFolder([string]$Folder) {
    try {
        $exe = Join-Path $Folder 'It Steals.exe'
        if (-not (Test-Path -LiteralPath (Join-Path $Folder 'It Steals_Data/Managed/Assembly-CSharp.dll') -PathType Leaf)) { return $false }
        $stream = [IO.File]::OpenRead($exe)
        try {
            $reader = New-Object IO.BinaryReader($stream)
            if ($stream.Length -lt 70 -or $reader.ReadUInt16() -ne 0x5a4d) { return $false }
            $stream.Position = 60
            $offset = $reader.ReadUInt32()
            if ($offset -lt 64 -or $offset -gt 1048576 -or $offset + 6 -gt $stream.Length) { return $false }
            $stream.Position = $offset
            return $reader.ReadUInt32() -eq 0x4550 -and $reader.ReadUInt16() -eq 0x8664
        } finally { $stream.Dispose() }
    } catch { return $false }
}

function Read-SteamText([string]$Path) {
    try {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-Item -LiteralPath $Path).Length -gt 524288) { return '' }
        return [IO.File]::ReadAllText($Path)
    } catch { return '' }
}

function Add-DiscoveryPath($Paths, [string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or $Paths.Count -ge 64) { return }
    try {
        $expanded = [Environment]::ExpandEnvironmentVariables($Path.Trim().Trim('"'))
        if (-not [IO.Path]::IsPathRooted($expanded)) { return }
        $full = [IO.Path]::GetFullPath($expanded)
        if (-not ($Paths -contains $full)) { $Paths.Add($full) }
    } catch { }
}

function Get-DiscoveryHints {
    $games = New-Object 'System.Collections.Generic.List[string]'
    $steam = New-Object 'System.Collections.Generic.List[string]'
    $registeredGames = New-Object 'System.Collections.Generic.List[string]'
    $suites = New-Object 'System.Collections.Generic.List[string]'
    Add-DiscoveryPath $suites $InstallDir
    foreach ($hive in @('HKLM:', 'HKCU:')) {
        foreach ($view in @('Software', 'Software\WOW6432Node')) {
            $suite = Get-ItemProperty -LiteralPath "$hive\$view\mab\ItStealsPracticeSuite" -ErrorAction SilentlyContinue
            if ($suite) {
                foreach ($field in @('GameDir', 'InstallDir')) {
                    $value = $suite.PSObject.Properties[$field]
                    if ($value) {
                        if ($field -eq 'GameDir') { Add-DiscoveryPath $registeredGames $value.Value }
                        else { Add-DiscoveryPath $suites $value.Value }
                    }
                }
            }
            $app = Get-ItemProperty -LiteralPath "$hive\$view\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 1349060" -ErrorAction SilentlyContinue
            if ($app) {
                $value = $app.PSObject.Properties['InstallLocation']
                if ($value) { Add-DiscoveryPath $registeredGames $value.Value }
            }
            $client = Get-ItemProperty -LiteralPath "$hive\$view\Valve\Steam" -ErrorAction SilentlyContinue
            if ($client) {
                foreach ($field in @('SteamPath', 'InstallPath', 'SteamExe')) {
                    $value = $client.PSObject.Properties[$field]
                    if ($value -and $value.Value) {
                        $path = [string]$value.Value
                        if ($field -eq 'SteamExe') { $path = [IO.Path]::GetDirectoryName($path.Trim('"')) }
                        Add-DiscoveryPath $steam $path
                    }
                }
            }
        }
    }
    foreach ($suite in $suites) {
        $config = Read-SteamText (Join-Path $suite 'installation.ini')
        $entry = [regex]::Match($config, '(?m)^gameExecutable=(.+)\r?$')
        if ($entry.Success) {
            try { Add-DiscoveryPath $games ([IO.Path]::GetDirectoryName($entry.Groups[1].Value.Trim())) } catch { }
        }
        try {
            $text = Read-SteamText (Join-Path $suite 'installed-manifest.json')
            if ($text) {
                $receipt = $text | ConvertFrom-Json
                $value = $receipt.PSObject.Properties['gameDirectory']
                if ($value) { Add-DiscoveryPath $games $value.Value }
            }
        } catch { }
    }
    foreach ($registered in $registeredGames) { Add-DiscoveryPath $games $registered }
    foreach ($name in @('It Steals', 'Steam')) {
        foreach ($process in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
            try {
                $folder = [IO.Path]::GetDirectoryName($process.Path)
                if ($name -eq 'It Steals') { Add-DiscoveryPath $games $folder }
                else { Add-DiscoveryPath $steam $folder }
            } catch { }
        }
    }
    foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles, $env:ProgramW6432, $env:LOCALAPPDATA)) {
        if ($base) { Add-DiscoveryPath $steam (Join-Path $base 'Steam') }
    }
    return [pscustomobject]@{ GameFolders = @($games.ToArray()); SteamRoots = @($steam.ToArray()) }
}

function Find-GameFolder($Hints = $null) {
    if ($null -eq $Hints) { $Hints = Get-DiscoveryHints }
    foreach ($candidate in $Hints.GameFolders) {
        if (Test-GameFolder $candidate) { return [IO.Path]::GetFullPath($candidate) }
    }
    foreach ($root in ($Hints.SteamRoots | Select-Object -First 16)) {
        $libraries = New-Object 'System.Collections.Generic.List[string]'
        Add-DiscoveryPath $libraries $root
        foreach ($relative in @('steamapps/libraryfolders.vdf', 'config/libraryfolders.vdf')) {
            $text = Read-SteamText (Join-Path $root $relative)
            foreach ($match in [regex]::Matches($text, '(?i)"(?:path|[0-9]+)"\s+"([^"]+)"')) {
                Add-DiscoveryPath $libraries ($match.Groups[1].Value.Replace('\\', '\'))
            }
        }
        foreach ($library in ($libraries | Select-Object -First 32)) {
            $common = Join-Path $library 'steamapps/common'
            $manifest = Read-SteamText (Join-Path $library 'steamapps/appmanifest_1349060.acf')
            $directory = [regex]::Match($manifest, '(?i)"installdir"\s+"([^"]+)"')
            if ($manifest -match '"appid"\s+"1349060"' -and $directory.Success) {
                $name = $directory.Groups[1].Value
                if ($name -notin '.', '..' -and $name -notmatch '[\\/:]') {
                    $candidate = Join-Path $common $name
                    if (Test-GameFolder $candidate) { return [IO.Path]::GetFullPath($candidate) }
                }
            }
            # A copied installation may still be valid after its ACF was removed.
            $candidate = Join-Path $common 'It Steals'
            if (Test-GameFolder $candidate) { return [IO.Path]::GetFullPath($candidate) }
        }
    }
    return ''
}

function Get-SuiteTarget([string]$Relative, [string]$Game, [string]$Application) {
    $null = Get-ContainedPath $Application $Relative
    if ($Relative -eq 'ItStealsHelper.dll') { return Get-ContainedPath $Game 'Mods/ItStealsHelper.dll' }
    if ($Relative -in @('ItStealsTrainer.exe', 'PracticeSuiteEncoder.exe', 'build-info.json', 'Installer/Install.ps1', 'Installer/Update.ps1', 'CHANGELOG.md') -or $Relative.StartsWith('Encoder/', [StringComparison]::Ordinal)) {
        return Get-ContainedPath $Application $Relative
    }
    throw "Unexpected package file: $Relative"
}

function Read-PackageManifest([string]$Folder) {
    $file = Join-Path $Folder 'manifest.json'
    if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -gt 1048576) { throw 'Package manifest is missing or too large.' }
    $manifest = [IO.File]::ReadAllText($file) | ConvertFrom-Json
    if ($manifest.version -notmatch '^\d+\.\d+\.\d+$' -or @($manifest.files).Count -lt 4 -or @($manifest.files).Count -gt 256) { throw 'Invalid package manifest.' }
    $seen = @{}
    foreach ($entry in $manifest.files) {
        $source = Get-ContainedPath $Folder $entry.path
        $null = Get-SuiteTarget $entry.path $Folder $Folder
        if ($seen.ContainsKey($entry.path) -or $entry.sha256 -notmatch '^[a-fA-F0-9]{64}$') { throw 'Duplicate or invalid package entry.' }
        $seen[$entry.path] = $true
        Assert-NoReparsePoint $source
        if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $entry.sha256) {
            throw "Package integrity check failed: $($entry.path). Download Setup again."
        }
    }
    foreach ($required in @('ItStealsTrainer.exe', 'PracticeSuiteEncoder.exe', 'ItStealsHelper.dll', 'Installer/Install.ps1')) {
        if (-not $seen.ContainsKey($required)) { throw "Package is missing $required" }
    }
    return $manifest
}

function Test-ExistingLoader([string]$Folder) {
    $core = @('MelonLoader/net35/MelonLoader.dll', 'MelonLoader/net472/MelonLoader.dll', 'MelonLoader/MelonLoader.dll') |
        Where-Object { Test-Path -LiteralPath (Join-Path $Folder $_) -PathType Leaf }
    $proxy = @('version.dll', 'winhttp.dll', 'dobby.dll') |
        Where-Object { Test-Path -LiteralPath (Join-Path $Folder $_) -PathType Leaf }
    if ($core -and $proxy) { return $true }
    foreach ($item in @('MelonLoader', 'version.dll', 'winhttp.dll', 'dobby.dll', 'BepInEx')) {
        if (Test-Path -LiteralPath (Join-Path $Folder $item)) {
            throw 'An incomplete or conflicting loader is present. Repair it with the official MelonLoader installer, then run Suite Setup again.'
        }
    }
    return $false
}

function Receive-LoaderArchive([string]$Destination) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $request = [Net.HttpWebRequest]::Create($script:LoaderUrl)
    $request.Timeout = 30000
    $request.ReadWriteTimeout = 30000
    $request.MaximumAutomaticRedirections = 8
    $response = $request.GetResponse()
    try {
        if ($response.ContentLength -gt 52428800) { throw 'MelonLoader download exceeds its size limit.' }
        $inputStream = $response.GetResponseStream()
        $outputStream = [IO.File]::Create($Destination)
        try {
            $buffer = New-Object byte[] 65536
            $total = 0
            while (($count = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                $total += $count
                if ($total -gt 52428800) { throw 'MelonLoader download exceeds its size limit.' }
                $outputStream.Write($buffer, 0, $count)
            }
        } finally { $outputStream.Dispose(); $inputStream.Dispose() }
    } finally { $response.Dispose() }
    if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -ne $script:LoaderHash) {
        throw 'MelonLoader download failed its SHA-256 check. No game files were changed.'
    }
}

function Expand-LoaderArchive([string]$Archive, [string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        if ($zip.Entries.Count -gt 2048) { throw 'Too many files in loader archive.' }
        [long]$total = 0
        $seen = @{}
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\', '/')
            $null = Get-ContainedPath $Destination $name.TrimEnd('/')
            if ($name -ne 'version.dll' -and -not $name.StartsWith('MelonLoader/', [StringComparison]::Ordinal)) { throw "Unexpected loader file: $name" }
            if ($seen.ContainsKey($name) -or (($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000) { throw 'Duplicate or symbolic link in loader archive.' }
            $seen[$name] = $true
            $total += $entry.Length
            if ($total -gt 268435456) { throw 'Expanded loader exceeds its size limit.' }
        }
        foreach ($entry in $zip.Entries) {
            $target = Get-ContainedPath $Destination $entry.FullName.TrimEnd('/')
            if ($entry.FullName.EndsWith('/')) { $null = [IO.Directory]::CreateDirectory($target); continue }
            $null = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target)
        }
    } finally { $zip.Dispose() }
    foreach ($required in @('version.dll', 'MelonLoader/net35/MelonLoader.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Destination $required) -PathType Leaf)) { throw "Loader is missing $required" }
    }
}

function New-OwnedDirectories([string]$Directory, $Journal) {
    if (Test-Path -LiteralPath $Directory -PathType Container) { return }
    $parent = [IO.Path]::GetDirectoryName($Directory)
    if ($parent) { New-OwnedDirectories $parent $Journal }
    $null = [IO.Directory]::CreateDirectory($Directory)
    $Journal.Add([pscustomobject]@{ kind = 'directory'; target = $Directory })
}

function Write-InstalledFile([string]$Source, [string]$Target, [string]$Recovery, $Journal) {
    Assert-NoReparsePoint $Target
    New-OwnedDirectories ([IO.Path]::GetDirectoryName($Target)) $Journal
    $backup = ''
    if (Test-Path -LiteralPath $Target -PathType Leaf) {
        $backup = Join-Path $Recovery ([guid]::NewGuid().ToString('N'))
        [IO.File]::Copy($Target, $backup)
    } elseif (Test-Path -LiteralPath $Target) { throw "A folder occupies a required file: $Target" }
    $Journal.Add([pscustomobject]@{ kind = 'file'; target = $Target; backup = $backup })
    $temporary = $Target + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::Copy($Source, $temporary)
        if ($backup) { [IO.File]::Replace($temporary, $Target, [NullString]::Value) }
        else { [IO.File]::Move($temporary, $Target) }
    } finally { if (Test-Path -LiteralPath $temporary) { [IO.File]::Delete($temporary) } }
}

function Restore-Installation($Journal) {
    $errors = @()
    for ($index = $Journal.Count - 1; $index -ge 0; $index--) {
        $entry = $Journal[$index]
        try {
            if ($entry.kind -eq 'file') {
                if ($entry.backup) { [IO.File]::Copy($entry.backup, $entry.target, $true) }
                elseif (Test-Path -LiteralPath $entry.target -PathType Leaf) { [IO.File]::Delete($entry.target) }
            } elseif (Test-Path -LiteralPath $entry.target -PathType Container) {
                [IO.Directory]::Delete($entry.target)
            }
        } catch { $errors += $_.Exception.Message }
    }
    if ($errors.Count) { throw ($errors -join '; ') }
}

function Install-Suite([string]$Game, [string]$Application, [string]$Package) {
    if (-not (Test-GameFolder $Game)) { throw 'Choose the Windows x64 Mono It Steals game folder.' }
    $Game = [IO.Path]::GetFullPath($Game)
    $Application = [IO.Path]::GetFullPath($Application)
    Assert-NoReparsePoint $Game
    Assert-NoReparsePoint $Application
    if ($Application -eq $Game -or $Application.StartsWith($Game.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Install PracticeSuite in its own folder outside the game directory.'
    }
    Write-Output '[1/4] Checking package and game installation...'
    $manifest = Read-PackageManifest $Package
    $hasLoader = Test-ExistingLoader $Game
    $work = Join-Path ([IO.Path]::GetTempPath()) ('PracticeSuite-' + [guid]::NewGuid().ToString('N'))
    $null = [IO.Directory]::CreateDirectory($work)
    $journal = New-Object 'System.Collections.Generic.List[object]'
    $keepRecovery = $false
    try {
        $loader = Join-Path $work 'loader'
        if (-not $hasLoader) {
            Write-Output '[2/4] Downloading and verifying official MelonLoader 0.7.3...'
            $archive = Join-Path $work 'loader.zip'
            Receive-LoaderArchive $archive
            Expand-LoaderArchive $archive $loader
        } else { Write-Output '[2/4] Keeping the existing MelonLoader installation.' }
        Write-Output '[3/4] Installing PracticeSuite, encoder, and game mod...'
        if (-not $hasLoader) {
            foreach ($file in Get-ChildItem -LiteralPath $loader -Recurse -File) {
                $relative = $file.FullName.Substring($loader.Length + 1)
                Write-InstalledFile $file.FullName (Get-ContainedPath $Game $relative) $work $journal
            }
        }
        $owned = @()
        foreach ($entry in $manifest.files) {
            $target = Get-SuiteTarget $entry.path $Game $Application
            Write-InstalledFile (Get-ContainedPath $Package $entry.path) $target $work $journal
            if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.sha256) { throw "Installed file failed verification: $($entry.path)" }
            $owned += [pscustomobject]@{ path = $entry.path; sha256 = $entry.sha256 }
        }
        $config = Join-Path $work 'installation.ini'
        [IO.File]::WriteAllText($config, "gameExecutable=$(Join-Path $Game 'It Steals.exe')`r`n", (New-Object Text.UTF8Encoding($false)))
        Write-InstalledFile $config (Join-Path $Application 'installation.ini') $work $journal
        $owned += [pscustomobject]@{ path = 'installation.ini'; sha256 = (Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash }
        $receipt = Join-Path $work 'installed-manifest.json'
        @{ version = $manifest.version; gameDirectory = $Game; files = $owned } | ConvertTo-Json -Depth 5 |
            Set-Content -LiteralPath $receipt -Encoding UTF8
        Write-InstalledFile $receipt (Join-Path $Application 'installed-manifest.json') $work $journal
        Write-Output '[4/4] Installation verified. PracticeSuite is ready.'
    } catch {
        $original = $_
        try { Restore-Installation $journal }
        catch { $keepRecovery = $true; throw "Installation failed; automatic recovery needs attention. Recovery files: $work. $($_.Exception.Message)" }
        throw $original
    } finally { if (-not $keepRecovery) { Remove-Item -LiteralPath $work -Recurse -Force } }
}

function Uninstall-Suite([string]$Application) {
    $Application = [IO.Path]::GetFullPath($Application)
    Assert-NoReparsePoint $Application
    $receiptPath = Join-Path $Application 'installed-manifest.json'
    if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf) -or (Get-Item -LiteralPath $receiptPath).Length -gt 1048576) { throw 'Installation receipt is missing. Remove the suite files manually.' }
    $receipt = [IO.File]::ReadAllText($receiptPath) | ConvertFrom-Json
    if ($receipt.version -notmatch '^\d+\.\d+\.\d+$' -or @($receipt.files).Count -gt 256) { throw 'Invalid installation receipt.' }
    $targets = @()
    foreach ($entry in $receipt.files) {
        $target = if ($entry.path -eq 'installation.ini') { Join-Path $Application 'installation.ini' }
                  else { Get-SuiteTarget $entry.path $receipt.gameDirectory $Application }
        Assert-NoReparsePoint $target
        if ($entry.sha256 -notmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid receipt checksum.' }
        $targets += [pscustomobject]@{ path = $target; hash = $entry.sha256 }
    }
    foreach ($entry in $targets) {
        if (Test-Path -LiteralPath $entry.path -PathType Leaf) {
            if ((Get-FileHash -LiteralPath $entry.path -Algorithm SHA256).Hash -eq $entry.hash) { [IO.File]::Delete($entry.path) }
            else { Write-Output "Keeping modified file: $($entry.path)" }
        }
    }
    [IO.File]::Delete($receiptPath)
    Write-Output 'Suite files removed. Replays, settings, other mods, and MelonLoader were kept.'
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'This installer requires Windows 10 or later.' }
        if ($Mode -eq 'Discover') {
            $found = Find-GameFolder
            if ($DiscoveryFile) { "[Game]`r`nDirectory=$found" | Set-Content -LiteralPath $DiscoveryFile -Encoding Unicode }
            else { Write-Output $found }
        } else {
            if (@(Get-Process -Name 'It Steals', 'ItStealsTrainer', 'PracticeSuiteEncoder' -ErrorAction SilentlyContinue).Count) { throw 'Close It Steals, PracticeSuite, and any active exports before continuing.' }
            if (-not $InstallDir) { throw 'An application folder is required.' }
            if ($Mode -eq 'Uninstall') { Uninstall-Suite $InstallDir }
            else {
                if (-not $GameDir) { throw 'A game folder is required.' }
                Install-Suite $GameDir $InstallDir $PayloadDir
            }
        }
        exit 0
    } catch { Write-Output ("ERROR: " + $_.Exception.Message); exit 1 }
}
