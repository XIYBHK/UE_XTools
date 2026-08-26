# UE 全自动测试最佳实践

本指南面向 Unreal Engine 项目和插件开发团队，适用于需要真实引擎状态、跨多帧执行或依赖 Editor/World fixture 的自动化测试。文末附一个具体项目案例，读者可替换为自己的模块名、资产路径和测试环境。

## 1. 先定义测试层级

不要把“能编译”当成“行为已验证”。按被测对象选择最小但真实的运行环境：

| 层级 | 适用范围 | 典型 fixture | 运行方式 |
| --- | --- | --- | --- |
| 纯逻辑测试 | 纯函数、边界条件、状态判定 | 无 World 的 C++ 对象 | `IMPLEMENT_SIMPLE_AUTOMATION_TEST` |
| 运行时对象测试 | Actor、Component、Ticker、委托 | `UWorld::CreateWorld`、真实 Actor | Automation Test + 手动 Tick |
| Slate/Editor 测试 | `SWindow`、Editor 工具、资产编辑器 | 真实 `FSlateApplication`、Editor World | Editor Automation |
| 地图集成测试 | NavMesh、物理、关卡 Actor、蓝图 | 固定 `.umap`、测试资产 | Editor/Client Automation |

每个缺陷至少应有一个能复现原始风险的测试层级。地图或 Slate 依赖不能用静态扫描替代。

## 2. Fixture 设计原则

### 2.1 固定、最小、可清理

- fixture 只包含被测行为所需对象，不复用复杂业务地图中的无关 Actor。
- 地图中固定放置地面碰撞、`NavMeshBoundsVolume`、测试起点和目标区域。
- 测试结束时销毁动态 Actor、Ticker、窗口和临时包。
- 测试可重复运行，不能依赖上一次运行留下的对象或文件。

### 2.2 真实 API，测试替身只用于观测

优先调用真实 UE API：

- Slate 使用 `FSlateApplication::AddWindow`、`RequestDestroyWindow` 和 `Tick`。
- 导航使用 `UNavigationSystemV1`、NavMesh 投影、路径查询和 `AAIController::MoveToLocation`。
- 需要统计调用次数时，可在测试专用派生类中覆盖虚函数；不要复制生产逻辑后测试复制品。

### 2.3 异步状态必须显式等待

UE 的窗口销毁、导航建网、Ticker 和路径跟随都可能跨帧完成。测试应使用：

- Slate：调用真实 `FSlateApplication::Tick()`，直到销毁队列处理完成。
- Functional Test：使用 `PrepareTest`、`IsReady`、`OnTestStart`、`FinishTest` 管理多帧准备和结束。
- Automation Spec：用异步 `It`、等待条件和超时表达跨线程/跨帧行为。

禁止使用固定的长时间睡眠代替状态条件；超时必须产生清晰失败信息。

## 3. Slate 生命周期测试

### 3.1 推荐断言

以所有权和终态为核心，而不是只断言按钮函数被调用：

1. 窗口加入 Slate 后有效。
2. 内容控件对窗口只持有 `TWeakPtr`，不形成反向强引用环。
3. `RequestDestroyWindow` 后经过 Slate tick，窗口弱引用失效。
4. 确认和取消回调返回 `FReply::Handled()`。
5. 确认状态和取消状态符合 `ShowDialog` 契约。

### 3.2 测试进程要求

Slate 测试必须运行在真实 Editor/Slate 进程中。`-nullrhi` 可以关闭渲染，但不能替代 Slate 应用循环。纯 commandlet 或无 Slate 环境不能证明窗口生命周期安全。

### 3.3 通用测试模板

测试类应创建自己的 `SWindow` 和内容控件，将窗口弱引用保存到控件，再请求销毁并泵 Slate tick。交互测试可使用 Automation Driver 的稳定 Id 定位控件；不要依赖屏幕坐标。

## 4. NavMesh/AI 地图测试

### 4.1 地图必须验证自身可用

测试加载地图后，先验证 fixture，再验证被测功能：

1. 地图加载成功。
2. `UNavigationSystemV1` 存在。
3. 至少有一个 `ANavMeshBoundsVolume`。
4. 起点和目标都能投影到 NavMesh。
5. `GetPathLength` 或同步路径查询成功且长度有效。
6. `AAIController::MoveToLocation` 返回 `RequestSuccessful` 或 `AlreadyAtGoal`。

如果第 1-5 步失败，应报告 fixture 失效；不要继续把失败归因于业务代码。

### 4.2 推荐的分层断言

导航功能通常拆成三组测试：

- 纯策略测试：目标更新阈值、重寻路条件和边界值。
- 运行时对象测试：真实 World、Pawn、AIController 和完成/中断清理。
- 地图集成测试：固定地图中的 NavMesh 投影、路径查询、MoveTo 请求和最终到达。

分层后，策略回归不会因 Recast 建网时序变动而失去诊断价值；地图测试则负责证明引擎集成链路可用。

## 5. 测试命名与宏

测试路径使用稳定的模块层级：

下面示例中的 `<Project>` 表示项目或插件自己的测试根命名空间，实际使用时应替换为真实名称。

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTest,
    "<Project>.Module.Feature.Behavior",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

建议：

- 名称描述可观察行为，不描述内部实现细节。
- 测试 fixture 类型放在 `Private/Tests` 或测试专用目录。
- 测试代码使用 `#if WITH_DEV_AUTOMATION_TESTS`；Editor/Slate 测试额外使用 `#if WITH_EDITOR`。
- 私有状态只通过测试专用 friend 或测试派生类观察，不新增生产公开 API。

## 6. 命令行和 CI

UE 官方支持通过命令行运行自动化测试并导出 JSON 报告：

命令中的 `<Project>` 和报告目录均为占位符，应按项目的 `.uproject` 路径和 CI 产物目录替换。

```text
UnrealEditor-Cmd.exe <Project>.uproject \
  -unattended -nop4 -nosplash -nullrhi \
  -ExecCmds="Automation RunTests <Project>.Navigation.MapFixture;Quit" \
  -ReportExportPath="Saved/Automation/Reports/NavigationMap"
```

CI 至少检查：

- `succeeded` 大于等于预期数量；
- `failed == 0`；
- `notRun == 0`；
- 报告文件存在；
- 测试前后没有遗留窗口、Ticker、临时地图修改或脏包。

Editor 测试应在独立进程中运行，避免上一个测试的 Editor/World 状态污染下一个测试。

## 7. 失败诊断顺序

出现失败时按以下顺序排查：

1. 测试是否被发现并实际执行。
2. fixture 是否创建/加载成功。
3. 引擎异步状态是否已等待到就绪。
4. 失败断言对应的是产品代码行为还是引擎前置条件。
5. 是否存在测试之间的全局状态污染。

日志中的预期业务 Warning 不应直接当作测试失败；但真正的 `Error`、崩溃或未清理状态必须保留并修复。

## 8. 人工验收边界

全自动测试不能覆盖所有体验和外部资产风险：

- Slate 的视觉布局、焦点、高 DPI、不同窗口管理器仍需人工抽样。
- 地图测试证明固定 fixture 的导航行为，不代表所有业务关卡均可达。
- 项目仓库无法自动证明外部项目中的蓝图资产兼容；应对已知资产集做 Editor 自动加载/编译，并把外部资产作为发布验收门禁。

人工验收是自动化测试的补充，不应替代可重复的行为断言。

## 9. 项目案例（可选附录）

```text
Automation RunTests <Project>.UI.Dialog.WindowLifecycle
Automation RunTests <Project>.Navigation.AIMoveTo.RepathThreshold
Automation RunTests <Project>.Navigation.AIMoveTo.CompletionStopsMovement
Automation RunTests <Project>.Navigation.MapFixture
```

以下为一个项目的落地示例：

- `XTools.AssetEditor.AutoConvexDialog.WindowLifecycle`
- `XTools.SplineMovement.AIMoveTo.RepathThreshold`
- `XTools.SplineMovement.AIMoveTo.CompletionStopsMovement`
- `XTools.SplineMovement.NavigationMapFixture`

新增测试时，应同步更新项目自己的变更日志、测试清单和 CI 过滤器；不要把上述示例名称直接复制到其他项目。
