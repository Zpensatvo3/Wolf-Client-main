$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$classRoot = Join-Path $projectRoot 'assets\hybrid'
New-Item -ItemType Directory -Path $classRoot -Force | Out-Null
& javac --release 17 -encoding UTF-8 -g:none -d $classRoot `
    (Join-Path $projectRoot 'src\bridge\GameBridge.java') `
    (Join-Path $projectRoot 'src\bridge\UtilityBridge.java') `
    (Join-Path $projectRoot 'src\bridge\ItemBridge.java') `
    (Join-Path $projectRoot 'src\bridge\InventoryBridge.java') `
    (Join-Path $projectRoot 'src\bridge\WorldBridge.java') `
    (Join-Path $projectRoot 'src\bridge\ClientBridge.java')
if ($LASTEXITCODE -ne 0) { throw 'GameBridge compilation failed.' }
Write-Host 'Built six original OneStep hybrid bridges (no Minecraft/Fabric compile dependency).'
