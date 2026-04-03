# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

XTools 是一个为 Unreal Engine 5.3-5.7 设计的模块化插件系统（v1.9.6），提供蓝图节点和 C++ 功能库。插件采用多模块架构（21个模块），每个模块独立编译和打包。

**核心设计原则**：
- 单一职责原则：每个模块专注于一个特定功能领域
- 蓝图友好：所有功能通过 `UFUNCTION(BlueprintCallable/Pure)` 暴露
- 中文优先：所有元数据、注释、文档使用中文
- 跨版本兼容：支持 UE 5.3-5.7，零警告编译
- 性能优先：包含 Unreal Insights 跟踪和性能优化

## 构建和编译

### 完整编译
```bash
# 在 VS Code 中运行任务（推荐）
# Ctrl+Shift+P -> Tasks: Run Task -> "Build Editor (Full & Formal)"

# 或使用命令行
"<UE_ROOT>/Engine/Build/BatchFiles/Build.bat" <ProjectName>Editor Win64 Development -Project="<ProjectPath>/<ProjectName>.uproject"
```

### Live Coding（快速迭代）
```bash
# VS Code 任务: "Live Coding (Official)"
# 需要 UE 编辑器处于运行状态，会自动热重载
```

### 插件打包（多版本）
```powershell
# 自动检测引擎并实时跟踪（最常用）
.\Scripts\BuildPlugin-MultiUE.ps1 -Follow

# 指定引擎版本
.\Scripts\BuildPlugin-MultiUE.ps1 -EngineRoots "UE_5.3","UE_5.4","UE_5.5","UE_5.6","UE_5.7" -Follow

# 其他参数：-OutputBase, -CleanOutput, -StrictIncludes, -NoHostProject
```

### 清理项目
```powershell
.\Scripts\Clean-UEPlugin.ps1   # 清理 *.pdb, Intermediate/
```

## 模块架构

插件采用运行时/编辑器分离的模块架构：

```
核心层（Runtime, PreDefault）
└─ XToolsCore                          # 跨版本兼容性层、统一错误处理、版本宏
   ├─ XToolsVersionCompat.h            # UE 5.3-5.7 版本兼容性宏
   ├─ XToolsErrorReporter.h            # 统一日志/错误处理
   └─ XToolsDefines.h                  # 插件版本和通用宏

运行时模块层（均依赖 XToolsCore）
├─ Sort                                # 排序算法库（整数、浮点、字符串、向量、Actor、通用结构体）
├─ ObjectPool                          # Actor 对象池（智能管理、自动扩池、状态重置）
├─ BlueprintExtensionsRuntime          # 蓝图运行时函数库
├─ XTools_EnhancedCodeFlow             # 异步操作和流程控制（延迟、时间轴、循环、协程）
├─ FieldSystemExtensions               # Chaos Field System 增强
├─ XTools_ComponentTimelineRuntime     # 组件时间轴系统
├─ PointSampling                       # 几何采样（泊松圆盘、纹理采样、贝塞尔曲线）
├─ GeometryTool                        # 几何工具（形状组件点阵生成、表面采样）
├─ RandomShuffles                      # PRD 伪随机分布算法和数组洗牌
├─ FormationSystem                     # 编队系统和移动控制
└─ XTools                              # 主模块（插件入口、欢迎界面）

编辑器模块层（UncookedOnly，每个都有对应 Runtime 模块）
├─ BlueprintExtensions                 # → BlueprintExtensionsRuntime: 14+ 自定义 K2Node
├─ ObjectPoolEditor                    # → ObjectPool: K2Node_SpawnActorFromPool
├─ SortEditor                          # → Sort: K2Node_SmartSort
└─ XTools_ComponentTimelineUncooked    # → ComponentTimelineRuntime: K2Node_ComponentTimeline

Editor Only 模块（仅 Win64，除 SwitchLanguage）
├─ X_AssetEditor                       # 资产批量处理（碰撞、材质函数、命名规范）
├─ XTools_AutoSizeComments             # 注释框自动调整（已汉化）
├─ XTools_BlueprintAssist              # 蓝图节点自动格式化（已汉化）
├─ XTools_ElectronicNodes              # 蓝图连线美化（已汉化）
├─ XTools_BlueprintScreenshotTool      # 蓝图截图工具（已汉化）
└─ XTools_SwitchLanguage               # 编辑器语言切换（Win64/Mac/Linux）
```

## 开发规范

### C++ 编码约定
```cpp
// 类命名
UCLASS() class U<ModuleName>Library : public UBlueprintFunctionLibrary  // 蓝图函数库
USTRUCT() struct F<ModuleName>Config                                    // 配置结构体
UENUM() enum class E<ModuleName>State                                   // 枚举类型

// 蓝图暴露（中文元数据是必须的）
UFUNCTION(BlueprintCallable, Category = "XTools|<模块名>|<子类别>",
    meta = (
        DisplayName = "函数显示名称（中文）",
        Keywords = "关键词,搜索",
        ToolTip = "详细说明（中文）。\n参数:\nParam1 - 参数1说明\n返回值:\nResult - 结果说明"
    ))
static void FunctionName(UPARAM(DisplayName="参数名称") const FVector& Param);
```

### 蓝图节点分类
所有节点使用统一前缀 `XTools|<模块名>|<子类别>`，如 `XTools|排序|Actor`、`XTools|对象池|核心`。

### K2Node 开发要点
- 继承 `UK2Node` 或 `UK2Node_CallFunction`
- 必须实现：`GetMenuActions()`、`GetNodeTitle()`、`AllocateDefaultPins()`、`ExpandNode()`
- 通配符引脚：`PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard`
- 类型传播：在 `NotifyPinConnectionListChanged` 中同步
- `.Build.cs` 需要依赖：`"BlueprintGraph", "KismetCompiler", "UnrealEd"`
- 参考实现：`Source/BlueprintExtensions/Public/K2Nodes/`

### 对象池核心架构
```cpp
UObjectPoolSubsystem       // 子系统：全局访问点、生命周期管理
FActorPool                 // 单个 Actor 类的池管理器
FObjectPoolConfigManager   // 配置管理
FObjectPoolMonitor         // 性能监控、统计、诊断
FObjectPoolManager         // 自适应扩池、内存优化
FActorStateResetter        // 状态重置（Transform、物理、组件、网络）
UK2Node_SpawnActorFromPool // 自定义蓝图节点（支持 ExposeOnSpawn）
```
永不失败机制：池空时回退到正常 SpawnActor。

## XToolsCore 跨版本兼容性

所有 Runtime 模块的基础依赖，在 `.Build.cs` 中添加：
```cs
PublicDependencyModuleNames.Add("XToolsCore");
```

**版本宏**（在 XToolsCore.Build.cs 中自动定义）：
```cpp
#define XTOOLS_ENGINE_5_4_OR_LATER  // UE 5.4+
#define XTOOLS_ENGINE_5_5_OR_LATER  // UE 5.5+
#define XTOOLS_ENGINE_5_6_OR_LATER  // UE 5.6+
```

**关键 API 差异**：
- UE 5.4+：强制 IWYU（显式包含所有头文件）
- UE 5.5+：`FProperty::ElementSize` → `GetElementSize()/SetElementSize()`
- UE 5.5+：`BufferCommand` → `BufferFieldCommand_Internal`
- UE 5.4+：`FCompression::GetMaximumCompressedSize` 参数变更

**统一错误处理**（推荐用法）：
```cpp
FXToolsErrorReporter::Warning(
    LogBlueprintExtensionsRuntime,
    TEXT("参数验证失败"),
    NAME_None,
    true,   // 显示屏幕提示
    5.0f    // 显示时长
);
```

## 关键模块技术细节

### EnhancedCodeFlow 异步系统
- 基于 UE Ticker，支持协程（C++20 co_await）
- `FECFHandle` 管理动作生命周期，`FFlow` 是静态接口
- 编译时宏 `ECF_INSIGHTS_ENABLED` 控制 Insight 跟踪

### ComponentTimeline
- 运行时：`UComponentTimelineComponent`；编辑器：`UK2Node_ComponentTimelineRuntime`
- 必须在 `BeginPlay` 中调用初始化函数

### Sort 通用排序
`SortGenericByProperty` 通过 UE 反射系统（`FProperty`）按属性名称排序任意结构体数组。

### RandomShuffles PRD
基于 DOTA2 的伪随机分布算法，使用 `thread_local` 隔离状态，`WorldSubsystem` 持久化。

## 常见陷阱

### K2Node
1. 必须在 `NotifyPinConnectionListChanged` 中处理通配符类型同步
2. `AllocateDefaultPins` 中不要调用 `Super::AllocateDefaultPins()` 避免重复创建
3. 编辑器代码必须 `#if WITH_EDITOR ... #endif` 包裹
4. Break 后确保执行流继续到 Completed 输出引脚

### 模块依赖
1. Runtime 模块不能依赖 Editor 模块
2. UncookedOnly 模块必须有对应 Runtime 模块
3. 所有 Runtime 模块应依赖 XToolsCore
4. UE 5.4+ 强制 IWYU，不能依赖隐式包含

### 性能
1. 泊松采样缓存键必须正确实现 `GetTypeHash` 和 `operator==`
2. Lambda 避免捕获原始指针，用 `TWeakPtr`/`TSharedPtr`
3. 优先用间隔检查或 Ticker 替代每帧 Tick
4. 贝塞尔曲线匀速模式计算量大，避免每帧调用

## Git 工作流

- 主分支：`main`
- 提交格式：`<type>(<scope>): <中文描述>`
- type: `feat`, `fix`, `refactor`, `perf`, `docs`, `chore`, `style`
- scope: 模块名（如 ObjectPool, BlueprintExtensions）

## 版本发布流程

1. 开发阶段在 `Docs/版本变更/UNRELEASED.md` 记录变更
2. 发布时用 `changelog-generator` 技能整理到 `CHANGELOG.md`
3. 更新 `XTools.uplugin` 版本号和 `README.md` 版本徽章
4. 运行 `.\Scripts\BuildPlugin-MultiUE.ps1 -Follow` 测试打包
5. 推送后用 GitHub Actions `manual-release.yml` 创建发布

### UNRELEASED.md 格式
```markdown
### 模块名
- 类型 描述（20字内，"动词+对象+效果"格式）
```
类型：新增/优化/修复/移除/调整

## 调试

```cpp
UE_LOG(LogXTools, Warning, TEXT("调试信息: %s"), *SomeString);
FXToolsErrorReporter::Warning(LogCategory, TEXT("消息"), NAME_None, true, 5.0f);  // 推荐
TRACE_CPUPROFILER_EVENT_SCOPE(FunctionName);  // Unreal Insights 性能分析
// 对象池控制台命令：objectpool.stats
```

VS Code 调试：启动 UE 编辑器 → Run → Attach to Process → 选择 UE 进程。

## GitHub Actions

- `build-plugin-optimized.yml`: 推送/PR 时自动多版本编译测试
- `manual-release.yml`: 手动触发，从 CHANGELOG.md 提取说明创建 Release
- `update-release-assets.yml`: 更新已有 Release 的资产文件
