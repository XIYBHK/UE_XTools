# UE 无头自动化测试最佳实践

> 支持范围：UE_XTools，Unreal Engine 5.3-5.8，Windows / PowerShell。
> 本地开发基线：`D:\UEProject\cppxtools` 的 Unreal Engine 5.3 项目。
> 调研与本地验证日期：2026-07-23。
> 本文的默认方案是在无 GUI 的真实 Unreal Editor 进程中运行 UE Automation Test Framework，不是用普通进程绕过引擎测试。

## 1. 核心结论

Session Frontend 和 `UnrealEditor-Cmd.exe` 是同一套 Automation Framework 的不同入口：

- Session Frontend 适合开发阶段筛选测试、查看事件和人工诊断。
- `UnrealEditor-Cmd.exe` 适合可重复的本地验证、批处理和 CI。
- `-NullRHI` 只是不创建真实渲染设备，并不绕过模块加载、UObject、World 或 Automation Controller/Worker。
- UBT/UHT 构建成功只能证明代码能够生成反射代码、编译和链接，不能证明运行时行为正确。
- 无头测试的成功判定不能只看进程退出码；还必须确认匹配到预期数量的测试，并检查 Automation 报告或测试完成日志。

本项目日常开发的推荐顺序：

1. UE 5.3 UHT/C++ 构建。
2. UE 5.3 `UnrealEditor-Cmd` 运行受影响测试前缀。
3. 本地小功能不额外编译 UE 5.4-5.8；跨版本问题留到版本更新/发布 CI 矩阵处理。
4. 只有已打包游戏、真实平台、多进程或联机测试才升级到 Gauntlet。

## 2. 选择正确的测试层级

| 验证目标 | 推荐工具 | 是否适合 `-NullRHI` | 说明 |
|---|---|---:|---|
| UHT、IWYU、编译、链接 | UBT `Build.bat` | 不适用 | 构建验证，不是行为测试 |
| 函数库、算法、容器、确定性状态 | Automation Simple Test | 是 | UE_XTools 当前首选 |
| 多场景、共享 Setup/Teardown、异步回调 | Automation Spec | 通常是 | 使用 `BeforeEach`、`AfterEach`、`LatentIt` |
| UObject、资产加载、编辑器 API | Automation Test / Spec | 视依赖而定 | 仍由真实 Editor 进程加载模块 |
| Actor、关卡、Blueprint 功能 | Functional Testing / Latent Test | 先验证 | 需要 World/PIE，不等于一定需要 GPU |
| Slate 输入和编辑器交互 | Automation Driver | 通常否 | 需要可用窗口和输入路径时不要使用 NullRHI |
| 材质、Niagara、Render Target、截图比较 | Screenshot / Functional Test | 否 | 必须使用真实 RHI 和合适的 GPU Agent |
| 纯 C++、希望最小引擎启动成本 | UE Low-Level Tests (Catch2) | 不适用 | 需要独立测试 Target/构建流程，当前仓库尚未采用 |
| 已打包游戏、主机、多客户端/服务器 | Gauntlet | 按场景 | 管理一个或多个 Unreal 会话，适合端到端验证 |

不要为了“无头”强行把所有测试都塞进 `-NullRHI`。是否需要 GUI、是否需要 Editor、是否需要 RHI 是三个独立问题。

## 3. 本项目测试组织规范

### 3.1 文件位置

模块测试放在被测模块内部：

```text
Source/<Module>/Private/Tests/<Feature>Tests.cpp
```

当前示例：

- [`RectangleGridSamplingTests.cpp`](../../Source/PointSampling/Private/Tests/RectangleGridSamplingTests.cpp)
- [`BezierMissileTrajectoryTests.cpp`](../../Source/XTools/Private/Tests/BezierMissileTrajectoryTests.cpp)

测试不应为了访问私有实现而破坏模块边界。优先测试公开 API 的可观察行为；确实需要大量白盒测试时，再评估拆出可独立测试的内部类型。

### 3.2 最小测试模板

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "MyFeatureLibrary.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMyFeature_RejectsInvalidInput,
    "XTools.MyModule.MyFeature.RejectsInvalidInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMyFeature_RejectsInvalidInput::RunTest(const FString& Parameters)
{
    const bool bResult = UMyFeatureLibrary::TryDoWork(/* ... */);
    TestFalse(TEXT("无效输入应被拒绝"), bResult);
    return true;
}

#endif
```

UE 5.3 和较新版本的 `ApplicationContextMask` 定义位置不同。为减少版本更新 CI 中的兼容问题，本仓库使用显式组合：

```cpp
EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
```

每个测试必须提供至少一个运行上下文和恰好一个 Filter。不要直接使用跨版本形态不同的 `ApplicationContextMask`。

### 3.3 命名

统一使用层级名称：

```text
XTools.<Module>.<Feature>.<Behavior>
```

例如：

```text
XTools.Bezier.MissileTrajectory.DegenerateInputsRemainFinite
XTools.PointSampling.RectangleGrid.GeneratesIndependentSpacing
```

好处是既可运行单个测试，也可按 Feature、Module 或整个 `XTools` 前缀运行。

### 3.4 测试设计

- 每个测试必须可以独立运行，不依赖执行顺序。
- 不假设 Editor、关卡、配置、磁盘或单例处于干净状态。
- 使用固定输入、固定种子和显式时间，不依赖墙钟时间或未播种随机数。
- 测试创建的 UObject、资产、配置和临时文件必须清理；开始前也要能处理上次异常退出留下的状态。
- 验证边界值、无效输入、退化输入和正常路径，避免只测 Happy Path。
- 测试公开结果，不要复制一份被测算法作为 Expected Result。
- 日志中的 Error/Warning 可能影响测试结果。预期错误使用 `AddExpectedError` / `AddExpectedMessage`，不要全局关闭日志检查。
- 只有承诺在 1 秒内完成的快速 Unit/Feature Test 才标记 `SmokeFilter`。
- 涉及多个 Tick、异步加载或回调时使用 Latent Command 或 Automation Spec 的 `LatentIt`，不要在 Game Thread 上 `Sleep` 或忙等。

## 4. 构建后再运行测试

测试源文件必须先被目标编译。推荐先执行最低支持版本 UE 5.3 构建：

```powershell
$buildExe = 'D:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat'
$buildArguments = @(
    'cppxtoolsEditor'
    'Win64'
    'Development'
    '-Project=D:\UEProject\cppxtools\cppxtools.uproject'
    '-NoHotReloadFromIDE'
)

& $buildExe @buildArguments
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) {
    throw "UE build failed with exit code $buildExitCode"
}
```

不要把带空格路径和整条命令拼成一个字符串，也不要额外套 `cmd.exe /c`。

如果 Editor 正在运行并锁定插件 DLL，应先正常关闭 Editor，再进行正式构建。不要把 Live Coding 成功当成正式构建通过。

## 5. 无头运行 Automation Test

### 5.1 推荐 PowerShell 命令

下面的命令按前缀运行贝塞尔导弹轨迹测试，并为每次运行创建独立报告目录：

```powershell
$engineRoot = 'D:\Program Files\Epic Games\UE_5.3'
$projectPath = 'D:\UEProject\cppxtools\cppxtools.uproject'
$projectRoot = Split-Path -Parent $projectPath
$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$reportPath = Join-Path $projectRoot "Saved\Automation\Reports\XTools-Bezier-$runStamp"
$logName = "Automation-XTools-Bezier-$runStamp.log"
$editorExe = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

$testArguments = @(
    $projectPath
    '-unattended'
    '-nop4'
    '-NullRHI'
    '-nosplash'
    '-ExecCmds=Automation RunTests XTools.Bezier.MissileTrajectory; Quit'
    '-TestExit=Automation Test Queue Empty'
    "-ReportExportPath=$reportPath"
    "-log=$logName"
)

& $editorExe @testArguments
$testExitCode = $LASTEXITCODE
if ($testExitCode -ne 0) {
    throw "UE automation process failed with exit code $testExitCode"
}

$reportFile = Join-Path $reportPath 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) {
    throw "Automation report was not generated: $reportFile"
}

$expectedTestCount = 3
$report = Get-Content -Raw -LiteralPath $reportFile | ConvertFrom-Json
if (
    $report.succeeded -ne $expectedTestCount -or
    $report.succeededWithWarnings -ne 0 -or
    $report.failed -ne 0 -or
    $report.notRun -ne 0 -or
    $report.inProcess -ne 0
) {
    throw "Automation result is incomplete or failed. Report: $reportFile"
}
```

注意：

- UE 5.8 官方页面示例写作 `Automation RunTest`；本项目 UE 5.3 引擎源码帮助和实际执行均确认 `Automation RunTests`。项目命令统一使用已验证的复数形式。
- 使用 `-ReportExportPath`。旧参数 `-ReportOutputPath` 在 UE 5.3 已被标记为旧名称并会产生警告。
- `-TestExit="Automation Test Queue Empty"` 让进程在测试队列完成时退出；`Automation ...; Quit` 同时把退出动作排入 Automation 命令队列。
- `-unattended` 防止模态对话框等待人工输入。
- `-nop4` 防止测试流程访问 Perforce。
- 报告目录和日志文件应按运行唯一命名，避免把上一次结果误认为本次结果。
- `$expectedTestCount` 必须随测试集合更新；它可以防止过滤器拼错、测试未编译或插件未加载时产生“0 个测试但流程结束”的假成功。

### 5.2 运行粒度

开发阶段只运行受影响的最小稳定前缀：

```text
Automation RunTests XTools.Bezier.MissileTrajectory
Automation RunTests XTools.PointSampling.RectangleGrid
Automation RunTests XTools
```

不要默认执行 `Automation RunAll`。Engine、插件和项目测试数量很大，既浪费时间，也会引入与当前修改无关的环境噪声。

需要组合多个分散前缀时，可以在 `DefaultEngine.ini` 配置 Test Group：

```ini
[/Script/AutomationController.AutomationControllerSettings]
+Groups=(Name="XToolsFast",Filters=((Contains="XTools.",MatchFromStart=true)))
```

然后运行：

```text
Automation RunTests Group:XToolsFast
```

### 5.3 什么时候移除 `-NullRHI`

以下测试必须移除 `-NullRHI`，并在具有合适 GPU/驱动的 Agent 上运行：

- 截图和像素比较。
- 材质、后处理、Niagara、Render Target。
- GPU Compute、RHI Readback、Shader 行为。
- 依赖真实 Slate 窗口绘制或输入命中的编辑器交互。

如果测试宏带有 `EAutomationTestFlags::NonNullRHI`，就不应使用 NullRHI 运行该测试。

Actor、World、PIE 和资产加载并不天然要求真实 RHI。先用 NullRHI 验证；只有出现明确的渲染依赖时才升级到 GPU 测试，避免无谓增加 CI 成本和波动。

## 6. 成功判定和防误报

一次可信的测试运行至少同时满足以下条件：

1. `UnrealEditor-Cmd.exe` 进程退出码为 0。
2. 日志出现 `Found N automation tests`，并且 `N` 等于预期数量且大于 0。
3. 每个测试都有 `Test Completed`，结果为成功或预期的跳过。
4. 日志出现 `TEST COMPLETE. EXIT CODE: 0`。
5. `-ReportExportPath` 下生成新的 `index.json`，CI 保存 JSON/HTML 和相关 Artifact。

本项目已验证的日志形态：

```text
Found 3 automation tests based on 'XTools.Bezier.MissileTrajectory'
Test Completed. Result={成功}
TEST COMPLETE. EXIT CODE: 0
```

最危险的误报是测试过滤器写错：进程可能正常启动并退出，但实际匹配 0 个测试。因此不能只检查 `$LASTEXITCODE`。

推荐 CI 把以下内容作为构建产物保留：

- `<Project>/Saved/Automation/Reports/<RunId>/index.json`
- `<Project>/Saved/Automation/Reports/<RunId>/index.html`
- `<Project>/Saved/Logs/<AutomationLog>.log`
- 截图测试生成的 Comparison / Artifact 目录

## 7. 异步、World 和功能测试

### 7.1 不要阻塞 Game Thread

加载地图、等待下一帧、异步回调和 PIE 状态变化都应使用：

- `ADD_LATENT_AUTOMATION_COMMAND`
- 自定义 `IAutomationLatentCommand`
- Automation Spec 的 `LatentBeforeEach` / `LatentIt` / `LatentAfterEach`

Latent Command 的 `Update()` 应在条件满足时返回 `true`，否则返回 `false` 让 Automation Framework 在后续 Tick 继续检查。必须设计超时和失败信息，避免无限等待。

### 7.2 何时使用 Automation Spec

出现以下任一情况时，优先考虑 Spec，而不是继续扩张一个大型 Simple Test：

- 多个测试共享初始化和清理。
- 需要 `Describe` / `It` 清楚表达行为组合。
- 有多段异步步骤或线程切换。
- 需要参数化生成彼此隔离的测试用例。

简单、同步、单一公开 API 的测试继续使用 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`，不为形式统一而迁移到 Spec。

### 7.3 何时使用 Functional Testing

当验收目标是“某个 Actor 在某张地图中完成完整行为”时，使用 Functional Testing 或专用测试地图通常比在 C++ 测试里手工拼装整个关卡更清晰。测试地图必须保持最小、确定，并避免依赖编辑器当前打开的关卡。

## 8. 测试隔离、日志和临时排除

Epic 官方建议假设测试会乱序运行、跨机器并行，并且上一次运行可能留下坏状态。因此：

- 不共享可变全局状态；必须共享时在每个测试中重置。
- 文件测试使用唯一临时目录，并在开始和结束阶段清理自己的目标。
- 不修改用户资产、编辑器偏好或仓库文件来完成测试。
- 不依赖本机用户名、固定盘符之外的隐式环境状态；CI 中通过参数显式提供路径。
- 不把普通 Warning 全部屏蔽。只对已知、确实属于预期行为的消息使用 Expected Message。

确需临时排除已知失败时，在 `DefaultEngine.ini` 中记录完整测试名和明确原因：

```ini
[AutomationTestExcludelist]
+ExcludeTest=(Test="XTools.SomeModule.SomeFeature.KnownFailure",Reason="Issue-XT-123",Warn=true)
```

排除必须绑定问题编号并及时移除。不要用宽泛前缀长期隐藏整个模块；RHI 特定排除也只用于已经确认的平台/RHI 差异的测试。

## 9. CI 分层建议

### 每次相关修改

- 使用 UE 5.3 基线项目编译。
- 运行受影响的 `XTools.<Module>.<Feature>` 前缀。
- 保存 Automation 报告和日志。

### 合并前

- 使用 UE 5.3 基线项目完成 UHT/C++ 构建。
- 运行受影响模块前缀。
- 涉及渲染时，在真实 RHI Agent 上运行对应测试。

### 版本更新或发布前

- UE 5.3-5.8 BuildPlugin 矩阵。
- 在 CI 中处理 UE 5.4-5.8 暴露的编译兼容问题，不把这些版本作为日常本地小功能的验证门槛。
- 运行约定的稳定测试组。
- 检查每个矩阵 Job，而不是只看 Workflow 总体状态。
- 需要已打包行为、目标平台或多进程验证时运行 Gauntlet。

如果外层命令工具超时但 UBT、UAT 或 UnrealEditor 子进程仍在运行，不要立即再启动一份相同任务。先检查进程和最终日志，避免并行写入同一 Intermediate、Binaries 或报告目录。

## 10. 常见问题

### GUI 中能看到测试，命令行却匹配不到

检查：

- 测试是否被当前 Target 编译，`WITH_DEV_AUTOMATION_TESTS` 是否启用。
- 测试运行上下文是否包含 `EditorContext`。
- 命令是否使用准确的层级前缀。
- 插件/模块是否在该项目中启用并成功加载。
- `.uplugin` 的 `EngineVersion` 是否与启动的 Editor 兼容；`-unattended` 遇到兼容性确认框时可能自动拒绝并记录 `Skipping load of '<Plugin>'`。
- 日志中的 `Found N automation tests`，不要只检查退出码。

本项目 `XTools.uplugin` 当前声明 `EngineVersion=5.3.0`，本地构建和无头测试统一使用 UE 5.3 基线项目。版本更新时，由 CI 使用为目标版本正确打包/声明兼容的插件副本执行 UE 5.4-5.8 验证。跨版本编译通过不等于跨版本运行时模块已经加载。

### 无头测试是否等于 Commandlet

不是。`UnrealEditor-Cmd.exe` 是命令行入口；本方案仍启动 Editor 引擎循环并运行 Automation Controller/Worker。只有使用 `-run=<Commandlet>` 才是在运行具体 Commandlet。

### 是否应该始终添加 `-NullRHI`

不应该。它适合不验证渲染的算法、UObject、资产和许多 World 测试。渲染、截图、真实窗口交互和 GPU 功能必须使用真实 RHI。

### 为什么不直接采用 Low-Level Tests

Low-Level Tests 使用 Catch2，启动和资源成本更低，适合严格的模块级 C++ 测试；但它需要单独的 Test Target 和构建工作流。本项目现有测试依赖插件模块、UObject 和 Editor 加载，Automation Framework 已形成稳定流程。只有出现大量纯 C++ 快速测试并且 Editor 启动时间成为瓶颈时再引入 LLT。

### 什么时候需要 Gauntlet

当测试对象不再是一个 Editor 内的 API，而是一个完整 Unreal 会话时：例如已打包客户端、Dedicated Server、多客户端、目标主机或长时间端到端流程。Gauntlet 是会话编排和结果验证层，不替代日常 UHT/C++ 构建与细粒度 Automation Test。

## 11. 官方资料

- [Automation Test Framework](https://dev.epicgames.com/documentation/unreal-engine/automation-test-framework-in-unreal-engine?lang=en-US)
- [Run Automation Tests](https://dev.epicgames.com/documentation/unreal-engine/run-automation-tests-in-unreal-engine?lang=en-US)
- [Configure Automation Tests](https://dev.epicgames.com/documentation/unreal-engine/configure-automation-tests-in-unreal-engine?lang=en-US)
- [Automation Spec](https://dev.epicgames.com/documentation/unreal-engine/automation-spec-in-unreal-engine?lang=en-US)
- [Low-Level Tests](https://dev.epicgames.com/documentation/unreal-engine/low-level-tests-in-unreal-engine?lang=en-US)
- [Gauntlet Automation Framework](https://dev.epicgames.com/documentation/unreal-engine/gauntlet-automation-framework-in-unreal-engine?lang=en-US)

引擎版本升级时，在 CI 中重新核对命令行参数、`EAutomationTestFlags` 定义和 Automation 报告格式。官方网页当前展示 UE 5.8 行为；本文的日常本地命令以 UE 5.3 基线项目的实际运行结果为准。
