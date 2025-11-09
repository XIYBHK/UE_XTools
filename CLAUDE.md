# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

XTools 是一个为 Unreal Engine 5.3-5.6 设计的模块化插件系统，提供蓝图节点和 C++ 功能库。插件采用多模块架构，每个模块独立编译和打包。

**核心设计原则**：
- 单一职责原则：每个模块专注于一个特定功能领域
- 蓝图友好：所有功能通过 `UFUNCTION(BlueprintCallable/Pure)` 暴露
- 中文优先：所有元数据、注释、文档使用中文
- 跨版本兼容：支持 UE 5.3-5.6，零警告编译

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
# 为 UE 5.3-5.6 打包插件
.\Scripts\BuildPlugin-MultiUE.ps1 -EngineRoots "UE_5.3","UE_5.4","UE_5.5","UE_5.6" -Follow

# 支持的参数：
# -EngineRoots   指定引擎路径列表
# -OutputBase    自定义输出目录（默认：Plugin_Packages/）
# -Follow        实时跟踪打包进度
# -Clean         打包前清理输出目录
```

### 清理项目
```powershell
# 清理插件临时文件
.\Scripts\Clean-UEPlugin.ps1

# 清理范围：Binaries/, Intermediate/, .vs/, *.sln, *.lib, *.exp
```

## 模块架构

插件采用运行时/编辑器分离的模块架构：

### 基础模块
- **XToolsCore** (Runtime, PreDefault): 核心工具和跨版本兼容性层
  - 提供 `XToolsVersionCompat.h`: UE 5.3-5.6 版本兼容性宏
  - 提供 `XToolsErrorReporter.h`: 统一错误/日志处理
  - 提供 `XToolsDefines.h`: 插件版本和通用宏定义
  - **所有 Runtime 模块都应依赖此模块**
  - **Editor 模块可选依赖此模块**

### 核心模块
- **XTools**: 主模块（Editor），包含编辑器工具和插件入口
- **Sort**: 排序算法库（整数、浮点、字符串、向量、Actor、通用结构体）
- **RandomShuffles**: PRD（伪随机分布）算法和数组洗牌
- **EnhancedCodeFlow**: 异步操作和流程控制（延迟、时间轴、循环、协程）
- **PointSampling**: 几何工具（静态网格体内部点阵生成、贝塞尔曲线）
- **FormationSystem**: 编队系统

### 运行时-编辑器模块对
- **ComponentTimelineRuntime** + **ComponentTimelineUncooked**: 组件时间轴系统
- **BlueprintExtensionsRuntime** + **BlueprintExtensions**: 自定义 K2Node 蓝图节点
- **ObjectPool** + **ObjectPoolEditor**: Actor 对象池系统
- **Sort** + **SortEditor**: 排序工具的编辑器扩展

### 编辑器专用模块
- **X_AssetEditor**: 资产批量处理工具（碰撞、材质函数、命名规范）
- **XTools_AutoSizeComments**: 集成第三方编辑器插件（已汉化）
- **XTools_BlueprintAssist**: 集成第三方编辑器插件（已汉化）
- **XTools_ElectronicNodes**: 集成第三方编辑器插件（已汉化）

### 模块依赖关系
```
BlueprintExtensions (UncookedOnly) → BlueprintExtensionsRuntime (Runtime)
ObjectPoolEditor (UncookedOnly) → ObjectPool (Runtime)
ComponentTimelineUncooked (UncookedOnly) → ComponentTimelineRuntime (Runtime)
SortEditor (UncookedOnly) → Sort (Runtime)
X_AssetEditor (Editor) → EditorScriptingUtilities (引擎插件)
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
   #define XTOOLS_VERSION_PATCH 1
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
- UE 5.5+ 弃用 `FProperty::ElementSize` → 使用条件编译适配
- UE 5.5+ 弃用 `BufferCommand` → 使用 `BufferFieldCommand_Internal`
- TAtomic API 在 UE 5.3+ 支持直接赋值，5.0-5.2 需要 `load()/store()`

### EnhancedCodeFlow 异步系统
- 基于 UE Ticker 的异步操作系统
- 支持 `FECFHandle` 句柄管理动作生命周期
- 性能分析：编译时宏 `ECF_INSIGHTS_ENABLED` 控制 Insight 跟踪
- 关键类：`FFlow`（静态接口）、`FECFActionBase`（动作基类）

### ComponentTimeline 运行时初始化
- 运行时模块：`UComponentTimelineComponent`（实际功能组件）
- 编辑器模块：`UK2Node_ComponentTimelineRuntime`（蓝图节点）
- 必须在 `BeginPlay` 中调用初始化函数
- 配置：项目设置 → XTools → 组件时间轴设置

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

支持 UE 5.3-5.6，主要差异点：
- 编译器版本：UE 5.3 使用 VS2019，5.4+ 使用 VS2022
- API 变更：少量引擎 API 在版本间有微小差异（主要是参数顺序）
- 构建工具：UBT（UnrealBuildTool）的路径和参数在各版本保持兼容

**注意**：UE 5.0-5.2 不受支持（5.0 依赖 .NET 3.1，5.2 有编译器兼容性问题）。

## 性能考虑

- **贝塞尔曲线匀速模式**：计算量大，避免每帧调用
- **几何采样**：点数与网格大小成正比，大型网格使用 `MaxPoints` 参数限制
- **对象池预热**：初始化时有一次性开销，运行时性能提升显著
- **PRD 状态**：每个系统的 PRD 状态持久保存，注意内存使用
- **异步操作**：使用 `FECFHandle` 管理生命周期，避免悬空引用

## 文件结构约定

```
XTools/
├── Source/
│   └── <ModuleName>/
│       ├── <ModuleName>.Build.cs        # 模块构建配置
│       ├── Public/
│       │   ├── <ModuleName>.h           # 模块头文件
│       │   ├── <FeatureName>Library.h   # 蓝图函数库
│       │   └── <FeatureName>Types.h     # 类型定义
│       └── Private/
│           ├── <ModuleName>.cpp         # 模块实现
│           └── <FeatureName>Library.cpp # 函数库实现
├── Scripts/                             # 自动化脚本
├── .vscode/                             # VS Code 配置
│   └── tasks.json                       # 构建任务配置
└── Content/                             # 插件内容资产
```

## Git 工作流

- 主分支：`main`
- 提交信息格式：`<type>: <description>`（中文描述）
  - `feat`: 新功能
  - `fix`: Bug 修复
  - `refactor`: 重构
  - `docs`: 文档
  - `chore`: 构建/工具相关

示例：
```
feat: 添加 Actor 对象池系统
fix: 修复 K2Node 通配符类型丢失问题
refactor: 统一插件目录与模块名为 XTools 前缀
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
```

### 性能分析
```cpp
// 使用 Unreal Insights
TRACE_CPUPROFILER_EVENT_SCOPE(FunctionName);

// EnhancedCodeFlow 内置统计
// 编译时定义 ECF_INSIGHTS_ENABLED=1
```
