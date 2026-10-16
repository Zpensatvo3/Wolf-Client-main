#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$authRoot = Split-Path -Parent $PSScriptRoot
$authVaultDir = Join-Path $authRoot 'server\.credentials'
New-Item -ItemType Directory -Path $authVaultDir -Force | Out-Null
$authKeyPath = Join-Path $authVaultDir 'wrapping-key.dpapi'
$authRsa = [System.Security.Cryptography.RSA]::Create(3072)
try {
    if (Test-Path -LiteralPath $authKeyPath) {
        $authPrivate = [System.Security.Cryptography.ProtectedData]::Unprotect(
            [System.IO.File]::ReadAllBytes($authKeyPath), $null,
            [System.Security.Cryptography.DataProtectionScope]::CurrentUser)
        try { $authRsa.ImportPkcs8PrivateKey($authPrivate, [ref]0) }
        finally { [Array]::Clear($authPrivate) }
    }
    else {
        $authPrivate = $authRsa.ExportPkcs8PrivateKey()
        try {
            $authProtected = [System.Security.Cryptography.ProtectedData]::Protect(
                $authPrivate, $null, [System.Security.Cryptography.DataProtectionScope]::CurrentUser)
            [System.IO.File]::WriteAllBytes($authKeyPath, $authProtected)
        }
        finally { [Array]::Clear($authPrivate) }
    }
    # Public wrapping key only. No server secret or private key is printed.
    [Convert]::ToBase64String($authRsa.ExportSubjectPublicKeyInfo())
}
finally { $authRsa.Dispose() }
