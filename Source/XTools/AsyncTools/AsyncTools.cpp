#include "AsyncTools.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Curves/CurveFloat.h"

DEFINE_LOG_CATEGORY_STATIC(LogAsyncTools, Log, All);

UAsyncTools::UAsyncTools(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 引用所有委托以避免"未使用"警告
	// 这些引用不会执行任何操作，仅用于告诉编译器这些委托被使用
	void* Dummy = &OnStartDelegate;
	Dummy = &OnUpdateDelegate;
	Dummy = &OnCompleteDelegate;
	Dummy = &OnProgressDelegate;
}

UAsyncTools::~UAsyncTools()
{
	// 🚀 确保所有资源都已清理
	if (TimerHandle.IsValid())
	{
		UE_LOG(LogAsyncTools, Warning, TEXT("AsyncTools 析构时定时器仍然有效，强制清理"));
		if (UWorld* ValidWorld = WorldWeak.Get())
		{
			ValidWorld->GetTimerManager().ClearTimer(TimerHandle);
		}
	}

	UE_LOG(LogAsyncTools, Verbose, TEXT("AsyncTools 实例被销毁 (ID: %p)"), this);
}

// 🚀 UObject 生命周期管理
void UAsyncTools::BeginDestroy()
{
	// 标记正在销毁
	StateManager.bIsBeingDestroyed.Store(true);

	// 🚀 安全清理定时器
	if (TimerHandle.IsValid())
	{
		if (UWorld* ValidWorld = WorldWeak.Get())
		{
			ValidWorld->GetTimerManager().ClearTimer(TimerHandle);
		}
		TimerHandle.Invalidate();
	}

	// 🚀 清理弱引用
	WorldContextWeak.Reset();
	WorldWeak.Reset();
	CurveFloatWeak.Reset();

	Super::BeginDestroy();
}

bool UAsyncTools::IsReadyForFinishDestroy()
{
	// 🚀 确保定时器已完全清理
	return !TimerHandle.IsValid() && Super::IsReadyForFinishDestroy();
}

/**
 * 创建一个新的异步操作实例
 * @param WorldContext - 世界上下文对象
 * @param Duration - 异步操作的总时长
 * @param StartValueA - 异步操作的起始值A
 * @param EndValueB - 异步操作的结束值B
 * @param Curve - 用于插值的曲线资源
 * @param TickInterval - 更新曲线的时间步长
 * @param StartDelay - 开始曲线前的延迟
 * @param OutAsyncRef - 创建的异步操作的引用
 * @return 新的异步操作实例
 */
UAsyncTools* UAsyncTools::AsyncAction(
		UObject* WorldContext,
		float Duration,
		float StartValueA,
		float EndValueB,
		UCurveFloat* Curve,
		float TickInterval,
		float StartDelay,
		UAsyncTools*& OutAsyncRef)
{
	if (!WorldContext)
	{
		HandleStaticAsyncError(EAsyncToolsErrorType::WorldContextInvalid, TEXT("WorldContext为空"), TEXT("AsyncAction"));
		return nullptr;
	}

	if (TickInterval <= 0.0f)
	{
		HandleStaticAsyncError(EAsyncToolsErrorType::InvalidParameter, FString::Printf(TEXT("无效的TickInterval: %f (必须为正数)"), TickInterval), TEXT("AsyncAction"));
		return nullptr;
	}

	if (Duration <= 0.0f)
	{
		HandleStaticAsyncError(EAsyncToolsErrorType::InvalidParameter, FString::Printf(TEXT("无效的Duration: %f (必须为正数)"), Duration), TEXT("AsyncAction"));
		return nullptr;
	}

	UAsyncTools* Action = NewObject<UAsyncTools>();

	// 🚀 使用弱引用避免循环引用
	Action->WorldContextWeak = WorldContext;
	Action->Time = Duration;
	Action->DeltaSeconds = TickInterval;
	Action->FirstDelay = StartDelay;
	Action->AValue = StartValueA;
	Action->BValue = EndValueB;
	Action->CurveFloatWeak = Curve;

	OutAsyncRef = Action;
	
	Action->RegisterWithGameInstance(WorldContext);

	return Action;
}

// 实现无输出引用的版本
UAsyncTools* UAsyncTools::AsyncAction(
	UObject* WorldContext,
	float Duration,
	float StartValueA,
	float EndValueB,
	UCurveFloat* Curve,
	float TickInterval,
	float StartDelay)
{
	UAsyncTools* OutRef = nullptr;
	return AsyncAction(WorldContext, Duration, StartValueA, EndValueB, Curve, TickInterval, StartDelay, OutRef);
}

void UAsyncTools::Activate()
{
	Super::Activate();

	// 🚀 检查是否正在销毁
	if (StateManager.bIsBeingDestroyed.Load())
	{
		HandleAsyncError(EAsyncToolsErrorType::StateError, TEXT("对象正在销毁，无法激活"), TEXT("Activate"));
		return;
	}

	// 🚀 使用弱引用获取 WorldContext
	UObject* ValidWorldContext = WorldContextWeak.Get();
	if (!ValidWorldContext)
	{
		HandleAsyncError(EAsyncToolsErrorType::WorldContextInvalid, TEXT("WorldContext在Activate时为空或已被销毁"), TEXT("Activate"));
		return;
	}

	UWorld* ValidWorld = GEngine->GetWorldFromContextObject(ValidWorldContext, EGetWorldErrorMode::ReturnNull);
	if (!ValidWorld)
	{
		HandleAsyncError(EAsyncToolsErrorType::WorldContextInvalid, TEXT("无法从WorldContext获取有效的World对象"), TEXT("Activate"));
		return;
	}

	// 🚀 安全设置弱引用
	WorldWeak = ValidWorld;

	// 🚀 验证参数有效性
	if (Time <= 0.0f)
	{
		HandleAsyncError(EAsyncToolsErrorType::InvalidParameter, FString::Printf(TEXT("无效的Duration: %f"), Time), TEXT("Activate"));
		return;
	}

	if (DeltaSeconds <= 0.0f)
	{
		HandleAsyncError(EAsyncToolsErrorType::InvalidParameter, FString::Printf(TEXT("无效的TickInterval: %f"), DeltaSeconds), TEXT("Activate"));
		return;
	}

	// 🚀 安全的曲线值获取
	float InitialCurveValue = 0.0f;
	UCurveFloat* ValidCurve = CurveFloatWeak.Get();
	if (ValidCurve && ValidCurve->IsValidLowLevel())
	{
		InitialCurveValue = ValidCurve->GetFloatValue(0.0f);
	}
	else if (CurveFloatWeak.IsValid())
	{
		HandleAsyncError(EAsyncToolsErrorType::CurveError, TEXT("曲线对象无效或已被销毁"), TEXT("Activate"));
		CurveFloatWeak.Reset(); // 清除无效曲线，使用线性插值
	}

	LastTime = 0.0f;
	OnStartDelegate.Broadcast(0.0f, InitialCurveValue, AValue, BValue);

	// 🚀 安全的定时器设置
	ValidWorld->GetTimerManager().SetTimer(TimerHandle, this, &UAsyncTools::OnUpdate, DeltaSeconds, true, FirstDelay);

	// 🚀 验证定时器是否成功设置
	if (!TimerHandle.IsValid())
	{
		HandleAsyncError(EAsyncToolsErrorType::TimerError, TEXT("定时器设置失败"), TEXT("Activate"));
	}
}

void UAsyncTools::Pause()
{
	// 🚀 检查销毁状态
	if (StateManager.bIsBeingDestroyed.Load())
	{
		return;
	}

	// 🚀 线程安全的暂停操作
	UWorld* ValidWorld = WorldWeak.Get();
	if (!ValidWorld)
	{
		HandleAsyncError(EAsyncToolsErrorType::WorldContextInvalid, TEXT("World对象无效或已被销毁"), TEXT("Pause"));
		return;
	}

	bool bExpectedPaused = false;
	if (StateManager.bPaused.CompareExchange(bExpectedPaused, true))
	{
		// 成功从 false 切换到 true
		ValidWorld->GetTimerManager().PauseTimer(TimerHandle);
		UE_LOG(LogAsyncTools, Log, TEXT("异步操作暂停"));
	}
	else
	{
		UE_LOG(LogAsyncTools, Warning, TEXT("异步操作已经处于暂停状态"));
	}
}

void UAsyncTools::Resume()
{
	// 🚀 检查销毁状态
	if (StateManager.bIsBeingDestroyed.Load())
	{
		return;
	}

	// 🚀 线程安全的恢复操作
	UWorld* ValidWorld = WorldWeak.Get();
	if (!ValidWorld)
	{
		HandleAsyncError(EAsyncToolsErrorType::WorldContextInvalid, TEXT("World对象无效或已被销毁"), TEXT("Resume"));
		return;
	}

	bool bExpectedPaused = true;
	if (StateManager.bPaused.CompareExchange(bExpectedPaused, false))
	{
		// 成功从 true 切换到 false
		ValidWorld->GetTimerManager().UnPauseTimer(TimerHandle);
		UE_LOG(LogAsyncTools, Log, TEXT("异步操作恢复"));
	}
	else
	{
		UE_LOG(LogAsyncTools, Warning, TEXT("异步操作未处于暂停状态"));
	}
}

void UAsyncTools::Cancel()
{
	// 🚀 检查销毁状态
	if (StateManager.bIsBeingDestroyed.Load())
	{
		return;
	}

	// 🚀 线程安全的取消操作
	UWorld* ValidWorld = WorldWeak.Get();
	if (!ValidWorld)
	{
		HandleAsyncError(EAsyncToolsErrorType::WorldContextInvalid, TEXT("World对象无效或已被销毁"), TEXT("Cancel"));
		return;
	}

	bool bExpectedCancelled = false;
	if (StateManager.bCancelled.CompareExchange(bExpectedCancelled, true))
	{
		// 成功从 false 切换到 true
		ValidWorld->GetTimerManager().ClearTimer(TimerHandle);
		SetReadyToDestroy();
		UE_LOG(LogAsyncTools, Log, TEXT("异步操作取消"));
	}
	else
	{
		UE_LOG(LogAsyncTools, Warning, TEXT("异步操作已经被取消"));
	}
}

void UAsyncTools::SetLoop(bool bInLoop)
{
	// 🚀 线程安全的循环设置
	StateManager.bLoop.Store(bInLoop);
	UE_LOG(LogAsyncTools, Log, TEXT("设置循环: %s"), bInLoop ? TEXT("true") : TEXT("false"));
}

// 🚀 实例级错误处理 - 支持 Blueprint 错误回调
void UAsyncTools::HandleAsyncError(EAsyncToolsErrorType ErrorType, const FString& ErrorMessage, const FString& Context)
{
	// 记录错误日志
	UE_LOG(LogAsyncTools, Error, TEXT("AsyncTools Error in %s: [%s] %s"), *Context, *UEnum::GetValueAsString(ErrorType), *ErrorMessage);

	// 触发错误委托，让 Blueprint 用户能够处理错误
	OnErrorDelegate.Broadcast(ErrorType, ErrorMessage, Context);

	// 根据错误类型决定是否需要取消操作
	switch (ErrorType)
	{
		case EAsyncToolsErrorType::WorldContextInvalid:
		case EAsyncToolsErrorType::TimerError:
		case EAsyncToolsErrorType::StateError:
			// 严重错误，自动取消操作
			Cancel();
			break;
		case EAsyncToolsErrorType::InvalidParameter:
		case EAsyncToolsErrorType::CurveError:
			// 参数错误，记录但继续执行
			break;
	}
}

// 🚀 静态错误处理 - 用于静态函数调用
void UAsyncTools::HandleStaticAsyncError(EAsyncToolsErrorType ErrorType, const FString& ErrorMessage, const FString& Context)
{
	// 静态函数只能记录日志，无法触发实例委托
	UE_LOG(LogAsyncTools, Error, TEXT("AsyncTools Static Error in %s: [%s] %s"), *Context, *UEnum::GetValueAsString(ErrorType), *ErrorMessage);
}

void UAsyncTools::PrintDebugInfo(bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration) const
{
	// 使用非常大的负值作为基础键，以确保我们的消息显示在最顶部
	// 使用INT32_MIN + 一个偏移量，这样我们的消息会显示在最顶部
	const int32 BaseKey = INT32_MIN + 1000;
	
	const float Progress = FMath::IsNearlyZero(Time) ? 0.0f : LastTime / Time;
	const TCHAR* LoopStatus = StateManager.bLoop.Load() ? TEXT("是") : TEXT("否");
	const TCHAR* PausedStatus = StateManager.bPaused.Load() ? TEXT("是") : TEXT("否");
	const TCHAR* CancelledStatus = StateManager.bCancelled.Load() ? TEXT("是") : TEXT("否");

	// 构建一个完整的调试信息字符串
	FString FullDebugInfo;
	
	FullDebugInfo = TEXT("===== AsyncTools 调试信息 =====\n");
	FullDebugInfo += FString::Printf(TEXT("总时长: %.2f\n"), Time);
	FullDebugInfo += FString::Printf(TEXT("已过时间: %.2f\n"), LastTime);
	FullDebugInfo += FString::Printf(TEXT("更新间隔: %.2f\n"), DeltaSeconds);
	FullDebugInfo += FString::Printf(TEXT("进度: %.2f\n"), Progress);
	FullDebugInfo += FString::Printf(TEXT("起始值A: %.2f\n"), AValue);
	FullDebugInfo += FString::Printf(TEXT("结束值B: %.2f\n"), BValue);
	FullDebugInfo += FString::Printf(TEXT("循环: %s\n"), LoopStatus);
	FullDebugInfo += FString::Printf(TEXT("暂停: %s\n"), PausedStatus);
	FullDebugInfo += FString::Printf(TEXT("取消: %s"), CancelledStatus);

	if (bPrintToScreen && GEngine)
	{
		const FColor DisplayColor = TextColor.ToFColor(true);
		
		// 设置超长的显示时间，使消息实际上永久显示
		const float PermanentDuration = 2.0f; 
		
		// 只使用一个键值显示所有信息，这样就只有一条消息
		// 使用最小的负整数作为键，确保它显示在所有其他消息之上
		GEngine->AddOnScreenDebugMessage(BaseKey, PermanentDuration, DisplayColor, FullDebugInfo);
	}

	// 只有在明确要求时才输出到日志
	if (bPrintToLog)
	{
		// 使用单个日志条目，而不是多个条目
		UE_LOG(LogAsyncTools, Log, TEXT("\n%s"), *FullDebugInfo);
	}
}

void UAsyncTools::OnUpdate()
{
	// 🚀 性能统计
	++UpdateCallCount;

	// 🚀 使用状态管理器进行高效检查
	if (!StateManager.IsActive())
	{
		return;
	}

	// 🚀 线程安全的状态更新
	float CurrentProgress, CurrentA, CurrentB;
	bool bShouldComplete = false;
	bool bShouldLoop = false;

	{
		FScopeLock Lock(&StateLock);

		LastTime += DeltaSeconds;
		CurrentProgress = FMath::Clamp(LastTime / Time, 0.0f, 1.0f);
		CurrentA = AValue;
		CurrentB = BValue;

		// 检查是否完成
		bShouldComplete = (LastTime >= Time);
		bShouldLoop = StateManager.bLoop.Load();

		// 如果循环且完成，重置时间并使缓存失效
		if (bShouldComplete && bShouldLoop)
		{
			LastTime = 0.0f;
			PerformanceCache.Invalidate();
		}
	}

	// 🚀 性能优化的曲线值计算
	const float CurrentCurveValue = CalculateCurveValueOptimized(CurrentProgress);

	// 触发更新委托
	OnUpdateDelegate.Broadcast(CurrentProgress, CurrentCurveValue, CurrentA, CurrentB);
	OnProgressDelegate.Broadcast(CurrentProgress, CurrentCurveValue, CurrentA, CurrentB);

	// 处理完成逻辑
	if (bShouldComplete)
	{
		OnCompleteDelegate.Broadcast(1.0f, CurrentCurveValue, CurrentA, CurrentB);

		if (!bShouldLoop)
		{
			Cancel();
		}
	}
}

void UAsyncTools::SetTimeScale(float InTimeScale)
{
	UWorld* ValidWorld = WorldWeak.Get();
	if (!ValidWorld)
	{
		HandleAsyncError(EAsyncToolsErrorType::WorldContextInvalid, TEXT("World对象无效或已被销毁"), TEXT("SetTimeScale"));
		return;
	}

	if (!TimerHandle.IsValid())
	{
		HandleAsyncError(EAsyncToolsErrorType::TimerError, TEXT("定时器句柄无效"), TEXT("SetTimeScale"));
		return;
	}

	// 🚀 使用配置常量进行时间缩放设置
	const float ClampedTimeScale = FMath::Max(AsyncToolsConfig::MinTimeScale, InTimeScale);
	StateManager.TimeScale.Store(ClampedTimeScale);

	// 🚀 线程安全的定时器更新
	float CurrentDeltaSeconds;
	{
		FScopeLock Lock(&StateLock);
		CurrentDeltaSeconds = DeltaSeconds;
	}

	const float NewTickInterval = CurrentDeltaSeconds / ClampedTimeScale;
	ValidWorld->GetTimerManager().SetTimer(TimerHandle, this, &UAsyncTools::OnUpdate, NewTickInterval, true);

	// 🚀 验证定时器是否成功重新设置
	if (!TimerHandle.IsValid())
	{
		HandleAsyncError(EAsyncToolsErrorType::TimerError, TEXT("定时器重新设置失败"), TEXT("SetTimeScale"));
		return;
	}

	UE_LOG(LogAsyncTools, Log, TEXT("时间缩放设置为: %.2f (新的更新间隔: %.4f)"), ClampedTimeScale, NewTickInterval);
}

void UAsyncTools::UpdateCurveParams(float InA, float InB)
{
	// 🚀 线程安全的参数更新
	{
		FScopeLock Lock(&StateLock);
		AValue = InA;
		BValue = InB;
	}
	UE_LOG(LogAsyncTools, Log, TEXT("曲线参数已更新: A=%.2f, B=%.2f"), InA, InB);
}

// 🚀 线程安全的进度获取
float UAsyncTools::GetProgress() const
{
	FScopeLock Lock(&StateLock);
	return FMath::IsNearlyZero(Time) ? 0.0f : FMath::Clamp(LastTime / Time, 0.0f, 1.0f);
}

// 🚀 性能优化的曲线值计算
float UAsyncTools::CalculateCurveValueOptimized(float Progress) const
{
	UCurveFloat* ValidCurve = CurveFloatWeak.Get();
	if (!ValidCurve || !ValidCurve->IsValidLowLevel())
	{
		return Progress; // 线性插值
	}

	// 🚀 使用改进的缓存检查
	if (PerformanceCache.IsValidForProgress(Progress))
	{
		++CacheHitCount;
		return PerformanceCache.CachedCurveValue;
	}

	// 计算新的曲线值
	const float CalculatedCurveValue = ValidCurve->GetFloatValue(Progress);

	// 更新缓存
	PerformanceCache.CachedProgress = Progress;
	PerformanceCache.CachedCurveValue = CalculatedCurveValue;
	PerformanceCache.bCacheValid = true;

	return CalculatedCurveValue;
}

// 🚀 缓存有效性检查
bool UAsyncTools::ShouldUseCachedValue(float CurrentLastTime) const
{
	return PerformanceCache.bCacheValid &&
		   FMath::IsNearlyEqual(PerformanceCache.CachedLastTime, CurrentLastTime, 0.001f);
}

// 🚀 性能统计
FString UAsyncTools::GetPerformanceStats() const
{
	const float CacheHitRate = UpdateCallCount > 0 ? (float)CacheHitCount / UpdateCallCount * 100.0f : 0.0f;

	return FString::Printf(TEXT("更新调用: %d | 缓存命中: %d | 命中率: %.1f%%"),
		UpdateCallCount, CacheHitCount, CacheHitRate);
}

void UAsyncTools::ResetPerformanceStats()
{
	UpdateCallCount = 0;
	CacheHitCount = 0;
	PerformanceCache.Invalidate();
}
