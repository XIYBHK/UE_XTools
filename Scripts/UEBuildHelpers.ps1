# Shared build arguments and automation report checks. Dot-source this file.
function Get-XToolsEngineVersion {
    param([Parameter(Mandatory)][string]$EngineRoot)
    $buildVersion = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
    return [version]::new([int]$buildVersion.MajorVersion, [int]$buildVersion.MinorVersion, [int]$buildVersion.PatchVersion)
}

function Get-XToolsBuildPluginArguments {
    param(
        [Parameter(Mandatory)][string]$EngineRoot,
        [Parameter(Mandatory)][string]$PluginPath,
        [Parameter(Mandatory)][string]$PackagePath,
        [string]$TargetPlatforms = 'Win64',
        [bool]$StrictIncludes = $true,
        [bool]$NoHostProject = $true,
        [switch]$CreateSubFolder,
        [switch]$Rocket
    )
    $engineVersion = Get-XToolsEngineVersion -EngineRoot $EngineRoot
    $arguments = @('BuildPlugin', "-Plugin=$PluginPath", "-Package=$PackagePath", "-TargetPlatforms=$TargetPlatforms")
    if ($NoHostProject) { $arguments += '-NoHostProject' }
    # UE 5.3 BuildPlugin omits engine UHT include paths with StrictIncludes.
    if ($StrictIncludes -and -not ($engineVersion.Major -eq 5 -and $engineVersion.Minor -eq 3)) {
        $arguments += '-StrictIncludes'
    }
    if ($CreateSubFolder) { $arguments += '-CreateSubFolder' }
    if ($Rocket) { $arguments += '-Rocket' }
    return $arguments
}

function Get-XToolsExpectedAutomationTests {
    param([Parameter(Mandatory)][string]$SourceRoot, [Parameter(Mandatory)][string[]]$Filters)
    $names = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($file in Get-ChildItem -LiteralPath $SourceRoot -Recurse -File | Where-Object Extension -In @('.cpp', '.h')) {
        $source = Get-Content -LiteralPath $file.FullName -Raw
        foreach ($match in [regex]::Matches($source, 'IMPLEMENT_(?:SIMPLE|CUSTOM_SIMPLE|COMPLEX)_AUTOMATION_TEST(?:_PRIVATE)?\s*\([^;]*?"(?<name>XTools\.[^"\r\n]+)"')) {
            $name = $match.Groups['name'].Value
            foreach ($filter in $Filters) {
                if ($name.StartsWith($filter, [System.StringComparison]::OrdinalIgnoreCase)) {
                    [void]$names.Add($name)
                    break
                }
            }
        }
    }
    return @($names | Sort-Object)
}

function Test-XToolsAutomationReport {
    param([Parameter(Mandatory)][string]$ReportPath, [Parameter(Mandatory)][string[]]$ExpectedTests)
    if ($ExpectedTests.Count -eq 0) { throw 'No expected tests were selected.' }
    $report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
    foreach ($field in @('succeeded', 'succeededWithWarnings', 'failed', 'notRun', 'inProcess', 'tests')) {
        if ($null -eq $report.PSObject.Properties[$field]) { throw "Automation report is missing $field." }
    }
    $completed = [int]$report.succeeded + [int]$report.succeededWithWarnings
    if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or $completed -le 0) {
        throw "Automation did not finish successfully: succeeded=$completed failed=$($report.failed) notRun=$($report.notRun) inProcess=$($report.inProcess)"
    }
    $actual = @($report.tests)
    if ($actual.Count -ne $completed -or @($actual | Where-Object state -NE 'Success').Count -gt 0) {
        throw 'Automation summary disagrees with individual test results.'
    }
    $actualNames = @($actual.fullTestPath | Sort-Object -Unique)
    if ($actualNames.Count -ne $actual.Count) { throw 'Automation report contains duplicate test names.' }
    $missing = @($ExpectedTests | Where-Object { $_ -notin $actualNames })
    if ($missing.Count -gt 0) { throw "Expected automation tests did not run: $($missing -join ', ')" }
    return $report
}
