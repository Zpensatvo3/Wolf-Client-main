param([string]$AsmClasspath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$AsmClasspath) {
    $libraryRoot = Join-Path $env:APPDATA 'ModrinthApp\meta\libraries\org\ow2\asm'
    $asmJar = Get-ChildItem -LiteralPath (Join-Path $libraryRoot 'asm') -Filter 'asm-*.jar' -Recurse | Sort-Object { [version]$_.Directory.Name } -Descending | Select-Object -First 1
    $treeJar = Get-ChildItem -LiteralPath (Join-Path $libraryRoot 'asm-tree') -Filter 'asm-tree-*.jar' -Recurse | Sort-Object { [version]$_.Directory.Name } -Descending | Select-Object -First 1
    if (!$asmJar -or !$treeJar) { throw 'Pass -AsmClasspath with ASM and ASM Tree 9.x jars.' }
    $AsmClasspath = $asmJar.FullName + ';' + $treeJar.FullName
}
$classRoot = Join-Path $projectRoot 'assets\nameprotect'
New-Item -ItemType Directory -Path $classRoot -Force | Out-Null
& javac --release 17 -encoding UTF-8 -g:none -cp $AsmClasspath -d $classRoot (Join-Path $projectRoot 'src\bridge\NameProtectBridge.java') (Join-Path $projectRoot 'src\bridge\NameProtectTransformer.java')
if ($LASTEXITCODE -ne 0) { throw 'NameProtect bridge compilation failed.' }
