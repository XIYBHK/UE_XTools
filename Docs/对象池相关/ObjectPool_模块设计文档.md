# ObjectPool 模块说明

本文描述当前 `ObjectPool` Runtime 模块的职责、使用契约和维护边界。公开 API 的最终定义以 `Source/ObjectPool/Public` 为准。

## 1. 目标与边界

ObjectPool 为频繁生成和回收的 `AActor` 提供按类分池、预热、批量操作、延迟生成、生命周期通知和统计能力。

它不保证每次请求都成功：

- 池命中时返回受池管理的 Actor。
- 池不可用或达到限制，但目标类仍可生成时，接口会尝试普通 `SpawnActor` 回退。
- 抽象类、无效类、World 无效或普通生成失败时返回 `nullptr`。
- 普通回退生成的 Actor 不属于池；需要区分来源时使用带 `EPoolOpResult` 的扩展接口。

所有 Actor 创建、激活、归还和销毁操作都必须发生在游戏线程。

## 2. 组成

| 类型 | 职责 |
| --- | --- |
| `UObjectPoolSubsystem` | 每个 World 的池注册表、生成/归还协调、统计和预热队列 |
| `FActorPool` | 单个 Actor 类的可用/活跃实例集合和容量约束 |
| `UObjectPoolLibrary` | Blueprint/C++ 静态入口，封装 WorldContext 和批量操作 |
| `IObjectPoolInterface` | 创建、激活、归还三个生命周期事件 |
| `FObjectPoolUtils` | Actor 状态切换、配置验证和生命周期派发辅助 |
| `FObjectPoolManager` | 可由外部显式调用的维护、分析和扩缩容 API |
| `UObjectPoolSettings` | 项目级开关和默认配置 |
| `ObjectPoolEditor` | SpawnActor 风格的 K2Node 编辑器集成，不进入运行时目标 |

`FObjectPoolManager` 没有子系统内部的定时调度入口。其 `PerformMaintenance`、`AutoResizePool` 等接口是显式管理 API，不应描述为后台自动扩缩容或自动内存回收。

## 3. 启用与配置

对象池子系统默认关闭：

```cpp
UObjectPoolSettings::bEnableObjectPoolSubsystem = false;
```

在编辑器中通过“项目设置 → 游戏 → XTools 对象池设置”启用。关闭时子系统不会创建或维护池，调用方应按接口返回值处理失败。

主要配置来自 `FObjectPoolConfig`：

- 初始池大小
- 硬限制
- 是否启动预热
- 预热数量与策略
- 每帧最大分配数量

注册 Actor 类时可覆盖初始大小和硬限制。配置值会经过模块现有校验与钳制逻辑。

## 4. 基本流程

### 4.1 注册

使用 `UObjectPoolLibrary::RegisterActorClass` 或 `UObjectPoolSubsystem::RegisterActorClass` 注册需要池化的类。注册会建立该类对应的池，并按配置决定是否排队预热。

### 4.2 生成

常用入口：

- `SpawnActorFromPool`
- `SpawnActorFromPoolEx`
- `AcquireOrSpawn`
- `BatchSpawnActors`
- `BatchSpawnActorsEx`

调用方必须对返回指针判空。扩展接口通过 `EPoolOpResult` 区分：

- `Success`：从池中复用；
- `FallbackSpawned`：普通生成成功，但 Actor 不受池管理；
- 其他结果：参数、子系统、World 或生成失败。

不要仅凭返回非空就假定 Actor 可以归还到池；可用 `IsActorPooled` 查询来源。

### 4.3 归还或销毁

- `ReturnActorToPool` / `ReturnActorToPoolEx` 只处理池管理对象。
- `ReleaseOrDespawn` 对池对象执行归还，对非池对象执行 `Destroy`。
- 批量接口支持 `BestEffort` 和 `AllOrNothing` 失败策略；是否保留输入顺序由接口参数决定。

归还前，业务 Actor 应在生命周期事件中清理 Timer、委托、AI、移动、粒子、音频和外部引用。

## 5. Deferred 两阶段生成

需要设置 ExposeOnSpawn 属性或延迟完成生成时使用：

1. `AcquireDeferredFromPool` 获取未完成激活的 Actor；
2. 写入生成期属性；
3. `FinalizeSpawnFromPool` 以最终 `FTransform` 完成激活。

池命中和普通回退都遵守两阶段契约。每个 Deferred Actor 只能完成一次；失败或放弃的对象必须由调用方按接口契约清理，不能长期悬挂。

## 6. 生命周期事件

Actor 可实现 `IObjectPoolInterface`：

- `OnPoolActorCreated`：实例首次加入池时调用一次；
- `OnPoolActorActivated`：每次从池中激活时调用；
- `OnReturnToPool`：归还前调用。

这些事件为 `BlueprintNativeEvent`，Blueprint 和原生 C++ 实现都通过 UE 的 `Execute_*` 派发路径调用。异步生命周期调用捕获 `TWeakObjectPtr<AActor>`，执行时重新解析并校验对象。

生命周期接口是业务状态重置的扩展点。旧的独立状态重置公共类型已移除，当前代码应直接使用生命周期事件和 `FObjectPoolUtils`。

## 7. 预热

`PrewarmPool` 将指定数量的实例准备到池中。启动预热通过延迟队列分帧处理，当前实现的总预算为每帧最多 10 个 Actor，以避免单帧集中创建。

预热不是成功生成的保证：无效类、抽象类、World 失效和构造失败仍会减少实际完成数量。统计和日志应以实际创建结果为准。

`FObjectPoolPreallocator` 和预分配统计类型属于显式工具层；模块不会在后台无限增长池容量。

## 8. 统计与维护

查询入口包括：

- `GetPoolStats`
- `GetAllPoolStats`
- `DisplayPoolStats`
- `IsActorClassRegistered`

控制台命令：

```text
objectpool.stats
objectpool.clear [ClassName]
objectpool.validate
```

`objectpool.clear` 不带类名时清空全部池；短类名存在歧义时应使用完整对象路径。

需要主动维护时，调用方可创建或使用 `FObjectPoolManager`，显式执行使用分析、扩缩容、预分配或报告生成。不要在 Tick 中无条件执行全量维护。

旧的独立内存优化器已移除；内存和使用情况应通过池统计、控制台命令和 Unreal Insights 观测。

## 9. 线程与生命周期规则

- Actor 池操作只允许游戏线程调用。
- 跨帧回调不能捕获裸 Actor 指针；使用弱引用并在回调时校验。
- World 清理时取消预热 Timer、清空延迟队列并释放池。
- 外部保存 Actor 引用时，不应假定其在归还后仍处于激活状态。
- 不要在池锁或状态锁内触发可能重入对象池的外部回调。

## 10. Blueprint 使用建议

典型流程：

```text
BeginPlay
  → RegisterActorClass

需要实例
  → SpawnActorFromPoolEx
  → 检查返回值和 EPoolOpResult
  → 设置本次使用状态

使用结束
  → ReleaseOrDespawn
```

在 Actor Blueprint 中实现生命周期事件，将每次使用相关的状态重置集中到 `OnPoolActorActivated` 和 `OnReturnToPool`。不要依赖 `BeginPlay` 在每次复用时重新执行。

## 11. 测试与变更要求

测试位于：

```text
Source/ObjectPool/Private/Tests/
```

修改以下行为时必须增加或更新确定性测试：

- 池命中、普通回退和不可生成类的结果码；
- 批量操作的失败策略与顺序占位；
- Deferred 完成一次性契约；
- 生命周期同步/异步派发和销毁后跳过；
- 预热预算、容量限制和 World 清理；
- 公共维护 API 的幂等与边界行为。

验证至少包括 Editor Development 编译、`XTools.ObjectPool` 自动化测试和 `git diff --check`。发布前再执行 UE 5.3-5.8 的完整插件打包矩阵。

## 12. 相关文档

- [UE 全自动测试最佳实践](../测试相关/UE全自动测试最佳实践.md)
- [UE 代码审查与实证核验指南](../开发文档/UE代码审查与实证核验指南.md)
- [未发布变更](../版本变更/UNRELEASED.md)
