<#
.SYNOPSIS
    Shared build helpers: release artifact naming and SHA256 verification.

.DESCRIPTION
    Dot-source this file from build.ps1 / build_agent.ps1 / package-all.ps1.

    Release artifacts are named

        <component>-<version>-<yyyyMMdd>.fb2k-component

    e.g. foo_floating_clouds-0.2.0-20261107.fb2k-component

    where <component> is the DLL stem (the name foobar2000 expects, so it always
    starts with foo_), <version> is the string from that component's
    DECLARE_COMPONENT_VERSION, and the date is the build date. Inside the
    package the DLL and its manifest keep their canonical names - only the
    install file gets the version/date suffix, because the manifest lookup
    depends on the DLL name.

    SHA256SUMS.txt is written next to the packages in dist/ as
    "<hash>  <filename>" lines (LF, ASCII, no BOM) so it verifies with either

        sha256sum -c SHA256SUMS.txt          (Linux / Git Bash / MSYS)
        Get-FileHash ... (see Format-Sha256Report)   (PowerShell)
#>

Set-StrictMode -Version Latest

# Visual Studio install directory. Empty input = auto-detect via vswhere, with
# the historical default path as a last resort.
function Resolve-VsInstallDir {
    param([string] $VsInstallDir = '')

    if ($VsInstallDir -and (Test-Path -LiteralPath $VsInstallDir)) { return $VsInstallDir }

    $pf86 = ${env:ProgramFiles(x86)}
    if (-not $pf86) { $pf86 = $env:ProgramFiles }
    $vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products '*' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
            Select-Object -First 1
        if ($found -and (Test-Path -LiteralPath $found)) { return $found }
    }
    return 'D:\APPS\Visual Studio'
}

# PlatformToolset to build with: prefer the project's native v142 when that
# toolset is actually installed, otherwise the newest one that is.
function Get-InstalledPlatformToolset {
    param([string] $VsDir)

    $vcRoot = Join-Path $VsDir 'MSBuild\Microsoft\VC'
    $vcDirs = @(Get-ChildItem -LiteralPath $vcRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^v\d+$' } | Sort-Object Name -Descending)
    foreach ($vc in $vcDirs) {
        $ptDir = Join-Path $vc.FullName 'Platforms\x64\PlatformToolsets'
        if (-not (Test-Path -LiteralPath $ptDir)) { continue }
        $toolsets = @(Get-ChildItem -LiteralPath $ptDir -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { $_.Name } | Sort-Object)
        if ($toolsets -contains 'v142') { return 'v142' }
        if ($toolsets.Count -gt 0) { return $toolsets[-1] }
    }
    return ''
}

# Version string of a component: the second argument of
# DECLARE_COMPONENT_VERSION("Name", "x.y.z", "about") in its main.cpp.
function Get-ComponentVersion {
    param(
        [Parameter(Mandatory = $true)][string] $ComponentDir,
        [string] $Fallback = '0.0.0'
    )

    $main = Join-Path $ComponentDir 'main.cpp'
    if (-not (Test-Path -LiteralPath $main)) { return $Fallback }

    $text = Get-Content -LiteralPath $main -Raw -Encoding UTF8
    $m = [regex]::Match($text,
        'DECLARE_COMPONENT_VERSION\s*\(\s*"[^"]*"\s*,\s*"([^"]+)"',
        [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $m.Success) { return $Fallback }
    return $m.Groups[1].Value.Trim()
}

# Build date as yyyyMMdd. Pass -Date for a reproducible run.
function Get-BuildDateString {
    param([datetime] $Date = (Get-Date))
    return $Date.ToString('yyyyMMdd')
}

# <component>-<version>-<yyyyMMdd>[-<suffix>]
function Get-ArtifactStem {
    param(
        [Parameter(Mandatory = $true)][string] $ComponentName,
        [Parameter(Mandatory = $true)][string] $Version,
        [Parameter(Mandatory = $true)][string] $Date,
        [string] $Suffix = ''
    )

    $stem = '{0}-{1}-{2}' -f $ComponentName, $Version, $Date
    if ($Suffix) { $stem += '-' + $Suffix }
    return $stem
}

# Write dist/SHA256SUMS.txt over every package in -DistDir.
# Pure "<hash>  <name>" lines: no BOM, no comments, LF separators, so
# sha256sum -c accepts it as-is.
function Write-Sha256Sums {
    param(
        [Parameter(Mandatory = $true)][string] $DistDir,
        [string] $FileName = 'SHA256SUMS.txt'
    )

    if (-not (Test-Path -LiteralPath $DistDir)) { return $null }

    # Ordinal sort: the default culture-aware sort weights '-' and '_'
    # differently, which makes the order machine-dependent.
    $files = @(Get-ChildItem -LiteralPath $DistDir -Filter '*.fb2k-component' -File)
    [Array]::Sort($files, [System.Comparison[object]] {
        param($a, $b) [string]::CompareOrdinal($a.Name, $b.Name)
    })
    if ($files.Count -eq 0) { return $null }

    $lines = foreach ($f in $files) {
        $hash = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        '{0}  {1}' -f $hash, $f.Name
    }

    $out = Join-Path $DistDir $FileName
    $text = ($lines -join "`n") + "`n"
    [System.IO.File]::WriteAllText($out, $text, (New-Object System.Text.UTF8Encoding($false)))
    return $out
}

# Rename unversioned packages in -DistDir to the release naming rule and refresh
# SHA256SUMS.txt.
#
#   -Components  hashtable: component name (DLL stem) -> source folder that holds
#                its main.cpp, used to read the version.
#   -Suffix      optional platform suffix, e.g. 'x64'.
#   -Date        build date (yyyyMMdd), default today.
#
# Files that are already versioned are left alone (so re-running is safe), and
# older dated packages from previous builds stay where they are and are still
# listed in the checksum file.
function Export-ReleaseArtifacts {
    param(
        [Parameter(Mandatory = $true)][string] $DistDir,
        [Parameter(Mandatory = $true)][hashtable] $Components,
        [string] $Suffix = '',
        [string] $Date = ''
    )

    if (-not (Test-Path -LiteralPath $DistDir)) { return @() }
    if (-not $Date) { $Date = Get-BuildDateString }

    $produced = @()
    foreach ($name in ($Components.Keys | Sort-Object)) {
        $src = Join-Path $DistDir ($name + '.fb2k-component')
        if (-not (Test-Path -LiteralPath $src)) { continue }

        $version = Get-ComponentVersion -ComponentDir $Components[$name]
        $stem = Get-ArtifactStem -ComponentName $name -Version $version -Date $Date -Suffix $Suffix
        $dst = Join-Path $DistDir ($stem + '.fb2k-component')

        if ($src -ne $dst) {
            if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Force }
            Move-Item -LiteralPath $src -Destination $dst -Force
        }
        $produced += [pscustomobject]@{
            Name    = [System.IO.Path]::GetFileName($dst)
            Version = $version
            Path    = $dst
        }
    }

    $sums = Write-Sha256Sums -DistDir $DistDir
    return [pscustomobject]@{ Artifacts = $produced; SumsFile = $sums }
}

# Human-readable verification block (console output only; the file itself stays
# machine-parseable).
function Format-Sha256Report {
    param(
        [Parameter(Mandatory = $true)][string] $DistDir,
        [string] $Indent = '  '
    )

    $lines = @()
    $sums = Join-Path $DistDir 'SHA256SUMS.txt'
    if (-not (Test-Path -LiteralPath $sums)) { return $lines }

    foreach ($line in (Get-Content -LiteralPath $sums)) {
        if (-not $line.Trim()) { continue }
        $parts = $line -split '\s+', 2
        if ($parts.Count -ne 2) { continue }
        $pkg = Join-Path $DistDir $parts[1]
        $size = if (Test-Path -LiteralPath $pkg) { (Get-Item $pkg).Length } else { 0 }
        $lines += ('{0}{1}...  {2}  ({3:N0} bytes)' -f $Indent, $parts[0].Substring(0, 16), $parts[1], $size)
    }
    $lines += ('{0}verify: sha256sum -c SHA256SUMS.txt' -f $Indent)
    return $lines
}
