param([switch]$Rebaseline)
$ErrorActionPreference = 'Stop'
$baselineRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$comparer = if ($env:ATRIUM_REG_CLI) { $env:ATRIUM_REG_CLI } else { Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $baselineRoot))) 'reg-cli/dist/cli.mjs' }
if (!(Test-Path -LiteralPath $comparer)) { throw "Build reg-cli first: $comparer" }

Push-Location $baselineRoot
try {
    if ($Rebaseline) {
        dotnet run --project AtriumVisualBaseline.csproj -c Release -- expected
        if ($LASTEXITCODE) { throw 'Baseline generation failed' }
    }
    dotnet run --project AtriumVisualBaseline.csproj -c Release -- actual
    if ($LASTEXITCODE) { throw 'Current image generation failed' }
    node $comparer actual expected diff -R report.html -J report.json --junit report.xml --diffFormat png
    if ($LASTEXITCODE) { throw 'Visual comparison failed; inspect report.html' }
    Write-Host "Visual report: $(Join-Path $baselineRoot 'report.html')"
}
finally { Pop-Location }
