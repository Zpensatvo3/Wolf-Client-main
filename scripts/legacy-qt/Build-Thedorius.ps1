param(
    [string]$QtRoot = $env:THEDORIUS_QT_ROOT,
    [switch]$Clean,
    [switch]$SkipSign,
    [switch]$AllowSharedQt,
    [switch]$MockOnly
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$uiRoot = Join-Path $root 'ui'
$uiBuild = Join-Path $root 'out\qt-release'
$dist = Join-Path $root 'dist'

if (-not $MockOnly) {
    & (Join-Path $PSScriptRoot 'Build-Release.ps1') -Clean:$Clean -SkipSign:$SkipSign
}

if (-not $QtRoot) {
    throw 'Qt 6.11.1 bulunamadi. THEDORIUS_QT_ROOT degerini statik MSVC 2022 x64 Qt klasorune ayarla.'
}
$qtConfig = Join-Path $QtRoot 'lib\cmake\Qt6\Qt6Config.cmake'
if (-not (Test-Path -LiteralPath $qtConfig)) {
    throw "Qt6Config.cmake bulunamadi: $qtConfig"
}

if ($Clean -and (Test-Path -LiteralPath $uiBuild)) {
    $resolved = [IO.Path]::GetFullPath($uiBuild)
    if (-not $resolved.StartsWith([IO.Path]::GetFullPath($root), [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Qt build klasoru proje disinda; temizleme iptal edildi.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}

$requireStatic = if ($AllowSharedQt) { 'OFF' } else { 'ON' }
cmake -S $uiRoot -B $uiBuild -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH=$QtRoot `
    -DTHEDORIUS_REQUIRE_STATIC_QT=$requireStatic
if ($LASTEXITCODE -ne 0) { throw 'Qt CMake yapilandirmasi basarisiz.' }

cmake --build $uiBuild --config Release
if ($LASTEXITCODE -ne 0) { throw 'Thedorius.exe derlemesi basarisiz.' }

$exe = Join-Path $dist 'Thedorius.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "EXE bulunamadi: $exe" }

if (-not $SkipSign) {
    $certificate = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq 'CN=Thedorius Lite Test' -and $_.HasPrivateKey } |
        Sort-Object NotAfter -Descending | Select-Object -First 1
    if ($certificate) {
        $signature = Set-AuthenticodeSignature -FilePath $exe -Certificate $certificate -HashAlgorithm SHA256
        Write-Host "EXE imzasi: $($signature.Status)"
    } else {
        Write-Warning 'Thedorius test sertifikasi bulunamadi; EXE imzalanmadi.'
    }
}

Get-ChildItem -LiteralPath $dist -File | Where-Object Name -ne 'Thedorius.exe' | ForEach-Object {
    Write-Warning "Tek EXE paketine dahil edilmeyen ek dosya: $($_.Name)"
}
Write-Host "Hazir: $exe"
