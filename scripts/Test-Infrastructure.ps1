$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $projectRoot 'custom-base.vcxproj'
[xml]$project = Get-Content -LiteralPath $projectPath -Raw
$namespace = New-Object Xml.XmlNamespaceManager($project.NameTable)
$namespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')

$missing = New-Object Collections.Generic.List[string]
foreach ($node in $project.SelectNodes('//m:ClCompile|//m:ClInclude|//m:ResourceCompile', $namespace)) {
    $include = [string]$node.Include
    if (-not $include -or $include.IndexOfAny(@('*', '?')) -ge 0) { continue }
    $path = Join-Path $projectRoot $include
    if (-not (Test-Path -LiteralPath $path)) { $missing.Add($include) }
}
if ($missing.Count -gt 0) {
    throw "Projede eksik dosya referanslari var:`n$($missing -join "`n")"
}

$filtersPath = Join-Path $projectRoot 'custom-base.vcxproj.filters'
[xml]$filters = Get-Content -LiteralPath $filtersPath -Raw
$filterNamespace = New-Object Xml.XmlNamespaceManager($filters.NameTable)
$filterNamespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
$projectItems = New-Object 'Collections.Generic.HashSet[string]'
$filterItems = New-Object 'Collections.Generic.HashSet[string]'
foreach ($node in $project.SelectNodes('//m:ClCompile|//m:ClInclude|//m:ResourceCompile|//m:None', $namespace)) {
    if ($node.Include) { [void]$projectItems.Add("$($node.LocalName)|$($node.Include)") }
}
foreach ($node in $filters.SelectNodes('//m:ClCompile|//m:ClInclude|//m:ResourceCompile|//m:None', $filterNamespace)) {
    if ($node.Include) { [void]$filterItems.Add("$($node.LocalName)|$($node.Include)") }
}
if (-not $projectItems.SetEquals($filterItems)) {
    throw 'Visual Studio filters dosyasi proje ile uyusmuyor. scripts\Refresh-ProjectFilters.py calistir.'
}

$moduleHeaders = Get-ChildItem (Join-Path $projectRoot 'src\modules') -Recurse -Filter '*.hpp' |
    Where-Object { Select-String -LiteralPath $_.FullName -Pattern 'class\s+\w+\s*:\s*public\s+module' -Quiet }
if ($moduleHeaders.Count -ne 43) {
    throw "Beklenen 43 modul basligi yerine $($moduleHeaders.Count) bulundu."
}

$umbrellaLines = (Get-Content (Join-Path $projectRoot 'src\modules\modules.hpp')).Count
if ($umbrellaLines -gt 80) {
    throw "modules.hpp yeniden buyumus ($umbrellaLines satir). Modul bildirimlerini kendi basliklarinda tut."
}

$catalog = Get-Content (Join-Path $projectRoot 'src\modules\module_catalog.cpp') -Raw
$catalogIds = [regex]::Matches($catalog, '\{\s*"([a-z0-9_]+)"\s*,\s*&') |
    ForEach-Object { $_.Groups[1].Value }
if ($catalogIds.Count -ne 62 -or ($catalogIds | Select-Object -Unique).Count -ne 62) {
    throw "Modul catalogunda 62 benzersiz kayit olmali; $($catalogIds.Count) bulundu."
}

$duplicateJniHelpers = Get-ChildItem (Join-Path $projectRoot 'src') -Recurse -Filter '*.cpp' |
    Select-String -Pattern 'bool\s+(clear_java_exception|ensure_client_jni_env)\s*\('
if ($duplicateJniHelpers) {
    throw 'Bir modul ortak jni_context yerine yerel JNI yardimcisi tanimliyor.'
}

$directInput = Get-ChildItem (Join-Path $projectRoot 'src\modules') -Recurse -Filter '*.cpp' |
    Select-String -Pattern '\bSendInput\s*\('
if ($directInput) {
    throw 'Bir modul ortak input_service disinda SendInput kullaniyor.'
}

$directHotbarWrite = Get-ChildItem (Join-Path $projectRoot 'src\modules') -Recurse -Filter '*.cpp' |
    Select-String -Pattern 'selectedSlot\.set\s*\('
if ($directHotbarWrite) {
    throw 'Bir modul ortak hotbar_service disinda selectedSlot yaziyor.'
}

Write-Host "Altyapi kontrolu gecti: $($moduleHeaders.Count) ayrik modul, $($catalogIds.Count) catalog kaydi, ortak JNI/input/hotbar servisleri temiz."
