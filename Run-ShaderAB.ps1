param([int]$Rounds = 2)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $root 'native/build/Release/atrium_graphics_baseline.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "Build the native baseline executable first: $exe" }
$oldBasic = [Environment]::GetEnvironmentVariable('ATRIUM_BASELINE_DISABLE_BASIC', 'Process')
try {
    for ($round = 1; $round -le $Rounds; ++$round) {
        $order = if ($round % 2) { @('A','B') } else { @('B','A') }
        foreach ($variant in $order) {
            if ($variant -eq 'A') { [Environment]::SetEnvironmentVariable('ATRIUM_BASELINE_DISABLE_BASIC', '1', 'Process') }
            else { Remove-Item Env:ATRIUM_BASELINE_DISABLE_BASIC -ErrorAction SilentlyContinue }
            $output = Join-Path $root "ab-glass-$variant-round$round.csv"
            & $exe --profile $output
            if ($LASTEXITCODE) { throw "Shader variant $variant round $round failed" }
            Write-Host "Saved $output"
        }
    }
}
finally {
    if ($null -eq $oldBasic) { Remove-Item Env:ATRIUM_BASELINE_DISABLE_BASIC -ErrorAction SilentlyContinue }
    else { [Environment]::SetEnvironmentVariable('ATRIUM_BASELINE_DISABLE_BASIC', $oldBasic, 'Process') }
}
