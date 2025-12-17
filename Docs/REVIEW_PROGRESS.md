# XTools 插件代码审查进度（可中断续跑）

更新时间：2025-12-17  
约定：`XTools_` 前缀模块为第三方整合插件，不纳入审查（除非单独点名）。  

## 进度总览

| 模块 | 状态 | 备注 |
|---|---|---|
| `PointSampling` | 已审查 + 已修复 | 泊松缓存键、坐标/随机流一致性、纹理采样与除零等问题 |
| `XToolsCore` | 已审查 + 已修复 | 版本宏同步、`XToolsDefines.h` 自洽、`XTOOLS_SET_ELEMENT_SIZE` 编译期分支 |
| `RandomShuffles` | 已审查 + 已修复 | 加权采样越界崩溃、日志宏分号、Build.cs 缩进 |
| `ObjectPool` | 已审查 + 已修复 | GC 委托 + move 语义潜在 UAF、GC 回调锁、去除不必要 const_cast |
| `FormationSystem` | 初步审查 + 已修复（构建侧） | 仅处理跨平台 `DLLEXPORT/DLLIMPORT` 兼容；算法/日志刷屏等待后续 |
| `Sort` | 未开始 |  |
| `BlueprintExtensionsRuntime` | 初步审查 + 已修复（关键崩溃点） | 修复除零/空指针/WorldContext 断言崩溃、Transform 返回引用 |
| `FieldSystemExtensions` | 初步审查 + 已修复（稳定性/刷屏） | 修复 GC 缓存去重、IsValid 替代 IsValidLowLevel、日志降噪 |
| `XTools` | 已审查 + 已修复 | 修复 `IMPLEMENT_MODULE` 分号、移除 try/catch、缓存键 Hash/Equals 合约、补 IWYU 与降噪 |

## 已做修改（简要记录）

### 版本同步到 `1.9.3`
- `XTools.uplugin`：`VersionName` 从 `1.9.2` → `1.9.3`
- `Source/XToolsCore/Public/XToolsDefines.h`：`XTOOLS_VERSION_PATCH` 从 `2` → `3`
- `README.md`：徽章版本更新、补充 `v1.9.3 (2025-12-15)` 简要条目
- `CLAUDE.md`：文档引用的版本号/范围更新（并修正示例中的 `XTOOLS_VERSION_PATCH`）

### `PointSampling`（核心问题修复）
- `Source/PointSampling/Public/Core/SamplingCache.h`：修复缓存键 `TMap` 的 Hash/Equals 合约（由“容差相等”改为“量化键”），并把 `MaxAttempts`、`Scale` 等纳入键（避免错误复用/命中率极低）。
- `Source/PointSampling/Private/Algorithms/PoissonSamplingHelpers.cpp`：`ApplyTransform` 的父缩放除零防护。
- `Source/PointSampling/Public/Algorithms/PoissonDiskSampling.h` + `.../Private/...cpp`：新增 `GeneratePoisson2DFromStream/GeneratePoisson3DFromStream`，并把高频 `Log` 降为 `Verbose`。
- `Source/PointSampling/Private/Sampling/SplineSamplingHelper.cpp`：边界采样改用 `GeneratePoisson2DFromStream`，保证随机流可复现。
- `Source/PointSampling/Private/Sampling/TextureSamplingHelper.cpp`：修复“纹理密度泊松采样”坐标归一化/居中输出；运行时 `FloatRGBA` bytes-per-pixel 推断与 FP16/FP32 兼容读取。
- `Source/PointSampling/Private/Sampling/MeshSamplingHelper.cpp`：删除未使用的局部变量。

### `XToolsCore`
- `Source/XToolsCore/Public/XToolsVersionCompat.h`：`XTOOLS_SET_ELEMENT_SIZE` 改为编译期 `#if/#else` 分支，避免 UE5.3/5.4 下错误编译到 `SetElementSize` 分支。
- `Source/XToolsCore/Public/XToolsDefines.h`：补 `CoreMinimal.h` 与 `DECLARE_LOG_CATEGORY_EXTERN(LogXToolsCore, ...)`，确保头文件自洽。

### `FormationSystem` / `XTools`（构建侧跨平台兼容）
- `Source/FormationSystem/FormationSystem.Build.cs`、`Source/XTools/XTools.Build.cs`：`DLLEXPORT/DLLIMPORT` 在非 Windows 平台定义为空，避免 `__declspec` 导致 Mac/Linux 编译失败。

### `RandomShuffles`
- `Source/RandomShuffles/Public/RandomSample.h`：修复 `MinIndexQueue` 容量错误导致越界崩溃（容量应为 `sampleSize` 而非 `validCount`）。
- `Source/RandomShuffles/Private/RandomShuffleLog.cpp`：`DEFINE_LOG_CATEGORY` 补分号。
- `Source/RandomShuffles/RandomShuffles.Build.cs`：修正缩进（不改逻辑）。

### `ObjectPool`
- `Source/ObjectPool/Public/ActorPool.h` + `.../Private/ActorPool.cpp`：禁用并移除 `FActorPool` move 语义（GC 委托捕获 `this`，move 会潜在 UAF）。
- `Source/ObjectPool/Private/ActorPool.cpp`：GC 回调中补 `PoolLock` 写锁，符合内部锁约定。
- `Source/ObjectPool/Private/ObjectPoolManager.cpp`：移除对 `mutable UsageHistory` 的 `const_cast`。

### 其他
- `Source/XTools_AutoSizeComments/Private/AutoSizeCommentsModule.cpp`：`DEFINE_LOG_CATEGORY` 补分号（防止编译问题）。

### `XTools`（主模块）
- `Source/XTools/Private/XToolsModule.cpp`：`IMPLEMENT_MODULE(FXToolsModule, XTools)` 补分号，避免编译问题。
- `Source/XTools/Private/XBlueprintLibraryCleanupTool.cpp`：移除 `try/catch (...)`（UE 默认禁用 C++ 异常），补 `FApp/FPlatformTime/FModuleManager` 等 IWYU 依赖，并将高频“从内存/快速获取”日志降为 `Verbose`。
- `Source/XTools/Public/XToolsLibrary.h`：移除枚举注释中的 `⚠️` 等特殊字符；清理 `CalculateBezierPoint` 的无效元数据键并统一格式。
- `Source/XTools/Private/XToolsLibrary.cpp`：修复 `FGridParametersKey` 的 Hash/Equals 合约（量化 Transform/Extent/Spacing），增加 `GEngine` 空指针保护；Bezier 匀速模式对曲线输出做 Clamp；采样诊断日志统一改为 `Verbose` 且仅在 `bEnableDebugDraw` 时输出。
- `Source/XTools/XTools.Build.cs`：Windows 下追加 `/utf-8` 编译参数，避免非中文系统区域设置下中文日志/显示名乱码。

### `BlueprintExtensionsRuntime`
- `Source/BlueprintExtensionsRuntime/Private/BlueprintExtensionsRuntime.cpp`：`IMPLEMENT_MODULE(...)` 统一为带分号写法（不影响行为）。
- `Source/BlueprintExtensionsRuntime/Private/Libraries/MathExtensionsLibrary.cpp`：`StableFrame` 增加 `DeltaTime/PastDeltaTime` 为 0 的防护，避免除零产生 Inf/NaN。
- `Source/BlueprintExtensionsRuntime/Private/Libraries/ObjectExtensionsLibrary.cpp`：`ClearObject` 增加 `IsValid(Object)` 检查，避免空指针崩溃。
- `Source/BlueprintExtensionsRuntime/Public/Libraries/TransformExtensionsLibrary.h` + `.../Private/...cpp`：`TLocation/TRotation` 改为按值返回，移除 `static` 引用返回（更符合蓝图函数库与线程安全预期）。
- `Source/BlueprintExtensionsRuntime/Private/Features/SupportSystemLibrary.cpp`：增加 `DeltaTime/ErrorRange` 防护与 `acos` 输入 Clamp；World 获取改为 `ReturnNull`，避免开发期断言崩溃。
- `Source/BlueprintExtensionsRuntime/Private/Libraries/TraceExtensionsLibrary.cpp`：World 获取改为 `ReturnNull`，避免开发期断言崩溃。

### `FieldSystemExtensions`
- `Source/FieldSystemExtensions/Private/XFieldSystemActor.cpp`：`CachedGeometryCollections` 使用 `AddUnique` 防重复；遍历时改用 `IsValid`；日志 Owner 判空；移除未使用变量/包含；把逐对象日志降到 `Verbose`。

## 下一步建议（便于接着做）
1. 继续补完 `XTools` 模块剩余文件审查（尤其是缓存/线程安全/日志级别与 EditorOnly 分支）。
2. 按非 `XTools_` 模块顺序继续：`Sort` → `BlueprintExtensions` →（按需）`PhysicsAssetOptimizer`/`ObjectPoolEditor`/`SortEditor`/`X_AssetEditor`。
3. 对 `FormationSystem` 做“非构建侧”的深入审查：日志级别、算法复杂度、编码/本地化乱码。
