param(
    [Parameter(Mandatory=$true)][string]$UnrealRoot,
    [ValidateSet('Linux','Win64')][string]$ServerPlatform = 'Linux',
    [ValidateSet('Development','Shipping')][string]$Configuration = 'Shipping',
    [string]$ArchiveDirectory = '',
    [switch]$SkipEditorBuild
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $ProjectRoot 'ShadowProtocol.uproject'
$BuildBat = Join-Path $UnrealRoot 'Engine\Build\BatchFiles\Build.bat'
$RunUat = Join-Path $UnrealRoot 'Engine\Build\BatchFiles\RunUAT.bat'

if (!(Test-Path $Project)) { throw "Project not found: $Project" }
if (!(Test-Path $BuildBat)) { throw "UE Build.bat not found: $BuildBat" }
if (!(Test-Path $RunUat)) { throw "UE RunUAT.bat not found: $RunUat" }

if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
    $ArchiveDirectory = Join-Path $ProjectRoot "Artifacts\DedicatedServer\$ServerPlatform-$Configuration"
}
$ArchiveDirectory = [IO.Path]::GetFullPath($ArchiveDirectory)
New-Item -ItemType Directory -Force -Path $ArchiveDirectory | Out-Null

Write-Host "Shadow Protocol UE5.6 dedicated server package"
Write-Host "Project: $Project"
Write-Host "Platform: $ServerPlatform"
Write-Host "Configuration: $Configuration"
Write-Host "Archive: $ArchiveDirectory"

if (!$SkipEditorBuild) {
    & $BuildBat ShadowProtocolEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "UE Editor/UHT build failed with exit code $LASTEXITCODE" }
}

& $BuildBat ShadowProtocolServer $ServerPlatform $Configuration "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Dedicated server build failed with exit code $LASTEXITCODE" }

$uatArgs = @(
    'BuildCookRun',
    "-project=$Project",
    '-noP4',
    '-utf8output',
    '-build',
    '-cook',
    '-stage',
    '-pak',
    '-archive',
    "-archivedirectory=$ArchiveDirectory",
    '-server',
    '-noclient',
    "-serverplatform=$ServerPlatform",
    "-serverconfig=$Configuration",
    '-map=/Game/Maps/Embassy/Embassy_P'
)

& $RunUat @uatArgs
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed with exit code $LASTEXITCODE" }

& (Join-Path $PSScriptRoot 'Verify-DedicatedServerPackage.ps1') -PackageRoot $ArchiveDirectory -ServerPlatform $ServerPlatform
if ($LASTEXITCODE -ne 0) { throw 'Package verification failed' }

Write-Host "Dedicated server package completed: $ArchiveDirectory"
