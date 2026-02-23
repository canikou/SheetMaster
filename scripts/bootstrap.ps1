# SPDX-License-Identifier: MIT

param(
    [Parameter(Mandatory = $true)]
    [string]$AppName,
    [string]$Namespace,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($Namespace)) {
    $Namespace = ($AppName.ToLowerInvariant() -replace "[^a-z0-9_]", "_")
}

if ($Namespace -notmatch "^[A-Za-z_][A-Za-z0-9_]*$") {
    throw "Namespace '$Namespace' is invalid. Use letters, digits, and underscores only."
}

function Replace-RegexInFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Replacement
    )

    if (-not (Test-Path $Path)) {
        return
    }

    $original = Get-Content $Path -Raw
    $updated = [regex]::Replace($original, $Pattern, $Replacement)
    if ($updated -ne $original) {
        Write-Host "update: $Path"
        if (-not $DryRun) {
            Set-Content -Path $Path -Value $updated -NoNewline -Encoding utf8
        }
    }
}

$mainPath = Join-Path $repoRoot "src/main.cpp"
$oldNamespace = "app"
if (Test-Path $mainPath) {
    $mainContent = Get-Content $mainPath -Raw
    $match = [regex]::Match($mainContent, '#include\s+"([^"/\\]+)/core\.hpp"')
    if ($match.Success) {
        $oldNamespace = $match.Groups[1].Value
    }
}

$oldNamespaceEscaped = [regex]::Escape($oldNamespace)
$appNameLower = $AppName.ToLowerInvariant()

$cmakePath = Join-Path $repoRoot "CMakeLists.txt"
Replace-RegexInFile -Path $cmakePath -Pattern 'set\(APP_NAME\s+"[^"]+"\)' -Replacement "set(APP_NAME `"$AppName`")"

$vcpkgPath = Join-Path $repoRoot "vcpkg.json"
Replace-RegexInFile -Path $vcpkgPath -Pattern '"name"\s*:\s*"[^"]+"' -Replacement "`"name`": `"$appNameLower-template`""

$sourceFiles = @(
    "src/main.cpp",
    "src/core.cpp",
    "tests/core_tests.cpp",
    "tests/string_tests.cpp"
) | ForEach-Object { Join-Path $repoRoot $_ }

foreach ($file in $sourceFiles) {
    Replace-RegexInFile -Path $file -Pattern ([regex]::Escape("""$oldNamespace/core.hpp""")) -Replacement """$Namespace/core.hpp"""
    Replace-RegexInFile -Path $file -Pattern "\b$oldNamespaceEscaped::" -Replacement "$Namespace::"
    Replace-RegexInFile -Path $file -Pattern "\bnamespace\s+$oldNamespaceEscaped\b" -Replacement "namespace $Namespace"
}

$oldHeaderPath = Join-Path $repoRoot "include/$oldNamespace/core.hpp"
Replace-RegexInFile -Path $oldHeaderPath -Pattern "\bnamespace\s+$oldNamespaceEscaped\b" -Replacement "namespace $Namespace"
Replace-RegexInFile -Path $oldHeaderPath -Pattern "\b$oldNamespaceEscaped::" -Replacement "$Namespace::"

if ($oldNamespace -ne $Namespace) {
    $oldHeaderDir = Join-Path $repoRoot "include/$oldNamespace"
    $newHeaderDir = Join-Path $repoRoot "include/$Namespace"
    $oldHeaderFile = Join-Path $oldHeaderDir "core.hpp"
    $newHeaderFile = Join-Path $newHeaderDir "core.hpp"

    if (Test-Path $oldHeaderFile) {
        Write-Host "move: $oldHeaderFile -> $newHeaderFile"
        if (-not $DryRun) {
            New-Item -ItemType Directory -Path $newHeaderDir -Force | Out-Null
            Move-Item -Path $oldHeaderFile -Destination $newHeaderFile -Force

            if ((Get-ChildItem -Path $oldHeaderDir -Force | Measure-Object).Count -eq 0) {
                Remove-Item -Path $oldHeaderDir
            }
        }
    }
}

Write-Host "bootstrap complete: AppName=$AppName Namespace=$Namespace DryRun=$DryRun"
