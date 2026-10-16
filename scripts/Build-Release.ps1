param(
    [switch]$Clean,
    [switch]$SkipSign,
    [string]$PlatformToolset = '',
    [switch]$RequireAuth,
    [string]$AuthServerDomain = '',
    [string]$AuthPublicKeySpkiBase64 = '',
    [switch]$ProtectWithVMProtect,
    [string]$VMProtectPath = '',
    [string]$VMProtectProject = ''
)

$ErrorActionPreference = 'Stop'
if ($ProtectWithVMProtect -and (-not $VMProtectProject -or -not (Test-Path -LiteralPath $VMProtectProject))) {
    throw 'VMProtect icin GUI ile hazirlanmis .vmp projesini VMProtectProject ile belirtin.'
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'custom-base.vcxproj'
$msbuildCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
)
$msbuild = $msbuildCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $msbuild) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
            Select-Object -First 1
    }
}
if (-not $msbuild) {
    throw 'MSBuild bulunamadi. Visual Studio C++ Build Tools kurulumunu kontrol et.'
}

$target = if ($Clean) { 'Rebuild' } else { 'Build' }
$buildArgs = @($project, '/m', "/t:$target", '/p:Configuration=Release', '/p:Platform=x64', '/v:minimal', '/nologo')
if ($PlatformToolset) { $buildArgs += "/p:PlatformToolset=$PlatformToolset" }
if ($RequireAuth) { $buildArgs += '/p:AuthRequired=true' }
if ($AuthServerDomain) { $buildArgs += "/p:AuthServerDomain=$AuthServerDomain" }
if ($AuthPublicKeySpkiBase64) { $buildArgs += "/p:AuthPublicKeySpkiBase64=$AuthPublicKeySpkiBase64" }
if ($ProtectWithVMProtect) { $buildArgs += '/p:AuthProtectionMap=true' }
& $msbuild @buildArgs
if ($LASTEXITCODE -ne 0) {
    throw "Release derlemesi basarisiz oldu ($LASTEXITCODE)."
}

$output = Join-Path $projectRoot 'build\Onestep.dll'
if (-not (Test-Path -LiteralPath $output)) {
    throw "Derleme ciktisi bulunamadi: $output"
}

if ($ProtectWithVMProtect) {
    $authProtectOutput = & (Join-Path $PSScriptRoot 'Protect-Release.ps1') -InputDll $output `
        -ProjectFile $VMProtectProject -VMProtectPath $VMProtectPath
    $output = @($authProtectOutput)[-1]
}

if (-not $SkipSign) {
    $certificate = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq 'CN=Thedorius Lite Test' -and $_.HasPrivateKey } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1
    if ($certificate) {
        $signature = Set-AuthenticodeSignature -FilePath $output -Certificate $certificate -HashAlgorithm SHA256
        Write-Host "Imza: $($signature.Status)"
    }
    else {
        Write-Warning 'Thedorius Lite test sertifikasi bulunamadi; DLL imzalanmadan birakildi.'
    }
}

$file = Get-Item -LiteralPath $output
Write-Host ("Hazir: {0} ({1:N2} MB)" -f $file.FullName, ($file.Length / 1MB))
