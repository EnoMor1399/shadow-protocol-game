param(
    [Parameter(Mandatory=$true)][string]$PackageRoot,
    [ValidateSet('Linux','Win64')][string]$ServerPlatform = 'Linux'
)

$ErrorActionPreference = 'Stop'
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)
if (!(Test-Path $PackageRoot)) { throw "Package root does not exist: $PackageRoot" }

$pattern = if ($ServerPlatform -eq 'Linux') { 'ShadowProtocolServer*' } else { 'ShadowProtocolServer.exe' }
$server = Get-ChildItem -Path $PackageRoot -Recurse -File -Filter $pattern |
    Where-Object { $_.Name -notmatch '\.(pdb|target|modules)$' } |
    Select-Object -First 1
if (!$server) { throw "Dedicated server executable not found under $PackageRoot" }

$pak = Get-ChildItem -Path $PackageRoot -Recurse -File -Filter '*.pak' | Select-Object -First 1
if (!$pak) { throw "Cooked .pak content not found under $PackageRoot" }

$assetRegistry = Get-ChildItem -Path $PackageRoot -Recurse -File -Filter 'AssetRegistry.bin' | Select-Object -First 1
if (!$assetRegistry) { throw "AssetRegistry.bin not found; cook/stage appears incomplete" }

$forbidden = Get-ChildItem -Path $PackageRoot -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '\.(env|pem|key)$' -or $_.Name -match 'secret|credential' }
if ($forbidden) {
    $names = ($forbidden | ForEach-Object FullName) -join "`n"
    throw "Potential secret material found in package:`n$names"
}

Write-Host "Verified executable: $($server.FullName)"
Write-Host "Verified cooked content: $($pak.FullName)"
Write-Host 'No obvious secret files found in the server package.'
