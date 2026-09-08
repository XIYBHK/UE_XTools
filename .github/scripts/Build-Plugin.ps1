<#
.SYNOPSIS
Compatibility entry for CI packaging; shares UAT arguments with local/release builds.
#>
param([Parameter(Mandatory)][string]$EngineVersion)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = $env:GITHUB_WORKSPACE
$engineRoot = $env:UE_PATH
if (-not $workspace -or -not $engineRoot) { throw 'GITHUB_WORKSPACE and UE_PATH are required.' }
. (Join-Path $workspace 'Scripts/UEBuildHelpers.ps1')
$installed = Get-XToolsEngineVersion -EngineRoot $engineRoot
if ("$($installed.Major).$($installed.Minor)" -ne $EngineVersion) {
    throw "Requested UE $EngineVersion, but UE_PATH points to $installed."
}
$packagePath = Join-Path $workspace "PackagedPlugin/XTools-UE_${EngineVersion}-Win64"
$buildArguments = @(Get-XToolsBuildPluginArguments -EngineRoot $engineRoot -PluginPath (Join-Path $workspace 'XTools.uplugin') -PackagePath $packagePath)
$uat = Join-Path $engineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
Write-Host "UAT arguments: $($buildArguments -join ' ')"
& $uat @buildArguments | Out-Host
$buildExit = $LASTEXITCODE
if ($buildExit -ne 0) { throw "BuildPlugin failed with exit code $buildExit." }
if (-not (Test-Path -LiteralPath (Join-Path $packagePath 'XTools.uplugin'))) {
    throw "UAT completed without the expected packaged plugin: $packagePath"
}
Write-Host "Packaged plugin: $packagePath"
