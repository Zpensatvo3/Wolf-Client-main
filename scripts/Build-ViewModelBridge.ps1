param([string]$AsmClasspath)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$AsmClasspath=& (Join-Path $PSScriptRoot 'Resolve-AsmClasspath.ps1') -AsmClasspath $AsmClasspath
$classRoot=Join-Path $projectRoot 'assets\viewmodel'
New-Item -ItemType Directory -Path $classRoot -Force | Out-Null
& javac --release 17 -encoding UTF-8 -g:none -cp $AsmClasspath -d $classRoot (Join-Path $projectRoot 'src\bridge\ViewModelBridge.java') (Join-Path $projectRoot 'src\bridge\ViewModelTransformer.java')
if ($LASTEXITCODE -ne 0) { throw 'View Model bridge compilation failed.' }
