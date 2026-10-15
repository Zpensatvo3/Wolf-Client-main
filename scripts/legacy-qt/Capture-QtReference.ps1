param(
    [ValidateSet('modules','settings','search','visuals','configs','friends','hud','closed')]
    [string]$State = 'modules',
    [string]$Output
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'dist\Thedorius.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'dist\Thedorius.exe bulunamadi. Once Qt mock buildini al.' }
if (-not $Output) { $Output = Join-Path $root "artifacts\qt-$State.png" }
$outputPath = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null

$process = Start-Process -FilePath $exe -ArgumentList '--mock', "--state=$State" -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(12)
    do { Start-Sleep -Milliseconds 120; $process.Refresh() } while ($process.MainWindowHandle -eq 0 -and [DateTime]::UtcNow -lt $deadline)
    if ($process.MainWindowHandle -eq 0) { throw 'Qt mock penceresi acilmadi.' }
    Start-Sleep -Milliseconds 700
    Add-Type -AssemblyName System.Drawing
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ThedoriusWindowRect {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
}
'@
    $rect = New-Object ThedoriusWindowRect+RECT
    if (-not [ThedoriusWindowRect]::GetWindowRect($process.MainWindowHandle, [ref]$rect)) { throw 'Pencere olculeri okunamadi.' }
    $bitmap = New-Object Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size); $bitmap.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png) }
    finally { $graphics.Dispose(); $bitmap.Dispose() }
    Write-Host "Screenshot: $outputPath"
}
finally {
    if (-not $process.HasExited) { $process.CloseMainWindow() | Out-Null; if (-not $process.WaitForExit(1500)) { Stop-Process -Id $process.Id } }
}
