param([int]$Rounds = 2, [switch]$NoBlurControl)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $root $(if ($NoBlurControl) { 'native/build-no-blur/Release/atrium_graphics_baseline.exe' } else { 'native/build/Release/atrium_graphics_baseline.exe' })
if (!(Test-Path -LiteralPath $exe)) { throw "Build the native baseline executable first: $exe" }
$old = [Environment]::GetEnvironmentVariable('ATRIUM_BASELINE_BLUR_SETTING', 'Process')
try {
    for ($round = 1; $round -le $Rounds; ++$round) {
        $values = if ($NoBlurControl) {
            if ($round % 2) { @(0,0.5) } else { @(0.5,0) }
        } else {
            if ($round % 2) { @(0,0.25,0.5,1) } else { @(1,0.5,0.25,0) }
        }
        foreach ($value in $values) {
            $label = $value.ToString('0.00',[Globalization.CultureInfo]::InvariantCulture)
            $env:ATRIUM_BASELINE_BLUR_SETTING = $label
            $prefix = if ($NoBlurControl) { 'blur-control' } else { 'blur' }
            $path = Join-Path $root "$prefix-$label-round$round.csv"
            & $exe --profile $path
            if ($LASTEXITCODE) { throw "Blur setting $label round $round failed" }
            Write-Host "Saved $path"
        }
    }
}
finally {
    if ($null -eq $old) { Remove-Item Env:ATRIUM_BASELINE_BLUR_SETTING -ErrorAction SilentlyContinue }
    else { [Environment]::SetEnvironmentVariable('ATRIUM_BASELINE_BLUR_SETTING',$old,'Process') }
}
