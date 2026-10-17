param([string]$AsmClasspath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$AsmClasspath = & (Join-Path $PSScriptRoot 'Resolve-AsmClasspath.ps1') -AsmClasspath $AsmClasspath
$classRoot = Join-Path $projectRoot 'assets\nameprotect'
New-Item -ItemType Directory -Path $classRoot -Force | Out-Null
& javac --release 17 -encoding UTF-8 -g:none -cp $AsmClasspath -d $classRoot (Join-Path $projectRoot 'src\bridge\NameProtectBridge.java') (Join-Path $projectRoot 'src\bridge\NameProtectTransformer.java')
if ($LASTEXITCODE -ne 0) { throw 'NameProtect bridge compilation failed.' }
