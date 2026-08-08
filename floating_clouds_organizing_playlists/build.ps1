<#
.SYNOPSIS
    Builds the foo_playlist_organizer foobar2000 component (Playlist Organizer).

.DESCRIPTION
    - Defaults to Release / x64.
    - Resolves vcvarsall.bat and MSBuild from a Visual Studio install (default
      "D:\APPS\Visual Studio"), then builds this component's project.
    - -Package builds an installable foo_playlist_organizer.fb2k-component
      under .\dist.
    - -Deploy copies the DLL into a foobar2000 components folder
      (requires -Foobar2000Dir).
    - Safety: refuses to run as admin, refuses to operate at a drive root.

.PARAMETER Configuration
    Debug or Release (default: Release).

.PARAMETER Platform
    x64 or Win32 (default: x64).

.PARAMETER VsInstallDir
    Visual Studio installation directory (default: D:\APPS\Visual Studio).

.PARAMETER Deploy
    Copy the freshly built DLL into a foobar2000 "components" folder
    (requires -Foobar2000Dir).

.PARAMETER Foobar2000Dir
    Root folder of a foobar2000 installation (only used with -Deploy).

.PARAMETER Package
    After a successful build, package the DLL into an installable
    foo_playlist_organizer.fb2k-component file under .\dist.

.PARAMETER MinVersion
    Minimum foobar2000 version written into the package manifest
    (default: 2.0).

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Configuration Debug -Platform x64
    .\build.ps1 -Package
    .\build.ps1 -Deploy -Foobar2000Dir "D:\foobar2000"
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Low')]
param(
    [ValidateSet('Debug', 'Release')] [string] $Configuration = 'Release',
    [ValidateSet('x64', 'Win32')]      [string] $Platform     = 'x64',
    [string] $VsInstallDir = 'D:\APPS\Visual Studio',
    [switch] $Deploy,
    [string] $Foobar2000Dir = '',
    [switch] $Package,
    [string] $MinVersion = '2.0'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Refuse to run elevated: not needed for building.
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isAdmin) {
    Write-Warning 'Running elevated. Not required; consider running as a normal user.'
}

# Workspace root = the component folder containing this script.
$WorkspaceRoot = (Resolve-Path $PSScriptRoot).Path
Set-Location $WorkspaceRoot

$rootItem = Get-Item $WorkspaceRoot
if ($null -eq $rootItem.Parent) {
    throw "Refusing to operate at a drive root: $WorkspaceRoot"
}

# Resolve vcvarsall.bat + MSBuild.
$vcvars = Join-Path $VsInstallDir 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "vcvarsall.bat not found at: $vcvars`n  Install VS Build Tools with the C++ workload, or pass -VsInstallDir."
}
$msbuild = Join-Path $VsInstallDir 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    $found = Get-Command msbuild -ErrorAction SilentlyContinue
    $msbuild = if ($found) { $found.Source } else { '' }
}
if ([string]::IsNullOrWhiteSpace($msbuild) -or -not (Test-Path -LiteralPath $msbuild)) {
    throw 'MSBuild.exe not found. Install VS Build Tools or pass -VsInstallDir.'
}

$ProjectFile = Join-Path $WorkspaceRoot 'foo_playlist_organizer.vcxproj'
if (-not (Test-Path -LiteralPath $ProjectFile)) {
    throw "Project file not found: $ProjectFile"
}

$arch = if ($Platform -eq 'x64') { 'x64' } else { 'x86' }
$componentName = 'foo_playlist_organizer'

Write-Host "== Playlist Organizer build ==" -ForegroundColor Cyan
Write-Host "  Workspace : $WorkspaceRoot"
Write-Host "  Config    : $Configuration / $Platform"
Write-Host "  VS        : $VsInstallDir"
Write-Host "  MSBuild   : $msbuild"
Write-Host ""

# Build via a temporary .bat (quoted paths with spaces survive cmd.exe this way).
$bat = Join-Path $env:TEMP "build_$componentName`_$PID.bat"
try {
    $lineVcvars = 'call "{0}" {1}' -f $vcvars, $arch
    $lineMsbuild = '"{0}" "{1}" /p:Configuration={2} /p:Platform={3} /p:VisualStudioVersion=17.0 /m' -f $msbuild, $ProjectFile, $Configuration, $Platform
    $content = '@echo off' + "`r`n" + $lineVcvars + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + $lineMsbuild + "`r`n" + 'exit /b %errorlevel%'
    Set-Content -LiteralPath $bat -Value $content -Encoding ASCII
    & cmd.exe /d /c "`"$bat`""
    if ($LASTEXITCODE -ne 0) {
        throw "Build FAILED (exit code $LASTEXITCODE)."
    }
} finally {
    Remove-Item -LiteralPath $bat -Force -ErrorAction SilentlyContinue
}

Write-Host "Build succeeded: $Configuration / $Platform" -ForegroundColor Green

$dll = if ($Platform -eq 'Win32') {
    Join-Path $WorkspaceRoot "$Configuration\$componentName.dll"
} else {
    Join-Path $WorkspaceRoot "x64\$Configuration\$componentName.dll"
}
if (Test-Path -LiteralPath $dll) {
    $len = (Get-Item $dll).Length
    Write-Host ('  Output: {0} ({1:N0} bytes)' -f $dll, $len) -ForegroundColor Green
} else {
    throw "Built DLL not found at: $dll"
}

if ($Deploy) {
    if ([string]::IsNullOrWhiteSpace($Foobar2000Dir)) {
        throw '-Deploy requires -Foobar2000Dir "<foobar2000 root>".'
    }
    $dest = Join-Path $Foobar2000Dir 'components'
    if (-not (Test-Path -LiteralPath $dest)) { New-Item -ItemType Directory -Path $dest | Out-Null }
    Copy-Item -LiteralPath $dll -Destination (Join-Path $dest "$componentName.dll") -Force
    Write-Host "Deployed: $dest\$componentName.dll" -ForegroundColor Green
}

if ($Package) {
    $outDir = Join-Path $WorkspaceRoot 'dist'
    $outZip = Join-Path $outDir "$componentName.fb2k-component"
    $stage  = Join-Path $env:TEMP "fb2k_pkg_$PID"
    try {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
        New-Item -ItemType Directory -Path $stage | Out-Null
        Copy-Item -LiteralPath $dll -Destination (Join-Path $stage "$componentName.dll") -Force
        $manifest = Join-Path $stage "$componentName.fb2k-component"
        Set-Content -LiteralPath $manifest -Value ('foobar2000 v' + $MinVersion) -Encoding ASCII
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
        if (Test-Path -LiteralPath $outZip) { Remove-Item -LiteralPath $outZip -Force }
        Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
        [System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $outZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)
        Write-Host "Packaged: $outZip" -ForegroundColor Green
    } finally {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    }
}
