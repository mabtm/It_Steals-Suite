[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out = Join-Path $root 'build/windows-seed-gpu'
$null = [IO.Directory]::CreateDirectory($out)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$studio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $studio 'VC/Auxiliary/Build/vcvars64.bat'
$normalResource = Join-Path $root 'Trainer/obj/x64/Release/TrainerResources.res'
if (-not (Test-Path $normalResource)) { throw 'Build the production trainer first.' }
# Inject a bad length only past layout 128: startup verification and the first
# visible 64 candidates succeed, then the real coordinator must restart on CPU.
$shader = [IO.File]::ReadAllText((Join-Path $root 'Trainer/SeedOrder.hlsl'))
if (-not $shader.Contains('outputLengths[testId] = length;')) { throw 'Fault-injection target is missing.' }
$shader = $shader.Replace('outputLengths[testId] = length;', 'outputLengths[testId] = length + (testId == 128 ? 1.0L : 0.0L);')
$badShader = Join-Path $out 'SeedOrder-fault.hlsl'
[IO.File]::WriteAllText($badShader, $shader)
$badRc = Join-Path $out 'fault.rc'
[IO.File]::WriteAllLines($badRc, @(
    ('105 RCDATA "' + $badShader.Replace('\', '/') + '"'),
    ('106 RCDATA "' + (Join-Path $root 'Trainer/SeedOrderKernel.hlsli').Replace('\', '/') + '"')
))
foreach ($fault in @($false, $true)) {
    $label = if ($fault) { 'fault' } else { 'normal' }
    $binary = Join-Path $out ("seed-gpu-$label.exe")
    $resource = if ($fault) { Join-Path $out 'fault.res' } else { $normalResource }
    $lines = @('@echo off', ('call "' + $vcvars + '" >nul'))
    if ($fault) { $lines += ('rc /nologo /fo "' + $resource + '" "' + $badRc + '"'); $lines += 'if errorlevel 1 exit /b %errorlevel%' }
    $lines += ('cl /nologo /std:c++20 /EHsc /O2 /fp:strict /MT /utf-8 /UNDEBUG /D_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING /I"' +
        (Join-Path $root 'Trainer') + '" /Fo"' + (Join-Path $out ("$label.obj")) + '" /Fe"' + $binary + '" "' +
        (Join-Path $root 'tests/seed_gpu_windows_tests.cpp') + '" "' + $resource + '" /link d3d11.lib d3dcompiler.lib dxgi.lib')
    $lines += 'exit /b %errorlevel%'
    $command = Join-Path $out ("compile-$label.cmd")
    [IO.File]::WriteAllLines($command, $lines, [Text.Encoding]::ASCII)
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw "Production GPU $label probe compilation failed." }
    if ($fault) { & $binary (Join-Path $root 'tests/fixtures') --fault | Tee-Object -FilePath (Join-Path $out 'fault.log') }
    else { & $binary (Join-Path $root 'tests/fixtures') | Tee-Object -FilePath (Join-Path $out 'normal.log') }
    if ($LASTEXITCODE -ne 0) { throw "Production GPU $label verification failed." }
}
