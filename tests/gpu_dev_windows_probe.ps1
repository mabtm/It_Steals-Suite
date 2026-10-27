[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out = Join-Path $root 'build/gpu-dev-check'
$null = [IO.Directory]::CreateDirectory($out)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$studio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $studio 'VC/Auxiliary/Build/vcvars64.bat'
$resource = Join-Path $root 'Trainer/obj/x64/Release/TrainerResources.res'
if (-not (Test-Path $resource)) { throw 'Build the GPU dev trainer first; its embedded shader resources are required.' }
$cmd = Join-Path $out 'compile.cmd'
$binary = Join-Path $out 'gpu-dev-probe.exe'
[IO.File]::WriteAllLines($cmd, @(
    '@echo off', ('call "' + $vcvars + '" >nul'),
    ('cl /nologo /std:c++20 /EHsc /O2 /fp:strict /MT /utf-8 /UNDEBUG /D_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING /I"' +
     (Join-Path $root 'Trainer') + '" /Fo"' + (Join-Path $out 'probe.obj') + '" /Fe"' + $binary + '" "' +
     (Join-Path $root 'tests/gpu_dev_windows_probe.cpp') + '" "' + $resource + '" /link d3d11.lib d3dcompiler.lib dxgi.lib'),
    'exit /b %errorlevel%'
), [Text.Encoding]::ASCII)
& $env:ComSpec /d /c $cmd
if ($LASTEXITCODE -ne 0) { throw 'GPU dev probe compilation failed.' }
& $binary (Join-Path $root 'tests/fixtures') $out
if ($LASTEXITCODE -ne 0) { throw 'Integrated GPU scoring or reporting failed.' }
python (Join-Path $root 'tests/gpu_dev_validate.py') $out
if ($LASTEXITCODE -ne 0) { throw 'Exported GPU reports failed JSON validation.' }
