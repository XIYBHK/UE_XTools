# EnhancedCodeFlow 时间轴事件轨道

ECF 的六类时间轴动作均支持命名事件轨道：标量、向量、线性颜色，以及对应的 Float、Vector、LinearColor 曲线时间轴。

## C++

在 `FFlow::AddTimeline*` 或 `FFlow::AddCustomTimeline*` 的末尾传入事件数组和回调：

```cpp
TArray<FECFTimelineEvent> Events;
Events.Add({0.25f, TEXT("Quarter")});
Events.Add({0.75f, TEXT("ThreeQuarter")});

FFlow::AddTimeline(
    Owner, 0.f, 1.f, 1.f,
    TickFunc, FinishFunc,
    EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {},
    EECFPlayDirection::Forward, MoveTemp(Events),
    [](FName EventName, float EventTime)
    {
        // 处理事件
    });
```

事件时间使用时间轴原生坐标：从 `0` 到时间轴长度。正向播放按时间升序触发，反向播放按时间降序触发；单次 Tick 跨过多个事件时会依次回调。循环跨越边界时按原生 `FTimeline` 规则处理边界段和落点段。

`SetActionTime` 是显式定位，不触发事件；调用 `ReverseAction` 或设置播放方向后，事件轨道会从当前播放位置继续按新方向工作。同一时间点的多个事件保持传入数组顺序。

## 蓝图

六个对应的 ECF 时间轴异步节点新增 `Events` 输入数组和 `On Event` 输出委托。数组元素填写事件时间与事件名称，在 `On Event` 中根据 `Event Name` 分派逻辑；`Event Time` 返回命中的原生时间轴位置。
