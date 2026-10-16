#requires -Version 7.0
param([ValidateSet('Admin', 'License')][string]$Kind = 'Admin')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AuthVault.ps1')
$authVault = Read-OnestepAuthVault
try {
    $authValue = if ($Kind -eq 'Admin') { $authVault.admin_token } else { $authVault.owner_license }
    if (-not $authValue) { throw 'Kimlik bilgisi bulunamadi.' }
    Set-Clipboard -Value $authValue
    Write-Host "$Kind bilgisi panoya kopyalandi."
}
finally { $authValue = $null; $authVault = $null }
