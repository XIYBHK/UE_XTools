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
		HandleAsyncError(
			EAsyncToolsErrorType::WorldContextInvalid,
			TEXT("WorldContext为空"),
			TEXT("AsyncAction")
		);
		return nullptr;
	}

	if (bUseCurve && !CurveFloat)
	{
		HandleAsyncError(
			EAsyncToolsErrorType::CurveError,
			TEXT("无效的CurveFloat: bUseCurve为true但CurveFloat为空"),
			TEXT("AsyncAction")
		);
		return nullptr;
	}

	if (DeltaSeconds <= 0.0f)
	{
		HandleAsyncError(
			EAsyncToolsErrorType::InvalidParameter,
			FString::Printf(TEXT("无效的DeltaSeconds: %f (必须为正数)"), DeltaSeconds),
			TEXT("AsyncAction")
		);
		return nullptr;
	}

	if (Time <= 0.0f)
	{
		HandleAsyncError(
			EAsyncToolsErrorType::InvalidParameter,
			FString::Printf(TEXT("无效的Time: %f (必须为正数)"), Time),
			TEXT("AsyncAction")
		);
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
	if (InTimeScale < 0.0f)
	{
		HandleAsyncError(
			EAsyncToolsErrorType::InvalidParameter,
			FString::Printf(TEXT("无效的时间缩放值: %f (必须大于0)"), InTimeScale),
			TEXT("SetTimeScale")
		);
		return;
	}
	TimeScale = InTimeScale;
	UE_LOG(LogTemp, Warning, TEXT("设置时间缩放: %.2f"), TimeScale);
}

void UAsyncTools::UpdateCurveParams(float InA, float InB)
{
	AValue = InA;
	BValue = InB;
	UE_LOG(LogTemp, Warning, TEXT("更新曲线参数: A=%.2f, B=%.2f"), AValue, BValue);
}

void UAsyncTools::PrintDebugInfo(bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration) const
{
	// 使用固定的Key，这样文本位置就不会变动，只有值会更新
	const int32 BaseKey = 39670;  // 使用一个不太常见的基础key值避免冲突

	if (bPrintToScreen && GEngine)
	{
		// 将FLinearColor转换为FColor
		const FColor DisplayColor = TextColor.ToFColor(true);
		
		// 固定的标题文本
		GEngine->AddOnScreenDebugMessage(BaseKey, Duration, DisplayColor, 
			TEXT("===== AsyncTools 调试信息 ====="));
		
		// 各项参数信息，每项使用不同的key以保持位置固定
		GEngine->AddOnScreenDebugMessage(BaseKey + 1, Duration, DisplayColor, 
			FString::Printf(TEXT("时间: %.2f"), Time));
		GEngine->AddOnScreenDebugMessage(BaseKey + 2, Duration, DisplayColor, 
			FString::Printf(TEXT("上次时间: %.2f"), LastTime));
		GEngine->AddOnScreenDebugMessage(BaseKey + 3, Duration, DisplayColor, 
			FString::Printf(TEXT("时间步长: %.2f"), DeltaSeconds));
		GEngine->AddOnScreenDebugMessage(BaseKey + 4, Duration, DisplayColor, 
			FString::Printf(TEXT("曲线值: %.2f"), CurveValue));
		GEngine->AddOnScreenDebugMessage(BaseKey + 5, Duration, DisplayColor, 
			FString::Printf(TEXT("A值: %.2f"), AValue));
		GEngine->AddOnScreenDebugMessage(BaseKey + 6, Duration, DisplayColor, 
			FString::Printf(TEXT("B值: %.2f"), BValue));
		GEngine->AddOnScreenDebugMessage(BaseKey + 7, Duration, DisplayColor, 
			FString::Printf(TEXT("时间缩放: %.2f"), TimeScale));
		GEngine->AddOnScreenDebugMessage(BaseKey + 8, Duration, DisplayColor, 
			FString::Printf(TEXT("循环: %s"), bLoop ? TEXT("是") : TEXT("否")));
		GEngine->AddOnScreenDebugMessage(BaseKey + 9, Duration, DisplayColor, 
			FString::Printf(TEXT("暂停: %s"), bPaused ? TEXT("是") : TEXT("否")));
		GEngine->AddOnScreenDebugMessage(BaseKey + 10, Duration, DisplayColor, 
			FString::Printf(TEXT("取消: %s"), bCancelled ? TEXT("是") : TEXT("否")));
	}

	if (bPrintToLog)
	{
		// 日志输出部分
		UE_LOG(LogAsyncTools, Warning, TEXT("AsyncTools 调试信息:"));
		UE_LOG(LogAsyncTools, Warning, TEXT("  - 时间: %.2f"), Time);
		UE_LOG(LogAsyncTools, Warning, TEXT("  - 上次时间: %.2f"), LastTime);
		UE_LOG(LogAsyncTools, Warning, TEXT("  - 时间步长: %.2f"), DeltaSeconds);
		UE_LOG(LogAsyncTools, Warning, TEXT("  - 曲线值: %.2f"), CurveValue);
		UE_LOG(LogAsyncTools, Warning, TEXT("  - A值: %.2f"), AValue);
		UE_LOG(LogAsyncTools, Warning, TEXT("  - B值: %.2f"), BValue);
		UE_LOG(LogAsyncTools, Warning, TEXT("  - 时间缩放: %.2f"), TimeScale);
		UE_LOG(LogAsyncTools, Warning, TEXT("  - 循环: %s"), bLoop ? TEXT("是") : TEXT("否"));
		UE_LOG(LogAsyncTools, Warning, TEXT("  - 暂停: %s"), bPaused ? TEXT("是") : TEXT("否"));
		UE_LOG(LogAsyncTools, Warning, TEXT("  - 取消: %s"), bCancelled ? TEXT("是") : TEXT("否"));
	}
}

void UAsyncTools::OnUpdate()
{
	if (!World || !ensure(World->IsValidLowLevel()))
	{
		const FString ErrorMsg = TEXT("无效的World引用");
		HandleAsyncError(
			EAsyncToolsErrorType::WorldContextInvalid,
			ErrorMsg,
			TEXT("OnUpdate")
		);
		return;
	}

	if (bCancelled)
	{
		HandleAsyncError(
			EAsyncToolsErrorType::StateError,
			TEXT("操作已被取消"),
			TEXT("OnUpdate")
		);
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

void UAsyncTools::HandleAsyncError(
	EAsyncToolsErrorType ErrorType,
	const FString& ErrorMessage,
	const FString& Context)
{
	// 根据错误类型确定日志级别和显示颜色
	ELogVerbosity::Type ErrorLogLevel;
	FColor DisplayColor;
	FString Prefix;
	const float DisplayTime = 5.0f;  // 固定显示时间为5秒
	
	switch(ErrorType)
	{
		case EAsyncToolsErrorType::WorldContextInvalid:
		case EAsyncToolsErrorType::CurveError:
			ErrorLogLevel = ELogVerbosity::Error;
			DisplayColor = FColor::Red;
			Prefix = TEXT("[AsyncTools错误]");
			break;
			
		case EAsyncToolsErrorType::InvalidParameter:
		case EAsyncToolsErrorType::TimerError:
			ErrorLogLevel = ELogVerbosity::Warning;
			DisplayColor = FColor::Yellow;
			Prefix = TEXT("[AsyncTools警告]");
			break;
			
		case EAsyncToolsErrorType::StateError:
			ErrorLogLevel = ELogVerbosity::Warning;
			DisplayColor = FColor::Orange;
			Prefix = TEXT("[AsyncTools状态]");
			break;
			
		default:
			ErrorLogLevel = ELogVerbosity::Warning;
			DisplayColor = FColor::White;
			Prefix = TEXT("[AsyncTools]");
	}
	
	// 构建完整的错误消息
	const FString FullMessage = FString::Printf(TEXT("%s [%s] %s"), *Prefix, *Context, *ErrorMessage);
	
	// 输出到日志
	switch (ErrorLogLevel)
	{
		case ELogVerbosity::Fatal:
		case ELogVerbosity::Error:
			UE_LOG(LogAsyncTools, Error, TEXT("%s"), *FullMessage);
			break;
		case ELogVerbosity::Warning:
			UE_LOG(LogAsyncTools, Warning, TEXT("%s"), *FullMessage);
			break;
		case ELogVerbosity::Display:
			UE_LOG(LogAsyncTools, Display, TEXT("%s"), *FullMessage);
			break;
		case ELogVerbosity::Log:
			UE_LOG(LogAsyncTools, Log, TEXT("%s"), *FullMessage);
			break;
		case ELogVerbosity::Verbose:
			UE_LOG(LogAsyncTools, Verbose, TEXT("%s"), *FullMessage);
			break;
		case ELogVerbosity::VeryVerbose:
			UE_LOG(LogAsyncTools, VeryVerbose, TEXT("%s"), *FullMessage);
			break;
		default:
			UE_LOG(LogAsyncTools, Log, TEXT("%s"), *FullMessage); // 默认为 Log
			break;
	}
	
	// 显示到屏幕
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			DisplayTime,
			DisplayColor,
			FullMessage
		);
	}
}
