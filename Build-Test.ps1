param(
    [ValidateSet('1.21.1','1.21.4','1.21.8','1.21.11','26.1','26.1.1','26.1.2')]
    [string]$MinecraftVersion = '1.21.11'
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'Visual Studio C++ Build Tools are required.' }
Push-Location $PSScriptRoot
try {
    $dll = Join-Path $PSScriptRoot "build\local-test\$MinecraftVersion\Onestep-TEST.dll"
    if (Test-Path -LiteralPath $dll) {
        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
        Move-Item -LiteralPath $dll -Destination "$dll.previous-$stamp"
    }
    & $msbuild 'OneStep.vcxproj' /m /t:Build /nologo /v:minimal /p:Configuration=Debug /p:Platform=x64 "/p:MinecraftVersion=$MinecraftVersion" /p:LocalTestDll=true
    if ($LASTEXITCODE -ne 0) { throw "Test DLL build failed ($LASTEXITCODE)." }
    if (!(Test-Path -LiteralPath $dll)) { throw 'Test DLL was not produced.' }
    Write-Host "Local test DLL (Fabric $MinecraftVersion): $dll"
} finally { Pop-Location }
