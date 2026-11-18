param(
    [ValidateSet('1.21.1', '1.21.4', '1.21.8', '1.21.11', '26.1', '26.1.1', '26.1.2')]
    [string]$MinecraftVersion = '1.21.1',
    [switch]$AllMinecraftVersions,
    [switch]$Clean,
    [switch]$SkipSign,
    [switch]$CompilerCache,
    [switch]$Timing,
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
$buildArgs += '/p:PreferredToolArchitecture=x64'

if ($CompilerCache) {
    $cacheCommand = if ($env:SCCACHE_PATH) { Get-Command $env:SCCACHE_PATH -CommandType Application -ErrorAction SilentlyContinue }
    if (-not $cacheCommand) {
        throw 'CompilerCache requires SCCACHE_PATH pointing to sccache.exe.'
    }
    $env:SCCACHE_PATH = $cacheCommand.Source
    $toolArguments = @($project, '/nologo', '/v:quiet', '/p:Configuration=Release', '/p:Platform=x64', '/getProperty:VCToolsInstallDir')
    if ($PlatformToolset) { $toolArguments += "/p:PlatformToolset=$PlatformToolset" }
    $toolsDirectory = (& $msbuild @toolArguments | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Could not resolve the selected MSVC toolchain.' }
    $env:ONESTEP_REAL_CL = Join-Path $toolsDirectory 'bin\Hostx64\x64\cl.exe'
    if (-not (Test-Path -LiteralPath $env:ONESTEP_REAL_CL)) { throw "MSVC compiler missing: $env:ONESTEP_REAL_CL" }
    $launcherDirectory = Join-Path $projectRoot 'build\tools\compiler-cache'
    New-Item -ItemType Directory -Force -Path $launcherDirectory | Out-Null
    $launcher = Join-Path $launcherDirectory 'Onestep.CompilerCache.exe'
    $csharpCompiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    & $csharpCompiler /nologo /optimize+ /target:exe "/out:$launcher" (Join-Path $PSScriptRoot 'CompilerCache.cs')
    if ($LASTEXITCODE -ne 0) { throw 'Compiler cache launcher build failed.' }
    # MSVC PCH invocations cannot be cached by sccache. Use one CL task per
    # source with MSBuild's bounded parallel task scheduler in this mode only.
    $buildArgs += @('/p:CompilerCache=true', '/p:UseMultiToolTask=true', '/p:EnforceProcessCountAcrossBuilds=true',
                    "/p:CL_MPCount=$([Environment]::ProcessorCount)", "/p:CLToolPath=$launcherDirectory\", '/p:CLToolExe=Onestep.CompilerCache.exe')
    & $env:SCCACHE_PATH --zero-stats
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize sccache statistics.' }
}
if ($Timing) {
    $logDirectory = Join-Path $projectRoot "build\logs\$MinecraftVersion"
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
    $buildArgs += @("/bl:$(Join-Path $logDirectory 'release.binlog')", '/clp:PerformanceSummary')
}
$buildWatch = [Diagnostics.Stopwatch]::StartNew()
try {
    & $msbuild @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Release build failed ($LASTEXITCODE)." }
}
finally {
    $buildWatch.Stop()
    Write-Host ("MSBuild elapsed: {0}" -f $buildWatch.Elapsed)
    if ($CompilerCache) { & $env:SCCACHE_PATH --show-stats }
}

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
