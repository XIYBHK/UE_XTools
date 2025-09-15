## UE C++ 收集的技巧

> 用于长期收录 UE C++ 实用技巧与片段。每条目包含：作用、用法、示例代码、注意事项、引用链接。
> 收录规范：
> - 每条目需包含明确标题与“引用”来源；
> - 示例代码应可编译，避免伪代码；
> - 若涉及引擎版本差异，请在条目内注明适用版本（例如 UE5.3+）。
---

### 1. 在蓝图实例化时可设置属性（ExposeOnSpawn）

- **作用**：让属性在蓝图的 “Spawn Actor from Class”“Construct Object from Class” 等节点上作为输入引脚出现，实例化时即可传参，参数在 Construction Script 执行前生效。
- **关键元数据**：`UPROPERTY(..., meta=(ExposeOnSpawn=true))`

#### 用法示例（Actor）
```cpp
UCLASS(BlueprintType, Blueprintable)
class YOURMODULE_API AYourActor : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn=true))
    FString DataSourceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn=true))
    TObjectPtr<UDataAsset> Config;
};
```

#### 适用场景
- 运行时 Spawn 需要传入 ID/配置/资源引用等；
- UObject 使用 “Construct Object from Class” 创建时需要入参；
- 需要在 Construction Script 中读取入参完成初始化。

#### 注意事项
- 通常与 `EditAnywhere`/`EditInstanceOnly` 与 `BlueprintReadWrite` 搭配使用；
- 与组件的 `BlueprintSpawnableComponent` 概念不同，请勿混淆；
- 若属性不参与构造逻辑且创建后再设值也可工作，则无需标记。

#### 引用
- 博客：UE C++ 写的变量开放到蓝图的构造函数、新建类、SpawnActor（`ExposeOnSpawn`） — [链接](https://dt.cq.cn/archives/1178)

---

### 2. UE C++ 线程同步：FCriticalSection 与 FScopeLock（作用域锁）

- **作用**：在多线程环境下保护共享资源，避免数据竞争；`FScopeLock` 基于 RAII，作用域进入加锁、离开自动解锁，降低手动管理风险。
- **核心类型**：
  - `FCriticalSection`：互斥量；
  - `FScopeLock`：作用域锁封装，构造即 `Lock()`，析构自动 `Unlock()`。

#### 用法示例
```cpp
// 成员或全局：同一受保护资源应共享同一把锁
FCriticalSection RefreshMutex;

void UMyObj::RefreshSharedState()
{
    FScopeLock Lock(&RefreshMutex); // 进入作用域即加锁
    // 安全访问共享数据...
}

void UMyObj::RefreshTwice()
{
    {
        FScopeLock Lock(&RefreshMutex);
        // 第一次更新...
    } // 作用域结束自动解锁

    {
        FScopeLock Lock(&RefreshMutex);
        // 第二次更新...
    }
}
```

#### 注意事项
- 同一资源使用同一把锁，避免混用多把锁导致竞态；
- 缩小持锁范围，避免在锁内做长耗时 IO/网络/阻塞操作；
- 统一锁顺序（多锁场景）以规避死锁；
- 读多写少的场景可考虑 `FRWLock`（读写锁）；
- 适用版本：UE4/UE5 通用；命名空间与包含由模块环境决定。

#### 引用
- 博客：UE 虚幻引擎 C++ 锁，线程锁，作用域锁 — [链接](https://dt.cq.cn/archives/518)

---

### 3. 每帧性能监控：STAT 标记与 Unreal Insights 采集

- **作用**：在高频 Tick/循环中监控函数/代码块耗时，定位瓶颈；开发阶段可视化到 Unreal Insights 或使用 STAT 命令分组查看。
- **常用手段**：
  - Cycle 统计：`DECLARE_CYCLE_STAT` + `SCOPE_CYCLE_COUNTER`（或 `QUICK_SCOPE_CYCLE_COUNTER`）。
  - Insights 事件：`TRACE_CPUPROFILER_EVENT_SCOPE(Name)`。

#### 用法示例（Cycle 统计）
```cpp
// .h：定义统计项与分组
DECLARE_CYCLE_STAT(TEXT("MyFeature Tick"), STAT_MyFeature_Tick, STATGROUP_XToolkit);

// .cpp：在热点代码块标记
void UMyObj::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_MyFeature_Tick);
    // ... 你的逻辑
}

// 轻量快速标记（无需 DECLARE）：
void Foo()
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_Foo);
    // ... 你的逻辑
}
```

#### 用法示例（Unreal Insights 事件）
```cpp
void Bar()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Bar);
    // ... 你的逻辑
}
```

#### 采集与查看
- Insights：编辑器 菜单 Window → Developer Tools → Unreal Insights，启动/附加采集会话后在 CPU 轨迹中查看事件名称与耗时；命令行也可使用 `-trace=cpu` 启动。
- STAT：在控制台使用 `stat group XToolkit` 或 `stat startfile`/`stat stopfile` 进行本地采集并离线分析。

#### 注意事项
- 标记应尽量细粒度但避免过度；在高频路径中不要打印日志代替剖析。
- 重任务放入异步线程，回主线程再更新对象，保持对比口径一致。
- Shipping 构建中 Cycle/Trace 标记通常会被裁剪或不生效；仅在开发/测试启用即可。

#### 引用
- 博客：监控每帧循环中 C++ 函数的性能 — [链接](https://dt.cq.cn/archives/357)
