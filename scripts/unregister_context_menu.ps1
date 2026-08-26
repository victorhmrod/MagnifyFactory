<#
.SYNOPSIS
    Removes the "Convert with MagnifyFactory" Explorer context menu entry
    added by register_context_menu.ps1.
#>
$shellKey = "HKCU:\Software\Classes\*\shell\MagnifyFactory"

if (Test-Path $shellKey) {
    Remove-Item -Path $shellKey -Recurse -Force
    Write-Host "Removed 'Convert with MagnifyFactory' from the Explorer context menu."
} else {
    Write-Host "Nothing to remove — context menu entry was not registered."
}
