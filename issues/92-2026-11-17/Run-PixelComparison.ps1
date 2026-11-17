$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
python (Join-Path $root 'pixel-compare/check_pixel_parity.py') `
    (Join-Path $root 'pixel-compare/before') `
    (Join-Path $root 'pixel-compare/after')
if ($LASTEXITCODE) { throw 'Pixel comparison failed' }
