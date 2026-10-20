param([switch]$Publish, [string]$Version = '')
$ErrorActionPreference = 'Stop'
$launcherRoot = Split-Path -Parent $PSScriptRoot
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    $finder = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $finder) {
        $msbuild = & $finder -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    }
}
if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild)) { throw 'Visual Studio C++ Build Tools bulunamadi.' }
if ($Publish -and -not $Version) { throw 'Yayin icin benzersiz bir Version belirt.' }
if (-not (Test-Path -LiteralPath (Join-Path $launcherRoot 'server\AuthDeployment.props'))) { throw 'Auth deployment ayarlari eksik.' }
$clientOutput = Join-Path $launcherRoot 'build\launcher-client\'
$clientObjects = Join-Path $launcherRoot 'build\etc\launcher-client\'
& $msbuild (Join-Path $launcherRoot 'OneStep.vcxproj') /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:MinecraftVersion=1.21.11 /p:AuthRequired=true "/p:OutDir=$clientOutput" "/p:IntDir=$clientObjects" /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Launcher DLL derlemesi basarisiz.' }
& $msbuild (Join-Path $launcherRoot 'launcher\launcher.vcxproj') /m /t:Build /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Launcher EXE derlemesi basarisiz.' }
if ($Publish) { & (Join-Path $PSScriptRoot 'Publish-LauncherRelease.ps1') -Dll (Join-Path $clientOutput 'Onestep.dll') -Version $Version }
Write-Host ('Hazir: ' + (Join-Path $launcherRoot 'build\launcher\Onestep-Launcher.exe'))
