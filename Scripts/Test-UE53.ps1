<#
.SYNOPSIS
Incrementally builds the configured UE 5.3 Editor target and runs real automation tests.
.DESCRIPTION
Defaults to the plugin's .vscode/settings.json and Build Editor (Full & Formal) task.
An explicit host project/target can be supplied by CI. No clean or packaging is performed.
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$ProjectPath,
    [string]$EditorTarget,
    [string[]]$Tests = @('XTools.EnhancedCodeFlow', 'XTools.Sort.Library', 'XTools.QueueSpline', 'XTools.ObjectPool', 'XTools.PointSampling'),
    [string]$ReportDirectory,
    [switch]$NonUnity
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'UEBuildHelpers.ps1')
$repoRoot = Split-Path -Parent $PSScriptRoot
$settings = Get-Content -LiteralPath (Join-Path $repoRoot '.vscode/settings.json') -Raw | ConvertFrom-Json -AsHashtable
$useConfiguredTask = -not $EngineRoot -and -not $ProjectPath -and -not $EditorTarget
if (-not $EngineRoot) { $EngineRoot = $settings['unreal.engine.path'] }
if (-not $ProjectPath) { $ProjectPath = $settings['unreal.project.path'] }
if (-not $EditorTarget) { $EditorTarget = $settings['unreal.project.name'] + 'Editor' }
$EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
$version = Get-XToolsEngineVersion -EngineRoot $EngineRoot
if ($version.Major -ne 5 -or $version.Minor -ne 3) { throw "This validation entry requires UE 5.3; selected engine is $version." }
if ($Tests.Count -eq 0 -or @($Tests | Where-Object { $_ -notmatch '^XTools(?:\.[A-Za-z0-9_]+)*$' }).Count -gt 0) {
    throw 'Tests must be one or more XTools automation name prefixes.'
}
$expected = @(Get-XToolsExpectedAutomationTests -SourceRoot (Join-Path $repoRoot 'Source') -Filters $Tests)
if ($expected.Count -eq 0) { throw 'Selected prefixes match no tests in this checkout.' }
foreach ($filter in $Tests) {
    if (@($expected | Where-Object { $_.StartsWith($filter, [System.StringComparison]::OrdinalIgnoreCase) }).Count -eq 0) {
        throw "Selected prefix matches no tests: $filter"
    }
}
if (-not $ReportDirectory) {
    $runName = 'XTools-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
    $ReportDirectory = Join-Path (Split-Path -Parent $ProjectPath) "Saved/Automation/Reports/$runName"
}
$ReportDirectory = [System.IO.Path]::GetFullPath($ReportDirectory)
if (Test-Path -LiteralPath $ReportDirectory) { throw "Use a new report directory to prevent stale results: $ReportDirectory" }
New-Item -ItemType Directory -Path $ReportDirectory | Out-Null

$buildCommand = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
$buildArguments = @($EditorTarget, 'Win64', 'Development', "-Project=$ProjectPath", '-FromMsBuild')
if ($useConfiguredTask) {
    $tasks = Get-Content -LiteralPath (Join-Path $repoRoot '.vscode/tasks.json') -Raw | ConvertFrom-Json
    $task = @($tasks.tasks | Where-Object label -EQ 'Build Editor (Full & Formal)')
    if ($task.Count -ne 1) { throw 'Expected one configured Editor build task.' }
    function Expand-XToolsTaskSetting([string]$Value) {
        foreach ($key in $settings.Keys) { $Value = $Value.Replace(('${config:' + $key + '}'), [string]$settings[$key]) }
        if ($Value.Contains('${')) { throw "Unresolved task variable: $Value" }
        return $Value
    }
    $buildCommand = Expand-XToolsTaskSetting $task[0].command
    $buildArguments = @($task[0].args | ForEach-Object { Expand-XToolsTaskSetting $_ })
}
if ($NonUnity) { $buildArguments += '-DisableUnity' }
Write-Host "UE $version incremental build: $buildCommand $($buildArguments -join ' ')"
& $buildCommand @buildArguments 2>&1 | Tee-Object -FilePath (Join-Path $ReportDirectory 'build.log') | Out-Host
$buildExit = $LASTEXITCODE
if ($buildExit -ne 0) { throw "Editor build failed with exit code $buildExit." }

$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$editorArguments = @($ProjectPath, '-unattended', '-nop4', '-nosplash', '-NullRHI',
    ('-ExecCmds=Automation RunTests ' + ($Tests -join '+')), '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$ReportDirectory", ('-abslog=' + (Join-Path $ReportDirectory 'automation.log')))
Write-Host "Running $($expected.Count) expected tests; report: $ReportDirectory"
& $editor @editorArguments *> (Join-Path $ReportDirectory 'console.log')
$editorExit = $LASTEXITCODE
if ($editorExit -ne 0) { throw "Editor automation process failed with exit code $editorExit." }
$report = Test-XToolsAutomationReport -ReportPath (Join-Path $ReportDirectory 'index.json') -ExpectedTests $expected
$report | Select-Object succeeded, succeededWithWarnings, failed, notRun, inProcess | Format-List
Write-Host "Verified all $($expected.Count) expected test names. Report: $ReportDirectory"
