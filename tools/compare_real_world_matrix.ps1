[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Benchmark,
    [Parameter(Mandatory = $true)]
    [string]$Manifest,
    [string]$Report = '.\real-world-tiling-matrix.tsv',
    [switch]$ContinueOnError
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ManifestProperty {
    param(
        [pscustomobject]$Dataset,
        [string]$Name,
        [object]$Default = $null
    )

    $property = $Dataset.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        return $Default
    }
    return $property.Value
}

$resolvedBenchmark = (Resolve-Path -LiteralPath $Benchmark).Path
$resolvedManifest = (Resolve-Path -LiteralPath $Manifest).Path
$manifestDirectory = Split-Path -Parent $resolvedManifest
$manifestData = Get-Content -LiteralPath $resolvedManifest -Raw |
    ConvertFrom-Json
if ($manifestData -isnot [System.Collections.IEnumerable] -or
    $manifestData -is [string]) {
    throw 'manifest must contain a JSON array of datasets'
}

$resolvedReport = [System.IO.Path]::GetFullPath($Report)
$reportDirectory = Split-Path -Parent $resolvedReport
if ([string]::IsNullOrEmpty($reportDirectory)) {
    $reportDirectory = (Get-Location).Path
}
New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
$singleComparison = Join-Path $PSScriptRoot 'compare_real_world_tiling.ps1'
$rows = @()
$failures = @()
$datasetIndex = 0

foreach ($dataset in $manifestData) {
    ++$datasetIndex
    $name = [string](Get-ManifestProperty $dataset 'name')
    $format = ([string](Get-ManifestProperty $dataset 'format')).ToLowerInvariant()
    $input = [string](Get-ManifestProperty $dataset 'input')
    if ([string]::IsNullOrWhiteSpace($name) -or
        [string]::IsNullOrWhiteSpace($input) -or
        $format -notin @('las', 'laz', 'copc', 'ply')) {
        throw "dataset $datasetIndex must define name, input, and format"
    }

    $inputPath = $input
    if (-not [System.IO.Path]::IsPathRooted($inputPath)) {
        $inputPath = Join-Path $manifestDirectory $inputPath
    }
    $safeName = $name -replace '[^A-Za-z0-9._-]', '_'
    if ([string]::IsNullOrWhiteSpace($safeName)) {
        throw "dataset $datasetIndex has an invalid name"
    }
    $datasetReport = Join-Path $reportDirectory "$safeName.tsv"
    $arguments = @{
        Benchmark = $resolvedBenchmark
        InputPath = $inputPath
        Format = $format
        ChunkPoints = [int](Get-ManifestProperty $dataset 'chunkPoints' 65536)
        TileSize = [double](Get-ManifestProperty $dataset 'tileSize' 128)
        MemoryLimitBytes = [UInt64](Get-ManifestProperty $dataset 'memoryLimitBytes' 1048576)
        MaxPointsPerTile = [UInt64](Get-ManifestProperty $dataset 'maxPointsPerTile' 4096)
        MinPointsPerTile = [UInt64](Get-ManifestProperty $dataset 'minPointsPerTile' 1)
        MaxDepth = [int](Get-ManifestProperty $dataset 'maxDepth' 16)
        Report = $datasetReport
    }
    $epsg = [int](Get-ManifestProperty $dataset 'epsg' 0)
    if ($format -eq 'ply') {
        if ($epsg -le 0) { throw "dataset '$name' requires a positive epsg" }
        $arguments.Epsg = $epsg
    }

    try {
        & $singleComparison @arguments | Out-Null
        $datasetRows = Import-Csv -LiteralPath $datasetReport -Delimiter "`t"
        foreach ($row in $datasetRows) {
            $row | Add-Member -NotePropertyName dataset -NotePropertyValue $name
            $rows += $row
        }
    } catch {
        $failure = [ordered]@{
            dataset = $name
            format = $format
            input = [System.IO.Path]::GetFullPath($inputPath)
            error = $_.Exception.Message
        }
        $failures += [pscustomobject]$failure
        if (-not $ContinueOnError) { throw }
    }
}

$columns = @('dataset')
foreach ($row in $rows) {
    foreach ($property in $row.PSObject.Properties.Name) {
        if ($property -ne 'dataset' -and $columns -notcontains $property) {
            $columns += $property
        }
    }
}
$tsv = @($columns -join "`t")
foreach ($row in $rows) {
    $tsv += (($columns | ForEach-Object {
                if ($null -eq $row.$_) { '' } else { [string]$row.$_ }
            }) -join "`t")
}
[System.IO.File]::WriteAllLines($resolvedReport, $tsv)

$jsonReport = [System.IO.Path]::ChangeExtension($resolvedReport, '.json')
$json = [ordered]@{
    generated_utc = [DateTime]::UtcNow.ToString('O')
    benchmark = $resolvedBenchmark
    manifest = $resolvedManifest
    results = $rows
    failures = $failures
}
$json | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonReport -Encoding utf8

Write-Output "report=$resolvedReport"
Write-Output "json=$jsonReport"
if ($failures.Count -gt 0) {
    Write-Output "failures=$($failures.Count)"
}