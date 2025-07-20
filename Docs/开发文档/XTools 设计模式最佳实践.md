# XTools 设计模式最佳实践

> 基于 EnhancedCodeFlow 模块的优秀设计模式分析与提取

[![UE Version](https://img.shields.io/badge/UE-5.3-blue.svg)](https://www.unrealengine.com/)
[![Architecture](https://img.shields.io/badge/Architecture-Design_Patterns-green.svg)]()
[![Version](https://img.shields.io/badge/Version-1.7.0-brightgreen.svg)](https://github.com/XIYBHK/UE_XTools)

## 📋 目录

- [概述](#概述)
- [核心设计模式](#核心设计模式)
  - [1. 句柄系统模式](#1-句柄系统模式)
  - [2. 实例ID防重复模式](#2-实例id防重复模式)
  - [3. 模板方法 + 策略模式](#3-模板方法--策略模式)
  - [4. 外观模式 + 工厂模式](#4-外观模式--工厂模式)
  - [5. 配置对象模式](#5-配置对象模式)
  - [6. 弱引用所有者模式](#6-弱引用所有者模式)
- [通用架构模板](#通用架构模板)
- [实际应用示例](#实际应用示例)
- [最佳实践指南](#最佳实践指南)
- [性能考虑](#性能考虑)

## 概述

XTools 插件在开发过程中积累了大量优秀的设计模式，特别是 **EnhancedCodeFlow (ECF)** 模块展现了企业级的架构设计思想。本文档提取这些设计模式，为后续模块开发提供可复用的架构参考。

### 核心设计哲学

- **类型安全**：编译时错误检查，避免运行时问题
- **内存安全**：自动生命周期管理，防止内存泄漏
- **一致性**：统一的API设计风格
- **可扩展性**：模块化架构支持功能扩展
- **用户友好**：简洁直观的接口设计

## 核心设计模式

### 1. 句柄系统模式

#### 设计思想
句柄系统提供类型安全的资源引用，避免原始指针的风险，支持资源的安全访问和生命周期管理。

#### ECF 实现
```cpp
class ENHANCEDCODEFLOW_API FECFHandle
{
public:
    FECFHandle() : Handle(0) {}
    
    // 核心接口
    bool IsValid() const { return Handle != 0; }
    void Invalidate() { Handle = 0; }
    
    // 完整的值语义支持
    bool operator==(const FECFHandle& Other) const { return Handle == Other.Handle; }
    bool operator!=(const FECFHandle& Other) const { return Handle != Other.Handle; }
    FECFHandle& operator=(const FECFHandle& Other) { Handle = Other.Handle; return *this; }
    FECFHandle& operator=(FECFHandle&& Other) { Handle = Other.Handle; Other.Invalidate(); return *this; }
    
    // 实用功能
    FString ToString() const { return FString::Printf(TEXT("%llu"), Handle); }
    FECFHandle& operator++() { Handle++; return *this; }

protected:
    uint64 Handle;
};
```

#### 通用模板
```cpp
template<typename THandleType>
class TXToolsHandle
{
    uint64 Handle;
    
public:
    TXToolsHandle() : Handle(0) {}
    
    bool IsValid() const { return Handle != 0; }
    void Invalidate() { Handle = 0; }
    
    // 值语义支持
    bool operator==(const TXToolsHandle& Other) const { return Handle == Other.Handle; }
    bool operator!=(const TXToolsHandle& Other) const { return Handle != Other.Handle; }
    
    FString ToString() const { return FString::Printf(TEXT("%llu"), Handle); }
    
    // 类型安全：每个模块使用不同的句柄类型
    static TXToolsHandle<THandleType> NewHandle() 
    {
        static uint64 Counter = 0;
        TXToolsHandle<THandleType> NewHandle;
        NewHandle.Handle = ++Counter;
        return NewHandle;
    }
};
```

#### 应用价值
- **类型安全**：防止不同模块的句柄混用
- **资源管理**：统一的失效检查机制
- **调试友好**：句柄可转换为字符串便于调试
- **性能优化**：值类型，避免动态分配

### 2. 实例ID防重复模式

#### 设计思想
通过实例ID实现单例执行控制，防止相同功能的重复执行，特别适用于状态管理和防抖场景。

#### ECF 实现
```cpp
class ENHANCEDCODEFLOW_API FECFInstanceId
{
public:
    FECFInstanceId() : Id(0) {}
    FECFInstanceId(uint64 InId) : Id(InId) {}
    
    bool IsValid() const { return Id != 0; }
    void Invalidate() { Id = 0; }
    
    // 比较操作
    bool operator==(const FECFInstanceId& Other) const { return Id == Other.Id; }
    bool operator!=(const FECFInstanceId& Other) const { return Id != Other.Id; }
    
    // 创建新的实例ID
    static FECFInstanceId NewId();
    
    FString ToString() const { return FString::Printf(TEXT("%llu"), Id); }

private:
    uint64 Id;
    static uint64 DynamicIdCounter;
};

// 使用示例：防重复执行
FECFHandle DoOnce(const UObject* InOwner, TUniqueFunction<void()>&& InExecFunc, 
                  const FECFInstanceId& InstanceId);
```

#### 通用模板
```cpp
template<typename TInstanceType>
class TXToolsInstanceId
{
    uint64 Id;
    static uint64 Counter;
    
public:
    TXToolsInstanceId() : Id(0) {}
    explicit TXToolsInstanceId(uint64 InId) : Id(InId) {}
    
    bool IsValid() const { return Id != 0; }
    void Invalidate() { Id = 0; }
    
    bool operator==(const TXToolsInstanceId& Other) const { return Id == Other.Id; }
    bool operator!=(const TXToolsInstanceId& Other) const { return Id != Other.Id; }
    
    static TXToolsInstanceId<TInstanceType> NewId() 
    {
        return TXToolsInstanceId<TInstanceType>(++Counter);
    }
    
    // 便利的字符串构造（用于固定ID）
    static TXToolsInstanceId<TInstanceType> FromString(const FString& StringId)
    {
        return TXToolsInstanceId<TInstanceType>(GetTypeHash(StringId));
    }
};

template<typename TInstanceType>
uint64 TXToolsInstanceId<TInstanceType>::Counter = 0;
```

#### 应用场景
- **防抖动作**：防止按钮连续点击
- **状态管理**：确保状态机的唯一性
- **资源加载**：防止重复加载同一资源
- **定时器管理**：管理具名定时器

### 3. 模板方法 + 策略模式

#### 设计思想
基类定义统一的执行框架（模板方法），子类实现具体的业务逻辑（策略），同时支持配置对象调整行为。

#### ECF 实现
```cpp
UCLASS()
class ENHANCEDCODEFLOW_API UECFActionBase : public UObject
{
    GENERATED_BODY()
    
    friend class UECFSubsystem;

protected:
    // 模板方法框架：子类重写实现具体逻辑
    virtual void Init() {}
    virtual void Tick(float DeltaTime) {}
    virtual void Complete(bool bStopped) {}
    virtual void RetriggeredInstancedAction() {}
    
    // 生命周期管理
    TWeakObjectPtr<const UObject> Owner;
    FECFHandle HandleId;
    FECFInstanceId InstanceId;
    FECFActionSettings Settings;

private:
    // 统一的执行框架（模板方法）
    void DoTick(float DeltaTime)
    {
        // 暂停检查
        if (bIsPaused) return;
        
        // 游戏暂停检查
        if (!Settings.bIgnorePause && GetWorld()->IsPaused()) return;
        
        // 时间膨胀处理
        if (!Settings.bIgnoreGlobalTimeDilation) {
            DeltaTime *= GetWorld()->GetWorldSettings()->TimeDilation;
        }
        
        // 延迟处理
        if (ActionDelayLeft > 0.f) {
            ActionDelayLeft -= DeltaTime;
            return;
        }
        
        // 调用子类实现
        Tick(DeltaTime);
    }

public:
    virtual bool IsValid() const {
        return bHasFinished == false && HasValidOwner();
    }
    
    virtual bool HasValidOwner() const {
        return Owner.IsValid() && 
               (Owner->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) == false);
    }
};
```

#### 通用模板
```cpp
template<typename TActionType, typename TSettingsType = FXToolsActionSettings>
class TXToolsActionBase : public UObject
{
    GENERATED_BODY()

protected:
    // 模板方法框架
    virtual void Init() {}
    virtual void Execute() {}
    virtual void Complete(bool bStopped) {}
    virtual void OnRetrigger() {}
    
    // 通用属性
    TWeakObjectPtr<const UObject> Owner;
    TXToolsHandle<TActionType> HandleId;
    TXToolsInstanceId<TActionType> InstanceId;
    TSettingsType Settings;
    
private:
    bool bHasFinished = false;
    bool bIsPaused = false;
    
public:
    // 统一执行框架
    void DoExecute()
    {
        if (!IsValid()) return;
        
        // 应用设置和通用逻辑
        if (bIsPaused) return;
        
        // 调用子类实现
        Execute();
    }
    
    // 通用接口
    virtual bool IsValid() const {
        return !bHasFinished && HasValidOwner();
    }
    
    virtual bool HasValidOwner() const {
        return Owner.IsValid() && 
               !Owner->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
    }
    
    TXToolsHandle<TActionType> GetHandle() const { return HandleId; }
    TXToolsInstanceId<TActionType> GetInstanceId() const { return InstanceId; }
    
    void MarkAsFinished() { bHasFinished = true; }
    void SetPaused(bool bPaused) { bIsPaused = bPaused; }
};
```

#### 设计价值
- **框架统一性**：所有动作共享相同的生命周期管理
- **配置驱动**：通过Settings对象控制行为
- **易于扩展**：新增动作类型只需实现核心虚函数
- **调试友好**：统一的状态检查和错误处理

### 4. 外观模式 + 工厂模式

#### 设计思想
静态外观类隐藏子系统复杂性，提供简洁统一的API；同时充当工厂，负责创建和管理各种类型的动作对象。

#### ECF 实现
```cpp
class ENHANCEDCODEFLOW_API FEnhancedCodeFlow
{
public:
    // 工厂方法：创建各种类型的动作
    static FECFHandle Delay(const UObject* InOwner, float InDelayTime, 
                           TUniqueFunction<void()>&& InCallbackFunc, 
                           const FECFActionSettings& Settings = {});
    
    static FECFHandle AddTicker(const UObject* InOwner, 
                               TUniqueFunction<void(float)>&& InTickFunc,
                               const FECFActionSettings& Settings = {});
    
    static FECFHandle AddTimeline(const UObject* InOwner, 
                                 float InStartValue, float InStopValue, float InTime,
                                 TUniqueFunction<void(float, float)>&& InTickFunc,
                                 const FECFActionSettings& Settings = {});
    
    // 管理接口：统一的控制方法
    static bool IsActionRunning(const UObject* WorldContextObject, const FECFHandle& Handle);
    static void StopAction(const UObject* WorldContextObject, FECFHandle& Handle, bool bComplete = false);
    static void PauseAction(const UObject* WorldContextObject, const FECFHandle& Handle);
    static void ResumeAction(const UObject* WorldContextObject, const FECFHandle& Handle);
    
    // 批量管理
    static void StopAllActions(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);
    static void RemoveAllDelays(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);
    
private:
    // 内部实现：获取子系统
    static UECFSubsystem* GetSubsystem(const UObject* WorldContextObject);
};
```

#### 通用模板
```cpp
template<typename TSubsystemType, typename THandleType>
class TXToolsFacade
{
public:
    // 通用工厂方法
    template<typename TActionType, typename... Args>
    static THandleType CreateAction(const UObject* Owner, Args&&... args)
    {
        if (auto* Subsystem = GetSubsystem(Owner)) {
            return Subsystem->template AddAction<TActionType>(Owner, Forward<Args>(args)...);
        }
        return THandleType{};
    }
    
    // 通用管理接口
    static bool IsActionRunning(const UObject* WorldContextObject, const THandleType& Handle)
    {
        if (auto* Subsystem = GetSubsystem(WorldContextObject)) {
            return Subsystem->HasAction(Handle);
        }
        return false;
    }
    
    static void StopAction(const UObject* WorldContextObject, THandleType& Handle, bool bComplete = false)
    {
        if (auto* Subsystem = GetSubsystem(WorldContextObject)) {
            Subsystem->RemoveAction(Handle, bComplete);
        }
    }
    
    // 批量操作
    template<typename TActionType>
    static void StopAllActionsOfType(const UObject* WorldContextObject, bool bComplete = false, UObject* Owner = nullptr)
    {
        if (auto* Subsystem = GetSubsystem(WorldContextObject)) {
            Subsystem->template RemoveActionsOfClass<TActionType>(bComplete, Owner);
        }
    }

private:
    static TSubsystemType* GetSubsystem(const UObject* WorldContextObject)
    {
        if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)) {
            if (UGameInstance* GameInstance = World->GetGameInstance()) {
                return GameInstance->GetSubsystem<TSubsystemType>();
            }
        }
        return nullptr;
    }
};
```

#### 设计价值
- **简化客户端**：用户无需了解内部子系统架构
- **类型安全**：模板保证编译时类型检查
- **一致性**：所有模块使用相同的调用模式
- **易于维护**：内部实现变更不影响客户端代码

### 5. 配置对象模式

#### 设计思想
将复杂的参数配置聚合为配置对象，提供合理的默认值，支持灵活的参数传递和扩展。

#### ECF 实现
```cpp
USTRUCT(BlueprintType)
struct ENHANCEDCODEFLOW_API FECFActionSettings
{
    GENERATED_BODY()

    FECFActionSettings() :
        TickInterval(0.f),
        FirstDelay(0.f),
        bIgnorePause(false),
        bIgnoreGlobalTimeDilation(false),
        bStartPaused(false)
    {}

    FECFActionSettings(float InTickInterval, float InFirstDelay = 0.f, 
                      bool InIgnorePause = false, bool InIgnoreTimeDilation = false, 
                      bool InStartPaused = false) :
        TickInterval(InTickInterval),
        FirstDelay(InFirstDelay),
        bIgnorePause(InIgnorePause),
        bIgnoreGlobalTimeDilation(InIgnoreTimeDilation),
        bStartPaused(InStartPaused)
    {}

    UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置")
    float TickInterval = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置")
    float FirstDelay = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置")
    bool bIgnorePause = false;

    UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置")
    bool bIgnoreGlobalTimeDilation = false;

    UPROPERTY(BlueprintReadWrite, Category = "XTools|ECF|设置")
    bool bStartPaused = false;
};

// 便利宏定义
#define ECF_TICKINTERVAL(_Interval) FECFActionSettings(_Interval, 0.f, false, false, false)
#define ECF_DELAYFIRST(_Delay) FECFActionSettings(0.f, _Delay, false, false, false)
#define ECF_IGNOREPAUSE FECFActionSettings(0.f, 0.f, true, false, false)
#define ECF_IGNORETIMEDILATION FECFActionSettings(0.f, 0.f, false, true, false)
#define ECF_STARTPAUSED FECFActionSettings(0.f, 0.f, false, false, true)
```

#### 通用模板
```cpp
template<typename TModuleType>
struct TXToolsActionSettings
{
    // 通用配置项
    float TickInterval = 0.f;
    float FirstDelay = 0.f;
    bool bIgnorePause = false;
    bool bIgnoreGlobalTimeDilation = false;
    bool bStartPaused = false;
    
    // 模块特定配置（通过特化扩展）
    typename TModuleType::ConfigType ModuleConfig;
    
    // 便利构造函数
    TXToolsActionSettings() = default;
    
    TXToolsActionSettings(float InTickInterval, float InFirstDelay = 0.f,
                         bool InIgnorePause = false, bool InIgnoreTimeDilation = false,
                         bool InStartPaused = false) :
        TickInterval(InTickInterval),
        FirstDelay(InFirstDelay),
        bIgnorePause(InIgnorePause),
        bIgnoreGlobalTimeDilation(InIgnoreTimeDilation),
        bStartPaused(InStartPaused)
    {}
    
    // 流式配置接口
    TXToolsActionSettings& SetTickInterval(float Interval) { TickInterval = Interval; return *this; }
    TXToolsActionSettings& SetFirstDelay(float Delay) { FirstDelay = Delay; return *this; }
    TXToolsActionSettings& IgnorePause(bool bIgnore = true) { bIgnorePause = bIgnore; return *this; }
    TXToolsActionSettings& IgnoreTimeDilation(bool bIgnore = true) { bIgnoreGlobalTimeDilation = bIgnore; return *this; }
    TXToolsActionSettings& StartPaused(bool bPaused = true) { bStartPaused = bPaused; return *this; }
};

// 便利宏生成器
#define XTOOLS_SETTINGS_MACRO(ModuleName) \
    using F##ModuleName##Settings = TXToolsActionSettings<F##ModuleName##Module>; \
    static F##ModuleName##Settings ModuleName##_TickInterval(float Interval) { return F##ModuleName##Settings{}.SetTickInterval(Interval); } \
    static F##ModuleName##Settings ModuleName##_FirstDelay(float Delay) { return F##ModuleName##Settings{}.SetFirstDelay(Delay); } \
    static F##ModuleName##Settings ModuleName##_IgnorePause() { return F##ModuleName##Settings{}.IgnorePause(); }
```

#### 设计价值
- **参数聚合**：避免函数参数过多的问题
- **默认值管理**：提供合理的默认配置
- **扩展性**：新增配置项不影响现有代码
- **易用性**：支持流式配置和便利宏

### 6. 弱引用所有者模式

#### 设计思想
使用弱引用管理对象所有权，实现自动清理机制，避免循环依赖和内存泄漏。

#### ECF 实现
```cpp
class UECFActionBase : public UObject
{
protected:
    // 弱引用所有者，避免循环依赖
    UPROPERTY(Transient)
    TWeakObjectPtr<const UObject> Owner;

public:
    // 所有者有效性检查
    virtual bool HasValidOwner() const 
    {
        return Owner.IsValid() && 
               (Owner->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) == false);
    }
    
    // 动作有效性依赖于所有者
    virtual bool IsValid() const 
    {
        return bHasFinished == false && HasValidOwner();
    }
    
    // 设置所有者
    void SetAction(const UObject* InOwner, const FECFHandle& InHandleId, 
                  const FECFInstanceId& InInstanceId, const FECFActionSettings& InSettings)
    {
        Owner = InOwner;  // 自动转换为弱引用
        HandleId = InHandleId;
        InstanceId = InInstanceId;
        Settings = InSettings;
        // ... 其他初始化
    }
};
```

#### 通用模板
```cpp
template<typename TOwnerType = UObject>
class TXToolsOwnershipMixin
{
protected:
    TWeakObjectPtr<const TOwnerType> Owner;
    
public:
    // 设置所有者
    void SetOwner(const TOwnerType* InOwner) 
    {
        Owner = InOwner;
    }
    
    // 获取所有者（可能为空）
    const TOwnerType* GetOwner() const 
    {
        return Owner.Get();
    }
    
    // 所有者有效性检查
    virtual bool HasValidOwner() const 
    {
        return Owner.IsValid() && 
               !Owner->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
    }
    
    // 检查特定所有者
    bool IsOwnedBy(const TOwnerType* TestOwner) const 
    {
        return Owner.IsValid() && Owner.Get() == TestOwner;
    }
    
    // 所有者销毁时的回调
    virtual void OnOwnerDestroyed() {}
};

// 自动清理的资源管理器
template<typename TResourceType, typename TOwnerType = UObject>
class TXToolsAutoCleanupResource : public TXToolsOwnershipMixin<TOwnerType>
{
    TArray<TResourceType*> Resources;
    
public:
    void AddResource(TResourceType* Resource) 
    {
        Resources.AddUnique(Resource);
    }
    
    void RemoveResource(TResourceType* Resource) 
    {
        Resources.Remove(Resource);
    }
    
    void CleanupInvalidResources() 
    {
        Resources.RemoveAll([](TResourceType* Resource) {
            return !IsValid(Resource);
        });
    }
    
    // 所有者销毁时自动清理所有资源
    virtual void OnOwnerDestroyed() override 
    {
        for (TResourceType* Resource : Resources) {
            if (IsValid(Resource)) {
                Resource->MarkPendingKill();
            }
        }
        Resources.Empty();
    }
};
```

#### 设计价值
- **自动清理**：所有者销毁时相关资源自动清理
- **内存安全**：避免悬挂指针和内存泄漏
- **循环依赖预防**：弱引用打破潜在的循环引用
- **生命周期绑定**：资源与所有者同生共死

## 通用架构模板

### 完整的模块架构模板

```cpp
// 1. 模块特定的类型定义
namespace XToolsSort 
{
    struct FSortModule {};
    using FSortHandle = TXToolsHandle<FSortModule>;
    using FSortInstanceId = TXToolsInstanceId<FSortModule>;
    using FSortSettings = TXToolsActionSettings<FSortModule>;
    
    // 模块特定配置
    template<>
    struct FSortModule::ConfigType 
    {
        bool bStableSort = true;
        bool bParallelSort = false;
        int32 ParallelThreshold = 1000;
    };
}

// 2. 动作基类
UCLASS()
class USortActionBase : public TXToolsActionBase<XToolsSort::FSortModule, XToolsSort::FSortSettings>
{
    GENERATED_BODY()
    
protected:
    // 子类实现具体排序逻辑
    virtual void Execute() override {}
    virtual void Complete(bool bStopped) override {}
};

// 3. 子系统管理器
UCLASS()
class USortSubsystem : public TXToolsSubsystem<USortActionBase, XToolsSort::FSortHandle>
{
    GENERATED_BODY()
    
public:
    // 模块特定的功能
    template<typename T>
    XToolsSort::FSortHandle SortArrayAsync(const UObject* Owner, TArray<T>& Array,
                                          TFunction<bool(const T&, const T&)> Predicate,
                                          const XToolsSort::FSortSettings& Settings = {});
};

// 4. 外观接口
class FSortFlow : public TXToolsFacade<USortSubsystem, XToolsSort::FSortHandle>
{
public:
    // 便利的静态接口
    template<typename T>
    static XToolsSort::FSortHandle SortArray(const UObject* Owner, TArray<T>& Array,
                                            TFunction<bool(const T&, const T&)> Predicate,
                                            const XToolsSort::FSortSettings& Settings = {})
    {
        return CreateAction<USortArrayAction>(Owner, Array, Predicate, Settings);
    }
    
    // 预定义的常用排序
    static XToolsSort::FSortHandle SortIntArray(const UObject* Owner, TArray<int32>& Array, bool bAscending = true);
    static XToolsSort::FSortHandle SortFloatArray(const UObject* Owner, TArray<float>& Array, bool bAscending = true);
};
```

### 使用示例

```cpp
// 客户端使用
void AMyActor::BeginPlay()
{
    Super::BeginPlay();
    
    // 简单排序
    TArray<int32> Numbers = {3, 1, 4, 1, 5, 9, 2, 6};
    auto SortHandle = FSortFlow::SortIntArray(this, Numbers);
    
    // 自定义排序
    TArray<FMyStruct> Structs = GetStructArray();
    auto CustomSortHandle = FSortFlow::SortArray(this, Structs,
        [](const FMyStruct& A, const FMyStruct& B) {
            return A.Priority > B.Priority;
        },
        XToolsSort::FSortSettings{}
            .SetTickInterval(0.016f)  // 60FPS
            .IgnorePause()
            .SetFirstDelay(1.0f)
    );
    
    // 检查排序状态
    if (FSortFlow::IsActionRunning(this, SortHandle)) {
        UE_LOG(LogTemp, Log, TEXT("Sorting in progress..."));
    }
}

void AMyActor::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    // 自动清理：所有相关动作会因为Owner销毁而自动停止
    Super::EndPlay(EndPlayReason);
}
```

## 实际应用示例

### Sort 模块应用

```cpp
namespace XToolsSort {
    // 长时间排序任务的句柄管理
    class FSortHandle : public TXToolsHandle<FSortHandle> {};
    
    // 排序动作实现
    class USortArrayAction : public TXToolsActionBase<USortArrayAction>
    {
    protected:
        virtual void Execute() override {
            // 执行排序逻辑，支持分帧处理
            PerformIncrementalSort();
        }
    };
    
    // 外观接口
    class FSortFlow {
    public:
        static FSortHandle SortLargeArrayAsync(const UObject* Owner, 
                                             TArray<int32>& Array,
                                             TFunction<void(bool)>&& Callback);
    };
}
```

### RandomShuffles 模块应用

```cpp
namespace XToolsRandom {
    // 使用实例ID防止重复随机
    class FRandomInstanceId : public TXToolsInstanceId<FRandomInstanceId> {};
    
    // PRD状态管理
    class FPRDFlow {
    public:
        static bool PseudoRandomBool(const UObject* Owner, 
                                   float Probability, 
                                   const FRandomInstanceId& InstanceId) {
            // 基于InstanceId管理PRD状态
            return GetOrCreatePRDState(InstanceId).TestProbability(Probability);
        }
        
        static void ResetPRDState(const FRandomInstanceId& InstanceId) {
            PRDStates.Remove(InstanceId);
        }
    };
}
```

### FormationSystem 模块应用

```cpp
namespace XToolsFormation {
    // 阵型计算任务管理
    class FFormationHandle : public TXToolsHandle<FFormationHandle> {};
    class FFormationInstanceId : public TXToolsInstanceId<FFormationInstanceId> {};
    
    // 阵型计算动作
    class UFormationCalculationAction : public TXToolsActionBase<UFormationCalculationAction>
    {
        TArray<AActor*> Units;
        FFormationPattern Pattern;
        
    protected:
        virtual void Execute() override {
            // 分帧计算阵型位置
            CalculateFormationPositions();
        }
    };
    
    // 外观接口
    class FFormationFlow {
    public:
        static FFormationHandle CalculateFormation(const UObject* Owner,
                                                  const TArray<AActor*>& Units,
                                                  const FFormationPattern& Pattern,
                                                  const FFormationInstanceId& InstanceId);
    };
}
```

## 最佳实践指南

### 1. 模块设计原则

#### 命名规范
```cpp
// 模块命名空间
namespace XToolsModuleName {
    // 模块标识结构体
    struct FModuleNameModule {};
    
    // 类型别名
    using FModuleHandle = TXToolsHandle<FModuleNameModule>;
    using FModuleInstanceId = TXToolsInstanceId<FModuleNameModule>;
    using FModuleSettings = TXToolsActionSettings<FModuleNameModule>;
}

// 类命名
class UModuleNameActionBase;      // 动作基类
class UModuleNameSubsystem;       // 子系统
class FModuleNameFlow;           // 外观接口
```

#### 文件组织
```
Source/ModuleName/
├── Public/
│   ├── ModuleNameCore.h         // 核心类型定义
│   ├── ModuleNameActions.h      // 动作类声明
│   ├── ModuleNameSubsystem.h    // 子系统声明
│   └── ModuleNameFlow.h         // 外观接口
├── Private/
│   ├── ModuleNameActions.cpp    // 动作实现
│   ├── ModuleNameSubsystem.cpp  // 子系统实现
│   └── ModuleNameFlow.cpp       // 外观实现
└── ModuleName.Build.cs
```

### 2. 错误处理和调试

#### 统一的错误处理
```cpp
class TXToolsActionBase {
protected:
    // 统一的错误检查宏
    #define XTOOLS_CHECK_VALID_OWNER() \
        if (!HasValidOwner()) { \
            UE_LOG(LogXTools, Warning, TEXT("Action owner is invalid: %s"), *GetClass()->GetName()); \
            MarkAsFinished(); \
            return; \
        }
    
    #define XTOOLS_CHECK_VALID_HANDLE(Handle) \
        if (!Handle.IsValid()) { \
            UE_LOG(LogXTools, Warning, TEXT("Invalid handle in %s"), *FString(__FUNCTION__)); \
            return; \
        }
};
```

#### 调试友好的设计
```cpp
class FXToolsHandle {
public:
    // 调试信息
    FString GetDebugString() const {
        return FString::Printf(TEXT("Handle_%llu"), Handle);
    }
    
    // 便于断点调试
    void DebugBreakIfInvalid() const {
        if (!IsValid()) {
            UE_DEBUG_BREAK();
        }
    }
};
```

### 3. 性能优化指南

#### 对象池集成
```cpp
template<typename TActionType>
class TXToolsActionPool {
    TArray<TActionType*> AvailableActions;
    TArray<TActionType*> ActiveActions;
    
public:
    TActionType* AcquireAction(UObject* Outer) {
        if (AvailableActions.Num() > 0) {
            return AvailableActions.Pop();
        }
        return NewObject<TActionType>(Outer);
    }
    
    void ReleaseAction(TActionType* Action) {
        Action->Reset();  // 重置状态
        AvailableActions.Push(Action);
    }
};
```

#### 内存使用监控
```cpp
class FXToolsMemoryTracker {
    static int32 ActiveActionCount;
    static int32 PeakActionCount;
    
public:
    static void OnActionCreated() {
        ActiveActionCount++;
        PeakActionCount = FMath::Max(PeakActionCount, ActiveActionCount);
    }
    
    static void OnActionDestroyed() {
        ActiveActionCount--;
    }
    
    static void LogMemoryStats() {
        UE_LOG(LogXTools, Log, TEXT("Actions: %d active, %d peak"), 
               ActiveActionCount, PeakActionCount);
    }
};
```

### 4. 蓝图集成

#### 蓝图友好的设计
```cpp
// 蓝图包装器
UCLASS(BlueprintType, Blueprintable)
class UXToolsFlowBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "XTools|Flow", 
              meta = (DisplayName = "创建延迟动作"))
    static FXToolsHandle CreateDelay(const UObject* Owner, float DelayTime);
    
    UFUNCTION(BlueprintCallable, Category = "XTools|Flow",
              meta = (DisplayName = "检查动作是否运行中"))
    static bool IsActionRunning(const UObject* WorldContext, FXToolsHandle Handle);
    
    UFUNCTION(BlueprintCallable, Category = "XTools|Flow",
              meta = (DisplayName = "停止动作"))
    static void StopAction(const UObject* WorldContext, UPARAM(ref) FXToolsHandle& Handle);
};
```

## 性能考虑

### 1. 内存管理

#### 对象生命周期
- **UObject动作**：由子系统管理，自动垃圾回收
- **句柄对象**：值类型，栈分配，无GC压力
- **配置对象**：轻量级结构体，支持移动语义

#### 内存优化策略
```cpp
class UXToolsSubsystem {
    // 使用对象池减少分配
    TMap<UClass*, TArray<UXToolsActionBase*>> ActionPools;
    
    // 批量处理减少单次开销
    TArray<UXToolsActionBase*> PendingRemovalActions;
    
    void BatchRemoveActions() {
        for (auto* Action : PendingRemovalActions) {
            ReturnToPool(Action);
        }
        PendingRemovalActions.Empty();
    }
};
```

### 2. 执行效率

#### Tick 优化
```cpp
class UXToolsSubsystem {
    // 分优先级的Tick队列
    TArray<UXToolsActionBase*> HighPriorityActions;
    TArray<UXToolsActionBase*> NormalPriorityActions;
    TArray<UXToolsActionBase*> LowPriorityActions;
    
    virtual void Tick(float DeltaTime) override {
        // 高优先级每帧执行
        for (auto* Action : HighPriorityActions) {
            Action->DoTick(DeltaTime);
        }
        
        // 低优先级分帧执行
        static int32 LowPriorityIndex = 0;
        if (LowPriorityActions.IsValidIndex(LowPriorityIndex)) {
            LowPriorityActions[LowPriorityIndex]->DoTick(DeltaTime);
            LowPriorityIndex = (LowPriorityIndex + 1) % LowPriorityActions.Num();
        }
    }
};
```

### 3. 并发安全

#### 线程安全的句柄管理
```cpp
class FXToolsThreadSafeHandle {
    std::atomic<uint64> Handle;
    
public:
    bool IsValid() const {
        return Handle.load() != 0;
    }
    
    void Invalidate() {
        Handle.store(0);
    }
    
    bool CompareAndInvalidate(uint64 ExpectedValue) {
        return Handle.compare_exchange_strong(ExpectedValue, 0);
    }
};
```

## 总结

XTools 插件中的设计模式体现了企业级软件开发的最佳实践：

1. **类型安全优先**：通过模板和强类型避免运行时错误
2. **自动资源管理**：弱引用和RAII确保内存安全
3. **统一架构风格**：模板化的通用框架保证一致性
4. **性能友好设计**：对象池、分帧处理等优化策略
5. **易于扩展维护**：清晰的分层架构和模块化设计

这些模式不仅适用于 XTools 内部模块，也可以作为 UE5 插件开发的通用指南，为构建高质量的游戏工具奠定坚实基础。

---

**© 2024 XTools Development Team. All rights reserved.**