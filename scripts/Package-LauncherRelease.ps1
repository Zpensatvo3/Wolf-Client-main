param([string]$Output = '')
$ErrorActionPreference='Stop'
$packageRoot=Split-Path -Parent $PSScriptRoot
$packageDll=Join-Path $packageRoot 'build/launcher-v2/1.21.11/Onestep.dll'
$packageExe=Join-Path $packageRoot 'build/launcher-v2/launcher/Onestep-Launcher.exe'
if(-not $Output){$Output=Join-Path $packageRoot 'build/launcher-v2/OneStep-Launcher-v2.1-1.21.11.zip'}
if(Test-Path -LiteralPath $Output){throw 'Output already exists; choose a new archive path.'}
foreach($packageFile in @($packageDll,$packageExe)){
    $packageInfo=Get-Item -LiteralPath $packageFile -ErrorAction Stop
    if($packageInfo.PSIsContainer -or $packageInfo.Length -lt 4096 -or
        ($packageInfo.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Invalid release artifact.'}
}
# Exact whitelist: never archive build directories, sources, test probes or vaults.
Compress-Archive -LiteralPath @($packageDll,$packageExe) -DestinationPath $Output -CompressionLevel Optimal
Add-Type -AssemblyName System.IO.Compression.FileSystem
$packageZip=[IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($Output))
try{
    $packageNames=@($packageZip.Entries.FullName | Sort-Object)
    if($packageNames.Count -ne 2 -or $packageNames[0] -ne 'Onestep-Launcher.exe' -or $packageNames[1] -ne 'Onestep.dll'){
        throw 'Unexpected archive entries; do not distribute.'
    }
}finally{$packageZip.Dispose()}
Get-FileHash -Algorithm SHA256 -LiteralPath $packageDll,$packageExe,$Output | Select-Object Path,Hash
