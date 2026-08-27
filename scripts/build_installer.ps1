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
    [string]$InnoSetupCompiler = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    [string]$SignTool = "",
    [string]$SignCertificatePath = $env:SIGN_CERT_PATH,
    [string]$SignCertificatePassword = $env:SIGN_CERT_PASSWORD,
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$installerScript = "$repoRoot\installer\MagnifyFactory.iss"
$appVersion = (Select-String -Path $installerScript -Pattern '#define MyAppVersion "([^"]+)"').Matches.Groups[1].Value

Write-Host "== Configuring Release build ==" -ForegroundColor Cyan
cmake -S $repoRoot -B "$repoRoot\build-release" -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== Building MagnifyFactory (Release) ==" -ForegroundColor Cyan
cmake --build "$repoRoot\build-release" --target MagnifyFactory --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($SignCertificatePath) {
    if (-not $SignTool) {
        $SignTool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like "*\x64\signtool.exe" } |
            Sort-Object FullName -Descending |
            Select-Object -First 1 -ExpandProperty FullName
    }

    if (-not $SignTool) {
        throw "signtool.exe not found. Install the Windows SDK or pass -SignTool."
    }

    $signArgs = @("sign", "/fd", "SHA256", "/tr", $TimestampUrl, "/td", "SHA256", "/f", $SignCertificatePath)
    if ($SignCertificatePassword) {
        $signArgs += @("/p", $SignCertificatePassword)
    }

    Write-Host "== Signing MagnifyFactory.exe ==" -ForegroundColor Cyan
    & $SignTool @signArgs "$repoRoot\build-release\MagnifyFactory.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not (Test-Path $InnoSetupCompiler)) {
    throw "Inno Setup compiler not found at '$InnoSetupCompiler'. Install it with: winget install --id JRSoftware.InnoSetup -e"
}

Write-Host "== Compiling installer ==" -ForegroundColor Cyan
& $InnoSetupCompiler $installerScript
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($SignCertificatePath) {
    Write-Host "== Signing installer ==" -ForegroundColor Cyan
    & $SignTool @signArgs "$repoRoot\dist\MagnifyFactory-Setup-$appVersion.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Done. Installer is in $repoRoot\dist\" -ForegroundColor Green
