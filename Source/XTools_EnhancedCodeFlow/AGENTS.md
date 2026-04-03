# XTools_EnhancedCodeFlow - 异步流程控制

Runtime 模块。GameInstanceSubsystem + Tickable 架构，Handle 生命周期管理。

## KEY CLASSES

| 类 | 职责 |
|----|------|
| `FEnhancedCodeFlow` (别名 `FFlow`) | 静态入口API |
| `UECFSubsystem` | GameInstanceSubsystem，管理所有 Action 的 Tick |
| `FECFHandle` | uint64 Action 标识符 |
| `UECFActionBase` | Action 基类 (Setup→Init→Tick→Complete) |
| `FECFInstanceId` | 防重复实例化 |

## STATIC API (FFlow::)

```
Delay / DelayTicks                          → 延迟执行
AddTicker                                   → 持续Tick
AddTimeline / AddTimelineVector / ...Color  → 插值动画
WaitAndExecute / WhileTrueExecute           → 条件执行
RunAsyncThen                                → 异步+回调

// C++20 协程
co_await FFlow::WaitSeconds(Owner, Time)
co_await FFlow::WaitUntil(Owner, Predicate)
co_await FFlow::RunAsyncAndWait(Owner, Task)
```

## ACTION LIFECYCLE

```
PendingAddActions → (下一帧) → Actions → Tick → Complete/Stop
```
当帧新增的 Action 放入 Pending 队列，下帧才移入 Active。防止迭代器失效。

## BLEND FUNCTIONS

`EECFBlendFunc`: Linear, Cubic, EaseIn, EaseOut, EaseInOut。Exponent 控制曲线形状。

## GOTCHAS

- Action 不持有 Owner 强引用，Owner 销毁不会自动停止 Action
- 协程未完成会永久挂起，需用 `RemoveAllWaitSeconds(true)` 恢复
- Async Task 在独立线程，超时只忽略回调不停线程
- `SetPause()` 暂停整个子系统所有 Action
- 仅游戏线程可启动 Action
- `ECF_INSIGHT_PROFILING` 宏控制 Unreal Insights 追踪
