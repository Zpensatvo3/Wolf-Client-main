#requires -Version 7.0
# Password-based operator vault. CurrentUser DPAPI alone does not isolate agents
# running under the same account. Unlock only from a trusted operator session.
function Get-OnestepAuthVaultDirectory {
    param([string]$Directory)
    if (-not $Directory) { $Directory=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OneStep\OperatorVault' }
    $resolved=[IO.Path]::GetFullPath($Directory)
    $workspace=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
    $broadPaths=@([IO.Path]::GetPathRoot($resolved),[IO.Path]::GetTempPath(),
        [Environment]::GetFolderPath('UserProfile'),[Environment]::GetFolderPath('LocalApplicationData'),
        [Environment]::GetFolderPath('ApplicationData'),[Environment]::GetFolderPath('CommonApplicationData'),
        [Environment]::GetFolderPath('Windows'))
    if ($broadPaths | Where-Object {$_ -and $resolved.TrimEnd('\','/').Equals($_.TrimEnd('\','/'),[StringComparison]::OrdinalIgnoreCase)}) {
        throw 'Choose a dedicated operator-vault subdirectory, never a drive/profile/system root.'
    }
    if ($resolved.Equals($workspace,[StringComparison]::OrdinalIgnoreCase) -or
        $resolved.StartsWith($workspace+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        $resolved -match '(?i)(^|[\\/])OneDrive(?: - [^\\/]+)?([\\/]|$)') {
        throw 'Operator vault must be outside the project and OneDrive. Use a separate operator account for production.'
    }
    for ($item=$resolved; $item; $item=[IO.Path]::GetDirectoryName($item)) {
        if ((Test-Path -LiteralPath $item) -and ((Get-Item -LiteralPath $item -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Operator vault paths cannot use junctions, symbolic links or cloud placeholders.'
        }
    }
    return $resolved
}

function ConvertTo-OnestepPasswordBytes {
    param([Parameter(Mandatory)][Security.SecureString]$Password)
    if ($Password.Length -lt 16) { throw 'Use an independent operator passphrase of at least 16 characters.' }
    $pointer=[IntPtr]::Zero; $characters=[char[]]::new($Password.Length)
    try {
        $pointer=[Runtime.InteropServices.Marshal]::SecureStringToGlobalAllocUnicode($Password)
        [Runtime.InteropServices.Marshal]::Copy($pointer,$characters,0,$characters.Length)
        return ,([Text.Encoding]::UTF8.GetBytes($characters))
    } finally {
        [Array]::Clear($characters)
        if ($pointer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeGlobalAllocUnicode($pointer) }
    }
}

function Protect-OnestepVaultDirectory {
    param([Parameter(Mandatory)][string]$Directory)
    if (-not $IsWindows) { throw 'The operator vault requires Windows ACL support.' }
    $Directory=Get-OnestepAuthVaultDirectory $Directory
    if ((Test-Path -LiteralPath $Directory) -and
        (Get-ChildItem -LiteralPath $Directory -Force | Measure-Object).Count -gt 0 -and
        -not (Test-Path -LiteralPath (Join-Path $Directory 'operator-vault.v2.json'))) {
        throw 'Refusing to change permissions on a nonempty unrelated directory.'
    }
    [IO.Directory]::CreateDirectory($Directory) | Out-Null
    $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User
    $acl=[Security.AccessControl.DirectorySecurity]::new(); $acl.SetOwner($sid); $acl.SetAccessRuleProtection($true,$false)
    foreach ($identity in @($sid,[Security.Principal.SecurityIdentifier]::new('S-1-5-18'))) {
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($identity,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))
    }
    Set-Acl -LiteralPath $Directory -AclObject $acl
}

function Write-OnestepAuthVault {
    param([Parameter(Mandatory)]$Payload,[Parameter(Mandatory)][Security.SecureString]$Password,[string]$Directory)
    $Directory=Get-OnestepAuthVaultDirectory $Directory
    $target=Join-Path $Directory 'operator-vault.v2.json'
    if (Test-Path -LiteralPath $target) { throw 'Operator vault already exists; it will not be overwritten.' }
    Protect-OnestepVaultDirectory $Directory
    $passwordBytes=$key=$plain=$aes=$null
    $temporary=Join-Path $Directory ([Guid]::NewGuid().ToString('N')+'.tmp')
    try {
        if ($Payload.admin_token -or $Payload.owner_license -or -not $Payload.secrets) {
            throw 'Keep admin and license credentials in a separate password manager, not in the production-secrets vault.'
        }
        $passwordBytes=ConvertTo-OnestepPasswordBytes $Password
        $salt=[Security.Cryptography.RandomNumberGenerator]::GetBytes(32); $iv=[Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
        $key=[Security.Cryptography.Rfc2898DeriveBytes]::Pbkdf2($passwordBytes,$salt,600000,[Security.Cryptography.HashAlgorithmName]::SHA256,32)
        $plain=[Text.Encoding]::UTF8.GetBytes(($Payload | ConvertTo-Json -Depth 12 -Compress))
        if ($plain.Length -gt 1048576) { throw 'Operator vault payload exceeds the limit.' }
        $cipher=[byte[]]::new($plain.Length); $tag=[byte[]]::new(16); $aes=[Security.Cryptography.AesGcm]::new($key,16)
        $aes.Encrypt($iv,$plain,$cipher,$tag,[Text.Encoding]::UTF8.GetBytes('onestep-operator-vault-v2'))
        $envelope=@{version=2;kdf='PBKDF2-SHA256';iterations=600000;salt=[Convert]::ToBase64String($salt);
            iv=[Convert]::ToBase64String($iv);tag=[Convert]::ToBase64String($tag);ciphertext=[Convert]::ToBase64String($cipher)}
        [IO.File]::WriteAllText($temporary,($envelope | ConvertTo-Json -Compress)); [IO.File]::Move($temporary,$target,$false)
    } finally {
        if ($aes) { $aes.Dispose() }
        foreach ($bytes in @($passwordBytes,$key,$plain)) { if ($bytes) { [Array]::Clear($bytes) } }
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Read-OnestepAuthVault {
    param([string]$Directory,[Security.SecureString]$Password)
    $Directory=Get-OnestepAuthVaultDirectory $Directory
    $target=Join-Path $Directory 'operator-vault.v2.json'
    if (-not (Test-Path -LiteralPath $target)) { throw 'Legacy project/DPAPI vaults are not opened automatically. See server/AUTH_HARDENING.md.' }
    if ((Get-Item -LiteralPath $target).Length -gt 2097152) { throw 'Operator vault envelope exceeds the limit.' }
    $envelope=Get-Content -LiteralPath $target -Raw | ConvertFrom-Json
    if ($envelope.version -ne 2 -or $envelope.kdf -ne 'PBKDF2-SHA256' -or $envelope.iterations -ne 600000) { throw 'Unsupported operator vault format.' }
    if (-not $Password) { $Password=Read-Host 'Operator vault passphrase (never paste into chat)' -AsSecureString }
    $passwordBytes=$key=$plain=$aes=$null
    try {
        $passwordBytes=ConvertTo-OnestepPasswordBytes $Password
        $salt=[Convert]::FromBase64String($envelope.salt); $iv=[Convert]::FromBase64String($envelope.iv)
        $tag=[Convert]::FromBase64String($envelope.tag); $cipher=[Convert]::FromBase64String($envelope.ciphertext)
        if ($salt.Length -ne 32 -or $iv.Length -ne 12 -or $tag.Length -ne 16 -or $cipher.Length -gt 1048576) { throw 'Invalid operator vault envelope.' }
        $key=[Security.Cryptography.Rfc2898DeriveBytes]::Pbkdf2($passwordBytes,$salt,600000,[Security.Cryptography.HashAlgorithmName]::SHA256,32)
        $plain=[byte[]]::new($cipher.Length); $aes=[Security.Cryptography.AesGcm]::new($key,16)
        $aes.Decrypt($iv,$cipher,$tag,$plain,[Text.Encoding]::UTF8.GetBytes('onestep-operator-vault-v2'))
        return ([Text.Encoding]::UTF8.GetString($plain) | ConvertFrom-Json)
    } finally {
        if ($aes) { $aes.Dispose() }
        foreach ($bytes in @($passwordBytes,$key,$plain)) { if ($bytes) { [Array]::Clear($bytes) } }
    }
}
