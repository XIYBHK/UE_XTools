# XTOOLS PLUGIN KNOWLEDGE BASE

## OVERVIEW

UE 5.3-5.8 模块化蓝图工具插件 (v1.9.9)，26个模块，C++ Runtime + K2Node Editor 分层架构。中文元数据优先。

## STRUCTURE

```
XTools/
├── Source/                    # 26个模块源码
│   ├── XToolsCore/            # [基础层] 版本兼容 + 错误处理 + 通用宏
│   ├── AxisLocker/            # [运行时] 物理轴向锁定 + 状态恢复
│   ├── ObjectPool/            # [运行时] Actor对象池子系统
│   ├── XTools_EnhancedCodeFlow/ # [运行时] 异步流程控制 + 协程
│   ├── BlueprintExtensionsRuntime/ # [运行时] 蓝图函数库 (14+库)
│   ├── BlueprintExtensions/   # [编辑器] K2Node (14+节点)
│   ├── Sort/                  # [运行时] 排序算法
│   ├── SortEditor/            # [编辑器] K2Node_SmartSort
│   ├── PointSampling/         # [运行时] 泊松/网格/样条采样
│   ├── GeometryTool/          # [运行时] 几何工具
│   ├── FormationSystem/       # [运行时] 编队系统
│   ├── RandomShuffles/        # [运行时] PRD伪随机
│   ├── FieldSystemExtensions/ # [运行时] Chaos场系统
│   ├── XTools_ComponentTimelineRuntime/ # [运行时] 组件时间轴
│   ├── SplineMovement/        # [运行时] 样条移动（含AI寻路模式）
│   ├── QueueSpline/           # [运行时] 队列样条（重入隔离派发）
│   ├── ObjectPoolEditor/      # [编辑器] K2Node_SpawnActorFromPool
│   ├── XTools_ComponentTimelineUncooked/ # [编辑器] K2Node_ComponentTimeline
│   ├── SplineMovementEditor/  # [编辑器] 样条移动编辑器支持
│   ├── X_AssetEditor/         # [Editor] 资产批处理 (Win64)
│   ├── XTools_AutoSizeComments/    # [Editor] 注释框自适应 (第三方汉化)
│   ├── XTools_BlueprintAssist/     # [Editor] 蓝图格式化 (第三方汉化)
│   ├── XTools_ElectronicNodes/     # [Editor] 连线美化 (第三方汉化)
│   ├── XTools_BlueprintScreenshotTool/ # [Editor] 截图工具 (第三方汉化)
│   ├── XTools_SwitchLanguage/ # [Editor] 语言切换
│   └── XTools/                # [主模块] 插件入口 + 运行时工具库
├── Scripts/                   # 构建/清理脚本
├── Docs/版本变更/             # CHANGELOG + UNRELEASED
├── .github/workflows/         # CI/CD (多版本构建 + 发布)
└── .vscode/                   # 编译/调试任务
```

## MODULE DEPENDENCY GRAPH

```
XToolsCore (PreDefault)
 ├─ Sort, RandomShuffles, GeometryTool           # 无额外UE依赖
 ├─ ObjectPool (+DeveloperSettings)
 ├─ BlueprintExtensionsRuntime (+GameplayTags)
 ├─ XTools_EnhancedCodeFlow (+Projects)
 ├─ PointSampling (+RenderCore, RHI)
 ├─ FieldSystemExtensions (+Chaos, FieldSystemEngine)
 ├─ FormationSystem (+AIModule)
 ├─ XTools_ComponentTimelineRuntime
 ├─ SplineMovement (+AIModule, Projects)
 ├─ QueueSpline
 │
 ├─ [UncookedOnly] BlueprintExtensions → BlueprintExtensionsRuntime (+RandomShuffles私有依赖)
 ├─ [UncookedOnly] ObjectPoolEditor → ObjectPool
 ├─ [UncookedOnly] SortEditor → Sort
 ├─ [UncookedOnly] XTools_ComponentTimelineUncooked → ComponentTimelineRuntime
 ├─ [UncookedOnly] SplineMovementEditor → SplineMovement
 │
 └─ [Main] XTools → ComponentTimelineRuntime, RandomShuffles, GeometryCore 等
```

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| 新建 K2Node | `Source/BlueprintExtensions/` | 参考 K2Node_ForEachLoopWithDelay，必读同目录 `AGENTS.md` |
| 新建运行时函数库 | `Source/BlueprintExtensionsRuntime/Public/Libraries/` | 继承 UBlueprintFunctionLibrary |
| 新建运行时模块 | 复制 Sort 模块结构 | .Build.cs 加 XToolsCore 依赖 |
| 版本兼容适配 | `Source/XToolsCore/Public/XToolsVersionCompat.h` | 用 XTOOLS_ENGINE_5_4_OR_LATER 等分版本宏 |
| 错误处理 | `Source/XToolsCore/Public/XToolsErrorReporter.h` | XTOOLS_LOG_* 宏或 FXToolsErrorReporter:: |
| 防御性编程 | `Source/XToolsCore/Public/XToolsDefines.h` | XTOOLS_SAFE_EXECUTE, XTOOLS_CHECK_VALID |
| 对象池集成 | `Source/ObjectPool/Public/` | UObjectPoolSubsystem 全局入口 |
| 异步操作 | `Source/XTools_EnhancedCodeFlow/Public/` | FFlow:: 静态API + co_await |
| 采样算法 | `Source/PointSampling/Public/` | 泊松/纹理/阵型 |
| CI/CD | `.github/workflows/` | build-plugin-optimized.yml |
| 打包脚本 | `Scripts/BuildPlugin-MultiUE.ps1` | `-Follow` 实时日志 |
| 版本记录 | `Docs/版本变更/UNRELEASED.md` | 开发中变更写这里 |
| 版本发布 | `CLAUDE.md` 版本发布流程 | 标签触发构建并创建 Release |
| 文档索引 | `Docs/README.md` | 长期文档入口与维护规则 |
| 代码审查 | `Docs/开发文档/UE代码审查与实证核验指南.md` | 证据链、严重度与测试护栏 |

## CONVENTIONS

### 命名
- 类: `U<Module>Library` (函数库), `F<Module>Config` (结构体), `E<Module>State` (枚举)
- 蓝图分类: `XTools|<中文模块名>|<子类别>` (如 `XTools|排序|Actor`)
- 提交: `<type>(<scope>): <中文描述>` (scope=模块名)
- PowerShell 多行提交信息: 不要用 `\n` 作为换行; 使用 here-string 管道到 `git commit -F -`, 确保 GitHub 正常显示正文换行

### 元数据 (强制中文)
```cpp
UFUNCTION(BlueprintCallable, Category = "XTools|模块名|子类",
    meta = (DisplayName = "中文名", Keywords = "关键词", ToolTip = "中文说明"))
static void Func(UPARAM(DisplayName="参数名") const FType& Param);
```

### 模块结构
- Runtime 模块不依赖 Editor 模块
- UncookedOnly 模块必有对应 Runtime 模块
- 所有 Runtime 模块必须依赖 XToolsCore
- UE 5.4+ 强制 IWYU (显式包含所有头文件)

### 错误处理 (统一)
```cpp
// 推荐: 宏 (自动注入文件/行号)
XTOOLS_LOG_WARNING(LogMyModule, TEXT("消息"));
XTOOLS_LOG_ERROR_EX(LogMyModule, TEXT("消息"), NAME_None, true, 5.0f);

// 推荐: 防御宏
XTOOLS_ENSURE_OR_ERROR_RETURN(Ptr != nullptr, LogModule, "Ptr is null");
XTOOLS_CHECK_VALID(Actor, false);
```

## ANTI-PATTERNS

| 禁止 | 原因 | 替代 |
|------|------|------|
| K2Node 展开时直接使用 `MakeLinkTo()` | 跳过 Schema 的方向、类型和连接响应校验 | 优先使用 `K2NodeHelpers::TryConnect()`；其他 Editor 模块使用等价的 `Schema->TryCreateConnection()` 并检查返回值 |
| 假定 `Super::ExpandNode()` 会自动断链 | `UK2Node` 基类不负责断链；实际行为取决于直接父类 | 按父类契约决定是否调用，并在完成引脚迁移后显式 `BreakAllNodeLinks()` / `EndExpandNode()` |
| 重建后复用旧Pin指针 | 悬空指针→崩溃 | `K2NodeHelpers::ReconstructAndFindPin()` |
| `UE_LOG` 用于用户错误 | 不统一 | `FXToolsErrorReporter` / `XTOOLS_LOG_*` |
| Runtime 模块依赖 Editor | 打包失败 | 拆分 Runtime/Editor 模块 |
| 隐式头文件包含 | UE 5.4+ IWYU 编译失败 | 显式 #include |
| `as any` / `@ts-ignore` 等价物 | 类型安全 | 正确类型转换 |
| Lambda 捕获原始指针 | 悬空引用 | `TWeakPtr` / `TSharedPtr` |
| 泊松缓存键缺少 `GetTypeHash` | 缓存失效 | 正确实现 hash + operator== |
| 每帧调用贝塞尔匀速模式 | 性能问题 | 缓存结果或降频调用 |

## COMMANDS

```powershell
# 完整编译优先使用宿主项目 .vscode/tasks.json；命令行格式见 Docs/打包相关/命令行编译指令.md

# 多版本打包
.\Scripts\BuildPlugin-MultiUE.ps1 -Follow

# 清理
.\Scripts\Clean-UEPlugin.ps1

# 对象池诊断
# 控制台: objectpool.stats
```

## LOADING ORDER

PreDefault → XToolsCore | Default → 14 个 Runtime + 6 个 Editor | PostDefault → 5 个 UncookedOnly

## THIRD-PARTY MODULES

AutoSizeComments, BlueprintAssist, ElectronicNodes, BlueprintScreenshotTool 是第三方fork，已汉化。修改时保留原始许可证头，不改动核心架构。

## VERSION COMPAT QUICK REF

| 版本 | 关键变化 | 宏 |
|------|----------|-----|
| 5.4+ | 强制 IWYU | `XTOOLS_ENGINE_5_4_OR_LATER` |
| 5.5+ | `FProperty::ElementSize` 废弃 | `XTOOLS_GET_ELEMENT_SIZE(Prop)` |
| 5.5+ | `BufferCommand` 更名 | 条件编译 |
| 5.6+ | 更严格 deprecation 警告 | `XTOOLS_ENGINE_5_6_OR_LATER` |
