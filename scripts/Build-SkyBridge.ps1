$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$classRoot = Join-Path $projectRoot 'assets\skybridge'
New-Item -ItemType Directory -Path $classRoot -Force | Out-Null
& javac --release 17 -encoding UTF-8 -g:none -d $classRoot (Join-Path $projectRoot 'src\bridge\SkyBridge.java')
if ($LASTEXITCODE -ne 0) { throw 'SkyBridge compilation failed.' }
