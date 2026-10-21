[CmdletBinding()]
param([string]$TrainerBinary)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'Run this smoke test on Windows.' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $TrainerBinary) { $TrainerBinary = Join-Path $repo 'bin/ItStealsTrainer.exe' }
if (@(Get-Process -Name 'It Steals', 'ItStealsTrainer' -ErrorAction SilentlyContinue).Count) { throw 'Run launcher smoke tests without a real game or trainer instance.' }
$work = Join-Path ([IO.Path]::GetTempPath()) ('SuiteLauncher-' + [guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($work)
$owned = New-Object 'System.Collections.Generic.List[System.Diagnostics.Process]'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SuiteSmokeWindow {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string name);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr w, IntPtr l);
}
'@
function Assert([bool]$Ok,[string]$Message) { if (-not $Ok) { throw $Message } }
function Wait-Until([scriptblock]$Condition, [string]$Message) {
    for ($i=0;$i -lt 150;$i++) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 100
    }
    throw $Message
}
function Start-ControlledTrainer([string]$Arguments = '') {
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=Join-Path $work 'ItStealsTrainer.exe'
    $info.WorkingDirectory=$work
    $info.Arguments=$Arguments
    $info.UseShellExecute=$false
    $info.EnvironmentVariables['LOCALAPPDATA']=Join-Path $work 'LocalAppData'
    $process=[Diagnostics.Process]::Start($info)
    $owned.Add($process)
    Wait-Until { $window=[SuiteSmokeWindow]::FindWindow('ItStealsPracticeControl.v6',$null); [uint32]$ownerId=0; if($window -ne [IntPtr]::Zero){$null=[SuiteSmokeWindow]::GetWindowThreadProcessId($window,[ref]$ownerId)}; $ownerId -eq $process.Id } 'Trainer control window did not appear.'
    return $process
}
function Close-ControlledTrainer($Process) {
    $window=[SuiteSmokeWindow]::FindWindow('ItStealsPracticeControl.v6',$null)
    [uint32]$ownerId=0
    $null=[SuiteSmokeWindow]::GetWindowThreadProcessId($window,[ref]$ownerId)
    Assert ($ownerId -eq $Process.Id) 'Refusing to close another process window.'
    $null=[SuiteSmokeWindow]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    Assert ($Process.WaitForExit(15000)) 'Trainer did not close normally.'
    Assert ($Process.ExitCode -eq 0) 'Trainer returned an error.'
}
function Read-StartedPid {
    try {
        if (-not (Test-Path -LiteralPath $script:started)) { return 0 }
        $text=[IO.File]::ReadAllText($script:started).Trim()
        [int]$value=0
        if ([int]::TryParse($text,[ref]$value) -and $value -gt 0) { return $value }
    } catch {}
    return 0
}
try {
    $game=Join-Path $work ('Game & caf' + [char]0xe9 + ' ' + [char]0x6e2c + [char]0x8a66)
    $null=[IO.Directory]::CreateDirectory((Join-Path $game 'It Steals_Data/Managed'))
    [IO.File]::WriteAllText((Join-Path $game 'It Steals_Data/Managed/Assembly-CSharp.dll'),'controlled test marker')
    $source=Join-Path $work 'Game.cs'
    [IO.File]::WriteAllText($source,@'
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
public class ControlledGame {
    [STAThread] public static void Main() {
        File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"started.txt"),Process.GetCurrentProcess().Id.ToString());
        while(true) Thread.Sleep(100);
    }
}
'@)
    $csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    & $csc /nologo /target:winexe /platform:x64 ("/out:"+(Join-Path $game 'It Steals.exe')) $source
    if ($LASTEXITCODE -ne 0) { throw 'Controlled game compilation failed.' }
    Copy-Item -LiteralPath $TrainerBinary -Destination (Join-Path $work 'ItStealsTrainer.exe')
    [IO.File]::WriteAllText((Join-Path $work 'installation.ini'),"gameExecutable=$(Join-Path $game 'It Steals.exe')", (New-Object Text.UTF8Encoding($false)))
    $script:started=Join-Path $game 'started.txt'
    $trainer=Start-ControlledTrainer
    Wait-Until { (Read-StartedPid) -gt 0 } 'Automatic game startup did not occur.'
    $gameProcess=[Diagnostics.Process]::GetProcessById((Read-StartedPid))
    $owned.Add($gameProcess)
    Close-ControlledTrainer $trainer

    $trainer=Start-ControlledTrainer
    Start-Sleep -Milliseconds 1000
    Assert ((Read-StartedPid) -eq $gameProcess.Id) 'Existing game was not reused.'
    $gameProcess.Kill()
    $null=$gameProcess.WaitForExit(5000)
    Remove-Item -LiteralPath $script:started
    Start-Sleep -Milliseconds 1500
    Assert (-not (Test-Path -LiteralPath $script:started)) 'Trainer restarted a closed game.'
    Close-ControlledTrainer $trainer

    $trainer=Start-ControlledTrainer '--no-launch'
    Start-Sleep -Milliseconds 1500
    Assert (-not (Test-Path -LiteralPath $script:started)) '--no-launch did not suppress startup.'
    Close-ControlledTrainer $trainer
    $settings=Join-Path $work 'LocalAppData/ItStealsPracticeSuite'
    $null=[IO.Directory]::CreateDirectory($settings)
    [IO.File]::WriteAllText((Join-Path $settings 'trainer.ini'),"version=18" + [Environment]::NewLine + "autoStartGame=0" + [Environment]::NewLine)
    $trainer=Start-ControlledTrainer
    Start-Sleep -Milliseconds 1500
    Assert (-not (Test-Path -LiteralPath $script:started)) 'Saved startup preference was ignored.'
    Close-ControlledTrainer $trainer
    $output=Join-Path $repo 'build/windows-launcher-tests'
    $null=[IO.Directory]::CreateDirectory($output)
    @{passed=$true; checks=@('native auto-start with Unicode path','existing game reused','no automatic restart','one-time suppression','saved preference')} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'summary.json') -Encoding UTF8
    Write-Output 'Windows native launcher smoke tests passed (controlled game, not Unity).'
} finally {
    foreach($process in $owned) {
        try { if(-not $process.HasExited){$process.Kill();$null=$process.WaitForExit(5000)};$process.Dispose() } catch {}
    }
    Remove-Item -LiteralPath $work -Recurse -Force
}
