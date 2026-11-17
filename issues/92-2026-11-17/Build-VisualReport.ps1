$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$comparer = if ($env:ATRIUM_REG_CLI) { $env:ATRIUM_REG_CLI } else { Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $root))) 'reg-cli/dist/cli.mjs' }
if (!(Test-Path -LiteralPath $comparer)) { throw "Build reg-cli first: $comparer" }
Push-Location (Join-Path $root 'pixel-compare')
try {
    python check_pixel_parity.py before after
    if ($LASTEXITCODE) { throw 'Exact pixel comparison failed' }
    node $comparer after before diff -R visual-report.html -J visual-report.json --diffFormat png
    if ($LASTEXITCODE) { throw 'Visual report generation failed' }
}
finally { Pop-Location }
