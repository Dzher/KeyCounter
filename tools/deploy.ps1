# keyCounter - release packaging: build + windeployqt + SQLite driver, into a portable folder.
# Usage: powershell -ExecutionPolicy Bypass -File tools/deploy.ps1 [-QtDir D:/Qt6/6.8.2/msvc2022_64] [-OutDir dist/keyCounter]
# NOTE: keep this file ASCII-only (PowerShell 5.1 reads .ps1 as ANSI without a BOM).
param(
    [string]$QtDir = "D:/Qt6/6.8.2/msvc2022_64",
    [string]$BuildDir = "build",
    [string]$OutDir = "dist/keyCounter",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

if (-not (Test-Path $QtDir)) {
    Write-Error "Qt directory not found: $QtDir (pass -QtDir <path to Qt msvc kit>)"
}

if (-not $SkipBuild) {
    Write-Output "[1/5] configure + build (Release)"
    cmake -S . -B $BuildDir -G Ninja -DCMAKE_PREFIX_PATH=$QtDir -DCMAKE_BUILD_TYPE=Release
    cmake --build $BuildDir
}

$exe = Join-Path $BuildDir "keyCounter.exe"
if (-not (Test-Path $exe)) { Write-Error "missing $exe" }

Write-Output "[2/5] copy executable to $OutDir"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Copy-Item $exe (Join-Path $OutDir "keyCounter.exe") -Force

Write-Output "[3/5] windeployqt"
$windeploy = Join-Path $QtDir "bin/windeployqt.exe"
& $windeploy --release --no-translations --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime (Join-Path $OutDir "keyCounter.exe")
if ($LASTEXITCODE -ne 0) { Write-Error "windeployqt failed" }

Write-Output "[4/5] SQLite driver (required: statistics database lives in SQLite)"
$sqlDir = Join-Path $OutDir "sqldrivers"
New-Item -ItemType Directory -Force -Path $sqlDir | Out-Null
Copy-Item (Join-Path $QtDir "plugins/sqldrivers/qsqlite.dll") $sqlDir -Force

Write-Output "[5/5] docs + portable marker"
foreach ($doc in @("README.md", "LICENSE")) {
    if (Test-Path $doc) { Copy-Item $doc $OutDir -Force }
}
# portable mode: create an empty "portable.flag" next to the exe -> data is stored in .\data instead of %APPDATA%

$size = (Get-ChildItem -Recurse $OutDir | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Output ("done: {0} ({1:N1} MB)" -f (Resolve-Path $OutDir), $size)
Write-Output "run: $OutDir\keyCounter.exe   (silent/autostart: --silent, console dump: --dump)"
