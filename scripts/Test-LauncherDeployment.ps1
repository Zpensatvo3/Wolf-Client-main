#requires -Version 7.0
param([Parameter(Mandatory)][string]$Probe)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AuthVault.ps1')
$probeFile = Get-Item -LiteralPath $Probe
$authVault = Read-OnestepAuthVault
$testLicense = $null
$base = 'https://onestep-auth.gogogamesas.workers.dev'
$headers = @{Authorization=('Bearer ' + $authVault.admin_token)}
try {
    $testLicense = Invoke-RestMethod -Method Post -Uri "$base/api/v1/admin/create-key" -Headers $headers `
        -ContentType 'application/json' -Body '{"days":"1"}'
    if (-not $testLicense.success -or -not $testLicense.key -or [int]$testLicense.id -le 0) { throw 'Temporary license could not be created.' }
    $start = [System.Diagnostics.ProcessStartInfo]::new($probeFile.FullName)
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardInput=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $process = [System.Diagnostics.Process]::Start($start)
    $process.StandardInput.WriteLine($testLicense.key); $process.StandardInput.Close()
    $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(120000)) { $process.Kill(); throw 'Launcher probe timed out.' }
    Write-Host $stdout.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 0) { throw $stderr.GetAwaiter().GetResult() }
}
finally {
    if ($testLicense -and [int]$testLicense.id -gt 0) {
        $removed = Invoke-RestMethod -Method Post -Uri "$base/api/v1/admin/delete-key" -Headers $headers `
            -ContentType 'application/json' -Body (@{id=[int]$testLicense.id} | ConvertTo-Json -Compress)
        if (-not $removed.success) { Write-Warning 'Temporary test license cleanup failed; inspect the admin panel.' }
        else { Write-Host ('Temporary test license removed: ID ' + $testLicense.id) }
    }
    $testLicense=$null; $authVault=$null; $headers=$null
}
