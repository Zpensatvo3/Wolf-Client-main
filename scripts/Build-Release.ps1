param(
    [ValidateSet('1.21.1', '1.21.4', '1.21.8', '1.21.11', '26.1', '26.1.1', '26.1.2')]
    [string]$MinecraftVersion = '1.21.1',
    [switch]$AllMinecraftVersions,
    [switch]$Clean,
    [switch]$SkipSign,
    [string]$PlatformToolset = ''
)

$ErrorActionPreference = 'Stop'
if ($AllMinecraftVersions) {
    foreach ($minecraftTarget in @('1.21.1', '1.21.4', '1.21.8', '1.21.11', '26.1', '26.1.1', '26.1.2')) {
        $minecraftBuildOptions = @{} + $PSBoundParameters
        $minecraftBuildOptions.Remove('AllMinecraftVersions')
        $minecraftBuildOptions['MinecraftVersion'] = $minecraftTarget
        & $PSCommandPath @minecraftBuildOptions
    }
    return
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'OneStep.vcxproj'
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
$buildArgs += "/p:MinecraftVersion=$MinecraftVersion"
if ($PlatformToolset) { $buildArgs += "/p:PlatformToolset=$PlatformToolset" }
& $msbuild @buildArgs
if ($LASTEXITCODE -ne 0) { throw "Release build failed ($LASTEXITCODE)." }

$minecraftOutput = if ($MinecraftVersion -eq '1.21.1') { 'build\Onestep.dll' } else { "build\minecraft\$MinecraftVersion\Onestep.dll" }
$output = Join-Path $projectRoot $minecraftOutput
if (-not (Test-Path -LiteralPath $output)) {
    throw "Derleme ciktisi bulunamadi: $output"
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
Write-Host ("Fabric {0}: {1} ({2:N2} MB)" -f $MinecraftVersion, $file.FullName, ($file.Length / 1MB))
