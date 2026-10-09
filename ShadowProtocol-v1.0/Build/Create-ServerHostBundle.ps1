param(
    [Parameter(Mandatory=$true)][string]$PackageRoot,
    [ValidateSet('Linux','Win64')][string]$ServerPlatform = 'Linux',
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $ProjectRoot "Artifacts\HostBundle\$ServerPlatform"
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

& (Join-Path $PSScriptRoot 'Verify-DedicatedServerPackage.ps1') -PackageRoot $PackageRoot -ServerPlatform $ServerPlatform
if ($LASTEXITCODE -ne 0) { throw 'Dedicated server package verification failed' }

if (Test-Path $OutputDirectory) { Remove-Item -Recurse -Force $OutputDirectory }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$ServerOut = Join-Path $OutputDirectory 'server'
$OrchestratorOut = Join-Path $OutputDirectory 'orchestrator'
$DeployOut = Join-Path $OutputDirectory 'deploy\linux'
New-Item -ItemType Directory -Force -Path $ServerOut,$OrchestratorOut,$DeployOut | Out-Null

Copy-Item -Path (Join-Path $PackageRoot '*') -Destination $ServerOut -Recurse -Force

$OrchestratorRoot = Join-Path $ProjectRoot 'ServerOrchestrator'
Push-Location $OrchestratorRoot
try {
    & npm install --no-audit --no-fund
    if ($LASTEXITCODE -ne 0) { throw 'ServerOrchestrator npm install failed' }
    & npm run build
    if ($LASTEXITCODE -ne 0) { throw 'ServerOrchestrator build failed' }
} finally { Pop-Location }

Copy-Item -Path (Join-Path $OrchestratorRoot 'dist') -Destination $OrchestratorOut -Recurse -Force
Copy-Item -Path (Join-Path $ProjectRoot 'Deploy\Linux\shadow-protocol-server.service') -Destination $DeployOut -Force
Copy-Item -Path (Join-Path $ProjectRoot 'Deploy\Linux\install-host.sh') -Destination $DeployOut -Force
Copy-Item -Path (Join-Path $ProjectRoot 'Deploy\Linux\configure-firewall.sh') -Destination $DeployOut -Force

$candidateNames = if ($ServerPlatform -eq 'Linux') { @('ShadowProtocolServer.sh','ShadowProtocolServer') } else { @('ShadowProtocolServer.exe') }
$serverExe = $null
foreach ($name in $candidateNames) {
    $serverExe = Get-ChildItem -Path $ServerOut -Recurse -File -Filter $name | Select-Object -First 1
    if ($serverExe) { break }
}
if (!$serverExe) {
    $serverExe = Get-ChildItem -Path $ServerOut -Recurse -File -Filter 'ShadowProtocolServer*' |
        Where-Object { $_.Name -notmatch '\.(pdb|target|modules|sym)$' } |
        Select-Object -First 1
}
if (!$serverExe) { throw 'Unable to determine packaged dedicated-server executable' }

$serverRelative = [IO.Path]::GetRelativePath($ServerOut, $serverExe.FullName).Replace('\','/')
$linuxExecutable = "/opt/shadow-protocol/server/$serverRelative"
$envTemplate = Get-Content (Join-Path $ProjectRoot 'Deploy\Linux\server.env.example') -Raw
$envTemplate = $envTemplate -replace '(?m)^GAME_SERVER_EXECUTABLE=.*$', "GAME_SERVER_EXECUTABLE=$linuxExecutable"
Set-Content -Path (Join-Path $DeployOut 'server.env.example') -Value $envTemplate -Encoding utf8NoBOM

$manifest = [ordered]@{
    project = 'Shadow Protocol'
    networkBuild = 'SP-1.0.1'
    serverPlatform = $ServerPlatform
    serverExecutable = $serverRelative
    createdUtc = [DateTime]::UtcNow.ToString('o')
}
$manifest | ConvertTo-Json | Set-Content -Path (Join-Path $OutputDirectory 'bundle-manifest.json') -Encoding utf8NoBOM

$zipPath = "$OutputDirectory.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path (Join-Path $OutputDirectory '*') -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Host bundle created: $OutputDirectory"
Write-Host "Upload archive: $zipPath"
Write-Host "Detected game executable: $serverRelative"
