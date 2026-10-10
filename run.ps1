[CmdletBinding()]
param(
    [switch]$Test,
    [string]$Scan,
    [switch]$Build,
    [switch]$Native,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ScriptDir

$ExePath = Join-Path $ScriptDir "build_rel\Koltzi.exe"
$TestsExePath = Join-Path $ScriptDir "build_rel\KoltziTests.exe"

function Find-VcVars64 {
    $possiblePaths = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    )

    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            return $path
        }
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstall = & $vswhere -latest -property installationPath
        if ($vsInstall) {
            $candidate = Join-Path $vsInstall "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    return $null
}

function Invoke-Build {
    Write-Host "[Koltzi] Building Release binaries (Koltzi.exe, KoltziTests.exe)..." -ForegroundColor Cyan
    $vcvars = Find-VcVars64
    if (-not $vcvars) {
        Write-Error "Could not find MSVC vcvars64.bat. Please run from a Visual Studio Developer Command Prompt."
        return $false
    }

    $cmd = "call `"$vcvars`" && cmake -B build_rel -S . -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build_rel"
    cmd.exe /c $cmd
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed with exit code $LASTEXITCODE."
        return $false
    }
    $compileCommands = Join-Path $ScriptDir "build_rel\compile_commands.json"
    if (Test-Path $compileCommands) {
        Copy-Item -Path $compileCommands -Destination (Join-Path $ScriptDir "compile_commands.json") -Force
    }
    Write-Host "[Koltzi] Build completed successfully." -ForegroundColor Green
    return $true
}

if ($Help) {
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host "  Koltzi - Malware Triage Agent Execution Script" -ForegroundColor Cyan
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host "Usage:"
    Write-Host "  .\run.ps1               Launch the floating desktop companion"
    Write-Host "  .\run.ps1 -Test         Run the automated unit test suite"
    Write-Host "  .\run.ps1 -Scan <path>  Run command-line triage on a PE file"
    Write-Host "  .\run.ps1 -Build        Recompile Release binaries with Ninja"
    Write-Host "  .\run.ps1 -Help         Display this help menu"
    exit 0
}

# 1. Automatic Timestamp Check & Build Handling
$needBuild = $Build
if ((-not (Test-Path $ExePath)) -or (-not (Test-Path $TestsExePath))) {
    $needBuild = $true
} else {
    $exeTime = (Get-Item $ExePath).LastWriteTime
    $newer = Get-ChildItem -Path "src" -Recurse -File | Where-Object { $_.LastWriteTime -gt $exeTime }
    if ($newer) {
        Write-Host "[Koltzi] Source changes detected ($($newer.Count) files newer than binary). Recompiling..." -ForegroundColor Yellow
        $needBuild = $true
    }
}

if ($needBuild) {
    $ok = Invoke-Build
    if (-not $ok) { exit 1 }
    if ($Build) { exit 0 }
}

# 2. Handle Automated Test Mode
if ($Test) {
    Write-Host "[Koltzi] Executing test suite via KoltziTests.exe..." -ForegroundColor Cyan
    & $TestsExePath
    exit $LASTEXITCODE
}

# 3. Handle CLI File Scan Mode
if ($Scan) {
    if (-not (Test-Path $Scan)) {
        Write-Error "File not found: $Scan"
        exit 1
    }
    $resolvedPath = (Resolve-Path $Scan).Path
    Write-Host "[Koltzi] Analyzing: $resolvedPath" -ForegroundColor Cyan
    & $TestsExePath $resolvedPath
    exit $LASTEXITCODE
}

# 4. Default Action: Launch Modern Desktop Application (or -Native for Direct2D)
if ($Native) {
    Write-Host "[Koltzi] Launching native Direct2D executable..." -ForegroundColor Green
    Start-Process -FilePath $ExePath -WorkingDirectory $PSScriptRoot
    exit 0
}

Write-Host "[Koltzi] Launching modern desktop frontend with Ghost companion..." -ForegroundColor Green
Start-Process -FilePath "cmd.exe" -ArgumentList "/c npx electron ." -WorkingDirectory $ScriptDir
exit 0


