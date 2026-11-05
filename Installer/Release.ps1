[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [string]$TrainerBinary,
    [string]$PlatformToolset = 'v143',
    [string]$SourceCommit = $env:GITHUB_SHA,
    [string]$OutputDir
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $OutputDir) { $OutputDir = Join-Path $root 'build/release' }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$header = [IO.File]::ReadAllText((Join-Path $root 'Trainer/SuiteBranding.h'))
$version = [regex]::Match($header, '#define SUITE_VERSION "(\d+\.\d+\.\d+)"').Groups[1].Value
if (-not $version) { throw 'Suite version is missing.' }
$modVersion = [regex]::Match($header, '#define SUITE_MOD_VERSION "(\d+\.\d+\.\d+)"').Groups[1].Value
if (-not $modVersion) { throw 'Expected game mod version is missing.' }
$modBinary = Join-Path $root 'bin/ItStealsHelper.dll'
$modBinaryVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($modBinary).FileVersion
if ($modBinaryVersion -ne "$modVersion.0") { throw "Game mod version $modBinaryVersion does not match expected $modVersion.0." }
if (-not $SourceCommit) {
    $SourceCommit = & git -C $root rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Source commit is required.' }
}
if (-not $SkipBuild) {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'Native release compilation requires Windows.' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild) { throw 'Install Visual Studio C++ Build Tools with MSBuild and a Windows SDK.' }
    $nativeLog = Join-Path $root 'build/native-build.log'
    $null = [IO.Directory]::CreateDirectory((Split-Path -Parent $nativeLog))
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $msbuild (Join-Path $root 'Trainer/ItStealsTrainer.vcxproj') /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$PlatformToolset" /nologo /verbosity:minimal 2>&1 | Tee-Object -FilePath $nativeLog
        $nativeExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($nativeExit -ne 0) {
        if ($env:GITHUB_ACTIONS -eq 'true') {
            foreach ($line in Get-Content -LiteralPath $nativeLog | Where-Object { $_ -match '\b(error|fatal)\b' } | Select-Object -First 12) {
                $message = $line.Replace('%','%25').Replace("`r",'%0D').Replace("`n",'%0A')
                Write-Output "::error::$message"
            }
        }
        throw "Native trainer build failed. Compiler output: $nativeLog"
    }
}
if (-not $TrainerBinary) { $TrainerBinary = Join-Path $root 'bin/ItStealsTrainer.exe' }
$TrainerBinary = [IO.Path]::GetFullPath($TrainerBinary)
$binaryVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($TrainerBinary).ProductVersion
if (-not $binaryVersion -and [Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    # Linux FileVersionInfo does not decode native Win32 version resources.
    $resource = [Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($TrainerBinary))
    $binaryVersion = [regex]::Match($resource, 'ProductVersion\x00{1,4}(\d+\.\d+\.\d+)').Groups[1].Value
}
if ($binaryVersion -ne $version) { throw "Native binary version $binaryVersion does not match source $version." }
$encoder = Join-Path $root 'bin/PracticeSuiteEncoder.exe'
$encoderHeader = [IO.File]::ReadAllText((Join-Path $root 'Trainer/EncoderIdentity.h'))
$encoderHash = (Get-FileHash -LiteralPath $encoder -Algorithm SHA256).Hash.ToLowerInvariant()
if (-not $encoderHeader.Contains($encoderHash)) { throw 'Encoder identity does not match the trainer.' }
$null = [IO.Directory]::CreateDirectory($OutputDir)
$work = Join-Path $root ('build/release-stage-' + [guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($work)
try {
    $payload = Join-Path $work 'payload'
    $null = [IO.Directory]::CreateDirectory((Join-Path $payload 'Installer'))
    Copy-Item -LiteralPath $TrainerBinary -Destination (Join-Path $payload 'ItStealsTrainer.exe')
    Copy-Item -LiteralPath $encoder -Destination (Join-Path $payload 'PracticeSuiteEncoder.exe')
    Copy-Item -LiteralPath (Join-Path $root 'bin/ItStealsHelper.dll') -Destination (Join-Path $payload 'ItStealsHelper.dll')
    Copy-Item -LiteralPath (Join-Path $root 'Encoder') -Destination $payload -Recurse
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install.ps1') -Destination (Join-Path $payload 'Installer/Install.ps1')
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Update.ps1') -Destination (Join-Path $payload 'Installer/Update.ps1')
    Copy-Item -LiteralPath (Join-Path $root 'CHANGELOG.md') -Destination (Join-Path $payload 'CHANGELOG.md')
    $buildInfo = [ordered]@{
        version = $version
        channel = $(if ($header -match '#define SUITE_PREVIEW 1') { 'preview' } else { 'stable' })
        author = 'mab'
        sourceCommit = $SourceCommit
        modVersion = $modVersion
        trainerSha256 = (Get-FileHash -LiteralPath $TrainerBinary -Algorithm SHA256).Hash.ToLowerInvariant()
        modSha256 = (Get-FileHash -LiteralPath (Join-Path $payload 'ItStealsHelper.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
        encoderSha256 = $encoderHash
        melonLoaderVersion = '0.7.3'
        melonLoaderSha256 = '5b2b2f3d1cd42b59ec886c5bdc2663edae87a0097a4f4a8f58c0965a99dda416'
    }
    $buildInfo | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payload 'build-info.json') -Encoding UTF8
    $entries = @()
    foreach ($file in Get-ChildItem -LiteralPath $payload -File -Recurse | Sort-Object FullName) {
        $entries += [pscustomobject]@{
            path = $file.FullName.Substring($payload.Length + 1).Replace('\', '/')
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    @{ version = $version; files = $entries } | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $payload 'manifest.json') -Encoding UTF8
    . (Join-Path $PSScriptRoot 'Install.ps1')
    $null = Read-PackageManifest $payload
    $portable = Join-Path $work 'portable'
    $null = [IO.Directory]::CreateDirectory($portable)
    Copy-Item -Path (Join-Path $payload '*') -Destination $portable -Recurse
    $quickstart = @'
ELITE-SUITE / IT STEALS
Portable installation:
1. Install MelonLoader for It Steals using its official installer.
2. Copy ItStealsHelper.dll into the game's Mods folder.
3. Keep ItStealsTrainer.exe, PracticeSuiteEncoder.exe, and Encoder together.
4. Open Elite-Suite; it starts the game. Press F3 to open the menu.
Optional automatic game launch: create installation.ini beside the trainer:
gameExecutable=C:\Your game folder\It Steals.exe
Use --no-launch to skip automatic launch once.
Practice. Understand. Improve.
Documentation: https://github.com/mabtm/It_Steals-Suite
'@
    [IO.File]::WriteAllText((Join-Path $portable 'README.txt'), $quickstart, (New-Object Text.UTF8Encoding($false)))
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    foreach ($archive in @('ItStealsSuite-Portable.zip', 'Encoder-Sources.zip')) {
        $target = Join-Path $OutputDir $archive
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target }
    }
    [IO.Compression.ZipFile]::CreateFromDirectory($portable, (Join-Path $OutputDir 'ItStealsSuite-Portable.zip'), [IO.Compression.CompressionLevel]::Optimal, $false)
    [IO.Compression.ZipFile]::CreateFromDirectory((Join-Path $root 'Encoder'), (Join-Path $OutputDir 'Encoder-Sources.zip'), [IO.Compression.CompressionLevel]::Optimal, $true)
    $nsis = Get-Command makensis -ErrorAction SilentlyContinue
    if ($nsis) { $compiler = $nsis.Source }
    else {
        $compiler = Join-Path ${env:ProgramFiles(x86)} 'NSIS/makensis.exe'
        if (-not (Test-Path -LiteralPath $compiler)) { throw 'Install NSIS 3 to build Setup.' }
    }
    $setup = Join-Path $OutputDir 'ItStealsSuite-Setup.exe'
    Push-Location $PSScriptRoot
    try {
        $prefix = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { '/' } else { '-' }
        $channel = if ($header -match '#define SUITE_PREVIEW 1') { ' Preview' } else { '' }
        & $compiler ($prefix + 'V2') ($prefix + "DVERSION=$version") ($prefix + "DCHANNEL=$channel") ($prefix + "DPAYLOAD=$payload") ($prefix + "DOUTPUT=$setup") 'Setup.nsi'
        if ($LASTEXITCODE -ne 0) { throw 'NSIS Setup build failed.' }
    } finally { Pop-Location }
    foreach ($name in @('ItStealsTrainer.exe', 'PracticeSuiteEncoder.exe', 'ItStealsHelper.dll', 'build-info.json')) {
        Copy-Item -LiteralPath (Join-Path $payload $name) -Destination (Join-Path $OutputDir $name) -Force
    }
    $names = @('ItStealsSuite-Setup.exe', 'ItStealsSuite-Portable.zip', 'ItStealsTrainer.exe', 'PracticeSuiteEncoder.exe', 'ItStealsHelper.dll', 'Encoder-Sources.zip')
    $checksums = foreach ($name in $names) {
        $hash = (Get-FileHash -LiteralPath (Join-Path $OutputDir $name) -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $name"
    }
    [IO.File]::WriteAllText((Join-Path $OutputDir 'SHA256SUMS.txt'), (($checksums -join [Environment]::NewLine) + [Environment]::NewLine), (New-Object Text.UTF8Encoding($false)))
    Write-Output "Release $version is ready in $OutputDir"
} finally { Remove-Item -LiteralPath $work -Force -Recurse }
