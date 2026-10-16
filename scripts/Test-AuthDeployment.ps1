#requires -Version 7.0
param([string]$Probe = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AuthVault.ps1')
$authRoot = Split-Path -Parent $PSScriptRoot
$authProfile = Get-Content -LiteralPath (Join-Path $authRoot 'server\deployment.json') -Raw | ConvertFrom-Json
$authBase = 'https://' + $authProfile.hostname
$authVault = Read-OnestepAuthVault
try {
    $authHealth = Invoke-RestMethod -Uri ($authBase + '/health') -TimeoutSec 25
    if ($authHealth.status -ne 'ok') { throw 'Health check failed.' }
    $authIdPath = Join-Path $authRoot 'server\.credentials\owner-license.id'
    if (-not (Test-Path -LiteralPath $authIdPath)) {
        try {
            $authCreated = Invoke-RestMethod -Uri ($authBase + '/api/v1/admin/create-key') -Method Post -TimeoutSec 25 `
                -Headers @{ Authorization = 'Bearer ' + $authVault.admin_token } -ContentType 'application/json' `
                -Body (@{ key = $authVault.owner_license; days = 'lifetime' } | ConvertTo-Json -Compress)
        }
        catch { throw 'Owner license could not be created; inspect Worker logs.' }
        if (-not $authCreated.success -or -not $authCreated.id) { throw 'Invalid license creation result.' }
        [System.IO.File]::WriteAllText($authIdPath, [string]$authCreated.id)
        $authCreated = $null
    }
    if ($Probe) {
        if (-not (Test-Path -LiteralPath $Probe)) { throw 'Native probe executable not found.' }
        $authVault.owner_license | & $Probe $authProfile.hostname
        if ($LASTEXITCODE -ne 0) { throw 'Native auth smoke check failed.' }
    }
    [pscustomobject]@{ Service = $authProfile.hostname; Health = 'ok'; OwnerLicenseId = [int](Get-Content $authIdPath) }
}
finally { $authVault = $null; $authCreated = $null }
