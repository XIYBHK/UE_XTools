## UE 子系统最佳实践（基于官方文档）

> 目标：给出在 UE 中使用子系统（Subsystem）的类型选型、生命周期、依赖管理、初始化/销毁、线程与异步、蓝图与编辑器可用性、插件化与测试等方面的可落地实践，附最小代码示例与引用链接。

适用版本：UE 5.1+（原则在 5.2/5.3 仍有效，个别 API 以具体版本为准）。

---

### 1. 子系统类型与选型建议

- Engine Subsystem（`UEngineSubsystem`）：
  - 作用域：引擎级，全局唯一，编辑器/运行时均可存在。
  - 适用：全局服务（全局缓存、全局事件总线、跨世界资源管理）。
- Editor Subsystem（`UEditorSubsystem`）：
  - 作用域：仅编辑器，随编辑器启动/关闭；不随打包进入游戏。
  - 适用：编辑器工具链、资产处理、菜单/命令注册。
- GameInstance Subsystem（`UGameInstanceSubsystem`）：
  - 作用域：每个游戏实例一份，覆盖多个关卡/世界生命周期。
  - 适用：会话级服务（账号/匹配/远程配置/跨关卡数据）。
- World Subsystem（`UWorldSubsystem`）：
  - 作用域：每个世界一份（PIE/关卡流送会产生多份）。
  - 适用：与世界强相关的逻辑（导航、天气、世界级管理器）。
- LocalPlayer Subsystem（`ULocalPlayerSubsystem`）：
  - 作用域：每个本地玩家一份（Split-Screen 会有多份）。
  - 适用：玩家本地状态、输入路由、UI/设置管理。

选型要点：
- 是否需要跨关卡持续存在？选 `GameInstanceSubsystem`。
- 是否与特定世界强绑定？选 `WorldSubsystem`。
- 是否每名本地玩家隔离？选 `LocalPlayerSubsystem`。
- 是否编辑器专用？选 `EditorSubsystem`。
- 是否全局一次性服务？选 `EngineSubsystem`。

---

### 2. 生命周期与初始化顺序

- Initialize/Deinitialize：在拥有者对象构造完毕后，由引擎调用 `Initialize`，在销毁前调用 `Deinitialize`。禁止在构造函数做重逻辑。
- 世界/玩家多实例：`UWorldSubsystem`、`ULocalPlayerSubsystem` 会出现多实例，避免持久化缓存跨实例误用。
- PIE 与多世界：编辑器 PIE 可能存在 EditorWorld + PIEWorld，多份 `UWorldSubsystem` 并存，调试时确认上下文。

依赖顺序（推荐）：
- 在 `Initialize(FSubsystemCollectionBase& Collection)` 内显式声明依赖：
  - `Collection.InitializeDependency<USomeOtherSubsystem>();`
  - 由框架保证依赖先初始化，有助于消除竞态。

---

### 3. 最小示例（GameInstance Subsystem）

```cpp
#include "Subsystems/GameInstanceSubsystem.h"

UCLASS(BlueprintType)
class YOURMODULE_API UMyGameSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override
    {
        // 声明依赖，确保依赖子系统先初始化
        // Collection.InitializeDependency<USomeOtherSubsystem>();
        // 轻量初始化；避免阻塞游戏线程
    }

    virtual void Deinitialize() override
    {
        // 释放资源，解绑代理、取消定时器/任务
    }
};
```

访问方式：
```cpp
UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
if (GI)
{
    if (UMyGameSubsystem* S = GI->GetSubsystem<UMyGameSubsystem>())
    {
        // 使用子系统
    }
}
```

蓝图：右键图表搜索 “Get Game Instance Subsystem”，选择 `MyGameSubsystem` 即可。

---

### 4. 访问方式速查

- Engine：`GEngine->GetEngineSubsystem<UMyEngineSubsystem>()`
- Editor（仅编辑器）：`GEditor->GetEditorSubsystem<UMyEditorSubsystem>()`（`#if WITH_EDITOR`）
- GameInstance：`GameInstance->GetSubsystem<UMyGameSubsystem>()`
- World：`World->GetSubsystem<UMyWorldSubsystem>()`
- LocalPlayer：`LocalPlayer->GetSubsystem<UMyLocalPlayerSubsystem>()`

建议：减少全局单例的直接使用，通过“拥有者 → 子系统”链式访问，利于测试与解耦。

---

### 5. 初始化、资源与依赖管理

- 轻初始化：`Initialize` 只做轻量工作；重任务放到异步或延迟到首次使用。
- 明确依赖：使用 `InitializeDependency<T>()`，避免在 `Initialize` 中手动查询尚未就绪的子系统。
- 解除绑定：在 `Deinitialize` 统一解除委托、注册项与计时器。
- 配置注入：通过模块设置对象、DataAsset 或构造入参（蓝图 `ExposeOnSpawn`）注入外部配置。

---

### 6. Tick、异步与线程

- Tick 方案：
  - `UWorldSubsystem` 可考虑使用 `UTickableWorldSubsystem`（如可用）或世界委托（`FWorldDelegates::OnWorldTickStart/End`）。
  - 其他子系统可使用 `FTSTicker`、`FTimerManager`、或注册引擎/世界代理。
- 异步建议：
  - 使用任务系统/线程池：`Async(EAsyncExecution::ThreadPool, ...)` 或 `FAsyncTask`；
  - 游戏线程同步回调用 `AsyncTask(ENamedThreads::GameThread, ...)`；
  - 避免在 `Initialize` 做阻塞 IO。

---

### 7. 蓝图、编辑器与运行时

- 蓝图友好：对子系统类加 `BlueprintType`，对外 API 采用 `BlueprintCallable/BlueprintPure`。
- 编辑器专用逻辑仅放入 `UEditorSubsystem`；运行时模块中避免 `WITH_EDITOR` 宏下的大量逻辑混杂。
- 资源与路径：编辑器子系统可注册菜单、命令与面板；运行时子系统只做游戏时所需逻辑。

---

### 8. 插件化与模块边界

- 运行时子系统放在 Runtime 模块；编辑器子系统放在 Editor 模块；公共 API 置于 Public 头文件。
- API 宏：为对外可见类/函数加 `<ModuleName>_API` 宏（UBT 自动生成）。
- 依赖声明：在 `.Build.cs` 中按最小依赖原则添加模块，保持解耦。

---

### 9. 测试与可维护性

- 自动化测试：对关键子系统编写功能测试（Automation Spec/Functional Tests），构造拥有者（Engine/World/GI/LP）上下文后再获取子系统实例。
- 可替换性：通过接口或抽象基类隔离实现，游戏实例在启动时选择具体实现（配置/工厂）。
- 监控与日志：统一前缀与类别，避免噪声；关键路径加统计与错误报告。

---

### 10. 常见陷阱与规避

- 在构造函数中执行重逻辑：应移动到 `Initialize`。
- 依赖子系统尚未初始化：使用 `InitializeDependency<T>()` 明确顺序。
- 缓存跨世界引用：`UWorldSubsystem` 多实例，避免跨世界静态缓存；必要时通过 `WorldContextObject` 解析。
- PIE 多世界：调试时确认是 EditorWorld 还是 PIEWorld；谨慎使用 `GetWorld()`。
- 阻塞主线程：IO/网络等放入异步；回主线程再触发游戏对象变更。

---

### 11. 进阶示例（依赖与异步）

```cpp
UCLASS()
class YOURMODULE_API UInventorySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override
    {
        // 确保 ProfileSubsystem 先初始化
        // Collection.InitializeDependency<UProfileSubsystem>();
    }

    UFUNCTION(BlueprintCallable)
    void PreloadItemTables()
    {
        // 在线程池预加载数据，完成后回到游戏线程
        Async(EAsyncExecution::ThreadPool, [this]()
        {
            // 重任务 ...
            AsyncTask(ENamedThreads::GameThread, [this]()
            {
                // 更新游戏对象/广播完成事件
            });
        });
    }
};
```

---

### 12. 目录与命名建议

- 目录：`Source/<Module>/Public/Subsystems/*.h`、`Source/<Module>/Private/Subsystems/*.cpp`。
- 命名：`U<Feature><Scope>Subsystem`（如 `UInventoryGameInstanceSubsystem`）。
- 文档：在模块 README/指南中说明各子系统职责、依赖与访问入口。

---

### 引用与延伸阅读

- 官方文档（5.1）：[编程子系统 | 虚幻引擎](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/programming-subsystems-in-unreal-engine?application_version=5.1)
- API 参考（示例）：`UGameInstanceSubsystem`、`UWorldSubsystem`、`ULocalPlayerSubsystem`、`UEngineSubsystem`、`UEditorSubsystem`（随引擎版本查询对应文档）。


