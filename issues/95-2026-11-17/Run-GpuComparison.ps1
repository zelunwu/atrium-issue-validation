param([switch]$Rebaseline)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $root 'native'
$build = Join-Path $source 'build'
$exe = Join-Path $build 'Release/atrium_graphics_baseline.exe'
$comparer = if ($env:ATRIUM_REG_CLI) { $env:ATRIUM_REG_CLI } else { Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $root))) 'reg-cli/dist/cli.mjs' }
function Assert-NumberedInventory($files, $label) {
    if ($files.Count -ne 147) { throw "$label image count is $($files.Count), expected 147" }
    foreach ($number in 0..146) {
        $prefix = '{0:D3}_' -f $number
        if (@($files | Where-Object { $_.Name.StartsWith($prefix) }).Count -ne 1) {
            throw "$label image $prefix is missing or duplicated"
        }
    }
}

Push-Location $root
try {
    if (!(Test-Path -LiteralPath $comparer)) { throw "Build reg-cli first: $comparer" }
    cmake -S $source -B $build -A x64
    if ($LASTEXITCODE) { throw 'CMake configuration failed' }
    cmake --build $build --config Release --parallel
    if ($LASTEXITCODE) { throw 'Native baseline build failed' }
    if ($Rebaseline) {
        & $exe (Join-Path $root 'gpu-expected')
        if ($LASTEXITCODE) { throw 'GPU reference generation failed' }
    }
    $expected = @(Get-ChildItem -LiteralPath (Join-Path $root 'gpu-expected') -Filter '*.png' -File)
    Assert-NumberedInventory $expected 'Reference'
    if ($expected.Count -ne 147 -or !(Test-Path -LiteralPath (Join-Path $root 'gpu-expected/146_glass_renderer_complex_parts.png'))) {
        throw "GPU references are missing or outdated ($($expected.Count)/147); review and run with -Rebaseline once"
    }
    $runStarted = (Get-Date).ToUniversalTime()
    & $exe (Join-Path $root 'gpu-actual')
    if ($LASTEXITCODE) { throw 'GPU image generation failed' }
    $actual = @(Get-ChildItem -LiteralPath (Join-Path $root 'gpu-actual') -Filter '*.png' -File)
    Assert-NumberedInventory $actual 'Generated'
    $expectedNames = @($expected.Name | Sort-Object)
    $actualNames = @($actual.Name | Sort-Object)
    if ($actual.Count -ne 147 -or (Compare-Object $expectedNames $actualNames)) {
        throw "Image inventory mismatch: expected=$($expected.Count), actual=$($actual.Count)"
    }
    $stale = @($actual | Where-Object { $_.LastWriteTimeUtc -lt $runStarted })
    if ($stale.Count) { throw "Images not regenerated: $($stale.Name -join ', ')" }
    node $comparer gpu-actual gpu-expected gpu-diff -R gpu-report.html -J gpu-report.json --junit gpu-report.xml --diffFormat png
    if ($LASTEXITCODE) { throw 'GPU visual comparison failed; inspect gpu-report.html' }
    Write-Host "GPU visual report: $(Join-Path $root 'gpu-report.html')"
}
finally { Pop-Location }
