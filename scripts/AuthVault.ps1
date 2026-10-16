#requires -Version 7.0
function Read-OnestepAuthVault {
    param([string]$EnvelopePath)
    $authRoot = Split-Path -Parent $PSScriptRoot
    $authVaultDir = Join-Path $authRoot 'server\.credentials'
    if (-not $EnvelopePath) { $EnvelopePath = Join-Path $authVaultDir 'worker-secrets.enc.json' }
    $authEnvelope = Get-Content -LiteralPath $EnvelopePath -Raw | ConvertFrom-Json
    $authPrivate = [System.Security.Cryptography.ProtectedData]::Unprotect(
        [System.IO.File]::ReadAllBytes((Join-Path $authVaultDir 'wrapping-key.dpapi')), $null,
        [System.Security.Cryptography.DataProtectionScope]::CurrentUser)
    $authRsa = [System.Security.Cryptography.RSA]::Create()
    $authKey = $null
    $authPlain = $null
    $authAes = $null
    try {
        $authRsa.ImportPkcs8PrivateKey($authPrivate, [ref]0)
        $authKey = $authRsa.Decrypt([Convert]::FromBase64String($authEnvelope.wrapped_key),
            [System.Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
        $authCipher = [Convert]::FromBase64String($authEnvelope.ciphertext)
        $authIv = [Convert]::FromBase64String($authEnvelope.iv)
        $authPlain = [byte[]]::new($authCipher.Length - 16)
        $authAes = [System.Security.Cryptography.AesGcm]::new($authKey, 16)
        $authAes.Decrypt($authIv, $authCipher[0..($authCipher.Length - 17)],
            $authCipher[($authCipher.Length - 16)..($authCipher.Length - 1)], $authPlain)
        return ([System.Text.Encoding]::UTF8.GetString($authPlain) | ConvertFrom-Json)
    }
    finally {
        if ($authAes) { $authAes.Dispose() }
        $authRsa.Dispose()
        [Array]::Clear($authPrivate)
        if ($authKey) { [Array]::Clear($authKey) }
        if ($authPlain) { [Array]::Clear($authPlain) }
    }
}
