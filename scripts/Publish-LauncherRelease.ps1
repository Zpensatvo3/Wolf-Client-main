#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$Dll,
    [Parameter(Mandatory)][ValidatePattern('^[0-9A-Za-z][0-9A-Za-z._-]{0,63}$')][string]$Version
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AuthVault.ps1')
$releaseFile = Get-Item -LiteralPath $Dll
if ($releaseFile.Extension -ne '.dll' -or $releaseFile.Length -lt 4096 -or $releaseFile.Length -gt 24MB) {
    throw 'Release must be a DLL between 4 KiB and 24 MiB.'
}
$releaseHash = (Get-FileHash -LiteralPath $releaseFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$authVault = Read-OnestepAuthVault
try {
    # The admin credential is carried only in an HTTPS header, never CLI args/logs.
    $releaseResult = Invoke-RestMethod -Method Put -Uri 'https://onestep-auth.gogogamesas.workers.dev/api/v1/admin/release' `
        -Headers @{Authorization=('Bearer ' + $authVault.admin_token); 'X-Release-Version'=$Version; 'X-Release-Sha256'=$releaseHash} `
        -ContentType 'application/octet-stream' -InFile $releaseFile.FullName -TimeoutSec 120
    if (-not $releaseResult.success -or $releaseResult.sha256 -ne $releaseHash) { throw 'Release publication failed.' }
    Write-Host ('Published {0}: {1} bytes; SHA256 {2}' -f $Version, $releaseResult.bytes, $releaseHash)
}
finally { $authVault = $null }
