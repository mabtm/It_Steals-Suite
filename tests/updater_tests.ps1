[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $repo 'Installer/Update.ps1') -RequestFile 'unused-request.json'
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('SuiteUpdateTests-'+[guid]::NewGuid().ToString('N'))
$null=[IO.Directory]::CreateDirectory($testRoot)
$passed=New-Object 'System.Collections.Generic.List[string]'
$owned=New-Object 'System.Collections.Generic.List[object]'
function Assert([bool]$Condition,[string]$Message) { if(-not $Condition){throw $Message} }
function Reject([scriptblock]$Action,[string]$Message) {
    $failed=$false
    try { & $Action | Out-Null } catch { $failed=$true }
    Assert $failed $Message
}
function Write-TestFile([string]$File,[string]$Text) {
    $null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($File))
    [IO.File]::WriteAllText($File,$Text,(New-Object Text.UTF8Encoding($false)))
}
function Save-Request($Request,[string]$Job) {
    $file=Join-Path $Job 'request.json'
    $null=[IO.Directory]::CreateDirectory($Job)
    $Request | ConvertTo-Json | Set-Content -LiteralPath $file -Encoding UTF8
    return $file
}
function Zip-Entries([string]$File,[string[]]$Names) {
    $zip=[IO.Compression.ZipFile]::Open($File,[IO.Compression.ZipArchiveMode]::Create)
    try { foreach($name in $Names){$null=$zip.CreateEntry($name)} } finally { $zip.Dispose() }
}
try {
    $game=Join-Path $testRoot 'Game & spaces'
    $app=Join-Path $testRoot 'Suite'
    $package=Join-Path $testRoot 'package'
    $job=Join-Path $testRoot 'Updates/job'
    $null=[IO.Directory]::CreateDirectory($game)
    $bytes=New-Object byte[] 128
    $bytes[0]=0x4d;$bytes[1]=0x5a;$bytes[60]=64;$bytes[64]=0x50;$bytes[65]=0x45;$bytes[68]=0x64;$bytes[69]=0x86
    [IO.File]::WriteAllBytes((Join-Path $game 'It Steals.exe'),$bytes)
    Write-TestFile (Join-Path $game 'It Steals_Data/Managed/Assembly-CSharp.dll') 'game marker'
    Write-TestFile (Join-Path $game 'MelonLoader/net35/MelonLoader.dll') 'existing loader'
    Write-TestFile (Join-Path $game 'version.dll') 'existing proxy'
    Write-TestFile (Join-Path $game 'Mods/Other.dll') 'other mod'
    Write-TestFile (Join-Path $app 'ItStealsTrainer.exe') 'original trainer'
    Write-TestFile (Join-Path $app 'keep/replay.isrun') 'user replay'
    foreach($name in @('ItStealsTrainer.exe','PracticeSuiteEncoder.exe','ItStealsHelper.dll','Installer/Install.ps1','Installer/Update.ps1','CHANGELOG.md','Encoder/NOTICE.md')) {
        Write-TestFile (Join-Path $package $name) ('updated '+$name)
    }
    $entries=@(Get-ChildItem -LiteralPath $package -File -Recurse | ForEach-Object {
        @{path=$_.FullName.Substring($package.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    @{version='7.12.1';files=$entries}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $package 'manifest.json') -Encoding UTF8
    Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
    $null=[IO.Directory]::CreateDirectory($job)
    $archive=Join-Path $job 'package.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($package,$archive)
    $request=[pscustomobject]@{
        version='7.12.1';sha256=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant();size=(Get-Item -LiteralPath $archive).Length
        application=$app;game=(Join-Path $game 'It Steals.exe');trainerPid=2147483000;trainerCreated='1';gamePid=0;gameCreated='0'
    }
    $requestFile=Save-Request $request $job
    $parsed=Read-UpdateRequest $requestFile
    Assert ($parsed.version -eq '7.12.1') 'Valid request rejected.'
    $manifest=Expand-UpdatePackage $archive (Join-Path $testRoot 'verified') $parsed
    Assert ($manifest.version -eq '7.12.1') 'Valid update extraction failed.'
    $passed.Add('verified complete package, optional update files, and Windows ZIP separators')

    $oldHash=$request.sha256
    $request.sha256='0'*64
    Reject { Expand-UpdatePackage $archive (Join-Path $testRoot 'wrong-hash') $request } 'Wrong hash accepted.'
    Assert (-not (Test-Path -LiteralPath (Join-Path $testRoot 'wrong-hash'))) 'Hash failure created extracted files.'
    $request.sha256=$oldHash
    $request.size++
    Reject { Expand-UpdatePackage $archive (Join-Path $testRoot 'wrong-size') $request } 'Truncated download accepted.'
    $request.size--
    $request.version='7.12.2'
    Reject { Expand-UpdatePackage $archive (Join-Path $testRoot 'wrong-version') $request } 'Wrong version accepted.'
    $request.version='7.12.1'
    Assert ([IO.File]::ReadAllText((Join-Path $app 'ItStealsTrainer.exe')) -eq 'original trainer') 'Rejected package modified the installation.'
    $passed.Add('hash, length, and release-version failures preserve the installed version')

    $n=0
    foreach($name in @('../escape','C:\escape','/escape','Encoder/a:stream','Encoder/CON','Encoder/NUL.txt','Encoder/trailing.','Encoder/trailing ','untrusted.exe','Encoder/a\..\escape')) {
        $bad=Join-Path $testRoot ('bad-'+$n+'.zip');$n++
        Zip-Entries $bad @($name,'README.txt','manifest.json','ItStealsTrainer.exe')
        $copy=$request.PSObject.Copy();$copy.sha256=(Get-FileHash -LiteralPath $bad).Hash.ToLowerInvariant();$copy.size=(Get-Item -LiteralPath $bad).Length
        Reject { Expand-UpdatePackage $bad (Join-Path $testRoot ('bad-extract-'+$n)) $copy } ('Unsafe archive accepted: '+$name)
    }
    $duplicate=Join-Path $testRoot 'duplicate.zip'
    Zip-Entries $duplicate @('Encoder/a','encoder/A','manifest.json','README.txt')
    $copy=$request.PSObject.Copy();$copy.sha256=(Get-FileHash -LiteralPath $duplicate).Hash.ToLowerInvariant();$copy.size=(Get-Item -LiteralPath $duplicate).Length
    Reject { Expand-UpdatePackage $duplicate (Join-Path $testRoot 'duplicates') $copy } 'Case-insensitive duplicate paths accepted.'
    $passed.Add('archive traversal, streams, reserved names, unexpected executables, and duplicate paths rejected')

    $copy=$request.PSObject.Copy();$copy.application=$game
    $badRequest=Save-Request $copy (Join-Path $testRoot 'bad-request')
    Reject { Read-UpdateRequest $badRequest } 'Overlapping game and suite roots accepted.'
    $copy=$request.PSObject.Copy();$copy.size=134217729
    $badRequest=Save-Request $copy (Join-Path $testRoot 'oversize-request')
    Reject { Read-UpdateRequest $badRequest } 'Oversize request accepted.'
    $passed.Add('request bounds and separate installation roots enforced before closing processes')

    $current=[Diagnostics.Process]::GetCurrentProcess()
    Reject { Get-UpdateProcess $current.Id (Join-Path $app 'ItStealsTrainer.exe') $current.StartTime.ToUniversalTime().ToFileTimeUtc().ToString() } 'Wrong executable identity accepted.'
    $current.Dispose()
    $passed.Add('process image and creation identity checked without closing unrelated applications')

    if([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        # Real held process handles and WM_CLOSE, using controlled WinForms
        # images. These fixtures do not launch or terminate the user's game.
        $csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
        $source=Join-Path $testRoot 'Fixture.cs'
        Write-TestFile $source 'using System;using System.Windows.Forms;class Fixture{[STAThread]static void Main(){var w=new Form();w.Text="Suite update fixture";w.ShowInTaskbar=false;w.Opacity=0;w.FormClosed+=(s,e)=>System.IO.File.WriteAllText(Application.ExecutablePath+".closed","graceful");Application.Run(w);}}'
        $fixture=Join-Path $testRoot 'Fixture.exe'
        & $csc /nologo /target:winexe /platform:x64 /reference:System.Windows.Forms.dll /reference:System.Drawing.dll ("/out:"+$fixture) $source
        if($LASTEXITCODE -ne 0){throw 'Controlled process compilation failed.'}
        Copy-Item -LiteralPath $fixture -Destination (Join-Path $game 'It Steals.exe') -Force
        Copy-Item -LiteralPath $fixture -Destination (Join-Path $app 'ItStealsTrainer.exe') -Force
        $trainer=Start-Process -FilePath (Join-Path $app 'ItStealsTrainer.exe') -PassThru;$owned.Add($trainer)
        $gameProcess=Start-Process -FilePath (Join-Path $game 'It Steals.exe') -PassThru;$owned.Add($gameProcess)
        $request.trainerPid=$trainer.Id;$request.trainerCreated=$trainer.StartTime.ToUniversalTime().ToFileTimeUtc().ToString()
        $request.gamePid=$gameProcess.Id;$request.gameCreated=$gameProcess.StartTime.ToUniversalTime().ToFileTimeUtc().ToString()
        Start-Sleep -Milliseconds 500
        $requestFile=Save-Request $request $job
        $verified=Get-UpdateProcess $trainer.Id (Join-Path $app 'ItStealsTrainer.exe') $request.trainerCreated
        $verified.Dispose()
        Reject { Get-UpdateProcess $trainer.Id (Join-Path $app 'ItStealsTrainer.exe') '1' } 'Reused PID identity accepted.'
        $passed.Add('live controlled Windows processes require exact image and creation time')
    }
    Invoke-UpdateApply $requestFile | Out-Null
    if([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        Assert ($trainer.HasExited -and $gameProcess.HasExited) 'Verified processes were not closed.'
        Assert (Test-Path -LiteralPath (Join-Path $app 'ItStealsTrainer.exe.closed')) 'Trainer fixture was forced to exit instead of closing normally.'
        Assert (Test-Path -LiteralPath (Join-Path $game 'It Steals.exe.closed')) 'Game fixture was forced to exit instead of closing normally.'
    }
    Assert ([IO.File]::ReadAllText((Join-Path $app 'ItStealsTrainer.exe')) -eq 'updated ItStealsTrainer.exe') 'Trainer update failed.'
    Assert ([IO.File]::ReadAllText((Join-Path $game 'Mods/ItStealsHelper.dll')) -eq 'updated ItStealsHelper.dll') 'Mod update failed.'
    Assert ([IO.File]::ReadAllText((Join-Path $app 'keep/replay.isrun')) -eq 'user replay') 'Replay changed.'
    Assert ([IO.File]::ReadAllText((Join-Path $game 'Mods/Other.dll')) -eq 'other mod') 'Other mod changed.'
    $passed.Add('production apply closes only verified processes and atomically updates trainer, encoder, mod, and receipt')

    $original=(Get-Item Function:Write-InstalledFile).ScriptBlock
    Set-Item Function:UpdateTestOriginalWriter $original
    $script:copyCount=0
    function Write-InstalledFile([string]$Source,[string]$Target,[string]$Recovery,$Journal) {
        $script:copyCount++
        if($script:copyCount -eq 3){throw 'Injected update write interruption'}
        UpdateTestOriginalWriter $Source $Target $Recovery $Journal
    }
    try { Reject { Invoke-UpdateApply $requestFile } 'Interrupted write was accepted.' }
    finally { Set-Item Function:Write-InstalledFile $original }
    Assert ([IO.File]::ReadAllText((Join-Path $app 'ItStealsTrainer.exe')) -eq 'updated ItStealsTrainer.exe') 'Update rollback failed.'
    $passed.Add('production update uses the installer rollback journal on interruption')

    $output=Join-Path $repo 'build/updater-tests'
    $null=[IO.Directory]::CreateDirectory($output)
    @{passed=$true;checks=@($passed.ToArray());liveUacTested=$false}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $output 'summary.json') -Encoding UTF8
    Write-Output "$($passed.Count) updater contract groups passed. UAC interaction and live Unity restart require an in-game test."
} finally {
    foreach($process in $owned){try{if(-not $process.HasExited){$process.Kill();$null=$process.WaitForExit(5000)};$process.Dispose()}catch{}}
    Remove-Item -LiteralPath $testRoot -Force -Recurse
}
