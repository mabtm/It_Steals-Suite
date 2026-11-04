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
$fixtureExecutable = ''
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SuiteSmokeWindow {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr FindWindow(string cls, string name);
    // Keep the native null in C#: PowerShell can convert a null string to empty.
    public static IntPtr FindControlWindow() { return FindWindow("ItStealsPracticeControl.v6", null); }
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
    Wait-Until { if($process.HasExited){throw "Trainer exited before creating its control window (code $($process.ExitCode))."}; $window=[SuiteSmokeWindow]::FindControlWindow(); [uint32]$ownerId=0; if($window -ne [IntPtr]::Zero){$null=[SuiteSmokeWindow]::GetWindowThreadProcessId($window,[ref]$ownerId)}; $ownerId -eq $process.Id } 'Trainer control window did not appear.'
    return $process
}
function Close-ControlledTrainer($Process) {
    $window=[SuiteSmokeWindow]::FindControlWindow()
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
    # Exercise production file replacement and routing on Windows as well as Linux.
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $msbuild=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild) { throw 'MSBuild is required for the native checks.' }
    $nativeIncludes=[Security.SecurityElement]::Escape((Join-Path $repo 'Trainer'))
    foreach ($nativeTest in @('startup_logos_tests','route_order_tests','route_bound_tests','seed_neighbor_tests','hotkey_tests','font_catalog_tests','physical_repair_tests','replay_library_tests','run_statistics_tests','navigation_visibility_tests','navigation_startup_tests')) {
        $nativeSource=[Security.SecurityElement]::Escape((Join-Path $repo ('tests/'+$nativeTest+'.cpp')))
        $nativeOutput=[Security.SecurityElement]::Escape((Join-Path $work 'native'))
        $nativeProject=Join-Path $work 'startup-tests.vcxproj'
        [IO.File]::WriteAllText($nativeProject,@"
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
  <PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup><OutDir>$nativeOutput\</OutDir><IntDir>$nativeOutput\obj\</IntDir><TargetName>$nativeTest</TargetName></PropertyGroup>
  <ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><Optimization>MaxSpeed</Optimization><FloatingPointModel>Strict</FloatingPointModel><AdditionalIncludeDirectories>$nativeIncludes;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories><PreprocessorDefinitions>NOMINMAX;%(PreprocessorDefinitions)</PreprocessorDefinitions><AdditionalOptions>/utf-8 /UNDEBUG %(AdditionalOptions)</AdditionalOptions><RuntimeLibrary>MultiThreaded</RuntimeLibrary></ClCompile><Link><SubSystem>Console</SubSystem></Link></ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="$nativeSource" /></ItemGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
"@)
        & $msbuild $nativeProject /p:Configuration=Release /p:Platform=x64 /nologo /verbosity:minimal
        if ($LASTEXITCODE -ne 0) { throw "Native check compilation failed: $nativeTest" }
        $nativeArgument = if ($nativeTest -eq 'navigation_startup_tests') { Join-Path $repo 'tests/fixtures' } else { $work }
        & (Join-Path $work ('native/'+$nativeTest+'.exe')) $nativeArgument
        if ($LASTEXITCODE -ne 0) { throw "Native contract failed: $nativeTest" }
    }
    $game=Join-Path $work ('Game & caf' + [char]0xe9 + ' ' + [char]0x6e2c + [char]0x8a66)
    $fixtureExecutable=Join-Path $game 'It Steals.exe'
    $null=[IO.Directory]::CreateDirectory((Join-Path $game 'It Steals_Data/Managed'))
    [IO.File]::WriteAllText((Join-Path $game 'It Steals_Data/Managed/Assembly-CSharp.dll'),'controlled test marker')
    $startupSettings=Join-Path $game 'It Steals_Data/globalgamemanagers'
    [IO.File]::WriteAllText($startupSettings,'unknown build must stay untouched')
    $source=Join-Path $work 'Game.cs'
    [IO.File]::WriteAllText($source,@'
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
public class ControlledGame {
    [STAThread] public static void Main() {
        File.WriteAllLines(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"arguments.txt"),Environment.GetCommandLineArgs());
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
    $launchArguments=[IO.File]::ReadAllLines((Join-Path $game 'arguments.txt'))
    Assert ($launchArguments -contains '--practice-suite-skip-intro') 'Saved intro preference did not reach the game before startup.'
    Assert ($launchArguments -contains '--melonloader.disablestartscreen') 'Loader startup screen was not suppressed.'
    Assert ([IO.File]::ReadAllText($startupSettings) -eq 'unknown build must stay untouched') 'An unsupported startup file was changed.'
    Assert (-not (Test-Path -LiteralPath ($startupSettings+'.practicesuite-original'))) 'An unsupported startup file was backed up or patched.'
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
    Assert ($trainer.WaitForExit(10000) -and $trainer.ExitCode -eq 0) 'Trainer did not close after the game exited.'

    $trainer=Start-ControlledTrainer '--no-launch'
    Start-Sleep -Milliseconds 1500
    Assert (-not (Test-Path -LiteralPath $script:started)) '--no-launch did not suppress startup.'
    Close-ControlledTrainer $trainer
    $settings=Join-Path $work 'LocalAppData/ItStealsPracticeSuite'
    $null=[IO.Directory]::CreateDirectory($settings)
    [IO.File]::WriteAllText((Join-Path $settings 'trainer.ini'),"version=18" + [Environment]::NewLine + "autoStartGame=0" + [Environment]::NewLine + "skipGameIntro=0" + [Environment]::NewLine)
    $trainer=Start-ControlledTrainer
    Start-Sleep -Milliseconds 1500
    Assert (-not (Test-Path -LiteralPath $script:started)) 'Saved startup preference was ignored.'
    Close-ControlledTrainer $trainer
    $trainer=Start-ControlledTrainer '--restart-after-update'
    Wait-Until { (Read-StartedPid) -gt 0 } 'Explicit update restart did not override the ordinary startup opt-out.'
    $launchArguments=[IO.File]::ReadAllLines((Join-Path $game 'arguments.txt'))
    Assert ($launchArguments -notcontains '--practice-suite-skip-intro') 'Saved intro opt-out was ignored.'
    Assert ($launchArguments -notcontains '--melonloader.disablestartscreen') 'Intro opt-out still suppressed the loader screen.'
    $restartedGame=[Diagnostics.Process]::GetProcessById((Read-StartedPid))
    $owned.Add($restartedGame)
    Close-ControlledTrainer $trainer
    . (Join-Path $repo 'Installer/Update.ps1') -RequestFile 'unused-request.json'
    $trainer=Start-ControlledTrainer '--no-launch'
    $verified=Get-UpdateProcess $trainer.Id (Join-Path $work 'ItStealsTrainer.exe') $trainer.StartTime.ToUniversalTime().ToFileTimeUtc().ToString()
    Close-UpdateProcess $verified
    Assert ($trainer.WaitForExit(5000) -and $trainer.ExitCode -eq 0) 'The updater did not close the native hidden control window normally.'
    $output=Join-Path $repo 'build/windows-launcher-tests'
    $null=[IO.Directory]::CreateDirectory($output)
    @{passed=$true; checks=@('native startup file replacement and recovery','frozen route orders and lengths','route bounds against exact optima','unsupported startup settings preserved','native auto-start with Unicode path','early intro and loader arguments','saved intro opt-out','existing game reused','trainer closes with the game','no automatic restart','one-time suppression','saved preference','explicit post-update restart overrides saved opt-out','updater gracefully closes actual hidden native control window')} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'summary.json') -Encoding UTF8
    Write-Output 'Windows native launcher smoke tests passed (controlled game, not Unity).'
} catch {
    Write-Output ("Launcher check failed: " + $_.Exception.Message)
    throw
} finally {
    foreach($process in $owned) {
        try { if(-not $process.HasExited){$process.Kill();$null=$process.WaitForExit(5000)};$process.Dispose() } catch {}
    }
    # Startup can fail before the child PID is registered; clean only our exact fixture image.
    if ($fixtureExecutable) {
        foreach ($candidate in @(Get-Process -Name 'It Steals' -ErrorAction SilentlyContinue)) {
            try {
                $module = $candidate.MainModule
                if ($module -and $module.FileName -eq $fixtureExecutable) {
                    $candidate.Kill()
                    $null=$candidate.WaitForExit(5000)
                }
            } catch [System.InvalidOperationException] {
                # The fixture may have exited between enumeration and inspection.
            } finally { $candidate.Dispose() }
        }
    }
    Remove-Item -LiteralPath $work -Recurse -Force
}
