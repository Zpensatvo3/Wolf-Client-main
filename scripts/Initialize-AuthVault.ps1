#requires -Version 7.0
param([string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AuthVault.ps1')
Protect-OnestepVaultDirectory (Get-OnestepAuthVaultDirectory $Directory)
Write-Host 'External operator directory prepared. No secrets were read or generated. Import interactively with Import-LegacyAuthVault.ps1.'
