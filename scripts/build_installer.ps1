<#
.SYNOPSIS
    Builds MagnifyFactory in Release mode and packages it into a Windows
    installer (dist\MagnifyFactory-Setup-<version>.exe) with Inno Setup.

.DESCRIPTION
    Requires: Visual Studio (C++ workload), CMake, a vcpkg install with
    qtbase[widgets], and Inno Setup 6 (ISCC.exe) — install it with:
      winget install --id JRSoftware.InnoSetup -e
#>
param(
    [string]$VcpkgToolchain = "C:/Users/Henrique/Documentos/vcpkg/scripts/buildsystems/vcpkg.cmake",
    [string]$InnoSetupCompiler = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

Write-Host "== Configuring Release build ==" -ForegroundColor Cyan
cmake -S $repoRoot -B "$repoRoot\build-release" -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DCMAKE_BUILD_TYPE=Release

Write-Host "== Building MagnifyFactory (Release) ==" -ForegroundColor Cyan
cmake --build "$repoRoot\build-release" --target MagnifyFactory --config Release

if (-not (Test-Path $InnoSetupCompiler)) {
    throw "Inno Setup compiler not found at '$InnoSetupCompiler'. Install it with: winget install --id JRSoftware.InnoSetup -e"
}

Write-Host "== Compiling installer ==" -ForegroundColor Cyan
& $InnoSetupCompiler "$repoRoot\installer\MagnifyFactory.iss"

Write-Host "Done. Installer is in $repoRoot\dist\" -ForegroundColor Green
