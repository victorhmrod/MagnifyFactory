<#
.SYNOPSIS
    Adds "Convert with MagnifyFactory" to the Windows Explorer right-click
    menu for every file type.

.DESCRIPTION
    Registers a per-user (HKEY_CURRENT_USER) shell command — no administrator
    rights required, and nothing outside the current user's own registry hive
    is touched. Run unregister_context_menu.ps1 to remove it again.
#>
param(
    [string]$ExePath = (Join-Path $PSScriptRoot "..\build\MagnifyFactory.exe")
)

$ExePath = (Resolve-Path $ExePath -ErrorAction Stop).Path

$shellKey = "HKCU:\Software\Classes\*\shell\MagnifyFactory"
$commandKey = "$shellKey\command"

New-Item -Path $shellKey -Force | Out-Null
Set-ItemProperty -Path $shellKey -Name "(default)" -Value "Convert with MagnifyFactory"
Set-ItemProperty -Path $shellKey -Name "Icon" -Value "`"$ExePath`""

New-Item -Path $commandKey -Force | Out-Null
Set-ItemProperty -Path $commandKey -Name "(default)" -Value "`"$ExePath`" `"%1`""

Write-Host "Registered 'Convert with MagnifyFactory' for the current user, pointing to:"
Write-Host "  $ExePath"
Write-Host "Right-click any file in Explorer to see it. Run unregister_context_menu.ps1 to remove."
