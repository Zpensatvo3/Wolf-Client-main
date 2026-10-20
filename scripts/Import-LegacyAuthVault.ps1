#requires -Version 7.0
[CmdletBinding(SupportsShouldProcess,ConfirmImpact='High')]
param([Parameter(Mandatory)][switch]$AcknowledgeExposure,[string]$SourceDirectory,[string]$Directory,
    [Security.SecureString]$Password,[switch]$RemoveLegacyAfterVerification,[switch]$CredentialsSavedSeparately)
$ErrorActionPreference='Stop'
if (-not $AcknowledgeExposure) { throw 'AcknowledgeExposure is required. Moving a vault does not revoke exposed secrets.' }
if ($RemoveLegacyAfterVerification -and -not $CredentialsSavedSeparately) {
    throw 'Confirm CredentialsSavedSeparately before deleting a vault containing the old admin and owner-license credentials.'
}
. (Join-Path $PSScriptRoot 'AuthVault.ps1')
if (-not $SourceDirectory) { $SourceDirectory=Join-Path (Split-Path -Parent $PSScriptRoot) 'server/.credentials' }
$SourceDirectory=[IO.Path]::GetFullPath($SourceDirectory); $Directory=Get-OnestepAuthVaultDirectory $Directory
$envelopeFile=Join-Path $SourceDirectory 'worker-secrets.enc.json'; $wrappingFile=Join-Path $SourceDirectory 'wrapping-key.dpapi'
if (-not $Password) {
    $Password=Read-Host 'New independent passphrase (16+ characters; never paste into chat)' -AsSecureString
    $repeat=Read-Host 'Repeat passphrase' -AsSecureString
    $firstBytes=ConvertTo-OnestepPasswordBytes $Password; $repeatBytes=ConvertTo-OnestepPasswordBytes $repeat
    try { if (-not [Security.Cryptography.CryptographicOperations]::FixedTimeEquals($firstBytes,$repeatBytes)) { throw 'Passphrases do not match.' } }
    finally { [Array]::Clear($firstBytes); [Array]::Clear($repeatBytes); $repeat.Dispose() }
}
if (-not $PSCmdlet.ShouldProcess($Directory,'Import legacy production secrets into external password-protected vault')) { return }
$private=$key=$plain=$rsa=$aes=$null
try {
    # The only explicit legacy reader. Run manually in a trusted operator session.
    $envelope=Get-Content -LiteralPath $envelopeFile -Raw | ConvertFrom-Json
    $private=[Security.Cryptography.ProtectedData]::Unprotect([IO.File]::ReadAllBytes($wrappingFile),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
    $rsa=[Security.Cryptography.RSA]::Create(); $rsa.ImportPkcs8PrivateKey($private,[ref]0)
    $key=$rsa.Decrypt([Convert]::FromBase64String($envelope.wrapped_key),[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
    $cipher=[Convert]::FromBase64String($envelope.ciphertext); $plain=[byte[]]::new($cipher.Length-16)
    $aes=[Security.Cryptography.AesGcm]::new($key,16)
    $aes.Decrypt([Convert]::FromBase64String($envelope.iv),$cipher[0..($cipher.Length-17)],$cipher[($cipher.Length-16)..($cipher.Length-1)],$plain)
    $legacy=[Text.Encoding]::UTF8.GetString($plain) | ConvertFrom-Json
    Write-OnestepAuthVault -Directory $Directory -Password $Password -Payload @{secrets=$legacy.secrets}
    $verified=Read-OnestepAuthVault -Directory $Directory -Password $Password
    foreach ($property in $legacy.secrets.PSObject.Properties) {
        if ($verified.secrets.($property.Name) -cne $property.Value) { throw 'Verification failed; source files preserved.' }
    }
    Write-Host 'External vault verified. Admin token and owner license were NOT copied; save separately before removal/reissue.'
    if ($RemoveLegacyAfterVerification -and $PSCmdlet.ShouldProcess($SourceDirectory,'Remove the TWO legacy vault files; cloud history/backups still require rotation')) {
        Remove-Item -LiteralPath $envelopeFile,$wrappingFile -Force
        Write-Host 'Removed worker-secrets.enc.json and wrapping-key.dpapi. Recovery is from the verified external vault, not these deleted files.'
    }
} finally {
    if ($aes) { $aes.Dispose() }; if ($rsa) { $rsa.Dispose() }
    foreach ($bytes in @($private,$key,$plain)) { if ($bytes) { [Array]::Clear($bytes) } }
    $legacy=$verified=$null
}
