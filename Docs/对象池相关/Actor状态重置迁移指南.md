# Actor 状态重置迁移指南

## 适用范围

v1.9.9 起，ObjectPool 删除未接线的 `FActorStateResetter`、`FActorResetConfig`、`FActorResetStats` 以及 `FActorPoolMemoryOptimizer` 公共类型。删除会影响直接包含这些头文件的 C++ 代码，以及在蓝图变量中保存旧 `USTRUCT` 类型的资产。

## C++ 迁移

| 旧接口 | 当前替代 |
|---|---|
| `FActorStateResetter::ResetActorForPooling` | `FObjectPoolUtils::ResetActorForPooling(AActor*)` |
| `FActorStateResetter::ActivateActorFromPool` | `FObjectPoolUtils::ActivateActorFromPool(AActor*, FTransform)` |
| `FActorStateResetter::ResetActorState` | `FObjectPoolUtils::BasicActorReset(AActor*, FTransform, bResetPhysics)`；需要完整池化流程时改用上面两个接口 |
| `FActorResetStats` | `FObjectPoolStats`，通过 `FActorPool::GetStats()` 或 `UObjectPoolSubsystem::GetAllPoolStats()` 获取 |
| `FActorPoolMemoryOptimizer::ShouldPreallocate` / `PerformSmartPreallocation` | `FObjectPoolManager::AutoResizePool`，或由调用方显式调用 `FActorPool::PrewarmPool` |
| `FActorPoolMemoryOptimizer::AnalyzeMemoryUsage` | `FObjectPoolUtils::EstimateMemoryUsage` 与 `FObjectPoolUtils::GetOptimizationSuggestions` |

`FActorResetConfig` 没有一一对应的配置结构。若只需要控制物理重置，使用 `BasicActorReset` 的 `bResetPhysics` 参数；若需要音频、粒子或自定义组件状态，请在业务 Actor 或池生命周期接口中实现专用恢复逻辑。

## 蓝图迁移

1. 在升级前打开引用 `Actor重置配置` 或 `Actor重置统计` 的蓝图资产。
2. 将配置变量改为对象池现有配置或普通布尔/统计变量，并把节点改为 `ResetActorForPooling`、`ActivateActorFromPool`、`GetPoolStats` 等 API。
3. 编译并保存所有受影响蓝图，再升级插件版本。旧 `USTRUCT` 类型没有自动资产重定向，未迁移的资产可能出现类型缺失告警。

## 兼容策略

这是破坏性 API 清理，不提供旧头文件兼容壳。源码用户需要在升级提交中完成上述替换；二进制插件用户不应直接依赖这些未接线 C++ 类型。
