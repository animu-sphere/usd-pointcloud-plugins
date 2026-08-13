[CmdletBinding()]
param(
    [string]$Converter = '.\build\cy2026-windows-x86_64-py313-usd\tools\usd-pointcloud-convert\usd-pointcloud-convert.exe',
    [string]$InputPath = '.\plugins\pointcloud-las\tests\corpus\usgs-3dep-2020\usgs-3dep-2020-thinned-4096.las',
    [string]$OutputDirectory = '.\build\usdview-fixture-usgs-4096'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedConverter = (Resolve-Path -LiteralPath $Converter).Path
$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$rootPath = Join-Path $resolvedOutput 'PointCloud.usda'
$payloadDirectory = Join-Path $resolvedOutput 'PointCloud_payloads'
$manifestPath = Join-Path $resolvedOutput 'PointCloud.usda.manifest'
$tileManifestPath = Join-Path $payloadDirectory 'tiles.manifest'

if (Test-Path -LiteralPath $resolvedOutput) {
    throw "output directory already exists: $resolvedOutput"
}
New-Item -ItemType Directory -Path $resolvedOutput | Out-Null

& $resolvedConverter $resolvedInput $rootPath `
    --max-points-per-tile 256 `
    --min-points-per-tile 1 `
    --max-depth 8 `
    --memory-limit 1048576 `
    --attributes xyz,intensity
if ($LASTEXITCODE -ne 0) {
    throw "converter failed with exit code $LASTEXITCODE"
}

foreach ($requiredPath in @($rootPath, $manifestPath, $tileManifestPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "fixture is missing required output: $requiredPath"
    }
}

$payloadCount = @(Get-ChildItem -File -Recurse $payloadDirectory -Filter '*.usdc').Count
if ($payloadCount -lt 2) {
    throw "fixture contains too few payloads: $payloadCount"
}

[PSCustomObject]@{
    root = $rootPath
    payload_directory = $payloadDirectory
    payload_count = $payloadCount
    manifest = $manifestPath
    tile_manifest = $tileManifestPath
} | ConvertTo-Json
