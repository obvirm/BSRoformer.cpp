param(
    [string]$ExePath,
    [string]$ModelPath,
    [string]$InputPath,
    [string]$OutputDir,
    [string]$Label = "current",
    [int]$Warmup = 2,
    [int]$Runs = 10,
    [string[]]$CliArgs = @()
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..")
$WorkspaceRoot = Split-Path -Parent $RepoRoot

if (-not $ExePath) {
    $ExePath = Join-Path $RepoRoot "build-msvc-cuda-release\bs_roformer-cli.exe"
}
if (-not $ModelPath) {
    $ModelPath = Join-Path $WorkspaceRoot "BS-RoFormer model\bs_roformer_anvuew_sdr_12.45-q8.0.gguf"
}
if (-not $InputPath) {
    $InputPath = Join-Path $WorkspaceRoot "test_segment.wav"
}
if (-not $OutputDir) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $WorkspaceRoot "bench_outputs\$Label-$stamp"
}

if ($Warmup -lt 0) {
    throw "Warmup must be >= 0"
}
if ($Runs -lt 1) {
    throw "Runs must be >= 1"
}
if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "CLI executable not found: $ExePath"
}
if (-not (Test-Path -LiteralPath $ModelPath)) {
    throw "Model not found: $ModelPath"
}
if (-not (Test-Path -LiteralPath $InputPath)) {
    throw "Input audio not found: $InputPath"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$results = New-Object System.Collections.Generic.List[object]
$total = $Warmup + $Runs

for ($i = 0; $i -lt $total; ++$i) {
    $phase = if ($i -lt $Warmup) { "warmup" } else { "run" }
    $index = if ($phase -eq "warmup") { $i + 1 } else { $i - $Warmup + 1 }
    $name = "$phase$index"
    $outputFile = Join-Path $OutputDir "$name.wav"
    $logFile = Join-Path $OutputDir "$name.log"

    Write-Host "=== $Label $name ==="
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $oldErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & $ExePath $ModelPath $InputPath $outputFile @CliArgs 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $oldErrorActionPreference
        $stopwatch.Stop()
    }

    $outputLines = @($output | ForEach-Object { $_.ToString() })
    $outputLines | Tee-Object -FilePath $logFile
    if ($exitCode -ne 0) {
        throw "Benchmark command failed with exit code $exitCode for $name"
    }
    if (-not (Test-Path -LiteralPath $outputFile)) {
        throw "Benchmark output was not created for $name"
    }

    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputFile).Hash
    $results.Add([pscustomobject]@{
        label = $Label
        phase = $phase
        index = $index
        seconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 6)
        output = $outputFile
        sha256 = $hash
        log = $logFile
    })
}

$measured = @($results | Where-Object { $_.phase -eq "run" })
$mean = ($measured | Measure-Object -Property seconds -Average).Average
$min = ($measured | Measure-Object -Property seconds -Minimum).Minimum
$max = ($measured | Measure-Object -Property seconds -Maximum).Maximum

$summary = [pscustomobject]@{
    label = $Label
    timestamp = (Get-Date).ToString("o")
    exe = (Resolve-Path -LiteralPath $ExePath).Path
    model = (Resolve-Path -LiteralPath $ModelPath).Path
    input = (Resolve-Path -LiteralPath $InputPath).Path
    output_dir = (Resolve-Path -LiteralPath $OutputDir).Path
    warmup = $Warmup
    runs = $Runs
    cli_args = $CliArgs
    mean_seconds = $mean
    min_seconds = $min
    max_seconds = $max
    output_hashes = @($measured.sha256 | Select-Object -Unique)
    results = $results
}

$jsonPath = Join-Path $OutputDir "summary.json"
$csvPath = Join-Path $OutputDir "results.csv"
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonPath -Encoding utf8
$results | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding utf8

Write-Host ("mean_seconds={0:N5} min_seconds={1:N5} max_seconds={2:N5}" -f $mean, $min, $max)
Write-Host ("output_hashes={0}" -f (($summary.output_hashes) -join ","))
Write-Host "summary_json=$jsonPath"
Write-Host "results_csv=$csvPath"
