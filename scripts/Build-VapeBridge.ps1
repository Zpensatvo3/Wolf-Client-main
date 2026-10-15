$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dependencies = Join-Path $root 'build\vape-deps'
$classes = Join-Path $root 'build\vape-java'
[void](New-Item -ItemType Directory -Force -Path $dependencies,$classes)
foreach ($artifact in @('asm','asm-tree')) {
    $jar = Join-Path $dependencies "$artifact-9.7.1.jar"
    if (-not (Test-Path -LiteralPath $jar)) {
        Invoke-WebRequest "https://maven.fabricmc.net/org/ow2/asm/$artifact/9.7.1/$artifact-9.7.1.jar" -OutFile $jar
    }
}
$javac = (Get-Command javac -ErrorAction Stop).Source
& $javac --release 17 -encoding UTF-8 -cp "$dependencies\asm-9.7.1.jar;$dependencies\asm-tree-9.7.1.jar" -d $classes (Join-Path $root 'src\java_hook\vape\NativeBridge.java')
if ($LASTEXITCODE -ne 0) { throw 'Vape Java bridge compilation failed.' }
