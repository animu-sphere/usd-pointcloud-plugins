param(
    [Parameter(Mandatory = $true)]
    [string]$Converter,
    [Parameter(Mandatory = $true)]
    [string]$Fixture,
    [Parameter(Mandatory = $true)]
    [string]$TestRoot,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$testRootPath = [System.IO.Path]::GetFullPath($TestRoot)
$outputRoot = Join-Path $testRootPath 'PointCloud.usda'
$transactionPath = "$outputRoot.transaction"
$transactionState = Join-Path $transactionPath 'state'
$payloadDirectory = Join-Path $testRootPath 'PointCloud_payloads'
$temporaryRoot = Join-Path $testRootPath 'PointCloud.tmp.usda'
$temporaryManifest = Join-Path $testRootPath 'PointCloud.usda.manifest.tmp'
$manifest = Join-Path $testRootPath 'PointCloud.usda.manifest'
$stdoutPath = Join-Path $testRootPath 'interrupted.stdout.log'
$stderrPath = Join-Path $testRootPath 'interrupted.stderr.log'
$nativeArguments = @(
    $Fixture,
    $outputRoot,
    '--chunk-points', '65536',
    '--tile-size', '128',
    '--memory-limit', '1048576',
    '--attributes', 'xyz,intensity'
)
$startArguments = (
    '"{0}" "{1}" --chunk-points 65536 --tile-size 128 ' +
    '--memory-limit 1048576 --attributes xyz,intensity'
) -f $Fixture, $outputRoot

Remove-Item -Recurse -Force $testRootPath -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $testRootPath | Out-Null
$watcher = [System.IO.FileSystemWatcher]::new(
    (Split-Path $transactionPath -Parent),
    (Split-Path $transactionPath -Leaf))
$watcher.IncludeSubdirectories = $true
$watcher.NotifyFilter = [System.IO.NotifyFilters]'DirectoryName,FileName'
$watcher.EnableRaisingEvents = $true
$sourceIdentifier = "usd-pointcloud-convert-interruption-$PID"
$subscription = Register-ObjectEvent -InputObject $watcher `
    -EventName Created -SourceIdentifier $sourceIdentifier
$process = $null
try {
    $process = Start-Process -FilePath $Converter -ArgumentList $startArguments `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath `
        -PassThru -WindowStyle Hidden
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not (Test-Path $transactionState) -and -not $process.HasExited) {
        $event = Wait-Event -SourceIdentifier $sourceIdentifier -Timeout 1
        if ($null -ne $event) {
            Remove-Event -EventIdentifier $event.EventIdentifier
        }
        if ([DateTime]::UtcNow -gt $deadline) {
            throw "converter did not publish transaction state within $TimeoutSeconds seconds"
        }
    }
    if ($process.HasExited) {
        throw "converter exited before interruption point with code $($process.ExitCode)"
    }
    Stop-Process -Id $process.Id -Force
    $process.WaitForExit()
} finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $process.WaitForExit()
        }
    }
    if ($null -ne $subscription) {
        Unregister-Event -SourceIdentifier $sourceIdentifier -ErrorAction SilentlyContinue
        Remove-Job -Name $sourceIdentifier -Force -ErrorAction SilentlyContinue
    }
    $watcher.Dispose()
}

if (-not (Test-Path $transactionPath)) {
    throw 'forced interruption did not leave a transaction marker'
}

$retryOutput = & $Converter @nativeArguments 2>&1
$retryExitCode = $LASTEXITCODE
if ($retryExitCode -ne 0) {
    throw "recovery conversion failed with exit code ${retryExitCode}: $retryOutput"
}

if (-not (Test-Path $outputRoot) -or -not (Test-Path $manifest) -or
    -not (Test-Path $payloadDirectory) -or (Test-Path $transactionPath) -or
    (Test-Path $temporaryRoot) -or (Test-Path $temporaryManifest)) {
    throw 'recovery conversion left an incomplete output bundle'
}

$payloadCount = @(Get-ChildItem $payloadDirectory -File -Filter '*.usdc').Count
if ($payloadCount -eq 0) {
    throw 'recovery conversion did not publish any payloads'
}

Write-Output "interrupted_exit=forced"
Write-Output "recovered_exit=$retryExitCode"
Write-Output "payload_count=$payloadCount"
Write-Output "output_root=$outputRoot"
Remove-Item -Recurse -Force $testRootPath