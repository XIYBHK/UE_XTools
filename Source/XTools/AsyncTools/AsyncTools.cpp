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
	UE_LOG(LogAsyncTools, Verbose, TEXT("AsyncTools 实例被销毁 (ID: %p)"), this);
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
		HandleAsyncError(EAsyncToolsErrorType::WorldContextInvalid, TEXT("WorldContext为空"), TEXT("AsyncAction"));
		return nullptr;
	}

	if (TickInterval <= 0.0f)
	{
		HandleAsyncError(EAsyncToolsErrorType::InvalidParameter, FString::Printf(TEXT("无效的TickInterval: %f (必须为正数)"), TickInterval), TEXT("AsyncAction"));
		return nullptr;
	}

	if (Duration <= 0.0f)
	{
		HandleAsyncError(EAsyncToolsErrorType::InvalidParameter, FString::Printf(TEXT("无效的Duration: %f (必须为正数)"), Duration), TEXT("AsyncAction"));
		return nullptr;
	}

	UAsyncTools* Action = NewObject<UAsyncTools>();
	Action->WorldContext = WorldContext;
	Action->Time = Duration;
	Action->DeltaSeconds = TickInterval;
	Action->FirstDelay = StartDelay;
	Action->AValue = StartValueA;
	Action->BValue = EndValueB;
	Action->CurveFloat = Curve;

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
	if (WorldContext)
	{
		World = GEngine->GetWorldFromContextObject(WorldContext,EGetWorldErrorMode::ReturnNull);
		if (World)
		{
			LastTime = 0.0f;
			const float InitialCurveValue = CurveFloat ? CurveFloat->GetFloatValue(0.f) : 0.f;
			const float InitialLerpValue = FMath::Lerp(AValue, BValue, InitialCurveValue);
			
			OnStartDelegate.Broadcast(0.f, InitialCurveValue, AValue, BValue);
			
			World->GetTimerManager().SetTimer(TimerHandle,this,&UAsyncTools::OnUpdate,DeltaSeconds,true,FirstDelay);
		}
	}
}

void UAsyncTools::Pause()
{
	if (World && !bPaused)
	{
		bPaused = true;
		World->GetTimerManager().PauseTimer(TimerHandle);
		UE_LOG(LogAsyncTools, Warning, TEXT("异步操作暂停"));
	}
}

void UAsyncTools::Resume()
{
	if (World && bPaused)
	{
		bPaused = false;
		World->GetTimerManager().UnPauseTimer(TimerHandle);
		UE_LOG(LogAsyncTools, Warning, TEXT("异步操作恢复"));
	}
}

void UAsyncTools::Cancel()
{
	if (World && !bCancelled)
	{
		bCancelled = true;
		World->GetTimerManager().ClearTimer(TimerHandle);
		SetReadyToDestroy();
		UE_LOG(LogAsyncTools, Warning, TEXT("异步操作取消"));
	}
}

void UAsyncTools::SetLoop(bool bInLoop)
{
	bLoop = bInLoop;
	UE_LOG(LogAsyncTools, Warning, TEXT("设置循环: %s"), bLoop ? TEXT("true") : TEXT("false"));
}

void UAsyncTools::HandleAsyncError(EAsyncToolsErrorType ErrorType, const FString& ErrorMessage, const FString& Context)
{
	// 这是一个静态函数，所以我们不能在这里广播一个非静态的委托。
	// 错误处理应该在调用方进行，或者通过一个全局的事件系统。
	// 这里我们只打印日志。
	UE_LOG(LogAsyncTools, Error, TEXT("AsyncTools Error in %s: [%s] %s"), *Context, *UEnum::GetValueAsString(ErrorType), *ErrorMessage);
}

void UAsyncTools::PrintDebugInfo(bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration) const
{
	// 使用非常大的负值作为基础键，以确保我们的消息显示在最顶部
	// 使用INT32_MIN + 一个偏移量，这样我们的消息会显示在最顶部
	const int32 BaseKey = INT32_MIN + 1000;
	
	const float Progress = FMath::IsNearlyZero(Time) ? 0.0f : LastTime / Time;
	const TCHAR* LoopStatus = bLoop ? TEXT("是") : TEXT("否");
	const TCHAR* PausedStatus = bPaused ? TEXT("是") : TEXT("否");
	const TCHAR* CancelledStatus = bCancelled ? TEXT("是") : TEXT("否");

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
	if (bCancelled || bPaused)
	{
		return;
	}

	LastTime += DeltaSeconds;

	float Progress = FMath::Clamp(LastTime / Time, 0.0f, 1.0f);
	
	float CurrentCurveValue = Progress;
	if (CurveFloat)
	{
		CurrentCurveValue = CurveFloat->GetFloatValue(Progress);
	}

	const float LerpValue = FMath::Lerp(AValue, BValue, CurrentCurveValue);
	
	OnUpdateDelegate.Broadcast(Progress, CurrentCurveValue, AValue, BValue);
	OnProgressDelegate.Broadcast(Progress, CurrentCurveValue, AValue, BValue);


	if (LastTime >= Time)
	{
		OnCompleteDelegate.Broadcast(1.f, CurrentCurveValue, AValue, BValue);

		if (bLoop)
		{
			LastTime = 0.0f;
		}
		else
		{
			Cancel();
		}
	}
}

void UAsyncTools::SetTimeScale(float InTimeScale)
{
	if (World && TimerHandle.IsValid())
	{
		// 确保时间缩放是正数
		TimeScale = FMath::Max(0.0001f, InTimeScale);
		
		// 重新设置定时器，应用新的时间缩放
		const float NewTickInterval = DeltaSeconds / TimeScale;
		World->GetTimerManager().SetTimer(TimerHandle, this, &UAsyncTools::OnUpdate, NewTickInterval, true);
		
		UE_LOG(LogAsyncTools, Log, TEXT("时间缩放设置为: %.2f (新的更新间隔: %.4f)"), TimeScale, NewTickInterval);
	}
}

void UAsyncTools::UpdateCurveParams(float InA, float InB)
{
	AValue = InA;
	BValue = InB;
	UE_LOG(LogAsyncTools, Log, TEXT("曲线参数已更新: A=%.2f, B=%.2f"), AValue, BValue);
}
