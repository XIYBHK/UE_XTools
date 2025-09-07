# .github/scripts/Build-Plugin.ps1

<#
.SYNOPSIS
    Builds and packages the Unreal Engine plugin using a dynamic host project strategy.
.DESCRIPTION
    This script automates the process of building and packaging the XTools plugin for a specific
    Unreal Engine version. It dynamically creates a temporary host project, compiles the plugin
    using a direct UnrealBuildTool (UBT) invocation, and then packages the required files into
    a clean distributable format. It's designed to be called from a GitHub Actions workflow.
.PARAMETER EngineVersion
    The Unreal Engine version to build against (e.g., "5.3"). This is mandatory.
.EXAMPLE
    ./.github/scripts/Build-Plugin.ps1 -EngineVersion 5.3
#>
param (
    [string][Parameter(Mandatory=$true)] $EngineVersion
)

# --- Initialization and Setup ---
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Get environment variables provided by the GitHub Actions runner
$Workspace = $env:GITHUB_WORKSPACE
$UePath = $env:UE_PATH
$VsInstallDir = $env:VS2022INSTALLDIR

# Function to write a formatted step header
function Write-StepHeader {
    param([string]$Title)
    Write-Host ""
    Write-Host "========================================================================"
    Write-Host "  $Title"
    Write-Host "========================================================================"
}

Write-StepHeader "INITIALIZING BUILD SCRIPT"
Write-Host "  > Engine Version: $EngineVersion"
Write-Host "  > Workspace: $Workspace"
Write-Host "  > UE Path: $UePath"
Write-Host "  > VS Path: $VsInstallDir"

# --- Main Build Process ---

# Step 1: Dynamically create a temporary host project.
Write-StepHeader "[1/5] CREATING DYNAMIC HOST PROJECT"
$temp_project_dir = Join-Path $Workspace "TempHostProject"
$temp_project_path = Join-Path $temp_project_dir "TempHostProject.uproject"
New-Item -ItemType Directory -Force -Path $temp_project_dir | Out-Null

$uproject_content = ('{{' + `
'    "FileVersion": 3,' + `
'    "EngineAssociation": "{0}",' + `
'    "Category": "",' + `
'    "Description": ""' + `
'}}') -f $EngineVersion

Set-Content -Path $temp_project_path -Value $uproject_content -Encoding UTF8
Write-Host "  > Successfully created temporary host project at: $temp_project_path"

# Step 2: Copy the plugin source into the temporary project.
Write-StepHeader "[2/5] COPYING PLUGIN SOURCE"
$source_plugin_repo_dir = $Workspace
$target_plugin_proj_dir = Join-Path $temp_project_dir "Plugins\XTools"
New-Item -ItemType Directory -Force -Path (Split-Path $target_plugin_proj_dir) | Out-Null

$exclude_dirs = @( (Split-Path -Leaf $temp_project_dir), "PackagedPlugin", ".git")
Get-ChildItem -Path $source_plugin_repo_dir -Exclude $exclude_dirs | Copy-Item -Destination $target_plugin_proj_dir -Recurse -Force
Write-Host "  > Successfully copied plugin source."

# Step 3: Initialize the Visual Studio Developer Environment.
Write-StepHeader "[3/5] INITIALIZING VISUAL STUDIO ENVIRONMENT"
$vcvars_path = Join-Path $VsInstallDir "VC\Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path $vcvars_path)) {
    throw "[ERROR] vcvarsall.bat not found at $vcvars_path."
}
Write-Host "  > vcvarsall.bat found."

# Step 4: Directly invoke UnrealBuildTool (UBT) to build the temporary project.
Write-StepHeader "[4/5] INVOKING UNREAL BUILD TOOL (UBT)"
$ubt_path = Join-Path $UePath "Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll"

$dotnet_search_path = Join-Path $UePath "Engine\Binaries\ThirdParty\DotNet"
$dotnet_exe_info = Get-ChildItem -Path $dotnet_search_path -Filter "dotnet.exe" -Recurse | Select-Object -First 1

if (-not $dotnet_exe_info) {
    throw "[ERROR] Could not find dotnet.exe for UE version $EngineVersion under '$dotnet_search_path'"
}
$dotnet_path = $dotnet_exe_info.FullName
Write-Host "  > Found dotnet.exe at: $dotnet_path"

$ubtArgs = @(
    "UnrealEditor",
    "Win64",
    "Development",
    "-Project=`"$temp_project_path`"",
    "-log=`"$Workspace\ubt.log`""
)

$command_chain = "call `"$vcvars_path`" x64 && `"$dotnet_path`" `"$ubt_path`" $($ubtArgs -join ' ')"

Write-Host "  > Executing command chain..."
Write-Host "    $command_chain"
cmd.exe /c $command_chain

if ($LASTEXITCODE -ne 0) {
    throw "[ERROR] UBT build failed with exit code $LASTEXITCODE. Check the ubt.log artifact for details."
}
Write-Host "  > UBT build completed successfully."

# Step 5: Manually package the plugin with the newly compiled binaries.
Write-StepHeader "[5/5] PACKAGING PLUGIN"
$output_name = "XTools-UE_${EngineVersion}-Win64" # Renamed to remove "-Test"
$final_package_dir = Join-Path $Workspace "PackagedPlugin\$output_name"

$built_plugin_dir = $target_plugin_proj_dir
$binary_source_path = Join-Path $built_plugin_dir "Binaries\Win64"
if (-not (Test-Path $binary_source_path)) {
    throw "[ERROR] Build succeeded but no binaries were found at $binary_source_path"
}

New-Item -ItemType Directory -Force -Path $final_package_dir | Out-Null

$include_dirs = @("Content", "Resources", "Source", "ThirdParty")
$include_files = @("README.md", "XTools.uplugin")

foreach ($dir in $include_dirs) {
    $source_dir = Join-Path $built_plugin_dir $dir
    if (Test-Path $source_dir) {
        Write-Host "  > Copying directory: $dir"
        Copy-Item -Path $source_dir -Destination $final_package_dir -Recurse
    }
}

foreach ($file in $include_files) {
    $source_file = Join-Path $built_plugin_dir $file
    if (Test-Path $source_file) {
        Write-Host "  > Copying file: $file"
        Copy-Item -Path $source_file -Destination $final_package_dir
    }
}

$binary_dest_path = Join-Path $final_package_dir "Binaries\Win64"
New-Item -ItemType Directory -Force -Path $binary_dest_path | Out-Null
Write-Host "  > Copying and cleaning binaries (DLLs only)..."
Get-ChildItem -Path $binary_source_path -Filter "*.dll" | Copy-Item -Destination $binary_dest_path

Write-Host "  > Plugin packaging complete to $final_package_dir"
Write-Host ""
Write-Host "SCRIPT FINISHED SUCCESSFULLY"
