#requires -Version 7.0
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AuthVault.ps1')
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$authVault = Read-OnestepAuthVault
$authWindow = [System.Windows.Forms.Form]::new()
$authWindow.Text = 'Onestep - Auth'
$authWindow.ClientSize = [System.Drawing.Size]::new(420, 200)
$authWindow.FormBorderStyle = 'FixedDialog'
$authWindow.MaximizeBox = $false
$authWindow.StartPosition = 'CenterScreen'
$authWindow.BackColor = [System.Drawing.Color]::FromArgb(21, 22, 29)
$authWindow.ForeColor = [System.Drawing.Color]::WhiteSmoke
$authWindow.Font = [System.Drawing.Font]::new('Segoe UI', 10)
$authTitle = [System.Windows.Forms.Label]::new()
$authTitle.Text = 'Onestep lisans yonetimi'
$authTitle.SetBounds(22, 18, 380, 24)
$authWindow.Controls.Add($authTitle)
$authAdmin = [System.Windows.Forms.Button]::new()
$authAdmin.Text = 'Yonetici parolasini kopyala'
$authAdmin.SetBounds(22, 54, 376, 34)
$authAdmin.FlatStyle = 'Flat'
$authWindow.Controls.Add($authAdmin)
$authLicense = [System.Windows.Forms.Button]::new()
$authLicense.Text = 'Client lisansini kopyala'
$authLicense.SetBounds(22, 96, 376, 34)
$authLicense.FlatStyle = 'Flat'
$authWindow.Controls.Add($authLicense)
$authStatus = [System.Windows.Forms.Label]::new()
$authStatus.SetBounds(22, 140, 376, 24)
$authWindow.Controls.Add($authStatus)
$authLink = [System.Windows.Forms.LinkLabel]::new()
$authLink.Text = 'Yonetim panelini ac'
$authLink.LinkColor = [System.Drawing.Color]::FromArgb(174, 136, 255)
$authLink.SetBounds(22, 170, 376, 24)
$authWindow.Controls.Add($authLink)
$authAdmin.Add_Click({
    Set-Clipboard -Value $authVault.admin_token
    $authStatus.Text = 'Yonetici parolasi panoya kopyalandi.'
})
$authLicense.Add_Click({
    Set-Clipboard -Value $authVault.owner_license
    $authStatus.Text = 'Client lisansi panoya kopyalandi.'
})
$authLink.Add_LinkClicked({ Start-Process 'https://onestep-auth.gogogamesas.workers.dev/admin' -WindowStyle Hidden })
try { [void]$authWindow.ShowDialog() }
finally { $authWindow.Dispose(); $authVault = $null }
