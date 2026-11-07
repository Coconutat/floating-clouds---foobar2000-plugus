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

# Release naming (<component>-<version>-<yyyyMMdd>) + checksums.
. (Join-Path $root 'build_common.ps1')
$componentDir = Join-Path $root 'foo_floating_clouds'
$version = Get-ComponentVersion -ComponentDir $componentDir
$date    = Get-BuildDateString

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

    # build.ps1 emits <component>-<version>-<date>.fb2k-component; the platform
    # suffix goes after the date so the name stays sortable.
    $built = Get-ArtifactStem -ComponentName 'foo_floating_clouds' -Version $version -Date $date
    $src = Join-Path $outDir ($built + '.fb2k-component')
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Expected package not found: $src"
    }

    $dst = Join-Path $outDir ((Get-ArtifactStem -ComponentName 'foo_floating_clouds' -Version $version -Date $date -Suffix $t.Suffix) + '.fb2k-component')
    Copy-Item -LiteralPath $src -Destination $dst -Force
    Write-Host ("Saved: {0} ({1:N0} bytes)" -f $dst, (Get-Item $dst).Length) -ForegroundColor Green
}

# Drop the unsuffixed intermediate once both platform copies exist, then write
# the checksum file over the real release artifacts.
$base = Get-ArtifactStem -ComponentName 'foo_floating_clouds' -Version $version -Date $date
$intermediate = Join-Path $outDir ($base + '.fb2k-component')
$x64 = Join-Path $outDir ((Get-ArtifactStem -ComponentName 'foo_floating_clouds' -Version $version -Date $date -Suffix 'x64') + '.fb2k-component')
$x86 = Join-Path $outDir ((Get-ArtifactStem -ComponentName 'foo_floating_clouds' -Version $version -Date $date -Suffix 'x86') + '.fb2k-component')
if ((Test-Path -LiteralPath $intermediate) -and (Test-Path -LiteralPath $x64) -and (Test-Path -LiteralPath $x86)) {
    Remove-Item -LiteralPath $intermediate -Force -ErrorAction SilentlyContinue
}

$sums = Write-Sha256Sums -DistDir $outDir
Write-Host ""
if ($sums) { Write-Host ("Checksums: {0}" -f $sums) -ForegroundColor Cyan }
Write-Host 'All packages ready:' -ForegroundColor Cyan
Get-ChildItem -LiteralPath $outDir -Filter '*.fb2k-component' | ForEach-Object {
    Write-Host ('  {0}  ({1:N0} bytes)' -f $_.Name, $_.Length)
}
