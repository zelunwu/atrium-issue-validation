param([switch]$RefreshImages)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$comparer = if ($env:ATRIUM_REG_CLI) { $env:ATRIUM_REG_CLI } else { Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $root))) 'reg-cli/dist/cli.mjs' }
if (!(Test-Path -LiteralPath $comparer)) { throw "Build reg-cli first: $comparer" }
if ($RefreshImages) { & (Join-Path $root 'Run-GpuComparison.ps1') }
if ($RefreshImages -and $LASTEXITCODE) { throw 'GPU image generation failed' }
$generated = Join-Path $root 'gpu-actual'
if (!(Test-Path -LiteralPath $generated)) { throw 'Run-GpuComparison.ps1 first to generate the synthetic actual images' }
$published = Join-Path $root 'visual-actual'
New-Item -ItemType Directory -Force -Path $published | Out-Null
Copy-Item -Path (Join-Path $generated '*.png') -Destination $published
$actualNames = @(Get-ChildItem -LiteralPath $published -Filter '*.png' -File | ForEach-Object Name | Sort-Object)
$expectedNames = @(Get-ChildItem -LiteralPath (Join-Path $root 'gpu-expected') -Filter '*.png' -File | ForEach-Object Name | Sort-Object)
if ($actualNames.Count -ne 147 -or (Compare-Object $actualNames $expectedNames)) { throw 'Published image inventory does not match the 147 references' }
Push-Location $root
try {
    python check_pixel_parity.py gpu-expected visual-actual
    if ($LASTEXITCODE) { throw 'Exact pixel comparison failed' }
    node $comparer visual-actual gpu-expected visual-diff -R visual-report.html -J visual-report.json --diffFormat png
    if ($LASTEXITCODE) { throw 'Visual report generation failed' }
    if ($RefreshImages) { & (Join-Path $root 'Run-Comparison.ps1') }
    if ($RefreshImages -and $LASTEXITCODE) { throw 'CPU preview comparison failed' }
    $cpuGenerated = Join-Path $root 'actual'
    if (Test-Path -LiteralPath $cpuGenerated) {
        $cpuPublished = Join-Path $root 'visual-cpu-actual'
        New-Item -ItemType Directory -Force -Path $cpuPublished | Out-Null
        Copy-Item -Path (Join-Path $cpuGenerated '*.png') -Destination $cpuPublished
        node $comparer visual-cpu-actual expected visual-cpu-diff -R cpu-visual-report.html -J cpu-visual-report.json --diffFormat png
        if ($LASTEXITCODE) { throw 'CPU visual report generation failed' }
    }
}
finally { Pop-Location }
