#include "AsyncTools.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Curves/CurveFloat.h"

DEFINE_LOG_CATEGORY_STATIC(LogAsyncTools, Log, All);

UAsyncTools::UAsyncTools(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AddToRoot();
	}
}

UAsyncTools::~UAsyncTools()
{
	UE_LOG(LogAsyncTools, Warning, TEXT("异步操作结束"));
}

/**
 * 创建一个新的异步操作实例
 * @param WorldContext - 世界上下文对象
 * @param CurveFloat - 用于插值的曲线资源
 * @param CurveValue - 用于插值的曲线值
 * @param bUseCurve - 是否使用曲线进行插值
 * @param A - 曲线的A值
 * @param B - 曲线的B值
 * @param DeltaSeconds - 更新曲线的时间步长
 * @param Time - 曲线的总时间
 * @param FirstDelay - 开始曲线前的延迟
 * @param OutAsyncRef - 创建的异步操作的引用
 * @return 新的异步操作实例
 */
UAsyncTools* UAsyncTools::AsyncAction(
		UObject* WorldContext,
		UCurveFloat* CurveFloat,
		float CurveValue,
		bool bUseCurve,
		float A,
		float B,
		float DeltaSeconds,
		float Time,
		float FirstDelay,
		UAsyncTools*& OutAsyncRef)
{
	if (!WorldContext)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldContext为空"));
		return nullptr;
	}

	// 参数检查
	if (bUseCurve && !CurveFloat)
	{
		UE_LOG(LogTemp, Error, TEXT("[AsyncTools] 无效的CurveFloat: bUseCurve为true但CurveFloat为空"));
		return nullptr;
	}

	if (DeltaSeconds <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("[AsyncTools] 无效的DeltaSeconds: %f (必须为正数)"), DeltaSeconds);
		return nullptr;
	}

	if (Time <= 0.0f) 
	{
		UE_LOG(LogTemp, Error, TEXT("[AsyncTools] 无效的Time: %f (必须为正数)"), Time);
		return nullptr;
	}

	// 调试信息
	UE_LOG(LogTemp, Log, TEXT("[AsyncTools] 创建新的异步操作:"));
	UE_LOG(LogTemp, Log, TEXT("  - CurveFloat: %s"), CurveFloat ? *CurveFloat->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Log, TEXT("  - UseCurve: %s"), bUseCurve ? TEXT("true") : TEXT("false"));
#if WITH_EDITOR
	UE_LOG(LogTemp, Log, TEXT("  - Time: %.2f"), Time);
#endif
	UE_LOG(LogTemp, Log, TEXT("  - DeltaSeconds: %.2f"), DeltaSeconds);
	UE_LOG(LogTemp, Log, TEXT("  - FirstDelay: %.2f"), FirstDelay);

	// 创建异步操作实例
	UAsyncTools* Action = NewObject<UAsyncTools>();
	Action->CurveFloat = CurveFloat;
	Action->Time = Time;
	Action->DeltaSeconds = DeltaSeconds;
	Action->CurveValue = CurveValue;
	Action->AValue = A;
	Action->BValue = B;
	Action->bUseCurve = bUseCurve;
	Action->FirstDelay = FirstDelay;

	Action->WorldContext = WorldContext;
	OutAsyncRef = Action;
	
	Action->RegisterWithGameInstance(WorldContext);

	return Action;
}

// 实现无输出引用的版本
UAsyncTools* UAsyncTools::AsyncAction(
	UObject* WorldContext,
	UCurveFloat* CurveFloat,
	float CurveValue,
	bool bUseCurve,
	float A,
	float B,
	float DeltaSeconds,
	float Time,
	float FirstDelay)
{
	UAsyncTools* OutRef = nullptr;
	return AsyncAction(WorldContext, CurveFloat, CurveValue, bUseCurve, 
					  A, B, DeltaSeconds, Time, FirstDelay, OutRef);
}

void UAsyncTools::Activate()
{
	Super::Activate();
	if (WorldContext)
	{
		World = GEngine->GetWorldFromContextObject(WorldContext,EGetWorldErrorMode::ReturnNull);
		if (World)
		{
			UE_LOG(LogTemp,Warning,TEXT("异步操作开始---时间:%f"),Time);
			World->GetTimerManager().SetTimer(TimerHandle,this,&UAsyncTools::OnUpdate,DeltaSeconds,true,FirstDelay);
			OnStartDelegate.Broadcast(Time,CurveValue,AValue,BValue);
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
		RemoveFromRoot();
		SetReadyToDestroy();
		UE_LOG(LogAsyncTools, Warning, TEXT("异步操作取消"));
	}
}

void UAsyncTools::SetLoop(bool bInLoop)
{
	bLoop = bInLoop;
	UE_LOG(LogTemp, Warning, TEXT("设置循环: %s"), bLoop ? TEXT("true") : TEXT("false"));
}

void UAsyncTools::SetTimeScale(float InTimeScale)
{
	TimeScale = FMath::Max(0.0f, InTimeScale);
	UE_LOG(LogTemp, Warning, TEXT("设置时间缩放: %.2f"), TimeScale);
}

void UAsyncTools::UpdateCurveParams(float InA, float InB)
{
	AValue = InA;
	BValue = InB;
	UE_LOG(LogTemp, Warning, TEXT("更新曲线参数: A=%.2f, B=%.2f"), AValue, BValue);
}

void UAsyncTools::PrintDebugInfo() const
{
	UE_LOG(LogTemp, Warning, TEXT("AsyncTools 调试信息:"));
	UE_LOG(LogTemp, Warning, TEXT("  - 时间: %.2f"), Time);
	UE_LOG(LogTemp, Warning, TEXT("  - 上次时间: %.2f"), LastTime);
	UE_LOG(LogTemp, Warning, TEXT("  - 时间步长: %.2f"), DeltaSeconds);
	UE_LOG(LogTemp, Warning, TEXT("  - 曲线值: %.2f"), CurveValue);
	UE_LOG(LogTemp, Warning, TEXT("  - A值: %.2f"), AValue);
	UE_LOG(LogTemp, Warning, TEXT("  - B值: %.2f"), BValue);
	UE_LOG(LogTemp, Warning, TEXT("  - 时间缩放: %.2f"), TimeScale);
	UE_LOG(LogTemp, Warning, TEXT("  - 循环: %s"), bLoop ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - 暂停: %s"), bPaused ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - 取消: %s"), bCancelled ? TEXT("true") : TEXT("false"));
}

void UAsyncTools::OnUpdate()
{
	if (!World || !ensure(World->IsValidLowLevel()))
	{
		UE_LOG(LogAsyncTools, Warning, TEXT("无效的World引用"));
		return;
	}

	if (bCancelled || bPaused)
	{
		return;
	}

	LastTime += DeltaSeconds * TimeScale;
	
	// 计算进度百分比
	float Progress = FMath::Clamp(LastTime / Time, 0.0f, 1.0f);
	OnProgressDelegate.Broadcast(Progress, CurveValue, AValue, BValue);

	if (LastTime > Time)
	{
		OnUpdateDelegate.Broadcast(Time,CurveValue,AValue,BValue);
		OnCompleteDelegate.Broadcast(Time,CurveValue,AValue,BValue);
		
		if (bLoop)
		{
			LastTime = 0.0f;
			OnStartDelegate.Broadcast(Time, CurveValue, AValue, BValue);
		}
		else
		{
			Time = 0.0f;
			if (World)
			{
				World->GetTimerManager().ClearTimer(TimerHandle);
				RemoveFromRoot();
				SetReadyToDestroy();
			}
		}
	}
	else
	{
		if (bUseCurve && CurveFloat)
		{
			CurveValue = CurveFloat->GetFloatValue(LastTime);
		}
		OnUpdateDelegate.Broadcast(LastTime-DeltaSeconds,CurveValue,AValue,BValue);
	}
}
