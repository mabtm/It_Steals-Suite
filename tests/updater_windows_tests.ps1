[CmdletBinding()]
param([switch]$PublicRelease)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $PublicRelease -and -not $env:SUITE_UPDATE_TEST_TOKEN){throw 'The private-repository transport check requires a CI token with release read access. Use -PublicRelease for anonymous public access.'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $visualStudio){throw 'Visual C++ build tools are missing.'}
$tools=Join-Path $visualStudio 'VC/Auxiliary/Build/vcvars64.bat'
$out=Join-Path $root 'build/windows-updater-tests'
$null=[IO.Directory]::CreateDirectory($out)
$command=Join-Path $out 'compile.cmd'
$lines=@(
    '@echo off',
    ('call "'+$tools+'" >nul'),
    ('cd /d "'+(Join-Path $root 'Trainer')+'"'),
    ('rc /nologo /fo "'+(Join-Path $out 'suite.res')+'" TrainerResources.rc'),
    'if errorlevel 1 exit /b 1',
    ('cl /nologo /std:c++20 /EHsc /MT /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"'+(Join-Path $root 'Trainer')+'" /Fo"'+(Join-Path $out 'contract.obj')+'" /Fe"'+(Join-Path $out 'contract.exe')+'" "'+(Join-Path $PSScriptRoot 'update_windows_contract.cpp')+'" "'+(Join-Path $out 'suite.res')+'" /link winhttp.lib advapi32.lib bcrypt.lib shell32.lib ole32.lib'),
    'if errorlevel 1 exit /b 1',
    ('cl /nologo /std:c++20 /EHsc /MT /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"'+(Join-Path $root 'Trainer')+'" /I"'+(Join-Path $root 'Trainer/imgui')+'" /I"'+(Join-Path $root 'Trainer/imgui/backends')+'" /Fo"'+(Join-Path $out 'capture.obj')+'" /Fe"'+(Join-Path $out 'capture.exe')+'" "'+(Join-Path $PSScriptRoot 'capture_windows_contract.cpp')+'" /link d3d11.lib dxgi.lib d3dcompiler.lib gdi32.lib user32.lib'),
    'exit /b %errorlevel%'
)
[IO.File]::WriteAllLines($command,$lines,[Text.Encoding]::ASCII)
& $env:ComSpec /d /c $command
if($LASTEXITCODE -ne 0){throw 'Native updater contract compilation failed.'}
if ($PublicRelease) {
    & (Join-Path $out 'contract.exe') --anonymous
    if($LASTEXITCODE -ne 0){throw 'Anonymous public release transport failed.'}
    & (Join-Path $out 'contract.exe') --stale-credential
}
else { & (Join-Path $out 'contract.exe') }
if($LASTEXITCODE -ne 0){throw 'Native updater transport contract failed.'}
& (Join-Path $out 'capture.exe')
if($LASTEXITCODE -ne 0){throw 'Native capture transfer contract failed.'}
@{passed=$true;releaseAccess=$(if($PublicRelease){'anonymous public'}else{'authenticated private'});checks=@('actual GitHub API','Credential Manager remains empty after test','safe asset redirect','streamed package SHA-256 and length','embedded current and historical changelog');liveUacTested=$false}|
    ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'summary.json') -Encoding UTF8
