# ObjectPool - Actor 对象池子系统

Runtime 模块。WorldSubsystem 架构；池失败时可回退普通生成，目标类不可生成时返回 null；Actor 操作仅限游戏线程。

## KEY CLASSES

| 类 | 职责 |
|----|------|
| `UObjectPoolSubsystem` | WorldSubsystem 入口，管理所有池 |
| `FActorPool` | 单类型池，维护 Available/Active 列表 |
| `FObjectPoolManager` | 外部显式调用的维护与扩缩容策略；无子系统内部调度入口 |
| `UObjectPoolLibrary` | 蓝图静态API |
| `IObjectPoolInterface` | Actor 生命周期接口 (Created/Activated/ReturnedToPool) |
| `FObjectPoolUtils` | Actor 状态恢复、配置校验和统计辅助 |

## BLUEPRINT API

```
RegisterActorClass → SpawnActorFromPool → ReturnActorToPool
PrewarmPool (异步，MAX 10/帧)
BatchSpawnActors / BatchReturnActors
AcquireDeferredFromPool → FinalizeSpawnFromPool (两步 Spawn)
GetPoolStats / DisplayPoolStats
```

## FALLBACK CONTRACT

池空且请求类可生成时 → 尝试普通 `SpawnActor`，返回 `EPoolOpResult::FallbackSpawned`；请求类不可生成时返回 null，并保持失败结果码。调用方必须判空，且普通回退 Actor 不属于池。

## THREADING CONTRACT

- 创建、获取、归还、预热和清理 Actor 的接口必须在游戏线程调用
- `FRWLock PoolsRWLock` 和池内锁用于维护容器及蓝图回调重入期间的状态一致性，不代表支持跨线程 Actor 操作
- `FCriticalSection CacheLock` 保护最近访问缓存，但不放宽 UObject/Actor 的游戏线程约束
- `TWeakObjectPtr` 防悬空

## GOTCHAS

- PrewarmPool 是延迟队列，不是当帧执行
- GC 回调自动清理已销毁 Actor，无需手动
- Deferred Spawn 必须调 FinalizeSpawnFromPool 才会触发 BeginPlay
- `CLEANUP_FREQUENCY=100` 次请求触发清理，`SHRINK_THRESHOLD_SECONDS=60s` 回收空闲池
- `FObjectPoolManager` 的维护 API 需由调用方显式触发，不是后台自动调度
