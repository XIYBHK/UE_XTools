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

**项目状态**：
- 当前版本：v1.9.6 (开发中)
- 文档体系：完整的版本变更（CHANGELOG.md）、开发文档、设计模式文档
- CI/CD：GitHub Actions 自动化构建和测试

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
# 为 UE 5.3-5.7 打包插件
.\Scripts\BuildPlugin-MultiUE.ps1 -EngineRoots "UE_5.3","UE_5.4","UE_5.5","UE_5.6","UE_5.7" -Follow

# 支持的参数：
# -EngineRoots     指定引擎路径列表（留空则自动检测已安装引擎）
# -OutputBase      自定义输出目录（默认：Plugin_Packages/）
# -Follow          实时跟踪打包进度（每3秒显示最新40行日志）
# -CleanOutput     打包前清理输出目录
# -StrictIncludes  启用严格包含检查（默认启用）
# -NoHostProject   不需要宿主项目（默认启用）

# 示例：自动检测引擎并实时跟踪
.\Scripts\BuildPlugin-MultiUE.ps1 -Follow
```

### 清理项目
```powershell
# 清理插件临时文件
.\Scripts\Clean-UEPlugin.ps1

# 清理范围：*.pdb, Intermediate/
```

## 模块架构

插件采用运行时/编辑器分离的模块架构：

### 基础模块
- **XToolsCore** (Runtime, PreDefault): 核心工具和跨版本兼容性层
  - 提供 `XToolsVersionCompat.h`: UE 5.3-5.7 版本兼容性宏
  - 提供 `XToolsErrorReporter.h`: 统一错误/日志处理（支持所有日志分类，包括 FNoLoggingCategory）
  - 提供 `XToolsDefines.h`: 插件版本和通用宏定义
  - **所有 Runtime 模块都应依赖此模块**
  - **Editor 模块可选依赖此模块**

### 运行时功能模块（11个）
- **XTools**: 主模块（Runtime），包含插件入口和欢迎界面
- **Sort**: 排序算法库（整数、浮点、字符串、向量、Actor、通用结构体）
- **RandomShuffles**: PRD（伪随机分布）算法和数组洗牌（支持 Android/iOS）
- **XTools_EnhancedCodeFlow**: 异步操作和流程控制（延迟、时间轴、循环、协程，支持 Android/iOS）
- **PointSampling**: 几何采样工具（泊松圆盘采样、纹理采样、贝塞尔曲线）
- **FormationSystem**: 编队系统和移动控制
- **XTools_ComponentTimelineRuntime**: 组件时间轴系统（运行时核心）
- **BlueprintExtensionsRuntime**: 蓝图运行时函数库（支持 Android/iOS）
- **ObjectPool**: Actor 对象池（智能管理、自动扩池、状态重置，支持 Android/iOS）
- **FieldSystemExtensions**: Chaos Field System 增强（高性能筛选，支持 Android/iOS）
- **GeometryTool**: 几何工具（基于形状组件的点阵生成、表面采样）

### 编辑器专用模块（UncookedOnly，4个）
- **BlueprintExtensions** → BlueprintExtensionsRuntime: 自定义 K2Node（14+ 节点）
- **ObjectPoolEditor** → ObjectPool: K2Node_SpawnActorFromPool（对象池蓝图节点）
- **XTools_ComponentTimelineUncooked** → ComponentTimelineRuntime: K2Node_ComponentTimeline（时间轴蓝图节点）
- **SortEditor** → Sort: K2Node_SmartSort（智能排序节点）

### Editor Only 模块（6个）
- **X_AssetEditor**: 资产批量处理工具（碰撞、材质函数、命名规范、纹理打包后缀，仅 Win64）
- **XTools_AutoSizeComments**: 注释框自动调整（已汉化，仅 Win64）
- **XTools_BlueprintAssist**: 蓝图节点自动格式化（已汉化，100+ 设置项，仅 Win64）
- **XTools_ElectronicNodes**: 蓝图连线美化（已汉化，仅 Win64）
- **XTools_BlueprintScreenshotTool**: 蓝图截图工具（已汉化，CPU 占用降低 60%，仅 Win64）
- **XTools_SwitchLanguage**: 编辑器语言切换（支持 Win64/Mac/Linux）

### 模块依赖关系
```
核心层（Runtime, PreDefault）
└─ XToolsCore

运行时模块层（依赖 XToolsCore）
├─ Sort
├─ ObjectPool
├─ BlueprintExtensionsRuntime
├─ XTools_EnhancedCodeFlow
├─ FieldSystemExtensions
├─ XTools_ComponentTimelineRuntime
├─ PointSampling
├─ GeometryTool
├─ RandomShuffles
├─ FormationSystem
└─ XTools（主模块）

编辑器模块层（UncookedOnly）
├─ BlueprintExtensions → BlueprintExtensionsRuntime
├─ ObjectPoolEditor → ObjectPool
├─ SortEditor → Sort
└─ XTools_ComponentTimelineUncooked → ComponentTimelineRuntime

Editor Only 模块
├─ X_AssetEditor → EditorScriptingUtilities
└─ XTools_SwitchLanguage / AutoSizeComments / BlueprintAssist / ElectronicNodes / BlueprintScreenshotTool
```

## 开发规范

### C++ 编码约定
```cpp
// 1. 类命名
UCLASS() class U<ModuleName>Library : public UBlueprintFunctionLibrary  // 蓝图函数库
USTRUCT() struct F<ModuleName>Config                                    // 配置结构体
UENUM() enum class E<ModuleName>State                                   // 枚举类型

// 2. 蓝图暴露
UFUNCTION(BlueprintCallable, Category = "XTools|<模块名>|<子类别>",
    meta = (
        DisplayName = "函数显示名称（中文）",
        Keywords = "关键词,搜索",
        ToolTip = "详细说明（中文）。\n参数:\nParam1 - 参数1说明\n返回值:\nResult - 结果说明"
    ))
static void FunctionName(UPARAM(DisplayName="参数名称") const FVector& Param);

// 3. 元数据标准格式
// - DisplayName: 蓝图中显示的中文名称
// - Keywords: 搜索关键词（逗号分隔）
// - ToolTip: 完整的中文说明文档
// - Category: 统一格式 "XTools|<模块>|<子类别>"

// 4. 注释规范
/** 类/函数的简要说明（Doxygen格式） */
// 实现细节或临时注释
```

### 自定义 K2Node 节点开发
K2Node 是 UE 的自定义蓝图节点系统，用于创建特殊的执行流或通配符节点。

**关键类**：
- `UK2Node`: 所有自定义蓝图节点的基类
- `UK2Node_CallFunction`: 函数调用节点的基类
- `FKismetCompilerContext`: 蓝图编译上下文

**示例**（参考 `BlueprintExtensions` 模块）：
```cpp
// Source/BlueprintExtensions/Public/K2Nodes/K2Node_ForEachArray.h
UCLASS()
class UK2Node_ForEachArray : public UK2Node
{
    GENERATED_BODY()

    // 必须实现的虚函数
    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual void AllocateDefaultPins() override;
    virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
};
```

**通配符节点处理**：
- 使用 `PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard` 创建通配符引脚
- 在 `NotifyPinConnectionListChanged` 中同步类型传播
- 示例节点：`K2Node_Assign`, `K2Node_ForEachArray`, `K2Node_ForEachMap`

### 对象池模块架构
对象池采用职责分离设计：

```cpp
// 核心类职责划分
UObjectPoolSubsystem       // 子系统：全局访问点、生命周期管理
FActorPool                 // 单个 Actor 类的池管理器
FObjectPoolConfigManager   // 配置管理：注册、查询池配置
FObjectPoolMonitor         // 性能监控：统计、建议、诊断
FObjectPoolManager         // 智能管理：自适应扩池、内存优化
FActorStateResetter        // 状态重置：Transform、物理、组件、网络
UK2Node_SpawnActorFromPool // 自定义蓝图节点：替代原生 SpawnActor 节点
```

**重要实现细节**：
- `ExposeOnSpawn` 支持：K2Node 在编译时动态生成属性引脚
- 状态重置：归还到池时自动重置 Transform、Visibility、Physics、Tags 等
- 永不失败机制：池空时回退到正常 SpawnActor 生成

## 常见开发任务

### 添加新的蓝图函数
1. 在对应模块的 `Public/` 目录创建/修改头文件
2. 使用 `UFUNCTION(BlueprintCallable)` 或 `UFUNCTION(BlueprintPure)` 标记
3. 添加完整的中文元数据（DisplayName, Category, ToolTip）
4. 参数使用 `UPARAM(DisplayName="中文名称")` 标记
5. 在 `.cpp` 文件实现函数逻辑

### 添加新的 K2Node 自定义节点
1. 创建 UncookedOnly 模块（如果不存在）
2. 继承 `UK2Node` 或 `UK2Node_CallFunction`
3. 实现必要的虚函数：
   - `GetMenuActions()`: 注册节点到蓝图面板
   - `GetNodeTitle()`: 设置节点显示名称
   - `AllocateDefaultPins()`: 创建输入输出引脚
   - `ExpandNode()`: 编译时展开为底层蓝图节点
4. 在 `.Build.cs` 中添加依赖：`"BlueprintGraph", "KismetCompiler", "UnrealEd"`

### 添加新模块
1. 在 `Source/` 目录创建模块文件夹
2. 创建 `<ModuleName>.Build.cs` 文件
3. 创建 `Public/` 和 `Private/` 子目录
4. 在 `XTools.uplugin` 的 `Modules` 数组添加模块定义
5. 确定模块类型：
   - `Runtime`: 运行时模块（可打包到游戏）
   - `UncookedOnly`: 仅编辑器模块（需要运行时对应模块）
   - `Editor`: 编辑器专用模块

### 修复编译错误
```bash
# 1. 清理项目
.\Scripts\Clean-UEPlugin.ps1

# 2. 重新生成项目文件
"<UE_ROOT>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" -projectfiles -Project="<ProjectPath>/<ProjectName>.uproject" -game -rocket -progress

# 3. 完整重新编译
# VS Code: Tasks -> "Build Editor (Full & Formal)"

# 4. 检查模块依赖
# 编辑 Source/<ModuleName>/<ModuleName>.Build.cs
PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
```

## 蓝图节点分类体系

所有蓝图节点使用统一的分类前缀 `XTools|<模块名>|<子类别>`：

```
XTools|排序|Actor           - Actor 排序（距离、坐标轴、名称）
XTools|排序|基础类型         - 整数、浮点、字符串排序
XTools|排序|向量            - 向量和坐标轴排序
XTools|数组操作|反转/截取/去重 - 数组工具
XTools|随机                 - PRD 算法、洗牌、随机流
XTools|Timeline             - 组件时间轴
XTools|异步                 - 延迟、循环、协程
XTools|对象池|核心           - 对象池 API
XTools|Blueprint Extensions|Loops/Array/Map/Variable/Math/Features
```

## 重要技术细节

### XToolsCore 跨版本兼容性

**模块定位**：XToolsCore 是所有 Runtime 模块的基础依赖，提供跨 UE 版本的兼容性抽象层。

**核心文件**：
1. **XToolsVersionCompat.h** - 版本兼容性宏和工具函数
   ```cpp
   // 版本判断宏
   #if XTOOLS_ENGINE_5_5_OR_LATER
       const int32 Size = Property->GetElementSize();  // UE 5.5+
   #else
       const int32 Size = Property->ElementSize;        // UE 5.3-5.4
   #endif
   
   // 原子操作兼容
   TAtomic<int32> Counter;
   XToolsVersionCompat::AtomicStore(Counter, 10);      // 跨版本安全
   int32 Value = XToolsVersionCompat::AtomicLoad(Counter);
   ```

2. **XToolsErrorReporter.h** - 统一日志和错误处理
   ```cpp
   // 支持所有日志分类类型（包括 FNoLoggingCategory）
   FXToolsErrorReporter::Warning(
       LogBlueprintExtensionsRuntime,
       TEXT("参数验证失败"),
       NAME_None,
       true,   // 显示屏幕提示
       5.0f    // 显示时长
   );
   ```

3. **XToolsDefines.h** - 插件版本和通用宏
   ```cpp
   #define XTOOLS_VERSION_MAJOR 1
   #define XTOOLS_VERSION_MINOR 9
   #define XTOOLS_VERSION_PATCH 3
   ```

**使用方法**：
```cs
// 在模块的 .Build.cs 中添加依赖
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "XToolsCore"  // 添加此依赖
});
```

**API 变更处理**：
- UE 5.4+ 强制执行 IWYU 原则（Include What You Use）→ 显式包含所有头文件
- UE 5.5+ 弃用 `FProperty::ElementSize` → 使用 `GetElementSize()/SetElementSize()`
- UE 5.5+ 弃用 `BufferCommand` → 使用 `BufferFieldCommand_Internal`
- UE 5.4+ `FCompression::GetMaximumCompressedSize` 参数变更 → 条件编译适配
- TAtomic API 在 UE 5.3+ 支持直接赋值，5.0-5.2 需要 `load()/store()`

### EnhancedCodeFlow 异步系统
- 基于 UE Ticker 的异步操作系统，支持协程（C++20 co_await）
- 支持 `FECFHandle` 句柄管理动作生命周期
- 性能分析：编译时宏 `ECF_INSIGHTS_ENABLED` 控制 Insight 跟踪
- 关键类：`FFlow`（静态接口）、`FECFActionBase`（动作基类）、`UECFSubsystem`（子系统）
- 设计模式：句柄系统、实例ID防重复、模板方法+策略、弱引用所有者

### ComponentTimeline 运行时初始化
- 运行时模块：`UComponentTimelineComponent`（实际功能组件）
- 编辑器模块：`UK2Node_ComponentTimelineRuntime`（蓝图节点，使用 Instance 创建）
- 必须在 `BeginPlay` 中调用初始化函数
- 配置：项目设置 → XTools → 组件时间轴设置

### GeometryTool 几何工具
- 基于形状组件的点阵生成（球体/立方体表面）
- 支持随机旋转、缩放、噪声参数和朝向原点控制
- 自定义矩形区域和圆形多层次点阵生成
- 完整的蓝图节点中文参数名

### PointSampling 几何采样
- 泊松圆盘采样（2D/3D，O(N)高效实现）
- 基于面积的网格体采样（三角形面积加权）
- 纹理密度采样（支持亮度反转、压缩纹理处理）
- 等距样条线采样、军事战术阵列型生成

### Sort 模块通用排序
`SortGenericByProperty` 函数使用 UE 反射系统按属性名称排序任意结构体数组：
```cpp
// 实现原理：通过 FProperty 动态获取结构体成员值
const FProperty* Property = StructType->FindPropertyByName(PropertyName);
```

### RandomShuffles PRD 算法
基于 DOTA2 的伪随机分布算法，提供更公平的随机体验：
- 状态管理：每个系统维护独立的 PRD 状态
- 自动持久化：PRD 状态在 WorldSubsystem 中保存
- 关键函数：`GetPRDBool()`, `ResetPRDState()`, `GetPRDProbability()`

## 跨版本兼容性

支持 UE 5.3-5.7，主要差异点：
- 编译器版本：UE 5.3 使用 VS2019，5.4+ 使用 VS2022
- API 变更：通过 XToolsVersionCompat.h 统一适配
- 构建工具：UBT（UnrealBuildTool）的路径和参数在各版本保持兼容
- IWYU 原则：UE 5.4+ 强制执行显式头文件包含

**注意**：UE 5.0-5.2 不受支持（5.0 依赖 .NET 3.1，5.2 有编译器兼容性问题）。

**版本宏定义**：
```cpp
// 在 XToolsCore.Build.cs 中自动定义
#define XTOOLS_ENGINE_5_4_OR_LATER  // UE 5.4+
#define XTOOLS_ENGINE_5_5_OR_LATER  // UE 5.5+
#define XTOOLS_ENGINE_5_6_OR_LATER  // UE 5.6+
```

## 性能考虑

- **BlueprintScreenshotTool**：间隔检查替代每帧 Tick，CPU 占用降低约 60%
- **贝塞尔曲线匀速模式**：计算量大，避免每帧调用
- **几何采样**：点数与网格大小成正比，大型网格使用 `MaxPoints` 参数限制
- **对象池预热**：初始化时有一次性开销，运行时性能提升显著
- **PRD 状态**：使用 thread_local 避免状态共享污染
- **异步操作**：使用 `FECFHandle` 管理生命周期，避免悬空引用
- **Unreal Insights**：EnhancedCodeFlow 支持编译时启用 `ECF_INSIGHTS_ENABLED` 进行性能分析
- **泊松采样**：O(N)时间复杂度，使用缓存避免重复计算

## 文件结构约定

```
XTools/
├── Source/
│   └── <ModuleName>/
│       ├── <ModuleName>.Build.cs        # 模块构建配置
│       ├── Public/
│       │   ├── <ModuleName>.h           # 模块头文件
│       │   ├── <FeatureName>Library.h   # 蓝图函数库
│       │   ├── <FeatureName>Types.h     # 类型定义
│       │   └── K2Nodes/                 # 自定义K2Node节点
│       └── Private/
│           ├── <ModuleName>.cpp         # 模块实现
│           └── <FeatureName>Library.cpp # 函数库实现
├── Scripts/                             # 自动化脚本
│   ├── BuildPlugin-MultiUE.ps1          # 多版本打包
│   ├── Clean-UEPlugin.ps1               # 清理插件
│   └── GenerateClangDB.ps1              # 生成Clang数据库
├── .vscode/                             # VS Code 配置
│   ├── tasks.json                       # 构建任务
│   ├── launch.json                      # 调试配置
│   ├── settings.json                    # 编辑器设置
│   └── c_cpp_properties.json            # IntelliSense配置
├── .github/workflows/                   # GitHub Actions
│   ├── build-plugin-optimized.yml       # 自动构建
│   ├── manual-release.yml               # 手动发布
│   └── update-release-assets.yml        # 更新资产
├── Docs/                                # 文档
│   ├── 版本变更/
│   ├── 开发文档/
│   ├── UE_CPP_最佳实践/
│   └── 命令行本地化最佳实践/
└── Content/                             # 插件内容资产
```

## Git 工作流

- 主分支：`main`
- 提交信息格式：`<type>(<scope>): <description>`（中文描述）
  - `feat`: 新功能
  - `fix`: Bug 修复
  - `refactor`: 重构
  - `perf`: 性能优化
  - `docs`: 文档
  - `chore`: 构建/工具相关
  - `style`: 代码格式（不影响功能）

示例：
```
feat(ObjectPool): 添加 Actor 对象池系统
fix(BlueprintExtensions): 修复 K2Node 通配符类型丢失问题
refactor(X_AssetEditor): 统一插件目录与模块名为 XTools 前缀
perf(PointSampling): 优化批量重命名性能 O(n²)→O(n)
docs: 更新 CLAUDE.md 文档
```

## 调试技巧

### VS Code 调试配置
项目已配置完整的编译任务（`.vscode/tasks.json`），但调试需要手动附加到 UE 编辑器进程：
1. 启动 UE 编辑器
2. VS Code → Run → Attach to Process → 选择 UE 编辑器进程

### UE 内置调试工具
```cpp
// 日志输出
UE_LOG(LogXTools, Warning, TEXT("调试信息: %s"), *SomeString);

// 屏幕消息
GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("调试信息"));

// 断点（仅 Debug 构建）
check(Condition);              // 断言，失败时崩溃
ensure(Condition);             // 软断言，失败时报错但继续运行

// 使用 XToolsErrorReporter 统一错误处理（推荐）
FXToolsErrorReporter::Warning(
    LogBlueprintExtensionsRuntime,
    TEXT("参数验证失败"),
    NAME_None,
    true,   // 显示屏幕提示
    5.0f    // 显示时长
);
```

### 性能分析
```cpp
// 使用 Unreal Insights
TRACE_CPUPROFILER_EVENT_SCOPE(FunctionName);

// EnhancedCodeFlow 内置统计
// 编译时定义 ECF_INSIGHTS_ENABLED=1

// 对象池统计信息
// 控制台命令：objectpool.stats
```

## 常用技能和工具

本项目已配置以下 Claude Code 技能（Skills）：

### git-commit
自动生成符合 Conventional Commits 规范的 git 提交信息并执行提交。
- 自动分析修改文件
- 推断作用域（模块名）
- 生成中文提交信息
- 智能分批提交多模块变更

### changelog-generator
自动管理更新日志：
- 整理变更到 UNRELEASED.md
- 推送版本到 CHANGELOG.md
- 更新版本号

### ue-code-simplifier
简化和优化 UE C++ 插件代码：
- 遵循 Epic Games 编码标准
- 优化代码结构、安全性、性能
- 保持功能不变

### ue-dev-env-config
配置 Unreal Engine 开发环境的 VSCode 设置：
- IntelliSense 配置
- 编译任务
- 调试配置
- 扩展推荐

### web-artifacts-builder
用于创建复杂的、多组件的 claude.ai HTML 构件
- 使用现代前端 Web 技术（React、Tailwind CSS、shadcn/ui）
- 适用于需要状态管理、路由或 shadcn/ui 组件的复杂构件

## 重要新增功能（v1.9.4-v1.9.6）

### GeometryTool 模块（v1.9.4新增）
全新几何工具模块，提供基于形状组件的点阵生成：
```cpp
// 核心功能：
- 球体/立方体表面点阵生成
- 随机旋转、缩放、噪声参数
- 朝向原点的变换控制
- 自定义矩形区域点阵
- 圆形多层次点阵
```

### PointSampling 性能优化（v1.9.5）
- 修复白底纹理采样异常问题
- 新增自动亮度反转功能（智能识别背景色）
- 新增反转采样模式（LuminanceInverted、AlphaInverted）
- 修复压缩纹理采样时尺寸缩放和纵横比错误
- 增强纹理采样自动对齐逻辑，确保点位整齐且不重叠

### X_AssetEditor 增强功能（v1.9.5）
```cpp
// 新增功能：
- 变体命名支持（FAssetNamingPattern 结构体）
- 重命名后的重名冲突自动处理
- 批量重命名性能优化（O(n²)→O(n)）
- 静态缓存正则表达式模式
- Lambda 悬空指针风险修复（使用 TSharedPtr/TWeakPtr）
```

### BlueprintExtensions 修复（v1.9.5）
- 修复带延迟循环节点初次延迟问题（首次迭代立即执行）
- 修复 ForEach 循环节点 Break 执行流中断问题
- 修复 ForEachArrayReverse 引脚重复创建问题
- 修复 Map 随机访问空 Map 边界问题

### RandomShuffles 状态隔离（v1.9.6）
- 使用 thread_local 避免 PRD 状态共享污染
- 修复通用数组随机采样输出状态不一致

## 常见问题和陷阱

### K2Node 开发陷阱
1. **通配符类型传播**：必须在 `NotifyPinConnectionListChanged` 中处理类型同步
2. **动态引脚创建**：避免在 `AllocateDefaultPins` 中调用 `Super::AllocateDefaultPins()` 导致重复创建
3. **编辑器代码包裹**：所有编辑器相关代码必须用 `#if WITH_EDITOR ... #endif` 包裹
4. **循环节点 Break**：Break 后必须确保执行流继续到 Completed 输出引脚

### 模块依赖陷阱
1. **Runtime vs Editor**：Runtime 模块不能依赖 Editor 模块
2. **UncookedOnly 模块**：必须有对应的 Runtime 模块（通过 `AdditionalDependencies` 声明）
3. **XToolsCore 依赖**：所有 Runtime 模块应依赖 XToolsCore 获取版本兼容性
4. **IWYU 原则**：UE 5.4+ 强制显式包含头文件，不能依赖隐式包含

### 性能陷阱
1. **泊松采样缓存键**：必须正确实现 `GetTypeHash` 和 `operator==`，否则缓存失效
2. **Lambda 捕获**：避免捕获原始指针，使用 `TWeakPtr` 或 `TSharedPtr`
3. **数组操作复杂度**：`GetCasePinCount()` 等函数应避免 O(n²) 算法
4. **每帧 Tick**：优先使用间隔检查或 Ticker 替代每帧 Tick

### 跨版本兼容性陷阱
1. **FProperty API**：UE 5.5+ `ElementSize` 改为 `GetElementSize()/SetElementSize()`
2. **原子操作**：UE 5.3+ 支持 `TAtomic` 直接赋值，早期版本需要 `load()/store()`
3. **BufferCommand**：UE 5.5+ 弃用，改用 `BufferFieldCommand_Internal`

## GitHub Actions 自动化

项目包含完整的 CI/CD 工作流：

### manual-release.yml
手动触发创建发行版：
```yaml
# 功能：
- 自动从 Docs/版本变更/CHANGELOG.md 提取版本说明
- 创建 Git Tag 并推送到远程
- 生成 GitHub Release
- 支持预发布版本标记
```

### update-release-assets.yml
更新现有发行版资产：
```yaml
# 功能：
- 手动触发更新指定发行版的资产文件
- 支持上传插件打包文件
```

### build-plugin-optimized.yml
自动化构建和测试：
```yaml
# 触发条件：推送到 main 分支或 Pull Request
# 功能：
- 多版本编译测试
- 构建结果汇总到 GitHub Summary
```

## 文档体系

项目包含完整的文档系统（`Docs/` 目录）：

### 版本变更
- `CHANGELOG.md`: 完整版本历史（v1.9.0-v1.9.5）
- `UNRELEASED.md`: 未发布更新（v1.9.6开发中）
- `BLUEPRINT_NODES.md`: 蓝图节点速查表

### 开发文档
- `UE K2Node 开发指南.md`: 自定义蓝图节点开发（完整生命周期、引脚管理、编译展开）
- `UE K2Node 最佳实践清单.md`: K2Node开发最佳实践
- `FieldSystemExtensions_测试指南.md`: Field System 扩展测试
- `PointSampling模块设计方案.md`: 几何采样模块设计
- `UE5.7-兼容性修复指南.md`: UE 5.7 兼容性指南
- `XTools 设计模式最佳实践.md`: 设计模式和最佳实践（基于EnhancedCodeFlow）

### 专题文档
- `ObjectPool_模块设计文档.md`: 对象池系统设计（职责分离、永不失败机制）
- `Actor状态重置实现总结.md`: Actor 状态重置机制
- `MaterialTools_API分析报告.md`: 材质工具API分析
- `MaterialTools_智能连接系统优化方案.md`: 材质连接优化
- `FieldSystem_工作原理详解.md`: Field System工作原理
- `插件设置系统实现总结.md`: 插件设置系统
- `UE_多版本插件打包_问题与解决方案.md`: 多版本打包方案
- `UE_跨版本条件编译_问题与解决方案.md`: 跨版本兼容性方案

### UE C++ 最佳实践
- `UE_CPP_插件架构设计.md`: 插件架构设计原则
- `UE_CPP_核心最佳实践.md`: 核心开发实践
- `UE_CPP_高级优化技巧.md`: 高级优化技巧
- `rules.md`: UE C++编码规范
- `常见错误检查清单.md`: 常见错误和检查清单
- `UE_本地化工具_蓝图节点参数名支持调研.md`: 本地化工具调研

### 命令行本地化
- `命令行本地化最佳实践/`: 完整的命令行本地化指南
  - 00_Important_Notes.md: 重要说明
  - 01_QuickStart.md: 快速开始
  - 02_Configuration.md: 配置
  - 03_Workflow.md: 工作流程
  - 04_PO_Format.md: PO格式说明
  - 05_FAQ.md: 常见问题

## 版本发布流程

### 1. 开发阶段
- 在 `Docs/版本变更/UNRELEASED.md` 中记录新功能和修复
- 遵循日志格式规范（简洁、清晰、关键信息）

### 2. 发布准备
1. 使用 `changelog-generator` 技能自动整理变更
2. 更新 `XTools.uplugin` 中的版本号
3. 更新 `README.md` 中的版本徽章
4. 运行多版本打包测试：`.\Scripts\BuildPlugin-MultiUE.ps1 -Follow`

### 3. 创建发布
1. 推送到 main 分支触发自动构建
2. 使用 GitHub Actions 的 `manual-release.yml` 创建正式发布
3. 自动从 CHANGELOG.md 提取版本说明
4. 创建 Git Tag 并生成 GitHub Release

### 4. 发布后清理
- 清空 `UNRELEASED.md`（保留说明部分）
- 准备下一轮开发

## 文档写作规范

### UNRELEASED.md 格式
```markdown
### 模块名
- 类型 描述
- 类型 描述
```

**类型说明**：
- **新增** / 新增功能
- **优化** / 改进现有功能
- **修复** / 修复问题
- **移除** / 删除功能
- **调整** / 参数或行为调整

**内容要求**：
- 简洁：每行记录一个独立变更，描述控制在 20 字内
- 清晰：使用"动词 + 对象 + （效果）"格式
- 关键信息：包含重要的参数变化、性能数据、修复的问题
- 不要：避免详细技术细节、文件路径、长篇问题分析
