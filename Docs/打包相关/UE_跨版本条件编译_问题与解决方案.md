## UE 跨版本条件编译（5.3-5.6）问题与解决方案

适用范围：XTools 插件跨 UE 5.3-5.6 版本编译，记录 API 变迁和 IWYU 原则导致的兼容性问题。

---

### 版本兼容策略

采用统一版本宏 + 最小修改原则：

```cpp
// Source/XTools/Public/XToolsVersionCompat.h

#define XTOOLS_ENGINE_VERSION_AT_LEAST(Major, Minor) \
    ((ENGINE_MAJOR_VERSION > Major) || \
     (ENGINE_MAJOR_VERSION == Major && ENGINE_MINOR_VERSION >= Minor))

#define XTOOLS_ENGINE_5_4_OR_LATER XTOOLS_ENGINE_VERSION_AT_LEAST(5, 4)
#define XTOOLS_ENGINE_5_5_OR_LATER XTOOLS_ENGINE_VERSION_AT_LEAST(5, 5)
#define XTOOLS_ENGINE_5_6_OR_LATER XTOOLS_ENGINE_VERSION_AT_LEAST(5, 6)
```

---

### 常见问题与修复

#### 1. IWYU 原则导致类型未定义（UE 5.4+）

症状：
```
error C2027: 使用了未定义类型"FOverlapResult"
error C2027: 使用了未定义类型"FHitResult"
```

原因：
- UE 5.4 强制执行 IWYU（Include What You Use）原则
- 头文件仅提供前置声明，不再包含完整定义
- `WorldCollision.h` 只有 `class FOverlapResult;` 前置声明

解决：
```cpp
// XToolsLibrary.cpp / TraceExtensionsLibrary.cpp
#include "Engine/World.h"
#include "Engine/HitResult.h"       // FHitResult 完整定义
#include "Engine/OverlapResult.h"   // FOverlapResult 完整定义
#include "WorldCollision.h"
```

影响版本：UE 5.4, 5.5, 5.6

---

#### 2. Chaos API 弃用（UE 5.5+）

症状：
```
warning C4996: 'FGeometryCollectionPhysicsProxy::BufferCommand': 
Use BufferFieldCommand_Internal instead
```

原因：
- UE 5.5 弃用 `BufferCommand` 方法
- 新 API 名称为 `BufferFieldCommand_Internal`

解决：
```cpp
// XFieldSystemActor.cpp
#include "XToolsVersionCompat.h"

#if XTOOLS_ENGINE_5_5_OR_LATER
    PhysicsProxy->BufferFieldCommand_Internal(Solver, NewCommand);
#else
    PhysicsProxy->BufferCommand(Solver, NewCommand);
#endif
```

影响版本：UE 5.5, 5.6

---

#### 3. FProperty API 弃用（UE 5.5+）

症状：
```
warning C4996: 'FProperty::ElementSize': 
Use GetElementSize/SetElementSize instead
```

原因：
- UE 5.5 弃用直接访问 `ElementSize` 成员变量
- 推荐使用 `GetElementSize()` 方法

解决方案（遵循 Epic 官方做法）：
```cpp
// MapExtensionsLibrary.h
PRAGMA_DISABLE_DEPRECATION_WARNINGS

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UMapExtensionsLibrary : public UBlueprintFunctionLibrary
{
    // ... 继续使用 ElementSize ...
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
```

```cpp
// MapExtensionsLibrary.cpp
#include "Libraries/MapExtensionsLibrary.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ... 实现代码 ...

PRAGMA_ENABLE_DEPRECATION_WARNINGS
```

参考：Epic 在 `CoreUObject/Property.cpp:859` 也使用此方式处理弃用 API。

影响版本：UE 5.5, 5.6

---

#### 4. 跨模块访问版本宏失败

症状：
```
fatal error C1083: 无法打开包括文件: "XToolsVersionCompat.h": No such file or directory
```

原因：
- `FieldSystemExtensions` 模块引用 `XTools` 模块的头文件
- 未在 Build.cs 中声明模块依赖

解决：
```csharp
// FieldSystemExtensions.Build.cs
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core",
        "CoreUObject",
        "Engine",
        "FieldSystemEngine",
        "Chaos",
        "GeometryCollectionEngine",
        "ChaosSolverEngine",
        "XTools"  // 添加 XTools 模块依赖
    }
);
```

影响版本：所有版本

---

### 版本 API 变更记录

| UE 版本 | API 变更 | 影响模块 |
|---------|---------|---------|
| 5.4 | IWYU 原则强制执行 | XTools, BlueprintExtensionsRuntime |
| 5.5 | BufferCommand 弃用 | FieldSystemExtensions |
| 5.5 | FProperty::ElementSize 弃用 | BlueprintExtensionsRuntime |
| 5.6 | 弃用警告视为错误（-Werror） | 所有模块 |

---

### 避免的反模式

错误做法：
```cpp
// 分散的版本判断（难以维护）
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    // ...
#endif

// 依赖前置声明（UE 5.4+ 不够用）
class FHitResult;
void UseHitResult(const FHitResult& Hit);

// 强制迁移稳定 API（增加维护成本）
const int32 Size = Prop->ElementSize;  // 旧代码
// 改为：
const int32 Size = Prop->GetElementSize();  // 新代码（可能带来更多修改）
```

推荐做法：
```cpp
// 统一版本宏
#if XTOOLS_ENGINE_5_5_OR_LATER
    // ...
#endif

// 显式包含完整定义
#include "Engine/HitResult.h"

// 抑制弃用警告（遵循 Epic 做法）
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// ... 使用稳定弃用 API ...
PRAGMA_ENABLE_DEPRECATION_WARNINGS
```

---

### CI 验证结果

全版本编译通过，0 错误 0 警告：

| 版本 | 状态 | 耗时 |
|------|------|------|
| UE 5.3 | BUILD SUCCESSFUL | 3m 35s |
| UE 5.4 | BUILD SUCCESSFUL | 3m 50s |
| UE 5.5 | BUILD SUCCESSFUL | 4m 0s |
| UE 5.6 | BUILD SUCCESSFUL | 3m 39s |

---

### 参考资料

- Epic Games: `Engine/Source/Runtime/CoreUObject/Private/UObject/Property.cpp`（弃用警告处理示例）
- 商业插件参考：BlueprintAssist（版本宏设计）
- UE 官方文档：IWYU 原则（5.4+ 强制执行）

