# XToolsCore - 基础兼容层

所有 Runtime 模块的唯一必须依赖。PreDefault 加载，提供三件事：版本兼容、错误处理、防御宏。

## HEADERS (4个，全部 Public)

| 头文件 | 用途 |
|--------|------|
| `XToolsCore.h` | 模块入口，LogXToolsCore，FXToolsLogCategories |
| `XToolsVersionCompat.h` | 版本宏 + 原子操作统一API + FProperty兼容 |
| `XToolsErrorReporter.h` | 模板化错误上报 + XTOOLS_LOG_* 宏 |
| `XToolsDefines.h` | 插件版本号 + 平台/调试标志 + 防御宏 |

## VERSION MACROS

```cpp
XTOOLS_ENGINE_5_4_OR_LATER   // IWYU 强制
XTOOLS_ENGINE_5_5_OR_LATER   // ElementSize 废弃, BufferCommand 更名
XTOOLS_ENGINE_5_6_OR_LATER   // 更严格 deprecation

XTOOLS_ENGINE_VERSION_AT_LEAST(Major, Minor)  // 通用版本判断
XTOOLS_GET_ELEMENT_SIZE(Prop)                 // 跨版本安全访问
```

## ERROR HANDLING API

```cpp
// 宏 (自动 __FILE__/__LINE__)
XTOOLS_LOG_ERROR(LogCategory, TEXT("msg"));
XTOOLS_LOG_WARNING_EX(LogCategory, TEXT("msg"), NAME_None, true, 5.0f);

// 断言+上报
XTOOLS_ENSURE_OR_ERROR_RETURN(Condition, LogCat, "fmt", ...);

// 防御
XTOOLS_SAFE_EXECUTE(Ptr, Ptr->Do());
XTOOLS_CHECK_VALID(Actor, false);
XTOOLS_CHECK_ARRAY_INDEX(Arr, Idx, false);
```

## BUILD.CS 要点

- 无外部依赖 (仅 Core/CoreUObject/Engine)
- `Build.cs` 根据引擎版本自动注入 `XTOOLS_ENGINE_5_X_OR_LATER` 宏
- 新模块仅需 `PublicDependencyModuleNames.Add("XToolsCore")`

## GOTCHAS

- 原子操作 API 在 5.3 和 5.5+ 底层实现不同，必须用 `XTOOLS_ATOMIC_*` 宏
- `XTOOLS_GET_ELEMENT_SIZE` 返回 `int32`，5.5 之前直接取字段，之后调方法
- 新增版本宏时必须同步更新 `XToolsCore.Build.cs` 的 `PublicDefinitions`
