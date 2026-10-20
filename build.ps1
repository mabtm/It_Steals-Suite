param(
    [Parameter(Mandatory = $true)][string]$GameDir,
    [string]$MelonAssemblyPath,
    [string]$HarmonyAssemblyPath,
    [string]$PlatformToolset
)

$ErrorActionPreference = 'Stop'
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$managed = Join-Path $GameDir 'It Steals_Data\Managed'
if (!(Test-Path -LiteralPath (Join-Path $managed 'Assembly-CSharp.dll'))) {
    throw 'Game assemblies are missing. Set GameDir to the installed It Steals folder.'
}

function Find-LoaderAssembly([string]$Filename) {
    $loader = Join-Path $GameDir 'MelonLoader'
    if (!(Test-Path -LiteralPath $loader)) {
        throw 'MelonLoader is missing. Install it and start the game once before building.'
    }
    $candidates = @(Get-ChildItem -LiteralPath $loader -Recurse -File -Filter $Filename)
    $mono = @($candidates | Where-Object { $_.FullName -match '[\\/]net35[\\/]|[\\/]net472[\\/]' })
    if ($mono.Count -eq 1) { return $mono[0].FullName }
    if ($candidates.Count -eq 1) { return $candidates[0].FullName }
    throw "Cannot select $Filename unambiguously. Supply its assembly-path parameter."
}

if (!$MelonAssemblyPath) { $MelonAssemblyPath = Find-LoaderAssembly 'MelonLoader.dll' }
if (!$HarmonyAssemblyPath) { $HarmonyAssemblyPath = Find-LoaderAssembly '0Harmony.dll' }
$MelonAssemblyPath = (Resolve-Path -LiteralPath $MelonAssemblyPath).Path
$HarmonyAssemblyPath = (Resolve-Path -LiteralPath $HarmonyAssemblyPath).Path

$msbuild = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
if ($msbuild) {
    $builder = $msbuild.Source
} else {
    $programFiles = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhere = Join-Path $programFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) {
        throw 'Visual Studio C++ Build Tools are required to build. Prebuilt releases are in bin/.'
    }
    $builder = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (!$builder) { throw 'MSBuild with the C++ tools was not found.' }
}

$modArgs = @(
    (Join-Path $PSScriptRoot 'Mod\ItStealsHelper.csproj'),
    '/t:Rebuild', '/p:Configuration=Release',
    "/p:GameManagedDir=$managed",
    "/p:MelonAssemblyPath=$MelonAssemblyPath",
    "/p:HarmonyAssemblyPath=$HarmonyAssemblyPath"
)
& $builder @modArgs
if ($LASTEXITCODE -ne 0) { throw 'The mod build failed.' }

$trainerArgs = @(
    (Join-Path $PSScriptRoot 'Trainer\ItStealsTrainer.vcxproj'),
    '/t:Rebuild', '/p:Configuration=Release', '/p:Platform=x64'
)
if ($PlatformToolset) { $trainerArgs += "/p:PlatformToolset=$PlatformToolset" }
& $builder @trainerArgs
if ($LASTEXITCODE -ne 0) { throw 'The trainer build failed.' }
Write-Host 'Built bin\ItStealsHelper.dll and bin\ItStealsTrainer.exe.'
