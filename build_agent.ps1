<#
.SYNOPSIS
    AI/debug-friendly non-interactive build script for the three foobar2000
    components in this repo.

.DESCRIPTION
    Unlike build.ps1 (interactive menus + Start-Process output redirection,
    which trips sandbox named-pipe restrictions), this script:
      - never prompts,
      - runs the toolchain through cmd.exe with INHERITED stdio (PowerShell's
        own pipeline, no named pipes),
      - prints MSBuild output at minimal verbosity with errors/warnings
        highlighted,
      - exits with 0 only when every selected component built cleanly.

.PARAMETER Component
    floating_clouds (default), organizing_playlists, tags, or all.

.PARAMETER Configuration / Platform
    Debug/Release, x64/Win32.

.PARAMETER VsInstallDir
    Visual Studio installation directory (default: D:\APPS\Visual Studio).

.PARAMETER Package
    Zip the built DLL into dist\<name>.fb2k-component.

.PARAMETER Clean
    Remove this component's MSBuild outputs first (strictly the component's
    own x64/Release/Debug/Win32 folders and .user files).

.EXAMPLE
    pwsh -NoProfile -File build_agent.ps1
    pwsh -NoProfile -File build_agent.ps1 -Component floating_clouds -Package
#>
[CmdletBinding()]
param(
    [ValidateSet('floating_clouds', 'organizing_playlists', 'tags', 'all')]
    [string] $Component = 'floating_clouds',

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'Win32')]
    [string] $Platform = 'x64',

    [string] $VsInstallDir = 'D:\APPS\Visual Studio',

    [switch] $Package,

    [switch] $Clean,

    [string] $MinVersion = '2.0'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$WorkspaceRoot = (Resolve-Path $PSScriptRoot).Path
Set-Location $WorkspaceRoot

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------
$vcvars = Join-Path $VsInstallDir 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "vcvarsall.bat not found at: $vcvars  (pass -VsInstallDir)"
}
$msbuild = Join-Path $VsInstallDir 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild.exe not found at: $msbuild  (pass -VsInstallDir)"
}

# ---------------------------------------------------------------------------
# Component table (all three built inline with the same toolchain)
# ---------------------------------------------------------------------------
$AllComponents = @(
    [pscustomobject]@{
        Key   = 'floating_clouds'
        Label = 'Floating Clouds'
        Dll   = 'foo_floating_clouds'
        Project = Join-Path $WorkspaceRoot 'foo_floating_clouds\foo_floating_clouds.vcxproj'
    },
    [pscustomobject]@{
        Key   = 'organizing_playlists'
        Label = 'Playlist Organizer'
        Dll   = 'foo_playlist_organizer'
        Project = Join-Path $WorkspaceRoot 'floating_clouds_organizing_playlists\foo_playlist_organizer.vcxproj'
    },
    [pscustomobject]@{
        Key   = 'tags'
        Label = 'Apple Music Tags'
        Dll   = 'foo_floating_clouds_tags'
        Project = Join-Path $WorkspaceRoot 'floating_clouds_tags\floating_clouds_tags.vcxproj'
    }
)

function Get-SelectedComponents {
    if ($Component -eq 'all') { return @($AllComponents) }
    return @($AllComponents | Where-Object { $_.Key -eq $Component })
}

$Selected = @(Get-SelectedComponents)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Write-Agent {
    param([string] $Message, [string] $Color = 'Gray')
    Write-Host $Message -ForegroundColor $Color
}

function Invoke-CleanComponent {
    param($Comp)
    $names = @('x64', 'Win32', 'Debug', 'Release', 'ipch', '.vs')
    $folder = Join-Path $WorkspaceRoot (Split-Path $Comp.Project -Parent)
    Get-ChildItem -LiteralPath $folder -Directory -Force -ErrorAction SilentlyContinue |
        Where-Object { $names -contains $_.Name } |
        ForEach-Object {
            Write-Agent "  remove $($_.FullName)" Yellow
            Remove-Item -LiteralPath $_.FullName -Recurse -Force
        }
}

function Invoke-BuildComponent {
    param($Comp)

    Write-Agent ("==== {0} ({1}) ====" -f $Comp.Label, $Comp.Dll) Cyan
    if (-not (Test-Path -LiteralPath $Comp.Project)) {
        throw "Project file not found: $($Comp.Project)"
    }

    # Run vcvarsall + msbuild in one cmd session with INHERITED stdio.
    # A temp .bat avoids PowerShell quoting hell for paths with spaces.
    $arch = if ($Platform -eq 'x64') { 'x64' } else { 'x86' }
    $bat = Join-Path $WorkspaceRoot ("logs\agent_build_{0}_{1}.cmd" -f $Comp.Key, $PID)
    New-Item -ItemType Directory -Path (Split-Path $bat) -Force | Out-Null

    # /m is deliberately omitted: parallel builds of this solution's project
    # references occasionally fail with "0 errors" (MSBuild task in
    # ResolveProjectReferences fails silently), which is useless for debugging.
    # Sequential builds are deterministic and still fast enough here.
    $msbuildArgs = '"{0}" /p:Configuration={1} /p:Platform={2} /p:VisualStudioVersion=17.0 /v:normal /nologo' `
        -f $Comp.Project, $Configuration, $Platform
    $content = '@echo off' + "`r`n" +
        'call "{0}" {1}' -f $vcvars, $arch + "`r`n" +
        'if errorlevel 1 exit /b %errorlevel%' + "`r`n" +
        '"{0}" {1}' -f $msbuild, $msbuildArgs + "`r`n" +
        'exit /b %errorlevel%'
    Set-Content -LiteralPath $bat -Value $content -Encoding ASCII

    try {
        # Streams stdout/stderr through PowerShell's own pipeline (no
        # Start-Process redirection -> no named pipes -> sandbox-safe).
        & cmd.exe /d /c ('"{0}"' -f $bat)
        $code = $LASTEXITCODE
        if ($code -ne 0) {
            throw ("MSBuild exited with code {0}. See output above." -f $code)
        }
    } finally {
        Remove-Item -LiteralPath $bat -Force -ErrorAction SilentlyContinue
    }

    Write-Agent ("Build succeeded: {0} / {1}" -f $Configuration, $Platform) Green
}

function Get-DllPath {
    param($Comp)
    $dir = Split-Path $Comp.Project -Parent
    if ($Platform -eq 'Win32') {
        return Join-Path $dir ($Configuration + '\' + $Comp.Dll + '.dll')
    }
    return Join-Path $dir ('x64\' + $Configuration + '\' + $Comp.Dll + '.dll')
}

function New-ComponentPackage {
    param($Comp)

    $dll = Get-DllPath -Comp $Comp
    if (-not (Test-Path -LiteralPath $dll)) {
        throw "Built DLL not found at: $dll"
    }
    $outDir = Join-Path $WorkspaceRoot 'dist'
    $outZip = Join-Path $outDir ($Comp.Dll + '.fb2k-component')
    $stage = Join-Path $WorkspaceRoot ("logs\agent_pkg_{0}_{1}" -f $Comp.Key, $PID)

    try {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
        New-Item -ItemType Directory -Path $stage -Force | Out-Null
        Copy-Item -LiteralPath $dll -Destination (Join-Path $stage ($Comp.Dll + '.dll')) -Force
        $manifest = Join-Path $stage ($Comp.Dll + '.fb2k-component')
        Set-Content -LiteralPath $manifest -Value ('foobar2000 v' + $MinVersion) -Encoding ASCII
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
        if (Test-Path -LiteralPath $outZip) { Remove-Item -LiteralPath $outZip -Force }
        Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
        [System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $outZip,
            [System.IO.Compression.CompressionLevel]::Optimal, $false)
        Write-Agent ("Packaged: {0} ({1:N0} bytes)" -f $outZip, (Get-Item $outZip).Length) Green
    } finally {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
Write-Host ''
Write-Host '== Floating Clouds agent build ==' -ForegroundColor Cyan
Write-Host ('  Component : {0}' -f $Component)
Write-Host ('  Config    : {0} / {1}' -f $Configuration, $Platform)
Write-Host ('  VS        : {0}' -f $VsInstallDir)
Write-Host ''

$failed = @()
foreach ($comp in $Selected) {
    try {
        if ($Clean) { Invoke-CleanComponent -Comp $comp }
        Invoke-BuildComponent -Comp $comp
        if ($Package) { New-ComponentPackage -Comp $comp }
    } catch {
        Write-Agent ("ERROR: {0}" -f $_.Exception.Message) Red
        $failed += $comp.Dll
    }
}

Write-Host ''
if ($failed.Count -eq 0) {
    Write-Agent 'ALL BUILDS SUCCEEDED' Green
    exit 0
} else {
    Write-Agent ('FAILED: {0}' -f ($failed -join ', ')) Red
    exit 1
}
