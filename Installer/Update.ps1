[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RequestFile,
    [switch]$Apply
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Install.ps1')

# This helper is embedded in the current trainer, not executed from a newly
# downloaded archive. The controller stays unelevated and owns the restart;
# only the short, transactional application step requests Windows elevation.
function Read-UpdateRequest([string]$File) {
    $File = [IO.Path]::GetFullPath($File)
    Assert-NoReparsePoint $File
    if ([IO.Path]::GetFileName($File) -ne 'request.json' -or
        -not (Test-Path -LiteralPath $File -PathType Leaf) -or (Get-Item -LiteralPath $File).Length -gt 16384) { throw 'The update request is missing or invalid.' }
    $request = [IO.File]::ReadAllText($File) | ConvertFrom-Json
    foreach ($key in @('version','sha256','size','application','game','trainerPid','trainerCreated','gamePid','gameCreated')) {
        if (-not $request.PSObject.Properties[$key]) { throw "Missing update field: $key" }
    }
    if ($request.version -notmatch '^\d{1,6}\.\d{1,6}\.\d{1,6}$' -or $request.sha256 -notmatch '^[a-f0-9]{64}$' -or
        [long]$request.size -le 0 -or [long]$request.size -gt 134217728 -or
        [long]$request.trainerPid -le 0 -or [long]$request.gamePid -lt 0 -or
        [string]$request.trainerCreated -notmatch '^\d{1,19}$' -or [string]$request.gameCreated -notmatch '^\d{1,19}$') { throw 'The update request has invalid identity or size fields.' }
    foreach ($path in @($request.application,$request.game)) {
        if (-not [IO.Path]::IsPathRooted($path)) { throw 'Update destinations must be absolute.' }
        Assert-NoReparsePoint $path
    }
    $request.application = [IO.Path]::GetFullPath($request.application).TrimEnd([char[]]'\/')
    $request.game = [IO.Path]::GetFullPath($request.game)
    if ([IO.Path]::GetFileName($request.game) -ne 'It Steals.exe' -or
        -not (Test-GameFolder ([IO.Path]::GetDirectoryName($request.game)))) { throw 'The selected game is not a supported It Steals installation.' }
    $gameFolder=[IO.Path]::GetDirectoryName($request.game)
    if ($request.application -eq $gameFolder -or $request.application.StartsWith($gameFolder.TrimEnd([char[]]'\/')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'PracticeSuite must be installed outside the game folder.' }
    $trainer = Join-Path $request.application 'ItStealsTrainer.exe'
    if (-not (Test-Path -LiteralPath $trainer -PathType Leaf)) { throw 'The original trainer was moved. Run Setup before updating.' }
    return $request
}

function Expand-UpdatePackage([string]$Archive,[string]$Destination,$Request) {
    Assert-NoReparsePoint $Archive
    if (-not (Test-Path -LiteralPath $Archive -PathType Leaf) -or (Get-Item -LiteralPath $Archive).Length -ne [long]$Request.size -or
        (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash -ne $Request.sha256) { throw 'The downloaded update failed size or SHA-256 verification. No installed files were changed.' }
    if (Test-Path -LiteralPath $Destination) { throw 'The extraction folder must be a new, empty update folder.' }
    $null = [IO.Directory]::CreateDirectory($Destination)
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        if ($zip.Entries.Count -lt 4 -or $zip.Entries.Count -gt 512) { throw 'The update archive contains an unexpected number of entries.' }
        $seen = @{}
        [long]$expanded = 0
        foreach ($entry in $zip.Entries) {
            # Windows PowerShell's ZipFile writer emits backslashes. Normalize
            # both forms before duplicate checks and path containment checks.
            $name = $entry.FullName.Replace('\','/')
            $target = Get-ContainedPath $Destination $name
            if ($seen.ContainsKey($name)) { throw 'The update archive contains duplicate paths.' }
            $seen[$name] = $true
            foreach ($part in $name.TrimEnd('/').Split('/')) {
                if (-not $part -or $part.EndsWith('.') -or $part.EndsWith(' ') -or $part -match '[<>"|?*\x00-\x1f]' -or
                    $part -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { throw 'The update archive contains a reserved Windows path.' }
            }
            if ((($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000) { throw 'Symbolic links are not supported in updates.' }
            if ($name.EndsWith('/')) { $null=[IO.Directory]::CreateDirectory($target); continue }
            if ($name -notin @('README.txt','manifest.json')) { $null = Get-SuiteTarget $name ([IO.Path]::GetDirectoryName($Request.game)) $Request.application }
            $expanded += $entry.Length
            if ($entry.Length -gt 134217728 -or $expanded -gt 268435456) { throw 'The expanded update exceeds its size limit.' }
            $null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target)
        }
        $manifest = Read-PackageManifest $Destination
        if ($manifest.version -ne $Request.version) { throw 'The package version does not match the selected GitHub release.' }
        return $manifest
    } finally { $zip.Dispose() }
}

function Get-UpdateProcess([long]$ProcessId,[string]$ExpectedImage,[string]$Created) {
    if (-not $ProcessId) { return $null }
    $process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if (-not $process) { return $null }
    try {
        $null = $process.Handle # Retain a handle to this process instance.
        if (-not [string]::Equals([IO.Path]::GetFullPath($process.MainModule.FileName),[IO.Path]::GetFullPath($ExpectedImage),[StringComparison]::OrdinalIgnoreCase) -or
            $process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString() -ne $Created) { throw 'A process identity changed. The updater refused to close it.' }
        return $process
    } catch { $process.Dispose(); throw }
}

function Initialize-UpdateWindows {
    if ('SuiteUpdateWindows' -as [type]) { return }
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class SuiteUpdateWindows {
    private delegate bool Visit(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] private static extern bool EnumWindows(Visit callback, IntPtr parameter);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint owner);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder text, int maximum);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] private static extern bool PostMessage(IntPtr window, uint message, IntPtr w, IntPtr l);
    public static void RequestClose(uint process) {
        IntPtr control=IntPtr.Zero;
        EnumWindows(delegate(IntPtr window,IntPtr parameter) {
            uint owner;
            GetWindowThreadProcessId(window,out owner);
            if(owner==process) {
                var name=new StringBuilder(256);
                GetClassName(window,name,name.Capacity);
                if(name.ToString()=="ItStealsPracticeControl.v6") { control=window; return false; }
            }
            return true;
        },IntPtr.Zero);
        if(control!=IntPtr.Zero) { PostMessage(control,0x10,IntPtr.Zero,IntPtr.Zero); return; }
        EnumWindows(delegate(IntPtr window,IntPtr parameter) {
            uint owner;
            GetWindowThreadProcessId(window,out owner);
            if(owner==process && IsWindowVisible(window)) PostMessage(window,0x10,IntPtr.Zero,IntPtr.Zero);
            return true;
        },IntPtr.Zero);
    }
}
'@
}

function Close-UpdateProcess($Process) {
    if (-not $Process) { return }
    try {
        if ($Process.HasExited) { return }
        Initialize-UpdateWindows
        # The trainer's stable control window is hidden and its overlay is
        # owned by the game. Process.CloseMainWindow() cannot find it.
        [SuiteUpdateWindows]::RequestClose([uint32]$Process.Id)
        if (-not $Process.WaitForExit(10000)) {
            # Install explicitly authorizes closing the two verified processes.
            # A held OS handle prevents a reused PID from identifying a new app.
            $Process.Kill()
            if (-not $Process.WaitForExit(5000)) { throw 'The game or trainer did not close. Close it manually and retry.' }
        }
    } finally { $Process.Dispose() }
}

function Assert-NoActiveEncoder([string]$Application) {
    foreach ($process in @(Get-Process -Name 'PracticeSuiteEncoder' -ErrorAction SilentlyContinue)) {
        try {
            if ([string]::Equals($process.MainModule.FileName,(Join-Path $Application 'PracticeSuiteEncoder.exe'),[StringComparison]::OrdinalIgnoreCase)) {
                throw 'An export is still running. Finish the export before installing.'
            }
        } finally { $process.Dispose() }
    }
}

function Set-UpdateStatus([string]$Job,[string]$Message) {
    [IO.File]::WriteAllText((Join-Path $Job 'status.txt'),$Message,(New-Object Text.UTF8Encoding($false)))
}

function Invoke-UpdateApply([string]$File) {
    $request=Read-UpdateRequest $File
    $job=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($File))
    $payload=Join-Path $job ('payload-'+[guid]::NewGuid().ToString('N'))
    $mutex=New-Object Threading.Mutex($false,'Local\ItStealsPracticeSuite.UpdateInstallation')
    $locked=$false
    $trainer=$null; $game=$null
    try {
        try { $locked=$mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $locked=$true }
        if (-not $locked) { throw 'Another PracticeSuite update is already being installed.' }
        Set-UpdateStatus $job 'Verifying package and process identities...'
        $null=Expand-UpdatePackage (Join-Path $job 'package.zip') $payload $request
        Assert-NoActiveEncoder $request.application
        $trainer=Get-UpdateProcess $request.trainerPid (Join-Path $request.application 'ItStealsTrainer.exe') $request.trainerCreated
        $game=Get-UpdateProcess $request.gamePid $request.game $request.gameCreated
        if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { Initialize-UpdateWindows }
        Set-UpdateStatus $job 'Closing It Steals and PracticeSuite...'
        Close-UpdateProcess $game; $game=$null
        Close-UpdateProcess $trainer; $trainer=$null
        # Install-Suite owns the existing verified write journal and rollback.
        # It also writes the installation receipt and automatic launch path.
        Set-UpdateStatus $job 'Installing and verifying the complete Suite...'
        Install-Suite ([IO.Path]::GetDirectoryName($request.game)) $request.application $payload
        $uninstall='HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\ItStealsPracticeSuite'
        if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT -and (Test-Path -LiteralPath $uninstall)) {
            $registered=Get-ItemProperty -LiteralPath $uninstall
            if ($registered.PSObject.Properties['InstallLocation'] -and
                [string]::Equals($registered.InstallLocation,$request.application,[StringComparison]::OrdinalIgnoreCase)) {
                try { Set-ItemProperty -LiteralPath $uninstall -Name DisplayVersion -Value $request.version } catch { Write-Output "Installation succeeded; Windows display version could not be refreshed." }
            }
        }
        Set-UpdateStatus $job ('Installed v'+$request.version+'. Restarting PracticeSuite and It Steals.')
    } finally {
        if ($trainer) { $trainer.Dispose() }
        if ($game) { $game.Dispose() }
        if (Test-Path -LiteralPath $payload) { Remove-Item -LiteralPath $payload -Recurse -Force }
        if ($locked) { $mutex.ReleaseMutex() }
        $mutex.Dispose()
    }
}

function Invoke-UpdateController([string]$File) {
    $request=Read-UpdateRequest $File
    $job=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($File))
    $result=Join-Path ([IO.Path]::GetDirectoryName($job)) 'result.txt'
    $original=Get-UpdateProcess $request.trainerPid (Join-Path $request.application 'ItStealsTrainer.exe') $request.trainerCreated
    try {
        Set-UpdateStatus $job 'Waiting for Windows installation permission. Cancelling keeps the current version running.'
        $arguments='-NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -WindowStyle Hidden -File "'+$PSCommandPath+'" -RequestFile "'+[IO.Path]::GetFullPath($File)+'" -Apply'
        # The child requests elevation before closing either application. The
        # original unelevated controller starts the game after the child exits.
        $child=Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -Verb RunAs -ArgumentList $arguments -WindowStyle Hidden -PassThru
        try {
            $null=$child.Handle
            $child.WaitForExit()
            if ($null -eq $child.ExitCode -or $child.ExitCode -ne 0) {
                $status=[IO.File]::ReadAllText((Join-Path $job 'status.txt'))
                throw $status
            }
        } finally { $child.Dispose() }
        [IO.File]::WriteAllText($result,('SUCCESS: Installed v'+$request.version+'. Settings and replays were kept.'),(New-Object Text.UTF8Encoding($false)))
    } catch {
        $message='Update did not complete: '+$_.Exception.Message
        Set-UpdateStatus $job $message
        [IO.File]::WriteAllText($result,$message,(New-Object Text.UTF8Encoding($false)))
        throw
    } finally {
        # Only restart after the original trainer actually stopped. This also
        # recovers a rolled-back installation without creating a second trainer.
        try {
            if (-not $original -or $original.HasExited) {
                $exe=Join-Path $request.application 'ItStealsTrainer.exe'
                if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'The trainer executable is missing.' }
                $null=Start-Process -FilePath $exe -WorkingDirectory $request.application -ArgumentList '--restart-after-update' -PassThru
            }
        } catch {
            $message='The update helper could not restart PracticeSuite. Start the trainer manually. '+$_.Exception.Message
            Set-UpdateStatus $job $message
            [IO.File]::WriteAllText($result,$message,(New-Object Text.UTF8Encoding($false)))
            Add-Type -AssemblyName System.Windows.Forms
            $null=[Windows.Forms.MessageBox]::Show($message,'PracticeSuite update')
        } finally { if ($original) { $original.Dispose() } }
        if (Test-Path -LiteralPath (Join-Path $job 'package.zip')) { Remove-Item -LiteralPath (Join-Path $job 'package.zip') -Force }
    }
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'Updates require Windows 10 or later.' }
        if ($Apply) { Invoke-UpdateApply $RequestFile } else { Invoke-UpdateController $RequestFile }
        exit 0
    } catch {
        try { Set-UpdateStatus ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($RequestFile))) ('Update failed: '+$_.Exception.Message) } catch {}
        exit 1
    }
}
