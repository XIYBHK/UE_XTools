# Dependency-free regression tests for argument construction and false-green reports.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../UEBuildHelpers.ps1')
$temporaryRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$fixtureRoot = Join-Path $temporaryRoot ('XTools Build Tests ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
$script:checks = 0
function Assert-Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}
function Assert-Rejected([scriptblock]$Action, [string]$Message) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert-Check $rejected $Message
}
try {
    $sourceRoot = Join-Path $fixtureRoot 'Source'
    New-Item -ItemType Directory -Path $sourceRoot | Out-Null
    @'
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOne, "XTools.Test.One", Flags)
    IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(FTwo, FBase, "XTools.Test.Two", Flags, __FILE__, __LINE__)
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOther, "XTools.Other.Test", Flags)
'@ | Set-Content -LiteralPath (Join-Path $sourceRoot 'Fixture.cpp') -Encoding utf8
    $discovered = @(Get-XToolsExpectedAutomationTests -SourceRoot $sourceRoot -Filters 'XTools.Test')
    Assert-Check ($discovered.Count -eq 2 -and $discovered -contains 'XTools.Test.Two') 'Private automation macros must be discovered without unrelated prefixes.'
    foreach ($minor in 3..8) {
        # Intentionally avoid versioned directory names: Build.version is authoritative.
        $engineRoot = Join-Path $fixtureRoot "Engine installation $minor"
        $buildRoot = Join-Path $engineRoot 'Engine/Build'
        New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
        @{ MajorVersion = 5; MinorVersion = $minor; PatchVersion = 2 } |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $buildRoot 'Build.version') -Encoding utf8
        $pluginPath = Join-Path $fixtureRoot 'Plugin source/XTools.uplugin'
        $packagePath = Join-Path $fixtureRoot 'Packaged Plugin'
        $arguments = @(Get-XToolsBuildPluginArguments -EngineRoot $engineRoot -PluginPath $pluginPath -PackagePath $packagePath)
        Assert-Check ($arguments[0] -eq 'BuildPlugin') 'BuildPlugin verb is required.'
        Assert-Check ($arguments -contains "-Plugin=$pluginPath") 'Plugin path must remain a single unquoted argument.'
        Assert-Check ($arguments -contains "-Package=$packagePath") 'Package path must remain a single unquoted argument.'
        Assert-Check (($arguments -contains '-StrictIncludes') -eq ($minor -ne 3)) 'StrictIncludes version exception drifted.'
        Assert-Check ($arguments -contains '-TargetPlatforms=Win64') 'Platforms must be explicit.'
        $disabled = @(Get-XToolsBuildPluginArguments -EngineRoot $engineRoot -PluginPath $pluginPath -PackagePath $packagePath -StrictIncludes:$false -NoHostProject:$false)
        Assert-Check ($disabled -notcontains '-StrictIncludes' -and $disabled -notcontains '-NoHostProject') 'Explicit switches must be respected.'
    }
    $expected = @('XTools.Test.One', 'XTools.Test.Two')
    $reportPath = Join-Path $fixtureRoot 'index.json'
    function Write-FixtureReport([hashtable]$Changes = @{}) {
        $report = @{
            succeeded = 2; succeededWithWarnings = 0; failed = 0; notRun = 0; inProcess = 0
            tests = @(@{ fullTestPath = 'XTools.Test.One'; state = 'Success' }, @{ fullTestPath = 'XTools.Test.Two'; state = 'Success' })
        }
        foreach ($key in $Changes.Keys) { $report[$key] = $Changes[$key] }
        $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding utf8
    }
    Write-FixtureReport
    $report = Test-XToolsAutomationReport -ReportPath $reportPath -ExpectedTests $expected
    Assert-Check ($report.succeeded -eq 2) 'Valid reports must pass.'
    foreach ($field in @('failed', 'notRun', 'inProcess')) {
        Write-FixtureReport @{ $field = 1 }
        Assert-Rejected { Test-XToolsAutomationReport -ReportPath $reportPath -ExpectedTests $expected } "Nonzero $field must fail."
    }
    Write-FixtureReport @{ succeeded = 0; tests = @() }
    Assert-Rejected { Test-XToolsAutomationReport -ReportPath $reportPath -ExpectedTests $expected } 'Zero tests must fail.'
    Write-FixtureReport @{ succeeded = 1; tests = @(@{ fullTestPath = 'XTools.Test.One'; state = 'Success' }) }
    Assert-Rejected { Test-XToolsAutomationReport -ReportPath $reportPath -ExpectedTests $expected } 'Missing expected names must fail.'
    Write-FixtureReport @{ tests = @(@{ fullTestPath = 'XTools.Test.One'; state = 'Fail' }, @{ fullTestPath = 'XTools.Test.Two'; state = 'Success' }) }
    Assert-Rejected { Test-XToolsAutomationReport -ReportPath $reportPath -ExpectedTests $expected } 'A green summary must not hide a failed test.'
    Write-FixtureReport @{ tests = @(@{ fullTestPath = 'XTools.Test.One'; state = 'Success' }, @{ fullTestPath = 'XTools.Test.One'; state = 'Success' }) }
    Assert-Rejected { Test-XToolsAutomationReport -ReportPath $reportPath -ExpectedTests $expected } 'Duplicate results must fail.'
    '{}' | Set-Content -LiteralPath $reportPath -Encoding utf8
    Assert-Rejected { Test-XToolsAutomationReport -ReportPath $reportPath -ExpectedTests $expected } 'Malformed reports must fail.'
    Write-Host "$script:checks build helper checks passed."
}
finally {
    $resolvedFixture = (Resolve-Path -LiteralPath $fixtureRoot).Path
    $expectedPrefix = Join-Path $temporaryRoot 'XTools Build Tests '
    if (-not $resolvedFixture.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected fixture cleanup path: $resolvedFixture"
    }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
