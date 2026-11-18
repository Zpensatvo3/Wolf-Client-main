$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$testDirectory = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$previousCache = $env:SCCACHE_PATH
$previousCompiler = $env:ONESTEP_REAL_CL
$previousExit = $env:ONESTEP_TEST_EXIT
try {
    $csharpCompiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    $launcher = Join-Path $testDirectory 'launcher.exe'
    $echoProgram = Join-Path $testDirectory 'argument echo.exe'
    & $csharpCompiler /nologo /target:exe "/out:$launcher" (Join-Path $PSScriptRoot 'CompilerCache.cs')
    if ($LASTEXITCODE) { throw 'Launcher compilation failed.' }
    & $csharpCompiler /nologo /target:exe "/out:$echoProgram" (Join-Path $projectRoot 'tests\build_cache\ArgumentEcho.cs')
    if ($LASTEXITCODE) { throw 'Argument fixture compilation failed.' }
    $env:SCCACHE_PATH = $echoProgram
    $env:ONESTEP_REAL_CL = 'C:\compiler with spaces\cl.exe'
    $arguments = @('', 'plain', 'with spaces', 'embedded"quote', 'C:\folder with space\',
                   'backslash\"quote', '@response file.rsp', 'Türkçe_日本語', 'literal$()`text')
    $expected = @($env:ONESTEP_REAL_CL) + $arguments
    $lines = @(& $launcher @arguments)
    if ($LASTEXITCODE -ne 0 -or $lines.Count -ne $expected.Count) { throw 'Launcher argument count or status mismatch.' }
    for ($index = 0; $index -lt $expected.Count; $index++) {
        $actual = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($lines[$index]))
        if ($actual -cne $expected[$index]) { throw "Launcher argument mismatch at $index" }
    }
    $env:ONESTEP_TEST_EXIT = '37'
    & $launcher 'exit-test' | Out-Null
    if ($LASTEXITCODE -ne 37) { throw 'Compiler exit code was not forwarded.' }
    $env:SCCACHE_PATH = $null
    & $launcher 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) { throw 'Missing cache configuration must fail.' }
    Write-Host 'PASS compiler cache launcher: argument quoting, Unicode, exit status and missing configuration'
}
finally {
    $env:SCCACHE_PATH = $previousCache
    $env:ONESTEP_REAL_CL = $previousCompiler
    $env:ONESTEP_TEST_EXIT = $previousExit
    Remove-Item -LiteralPath $testDirectory -Recurse -Force
}
exit 0
