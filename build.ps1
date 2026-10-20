param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [string]$MelonAssemblyPath,
    [string]$HarmonyAssemblyPath,
    [string]$PlatformToolset
)
$ErrorActionPreference = 'Stop'
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$managed = Join-Path $GameDir 'It Steals_Data\Managed'
if (!(Test-Path -LiteralPath (Join-Path $managed 'Assembly-CSharp.dll'))) { throw 'Managed-Ordner des Spiels fehlt.' }

function Find-LoaderAssembly([string]$filename) {
    $loader = Join-Path $GameDir 'MelonLoader'
    $candidates = @(Get-ChildItem -LiteralPath $loader -Recurse -File -Filter $filename)
    $mono = @($candidates | Where-Object { $_.FullName -match '[\\/]net35[\\/]|[\\/]net472[\\/]' })
    if ($mono.Count -eq 1) { return $mono[0].FullName }
    if ($candidates.Count -eq 1) { return $candidates[0].FullName }
    throw "$filename nicht eindeutig gefunden. Pfad als Parameter angeben."
}
if (!$MelonAssemblyPath) { $MelonAssemblyPath = Find-LoaderAssembly 'MelonLoader.dll' }
if (!$HarmonyAssemblyPath) { $HarmonyAssemblyPath = Find-LoaderAssembly '0Harmony.dll' }
if (!(Test-Path -LiteralPath $MelonAssemblyPath) -or !(Test-Path -LiteralPath $HarmonyAssemblyPath)) { throw 'Loader-/Harmony-DLL fehlt.' }

$msbuild = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
if ($msbuild) { $builder = $msbuild.Source }
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Build Tools fehlen. Die fertigen Dateien in bin sind sofort nutzbar.' }
    $builder = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (!$builder) { throw 'MSBuild nicht gefunden.' }
}
$modArgs = @((Join-Path $PSScriptRoot 'Mod\ItStealsHelper.csproj'), '/t:Rebuild', '/p:Configuration=Release',
    "/p:GameManagedDir=$managed", "/p:MelonAssemblyPath=$MelonAssemblyPath", "/p:HarmonyAssemblyPath=$HarmonyAssemblyPath")
& $builder @modArgs
if ($LASTEXITCODE -ne 0) { throw 'Mod-Build fehlgeschlagen.' }
$trainerArgs = @((Join-Path $PSScriptRoot 'Trainer\ItStealsTrainer.vcxproj'), '/t:Rebuild', '/p:Configuration=Release', '/p:Platform=x64')
if ($PlatformToolset) { $trainerArgs += "/p:PlatformToolset=$PlatformToolset" }
& $builder @trainerArgs
if ($LASTEXITCODE -ne 0) { throw 'Trainer-Build fehlgeschlagen.' }
Write-Host 'Fertig: bin\ItStealsHelper.dll und bin\ItStealsTrainer.exe'
