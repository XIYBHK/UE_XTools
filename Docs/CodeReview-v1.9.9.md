# UE_XTools v1.9.9 全面代码审查报告

> 初审基线：commit `695be82`（main 分支）；当前收尾核验基线：`d63557d`。
> 审查方式：主审精读 XToolsCore 基础层 + 跨模块交叉检查；7 个并行深审组逐文件通读全部 26 个模块源码；关键引擎行为疑点均对照本机引擎源码（UE 5.0 / 5.3 / 5.5）或 Epic 官方文档核实，不采信臆测。
> 发现统计（初审）：高危 6 · 中危 28 · 低危 49，合计 83 条。
> **实证核验后**：经网络调研 + 本机 UE 5.0–5.8 全版本引擎源码交叉验证，**撤销误报 5 条**（H4、M9、M26、PointSampling 矩形网格溢出、圆形缓存流状态）、**修正 1 条**（M1 部分证伪）、其余关键断言全部确认成立（详见第九节核验附录）。修正后口径：**高危 5 · 中危 25 · 低危 48**，合计 **78 条有效发现**。M-12 排障中新提出且未计入统计的 M29 也已通过受控复验证伪：原生接口派发正常，异常来自未初始化的测试世界。

---

## 一、仓库概览

- **定位**：UE 5.3–5.8 模块化蓝图工具插件，v1.9.9，Runtime C++ 功能库 + UncookedOnly K2Node 编辑器节点双层架构，中文元数据优先。
- **规模**：26 个模块（`AGENTS.md`/`CLAUDE.md` 已同步），670 个源文件约 14 万行；其中自研约 7.1 万行，第三方汉化 fork（AutoSizeComments / BlueprintAssist / ElectronicNodes / BlueprintScreenshotTool）约 7 万行。
- **分层**：PreDefault 的 XToolsCore 兼容层 → 12 个 Runtime 模块 → 5 个 UncookedOnly 编辑器节点模块 → Editor 工具模块。
- **质量基建**：27 个自动化测试文件（含 720 行 K2Node 展开拓扑实测）；GitHub Actions 多版本矩阵构建（5.3–5.8）+ 标签触发 Release；UNRELEASED 变更记录与 git 提交吻合度高；版本号在 uplugin / XToolsDefines.h / README 三处一致；仓库无二进制残留入库。

---

## 二、总体结论

整体工程质量**高于典型社区插件水准**，但呈明显的"梯队分化"：

| 梯队 | 模块 | 特征 |
|------|------|------|
| 第一梯队 | QueueSpline、RandomShuffles、五个延迟循环节点族、XToolsCore、Sort 自然排序、X_AssetEditor 防御层 | 设计严谨、有测试回归、生命周期管理完整 |
| 第二梯队 | ECF 异步框架核心、ComponentTimeline、PointSampling 核心、FormationSystem、GeometryTool、FieldSystemExtensions、BlueprintExtensionsRuntime | 功能正确，债务集中在一致性 |
| 第三梯队 | **ObjectPool**、AdvancedControlFlow 三节点、旧 Map 系列 K2Node、CI 发布链 | 宣称能力 > 实际接线，或存在可机械修复但真实存在的缺陷 |

三条主线结论：

1. **历史实现与公开能力的落差已完成收敛**。ObjectPool 的自动维护死入口已删除，StateResetter/MemoryOptimizer 的兼容保留状态已明示，活路径和公共维护 API 均有测试；ECF `CppStandard=Default` 的不可用断言已由 UE 5.3–5.8 引擎源码核验撤销。剩余工作属于下一大版本 API 清理和文档维护，不再是当前高危缺陷。
2. **新旧代码规范仍有梯队差异**。K2Node 新骨架已统一，旧节点仍存在零散 `TryCreateConnection`、`MessageLog.Error`、裸 `FindUField` 等维护债务；这些不阻塞当前构建和已覆盖的行为测试。
3. **后续重点是规范一致性而非运行时抢修**：全仓中文元数据、统一错误上报宏覆盖率、重复 K2/Timeline helper 整理，以及 ObjectPool 下一大版本公共 API 清理。

---

## 三、高危发现（初审 6 条，现行有效 5 条）

| # | 位置 | 问题 | 建议 |
|---|------|------|------|
| H1 | ~~已处理~~ `Source/XTools_EnhancedCodeFlow/Public/Coroutines/ECFCoroutineAwaiters.h:35-63` | 子系统获取失败的兜底路径曾在 `await_suspend` 内 `resume()` 后同步 `destroy()`，构成协程帧 UAF | **已处理（3cf598d）**：失败路径改为下一 tick 延迟销毁，并以 `bFailureCleanupArmed` 防止重复调度；协程动作按 `ActionHandle` 归属回收。 |
| H2 | ~~已处理~~ `Source/ObjectPool/Private/ObjectPoolSubsystem.cpp:295-338`（配合 `ActorPool.cpp`） | 延迟获取链路曾在池满/创建失败时直接返回 nullptr，K2Node 展开后静默输出 None | **已处理（37f5145）**：`AcquireDeferredFromPool` 回退 `SpawnActorDeferred`，登记 `DeferredFallbackActors`，`FinalizeSpawnFromPool` 完成构造并激活；对象池回退与不可生成类的 nullptr 契约由 `XTools.ObjectPool.Library` 测试覆盖。 |
| H3 | ~~已处理~~ `Source/ObjectPool/Private/ObjectPoolUtils.cpp:90-120`（配合 `ActorPool.cpp:514-520`、`ObjectPoolPreallocator.cpp`） | 激活曾无条件覆写根组件碰撞/物理配置，预热也可能先禁用后错误保存 | **已处理（37f5145）**：激活按归还前快照恢复碰撞与 `bSimulatePhysics`；预热/预分配在禁用前先保存原始设置，避免首轮循环固化错误值。 |
| H4 | ~~已撤销（实证核验证伪）~~ `Source/AxisLocker/Private/AxisLockLibrary.cpp` | 初审声称"引擎 SetDOFLock 仅在 DOFMode 变化时才重建约束、模式未变早退"。经本机 UE **5.3/5.4/5.5/5.6/5.7/5.8 全六版本** BodyInstance.cpp 核验：`SetDOFLock` 一律无条件调用 `CreateDOFLock()`（Term 旧约束后按 6 开关重建），与代码注释完全一致；UnlockAll 走"全 false 不创建约束"路径也正确。子代理误读了论坛帖语境，**该条为误报，撤销** | 无需修复 |
| H5 | ~~已处理~~ `Source/PointSampling/Private/Algorithms/PoissonSamplingHelpers.cpp:421-433` | `TrimToOptimalDistribution` 曾让 `ToRemove<10` 分支绕过 `BatchSizeLimit=2000`，大数组少量裁剪可能触发 O(N²) 卡顿 | **已处理（7f8bf2e）**：上限检查已移到两个裁剪分支之前，超限直接跳过并记录告警。 |
| H6 | ~~已处理~~ `.github/workflows/build-plugin-optimized.yml:638-667` | 发布资产曾收集共享临时目录中的全部 zip，可能混入旧运行产物 | **已处理（c165274、61fa90c）**：按当前 tag 构造官方文件名模式，仅收集 `XTools-UE_<版本>-<tag>.zip`，并强制校验 5.3–5.8 六个版本均存在。 |

---

## 四、中危发现（初审 28 条，现行有效 25 条）

### 4.1 崩溃 / UB / 正确性（9 条）

| # | 位置 | 问题 | 建议 |
|---|------|------|------|
| M1 | ~~已处理~~ `Source/GeometryTool/Private/GeometryInstance.cpp:339-647`【核验后降级为低，"崩溃"论断证伪】 | 误用 `FRandomStream::RandRange(int32,int32)` 属实：float 实参隐式截断为整数，导致小数旋转/噪声/缩放量化。~~反向区间触发 checkf 崩溃~~ 经引擎源码核验不成立 | **已处理**：噪声、旋转、缩放统一改用 `FRandRange(float,float)`，旋转/缩放端点以 `Min/Max` 规范化；新增小数范围、反向范围确定性测试，不改公开 API、种子或点生成顺序。 |
| M2 | ~~已处理~~ `Source/FormationSystem/Private/FormationLibrary.cpp:727-735` | `CalculateTransitionCost` 两空阵型曾通过数量相等检查后索引 `FromPositions[0]`，造成越界 | **已处理**：数量相等后增加空阵型守卫并返回 `-1`，沿用无效比较契约；FormationLibrary 测试补两个空阵型回归断言。 |
| M3 | ~~已处理~~ `Source/BlueprintExtensions/Private/K2Nodes/K2Node_Map*Item.cpp`（6 个 handler） | Map 系列 FKCHandler 曾在 `FunctionToCall` 为空时直接生成调用语句，可能在 VM 后端空指针解引用 | **已处理（7e48b99）**：六个 handler 均判空并记录 Warning 后终止展开，避免生成残缺语句；当前仍使用字面函数名查找，但失败路径已安全收敛。 |
| M4 | ~~已处理~~ `Source/BlueprintExtensions/Private/K2Nodes/K2Node_Assign.cpp:204-218` | `IsActionFilteredOut` 初始值曾为 false，导致节点菜单过滤失效 | **已处理（7e48b99）**：默认值改为 true，仅拖拽引用输出引脚时放行。 |
| M5 | ~~已处理~~ `Source/BlueprintExtensions/Private/K2Nodes/*Loop*WithDelay.cpp` 与 `K2NodeHelpers.h` | Latent 图兼容性诊断曾使用 `MessageLog.Error`，可能触发引擎断言 | **已处理（7e48b99）**：延迟循环节点和共享连线辅助统一使用 Warning，并在失败时断开节点链接。 |
| M6 | ~~已处理~~ `Source/BlueprintExtensions/Private/K2Nodes/K2Node_ConditionalSequence.cpp`、`K2Node_MultiConditionalSelect.cpp` | 展开时曾散落调用 `TryCreateConnection` 且丢弃返回值，连线失败会静默生成残缺图 | **已处理（7e48b99）**：改用 `K2NodeHelpers::TryConnect`，失败记录 Warning、断链并终止展开；当前代码中未发现未处理的 `TryCreateConnection` 调用。 |
| M7 | `Source/RandomShuffles/Private/RandomShuffleArrayLibrary.cpp:539-546` | PRD 简单版 GetOrCreate→计算→Set 三步非原子，而类标注 BlueprintThreadSafe，同 StateID 并发丢失败计数更新 | **已处理**：新增私有 ApplyPRDAutoLocked，自动路径（PseudoRandomBool/FromStream）的读-算-写收拢进单个 PRDStateLock 临界区（复用递归锁语义，满表/钳制契约不变），性能统计保持锁外；Advanced 调用方状态语义不动；并发护栏测试注入必失败随机源确定性钉定 |
| M8 | `Source/Sort/Private/SortLibrary.cpp:1327-1331` | 反射排序比较器对浮点 NaN 双向 false 破坏严格弱序（GenericSort 模板路径已正确处理，行为不一致），手写 IntroSort 可能输出错误顺序 | **已处理**：ComparePropertyValues 浮点分支补 NaN 最大值语义——升序所有有限值与 ±Inf 先于 NaN、NaN 与 NaN 等价、有限值/±Inf/±0 保持原生比较；降序经既有"交换左右比较对象"机制复用同一分支使 NaN 居首，与 GenericSort 的 FSortPair 语义逐条对齐；不改函数签名/Blueprint API/快排堆排框架，不做预提取键优化；新增 6 个自动化测试（float/double 升序、降序 NaN 居首、±Inf±0NaN 混合、null 对象契约、SortFloatArray 交叉验证）以不变量断言钉定 |
| M9 | ~~已撤销（引擎范式核验证伪）~~ `Source/X_AssetEditor/Private/CollisionTools/X_CollisionManager.cpp:441` | 初审把“通知对整体位于修改后”误写成“Pre/Post 配对顺序颠倒”，并推断撤销/重编译状态可能不一致。实际撤销快照已由修改前的 `Modify()` 捕获；UE 5.3 `StaticMeshEditorSubsystem` 的碰撞编辑路径同样是修改后仅调用 `PostEditChange()`，不要求为 BodySetup 修改预先调用 UStaticMesh::PreEditChange | 不实施原方案；现有迟到的 Pre 仅属可选性能清理（会额外拆除渲染资源），与正确性无关 |

### 4.2 生命周期 / 线程安全（4 条）

| # | 位置 | 问题 | 建议 |
|---|------|------|------|
| M10 | `Source/XTools_EnhancedCodeFlow/Private/BP/Actions/*.cpp`（代表 ECFDelayBP.cpp:14，共 13 个工厂） | BP 异步工厂以裸指针捕获 Proxy 存入动作长生命周期回调，IsProxyValid 先解引用再断言，安全性依赖隐式保活契约，违反项目反模式清单 | **已处理**：`3cf598d` 将 13 个异步 BP 工厂统一改为 `TWeakObjectPtr` 捕获，回调先解析弱引用再校验 Proxy；`RunAsyncThen` 的工作线程转游戏线程回调同样保持弱引用。 |
| M11 | `Source/XTools_EnhancedCodeFlow/Private/BP/ECFActionBP.cpp:102` + `Private/ECFSubsystem.cpp:86` | 持有 latent 节点的 Actor 销毁后看门狗会进入 `ensureAlwaysMsgf("Can't obtain ThisWorld")`；原报告“每帧必命中”表述过重，查询失败后 ticker 会自清理，实际为每个 Proxy 最多一次 | **已处理**：`3cf598d` 在看门狗调用 `FFlow::IsActionRunning` 前校验 `Proxy_WorldContextObject`，失效即移除 ticker 并清理 Proxy；保留 `UECFSubsystem::Get` 的 ensure 作为直接错误调用的防御诊断。 |
| M12 | `Source/ObjectPool/Public/ObjectPoolInterface.h:118` | `CallLifecycleEventEnhanced(bAsync=true)` 在 AsyncTask lambda 中捕获裸 `AActor*`，延迟执行期 Actor 可能已 GC，悬空调用 IsValid 为 UB | **已处理**：37f5145 已改 TWeakObjectPtr\<AActor\> 捕获并在回调内重新解析判空（同步路径与 Blueprint API 不变）；本次补充确定性回归测试（任务图冲刷驱动，不依赖 Tick/GC 时序）：调用门（未实现接口返回 false）、同步/异步原生接口精确派发、异步=延迟执行语义、异步入队后销毁 Actor 回调经弱指针安全跳过。 |
| M13 | `Source/X_AssetEditor/Private/CollisionTools/X_AutoConvexDialog.cpp:127` | DialogWidget 持有 TSharedPtr\<SWindow\> 而 SWindow 内容又共享引用控件，引用环致每次弹窗泄漏一个窗口树（同文件 MaterialFunctionParamDialog 的 TWeakPtr 写法正确） | **已处理**：`DialogWindow` 已改为 `TWeakPtr<SWindow>`（头文件:43），确认/取消回调通过 `Pin()` 后请求关闭（cpp:144-147、155-158）；静态所有权图确认窗口强持有内容、控件仅弱持有窗口，不再形成环。 |
#### M29 已撤销：未初始化测试世界造成的误报（未计入发现统计）

M-12 测试排障曾观测到原生 C++ `IObjectPoolInterface` 实现者经 `Execute_*` 未收到生命周期事件。网络调研确认标准用法就是实现类覆盖 `_Implementation` 并由调用方走 `Execute_*`；本机 UE 5.3 源码与运行时探针进一步证明 `FindFunction`、native thunk 绑定、`GetInterfaceAddress` 指针调整、调用空间和 `UFunction::Invoke` 全部正常。

真实根因是测试 fixture 只调用 `UWorld::CreateWorld`，没有创建 `FWorldContext` 或调用 `InitializeActorsForPlay`。UE 5.3 的 `AActor::ProcessEvent` 在 `World->AreActorsInitialized()==false` 时会直接跳过事件；原生直调和直接 `UFunction::Invoke` 绕开该 Actor 层前置条件，因而造成看似矛盾的观测。测试世界按 Epic 自动化测试范式初始化后，严格计数用例同步、异步派发均通过（1 succeeded / 0 warnings / 0 failed）。结论：原生 C++ 实现者派发正常，不实施双路派发 helper，15 处 `Execute_*` 调用无需修改。

### 4.3 行为一致性 / 架构（11 条）

| # | 位置 | 问题 | 建议 |
|---|------|------|------|
| M14 | `Source/ObjectPool/Private/ObjectPoolLibrary.cpp:66` | 请求类 SpawnActor 失败后回退生成裸 `AActor::StaticClass()` 当作目标类型返回，抽象类场景持续产出空壳 Actor（类型污染） | **已处理**：37f5145 已移除基类空壳回退；按请求类生成失败时返回 `nullptr`，Ex 结果保持 `InvalidArgs`；新增抽象 Actor 普通、Ex、AcquireOrSpawn、批量入口回归测试，同时保留池满时目标类 `FallbackSpawned` 测试。 |
| M15 | `Source/ObjectPool/Private/ObjectPoolSubsystem.cpp:142` | 子系统开关反射读取 Editor-only 的 `/Script/X_AssetEditor.X_AssetEditorSettings`：编辑器默认关闭、打包后类不存在则一律启用，同一项目**编辑器/成品行为不对称** | **已处理**：`37f5145` 迁移至 Runtime `UObjectPoolSettings(config=Game)`，`61fa90c` 将默认值校准为关闭以保持旧项目升级兼容；现有测试覆盖 CDO 默认关闭及开关决定 Game 世界是否创建子系统。 |
| M16 | `Source/XTools_EnhancedCodeFlow/Private/ECFSubsystem.cpp:65-85` | 与 M15 同型的第二处实例（严重度评低）：运行时模块读 Editor-only 设置，打包后 bEnableEnhancedCodeFlowSubsystem 被忽略 | **已处理**：`3cf598d` 迁移至 Runtime `UECFSettings(config=Game)`，模块显式依赖 `DeveloperSettings`；`ShouldCreateSubsystem` 直接读取运行时 CDO，默认保持启用，编辑器与打包成品共用同一配置节。 |
| M17 | `Source/ObjectPool/Private/ObjectPoolSubsystem.cpp:736` | PerformMaintenance 全工程零调用者，MAINTENANCE_INTERVAL 等常量全为死配置，FObjectPoolManager 自动扩缩容整套机制不可达 | **已处理**：删除子系统内不可达维护入口与死配置；保留并测试导出的管理器 API，接线/移除公共维护类留待架构决策 |
| M18 | `Source/ObjectPool/Public/ObjectPoolSubsystem.h:22` 等 | FObjectPoolMonitor 仅剩前向声明从未实现；FActorStateResetter 是桩代码（ResetActorState 恒 true）；FActorPoolMemoryOptimizer 未接线且存在容量判断缺陷 | **已处理（破坏性删除）**：移除 Monitor 前向声明、StateResetter/MemoryOptimizer 头与实现，以及 `FActorResetConfig`/`FActorResetStats` 反射类型；保留 `FObjectPoolManager`、`FObjectPoolUtils` 和 `FObjectPoolStats` 作为替代 API，并新增 C++/蓝图迁移指南。 |
| M19 | `Source/XTools/XTools.Build.cs` | Runtime 主模块在编辑器目标下链接 UnrealEd/Kismet/BlueprintGraph/KismetCompiler，违背 AGENTS.md 自身红线（有双重守卫暂不致打包失败） | **已处理**：移除零使用的 Kismet/GraphEditor/EditorStyle/EditorWidgets/AppFramework/ToolWidgets 并修正失实注释，保留项均经头文件归属+导出符号取证（UnrealEd/BlueprintGraph/KismetCompiler/AssetRegistry/ComponentTimelineUncooked）；UE 5.3–5.8 `BuildPlugin -StrictIncludes` 的 Editor Development、Game Development、Game Shipping 全部成功。Runtime/Editor 拆分经复审否决——CleanupTool 为运行时可见 API+编辑器门控实现模式、无打包缺陷，拆分会引入打包语义变化。 |
| M20 | `Source/FieldSystemExtensions/Private/XFieldSystemActor.cpp:344-356` | ApplyFieldToFilteredGeometryCollections 每个 GC 曾做两次求值图深拷贝 | **已由 cf6d246 修复**：按值捕获命令并在入队 lambda 内初始化时间元数据 |
| M21 | ~~已处理~~ `Source/SplineMovement/Private/SplineMoveAlongAction.cpp:201-217` | AIMoveTo 模式曾接近每帧中止并重建寻路请求 | **已处理（7724978）**：缓存上次下发目标，仅当目标位移超过前瞻距离一半时重新 `MoveToLocation`；结束动作仍停止当前 AI 移动。当前缺少可控导航场景自动化，静态状态机与 UE 5.3 编译为现有证据边界。 |
| M22 | ~~已处理~~ `Source/FormationSystem/Private/FormationMovementComponent.cpp:49-82` | `StartMoveToLocation` 在起点已到达时曾直接返回而不广播完成事件 | **已处理（ec43a6d）**：提前返回路径统一广播 `OnMovementCompleted(this)`；`XTools.Formation.Movement.StartInsideRadiusCompletesImmediately` 已通过。 |
| M23 | ~~已处理~~ `Source/X_AssetEditor/Private/AssetNaming/X_AssetNamingManager.cpp:414-550` | 批量重命名取消/失败后曾缺少已完成与未处理台账，用户无法可靠续跑 | **已处理（f12e454）**：结果结构记录成功包路径、逐项失败原因和取消后的未处理资产，并显式标识部分完成；专用 AssetNaming 测试覆盖失败详情与格式化。 |
| M24 | ~~已处理~~ `Source/X_AssetEditor/Private/AssetNaming/X_AssetNamingManager.cpp:54-65, 657-659, 797-799` | `FolderNameCache` 曾在批量期间不更新，导致同批冲突判断使用陈旧占用状态 | **已处理（f12e454）**：每次成功重命名后移除旧名并加入新名，后续资产基于最新缓存解算冲突。 |

### 4.4 性能 / 构建（4 条）

| # | 位置 | 问题 | 建议 |
|---|------|------|------|
| M25 | ~~维持现状（显式调用契约）~~ `Source/XTools/Private/XToolsLibrary.cpp:473-520` | 纯 BP 节点在匀速模式下每次调用重建固定 100 段弧长表 | **无需新增修复**：纯节点定位为低频一次性求值，ToolTip 已明确禁止 Tick 逐帧调用并指向带 `FBezierRotationFrameState` 的导弹轨迹接口；状态接口缓存命中/失效已有确定性测试。隐式全局缓存缺少 World/生命周期/内存上界且目标移动时命中率低，不引入。 |
| M26 | ~~已撤销（实证核验证伪）~~ `Source/XTools_EnhancedCodeFlow/XTools_EnhancedCodeFlow.Build.cs:18-19` | 初审声称"CppStandard=Default 在 UE5.3–5.5 默认 C++17 下协程不可用"。经 UBT 源码核验**不成立**：UE 5.3 TargetRules.cs L137 明文记载 `CppStandard.Default has changed from Cpp17 to Cpp20`（V4 构建设置起），L2791 的迁移提示亦确认 Default==C++20；工具链 switch 遇 Default 直接抛异常，证明其必被解析为具体标准。显式设 `Default` 在全部支持版本均得 `/std:c++20` | 无需修复 |
| M27 | `.github/workflows/build-plugin-optimized.yml:88` | verify-toolchain 无矩阵却读 matrix.ue_version，分版本 MSVC 校验曾是死代码 | **已由 c165274 修复**：改为显式遍历 UE 5.3–5.8 校验最低 MSVC 版本 |
| M28 | `Source/XToolsCore/Private/XToolsCore.cpp:15-48` | FXToolsLogCategories 清单漂移：`LogEnhancedCodeFlow` 实际叫 `LogECF`（条目失效）；缺 LogAxisLocker/LogSplineMovement/LogQueueSpline/LogComponentTimelineUncooked；下游 ApplyPluginLogVerbosity 批量设置对 ECF 永不生效 | **已由 b694f8e 修复**：按实际模块声明同步清单 |

---

## 五、低危发现（初审 49 条，现行有效 48 条）

### 5.1 规范回迁类（约 16 条）
- 中文元数据不达标：全部 K2Node 分类用英文 `"XTools|Blueprint Extensions|Loops/Map"`、运行时库中英混杂（MapExtensionsLibrary 英文 vs TurretRotationLibrary 中文）；时间轴节点/库分类缺中文模块名、DisplayName 英文。
- 统一错误宏未覆盖仍属持续性规范债务；**VariableReflection 已处理**：检查 `ImportText_InContainer` 返回值，失败走统一警告，成功在 Editor 发出 `PostEditChangeProperty(ValueSet)`；**错误上报线程边界已处理**：日志保持原线程同步，屏幕提示与 MessageLog 在非游戏线程调用时转发游戏线程。
- **Build.cs 卫生已处理**：FormationSystem、ComponentTimelineRuntime 移除零使用 UI 依赖，BlueprintExtensions 将 RandomShuffles 降为 Private；四个模块不再重复注入引擎版本宏，改由显式 `Version.h` 提供。脚本中的本机路径是可覆写参数后的自动探测候选，CI 路径是 runner 环境契约，不再按硬编码缺陷处理。
- PythonScriptPlugin 经完整调用面复核为资产预设生成工具的显式依赖，并非零用途；`DEFINE_LOG_CATEGORY_STATIC` 只限制 C++ 链接可见性，不影响按日志类别名称过滤，二者均撤销为误报。

### 5.2 死代码 / 未接线类（约 10 条）
- **ECF 已处理**：两个兼容动作类在回调前先 `MarkAsFinished`，避免完成回调重入再次完成；四个导出 STAT 已补 `DEFINE_STAT`。零内部调用的导出动作与未使用 STAT ID 只属兼容/维护债务，不删除公共表面。
- FieldSystem 的 EFieldResponseDisableMethod 虽无源码调用，但为 `BlueprintType` 公开反射类型，仓内扫描无法排除用户资产引用；本版本保留，不按死代码删除。
- **ObjectPool 已完成旧维护层清理**：公共旧工具继续保留并由确定性测试钉定；StateResetter、MemoryOptimizer 及其反射结构体已删除，运行时状态恢复统一使用 FObjectPoolUtils，扩缩容和维护使用 FObjectPoolManager。
- 时间轴两份 PostPasteNode 是为绕开跨版本未导出基类符号而保留的平行实现；当前行为正确，合并属于高风险维护重构，不在缺陷修复中进行。

### 5.3 细节缺陷类（约 15 条）
- **Sort 已处理**：属性 QuickSort/HeapSort 对等价键增加原索引决胜，数值、布尔、枚举、NaN、null 与自然字符串路径均保留首现顺序；字符串降序同时固定 null 仍置末。去重 Actor、整数、字符串路径也已改为按输入遍历、TSet 仅负责判重，并覆盖大小写语义及输入输出同数组。
- PointSampling：~~稠密网格 TotalCells=int32 直乘可溢出~~（已证伪：节点首次引入时即以 int64 计算并在分配前拒绝超出 int32 容量，已补极值回归测试）；~~FCircleSamplingCacheKey 含种子不含流状态~~（已证伪：公开圆形节点只接收种子且每次从种子新建流，私有 helper 无其他调用者；已补非零扰动的缓存命中/关闭缓存重算一致性测试）；**缓存更新误淘汰已处理**（Poisson `Store` 仅在插入新键且容量已满时执行 LRU 淘汰，与 CircleCache 既有规则对齐；新增 50 项满容量更新并逐项读回测试）；**PRD 二分容差已处理**（仅精确表项命中，其他输入走相邻表项线性插值；新增 0.499/0.500/0.501 连续性与插值值回归测试）。超大但未溢出的网格仍可能耗尽资源，但仓内没有统一点数预算，需作为独立 API 设计项处理，不据此虚构固定上限。
- **K2Node 行为缺陷已处理**：延迟循环将 Break 纳入必需引脚，Array Get 的 Item 缺失改为 Warning 后安全中止；MultiConditionalSelect 在早退前通知 Super，并删除误写且未使用的临时类型块。ForEachMap 全量传播经复核为幂等的多余通知，其余 PropagatePinType/ResetPinToWildcard/Super::AllocateDefaultPins 项均为无错误输出的维护重构，避免在本轮改动节点重建语义。
- ObjectPool：**统计锁约定已处理**（GetActor/AcquireDeferred 的 TotalRequests 与延迟命中计数收回既有 PoolLock 写区间，并补 TotalAcquired/TotalReleased/HitRate 回归断言）；**BatchReturnActors 成功数已处理**（复用 ReturnActorToPoolEx 的真实结果，仅统计实际归还成功项，并补池对象+非池对象混合回归）；**静态访问已处理**（Get 与 ResolveWorld 在解引用 GEngine 前返回空）；**objectpool.clear 类解析已处理**（仅在当前已注册池类中解析，完整对象路径优先；原生和蓝图生成类短名均支持省略 `_C`，短名必须唯一，歧义时拒绝操作并列出候选；无参数路径直接调用子系统全清理）。
- **X_AssetEditor 已处理/校准**：四个批量碰撞入口增加延迟显示的非取消进度框，避免大选择集静默冻结；内部 helper 更名 `FinalizeStaticMeshChanges` 以匹配标脏+编辑通知语义。日志字符串当前左右引号完整，原断言为陈旧误报；资产加载仍同步，但已有可见进度且结果结构无未处理台账，故不引入不完整取消语义。

### 5.4 表现/文档类（约 8 条）
- **编队已处理/校准**：移除每帧无效果的 scale 写回并拒绝无根组件单位，外部缩放在过渡期间保持；补充阵型中心/旋转/位置的有限值校验及分配结果数量、目标索引护栏；UE 5.3 `FMath::Lerp<FRotator>` 专门化本身使用归一化差值走最短路径，原跨 ±180° 断言撤销；无 Outer 的临时 Manager 仅在同步调用栈使用，默认 TransientPackage 生命周期安全。
- **ECF 元数据已处理**：移除错误的“协程/异步流程”后缀，7 个节点迁入 `XTools|ECF|...` 分类。LoadObjectsAsync 的每条完成路径均已由 `IsValid/IsActionValid`（含 Owner）门控，原“漏加 HasValidOwner”断言撤销。
- **文档表现已处理/校准**：XToolsLibrary 注释已改为“全量清空并预留 50”；ComponentTimelineSettings.h 当前仅一份版权头，原断言陈旧；AGENTS.md/CLAUDE.md 已同步 26 模块及真实 InitializeComponentTimelines 架构。
- **基础设施已处理**：移除 Build.cs 重复引擎版本宏并显式包含 `Version.h`；FXToolsErrorReporter 将 UI/MessageLog 工作转发游戏线程，同时保留原线程日志顺序。

---

## 六、已核查确认无问题的重点项（正面清单）

1. **K2Node 安全红线**：自研代码零 MakeLinkTo（仅第三方 fork 上游原样存在）；ReconstructAndFindPin 杜绝重建后旧 Pin 复用；延迟循环节点 Break 经越界哨兵+PostBodyBranch 保证唯一完成路径。
2. **算法正确性**：RandomShuffles PRD 公式与 DOTA2 一致、满表退化与世界清理（commit 5ebaa61/f41ebbc）落实且有测试；泊松缓存键 GetTypeHash/operator== 严格配套、LRU 有界；竞争泊松加权采样数学正确。
3. **异步框架设计**：ECF PendingAdd/Active 双队列保证任意回调内增删安全；Owner 弱引用跟踪销毁即回收；FTSTicker 弱捕获配对正确；协程帧所有权转移杜绝双析构，H1 失败路径已在后续提交中改为延迟销毁。曾怀疑的 standalone bCanTick 问题经引擎源码核实**不构成缺陷**。
4. **工程卫生**：Content/Resources 无二进制残留；workflow 无密钥泄漏、GITHUB_TOKEN 最小权限；AssetRegistry 通知内改名延迟 Ticker 执行无死锁路径；Python 与 C++ 零耦合；四个第三方 fork 许可证头完整、汉化低侵入。
5. **兼容层有效性**：TAtomic 封装经引擎 5.3 源码验证可用；XTOOLS_GET_ELEMENT_SIZE 设计正确；近期修复 commit（be89330 场拷贝、3dbfbba 时间轴重建、fdcf1f6 重入隔离、deaed37 临时 Actor 清理）均验证为真实有效的修复。

---

## 七、修复路线图建议

- **P0（已处理）**：H1 协程 UAF、H5 Trim 上限移位、H6 CI 发布过滤、H2/H3 对象池回退与碰撞保存逻辑。
- **P1（已处理）**：M3–M6 K2Node 编译期崩溃/断言/丢线四件套、M1/M2 崩溃类、M15+M16 Runtime DeveloperSettings 迁移、M4 过滤器写反；M26 为误报无需修复。
- **P2（已处理）**：M10–M24 的真实行为缺陷均完成代码修复或契约决策；M25 维持显式性能契约；M18 已完成旧公共维护层的破坏性删除并提供迁移指南；M19 主模块拆分经复审否决。M29 已确认为未初始化测试世界造成的误报，不实施生产修复。
- **P3（持续还债）**：剩余项为全仓中文分类/错误宏覆盖率、重复 K2/Timeline helper 与下一大版本公共 API 清理等维护工作；本轮已完成 Build.cs 卫生、ECF 分类、文档架构同步和 M28 日志清单。

### 收尾清单（截至 `d63557d`）

- **正确性修复主线：完成**。现行有效的 H/M 项均已修复、撤销或明确为兼容/性能契约；FormationSystem 最近补充的有限值与异常分配护栏已通过 UE 5.3 编译和 19 项自动化测试。
- **构建与测试：完成当前范围验证**。全量自动化测试为 100 项成功、0 项失败；UE 5.4–5.8 严格插件矩阵已有 Editor/Game Development/Shipping 成功记录。报告中的测试警告包含预期诊断，不应统称为“全部历史噪音”。
- **开放维护项：不阻塞当前发布**。
  - P3 规范债务（中文元数据、错误宏覆盖率、重复 helper）延期到独立维护批次，避免与行为修复混提交。
  - M21 导航场景测试、M13 Slate 生命周期测试需要真实引擎 fixture，列为发布前增强验证，不用静态测试替代。
  - 真实 PIE/蓝图资产兼容验证列为删除 M18 公共类型前的发布门禁。
- **文档状态：已完成本轮同步**。旧任务清单仅作为 2026-03-20 历史记录，当前状态以本报告和 `UNRELEASED.md` 为准。

---

## 八、审查覆盖度说明

| 组 | 覆盖范围 | 结论 |
|----|----------|------|
| 主审 | XToolsCore 全部、跨模块依赖扫描、日志/原子 API 引擎源码验证、MakeLinkTo 全局扫描、规模与测试统计 | 1 中 2 低 |
| 组1 | X_AssetEditor 全部 + uplugin/CI/脚本/文档一致性 + 第三方 fork 抽查 | 1 高 5 中 7 低 |
| 组2 | Sort / RandomShuffles / PointSampling / GeometryTool + 贝塞尔匀速 | 1 高 4 中 6 低 |
| 组3 | ObjectPool + ObjectPoolEditor 全部 30 文件 | 2 高 5 中 7 低 |
| 组4 | FormationSystem / SplineMovement / QueueSpline / SplineMovementEditor / AxisLocker | 1 高 3 中 5 低 |
| 组5 | FieldSystemExtensions / ComponentTimeline×2 / XTools 主模块 | 0 高 2 中 8 低 |
| 组6 | BlueprintExtensions（24 文件）+ BlueprintExtensionsRuntime（19 库） | 0 高 5 中 9 低 |
| 组7 | XTools_EnhancedCodeFlow 全部 44 头/23 cpp + 上游 fork 对比 | 1 高 3 中 5 低 |

---

## 九、实证核验附录（网络调研 + 引擎源码交叉验证）

核验方法：外部技术断言走网络调研（cppreference / LLVM 官方 bug 库 / Epic 官方文档与论坛），引擎行为断言逐版本对照本机 UE 5.3–5.8 引擎源码。以下为关键断言的裁决与证据。

### 9.1 证伪并撤销的发现（5 条）

| 发现 | 证伪证据 |
|------|----------|
| H4 "SetDOFLock 模式未变即早退致锁定静默失效" | UE 5.3–5.8 六个版本的 `BodyInstance.cpp` 中 `SetDOFLock` 均为 `{ DOFMode = NewAxisMode; CreateDOFLock(); }`，无条件 Term/Create；插件代码注释与引擎行为**完全一致** |
| M9 "碰撞修改后调用 Pre/Post 导致撤销与重编译不一致" | 三个入口均在修改前调用 `StaticMesh->Modify()` 与 `BodySetup->Modify()` 捕获事务快照；UE 5.3 `StaticMeshEditorSubsystem` 的碰撞编辑范式同样在修改后仅调用 `PostEditChange()`。`UStaticMesh::PreEditChange` 是渲染资源修改的前置拆除钩子，不是 BodySetup 碰撞事务的必要条件 |
| M26 "Default=C++17 致协程不可用" | UE 5.3 `TargetRules.cs:137`："TargetRules.CppStandard = CppStandard.Default has changed from Cpp17 to Cpp20"；L2791 迁移提示同义；`VCToolChain.cs:961-977` 的 switch 对未解析值抛 BuildException |
| PointSampling "稠密矩形网格使用 int32 直乘可溢出" | `GenerateRectangleGrid` 自首次引入即先将行列分别提升为 `int64` 再相乘，并在 `PointCount >= MAX_int32` 时于 `TArray::Reserve` 前返回空数组；新增 `50000 x 50000` 极值测试钉定该路径。公开资料与 UE 5.3 `TArray::Reserve` 仅确认按请求容量分配，不提供业务安全上限，因此不擅自改变合法输入契约 |
| PointSampling "FCircleSamplingCacheKey 含种子不含流状态" | 公开 `GenerateCircle` API 只接收 `RandomSeed`，每次调用都以该种子新建 `FRandomStream`；私有 `FCircleSamplingHelper::GenerateCircle` 仅由该包装器调用，不存在调用者传入已推进流状态的路径。新增非零扰动测试证明首次生成、跨中心缓存命中及关闭缓存重算的局部几何逐项一致 |

### 9.2 确认成立的关键发现与证据

| 发现 | 裁决 | 证据 |
|------|------|------|
| H1 协程 awaiter 内 resume+destroy UAF | ✅ 成立 | C++ 标准 [dcl.fct.def.coroutine] 要求 destroy 仅作用于挂起态协程；resume 后续体若运行至完成，协程帧已随 co_return 释放，再 destroy 即 UAF。[cppreference coroutine_handle::destroy](https://en.cppreference.com/w/cpp/coroutine/coroutine_handle/destroy) 前置条件明确；[LLVM Bug 47475](https://lists.llvm.org/pipermail/llvm-bugs/2020-September/086482.html) 记录 await_suspend 内 destroy 触发 asan use-after-free 实例 |
| M8 NaN 破坏严格弱序 → 排序 UB | ✅ 成立 | [std::sort 文档](https://en.cppreference.com/w/cpp/algorithm/sort) 要求比较器满足严格弱序否则 UB；[GCC Bug 108556](https://gcc.gnu.org/pipermail/gcc-bugs/2023-January/810449.html) 为非法比较器导致元素被改写的真实事故记录 |
| M3 Map 系列 FindUField 无空检查 → 编译期崩溃 | ✅ 成立（机理闭环） | 引擎自身守卫范式：`CallFunctionHandler.cpp:83` 用 `if (UFunction* Function = FindFunction(...))` 过滤空函数；而 VM 后端 `KismetCompilerVMBackend.cpp:1242` 对 `FunctionToCall` **裸解引用无空检查**。插件 handler 绕过守卫直接赋空指针 → 编译期必崩 |
| M21 AI 模式每帧重建寻路 | ✅ 成立且比初审更严重 | UE 5.3 `AIController.cpp` 的 `MoveToLocation/MoveToActor` 开头即注释 "abort active movement to keep only one request running" 并无条件 AbortMove——**连"相同目标去重"都不存在**，每帧调用=每帧中止+重建寻路请求 |
| M7 BlueprintThreadSafe 下 PRD 三步非原子竞态 | ✅ 成立 | [Epic 元数据说明符文档](https://dev.epicgames.com/documentation/unreal-engine/metadata-specifiers-in-unreal-engine)确认该 specifier 允许蓝图 VM 并行调用；社区亦有[语义讨论帖](https://unreal2.epic-prod-us2.discourse.cloud/t/meta-blueprintthreadsafe-not-working-as-specified/409477/10)。静态 TMap GetOrCreate→计算→Set 非原子前提成立 |
| M5 MessageLog.Error 断言风险 | ⚠️ 成立（证据为项目历史记录） | 引擎侧：`EdGraphNode.h:563` 确为 `FindPinChecked` 内裸 `check(Result)`；仓库侧：K2NodeSafetyHelpers/K2Node_Assign 等 **10+ 处**修复注释一致记录"Error 触发 EdGraphNode.h:563 断言崩溃"。Error→FindPinChecked 的完整因果链未能独立复现，故标注为"依据项目已验证的历史崩溃模式"维持中危 |
| M15/M16 Editor-only 设置打包后失效 | ✅ 成立，现已处理 | X_AssetEditor 为 Editor 类型模块不参与打包是引擎模块体系基本行为；`37f5145`/`3cf598d` 已分别迁移到 `UObjectPoolSettings`/`UECFSettings` 的 `config=Game` Runtime CDO，编辑器与打包成品共用同一配置节。 |
| M11 ensureAlwaysMsgf 反复弹报告 | ✅ 原缺陷成立，现已处理 | `ensureAlways` 每次失败都会报告；原实现对多个失效 Proxy 可重复触发，但单个看门狗查询失败后会自清理，并非永久逐帧。`3cf598d` 已在查询前校验 WorldContext 并提前清理。 |
| M1 RandRange 整型截断量化 | ⚠️ 部分成立，现已处理 | 截断量化成立：引擎源码 `RandRange(int32,int32)` + float→int32 隐式转换使 <1°旋转、±1cm 内噪声、0.8–1.2 缩放全部塌缩为整数值；~~反向区间 checkf 崩溃~~证伪：`RandHelper(A≤0)` 返回 0，静默返回下界。当前实现已改 `FRandRange` 并规范化端点，新增连续小数范围回归测试。 |

### 9.3 无需外部佐证的发现（读码即证据）

H2/H3/H5/H6（对象池回退缺失、碰撞覆写、Trim 上限绕过、CI zip 收集）、M2/M4/M6（空阵型越界、过滤器恒 false、丢弃连接返回值）、M13/M14/M17/M18/M22–M25/M27/M28 等均为插件自身代码逻辑或配置问题，其真实性由直接读码确立，本次核验复核了原始行号与上下文，均维持原判。M9 经 UE 5.3 引擎源码与 StaticMeshEditorSubsystem 对照后撤销：BodySetup 碰撞修改不要求预先调用 UStaticMesh::PreEditChange，修改前的 Modify 已负责事务快照。

### 9.4 核验后的修正口径

- 高危：6 → **5**（H4 撤销）
- 中危：28 → **25**（M9/M26 撤销、M1 降级至低危）
- 低危：49 → **48**（M1 降级新增 1 条，PointSampling 两项误报撤销 2 条）
- 总计：**78 条有效发现**

### 9.5 基线后的修复状态

本报告基线为 `695be82`。后续提交已处理以下审查项：

- H1、H2、H3、H5、H6：完成协程失败清理、对象池回退与碰撞配置、泊松裁剪上限、CI 发布资产筛选。
- M3-M6、M10-M16、M20-M24、M27-M28：完成 K2Node 安全、异步代理弱引用、Runtime DeveloperSettings、场命令拷贝、SplineMovement AI 防抖、资产重命名台账、CI 工具链校验及日志类别清单同步等修复。
- FormationSystem：补齐起点即到达完成事件、停止事件清理临时 Actor、制动带速度归零恢复输入，并修复 BeginPlay 前动态移动组件丢失指令。
- M18 已完成：删除 FActorStateResetter、FActorPoolMemoryOptimizer 及 FActorResetConfig/FActorResetStats，保留 FObjectPoolManager/FObjectPoolUtils/FObjectPoolStats，并新增迁移指南。仍开放的仅是卡墙超时/失败事件及全仓分类、错误宏、重复 helper 等低优先级设计/规范债务。

> 核验结论：原审查报告整体可信度高——83 条初审发现经实证有 5 条误报、1 条部分修正，修正率约 7.2%；误报均源于未完整核对实际实现、调用面或对引擎行为的推断未经对应源码与官方实现范式验证，已在附录中记录教训：**代码事实以当前实现、完整调用面和提交历史为准；引擎行为类断言必须以本机对应版本引擎源码为准，网络资料（含官方论坛）只能作为线索而非结论**。
