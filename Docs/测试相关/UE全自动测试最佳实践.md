# Unreal Engine 全自动测试最佳实践

本文面向 Unreal Engine 5.3-5.8 的项目和插件开发。目标是在真实引擎进程中，以可重复、可诊断、可进入 CI 的方式验证纯逻辑、UObject、World、Slate、导航、资产和端到端行为。

## 1. 基本原则

1. 构建成功只证明 UHT、编译和链接通过，不等于行为正确。
2. 使用能复现风险的最小真实 fixture，不用静态扫描替代运行时契约。
3. 无头、Editor 和 NullRHI 是三个独立维度。
4. 异步测试等待状态条件，不在游戏线程 `Sleep` 或忙等。
5. 成功判定必须同时检查进程退出码、报告存在、测试数量和结果字段。
6. 测试可独立重复运行，并清理创建的 World、窗口、Ticker、资产和临时文件。

## 2. 选择测试层级

| 验证目标 | 推荐工具 | 典型 fixture | `-NullRHI` |
| --- | --- | --- | --- |
| 纯函数、容器、边界条件 | Simple Automation Test | 无 World | 适合 |
| 多场景、共享 Setup/Teardown | Automation Spec | UObject 或轻量 World | 通常适合 |
| Actor、Component、Ticker、委托 | Automation Test / Latent Command | `UWorld::CreateWorld`、真实 Actor | 视依赖 |
| Slate 与编辑器工具 | Editor Automation / Automation Driver | `FSlateApplication`、`SWindow` | 视输入和渲染需求 |
| NavMesh、物理、关卡 Actor | Functional Test / 地图 fixture | 固定 `.umap` | 先验证 |
| Blueprint 编译和执行 | Editor Automation | 临时 Blueprint 或测试资产 | 通常适合 |
| 渲染、材质、Niagara、截图 | Screenshot / Functional Test | 真实 RHI 和 GPU | 不适合 |
| 纯 C++ 低启动成本 | UE Low-Level Tests | 独立 Test Target | 不适用 |
| 已打包游戏、多进程、联机 | Gauntlet | 一个或多个 Unreal 会话 | 按场景 |

优先选择最低成本且仍能触达真实缺陷的层级。需要 World、Slate 或 Blueprint 字节码时，不要把生产逻辑复制成纯函数后只测试复制品。

## 3. 测试组织

插件模块测试通常放在：

```text
Source/<Module>/Private/Tests/<Feature>Tests.cpp
```

测试名称使用稳定层级：

```text
<Product>.<Module>.<Feature>.<Behavior>
```

示例：

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFeatureRejectsInvalidInput,
    "Product.Module.Feature.RejectsInvalidInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFeatureRejectsInvalidInput::RunTest(const FString& Parameters)
{
    const bool bResult = TryDoWork(/* invalid input */);
    TestFalse(TEXT("无效输入应被拒绝"), bResult);
    return true;
}

#endif
```

规则：

- 测试名描述外部可观察行为，不绑定私有实现细节。
- 每个测试提供运行 Context 和一个 Filter。
- Editor 专属测试再使用 `#if WITH_EDITOR`。
- 不为测试新增生产公开查询 API；优先用公开结果、测试派生类或最小 friend。
- 预期 Error/Warning 使用 `AddExpectedError` / `AddExpectedMessage`，不要全局关闭日志检查。
- 只有稳定在一秒内完成的测试才标记 `SmokeFilter`。

## 4. 确定性设计

### 4.1 输入和期望值

- 随机算法使用固定种子或可注入随机源。
- 时间相关逻辑使用显式 Delta，不依赖墙钟时间。
- 并发测试断言最终不变量，不依赖线程执行顺序。
- 浮点排序同时覆盖 `NaN`、`±Inf`、`±0` 和重复值。
- 数组/采样测试同时验证数量、顺序、边界、退化输入和确定性。
- 不复制生产算法来计算 Expected Result；使用手算小样本或不变量。

### 4.2 异步状态

跨帧逻辑使用 Latent Command、Spec 的 `LatentIt` 或 Functional Test 生命周期：

- `PrepareTest` / `IsReady` 等待 fixture 就绪；
- `OnTestStart` 发起操作；
- 条件满足后 `FinishTest`；
- 超时给出具体的未满足状态。

固定长时间睡眠既慢又不可靠。只有在验证真实时间语义且无法注入时钟时，才考虑受控的时间等待。

### 4.3 隔离和清理

- 测试开始前不假设单例、配置或磁盘状态干净。
- 把全局配置的旧值保存并在结束时恢复。
- 销毁动态 Actor、World、Slate 窗口和临时 UObject。
- 取消 Timer、Ticker、异步句柄和委托。
- 临时资产使用唯一包名，并在结束时卸载或删除。
- 报告目录每次运行唯一，避免误读旧结果。

## 5. World 与地图 fixture

### 5.1 轻量 World

Actor/Component 行为可以在测试内创建 `UWorld`，生成最小对象并手动 Tick。适合不依赖关卡烘焙数据的生命周期、委托和状态测试。

测试结束时按引擎约定清理 World，不能让 WorldContext、Actor 或 Timer 泄漏到下一项测试。

### 5.2 固定地图

NavMesh、物理体积、关卡蓝图和资产引用需要固定 `.umap` 时，使用独立的最小地图 fixture。测试应先验证地图自身，再验证业务行为：

1. 地图成功加载；
2. 需要的系统和 Actor 存在；
3. 烘焙或运行时数据已就绪；
4. 起点、目标和碰撞条件有效；
5. 业务操作成功并到达可观察终态。

fixture 失效应报告为 fixture 错误，不能伪装成产品回归。

### 5.3 NavMesh/AI

导航地图至少验证：

- `UNavigationSystemV1` 存在；
- `ANavMeshBoundsVolume` 存在；
- 起点和目标可投影到 NavMesh；
- 同步路径查询成功且路径长度有效；
- `MoveToLocation` 返回 `RequestSuccessful` 或 `AlreadyAtGoal`；
- 完成或取消后移动状态和委托得到清理。

策略阈值、重寻路条件应另有纯逻辑测试。地图测试负责证明引擎集成，不应承担所有边界组合。

## 6. Slate 生命周期测试

Slate 测试必须在真实 Slate 应用中创建窗口和内容控件。推荐断言：

1. `SWindow` 加入应用后有效；
2. 内容控件反向只持有 `TWeakPtr<SWindow>`；
3. 确认和取消回调返回正确的 `FReply`；
4. `RequestDestroyWindow` 后泵 `FSlateApplication::Tick()`；
5. 销毁队列完成后窗口弱引用失效；
6. 确认状态与对话框返回契约一致。

Automation Driver 应通过稳定 Id 查找控件，不依赖屏幕坐标。`-NullRHI` 不等于没有 Slate 循环；但需要真实输入、窗口管理器或视觉验证时，应在带桌面会话的 Agent 上运行。

## 7. Blueprint 与资产测试

- `CustomThunk` 和 K2Node 应通过真实 Blueprint 图编译与执行验证，不能把 thunk 当普通 C++ UFunction 直接调用来代替字节码路径。
- 反射类型删除或重命名前，准备包含相关变量/节点的测试资产，自动加载并编译。
- 外部项目资产无法由插件仓库单独证明兼容；把已知下游资产集作为发布门禁。
- 测试资产保持最小，避免依赖业务关卡或内容团队仍在编辑的资产。

## 8. 构建后运行

先构建包含测试实现的 Editor Target：

```powershell
$buildExe = '<EngineRoot>\Engine\Build\BatchFiles\Build.bat'
$buildArgs = @(
    '<ProjectName>Editor'
    'Win64'
    'Development'
    '-Project=<Project>\Project.uproject'
    '-NoHotReloadFromIDE'
)

& $buildExe @buildArgs
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) {
    throw "UE build failed with exit code $buildExitCode"
}
```

每个 native 参数都是独立数组元素。不要把整条命令拼成字符串，也不要额外套 `cmd.exe /c`。

## 9. 命令行运行 Automation

```powershell
$engineRoot = '<EngineRoot>'
$projectPath = '<Project>\Project.uproject'
$projectRoot = Split-Path -Parent $projectPath
$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$reportPath = Join-Path $projectRoot "Saved\Automation\Reports\Feature-$runStamp"
$logName = "Automation-Feature-$runStamp.log"
$editorExe = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

$testArgs = @(
    $projectPath
    '-unattended'
    '-nop4'
    '-nosplash'
    '-NullRHI'
    '-ExecCmds=Automation RunTests Product.Module.Feature'
    '-TestExit=Automation Test Queue Empty'
    "-ReportExportPath=$reportPath"
    "-log=$logName"
)

& $editorExe @testArgs
$testExitCode = $LASTEXITCODE
if ($testExitCode -ne 0) {
    throw "UE automation process failed with exit code $testExitCode"
}
```

关键点：

- 使用 `Automation RunTests` 运行前缀或完整名称。
- 使用 `-ReportExportPath` 导出 JSON/HTML 报告。
- 使用 `-TestExit=Automation Test Queue Empty` 等待队列完成。
- 使用 `-TestExit` 时不要把 `Quit` 加进 `ExecCmds`，否则可能在报告落盘前退出。
- 只有测试不依赖渲染时才加 `-NullRHI`。
- `-unattended` 防止模态对话框阻塞 CI。

## 10. 严格判定报告

不能只检查 `$LASTEXITCODE`。至少读取 `index.json`：

```powershell
$reportFile = Join-Path $reportPath 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) {
    throw "Automation report was not generated: $reportFile"
}

$expectedTestCount = 6
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
$completed = $report.succeeded + $report.succeededWithWarnings + $report.failed

if (
    $completed -ne $expectedTestCount -or
    $report.failed -ne 0 -or
    $report.notRun -ne 0 -or
    $report.inProcess -ne 0
) {
    throw "Automation result is incomplete or failed: $reportFile"
}
```

`$expectedTestCount` 应由当前筛选器的受控清单或 CI 配置维护。它用于防止过滤器拼错、测试未编译或插件未加载时出现“0 项测试但进程正常退出”的假成功。

对 `succeededWithWarnings` 的策略应显式定义：核心门禁通常要求为 0；已知引擎噪音必须有来源说明和独立清理计划，不能无限期豁免。

## 11. CI 分层

建议按成本分层：

1. 每次提交：编译 + 受影响模块的快速确定性测试；
2. 主分支：完整 Editor 自动化、World/地图 fixture；
3. 发布：UE 5.3-5.8 BuildPlugin 矩阵、资产加载/Blueprint 编译、必要的 GPU/截图测试；
4. 端到端：已打包游戏、多客户端或平台测试使用 Gauntlet。

不同层级使用独立进程和报告目录。地图、Slate、全局配置修改类测试不应与其他测试共享残留状态。

## 12. 失败诊断

按顺序排查：

1. 测试是否被发现，数量是否符合预期；
2. 正确模块和测试代码是否已编译；
3. fixture 是否成功创建或加载；
4. World、Slate、NavMesh、资产等前置状态是否就绪；
5. 异步等待条件是否表达了真实终态；
6. 断言失败属于产品行为还是 fixture/环境问题；
7. 是否存在上一个测试留下的全局状态。

保留完整日志和报告。预期日志应在测试中声明，不要通过过滤日志掩盖真实错误。

## 13. 自动化与人工验收边界

以下内容仍适合人工抽样，但不应取代自动化：

- Slate 的视觉布局、焦点、高 DPI 和不同窗口管理器表现；
- 固定 fixture 之外的业务关卡可达性；
- 外部项目中未纳入测试资产集的 Blueprint 兼容；
- 主观交互体验和视觉质量。

凡是能表达为确定状态、资产加载结果、委托次数、引用终态或路径查询结果的行为，都应优先自动化。
