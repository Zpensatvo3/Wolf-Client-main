$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$releaseBuilder = Join-Path $PSScriptRoot 'Build-Release.ps1'
$output = Join-Path $projectRoot 'build\minecraft\1.21.11\Onestep.dll'
$backup = $null

# Keep MSBuild's per-target .obj/PCH files so it can compile only stale sources.
# Moving the DLL forces a fresh link while leaving the incremental build cache intact.
if (Test-Path -LiteralPath $output) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $backup = "$output.previous-$stamp"
    Move-Item -LiteralPath $output -Destination $backup
}

try {
    & $releaseBuilder -MinecraftVersion '1.21.11' -SkipSign
}
catch {
    if ($backup -and (Test-Path -LiteralPath $backup)) {
        if (Test-Path -LiteralPath $output) {
            $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
            Move-Item -LiteralPath $output -Destination "$output.failed-$stamp"
        }
        Move-Item -LiteralPath $backup -Destination $output
        Write-Warning "Derleme başarısız; önceki DLL geri yüklendi: $output"
    }
    throw
}

if ($backup) {
    Write-Host "Önceki DLL yedek olarak korundu: $backup"
}
