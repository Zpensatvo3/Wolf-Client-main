$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Join-Path $root 'ui'
$cmake = Get-Content -LiteralPath (Join-Path $ui 'CMakeLists.txt') -Raw
$entries = [regex]::Matches($cmake, '(?m)^\s+(qml/[A-Za-z0-9_./-]+)$') | ForEach-Object { $_.Groups[1].Value }
$missing = @($entries | Where-Object { -not (Test-Path -LiteralPath (Join-Path $ui $_)) })
if ($missing.Count) { throw "Eksik Qt kaynagi: $($missing -join ', ')" }

$qmlFiles = Get-ChildItem -LiteralPath (Join-Path $ui 'qml') -Recurse -Filter *.qml
$controlsImports = @($qmlFiles | Select-String -Pattern '^\s*import\s+QtQuick\.Controls')
if ($controlsImports.Count) { throw 'Varsayilan Qt Quick Controls kullanimi bulundu.' }

$required = @(
    'qml\Main.qml',
    'qml\components\ModuleSettingsPanel.qml',
    'qml\components\SearchPopover.qml',
    'qml\pages\ConfigsPage.qml',
    'qml\pages\FriendsPage.qml',
    'qml\overlays\WatermarkOverlay.qml',
    'qml\overlays\ArrayListOverlay.qml'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $ui $relative))) { throw "Eksik arayuz parcasi: $relative" }
}

Write-Host "Qt UI kaynak testi basarili: $($qmlFiles.Count) QML dosyasi, $($entries.Count) gomulu kaynak."
