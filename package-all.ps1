<#
.SYNOPSIS
    Builds and packages foo_floating_clouds for BOTH architectures
    (x64 and Win32), saving each as a versioned .fb2k-component.

.DESCRIPTION
    - Calls .\build.ps1 -Package for Release / x64 and Release / Win32.
    - Saves each package with a platform suffix under .\dist:
        dist\foo_floating_clouds-x64.fb2k-component
        dist\foo_floating_clouds-x86.fb2k-component
    - A full rebuild of the SDK libs happens once per platform, so the first
      run is slow; later runs are incremental.

.PARAMETER MinVersion
    Minimum foobar2000 version written into each package manifest
    (passed through to build.ps1; default: 2.0).

.EXAMPLE
    .\package-all.ps1
    .\package-all.ps1 -MinVersion 1.4
#>
[CmdletBinding()]
param(
    [string] $MinVersion = '2.0'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root   = (Resolve-Path $PSScriptRoot).Path
$build  = Join-Path $root 'build.ps1'
$outDir = Join-Path $root 'dist'

if (-not (Test-Path -LiteralPath $build)) {
    throw "build.ps1 not found at: $build"
}
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

$targets = @(
    @{ Platform = 'x64';   Suffix = 'x64' },
    @{ Platform = 'Win32'; Suffix = 'x86' }
)

foreach ($t in $targets) {
    Write-Host ""
    Write-Host ("=========== Release / {0} ===========" -f $t.Platform) -ForegroundColor Cyan

    & $build -Configuration Release -Platform $t.Platform -Package -MinVersion $MinVersion
    if ($LASTEXITCODE -ne 0) { throw "build.ps1 failed for $($t.Platform)." }

    $src = Join-Path $outDir 'foo_floating_clouds.fb2k-component'
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Expected package not found: $src"
    }

    $dst = Join-Path $outDir ("foo_floating_clouds-{0}.fb2k-component" -f $t.Suffix)
    Copy-Item -LiteralPath $src -Destination $dst -Force
    Write-Host ("Saved: {0} ({1:N0} bytes)" -f $dst, (Get-Item $dst).Length) -ForegroundColor Green
}

# Remove the transient unsuffixed package left by the last build.ps1 -Package.
$transient = Join-Path $outDir 'foo_floating_clouds.fb2k-component'
Remove-Item -LiteralPath $transient -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host 'All packages ready:' -ForegroundColor Cyan
Get-ChildItem -LiteralPath $outDir -Filter '*.fb2k-component' | ForEach-Object {
    Write-Host ('  {0}  ({1:N0} bytes)' -f $_.Name, $_.Length)
}
