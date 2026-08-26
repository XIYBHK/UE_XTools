## UE 多版本插件打包（5.3–5.7）问题与解决方案

适用范围：本仓库的 `XTools` 插件，使用 UE5.3–5.7 的 RunUAT BuildPlugin 在 Win64 平台打包。

### 打包方式（推荐）
- 使用各版本引擎的 `RunUAT.bat` 执行 BuildPlugin（无需新建工程）：
  - 关键参数：`-NoHostProject -StrictIncludes -TargetPlatforms=Win64`
  - `BuildPlugin` 默认关闭 Unity/PCH，并启用严格包含检查，需要按 IWYU 精确包含头文件。

示例（PowerShell）：
```powershell
$plugin = "E:\Unreal Projects\cpp0623\Plugins\UE_XTools\XTools.uplugin"
$out    = "E:\Plugin_Packages"
$engines = @(
  "D:\Program Files\Epic Games\UE_5.4",
  "D:\Program Files\Epic Games\UE_5.5",
  "D:\Program Files\Epic Games\UE_5.6"
)
foreach ($e in $engines) {
  $ver = Split-Path $e -Leaf
  & "$e\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
    -Plugin=$plugin `
    -Package=(Join-Path $out ("XTools-" + $ver)) `
    -TargetPlatforms=Win64 `
    -NoHostProject `
    -StrictIncludes
}
```

### 常见错误与修复
1) OBJECTPOOL_API 宏冲突（Definitions.h 重复定义）
- 症状：UAT 报告 {Module}_API 重复定义/警告，ExitCode=6。
- 原因：模块手动定义了 `OBJECTPOOL_API`，与 UBT 自动生成的宏冲突。
- 解决：
  - 移除头文件/Build.cs 中对 `OBJECTPOOL_API` 的手动定义；
  - 继续在导出类/函数上使用 `OBJECTPOOL_API` 标注，宏由 UBT 统一生成。

2) C4702 不可达代码（SpawnActorFromPool 尾部 return）
- 症状：`ObjectPoolLibrary.cpp` 在 5.4 下 C4702。
- 原因：回退分支里已 `return`，函数末尾又多一次 `return`；或存在“静态紧急 Actor + AddToRoot”造成死代码路径。
- 解决：
  - 删除尾部多余 `return`；移除“静态紧急 Actor”分支；
  - 回退全部失败时统一 `return nullptr`，由调用方决定是否改用 `SpawnActor` 兜底。

3) C4530 使用异常但未启用展开语义
- 症状：`try/catch` 引发 C4530。
- 原因：UE 默认禁用 C++ 异常。
- 解决：移除 `try/catch`，改用返回值判断/日志；如必须用第三方异常，需在 Target/Build 配置启用，但不推荐。

4) 严格包含导致类型未定义（-StrictIncludes）
- 症状：`FTextProperty` 未定义、K2/Editor API 未包含头等。
- 解决（按文件）：
  - `Sort/Private/SortLibrary.cpp`
    - 添加：`#include "UObject/UnrealType.h"`，`#include "UObject/TextProperty.h"`
    - `FTextProperty` 分支改为：按值取 `FText` 并转 `FString` 比较，避免右值绑定问题。
  - `SortEditor/Public/K2Node_SmartSort.h`
    - 补：`CoreMinimal.h`、`UObject/ObjectMacros.h`、`UObject/ScriptMacros.h`、`K2Node.h`、`EdGraphSchema_K2.h`、`BlueprintActionDatabaseRegistrar.h`、`Kismet2/BlueprintEditorUtils.h` 等。
  - `SortEditor/Private/SortTestLibrary.cpp`
    - 补：`#include "Engine/Engine.h"`（`GEngine`）。
  - `ObjectPool/*`
    - 异步：`#include "Async/Async.h"`
    - Editor 专属 API：用 `#if WITH_EDITOR` 包裹（如 `SetActorLabel`）。
  - `X_AssetEditor/Private/CollisionTools/X_CollisionManager.cpp`
    - 将过时 `Rendering/StaticMeshRenderData.h` 改为 `#include "StaticMeshResources.h"`。
  - `X_AssetEditor/Private/CollisionTools/X_CollisionSettingsDialog.cpp`
    - 补：`#include "Widgets/Layout/SScrollBox.h"`。
  - `X_AssetEditor/Private/AssetNaming/X_AssetNamingManager.cpp`
    - 补：`#include "ScopedTransaction.h"`。
  - `X_AssetEditor/Public/MenuExtensions/X_MenuExtensionManager.h`
    - 补：`#include "Materials/MaterialFunctionInterface.h"`。

5) 资源加载与编辑器依赖
- 症状：运行时构建引用了编辑器专属的 `ConstructorHelpers::FObjectFinder` 或 Editor API。
- 解决：
  - 运行时代码使用 `FSoftObjectPath` + `TryLoad()` 动态加载引擎内容（例如 `/Engine/BasicShapes/Cube.Cube`）。
  - 编辑器功能用 `#if WITH_EDITOR` 宏保护。

6) 不同源码路径导致“修复未生效”
- 症状：UAT 仍然编译旧文件（HostProject 插件源码与工作区不一致）。
- 解决：
  - 打包前把工作区源码镜像到 UAT 使用的 HostProject 插件目录：
    - `robocopy "E:\Unreal Projects\cpp0623\Plugins\UE_XTools\Source" "E:\Plugin_Packages\XTools-UE_5.X\HostProject\Plugins\XTools\Source" /MIR /XD Binaries Intermediate .git .vs`
  - 清理：删除 `HostProject\Plugins\XTools\Binaries` 与 `Intermediate` 再打包。

7) 去除不必要模块依赖
- 症状：无实际引用却声明 `Niagara` 依赖，带来编译告警或冗余。
- 解决：从 `ObjectPool.Build.cs` 与 `XTools.uplugin` 移除 `Niagara`（若源码未用）。

### 版本差异处理策略
- 优先“通用代码”兼容 5.3–5.7（严格包含、运行时与编辑器分层、无异常）。
- 只有遇到真实 API/类型差异时再加最小化版本宏：
```cpp
#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION>=5
// 仅 5.5+ 的代码
#endif
```

### 复核清单（出包前）
- [ ] 移除自定义 `{Module}_API` 定义
- [ ] 无 `try/catch`，异步用 `Async/Async.h`
- [ ] Editor API 用 `WITH_EDITOR` 包裹（**关键：`RerunConstructionScripts` 等编辑器专用方法**）
- [ ] 严格包含缺失的头已补齐（见上文列表）
- [ ] 可能未定义的构建宏使用 `defined(Macro) && Macro` 判断
- [ ] 跨版本引擎 API 已确认覆盖 UE 5.3–5.7，未使用目标版本中已移除的函数
- [ ] Game 与 Editor Target 均已编译，避免仅在编辑器构建中掩盖运行时问题
- [ ] `SpawnActorFromPool` 无不可达代码；失败返回 `nullptr`
- [ ] 无不必要的第三方/引擎模块依赖
- [ ] HostProject 源码已同步且清理了 `Intermediate/Binaries`
- [ ] 遵循 IWYU 原则，避免过度包含头文件

### 常用命令
- 单版本：
```powershell
& 'D:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\RunUAT.bat' `
  BuildPlugin -Plugin='E:\Unreal Projects\cpp0623\Plugins\UE_XTools\XTools.uplugin' `
  -Package='E:\Plugin_Packages\XTools-UE_5.4' -TargetPlatforms=Win64 -NoHostProject -StrictIncludes
```
- 批量（见文首示例）。

### 产物
- 产出目录：`E:\Plugin_Packages\XTools-UE_5.X`（包含 Binaries、Resources、配置与 `FilterPlugin*.ini` 过滤结果）。

---
维护者备注：如新增平台或引擎小版本后有新增错误，请在本文件追加“症状/原因/解决”条目并注明具体引擎版本与编译日志片段。

### 批量编译脚本使用（BuildPlugin-MultiUE.ps1）
- 脚本路径：`Scripts/BuildPlugin-MultiUE.ps1`
- 功能：对一个或多个 UE 版本执行 `RunUAT BuildPlugin` 打包，可选实时日志跟踪与清理输出。

示例
```powershell
# 1) 自动探测（已安装 5.4/5.5/5.6）
.\Scripts\\BuildPlugin-MultiUE.ps1

# 2) 指定引擎列表 + 自定义输出目录
.\Scripts\\BuildPlugin-MultiUE.ps1 `
  -EngineRoots "D:\Program Files\Epic Games\UE_5.4","D:\Program Files\Epic Games\UE_5.5" `
  -OutputBase "E:\Plugin_Packages"

# 3) 仅打包 5.6，显示实时日志并先清空输出
.\Scripts\\BuildPlugin-MultiUE.ps1 `
  -EngineRoots "F:\ProgramFiles\UE_5.6" `
  -OutputBase "E:\Plugin_Packages" `
  -StrictIncludes -NoHostProject -CleanOutput -Follow
```

参数说明
- `-EngineRoots <string[]>`：UE 安装根目录列表；不传则自动探测内置候选（5.4/5.5/5.6）。
- `-OutputBase <string>`：输出根目录，默认 `..\\..\\Plugin_Packages`。
- `-TargetPlatforms <string>`：目标平台，默认 `Win64`。
- `-StrictIncludes`：启用严格包含（默认开）。
- `-NoHostProject`：不使用 HostProject（默认开）。
- `-CleanOutput`：打包前清空对应版本的输出目录。
- `-Follow`：实时跟踪 UAT 日志（每 3 秒输出最新 40 行）。

注意
- `-StrictIncludes` 会要求源码符合 IWYU；本仓库已按“常见错误与修复”章节补全头文件。
- 如果未安装某版本 UE，需在 `-EngineRoots` 中传入正确路径；否则脚本会报 `RunUAT not found`。

### CI 发布故障记录（v1.9.6，2026-07-16）

首次发布流水线 [CI #127](https://github.com/XIYBHK/UE_XTools/actions/runs/29491780443) 的 UE 5.3–5.7 五个矩阵任务均在 `Build Plugin with BuildPlugin Command` 阶段失败，Release 任务因此跳过。修复后，[CI #128](https://github.com/XIYBHK/UE_XTools/actions/runs/29494531968) 的五个版本全部通过，并成功创建包含五个版本 ZIP 的 Release。

1. `ENABLE_DRAW_DEBUG` 未定义（UE 5.3/5.4）
   - 文件：`Source/BlueprintExtensionsRuntime/Private/Libraries/BulletHomingLibrary.cpp`
   - 症状：`error C4668: ENABLE_DRAW_DEBUG is not defined as a preprocessor macro`。
   - 原因：关闭调试绘制的构建中宏可能不存在，而未定义标识符警告被视为错误。
   - 修复：包含 `DrawDebugHelpers.h` 和调试实现处统一使用 `#if defined(ENABLE_DRAW_DEBUG) && ENABLE_DRAW_DEBUG`。

2. 浮点比较 API 已移除（UE 5.5/5.6/5.7）
   - 文件：四个延迟循环 K2Node：`K2Node_ForEachArrayReverse.cpp`、`K2Node_ForEachLoopWithDelay.cpp`、`K2Node_ForLoopWithDelay.cpp`、`K2Node_ForLoopWithDelayReverse.cpp`。
   - 症状：`LessEqual_FloatFloat is not a member of UKismetMathLibrary`。
   - 原因：`LessEqual_FloatFloat` 在 UE 5.5+ 已移除。
   - 修复：改用 UE 5.3–5.7 均存在的 `UKismetMathLibrary::LessEqual_DoubleDouble`。

3. `FTimerManager` 类型不完整（UE 5.3/5.4）
   - 历史文件：`Source/ObjectPool/Private/ActorStateResetter.cpp`（已删除；当前迁移方案见《Actor 状态重置迁移指南》）
   - 症状：`error C2027: use of undefined type FTimerManager`。
   - 原因：依赖传递包含，严格 IWYU 构建下缺少完整类型。
   - 修复：显式包含 `TimerManager.h`。

4. `NewObject` / `GetTransientPackage` 类型信息不足（UE 5.3/5.4）
   - 文件：`Source/PointSampling/Private/Sampling/TextureSamplingHelper.cpp`
   - 症状：`error C2672: NewObject: no matching overloaded function found`。
   - 原因：严格 IWYU 构建下不可见 `UPackage` 的完整继承关系。
   - 修复：显式包含 `UObject/Package.h`。

5. Runtime 构建调用 Editor-only 材质 API（UE 5.3/5.4）
   - 文件：`Source/PointSampling/Private/Sampling/MeshSamplingHelper.cpp`
   - 症状：`GetTexturesInPropertyChain is not a member of UMaterialInterface`。
   - 原因：该 API 受引擎头文件中的 `#if WITH_EDITOR` 保护，而 BuildPlugin 还会编译 `WITH_EDITOR=0` 的运行时目标。
   - 修复：初始化输出指针，并将 BaseColor 属性链查询包裹在 `#if WITH_EDITOR` 中；非编辑器构建返回 `false`，继续使用材质纹理参数、`GetUsedTextures` 和材质颜色参数等既有回退路径。

验证结果：本地 UE 5.4、5.5、5.6 的 Game 与 Editor Target 均编译通过；CI #128 的 UE 5.3、5.4、5.5、5.6、5.7 BuildPlugin 矩阵全部通过。发布完成条件应同时包含全部矩阵任务成功和 `Create GitHub Release` 成功，不能只以提交标签或单版本本地编译作为完成依据。


### 新增补充（2025-08）
- 5.5 链接错误：UK2Node_Timeline 符号未导出
  - 症状：链接阶段 `UK2Node_Timeline::GetToolTipHeading()` 未解析（LNK2001/LNK1120）。
  - 原因：5.5 中该符号导出不稳定，自定义节点继承链引用到该实现导致链接失败。
  - 解决：在自定义基类 `UK2Node_HackTimeline` 提供本地覆盖：头文件声明 `virtual FText GetToolTipHeading() const override;`，源码实现 `return LOCTEXT("TimelineNodeHeading", "Timeline");`。仅影响编辑器提示。
- Build.cs 弃用字段（5.5+）
  - 症状：`ModuleRules.bEnableUndefinedIdentifierWarnings` 被标记弃用（CS0618），但构建不受影响。
  - 方案：以 5.3 为主力时可不改；如需清警告，按版本宏切换到 `UndefinedIdentifierWarningLevel`，或局部 `#pragma warning disable 0618`（不建议全局）。
- 极端兜底与返回值
  - 说明：`UObjectPoolLibrary::SpawnActorFromPool` 在“无法获取子系统且多级回退均失败”时返回 `nullptr`，由调用方回退到 `SpawnActor`。避免静态 AddToRoot 对象带来的不可达代码与 GC 风险。
- 严格包含与编辑器 API 说明
  - `ObjectPool/*`：异步使用 `#include "Async/Async.h"`；编辑器 API 使用 `#if WITH_EDITOR`（如 `AActor::SetActorLabel`）。
- **关键发现：RerunConstructionScripts 编辑器 API 保护（2025-08）**
  - 症状：`ActorPool.cpp(314): error C2039: "RerunConstructionScripts": 不是 "AActor" 的成员`
  - 原因：`AActor::RerunConstructionScripts()` 是编辑器专用 API，在运行时构建（Runtime Build）中不可用。
  - 解决：用 `#if WITH_EDITOR` 宏包裹编辑器专用调用：
    ```cpp
    #if WITH_EDITOR
        Actor->RerunConstructionScripts();
    #endif
    ```
  - **重要：此修复是解决打包失败的关键，而非头文件包含问题。遵循 IWYU (Include What You Use) 原则，只包含实际使用的头文件即可。**

