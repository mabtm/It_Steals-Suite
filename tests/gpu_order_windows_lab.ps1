[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out = Join-Path $root 'build/gpu-order-lab'
$null = [IO.Directory]::CreateDirectory($out)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$studio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $studio 'VC/Auxiliary/Build/vcvars64.bat'
$binary = Join-Path $out 'gpu-order-lab.exe'
$cmd = Join-Path $out 'compile.cmd'
[IO.File]::WriteAllLines($cmd, @(
    '@echo off', ('call "' + $vcvars + '" >nul'),
    ('cl /nologo /std:c++20 /EHsc /O2 /fp:strict /MT /utf-8 /I"' + (Join-Path $root 'Trainer') +
     '" /Fo"' + (Join-Path $out 'cpu.obj') + '" /Fe"' + (Join-Path $out 'gpu-cpu-check.exe') + '" "' +
     (Join-Path $root 'tests/gpu_order_kernel_tests.cpp') + '"'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cl /nologo /std:c++20 /EHsc /O2 /fp:strict /MT /utf-8 /I"' + (Join-Path $root 'Trainer') +
     '" /Fo"' + (Join-Path $out 'gpu.obj') + '" /Fe"' + $binary + '" "' +
     (Join-Path $root 'tests/gpu_order_windows_lab.cpp') + '" /link d3d11.lib d3dcompiler.lib dxgi.lib'),
    'exit /b %errorlevel%'
), [Text.Encoding]::ASCII)
& $env:ComSpec /d /c $cmd
if ($LASTEXITCODE -ne 0) { throw 'GPU research host compilation failed.' }
& (Join-Path $out 'gpu-cpu-check.exe')
if ($LASTEXITCODE -ne 0) { throw 'Shared kernel differs from the MSVC production solver.' }
Copy-Item (Join-Path $root 'tests/gpu_order_lab.hlsl') $out
Copy-Item (Join-Path $root 'tests/gpu_order_kernel.hlsli') $out
& $binary (Join-Path $out 'gpu_order_lab.hlsl') (Join-Path $out 'gpu-results.json')
if ($LASTEXITCODE -eq 77) {
    Write-Output 'Shader compilation passed. Actual execution remains UNTESTED: no FP64-capable device.'
} elseif ($LASTEXITCODE -ne 0) { throw 'GPU kernel validation failed.' }
Get-Content (Join-Path $out 'gpu-results.json')
