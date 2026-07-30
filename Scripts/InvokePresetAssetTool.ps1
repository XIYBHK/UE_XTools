[CmdletBinding()]
param(
    [ValidateSet('PresetLibrary', 'Easing', 'All')]
    [string]$Set = 'PresetLibrary',

    [ValidateSet('Generate', 'Validate')]
    [string]$Mode = 'Generate',

    [string]$EngineRoot = '',

    [string]$ProjectFile = '',

    [switch]$Force
)

$ErrorActionPreference = 'Stop'

if ($Mode -eq 'Validate' -and $Force) {
    throw '-Force cannot be combined with -Mode Validate.'
}

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    $EngineRoot = if ($env:XTOOLS_UE53_ROOT) {
        $env:XTOOLS_UE53_ROOT
    }
    else {
        'D:\Program Files\Epic Games\UE_5.3'
    }
}

if ([string]::IsNullOrWhiteSpace($ProjectFile)) {
    $ProjectFile = Join-Path $PSScriptRoot '..\..\..\cppxtools.uproject'
}

$editorExe = [System.IO.Path]::GetFullPath(
    (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe')
)
$projectPath = [System.IO.Path]::GetFullPath($ProjectFile)

if (-not (Test-Path -LiteralPath $editorExe -PathType Leaf)) {
    throw "UnrealEditor.exe not found: $editorExe"
}
if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
    throw "Project file not found: $projectPath"
}

$toolDefinitions = [ordered]@{
    Easing = [ordered]@{
        Script = 'GenerateEasingCurveAssets.py'
        CompletionMarker = 'easing CurveFloat assets'
    }
    PresetLibrary = [ordered]@{
        Script = 'GeneratePresetLibraryAssets.py'
        CompletionMarker = 'preset library assets:'
    }
}

$selectedTools = if ($Set -eq 'All') {
    @('Easing', 'PresetLibrary')
}
else {
    @($Set)
}

$environmentNames = @(
    'XTOOLS_EASING_CURVE_VALIDATE_ONLY',
    'XTOOLS_EASING_CURVE_FORCE_REBUILD',
    'XTOOLS_PRESET_LIBRARY_VALIDATE_ONLY',
    'XTOOLS_PRESET_LIBRARY_FORCE_REBUILD'
)
$previousEnvironment = @{}
foreach ($name in $environmentNames) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

try {
    $validateValue = if ($Mode -eq 'Validate') { '1' } else { '0' }
    $forceValue = if ($Force) { '1' } else { '0' }
    [Environment]::SetEnvironmentVariable('XTOOLS_EASING_CURVE_VALIDATE_ONLY', $validateValue, 'Process')
    [Environment]::SetEnvironmentVariable('XTOOLS_PRESET_LIBRARY_VALIDATE_ONLY', $validateValue, 'Process')
    [Environment]::SetEnvironmentVariable('XTOOLS_EASING_CURVE_FORCE_REBUILD', $forceValue, 'Process')
    [Environment]::SetEnvironmentVariable('XTOOLS_PRESET_LIBRARY_FORCE_REBUILD', $forceValue, 'Process')

    foreach ($toolName in $selectedTools) {
        $definition = $toolDefinitions[$toolName]
        $scriptPath = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $definition.Script))
        if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
            throw "Tool script not found: $scriptPath"
        }

        $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
        $logDirectory = Join-Path (Split-Path -Parent $projectPath) 'Saved\Logs'
        [System.IO.Directory]::CreateDirectory($logDirectory) | Out-Null
        $logPath = Join-Path $logDirectory "XToolsPresetAssets-$toolName-$Mode-$timestamp.log"

        $editorArguments = @(
            "`"$projectPath`"",
            "`"-ExecutePythonScript=$scriptPath`"",
            '-unattended',
            '-nop4',
            '-nosplash',
            '-RenderOffScreen',
            '-NoSound',
            "`"-abslog=$logPath`""
        )

        Write-Host "[$Mode] $toolName"
        $process = Start-Process `
            -FilePath $editorExe `
            -ArgumentList $editorArguments `
            -PassThru `
            -Wait `
            -WindowStyle Hidden

        if ($process.ExitCode -ne 0) {
            throw "$toolName failed with exit code $($process.ExitCode). Log: $logPath"
        }
        if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
            throw "$toolName did not create its expected log: $logPath"
        }

        $logText = Get-Content -LiteralPath $logPath -Raw
        if ($logText -match 'LogPython: Error|Fatal error:|Assertion failed:') {
            throw "$toolName reported an Unreal or Python error. Log: $logPath"
        }
        if (-not $logText.Contains($definition.CompletionMarker)) {
            throw "$toolName exited without its completion marker. Log: $logPath"
        }

        Write-Host "Completed. Log: $logPath"
    }
}
finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
    }
}
