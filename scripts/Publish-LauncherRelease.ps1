#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$Dll,
    [Parameter(Mandatory)][ValidatePattern('^[0-9A-Za-z][0-9A-Za-z._-]{0,63}$')][string]$Version
)
$ErrorActionPreference = 'Stop'
# No legacy vault unlocking, bearer credentials or secret files.
throw 'Legacy bearer/vault publication is disabled. Authenticate with Cloudflare + MFA in the OneStep admin panel and publish the DLL from System status.'
