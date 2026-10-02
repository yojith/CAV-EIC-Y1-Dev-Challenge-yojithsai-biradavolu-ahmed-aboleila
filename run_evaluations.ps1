param(
    [string]$CasesPath = (Join-Path $PSScriptRoot 'evaluation_cases.json'),
    [string]$Generator = 'Visual Studio 16 2019'
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot
$cases = Get-Content -LiteralPath $CasesPath -Raw | ConvertFrom-Json
if ($cases.Count -eq 0) { throw 'The case file must contain a nonempty JSON array.' }

$versions = @(
    @{ Name = 'v1'; Flags = '' },
    @{ Name = 'v2'; Flags = '/DUSE_V2_STRATEGY=1' },
    @{ Name = 'v3-next'; Flags = '/DUSE_V3_STRATEGY=1' },
    @{ Name = 'v3-home'; Flags = '/DUSE_V3_STRATEGY=1 /DV3_HOME_PHEROMONE=1' }
)

foreach ($version in $versions) {
    $version.BuildDir = Join-Path $repoRoot "build-eval-$($version.Name)"
    $flags = "/EHsc /DWAIT_FOR_ENTER=0 /DENABLE_VISUALIZER=0 $($version.Flags)"
    & cmake -S $repoRoot -B $version.BuildDir -G $Generator -A x64 "-DCMAKE_CXX_FLAGS=$flags" -DBUILD_TESTING=OFF | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed for $($version.Name)." }
    & cmake --build $version.BuildDir --config Release --target dev_challenge | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $($version.Name)." }
    $version.Exe = Join-Path $version.BuildDir 'Release/dev_challenge.exe'
    if (-not (Test-Path -LiteralPath $version.Exe)) { throw "Executable not found: $($version.Exe)" }
}

$results = @()
foreach ($case in $cases) {
    foreach ($field in @('name', 'seed', 'rows', 'cols', 'ants', 'foodDensity')) {
        if ($case.PSObject.Properties.Name -notcontains $field) { throw "Case is missing '$field'." }
    }
    $seed = [uint32]$case.seed
    $rows = [int]$case.rows
    $cols = [int]$case.cols
    $ants = [int]$case.ants
    $density = [double]$case.foodDensity
    if ($rows -lt 1 -or $cols -lt 1 -or $ants -lt 1 -or
        [double]::IsNaN($density) -or $density -lt 0 -or $density -gt 1) {
        throw "Invalid parameters in case '$($case.name)'."
    }
    $initialFood = [int][math]::Truncate($rows * $cols * $density)
    $densityText = $density.ToString('R', [cultureinfo]::InvariantCulture)

    foreach ($version in $versions) {
        $outputLines = & $version.Exe $seed $rows $cols $ants $densityText
        if ($LASTEXITCODE -ne 0) { throw "Simulation failed: $($case.name) / $($version.Name)." }
        $scoreMatch = [regex]::Match(($outputLines -join "`n"), 'Total score:\s*(\d+)')
        if (-not $scoreMatch.Success) { throw "No score found: $($case.name) / $($version.Name)." }
        $boundMatch = [regex]::Match(($outputLines -join "`n"), 'Energy-only upper bound:\s*(\d+)')
        if (-not $boundMatch.Success) { throw "No energy bound found: $($case.name) / $($version.Name)." }
        $score = [int]$scoreMatch.Groups[1].Value
        $bound = [int]$boundMatch.Groups[1].Value
        if ($score -gt $bound) { throw "Score exceeds energy bound: $($case.name) / $($version.Name)." }
        $fraction = if ($initialFood -eq 0) { 0 } else { [math]::Round(100.0 * $score / $initialFood, 1) }
        $results += [pscustomobject]@{
            Case = $case.name
            Version = $version.Name
            Score = $score
            EnergyBound = $bound
            CollectedPercent = $fraction
        }
    }
}

$results | Format-Table -AutoSize
