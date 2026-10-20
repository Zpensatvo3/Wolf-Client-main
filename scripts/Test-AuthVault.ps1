#requires -Version 7.0
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AuthVault.ps1')
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('onestep-vault-test-'+[Guid]::NewGuid().ToString('N'))
$password=ConvertTo-SecureString ('Local-test-only-'+[Guid]::NewGuid().ToString('N')) -AsPlainText -Force
$wrong=ConvertTo-SecureString ('Wrong-test-only-'+[Guid]::NewGuid().ToString('N')) -AsPlainText -Force
function Expect-Failure([scriptblock]$Operation) {
    $failed=$false
    try { & $Operation | Out-Null } catch {$failed=$true}
    if (-not $failed) {throw 'Expected vault policy failure did not occur.'}
}
try {
    # Entirely synthetic material. Never reads the real operator/legacy vault.
    $fake='fake-local-secret-'+[Guid]::NewGuid().ToString('N')
    Write-OnestepAuthVault -Directory $testRoot -Password $password -Payload @{secrets=@{TEST_SECRET=$fake}}
    $read=Read-OnestepAuthVault -Directory $testRoot -Password $password
    if ($read.secrets.TEST_SECRET -cne $fake) {throw 'Vault roundtrip failed.'}
    $file=Join-Path $testRoot 'operator-vault.v2.json'
    if ((Get-Content -LiteralPath $file -Raw).Contains($fake)) {throw 'Secret found outside ciphertext.'}
    Expect-Failure {Read-OnestepAuthVault -Directory $testRoot -Password $wrong}
    Expect-Failure {Write-OnestepAuthVault -Directory $testRoot -Password $password -Payload @{secrets=@{TEST_SECRET=$fake}}}
    Expect-Failure {Get-OnestepAuthVaultDirectory (Join-Path (Split-Path -Parent $PSScriptRoot) 'server/.credentials')}
    Expect-Failure {Get-OnestepAuthVaultDirectory (Join-Path ([IO.Path]::GetTempPath()) 'OneDrive/operator')}
    Expect-Failure {Read-OnestepAuthVault -Directory (Join-Path $testRoot 'missing') -Password $password}
    Expect-Failure {Get-OnestepAuthVaultDirectory ([IO.Path]::GetTempPath())}
    if (-not (Get-Acl -LiteralPath $testRoot).AreAccessRulesProtected) {throw 'Vault ACL inheritance was not removed.'}
    # Prove the explicit migration with disposable v1 data, not the real vault.
    $legacyRoot=Join-Path $testRoot 'legacy-fixture'; [IO.Directory]::CreateDirectory($legacyRoot) | Out-Null
    $migrationRoot=Join-Path $testRoot 'migration-fixture'
    $rsa=[Security.Cryptography.RSA]::Create(2048)
    $legacyKey=[Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
    $legacyIv=[Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
    $legacyPlain=[Text.Encoding]::UTF8.GetBytes((@{secrets=@{TEST_SECRET=$fake};admin_token='fake-test-admin';owner_license='fake-test-license'} | ConvertTo-Json -Compress))
    $legacyCipher=[byte[]]::new($legacyPlain.Length); $legacyTag=[byte[]]::new(16)
    $legacyAes=[Security.Cryptography.AesGcm]::new($legacyKey,16)
    try {
        $legacyAes.Encrypt($legacyIv,$legacyPlain,$legacyCipher,$legacyTag)
        [IO.File]::WriteAllBytes((Join-Path $legacyRoot 'wrapping-key.dpapi'),
            [Security.Cryptography.ProtectedData]::Protect($rsa.ExportPkcs8PrivateKey(),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser))
        $legacyEnvelope=@{iv=[Convert]::ToBase64String($legacyIv);ciphertext=[Convert]::ToBase64String([byte[]]($legacyCipher+$legacyTag));
            wrapped_key=[Convert]::ToBase64String($rsa.Encrypt($legacyKey,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256))}
        [IO.File]::WriteAllText((Join-Path $legacyRoot 'worker-secrets.enc.json'),($legacyEnvelope | ConvertTo-Json -Compress))
        & (Join-Path $PSScriptRoot 'Import-LegacyAuthVault.ps1') -AcknowledgeExposure -SourceDirectory $legacyRoot -Directory $migrationRoot -Password $password -Confirm:$false
        $migrated=Read-OnestepAuthVault -Directory $migrationRoot -Password $password
        if ($migrated.secrets.TEST_SECRET -cne $fake -or $migrated.admin_token -or $migrated.owner_license) {throw 'Migration retained credentials or lost secrets.'}
        if (-not (Test-Path -LiteralPath (Join-Path $legacyRoot 'worker-secrets.enc.json'))) {throw 'Migration unexpectedly removed legacy fixture.'}
    } finally {
        $legacyAes.Dispose(); $rsa.Dispose(); [Array]::Clear($legacyKey); [Array]::Clear($legacyPlain)
    }
    $envelope=Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
    $cipher=[Convert]::FromBase64String($envelope.ciphertext); $cipher[0]=$cipher[0] -bxor 1
    $envelope.ciphertext=[Convert]::ToBase64String($cipher)
    [IO.File]::WriteAllText($file,($envelope | ConvertTo-Json -Compress))
    Expect-Failure {Read-OnestepAuthVault -Directory $testRoot -Password $password}
    Write-Host 'PASS external vault, ACLs, password, ciphertext integrity, overwrite and legacy/cloud-path rejection.'
} finally {
    $password.Dispose(); $wrong.Dispose()
    # Exact disposable test files only; never recursively delete a vault/root.
    $testFile=Join-Path $testRoot 'operator-vault.v2.json'
    if (Test-Path -LiteralPath $testFile) {Remove-Item -LiteralPath $testFile -Force}
    foreach ($relative in @('legacy-fixture/worker-secrets.enc.json','legacy-fixture/wrapping-key.dpapi','migration-fixture/operator-vault.v2.json')) {
        $fixture=Join-Path $testRoot $relative
        if (Test-Path -LiteralPath $fixture) {Remove-Item -LiteralPath $fixture -Force}
    }
    foreach ($relative in @('legacy-fixture','migration-fixture')) {
        $fixture=Join-Path $testRoot $relative
        if (Test-Path -LiteralPath $fixture) {Remove-Item -LiteralPath $fixture -Force}
    }
    if (Test-Path -LiteralPath $testRoot) {Remove-Item -LiteralPath $testRoot -Force}
}
