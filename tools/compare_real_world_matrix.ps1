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
$manifestText = (Get-Content -LiteralPath $resolvedManifest -Raw).Trim()
if ($manifestText.Length -eq 0 -or $manifestText[0] -ne '[') {
    throw 'manifest must contain a JSON array of datasets'
}
$manifestData = $manifestText | ConvertFrom-Json
$datasets = @($manifestData)
if ($datasets.Count -eq 0) {
    throw 'manifest must contain at least one dataset'
}

$resolvedReport = [System.IO.Path]::GetFullPath($Report)
$jsonReport = [System.IO.Path]::ChangeExtension($resolvedReport, '.json')
$reportDirectory = Split-Path -Parent $resolvedReport
if ([string]::IsNullOrEmpty($reportDirectory)) {
    $reportDirectory = (Get-Location).Path
}
New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
$singleComparison = Join-Path $PSScriptRoot 'compare_real_world_tiling.ps1'
$reportNames = @{}
$rows = @()
$failures = @()
$datasetIndex = 0

foreach ($dataset in $datasets) {
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
    $reportKey = $safeName.ToLowerInvariant()
    if ($reportNames.ContainsKey($reportKey)) {
        throw "dataset '$name' collides with dataset '$($reportNames[$reportKey])' after output-name sanitization"
    }
    $reportNames[$reportKey] = $name
    $datasetReport = Join-Path $reportDirectory "$safeName.tsv"
    if ([System.StringComparer]::OrdinalIgnoreCase.Equals(
            $datasetReport, $resolvedReport) -or
        [System.StringComparer]::OrdinalIgnoreCase.Equals(
            $datasetReport, $jsonReport)) {
        throw "dataset '$name' report would overwrite the aggregate report"
    }
    $sourceMetadata = [ordered]@{
        source_url = [string](Get-ManifestProperty $dataset 'sourceUrl' '')
        source_repository = [string](Get-ManifestProperty $dataset 'sourceRepository' '')
        source_license = [string](Get-ManifestProperty $dataset 'license' '')
        source_attribution = [string](Get-ManifestProperty $dataset 'attribution' '')
    }
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

    Write-Output "dataset=$name status=starting format=$format"
    try {
        & $singleComparison @arguments | Out-Null
        $datasetRows = Import-Csv -LiteralPath $datasetReport -Delimiter "`t"
        foreach ($row in $datasetRows) {
            $row | Add-Member -NotePropertyName dataset -NotePropertyValue $name
            foreach ($metadata in $sourceMetadata.GetEnumerator()) {
                $row | Add-Member -Force -NotePropertyName $metadata.Key `
                    -NotePropertyValue $metadata.Value
            }
            $rows += $row
        }
        Write-Output "dataset=$name status=completed report=$datasetReport"
    } catch {
        $failure = [ordered]@{
            dataset = $name
            format = $format
            input = [System.IO.Path]::GetFullPath($inputPath)
            error = $_.Exception.Message
        }
        $failures += [pscustomobject]$failure
        Write-Output "dataset=$name status=failed"
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

$json = [ordered]@{
    generated_utc = [DateTime]::UtcNow.ToString('O')
    benchmark = $resolvedBenchmark
    manifest = $resolvedManifest
    manifest_datasets = $datasets
    results = $rows
    failures = $failures
}
$json | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonReport -Encoding utf8

Write-Output "report=$resolvedReport"
Write-Output "json=$jsonReport"
if ($failures.Count -gt 0) {
    Write-Output "failures=$($failures.Count)"
}