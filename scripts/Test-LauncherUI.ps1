param([string]$Launcher='', [string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$launcherUiRoot=Split-Path -Parent $PSScriptRoot
if (-not $Launcher) { $Launcher=Join-Path $launcherUiRoot 'build\launcher-reference\Onestep-Launcher.exe' }
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $launcherUiRoot 'build\launcher-reference\previews' }
$Launcher=(Resolve-Path -LiteralPath $Launcher).Path
$OutputDirectory=[System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
function Invoke-LauncherUiOffline([string[]]$UiArguments) {
    $launcherUiProcess=Start-Process -FilePath $Launcher -ArgumentList $UiArguments -WindowStyle Hidden -Wait -PassThru
    if ($launcherUiProcess.ExitCode -ne 0) { throw "Launcher offline UI check failed ($($launcherUiProcess.ExitCode)): $UiArguments" }
}
$launcherUiReport=Join-Path $OutputDirectory 'ui-test.txt'
Invoke-LauncherUiOffline @('--ui-test',('"'+$launcherUiReport+'"'))
Get-Content -LiteralPath $launcherUiReport
foreach ($launcherUiScale in @('1','1.25','1.5')) {
    foreach ($launcherUiMode in @('login','session')) {
        $launcherUiSuffix=if ($launcherUiScale -eq '1') { '' } else { '-'+$launcherUiScale+'x' }
        $launcherUiPath=Join-Path $OutputDirectory ($launcherUiMode+$launcherUiSuffix+'.png')
        Invoke-LauncherUiOffline @('--preview',('"'+$launcherUiPath+'"'),$launcherUiMode,$launcherUiScale)
    }
}
foreach ($launcherUiMode in @('working','download','launching','done','error','login-filled','login-revealed','session-long','session-empty')) {
    $launcherUiPath=Join-Path $OutputDirectory ($launcherUiMode+'.png')
    Invoke-LauncherUiOffline @('--preview',('"'+$launcherUiPath+'"'),$launcherUiMode,'1')
}
Add-Type -AssemblyName System.Drawing
foreach ($launcherUiMode in @('login-filled','login-revealed')) {
    $launcherUiBitmap=[System.Drawing.Bitmap]::new((Join-Path $OutputDirectory ($launcherUiMode+'.png')))
    try {
        $launcherUiBrightPixels=0
        for ($launcherUiY=158;$launcherUiY -lt 180;$launcherUiY++) {
            for ($launcherUiX=448;$launcherUiX -lt 668;$launcherUiX++) {
                $launcherUiPixel=$launcherUiBitmap.GetPixel($launcherUiX,$launcherUiY)
                if ($launcherUiPixel.R -gt 180 -and $launcherUiPixel.G -gt 180 -and $launcherUiPixel.B -gt 180) { $launcherUiBrightPixels++ }
            }
        }
        if ($launcherUiBrightPixels -lt 20) { throw "Real EDIT print output is missing: $launcherUiMode" }
        Write-Host ("PASS $launcherUiMode EDIT print output is visible")
    } finally { $launcherUiBitmap.Dispose() }
}
Write-Host ('Offline GDI+/native EDIT previews: '+$OutputDirectory)
