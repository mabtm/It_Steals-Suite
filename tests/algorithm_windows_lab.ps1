[CmdletBinding()]
param([int]$Seeds = 10000, [switch]$Trainer)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out = Join-Path $root $(if ($Trainer) { 'build/trainer-pgo-lab' } else { 'build/algorithm-windows-lab' })
$null = [IO.Directory]::CreateDirectory($out)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$studio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $studio) { throw 'Visual C++ tools are required.' }
$vcvars = Join-Path $studio 'VC/Auxiliary/Build/vcvars64.bat'
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
$source = Join-Path $root 'tests/seed_route_benchmark.cpp'
$include = Join-Path $root 'Trainer'
$pgoDatabase = Join-Path $out 'search.pgd'
$baseline = Join-Path $out 'baseline.exe'
$trained = Join-Path $out 'trained.exe'
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name

function Build-Benchmark([string]$Target, [string]$Options) {
    if ($Trainer) {
        $mode = if ($Options -like '/GENPROFILE*') { 'Instrument' } elseif ($Options -like '/USEPROFILE*') { 'Use' } else { 'Baseline' }
        $name = [IO.Path]::GetFileNameWithoutExtension($Target)
        # Rebuild would delete the training .pgd. Reuse the same /GL objects and relink for /USEPROFILE.
        $buildTarget = if ($mode -eq 'Use') { '/t:Build' } else { '/t:Rebuild' }
        & $msbuild (Join-Path $root 'Trainer/ItStealsTrainer.vcxproj') /m $buildTarget /nologo /verbosity:minimal `
            /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:SuitePgoResearch=true `
            "/p:SuitePgoMode=$mode" "/p:SuitePgoDatabase=$pgoDatabase" "/p:TargetName=$name" `
            "/p:OutDir=$out/" "/p:IntDir=$out/obj-$name/"
        if ($LASTEXITCODE -ne 0) { throw 'Actual trainer PGO build failed.' }
        return
    }
    $cmd = Join-Path $out 'compile.cmd'
    [IO.File]::WriteAllLines($cmd, @(
        '@echo off', ('call "' + $vcvars + '" >nul'),
        ('cl /nologo /std:c++20 /EHsc /O2 /GL /MT /utf-8 /DNOMINMAX /I"' + $include + '" /Fo"' +
          (Join-Path $out 'search.obj') + '" /Fe"' + $Target + '" "' + $source + '" /link /LTCG ' + $Options),
        'exit /b %errorlevel%'
    ), [Text.Encoding]::ASCII)
    & $env:ComSpec /d /c $cmd
    if ($LASTEXITCODE -ne 0) { throw 'Search benchmark compilation failed.' }
}
function Run-Benchmark([string]$Binary, [string]$Fixture, [int]$Crystals, [int]$Count, [string]$Radius) {
    $map = Join-Path $root ('tests/fixtures/' + $Fixture + '.routefixture')
    if ($Trainer) {
        $resultFile = Join-Path $out 'last-benchmark.txt'
        $launchArgs = @('--suite-pgo-benchmark', $resultFile, $map, $Count, $Crystals, 1, 'search', $Radius)
        $quoted = ($launchArgs | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $process = Start-Process -FilePath $Binary -ArgumentList $quoted -Wait -PassThru
        $exitCode = $process.ExitCode
        $text = @(Get-Content $resultFile)
    } else {
        $text = @(& $Binary $map $Count $Crystals 1 search $Radius)
        $exitCode = $LASTEXITCODE
    }
    if ($exitCode -ne 0) {
        $text | Write-Output
        throw "Search benchmark failed: $Binary, $Fixture, radius=$Radius, exit=$exitCode"
    }
    $line = $text | Where-Object { $_ -like 'search,*' } | Select-Object -Last 1
    if (-not $line) { throw 'Benchmark result is missing.' }
    return [string]$line
}
function Median($Values) {
    $sorted = @($Values | Sort-Object)
    return $sorted[[int][Math]::Floor($sorted.Count / 2)]
}

Build-Benchmark $baseline ''
Build-Benchmark $trained ('/GENPROFILE:EXACT,PGD="' + $pgoDatabase + '"')
# The instrumentation runtime is a build input, never a release dependency.
$runtime = Get-ChildItem (Join-Path $studio 'VC/Tools/MSVC') -Filter pgort140.dll -Recurse |
    Where-Object { $_.DirectoryName -match 'bin\\Hostx64\\x64$' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if ($runtime) { Copy-Item $runtime.FullName $out }
# Atomic instrumentation is intentionally much slower. These representative
# layouts already exercise millions of inner-loop branches without timing out.
Run-Benchmark $trained 'hide-seek-outside-boxes' 52 128 '1.35' | Out-File (Join-Path $out 'training.log')
Run-Benchmark $trained 'hide-seek-outside-boxes' 52 128 '0.5' | Out-File (Join-Path $out 'training.log') -Append
Run-Benchmark $trained 'hide-seek-outside-boxes' 52 256 '0' | Out-File (Join-Path $out 'training.log') -Append
Run-Benchmark $trained 'hide-seek-boxes' 22 256 '1.35' | Out-File (Join-Path $out 'training.log') -Append
if (-not (Test-Path $pgoDatabase)) { throw 'PGO training database was not produced.' }
if ($Trainer) {
    $previousResearchPath = $env:PATH
    try {
        $env:PATH = $out + ';' + $env:PATH
        & (Join-Path $PSScriptRoot 'launcher_windows_tests.ps1') -TrainerBinary $trained
        if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Instrumented trainer UI training failed.' }
    } finally { $env:PATH = $previousResearchPath }
}
Build-Benchmark $trained ('/USEPROFILE:PGD="' + $pgoDatabase + '"')

$samples = @()
$reference = @{}
for ($pair = 1; $pair -le 3; $pair++) {
    $variants = if ($pair % 2) { @('baseline', 'pgo') } else { @('pgo', 'baseline') }
    foreach ($variant in $variants) {
        foreach ($radius in @('1.35', '0.5', '0')) {
            $binary = if ($variant -eq 'baseline') { $baseline } else { $trained }
            $line = Run-Benchmark $binary 'hide-seek-outside-boxes' 52 $Seeds $radius
            $cells = $line.Split(',')
            $fingerprint = ($cells | Where-Object { $_ -match '^\d+:[^:]+:\d+$' }) -join ','
            if ($reference.ContainsKey($radius) -and $reference[$radius] -ne $fingerprint) {
                throw "PGO changed the final Top 10 at radius $radius."
            }
            $reference[$radius] = $fingerprint
            $rate = [double]::Parse($cells[5], [Globalization.CultureInfo]::InvariantCulture)
            $samples += [ordered]@{ pair=$pair; variant=$variant; radius=$radius; rate=$rate; raw=$line }
            Write-Output "$variant pair $pair radius $radius : $rate seeds/s"
            @{ cpu=$cpu; compiler='MSVC v143 /O2 /GL /LTCG'; actualTrainer=[bool]$Trainer; samples=$samples } |
                ConvertTo-Json -Depth 6 | Set-Content (Join-Path $out 'pgo-results.json') -Encoding UTF8
        }
    }
}
foreach ($radius in @('1.35', '0.5', '0')) {
    $before = Median @($samples | Where-Object { $_.radius -eq $radius -and $_.variant -eq 'baseline' } | ForEach-Object { $_.rate })
    $after = Median @($samples | Where-Object { $_.radius -eq $radius -and $_.variant -eq 'pgo' } | ForEach-Object { $_.rate })
    Write-Output "MSVC radius $radius median: baseline=$before PGO=$after ratio=$($after/$before)"
}
Write-Output 'MSVC PGO training, paired complete searches and exact Top-10 parity: PASS'
if ($Trainer) {
    & (Join-Path $PSScriptRoot 'launcher_windows_tests.ps1') -TrainerBinary $trained
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Optimized actual trainer smoke tests failed.' }
}
