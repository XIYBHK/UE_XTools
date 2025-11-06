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

#### 5. UMetaData 重构为 FMetaData（UE 5.6）

症状：
```
error C2143: 语法错误: 缺少';'(在'*'的前面)
error C2440: 无法从"FMetaData"转换为"UMetaData *"
error C4099: "FMetaData": 类型名称前使用"class"现在使用的是"struct"
```

原因：
- UE 5.6 将 `UMetaData` 类重构为 `FMetaData` 结构
- `UPackage::GetMetaData()` 返回类型从 `UMetaData*` 改为 `FMetaData&`
- 这是一次重大的 API 重构

解决方案（采用类型别名 - BlueprintAssist 官方方案）：

```cpp
// BlueprintAssist/Public/BlueprintAssistGlobals.h
// UE 5.6+ 类型别名：处理 API 变化
#if BA_UE_VERSION_OR_LATER(5, 6)
using FBAMetaData = class FMetaData;  // UE 5.6: FMetaData
using FBAVector2 = FVector2f;         // UE 5.6: FVector2f
#else
using FBAMetaData = class UMetaData;  // UE 5.5-: UMetaData
using FBAVector2 = FVector2D;         // UE 5.5-: FVector2D
#endif
```

```cpp
// BlueprintAssist/Public/BlueprintAssistUtils.h
class UMetaData;
class FMetaData;

// UE 5.6: FBAMetaData = FMetaData*, UE 5.5-: FBAMetaData = UMetaData*
static FBAMetaData* GetPackageMetaData(UPackage* Package);
static FBAMetaData* GetNodeMetaData(UEdGraphNode* Node);
```

```cpp
// BlueprintAssist/Private/BlueprintAssistUtils.cpp
FBAMetaData* FBAUtils::GetPackageMetaData(UPackage* Package)
{
#if BA_UE_VERSION_OR_LATER(5, 6)
    // UE 5.6: GetMetaData() 返回 FMetaData& 引用，返回其地址
    return &Package->GetMetaData();
#else
    // UE 5.5-: GetMetaData() 直接返回 UMetaData* 指针
    return Package->GetMetaData();
#endif
}
```

**优势**：
- ✅ 类型安全，无需 `reinterpret_cast`
- ✅ 一处定义，全局复用
- ✅ 代码清晰易读易维护
- ✅ 符合 C++ 最佳实践

影响版本：UE 5.6  
影响模块：BlueprintAssist

---

#### 6. FVector2D 精度变更为 FVector2f（UE 5.6）

症状：
```
error C2440: 无法从 FVector2D 转换为 UE::Math::TVector2<float>
error C2664: 无法将参数从 const FVector2D 转换为 const FBAVector2&
```

原因：
- UE 5.6 Slate API 从 `FVector2D` (double) 改为 `FVector2f` (float)
- 但某些API（如 `GetPasteLocation()`, `GetCursorPos()`）仍返回 `FVector2D`
- `FVector2f` 和 `FVector2D` 之间**无隐式转换**
- `FMath::ClosestPointOnSegment2D()` 在5.6返回 `FVector2f` 而非 `FVector2D`

解决方案 1（类型别名 + 显式转换）：
```cpp
// BlueprintAssistGlobals.h - 定义类型别名
#if BA_UE_VERSION_OR_LATER(5, 6)
using FBAVector2 = FVector2f;
#else
using FBAVector2 = FVector2D;
#endif

// 使用场景 - 需要显式转换API返回值
const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();  // 返回FVector2D
const FBAVector2 MenuLocation(CursorPos.X, CursorPos.Y);  // 显式转换为FBAVector2
OpenContextMenu(MenuLocation, ...);  // 函数接受FBAVector2参数
```

解决方案 2（FMath API 返回值转换）：
```cpp
// ElectronicNodes/Private/ENConnectionDrawingPolicy.cpp
#if defined(ENGINE_MAJOR_VERSION) && defined(ENGINE_MINOR_VERSION) && \
    ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6
    // UE 5.6: FMath API 返回 FVector2f，需要转换回 FVector2D
    const FVector2f ClosestPointF = FMath::ClosestPointOnSegment2D(
        FVector2f(LocalMousePosition), 
        FVector2f(Start), 
        FVector2f(End)
    );
    const FVector2D TemporaryPoint(ClosestPointF.X, ClosestPointF.Y);  // 显式转换
#else
    const FVector2D TemporaryPoint = FMath::ClosestPointOnSegment2D(
        LocalMousePosition, Start, End
    );
#endif
```

**关键点**：
- ⚠️ **无隐式转换**：`FVector2f` 和 `FVector2D` 不能自动转换
- ✅ **使用构造函数转换**：`FBAVector2(x, y)` 是类型安全的显式转换
- ✅ **中间变量正确类型**：用原始类型接收API返回值，再转换
- ❌ **避免 reinterpret_cast**：使用构造函数而非指针转换

影响版本：UE 5.6  
影响模块：BlueprintAssist, ElectronicNodes

**修复文件**：
- `BlueprintAssist/Private/BlueprintAssistWidgets/BABlueprintActionMenu.cpp`
- `BlueprintAssist/Private/BlueprintAssistActions/BlueprintAssistGraphActions.cpp`
- `BlueprintAssist/Private/BlueprintAssistActions/BlueprintAssistNodeActions.cpp`
- `ElectronicNodes/Private/ENConnectionDrawingPolicy.cpp`

---

#### 7. Material Graph 连接策略头文件路径变更（UE 5.6）

症状：
```
fatal error C1083: 无法打开包括文件: "MaterialGraphConnectionDrawingPolicy.cpp"
```

原因：
- UE 5.6 重构了 Material Graph 渲染管线
- `MaterialGraphConnectionDrawingPolicy.cpp` 不再可用或路径变更

解决方案（优雅降级）：
```cpp
// ElectronicNodes/Private/Policies/ENMaterialGraphConnectionDrawingPolicy.h
#if defined(ENGINE_MAJOR_VERSION) && defined(ENGINE_MINOR_VERSION) && \
    ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6
#pragma message("警告: UE 5.6 MaterialGraph增强连接绘制暂不支持，使用标准绘制")
// UE 5.6: 优雅降级，不启用 Material Graph 增强连接绘制
#else
// UE 5.5-: 正常启用 Material Graph 增强连接绘制
#include "MaterialGraphConnectionDrawingPolicy.cpp"

class FENMaterialGraphConnectionDrawingPolicy : public FMaterialGraphConnectionDrawingPolicy
{
    // ... 实现 ...
};
#endif
```

```cpp
// ElectronicNodes/Private/ENConnectionDrawingPolicy.cpp
#if defined(ENGINE_MAJOR_VERSION) && defined(ENGINE_MINOR_VERSION) && \
    ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6
    // UE 5.6: 对 Material Graph 使用标准连接绘制
    return MakeShareable(new FENConnectionDrawingPolicy(...));
#else
    // UE 5.5-: 使用增强的 Material Graph 连接绘制
    return MakeShareable(new FENMaterialGraphConnectionDrawingPolicy(...));
#endif
```

影响版本：UE 5.6  
影响模块：ElectronicNodes

---

### 版本 API 变更记录

| UE 版本 | API 变更 | 影响模块 |
|---------|---------|---------|
| 5.4 | IWYU 原则强制执行 | XTools, BlueprintExtensionsRuntime |
| 5.5 | BufferCommand 弃用 | FieldSystemExtensions |
| 5.5 | FProperty::ElementSize 弃用 | BlueprintExtensionsRuntime |
| 5.6 | UMetaData → FMetaData 重构 | BlueprintAssist |
| 5.6 | FVector2D → FVector2f 精度变更 | BlueprintAssist, ElectronicNodes |
| 5.6 | MaterialGraphConnectionDrawingPolicy 路径变更 | ElectronicNodes |
| 5.6 | 弃用警告视为错误（-Werror） | 所有模块 |

---

### 避免的反模式

错误做法：
```cpp
// 1. 分散的版本判断（难以维护）
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    // ...
#endif

// 2. 依赖前置声明（UE 5.4+ 不够用）
class FHitResult;
void UseHitResult(const FHitResult& Hit);

// 3. 强制迁移稳定 API（增加维护成本）
const int32 Size = Prop->ElementSize;  // 旧代码
// 改为：
const int32 Size = Prop->GetElementSize();  // 新代码（可能带来更多修改）

// 4. 使用 reinterpret_cast 处理类型变更（不安全）
UMetaData* MetaData = reinterpret_cast<UMetaData*>(&Package->GetMetaData());

// 5. 每个使用点都添加条件编译（代码冗余）
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6
    FMetaData& MetaData = Package->GetMetaData();
    MetaData.SetValue(...);
#else
    UMetaData* MetaData = Package->GetMetaData();
    MetaData->SetValue(...);
#endif
```

推荐做法：
```cpp
// 1. 统一版本宏
#if XTOOLS_ENGINE_5_5_OR_LATER
    // ...
#endif

// 2. 显式包含完整定义
#include "Engine/HitResult.h"

// 3. 抑制弃用警告（遵循 Epic 做法）
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// ... 使用稳定弃用 API ...
PRAGMA_ENABLE_DEPRECATION_WARNINGS

// 4. 使用类型别名处理类型变更（类型安全 + 代码清晰）
#if BA_UE_VERSION_OR_LATER(5, 6)
using FBAMetaData = class FMetaData;
#else
using FBAMetaData = class UMetaData;
#endif

// 5. 封装辅助函数集中处理版本差异
FBAMetaData* GetPackageMetaData(UPackage* Package)
{
#if BA_UE_VERSION_OR_LATER(5, 6)
    return &Package->GetMetaData();  // 返回引用的地址
#else
    return Package->GetMetaData();   // 直接返回指针
#endif
}
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
- 商业插件参考：BlueprintAssist（版本宏设计、类型别名方案）
  - `BlueprintAssistGlobals.h`：类型别名定义（UE 5.6 UMetaData/FMetaData 处理）
  - `BlueprintAssistUtils.cpp`：辅助函数封装版本差异
- UE 官方文档：IWYU 原则（5.4+ 强制执行）
- C++ 最佳实践：类型别名（`using`）优于类型转换（`reinterpret_cast`）

