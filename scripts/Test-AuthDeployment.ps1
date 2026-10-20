#requires -Version 7.0
param([string]$Probe = '')
$ErrorActionPreference = 'Stop'
$authRoot = Split-Path -Parent $PSScriptRoot
$authProfile = Get-Content -LiteralPath (Join-Path $authRoot 'server\deployment.json') -Raw | ConvertFrom-Json
$authBase = 'https://' + $authProfile.hostname
if ($Probe) { throw 'Automatic credential-backed live probes are disabled. Use an explicit operator session; no production vault is opened by a health check.' }
$authHealth = Invoke-RestMethod -Uri ($authBase + '/health') -TimeoutSec 25
if ($authHealth.status -ne 'ok') { throw 'Health check failed.' }
[pscustomobject]@{Service=$authProfile.hostname;Health='ok';Mutation='none'}
