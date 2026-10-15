param(
    [switch]$Clean,
    [switch]$SkipSign
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Build-Release.ps1') -Clean:$Clean -SkipSign:$SkipSign

$msbuildCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
)
$msbuild = $msbuildCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild bulunamadi.' }

$project = Join-Path $root 'launcher\Thedorius-Launcher.vcxproj'
$target = if ($Clean) { 'Rebuild' } else { 'Build' }
& $msbuild $project /m /t:$target /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "Launcher derlemesi basarisiz oldu ($LASTEXITCODE)." }

$output = Join-Path $root 'dist\Thedorius.exe'
if (-not (Test-Path -LiteralPath $output)) { throw "Launcher bulunamadi: $output" }
if (-not $SkipSign) {
    $certificate = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq 'CN=Thedorius Lite Test' -and $_.HasPrivateKey } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1
    if ($certificate) {
        $signature = Set-AuthenticodeSignature -FilePath $output -Certificate $certificate -HashAlgorithm SHA256
        Write-Host "Launcher imzasi: $($signature.Status)"
    }
}
$file = Get-Item -LiteralPath $output
Write-Host ("Hazir: {0} ({1:N2} MB)" -f $file.FullName, ($file.Length / 1MB))
