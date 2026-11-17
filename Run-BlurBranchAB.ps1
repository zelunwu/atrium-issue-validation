param(
    [int]$Rounds = 2,
    [string]$OriginalExe = 'native/build-original-blur/Release/atrium_graphics_baseline.exe',
    [string]$OptimizedExe = 'native/build/Release/atrium_graphics_baseline.exe',
    [string]$Prefix = 'ab-blur'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$old = Join-Path $root $OriginalExe
$new = Join-Path $root $OptimizedExe
if (!(Test-Path -LiteralPath $old) -or !(Test-Path -LiteralPath $new)) { throw 'Build both shader variants first' }
$previous = [Environment]::GetEnvironmentVariable('ATRIUM_BASELINE_BLUR_SETTING', 'Process')
try {
    for ($round = 1; $round -le $Rounds; ++$round) {
        $variants = if ($round % 2) { @('A','B') } else { @('B','A') }
        foreach ($blur in @('0.00','0.50')) {
            $env:ATRIUM_BASELINE_BLUR_SETTING = $blur
            foreach ($variant in $variants) {
                $exe = if ($variant -eq 'A') { $old } else { $new }
                $out = Join-Path $root "$Prefix-$blur-$variant-round$round.csv"
                & $exe --profile $out
                if ($LASTEXITCODE) { throw "$variant blur $blur failed" }
                Write-Host "Saved $out"
            }
        }
    }
}
finally {
    if ($null -eq $previous) { Remove-Item Env:ATRIUM_BASELINE_BLUR_SETTING -ErrorAction SilentlyContinue }
    else { [Environment]::SetEnvironmentVariable('ATRIUM_BASELINE_BLUR_SETTING', $previous, 'Process') }
}
