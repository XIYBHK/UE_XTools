# UE 5.3-5.8 跨版本条件编译

XTools 支持 Unreal Engine 5.3-5.8。跨版本适配的目标是把真实 API 差异限制在最小范围，同时保持 UE 5.4+ IWYU 和 Runtime/Editor 模块边界。

## 1. 统一版本入口

版本宏定义在：

```text
Source/XToolsCore/Public/XToolsVersionCompat.h
```

头文件直接基于引擎提供的 `ENGINE_MAJOR_VERSION` 和 `ENGINE_MINOR_VERSION` 推导，不在 `.Build.cs` 重复注入版本宏。

所有 Runtime 模块依赖 `XToolsCore`，需要版本判断的源文件显式包含：

```cpp
#include "XToolsVersionCompat.h"
```

## 2. 可用宏

```cpp
XTOOLS_ENGINE_VERSION_AT_LEAST(Major, Minor)

XTOOLS_ENGINE_5_4_OR_LATER
XTOOLS_ENGINE_5_5_OR_LATER
XTOOLS_ENGINE_5_6_OR_LATER
XTOOLS_ENGINE_5_7_OR_LATER
XTOOLS_ENGINE_5_8_OR_LATER
```

示例：

```cpp
#if XTOOLS_ENGINE_5_5_OR_LATER
    const int32 ElementSize = Property->GetElementSize();
#else
    const int32 ElementSize = Property->ElementSize;
#endif
```

已有兼容封装时优先使用封装，例如：

```cpp
const int32 ElementSize = XTOOLS_GET_ELEMENT_SIZE(Property);
```

## 3. 何时使用条件编译

只在下列情况使用：

- API、类型或枚举在支持版本之间确实改名；
- 参数签名或访问级别发生变化；
- 头文件路径或模块归属变化；
- 某版本缺少后续版本能力，需要最小替代实现。

不要用于：

- 掩盖缺失 include；
- 绕过错误的 Runtime/Editor 依赖；
- 为未验证的未来版本预写分支；
- 在多个模块复制相同版本判断；
- 保留已经不支持版本的历史代码。

## 4. IWYU 与模块归属

UE 5.4+ 对 IWYU 更严格。遇到符号缺失时按以下顺序处理：

1. 在引擎源码或 API 文档中确认拥有该符号的头；
2. 在使用文件中显式 include；
3. 根据头和导出符号确认模块归属；
4. 在 `.Build.cs` 添加最小 Public/Private 依赖；
5. 同时验证 Editor 和 Game Target。

不要把大型聚合头当兼容层，也不要仅凭类名前缀猜测模块。例如 K2 节点头、编译器辅助类和编辑器 UI 可能属于不同模块。

## 5. Public 与 Private 依赖

- 公共头的签名、基类或成员暴露某模块类型时，该模块通常是 Public 依赖。
- 类型只在 `.cpp` 或私有头中使用时，优先 Private 依赖。
- Runtime 模块不能依赖 `UnrealEd`、`BlueprintGraph`、`GraphEditor` 等 Editor 模块。
- K2Node 放在 UncookedOnly/Editor 模块，运行时函数放在对应 Runtime 模块。

版本差异不能改变依赖方向；必要时拆分模块。

## 6. 兼容封装原则

### 6.1 小而集中

同一差异被多个模块使用时，放入 `XToolsVersionCompat.h` 的小型 inline 函数或宏。单一调用点的差异直接留在调用点附近，避免创建一次性抽象。

### 6.2 保持语义一致

不同版本分支必须提供相同外部行为。不要只追求“每个版本都能编译”，还要对返回值、错误路径、线程和生命周期契约做测试。

### 6.3 不隐藏弃用警告

优先迁移到较新 API，并为旧版本保留分支。不要通过全局关闭 deprecation warning 维持旧调用。

### 6.4 明确版本边界

条件应表达首次发生变化的版本：

```cpp
#if XTOOLS_ENGINE_5_6_OR_LATER
    // 5.6+ API
#else
    // 5.3-5.5 API
#endif
```

不要写难以审计的多个相等判断。

## 7. 常见差异类型

### FProperty ElementSize

使用 `XTOOLS_GET_ELEMENT_SIZE(Property)`，避免在业务代码直接访问已弃用成员。设置 ElementSize 属于引擎/UHT 内部行为，非必要不要在插件代码中执行。

### 原子操作

需要跨版本一致语义时使用 `XToolsVersionCompat` 提供的 `AtomicLoad`、`AtomicStore`、`AtomicExchange` 和 CompareExchange 封装。读-算-写状态转换仍需在同一锁或同一原子操作中完成；封装单次读写不会自动让复合操作原子化。

### 编辑器 API

编辑器 API 的头路径和模块归属可能变化。先验证当前最低和最高支持版本，不要因为某个版本构建通过就假定整个矩阵相同。

## 8. 验证矩阵

每个跨版本改动至少验证：

1. `git diff --check`；
2. UE 5.3 Editor Development；
3. 受影响模块的自动化测试；
4. UE 5.8 BuildPlugin 或 Editor Target；
5. 发布前 UE 5.3-5.8 完整 BuildPlugin 矩阵；
6. Runtime 变更额外验证 Game Development 和 Shipping。

若本机没有全部引擎，必须在交付说明中列明实际覆盖版本，并由 CI 补齐矩阵。

## 9. 审查清单

- [ ] 版本差异已由源码或编译错误证实，不是猜测；
- [ ] 版本判断位于最小范围；
- [ ] 没有重复定义引擎版本宏；
- [ ] Public/Private 依赖与头文件暴露一致；
- [ ] Runtime 未引入 Editor 模块；
- [ ] 较新版本不依赖隐式 include；
- [ ] 各版本分支外部语义一致；
- [ ] 自动化测试覆盖差异行为；
- [ ] 发布矩阵覆盖 UE 5.3-5.8。

相关文档：[多版本插件打包](UE_多版本插件打包_问题与解决方案.md) 与 [命令行编译](命令行编译指令.md)。
