#include "AsyncTools.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Curves/CurveFloat.h"

UAsyncTools::UAsyncTools(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		AddToRoot();
	}
}

UAsyncTools::~UAsyncTools()
{
	UE_LOG(LogTemp,Warning,TEXT("AsyncAction is End"));
}

UAsyncTools* UAsyncTools::AsyncAction(
		UObject* worldContext,
		UCurveFloat* CurveFloat,
		float CurveValue,
		bool bIsUseCurve,
		float A,
		float B,
		float DeltaSeconds,
		float Time,
		float FirstDelay,
		UPARAM(DisplayName="Async Reference") UAsyncTools*& OutAsyncRef)
{
	if (!worldContext)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldContext is null"));
		return nullptr;
	}

	// 参数检查
	if (bIsUseCurve && !CurveFloat)
	{
		UE_LOG(LogTemp, Error, TEXT("[AsyncTools] Invalid CurveFloat: bIsUseCurve is true but CurveFloat is null"));
		return nullptr;
	}

	if (DeltaSeconds <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("[AsyncTools] Invalid DeltaSeconds: %f (must be positive)"), DeltaSeconds);
		return nullptr;
	}

	if (Time <= 0.0f) 
	{
		UE_LOG(LogTemp, Error, TEXT("[AsyncTools] Invalid Time: %f (must be positive)"), Time);
		return nullptr;
	}

	// 调试信息
	UE_LOG(LogTemp, Log, TEXT("[AsyncTools] Creating new async action:"));
	UE_LOG(LogTemp, Log, TEXT("  - CurveFloat: %s"), CurveFloat ? *CurveFloat->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Log, TEXT("  - UseCurve: %s"), bIsUseCurve ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Log, TEXT("  - Time: %.2f"), Time);
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
	Action->bIsUseCurve = bIsUseCurve;
	Action->FirstDelay = FirstDelay;

	Action->WorldContext = worldContext;
	OutAsyncRef = Action;
	
	Action->RegisterWithGameInstance(worldContext);

	return Action;
}

void UAsyncTools::Activate()
{
	Super::Activate();
	if (WorldContext)
	{
		World = GEngine->GetWorldFromContextObject(WorldContext,EGetWorldErrorMode::ReturnNull);
		if (World)
		{
			UE_LOG(LogTemp,Warning,TEXT("AsyncAction Start---Time:%f"),Time);
			World->GetTimerManager().SetTimer(TimerHandle,this,&UAsyncTools::OnUpdate,DeltaSeconds,true,FirstDelay);
			OnStartDelegate.Broadcast(Time,CurveValue,AValue,BValue);
		}
	}
}

void UAsyncTools::Pause()
{
	if (World && !bIsPaused)
	{
		bIsPaused = true;
		World->GetTimerManager().PauseTimer(TimerHandle);
		UE_LOG(LogTemp, Warning, TEXT("AsyncAction Paused"));
	}
}

void UAsyncTools::Resume()
{
	if (World && bIsPaused)
	{
		bIsPaused = false;
		World->GetTimerManager().UnPauseTimer(TimerHandle);
		UE_LOG(LogTemp, Warning, TEXT("AsyncAction Resumed"));
	}
}

void UAsyncTools::Cancel()
{
	if (World && !bIsCancelled)
	{
		bIsCancelled = true;
		World->GetTimerManager().ClearTimer(TimerHandle);
		RemoveFromRoot();
		SetReadyToDestroy();
		UE_LOG(LogTemp, Warning, TEXT("AsyncAction Cancelled"));
	}
}

void UAsyncTools::SetLoop(bool bInLoop)
{
	bLoop = bInLoop;
	UE_LOG(LogTemp, Warning, TEXT("Set Loop: %s"), bLoop ? TEXT("true") : TEXT("false"));
}

void UAsyncTools::SetTimeScale(float InTimeScale)
{
	TimeScale = FMath::Max(0.0f, InTimeScale);
	UE_LOG(LogTemp, Warning, TEXT("Set TimeScale: %.2f"), TimeScale);
}

void UAsyncTools::UpdateCurveParams(float InA, float InB)
{
	AValue = InA;
	BValue = InB;
	UE_LOG(LogTemp, Warning, TEXT("Update Curve Params: A=%.2f, B=%.2f"), AValue, BValue);
}

void UAsyncTools::PrintDebugInfo() const
{
	UE_LOG(LogTemp, Warning, TEXT("AsyncTools Debug Info:"));
	UE_LOG(LogTemp, Warning, TEXT("  - Time: %.2f"), Time);
	UE_LOG(LogTemp, Warning, TEXT("  - LastTime: %.2f"), LastTime);
	UE_LOG(LogTemp, Warning, TEXT("  - DeltaSeconds: %.2f"), DeltaSeconds);
	UE_LOG(LogTemp, Warning, TEXT("  - CurveValue: %.2f"), CurveValue);
	UE_LOG(LogTemp, Warning, TEXT("  - AValue: %.2f"), AValue);
	UE_LOG(LogTemp, Warning, TEXT("  - BValue: %.2f"), BValue);
	UE_LOG(LogTemp, Warning, TEXT("  - TimeScale: %.2f"), TimeScale);
	UE_LOG(LogTemp, Warning, TEXT("  - bLoop: %s"), bLoop ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - bIsPaused: %s"), bIsPaused ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - bIsCancelled: %s"), bIsCancelled ? TEXT("true") : TEXT("false"));
}

void UAsyncTools::OnUpdate()
{
	if (bIsCancelled || bIsPaused)
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
		if (bIsUseCurve && CurveFloat)
		{
			CurveValue = CurveFloat->GetFloatValue(LastTime);
		}
		OnUpdateDelegate.Broadcast(LastTime-DeltaSeconds,CurveValue,AValue,BValue);
	}
}
