$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot

$buildDir = Join-Path $repoRoot 'build\release'
$distRoot = Join-Path $repoRoot 'dist'
$appDir = Join-Path $distRoot 'SheetMaster'
$exePath = Join-Path $buildDir 'SheetMaster.exe'
$qtBin = 'C:\msys64\ucrt64\bin'
$qtTlsDir = 'C:\msys64\ucrt64\share\qt6\plugins\tls'

cmake --preset release
cmake --build --preset release --parallel

if (Test-Path $appDir) { Remove-Item -Recurse -Force $appDir }
New-Item -ItemType Directory -Force $appDir | Out-Null

Copy-Item $exePath (Join-Path $appDir 'SheetMaster.exe') -Force
windeployqt6.exe --release --compiler-runtime --dir $appDir (Join-Path $appDir 'SheetMaster.exe')

# Force OpenSSL backend + libs for broader TLS compatibility.
Copy-Item -Force (Join-Path $qtTlsDir 'qopensslbackend.dll') (Join-Path $appDir 'tls\qopensslbackend.dll')
Copy-Item -Force (Join-Path $qtBin 'libssl-3-x64.dll') $appDir
Copy-Item -Force (Join-Path $qtBin 'libcrypto-3-x64.dll') $appDir

# Copy project runtime data/docs.
Copy-Item -Force README.md, LICENSE, CHANGELOG.md $appDir
if (Test-Path 'settings.PACFG') { Copy-Item -Force 'settings.PACFG' $appDir }
Copy-Item -Recurse -Force 'sheets' (Join-Path $appDir 'sheets')

# Fill missing non-system DLL dependencies from MSYS2 UCRT runtime bin.
$systemDlls = @(
  'kernel32.dll','user32.dll','gdi32.dll','advapi32.dll','shell32.dll','ole32.dll','oleaut32.dll',
  'comdlg32.dll','comctl32.dll','ws2_32.dll','imm32.dll','winmm.dll','mpr.dll','version.dll',
  'setupapi.dll','dwmapi.dll','uxtheme.dll','bcrypt.dll','crypt32.dll','secur32.dll','wtsapi32.dll',
  'shlwapi.dll','netapi32.dll','iphlpapi.dll','dnsapi.dll','winspool.drv','rpcrt4.dll','msvcrt.dll',
  'ntdll.dll','cfgmgr32.dll','powrprof.dll','authz.dll','userenv.dll','winhttp.dll','ncrypt.dll',
  'dwrite.dll','usp10.dll','d3d9.dll','d3d11.dll','d3d12.dll','dxgi.dll','shcore.dll','msvcp_win.dll',
  'normaliz.dll','wsock32.dll','combase.dll','win32u.dll'
)

function Get-MissingNonSystemDlls {
  $files = Get-ChildItem $appDir -Recurse -File -Include *.exe,*.dll
  $present = @{}
  foreach($dll in Get-ChildItem $appDir -Recurse -File -Filter *.dll){ $present[$dll.Name.ToLower()] = $true }

  $missing = New-Object System.Collections.Generic.HashSet[string]
  foreach($file in $files){
    $imports = (& objdump.exe -p $file.FullName | Select-String 'DLL Name:').Line |
      ForEach-Object { ($_ -replace '.*DLL Name:\s*','').Trim().ToLower() }

    foreach($dep in $imports){
      if($dep -match '^(api-ms-win-|ext-ms-win-)'){ continue }
      if($systemDlls -contains $dep){ continue }
      if($dep -eq $file.Name.ToLower()){ continue }
      if(-not $present.ContainsKey($dep)){ [void]$missing.Add($dep) }
    }
  }

  return @($missing)
}

for($i=0; $i -lt 10; $i++){
  $missing = Get-MissingNonSystemDlls
  if($missing.Count -eq 0){ break }
  foreach($name in $missing){
    $src = Join-Path $qtBin $name
    if(Test-Path $src){ Copy-Item -Force $src $appDir }
  }
}

$finalMissing = Get-MissingNonSystemDlls
if($finalMissing.Count -gt 0){
  throw "Missing non-system DLLs remain: $($finalMissing -join ', ')"
}

$zipPath = Join-Path $distRoot 'SheetMaster-portable-win64.zip'
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path (Join-Path $appDir '*') -DestinationPath $zipPath -CompressionLevel Optimal

Write-Output "Portable folder: $appDir"
Write-Output "Portable zip: $zipPath"
