[CmdletBinding()]
param([string]$LoaderArchive)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $repo 'Installer/Install.ps1')
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('SuiteInstallerTests-' + [guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($testRoot)
$passed = New-Object 'System.Collections.Generic.List[string]'
function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Reject([scriptblock]$Action, [string]$Message) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert $rejected $Message
}
function Write-TestFile([string]$Path, [string]$Text) {
    $null = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path))
    [IO.File]::WriteAllText($Path, $Text)
}
function Make-Game([string]$Folder, [bool]$Loader = $true) {
    $null = [IO.Directory]::CreateDirectory((Join-Path $Folder 'It Steals_Data/Managed'))
    $bytes = New-Object byte[] 128
    $bytes[0]=0x4d; $bytes[1]=0x5a; $bytes[60]=64
    $bytes[64]=0x50; $bytes[65]=0x45; $bytes[68]=0x64; $bytes[69]=0x86
    [IO.File]::WriteAllBytes((Join-Path $Folder 'It Steals.exe'), $bytes)
    Write-TestFile (Join-Path $Folder 'It Steals_Data/Managed/Assembly-CSharp.dll') 'game marker'
    if ($Loader) {
        Write-TestFile (Join-Path $Folder 'MelonLoader/net35/MelonLoader.dll') 'existing loader'
        Write-TestFile (Join-Path $Folder 'version.dll') 'existing proxy'
    }
}
function Make-Package([string]$Folder, [string]$Tag) {
    foreach ($name in @('ItStealsTrainer.exe', 'PracticeSuiteEncoder.exe', 'ItStealsHelper.dll', 'Installer/Install.ps1', 'Encoder/NOTICE.md')) {
        Write-TestFile (Join-Path $Folder $name) ($Tag + $name)
    }
    $entries = foreach ($file in Get-ChildItem -LiteralPath $Folder -Recurse -File) {
        if ($file.Name -eq 'manifest.json') { continue }
        @{ path=$file.FullName.Substring($Folder.Length+1).Replace('\','/'); sha256=(Get-FileHash -LiteralPath $file.FullName).Hash }
    }
    @{ version='7.12.0'; files=@($entries) } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Folder 'manifest.json') -Encoding UTF8
}
try {
    $startupGame=Join-Path $testRoot 'Startup recovery'
    $startupFile=Join-Path $startupGame 'It Steals_Data/globalgamemanagers'
    Write-TestFile $startupFile 'external settings'
    Restore-StartupSettings $startupGame
    Write-TestFile ($startupFile+'.practicesuite-original') 'unverified backup'
    Reject { Restore-StartupSettings $startupGame } 'An unverified startup backup was restored.'
    Assert ([IO.File]::ReadAllText($startupFile) -eq 'external settings') 'Startup recovery changed external settings.'
    Assert ([IO.File]::ReadAllText($startupFile+'.practicesuite-original') -eq 'unverified backup') 'Startup recovery discarded an unverified backup.'
    $passed.Add('startup recovery preserves unknown files and backups')
    $game = Join-Path $testRoot 'Game & spaces'
    $application = Join-Path $testRoot 'Suite'
    $package = Join-Path $testRoot 'package'
    Make-Game $game
    Make-Package $package 'first'
    # App-size entries must not consume the library limit before a second library.
    $script:discoverySteamRoot = Join-Path $testRoot 'Steam'
    $libraryRoot = Join-Path $testRoot 'Second library'
    $secondaryGame = [IO.Path]::GetFullPath((Join-Path $libraryRoot 'steamapps/common/It Steals'))
    Make-Game $secondaryGame
    $apps = (1..100 | ForEach-Object { '"' + $_ + '" "123456789" ' }) -join ''
    $escapedSteam = $script:discoverySteamRoot.Replace('\', '\\')
    $escapedLibrary = $libraryRoot.Replace('\', '\\')
    Write-TestFile (Join-Path $script:discoverySteamRoot 'steamapps/libraryfolders.vdf') ('"libraryfolders" { "0" { "path" "' + $escapedSteam + '" "apps" { ' + $apps + ' } } "1" { "path" "' + $escapedLibrary + '" } }')
    Write-TestFile (Join-Path $libraryRoot 'steamapps/appmanifest_1349060.acf') '"AppState" { "appid" "1349060" "installdir" "It Steals" }'
    function Get-ItemProperty {
        [CmdletBinding()]param([string]$LiteralPath)
        [pscustomobject]@{ SteamPath = $script:discoverySteamRoot }
    }
    try {
        Assert ((Find-GameFolder) -eq $secondaryGame) 'Large modern Steam catalog hid the secondary library.'
        Write-TestFile (Join-Path $script:discoverySteamRoot 'steamapps/libraryfolders.vdf') ('"LibraryFolders" { "1" "' + $escapedLibrary + '" }')
        Assert ((Find-GameFolder) -eq $secondaryGame) 'Legacy Steam library discovery failed.'
    } finally { Remove-Item Function:Get-ItemProperty }
    $hints = [pscustomobject]@{ GameFolders = @(); SteamRoots = @($script:discoverySteamRoot) }
    Remove-Item -LiteralPath (Join-Path $libraryRoot 'steamapps/appmanifest_1349060.acf')
    Assert ((Find-GameFolder $hints) -eq $secondaryGame) 'A valid game without an ACF was not discovered.'
    Remove-Item -LiteralPath (Join-Path $script:discoverySteamRoot 'steamapps/libraryfolders.vdf')
    Write-TestFile (Join-Path $script:discoverySteamRoot 'config/libraryfolders.vdf') ('"libraryfolders" { "1" { "PATH" "' + $escapedLibrary + '" } }')
    Assert ((Find-GameFolder $hints) -eq $secondaryGame) 'Alternate Steam library metadata was not read.'
    $brokenRoot = Join-Path $testRoot 'Broken metadata'
    $null = [IO.Directory]::CreateDirectory((Join-Path $brokenRoot 'steamapps/libraryfolders.vdf'))
    $hints.SteamRoots = @($brokenRoot, $script:discoverySteamRoot)
    Assert ((Find-GameFolder $hints) -eq $secondaryGame) 'An unreadable first location prevented later discovery.'
    $passed.Add('Steam libraries and valid installations survive missing or unreadable metadata')

    $unicodeGame = Join-Path $testRoot ('Game & caf' + [char]0xE9 + ' [fixture]')
    Make-Game $unicodeGame
    $script:hintCase = 'uninstall'
    $script:hintGame = $unicodeGame
    function Get-ItemProperty {
        [CmdletBinding()]param([string]$LiteralPath)
        if ($script:hintCase -eq 'uninstall' -and $LiteralPath -like '*Uninstall\Steam App 1349060') {
            return [pscustomobject]@{ InstallLocation = $script:hintGame }
        }
        if ($script:hintCase -eq 'steam-exe' -and $LiteralPath -like '*Valve\Steam') {
            return [pscustomobject]@{ SteamExe = (Join-Path $script:discoverySteamRoot 'Steam.exe') }
        }
    }
    function Get-Process {
        [CmdletBinding()]param([string]$Name)
        if ($script:hintCase -eq 'running-game' -and $Name -eq 'It Steals') {
            return [pscustomobject]@{ Path = (Join-Path $script:hintGame 'It Steals.exe') }
        }
        if ($script:hintCase -eq 'running-steam' -and $Name -eq 'Steam') {
            return [pscustomobject]@{ Path = (Join-Path $script:discoverySteamRoot 'Steam.exe') }
        }
    }
    try {
        Assert ((Find-GameFolder) -eq $unicodeGame) 'Windows game installation entry was not used.'
        $script:hintCase = 'steam-exe'
        Assert ((Find-GameFolder) -eq $secondaryGame) 'SteamExe-only registry discovery failed.'
        $script:hintCase = 'running-game'
        Assert ((Find-GameFolder) -eq $unicodeGame) 'A running game was not used as a path hint.'
        $script:hintCase = 'running-steam'
        Assert ((Find-GameFolder) -eq $secondaryGame) 'Running Steam did not reveal its libraries.'
    } finally { Remove-Item Function:Get-ItemProperty; Remove-Item Function:Get-Process }
    $passed.Add('Windows installation entries, SteamExe, and running processes provide verified paths')

    $cachedSuite = Join-Path $testRoot 'Saved suite'
    Write-TestFile (Join-Path $cachedSuite 'installation.ini') ("gameExecutable=" + (Join-Path $unicodeGame 'It Steals.exe') + "`r`n")
    $savedInstallDir = $InstallDir
    try {
        $InstallDir = $cachedSuite
        Assert ((Find-GameFolder) -eq $unicodeGame) 'Saved trainer game path was not reused.'
        Remove-Item -LiteralPath (Join-Path $cachedSuite 'installation.ini')
        Write-TestFile (Join-Path $cachedSuite 'installed-manifest.json') (@{ gameDirectory = $unicodeGame } | ConvertTo-Json)
        Assert ((Find-GameFolder) -eq $unicodeGame) 'Existing installation receipt was not reused.'
    } finally { $InstallDir = $savedInstallDir }
    $passed.Add('saved game paths and installation receipts are reused with Unicode paths')

    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        $ini = Join-Path $testRoot 'detected.ini'
        $hostExecutable = [Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
        & $hostExecutable -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File (Join-Path $repo 'Installer/Install.ps1') -Mode Discover -InstallDir $cachedSuite -DiscoveryFile $ini | Out-Null
        Assert ($LASTEXITCODE -eq 0) 'The actual discovery command failed.'
        Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
using System.Text;
public static class SuiteDiscoveryIniTest {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetPrivateProfileStringW")]
    public static extern uint Read(string section, string key, string fallback, StringBuilder output, uint size, string file);
}
'@
        $value = New-Object Text.StringBuilder 32768
        $null = [SuiteDiscoveryIniTest]::Read('Game', 'Directory', '', $value, 32768, $ini)
        Assert ($value.ToString() -eq $unicodeGame) 'The Windows INI API used by Setup did not receive the detected Unicode path.'
        $passed.Add('real discovery process writes a Unicode path readable by the Windows Setup INI API')
    }

    Write-TestFile (Join-Path $game 'Mods/Other.dll') 'other mod'
    $existingLoaderHash = (Get-FileHash -LiteralPath (Join-Path $game 'version.dll')).Hash
    Install-Suite $game $application $package | Out-Null
    Assert (Test-Path -LiteralPath (Join-Path $application 'ItStealsTrainer.exe')) 'Trainer was not installed.'
    Assert ([IO.File]::ReadAllText((Join-Path $application 'installation.ini')).Contains((Join-Path $game 'It Steals.exe'))) 'Startup path was not saved.'
    Make-Package $package 'second'
    Install-Suite $game $application $package | Out-Null
    Assert ((Get-FileHash -LiteralPath (Join-Path $game 'version.dll')).Hash -eq $existingLoaderHash) 'Existing loader changed.'
    Assert ([IO.File]::ReadAllText((Join-Path $game 'Mods/Other.dll')) -eq 'other mod') 'Other mod changed.'
    $passed.Add('install and update preserve game, loader, and other mods')

    Write-TestFile (Join-Path $package 'PracticeSuiteEncoder.exe') 'corrupt'
    Reject { Install-Suite $game $application $package } 'Corrupt payload was accepted.'
    Assert ([IO.File]::ReadAllText((Join-Path $application 'PracticeSuiteEncoder.exe')).StartsWith('second')) 'Corruption modified installation.'
    $passed.Add('corruption rejected before mutation')
    Make-Package $package 'third'

    Reject { Install-Suite (Join-Path $testRoot 'missing') $application $package } 'Missing game accepted.'
    $invalid = Join-Path $testRoot 'x86'
    Make-Game $invalid
    $image = [IO.File]::ReadAllBytes((Join-Path $invalid 'It Steals.exe')); $image[68]=0x4c; $image[69]=1
    [IO.File]::WriteAllBytes((Join-Path $invalid 'It Steals.exe'),$image)
    Assert (-not (Test-GameFolder $invalid)) 'x86 executable accepted.'
    $conflict = Join-Path $testRoot 'conflict'
    Make-Game $conflict $false
    Write-TestFile (Join-Path $conflict 'version.dll') 'unknown proxy'
    Reject { Install-Suite $conflict $application $package } 'Conflicting proxy accepted.'
    $passed.Add('missing, x86, and conflicting installations rejected')

    foreach ($unsafe in @('../outside','a/../../outside','/absolute','C:\absolute','a:stream','\\server\share','a\..\outside')) {
        Reject { Get-ContainedPath $application $unsafe } "Unsafe path accepted: $unsafe"
    }
    $passed.Add('path traversal, absolute paths, and alternate streams rejected')

    $originalWriter = (Get-Item Function:Write-InstalledFile).ScriptBlock
    Set-Item Function:Write-TestOriginal $originalWriter
    $script:writeCount = 0
    function Write-InstalledFile([string]$Source,[string]$Target,[string]$Recovery,$Journal) {
        $script:writeCount++
        if ($script:writeCount -eq 3) { throw 'Injected copy interruption' }
        Write-TestOriginal $Source $Target $Recovery $Journal
    }
    try { Reject { Install-Suite $game $application $package } 'Injected failure was ignored.' }
    finally { Set-Item Function:Write-InstalledFile $originalWriter }
    Assert ([IO.File]::ReadAllText((Join-Path $application 'ItStealsTrainer.exe')).StartsWith('second')) 'Trainer rollback failed.'
    Assert ([IO.File]::ReadAllText((Join-Path $game 'Mods/ItStealsHelper.dll')).StartsWith('second')) 'Mod rollback failed.'
    $passed.Add('interrupted updates restore previous files')

    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $badZip = Join-Path $testRoot 'bad.zip'
    $zip=[IO.Compression.ZipFile]::Open($badZip,[IO.Compression.ZipArchiveMode]::Create)
    $null=$zip.CreateEntry('../escape')
    $zip.Dispose()
    Reject { Expand-LoaderArchive $badZip (Join-Path $testRoot 'bad-loader') } 'Unsafe loader ZIP accepted.'
    $passed.Add('loader archive layout and traversal verified')

    $loaderFixture = Join-Path $testRoot 'loader-fixture'
    Write-TestFile (Join-Path $loaderFixture 'version.dll') 'pinned proxy fixture'
    Write-TestFile (Join-Path $loaderFixture 'MelonLoader/net35/MelonLoader.dll') 'pinned loader fixture'
    $script:fixtureArchive = Join-Path $testRoot 'fixture.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($loaderFixture,$script:fixtureArchive)
    $downloadOriginal=(Get-Item Function:Receive-LoaderArchive).ScriptBlock
    function Receive-LoaderArchive([string]$Destination) { [IO.File]::Copy($script:fixtureArchive,$Destination) }
    try {
        $fresh = Join-Path $testRoot 'fresh game'
        Make-Game $fresh $false
        Install-Suite $fresh (Join-Path $testRoot 'fresh suite') $package | Out-Null
        Assert (Test-ExistingLoader $fresh) 'First-time loader installation failed.'
    } finally { Set-Item Function:Receive-LoaderArchive $downloadOriginal }
    $passed.Add('first-time loader and suite transaction')

    if (-not $LoaderArchive) {
        $LoaderArchive = Join-Path $testRoot 'official.zip'
        Receive-LoaderArchive $LoaderArchive
    }
    Assert ((Get-FileHash -LiteralPath $LoaderArchive -Algorithm SHA256).Hash -eq $script:LoaderHash) 'Official archive hash differs.'
    $official = Join-Path $testRoot 'official-loader'
    Expand-LoaderArchive $LoaderArchive $official
    Assert (Test-ExistingLoader $official) 'Official loader layout is incompatible.'
    $passed.Add('actual official pinned archive verifies and extracts')

    Write-TestFile (Join-Path $game 'Mods/ItStealsHelper.dll') 'user modified'
    Write-TestFile (Join-Path $application 'keep.txt') 'user data'
    Uninstall-Suite $application | Out-Null
    Assert (-not (Test-Path -LiteralPath (Join-Path $application 'ItStealsTrainer.exe'))) 'Owned trainer was not removed.'
    Assert (Test-Path -LiteralPath (Join-Path $game 'Mods/ItStealsHelper.dll')) 'Modified mod was deleted.'
    Assert (Test-Path -LiteralPath (Join-Path $application 'keep.txt')) 'User data was deleted.'
    Assert (Test-ExistingLoader $game) 'Shared loader was deleted.'
    Assert (Test-Path -LiteralPath (Join-Path $game 'Mods/Other.dll')) 'Other mod was deleted.'
    $passed.Add('uninstall removes only unchanged owned files')
    $output = Join-Path $repo 'build/installer-tests'
    $null=[IO.Directory]::CreateDirectory($output)
    @{ passed=$true; checks=@($passed.ToArray()) } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'summary.json') -Encoding UTF8
    Write-Output "$($passed.Count) installer contract groups passed."
} finally { Remove-Item -LiteralPath $testRoot -Recurse -Force }
