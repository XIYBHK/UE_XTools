Param(
  [string]$PluginUplugin = (Join-Path $PSScriptRoot "..\XTools.uplugin"),
  [string[]]$EngineRoots,
  [string]$OutputBase = (Join-Path $PSScriptRoot "..\..\Plugin_Packages"),
  [string]$TargetPlatforms = "Win64+Linux+Mac",
  [switch]$StrictIncludes = $true,
  [switch]$NoHostProject = $true,
  [switch]$CleanOutput = $false,
  [switch]$Follow = $false
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-Engines {
  param([string[]]$Roots)
  if ($Roots -and $Roots.Count -gt 0) { return $Roots }
  $candidates = @(
    "D:\\Program Files\\Epic Games\\UE_5.4",
    "D:\\Program Files\\Epic Games\\UE_5.5",
    "D:\\Program Files\\Epic Games\\UE_5.6",
    "F:\\ProgramFiles\\UE_5.6"
  )
  return $candidates | Where-Object { Test-Path $_ }
}

function Invoke-BuildPlugin {
  param(
    [string]$EngineRoot,
    [string]$PluginPath,
    [string]$OutDir,
    [string]$Platforms,
    [switch]$UseStrict,
    [switch]$UseNoHostProject,
    [switch]$FollowLogs
  )

  if ($CleanOutput -and (Test-Path $OutDir)) {
    Write-Host "[CLEAN] $OutDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $OutDir
  }
  New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

  $uat = Join-Path $EngineRoot "Engine\\Build\\BatchFiles\\RunUAT.bat"
  if (!(Test-Path $uat)) { throw "RunUAT not found: $uat" }

  $args = @(
    'BuildPlugin',
    "-Plugin=`"$PluginPath`"",
    "-Package=`"$OutDir`"",
    "-TargetPlatforms=$Platforms"
  )
  if ($UseNoHostProject) { $args += '-NoHostProject' }
  if ($UseStrict)       { $args += '-StrictIncludes' }

  Write-Host "[RUN] $uat $($args -join ' ')" -ForegroundColor Cyan

  if ($FollowLogs) {
    # Build expected UAT log directory from engine root
    $sanitized = ($EngineRoot -replace '[:\\]', '+')
    $logRoot = Join-Path "$env:APPDATA\Unreal Engine\AutomationTool\Logs" $sanitized

    $proc = Start-Process -FilePath $uat -ArgumentList $args -PassThru -NoNewWindow
    Write-Host "[FOLLOW] $logRoot" -ForegroundColor DarkCyan

    $lastStamp = $null
    while (-not $proc.HasExited) {
      if (Test-Path $logRoot) {
        $latest = Get-ChildItem -Path $logRoot -Filter *.txt -ErrorAction SilentlyContinue |
                  Sort-Object LastWriteTime -Descending |
                  Select-Object -First 1
        if ($latest) {
          $tail = Get-Content -Path $latest.FullName -Tail 40 -ErrorAction SilentlyContinue
          $text = $tail -join "`n"
          $stamp = [System.String]::Intern($text)
          if ($stamp -ne $lastStamp) {
            Write-Host ("`n--- {0} (tail 40) ---" -f $latest.Name) -ForegroundColor DarkGray
            Write-Host $text
            $lastStamp = $stamp
          }
        }
      }
      Start-Sleep -Seconds 3
    }

    $proc.WaitForExit()
    return $proc.ExitCode
  }
  else {
    & $uat @args
    return $LASTEXITCODE
  }
}

function New-SummaryRow {
  param([string]$Engine, [string]$OutDir, [int]$Code)
  [PSCustomObject]@{
    Engine = $Engine
    Output = $OutDir
    ExitCode = $Code
    Result = if ($Code -eq 0) { 'Success' } else { 'Failed' }
  }
}

$engines = @((Resolve-Engines -Roots $EngineRoots))
$engineList = @($engines)  # normalize to array
if (-not $engineList -or $engineList.Count -eq 0) { throw "No UE engines found. Provide -EngineRoots or install UE_5.4/5.5/5.6." }

Write-Host "Plugin:   $PluginUplugin" -ForegroundColor Green
Write-Host "Out Base: $OutputBase" -ForegroundColor Green
Write-Host ("Engines:`n  - " + ($engineList -join "`n  - ")) -ForegroundColor Green

$results = @()
foreach ($e in $engineList) {
  $ver = Split-Path $e -Leaf
  $out = Join-Path $OutputBase ("XTools-" + $ver)
  $code = Invoke-BuildPlugin -EngineRoot $e -PluginPath $PluginUplugin -OutDir $out -Platforms $TargetPlatforms -UseStrict:$StrictIncludes -UseNoHostProject:$NoHostProject -FollowLogs:$Follow
  $results += (New-SummaryRow -Engine $ver -OutDir $out -Code $code)
}

Write-Host "`n=== Summary ===" -ForegroundColor Magenta
$results | Format-Table -AutoSize

# Non-zero exit if any failed
if ($results | Where-Object { $_.ExitCode -ne 0 }) { exit 1 } else { exit 0 }

