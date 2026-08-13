[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Benchmark,
    [Parameter(Mandatory = $true)]
    [string]$InputPath,
    [Parameter(Mandatory = $true)]
    [ValidateSet('las', 'laz', 'copc', 'ply')]
    [string]$Format,
    [int]$Epsg,
    [int]$ChunkPoints = 65536,
    [double]$TileSize = 128,
    [UInt64]$MemoryLimitBytes = 1048576,
    [UInt64]$MaxPointsPerTile = 4096,
    [UInt64]$MinPointsPerTile = 1,
    [int]$MaxDepth = 16,
    [string]$Report = '.\real-world-tiling.tsv'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-BenchmarkFields {
    param([string]$Output)

    $line = $Output -split '\r?\n' |
        Where-Object { $_ -match '^format=' } |
        Select-Object -Last 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        throw "benchmark output did not contain a metrics line`n$Output"
    }

    $fields = [ordered]@{}
    foreach ($token in ($line -split '\s+')) {
        $separator = $token.IndexOf('=')
        if ($separator -le 0) { continue }
        $name = $token.Substring(0, $separator)
        $value = $token.Substring($separator + 1)
        $fields[$name] = $value
    }
    foreach ($required in @(
            'format', 'tiling', 'points', 'tile_count',
            'tile_manifest_count', 'tile_point_min', 'tile_point_max',
            'tile_point_average', 'tile_payload_min_bytes',
            'tile_payload_max_bytes', 'tile_payload_average_bytes',
            'tree_depth', 'peak_rss_bytes', 'rss_delta_bytes',
            'peak_spool_file_bytes', 'payload_bytes', 'source_read_bytes',
            'spool_bytes_written', 'spool_bytes_read', 'io_amplification',
            'output_bytes', 'process_write_bytes', 'elapsed_seconds',
            'success')) {
        if (-not $fields.Contains($required)) {
            throw "benchmark output is missing '$required'`n$Output"
        }
    }
    if ($fields['success'] -ne 'true') {
        throw "benchmark reported failure`n$Output"
    }
    return $fields
}

function Invoke-TilingBenchmark {
    param([ValidateSet('fixed-grid', 'adaptive')][string]$Strategy)

    $arguments = @(
        '--input', $InputPath,
        '--format', $Format,
        '--chunk-points', $ChunkPoints,
        '--tile-size', $TileSize,
        '--memory-limit', $MemoryLimitBytes,
        '--tiling', $Strategy
    )
    if ($Format -eq 'ply') {
        if ($Epsg -le 0) { throw '--epsg is required for PLY input' }
        $arguments += @('--epsg', $Epsg)
    }
    if ($Strategy -eq 'adaptive') {
        $arguments += @(
            '--max-points-per-tile', $MaxPointsPerTile,
            '--min-points-per-tile', $MinPointsPerTile,
            '--max-depth', $MaxDepth
        )
    }

    $output = & $Benchmark @arguments 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "benchmark failed with exit code $LASTEXITCODE`n$output"
    }
    return Get-BenchmarkFields $output
}

if (-not (Test-Path -LiteralPath $Benchmark -PathType Leaf)) {
    throw "benchmark executable was not found: $Benchmark"
}
if (-not (Test-Path -LiteralPath $InputPath -PathType Leaf)) {
    throw "input file was not found: $InputPath"
}
if ($ChunkPoints -le 0 -or $TileSize -le 0 -or $MemoryLimitBytes -eq 0 -or
    $MaxPointsPerTile -eq 0 -or $MinPointsPerTile -gt $MaxPointsPerTile -or
    $MaxDepth -lt 0) {
    throw 'benchmark limits are invalid'
}

$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
$resolvedBenchmark = (Resolve-Path -LiteralPath $Benchmark).Path
$resolvedReport = [System.IO.Path]::GetFullPath($Report)
$jsonReport = [System.IO.Path]::ChangeExtension($resolvedReport, '.json')
$protectedPaths = @($resolvedInput, $resolvedBenchmark)
foreach ($outputPath in @($resolvedReport, $jsonReport)) {
    if ($protectedPaths | Where-Object {
            [System.StringComparer]::OrdinalIgnoreCase.Equals($_, $outputPath) }) {
        throw "report output must not overwrite the input or benchmark: $outputPath"
    }
}
if ([System.StringComparer]::OrdinalIgnoreCase.Equals(
        $resolvedReport, $jsonReport)) {
    throw 'report path must not use the .json extension'
}
$reportDirectory = Split-Path -Parent $resolvedReport
if ([string]::IsNullOrEmpty($reportDirectory)) { $reportDirectory = (Get-Location).Path }
New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null

$sourceFile = Get-Item -LiteralPath $resolvedInput
$sourceHash = (Get-FileHash -LiteralPath $resolvedInput -Algorithm SHA256).Hash
$common = [ordered]@{
    input = $resolvedInput
    format = $Format
    source_size_bytes = $sourceFile.Length
    source_sha256 = $sourceHash
    chunk_points = $ChunkPoints
    tile_size = $TileSize
    memory_limit_bytes = $MemoryLimitBytes
    max_points_per_tile = $MaxPointsPerTile
    min_points_per_tile = $MinPointsPerTile
    max_depth = $MaxDepth
}

$rows = @()
foreach ($strategy in @('fixed-grid', 'adaptive')) {
    $metrics = Invoke-TilingBenchmark $strategy
    $row = [ordered]@{ strategy = $strategy }
    foreach ($entry in $common.GetEnumerator()) { $row[$entry.Key] = $entry.Value }
    foreach ($entry in $metrics.GetEnumerator()) { $row[$entry.Key] = $entry.Value }
    $rows += $row
}

$columns = @(
    'format', 'strategy', 'input', 'source_size_bytes', 'source_sha256',
    'chunk_points', 'tile_size', 'memory_limit_bytes',
    'max_points_per_tile', 'min_points_per_tile', 'max_depth', 'points',
    'tile_count', 'tile_manifest_count', 'tile_point_min', 'tile_point_max',
    'tile_point_average', 'tile_payload_min_bytes',
    'tile_payload_max_bytes', 'tile_payload_average_bytes', 'tree_depth',
    'peak_rss_bytes', 'rss_delta_bytes', 'peak_spool_file_bytes',
    'payload_bytes', 'source_read_bytes', 'spool_bytes_written',
    'spool_bytes_read', 'io_amplification', 'output_bytes',
    'process_write_bytes', 'elapsed_seconds', 'success'
)
$tsv = @(
    ($columns -join "`t")
)
foreach ($row in $rows) {
    $tsv += (($columns | ForEach-Object { [string]$row[$_] }) -join "`t")
}
[System.IO.File]::WriteAllLines($resolvedReport, $tsv)

$json = [ordered]@{
    generated_utc = [DateTime]::UtcNow.ToString('O')
    benchmark = $resolvedBenchmark
    configuration = $common
    results = $rows
}
$json | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $jsonReport -Encoding utf8

Write-Output "report=$resolvedReport"
Write-Output "json=$jsonReport"