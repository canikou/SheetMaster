# SPDX-License-Identifier: MIT

param(
    [string]$RepoRoot = (Resolve-Path ".").Path,
    [string]$Version = "",
    [string]$PortableDir = "",
    [string]$DistDir = "",
    [string]$InnoCompiler = "",
    [switch]$SkipPortableBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Set-Location $RepoRoot

function Get-CMakeCacheValue {
    param(
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)][string]$CachePath
    )

    $escaped = [regex]::Escape($Key)
    $match = Select-String -Path $CachePath -Pattern "^${escaped}:[^=]*=(.*)$" | Select-Object -First 1
    if (-not $match) {
        return $null
    }
    return $match.Matches[0].Groups[1].Value.Trim()
}

function Resolve-InnoCompiler {
    param(
        [string]$PreferredPath
    )

    if (-not [string]::IsNullOrWhiteSpace($PreferredPath) -and (Test-Path $PreferredPath)) {
        return (Resolve-Path $PreferredPath).Path
    }

    $candidates = @()
    $fromPath = Get-Command -Name iscc.exe, iscc -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($fromPath) {
        $candidates += $fromPath.Source
    }

    $candidates += @(
        (Join-Path $env:LOCALAPPDATA "Programs\\Inno Setup 6\\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\\ISCC.exe")
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw @"
Inno Setup compiler (ISCC.exe) not found.
Install with:
  winget install --id JRSoftware.InnoSetup -e --source winget
"@
}

if (-not $SkipPortableBuild) {
    powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepoRoot "scripts/package-portable.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "Portable packaging failed with exit code $LASTEXITCODE"
    }
}

$buildDir = Join-Path $RepoRoot "build/release"
$cachePath = Join-Path $buildDir "CMakeCache.txt"
if (-not (Test-Path $cachePath)) {
    throw "Release configure cache not found: $cachePath`nRun: cmake --preset release"
}

$projectName = Get-CMakeCacheValue -Key "CMAKE_PROJECT_NAME" -CachePath $cachePath
$projectVersion = Get-CMakeCacheValue -Key "CMAKE_PROJECT_VERSION" -CachePath $cachePath
if ([string]::IsNullOrWhiteSpace($projectName)) {
    $projectName = "SheetMaster"
}
if ([string]::IsNullOrWhiteSpace($projectVersion)) {
    $projectVersion = "0.0.0"
}
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $projectVersion
}

if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path $RepoRoot "dist"
}
if ([string]::IsNullOrWhiteSpace($PortableDir)) {
    $PortableDir = Join-Path $DistDir $projectName
}
if (-not (Test-Path $PortableDir)) {
    throw "Portable folder not found: $PortableDir`nRun scripts/package-portable.ps1 first."
}

$issPath = Join-Path $RepoRoot "installer/SheetMaster.iss"
if (-not (Test-Path $issPath)) {
    throw "Installer script not found: $issPath"
}

$resolvedDistDir = (Resolve-Path (New-Item -ItemType Directory -Path $DistDir -Force)).Path
$resolvedPortableDir = (Resolve-Path $PortableDir).Path
$iscc = Resolve-InnoCompiler -PreferredPath $InnoCompiler

& $iscc `
    "/Qp" `
    "/O$resolvedDistDir" `
    "/DMyAppName=$projectName" `
    "/DMyAppVersion=$Version" `
    "/DMySourceDir=$resolvedPortableDir" `
    $issPath

if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE"
}

$setupPath = Join-Path $DistDir ("{0}-{1}-setup.exe" -f $projectName, $Version)
if (-not (Test-Path $setupPath)) {
    throw "Expected setup output missing: $setupPath"
}

Write-Host "Setup installer: $setupPath" -ForegroundColor Green

