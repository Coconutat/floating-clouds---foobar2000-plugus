<#
.SYNOPSIS
    Builds all three foobar2000 components (Floating Clouds, Playlist
    Organizer, Apple Music Tags) in one run.

.DESCRIPTION
    - Defaults to Release / x64.
    - Builds the Floating Clouds component inline and invokes the Playlist
      Organizer / Apple Music Tags build.ps1 scripts, all with the same
      configuration / platform / VS instance.
    - -Package collects every component's .fb2k-component into .\dist.
    - Per-component build output is written to logs\<component>-<timestamp>.log.
    - Running without action switches starts an interactive menu (language,
      build form, component) with Chinese/English prompts (-Language selects).
    - -Clean / -CleanOnly remove generated build outputs, strictly scoped to
      this workspace. Everything deleted is printed first; use -WhatIf to preview.
    - Safety: refuses to run as admin, refuses to operate at a drive root, and
      never touches anything outside the workspace.

.PARAMETER Configuration
    Debug or Release (default: Release).

.PARAMETER Platform
    x64 or Win32 (default: x64).

.PARAMETER VsInstallDir
    Visual Studio installation directory (default: D:\APPS\Visual Studio).

.PARAMETER Clean
    Clean build outputs first, then build.

.PARAMETER CleanOnly
    Only clean build outputs, do not build.

.PARAMETER Force
    Skip the interactive confirmation prompt when cleaning.

.PARAMETER Deploy
    Copy the freshly built DLL into a foobar2000 "components" folder
    (requires -Foobar2000Dir).

.PARAMETER Foobar2000Dir
    Root folder of a foobar2000 installation (only used with -Deploy).

.PARAMETER Package
    After a successful build, package the DLL into an installable
    foo_floating_clouds.fb2k-component file under .\dist.

.PARAMETER MinVersion
    Minimum foobar2000 version written into the package manifest
    (default: 2.0).

.PARAMETER Language
    Console prompt language: en or zh (default: en). Interactive prompts are
    shown in the selected language when running without action switches.

.PARAMETER Component
    Which component(s) to build: all, floating_clouds, organizing_playlists,
    or tags (default: all).

.PARAMETER Interactive
    Force the interactive menu even when action switches are present.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Language zh
    .\build.ps1 -Package
    .\build.ps1 -Package -Component tags
    .\build.ps1 -Deploy -Foobar2000Dir "D:\foobar2000"
    .\build.ps1 -Clean -Force
    .\build.ps1 -Configuration Debug -Platform x64
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Low')]
param(
    [ValidateSet('Debug', 'Release')] [string] $Configuration = 'Release',
    [ValidateSet('x64', 'Win32')]      [string] $Platform     = 'x64',
    [string] $VsInstallDir = 'D:\APPS\Visual Studio',
    [switch] $Clean,
    [switch] $CleanOnly,
    [switch] $Force,
    [switch] $Deploy,
    [string] $Foobar2000Dir = '',
    [switch] $Package,
    [string] $MinVersion = '2.0',
    [ValidateSet('en', 'zh')] [string] $Language = 'en',
    [ValidateSet('all', 'floating_clouds', 'organizing_playlists', 'tags')] [string] $Component = 'all',
    [switch] $Interactive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# 0. Preflight / safety
# ---------------------------------------------------------------------------

# Refuse to run elevated: not needed for building, and cleaning while elevated
# is riskier than helpful.
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isAdmin) {
    Write-Warning 'Running elevated. Not required; consider running as a normal user.'
}

# Workspace root = folder containing this script.
$WorkspaceRoot = (Resolve-Path $PSScriptRoot).Path
Set-Location $WorkspaceRoot

# Never operate at a drive root (e.g. C:\) — a safety net against catastrophic cleans.
$rootItem = Get-Item $WorkspaceRoot
if ($null -eq $rootItem.Parent) {
    throw "Refusing to operate at a drive root: $WorkspaceRoot"
}

# Resolve vcvarsall.bat from the chosen VS install.
$vcvars = Join-Path $VsInstallDir 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "vcvarsall.bat not found at: $vcvars`n  Install VS Build Tools with the C++ workload, or pass -VsInstallDir."
}

# Resolve MSBuild (prefer the one inside the same VS install, else PATH).
$msbuild = Join-Path $VsInstallDir 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    $found = Get-Command msbuild -ErrorAction SilentlyContinue
    $msbuild = if ($found) { $found.Source } else { '' }
}
if ([string]::IsNullOrWhiteSpace($msbuild) -or -not (Test-Path -LiteralPath $msbuild)) {
    throw 'MSBuild.exe not found. Install VS Build Tools or pass -VsInstallDir.'
}

$arch = if ($Platform -eq 'x64') { 'x64' } else { 'x86' }

# ---------------------------------------------------------------------------
# Component table
# ---------------------------------------------------------------------------
# Each entry describes one component. floating_clouds is built inline (this
# script IS its build script); the other two are built by invoking their own
# build.ps1 with the same configuration / platform / VS instance.
$AllComponents = @(
    [pscustomobject]@{
        Key           = 'floating_clouds'
        Label         = 'Floating Clouds'
        ComponentName = 'foo_floating_clouds'
        Folder        = 'foo_floating_clouds'
        ProjectFile   = Join-Path $WorkspaceRoot 'foo_floating_clouds\foo_floating_clouds.vcxproj'
        Inline        = $true
    },
    [pscustomobject]@{
        Key           = 'organizing_playlists'
        Label         = 'Playlist Organizer'
        ComponentName = 'foo_playlist_organizer'
        Folder        = 'floating_clouds_organizing_playlists'
        Script        = Join-Path $WorkspaceRoot 'floating_clouds_organizing_playlists\build.ps1'
        Inline        = $false
    },
    [pscustomobject]@{
        Key           = 'tags'
        Label         = 'Apple Music Tags'
        ComponentName = 'foo_floating_clouds_tags'
        Folder        = 'floating_clouds_tags'
        Script        = Join-Path $WorkspaceRoot 'floating_clouds_tags\build.ps1'
        Inline        = $false
    }
)

function Get-SelectedComponents {
    if ($Component -eq 'all') { return @($AllComponents) }
    $sel = @($AllComponents | Where-Object { $_.Key -eq $Component })
    if ($sel.Count -eq 0) { throw "Unknown component: $Component" }
    return $sel
}

# ---------------------------------------------------------------------------
# Logging: Write-Log mirrors to the console AND appends to the current
# component's log file (set via $script:CurrentLogFile).
# ---------------------------------------------------------------------------
$script:CurrentLogFile = $null

function Write-Log {
    param(
        [string] $Message,
        [ConsoleColor] $ForegroundColor = [ConsoleColor]::Gray
    )
    Write-Host $Message -ForegroundColor $ForegroundColor
    if ($script:CurrentLogFile) {
        Add-Content -LiteralPath $script:CurrentLogFile -Value $Message -Encoding utf8
    }
}

# Read a captured output file as text. MSBuild emits UTF-8 on this machine
# (console CP 65001); fall back to the system ANSI codepage (GBK) when strict
# UTF-8 decoding fails, so Chinese lines survive either way.
function Read-DecodedLines {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) { return @() }
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -eq 0) { return @() }
    $strict = New-Object System.Text.UTF8Encoding -ArgumentList $false, $true
    try {
        $text = $strict.GetString($bytes)
    } catch {
        $text = [System.Text.Encoding]::GetEncoding(936).GetString($bytes)
    }
    return @($text -split "`r?`n")
}

# ---------------------------------------------------------------------------
# Bilingual UI text
# ---------------------------------------------------------------------------

function New-TextTable {
    param([string] $Language)
    if ($Language -eq 'zh') {
        return @{
            banner       = '== Floating Clouds 三组件统一构建 =='
            lang_title   = '请选择提示语言 / Select prompt language:'
            lang_1       = '  1. English'
            lang_2       = '  2. 中文'
            lang_prompt  = '输入 1/2: '
            form_title   = '请选择构建形式：'
            form_1       = '  1. 仅构建 DLL'
            form_2       = '  2. 打包 .fb2k-component'
            form_3       = '  3. 部署到 foobar2000（需 foobar2000 目录）'
            form_4       = '  4. 仅清理'
            form_5       = '  5. 全部（清理 + 构建 + 打包 + 部署）'
            form_prompt  = '输入 1-5: '
            form_invalid = '无效的选择: {0}'
            deploy_ask   = 'foobar2000 安装目录（用于部署）: '
            deploy_skip  = '未提供 -Foobar2000Dir，已跳过部署。'
            comp_title   = '请选择要构建的组件：'
            comp_1       = '  1. 全部 3 个'
            comp_2       = '  2. Floating Clouds'
            comp_3       = '  3. Playlist Organizer'
            comp_4       = '  4. Apple Music Tags'
            comp_prompt  = '输入 1-4: '
            comp_invalid = '无效的选择: {0}'
            usage_hint   = '未指定操作。不带参数运行可进入交互菜单；或用 -Package / -Deploy / -Clean / -CleanOnly。'
            summary      = '构建汇总：{0} 成功，{1} 失败。'
            failed_list  = '失败组件：{0}'
            packages     = '根 dist 下的打包文件：'
            logs         = 'logs 目录下的本次构建日志：'
        }
    }
    return @{
        banner       = '== Floating Clouds unified build =='
        lang_title   = 'Select prompt language / 请选择提示语言:'
        lang_1       = '  1. English'
        lang_2       = '  2. 中文'
        lang_prompt  = 'Enter 1/2: '
        form_title   = 'Select build form:'
        form_1       = '  1. Build DLL only'
        form_2       = '  2. Package .fb2k-component'
        form_3       = '  3. Deploy to foobar2000 (requires foobar2000 dir)'
        form_4       = '  4. Clean only'
        form_5       = '  5. All (clean + build + package + deploy)'
        form_prompt  = 'Enter 1-5: '
        form_invalid = 'Invalid choice: {0}'
        deploy_ask   = 'foobar2000 install directory (for deploy): '
        deploy_skip  = 'No -Foobar2000Dir provided; deploy skipped.'
        comp_title   = 'Select component(s) to build:'
        comp_1       = '  1. All three'
        comp_2       = '  2. Floating Clouds'
        comp_3       = '  3. Playlist Organizer'
        comp_4       = '  4. Apple Music Tags'
        comp_prompt  = 'Enter 1-4: '
        comp_invalid = 'Invalid choice: {0}'
        usage_hint   = 'No action specified. Run without arguments for the interactive menu, or use -Package / -Deploy / -Clean / -CleanOnly.'
        summary      = 'Build summary: {0} succeeded, {1} failed.'
        failed_list  = 'Failed components: {0}'
        packages     = 'Packages in root dist:'
        logs         = 'Build logs in logs/:'
    }
}

$T = New-TextTable -Language $Language

# ---------------------------------------------------------------------------
# 1. Clean
# ---------------------------------------------------------------------------

function Get-BuildOutputDirs {
    param(
        [string] $Root,
        [int]    $MaxDepth = 5
    )
    # Build-output directory names generated by MSBuild for this repo.
    $names = @('x64', 'Win32', 'Debug', 'Release', 'ipch', '.vs')
    $results = [System.Collections.Generic.List[string]]::new()
    $queue = [System.Collections.Generic.Queue[object]]::new()
    $queue.Enqueue([pscustomobject]@{ Path = $Root; Depth = 0 })

    while ($queue.Count -gt 0) {
        $node = $queue.Dequeue()
        if ($node.Depth -gt $MaxDepth) { continue }
        foreach ($d in Get-ChildItem -LiteralPath $node.Path -Directory -Force -ErrorAction SilentlyContinue) {
            if ($d.Name -eq '.git') { continue }            # never touch git metadata
            if ($names -contains $d.Name) {
                $results.Add($d.FullName)                   # collect, do not descend
            } elseif ($node.Depth -lt $MaxDepth) {
                $queue.Enqueue([pscustomobject]@{ Path = $d.FullName; Depth = $node.Depth + 1 })
            }
        }
    }
    return $results
}

function Invoke-Clean {
    # Directory names that can only ever be build outputs (see $names above).
    $dirs = @(Get-BuildOutputDirs -Root $WorkspaceRoot)
    # VS-generated per-user files and stray build logs.
    $userFiles = @(Get-ChildItem -LiteralPath $WorkspaceRoot -Depth 3 -Filter '*.user' -File -Force -ErrorAction SilentlyContinue)
    $logs      = @(Get-ChildItem -LiteralPath $WorkspaceRoot -Depth 1 -Filter 'build_*.txt' -File -Force -ErrorAction SilentlyContinue)

    if ($dirs.Count -eq 0 -and $userFiles.Count -eq 0 -and $logs.Count -eq 0) {
        Write-Host 'Nothing to clean.' -ForegroundColor Yellow
        return
    }

    $summary = 'Will remove {0} build output folder(s), {1} .user file(s), {2} log file(s):' -f `
        $dirs.Count, $userFiles.Count, $logs.Count
    Write-Host $summary -ForegroundColor Cyan
    $dirs  | ForEach-Object { Write-Host ('  [D] ' + $_) }
    $userFiles | ForEach-Object { Write-Host ('  [F] ' + $_.FullName) }
    $logs  | ForEach-Object { Write-Host ('  [F] ' + $_.FullName) }

    # Interactive confirmation unless -Force (or -WhatIf, which is non-destructive).
    if (-not $Force -and -not $WhatIfPreference -and [Environment]::UserInteractive) {
        $ans = Read-Host 'Proceed with the cleanup above? [y/N]'
        if ($ans -notmatch '^[yY]') { Write-Host 'Clean cancelled.' -ForegroundColor Yellow; return }
    }

    foreach ($d in $dirs) {
        if ($PSCmdlet.ShouldProcess($d, 'Remove directory')) {
            Remove-Item -LiteralPath $d -Recurse -Force -ErrorAction Stop
        }
    }
    foreach ($f in $userFiles + $logs) {
        if ($PSCmdlet.ShouldProcess($f.FullName, 'Remove file')) {
            Remove-Item -LiteralPath $f.FullName -Force -ErrorAction Stop
        }
    }
    Write-Host 'Clean finished.' -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# 2. Build
# ---------------------------------------------------------------------------

function Invoke-BuildComponent {
    param([object] $Component)

    $projectFile   = $Component.ProjectFile
    $componentName = $Component.ComponentName
    if (-not (Test-Path -LiteralPath $projectFile)) {
        throw "Project file not found: $projectFile"
    }

    $cmdLine = '"{0}" {1} && "{2}" "{3}" /p:Configuration={4} /p:Platform={5} /p:VisualStudioVersion=17.0 /m' `
        -f $vcvars, $arch, $msbuild, $projectFile, $Configuration, $Platform
    Write-Log ">> $cmdLine" Cyan

    if ($WhatIfPreference) { return }   # -WhatIf: just show, don't run

    # Run via a temporary .bat file. Passing a command with quoted paths
    # (spaces!) straight to `cmd.exe /c` mangles the quoting, so we avoid it.
    $bat = Join-Path $env:TEMP "build_${componentName}_$PID.bat"
    try {
        $lineVcvars = 'call "{0}" {1}' -f $vcvars, $arch
        $lineMsbuild = '"{0}" "{1}" /p:Configuration={2} /p:Platform={3} /p:VisualStudioVersion=17.0 /m' -f $msbuild, $projectFile, $Configuration, $Platform
        $content = '@echo off' + "`r`n" + $lineVcvars + "`r`n" + 'if errorlevel 1 exit /b %errorlevel%' + "`r`n" + $lineMsbuild + "`r`n" + 'exit /b %errorlevel%'
        Set-Content -LiteralPath $bat -Value $content -Encoding ASCII
        # Capture cmd.exe's raw bytes to temp files (bypasses PowerShell 5.1's
        # fixed-at-startup native-output decode), then smart-decode to UTF-8.
        $outTmp = Join-Path $env:TEMP "${componentName}_out_$PID.txt"
        $errTmp = Join-Path $env:TEMP "${componentName}_err_$PID.txt"
        $p = Start-Process -FilePath 'cmd.exe' -ArgumentList '/d', '/c', ('"{0}"' -f $bat) `
            -RedirectStandardOutput $outTmp -RedirectStandardError $errTmp -NoNewWindow -PassThru -Wait
        foreach ($line in (Read-DecodedLines $outTmp)) {
            Write-Host $line
            if ($script:CurrentLogFile -and $line.Length -gt 0) { Add-Content -LiteralPath $script:CurrentLogFile -Value $line -Encoding utf8 }
        }
        foreach ($line in (Read-DecodedLines $errTmp)) {
            Write-Host $line
            if ($script:CurrentLogFile -and $line.Length -gt 0) { Add-Content -LiteralPath $script:CurrentLogFile -Value $line -Encoding utf8 }
        }
        Remove-Item -LiteralPath $outTmp, $errTmp -Force -ErrorAction SilentlyContinue
        if ($p.ExitCode -ne 0) {
            throw "Build FAILED (exit code $($p.ExitCode))."
        }
    } finally {
        Remove-Item -LiteralPath $bat -Force -ErrorAction SilentlyContinue
    }

    Write-Log ("Build succeeded: {0} / {1}" -f $Configuration, $Platform) Green

    $dll = if ($Platform -eq 'Win32') {
        Join-Path $WorkspaceRoot ($Component.Folder + '\' + $Configuration + '\' + $componentName + '.dll')
    } else {
        Join-Path $WorkspaceRoot ($Component.Folder + '\x64\' + $Configuration + '\' + $componentName + '.dll')
    }
    if (Test-Path -LiteralPath $dll) {
        $len = (Get-Item $dll).Length
        Write-Log ('  Output: {0} ({1:N0} bytes)' -f $dll, $len) Green
    } else {
        throw "Built DLL not found at: $dll"
    }

    if ($Deploy) { Invoke-Deploy -DllPath $dll -ComponentName $componentName }
    if ($Package) { New-ComponentPackage -DllPath $dll -ComponentName $componentName }
}

# Invoke a component's own build.ps1 in a child process, teeing its output to
# the console AND to the component's log file. Throws on failure.
function Invoke-ComponentScript {
    param([object] $Component, [string] $LogFile)

    $scriptPath = $Component.Script
    if (-not (Test-Path -LiteralPath $scriptPath)) {
        throw "build script not found: $scriptPath"
    }

    $shell = if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh' } else { 'powershell' }
    $cmdParts = @(
        $shell, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"{0}"' -f $scriptPath),
        '-Configuration', $Configuration,
        '-Platform', $Platform,
        '-VsInstallDir', ('"{0}"' -f $VsInstallDir),
        '-MinVersion', $MinVersion
    )
    if ($Package) { $cmdParts += '-Package' }
    if ($Deploy -and -not [string]::IsNullOrWhiteSpace($Foobar2000Dir)) {
        $cmdParts += '-Deploy'; $cmdParts += '-Foobar2000Dir'; $cmdParts += ('"{0}"' -f $Foobar2000Dir)
    }
    $cmdLine = $cmdParts -join ' '

    if ($WhatIfPreference) {
        Write-Log ("Would run: {0}" -f $cmdLine) Cyan
        return
    }

    Write-Log ("==== {0} ====" -f $Component.ComponentName) Cyan

    # Raw capture to temp files (bypasses PowerShell 5.1's fixed-at-startup
    # native-output decode) + smart UTF-8 decode keeps Chinese MSBuild output clean.
    $outTmp = Join-Path $env:TEMP ("{0}_out_{1}.txt" -f $Component.ComponentName, $PID)
    $errTmp = Join-Path $env:TEMP ("{0}_err_{1}.txt" -f $Component.ComponentName, $PID)
    $p = Start-Process -FilePath 'cmd.exe' -ArgumentList '/d', '/c', $cmdLine `
        -RedirectStandardOutput $outTmp -RedirectStandardError $errTmp -NoNewWindow -PassThru -Wait
    try {
        foreach ($line in (Read-DecodedLines $outTmp)) {
            Write-Host $line
            if ($script:CurrentLogFile -and $line.Length -gt 0) { Add-Content -LiteralPath $script:CurrentLogFile -Value $line -Encoding utf8 }
        }
        foreach ($line in (Read-DecodedLines $errTmp)) {
            Write-Host $line
            if ($script:CurrentLogFile -and $line.Length -gt 0) { Add-Content -LiteralPath $script:CurrentLogFile -Value $line -Encoding utf8 }
        }
        if ($p.ExitCode -ne 0) {
            throw ("{0} build failed (exit code {1})." -f $Component.ComponentName, $p.ExitCode)
        }
    } finally {
        Remove-Item -LiteralPath $outTmp, $errTmp -Force -ErrorAction SilentlyContinue
    }
}

# Copy a component's packaged .fb2k-component (produced in its own dist/) into
# the root dist/ release folder. Only meaningful for non-inline components.
function Copy-PackageToRootDist {
    param([object] $Component)

    $pkg = Join-Path $WorkspaceRoot ($Component.Folder + '\dist\' + $Component.ComponentName + '.fb2k-component')
    if (-not (Test-Path -LiteralPath $pkg)) {
        Write-Log ('Package not found (skipped collect): {0}' -f $pkg) Yellow
        return
    }
    $dstDir = Join-Path $WorkspaceRoot 'dist'
    New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    $dst = Join-Path $dstDir ($Component.ComponentName + '.fb2k-component')
    if ($WhatIfPreference) {
        Write-Log ("Would collect: {0} -> {1}" -f $pkg, $dst) Cyan
        return
    }
    Copy-Item -LiteralPath $pkg -Destination $dst -Force
    Write-Log ("Collected: {0}" -f $dst) Green
}

# ---------------------------------------------------------------------------
# 3.5 Package (optional) - build a .fb2k-component install file
# ---------------------------------------------------------------------------

function New-ComponentPackage {
    param([string] $DllPath, [string] $ComponentName)

    if (-not (Test-Path -LiteralPath $DllPath)) {
        throw "Built DLL not found at: $DllPath"
    }

    $componentName = $ComponentName
    $outDir = Join-Path $WorkspaceRoot 'dist'
    $outZip = Join-Path $outDir "$componentName.fb2k-component"
    $stage  = Join-Path $env:TEMP "fb2k_pkg_$PID"

    if ($WhatIfPreference) {
        Write-Log "Would create: $outZip  (from $DllPath)" Cyan
        return
    }

    try {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
        New-Item -ItemType Directory -Path $stage | Out-Null

        # 1) The component DLL (name must start with "foo_").
        Copy-Item -LiteralPath $DllPath -Destination (Join-Path $stage "$componentName.dll") -Force

        # 2) The manifest: a plain-text file with the SAME base name as the
        #    component, placed at the archive root. foobar2000 reads the minimum
        #    version from it and finds the DLL by name.
        $manifest = Join-Path $stage "$componentName.fb2k-component"
        Set-Content -LiteralPath $manifest -Value ('foobar2000 v' + $MinVersion) -Encoding ASCII

        # 3) ZIP the staging contents (entries at archive root, not nested).
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
        if (Test-Path -LiteralPath $outZip) { Remove-Item -LiteralPath $outZip -Force }
        Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
        [System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $outZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

        Write-Log "Packaged: $outZip" Green
        Write-Log ('  Size    : {0:N0} bytes' -f (Get-Item $outZip).Length) Green
        Write-Log '  Contents:' Cyan
        $zip = [System.IO.Compression.ZipFile]::OpenRead($outZip)
        try {
            $zip.Entries | ForEach-Object { Write-Log ('    ' + $_.FullName) }
        } finally {
            $zip.Dispose()
        }
        Write-Log 'Install: foobar2000 > File > Preferences > Components > Install...' DarkGray
    } finally {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ---------------------------------------------------------------------------
# 3. Deploy (optional)
# ---------------------------------------------------------------------------

function Invoke-Deploy {
    param([string] $DllPath, [string] $ComponentName)

    if ([string]::IsNullOrWhiteSpace($Foobar2000Dir)) {
        throw '-Deploy requires -Foobar2000Dir <path to a foobar2000 folder>.'
    }
    $components = Join-Path $Foobar2000Dir 'components'
    if (-not (Test-Path -LiteralPath $components)) {
        throw "foobar2000 components folder not found: $components"
    }
    if (-not (Test-Path -LiteralPath $DllPath)) {
        throw "Built DLL not found at: $DllPath"
    }

    $dest = Join-Path $components "$ComponentName.dll"
    Write-Log "Deploying to $dest" Cyan
    if ($PSCmdlet.ShouldProcess($dest, 'Overwrite DLL in components')) {
        try {
            Copy-Item -LiteralPath $DllPath -Destination $dest -Force -ErrorAction Stop
            Write-Log 'Deployed. Restart foobar2000 if it is already running.' Green
        } catch {
            Write-Warning ("Could not copy to $dest`n  {0}`n  If foobar2000 is running, close it and retry." -f $_.Exception.Message)
        }
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

$hasAction = $Package -or $Deploy -or $Clean -or $CleanOnly
$interactive = $Interactive -or (-not $hasAction -and -not $WhatIfPreference)

if ($interactive) {
    # --- language (only if not passed as a parameter) ---
    if (-not $PSBoundParameters.ContainsKey('Language')) {
        Write-Host ''
        Write-Host $T['lang_title']
        Write-Host $T['lang_1']
        Write-Host $T['lang_2']
        $ans = Read-Host $T['lang_prompt']
        if ($ans -eq '2') { $Language = 'zh' } else { $Language = 'en' }
        $T = New-TextTable -Language $Language
    }

    # --- build form ---
    Write-Host ''
    Write-Host $T['form_title']
    Write-Host $T['form_1']
    Write-Host $T['form_2']
    Write-Host $T['form_3']
    Write-Host $T['form_4']
    Write-Host $T['form_5']
    $formChoice = Read-Host $T['form_prompt']
    switch ($formChoice) {
        '1' { $Package = $false; $Deploy = $false; $Clean = $false; $CleanOnly = $false }
        '2' { $Package = $true;  $Deploy = $false; $Clean = $false; $CleanOnly = $false }
        '3' { $Package = $false; $Deploy = $true;  $Clean = $false; $CleanOnly = $false }
        '4' { $Package = $false; $Deploy = $false; $Clean = $false; $CleanOnly = $true  }
        '5' { $Package = $true;  $Deploy = $true;  $Clean = $true;  $CleanOnly = $false }
        default { throw ($T['form_invalid'] -f $formChoice) }
    }

    # --- deploy folder ---
    if ($Deploy -and [string]::IsNullOrWhiteSpace($Foobar2000Dir)) {
        if ($formChoice -eq '3') {
            $Foobar2000Dir = Read-Host $T['deploy_ask']
        } else {
            Write-Host $T['deploy_skip'] -ForegroundColor Yellow
            $Deploy = $false
        }
    }

    # --- component (only if not passed as a parameter) ---
    if (-not $PSBoundParameters.ContainsKey('Component')) {
        Write-Host ''
        Write-Host $T['comp_title']
        Write-Host $T['comp_1']
        Write-Host $T['comp_2']
        Write-Host $T['comp_3']
        Write-Host $T['comp_4']
        $compChoice = Read-Host $T['comp_prompt']
        switch ($compChoice) {
            '1' { $Component = 'all' }
            '2' { $Component = 'floating_clouds' }
            '3' { $Component = 'organizing_playlists' }
            '4' { $Component = 'tags' }
            default { throw ($T['comp_invalid'] -f $compChoice) }
        }
    }
} elseif (-not $hasAction) {
    # Non-interactive run with no action switches (e.g. -WhatIf alone).
    Write-Host $T['usage_hint'] -ForegroundColor Yellow
    return
}

$selected = @(Get-SelectedComponents)

# Banner
Write-Host ''
Write-Host $T['banner'] -ForegroundColor Cyan
Write-Host ('  Workspace : {0}' -f $WorkspaceRoot)
Write-Host ('  Config    : {0} / {1}' -f $Configuration, $Platform)
Write-Host ('  VS        : {0}' -f $VsInstallDir)
Write-Host ('  MSBuild   : {0}' -f $msbuild)
Write-Host ('  Language  : {0}' -f $Language)
Write-Host ('  Component : {0}' -f $Component)
Write-Host ('  Builds    : {0}' -f (($selected | ForEach-Object { $_.ComponentName }) -join ', '))
Write-Host ''

# Clean
if ($CleanOnly) {
    Invoke-Clean
    return
}
if ($Clean) {
    Invoke-Clean
}

# Build loop: one log file per component in .\logs
$LogsDir = Join-Path $WorkspaceRoot 'logs'
New-Item -ItemType Directory -Path $LogsDir -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMddHHmmss'

$okCount   = 0
$failCount = 0
$failedNames = [System.Collections.Generic.List[string]]::new()

foreach ($comp in $selected) {
    $logFile = Join-Path $LogsDir ("{0}-{1}.log" -f $comp.ComponentName, $stamp)
    $script:CurrentLogFile = $logFile
    Add-Content -LiteralPath $logFile -Value ("==== {0} build {1} ====" -f $comp.ComponentName, (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')) -Encoding utf8
    Write-Log ("==== Building {0} ({1}) ====" -f $comp.Label, $comp.ComponentName) Cyan

    $compOk = $true
    try {
        if ($comp.Inline) {
            Invoke-BuildComponent -Component $comp
        } else {
            Invoke-ComponentScript -Component $comp -LogFile $logFile
        }
        if ($Package -and -not $comp.Inline) {
            Copy-PackageToRootDist -Component $comp
        }
    } catch {
        $compOk = $false
        Write-Log ("ERROR: {0}" -f $_.Exception.Message) Red
    }

    if ($compOk) { $okCount++ } else { $failCount++; $failedNames.Add($comp.ComponentName) }
    $script:CurrentLogFile = $null
}

# Summary
Write-Host ''
Write-Log ($T['summary'] -f $okCount, $failCount) Cyan
if ($failCount -gt 0) {
    Write-Log ($T['failed_list'] -f ($failedNames -join ', ')) Red
}

Write-Host ''
Write-Host $T['packages'] -ForegroundColor Cyan
$rootDist = Join-Path $WorkspaceRoot 'dist'
if (Test-Path -LiteralPath $rootDist) {
    Get-ChildItem -LiteralPath $rootDist -Filter '*.fb2k-component' | ForEach-Object {
        Write-Host ('  {0}  ({1:N0} bytes)' -f $_.Name, $_.Length)
    }
}
Write-Host $T['logs'] -ForegroundColor Cyan
Get-ChildItem -LiteralPath $LogsDir -Filter ("*-{0}.log" -f $stamp) -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host ('  ' + $_.Name)
}

if ($failCount -gt 0) { exit 1 }
