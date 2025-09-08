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

# Step 4: Use RunUAT.bat BuildPlugin on our dynamic host project.
# This is the standard, most reliable way to create a distributable, pre-compiled plugin.
Write-StepHeader "[4/5] INVOKING RUNUAT TO BUILD AND PACKAGE PLUGIN"
$uat_path = Join-Path $UePath "Engine\Build\BatchFiles\RunUAT.bat"
$plugin_to_build_path = Join-Path $target_plugin_proj_dir "XTools.uplugin"

# RunUAT will package the plugin into this temporary directory.
$package_intermediary_path = Join-Path $Workspace "PackageIntermediary"
New-Item -ItemType Directory -Force -Path $package_intermediary_path | Out-Null

$buildPluginArgs = @(
    "BuildPlugin",
    "-Plugin=`"$plugin_to_build_path`"",
    "-Package=`"$package_intermediary_path`"",
    "-Project=`"$temp_project_path`"",
    # Reverting to Win64 only for stability testing.
    "-TargetPlatforms=Win64",
    "-StrictIncludes"
)
$command_chain = "call `"$vcvars_path`" x64 && `"$uat_path`" $($buildPluginArgs -join ' ')"

Write-Host "  > Executing RunUAT.bat BuildPlugin command..."
Write-Host "    $command_chain"
cmd.exe /c $command_chain

if ($LASTEXITCODE -ne 0) {
    throw "[ERROR] RunUAT BuildPlugin failed with exit code $LASTEXITCODE. Check logs for details."
}
Write-Host "  > RunUAT BuildPlugin completed successfully."


# Step 5: Collect the packaged plugin from the intermediary directory.
Write-StepHeader "[5/5] COLLECTING FINAL ARTIFAcT"
$output_name = "XTools-UE_${EngineVersion}-Win64"
$final_package_dir = Join-Path $Workspace "PackagedPlugin\$output_name"

# RunUAT creates a subfolder with the plugin name inside the package path.
# We need to find this folder to copy the final result.
$packaged_plugin_source = Get-ChildItem -Path $package_intermediary_path -Directory | Select-Object -First 1
if (-not $packaged_plugin_source) {
    throw "[ERROR] RunUAT succeeded, but no packaged plugin was found in the intermediary directory."
}

Write-Host "  > Found packaged plugin at: $($packaged_plugin_source.FullName)"
Write-Host "  > Copying to final destination: $final_package_dir"

# Clean the final destination and copy the entire contents from the UAT output.
if (Test-Path $final_package_dir) {
    Remove-Item $final_package_dir -Recurse -Force
}
Copy-Item -Path $packaged_plugin_source.FullName -Destination $final_package_dir -Recurse

Write-Host "  > Final package created successfully at $final_package_dir"
Write-Host ""
Write-Host "SCRIPT FINISHED SUCCESSFULLY"
