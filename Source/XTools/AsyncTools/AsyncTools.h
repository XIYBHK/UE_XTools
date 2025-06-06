#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TimerManager.h"
#include "AsyncTools.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAsyncDelegate, float, Time, float, CurveValue, float, A, float, B);

// 错误类型枚举
UENUM(BlueprintType)
enum class EAsyncToolsErrorType : uint8
{
	InvalidParameter    UMETA(DisplayName = "参数错误"),
	WorldContextInvalid UMETA(DisplayName = "World上下文无效"),
	CurveError         UMETA(DisplayName = "曲线错误"),
	TimerError         UMETA(DisplayName = "定时器错误"),
	StateError         UMETA(DisplayName = "状态错误")
};

// 错误委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAsyncToolsError, 
	EAsyncToolsErrorType, ErrorType,
	const FString&, ErrorMessage,
	const FString&, Context);

/**
 * 异步工具类，用于处理异步操作和定时器功能
 * 支持曲线插值、暂停/恢复、循环等功能
 */
UCLASS(Blueprintable, meta=(DisplayName="Async Tools"))
class XTOOLS_API UAsyncTools : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
	UAsyncTools(const FObjectInitializer& ObjectInitializer);
	~UAsyncTools();
	
public:
	/**
	 * 创建异步操作实例（带输出引用）
	 */
	UFUNCTION(BlueprintCallable, 
		meta=(BlueprintInternalUseOnly="true", 
			DisplayName="Async Action",
				CompactNodeTitle="AsyncAction",
				ToolTip="创建异步操作实例",
				WorldContext="WorldContext",
				CurveValue="0.0",
				bUseCurve="false",
				A="0.0",
				B="0.0",
				DeltaSeconds="0.033",
				Time="1.0",
				FirstDelay="0.0"),
		Category="XTools|Async")
	static UAsyncTools* AsyncAction(
		UObject* WorldContext,
		UCurveFloat* CurveFloat,
		float CurveValue,
		bool bUseCurve,
		float A,
		float B,
		float DeltaSeconds,
		float Time,
		float FirstDelay,
		UPARAM(DisplayName="Async Reference") UAsyncTools*& OutAsyncRef
	);

	/**
	 * 创建异步操作实例（无输出引用）
	 */
	static UAsyncTools* AsyncAction(
		UObject* WorldContext,
		UCurveFloat* CurveFloat = nullptr,
		float CurveValue = 0.0f,
		bool bUseCurve = false,
		float A = 0.0f,
		float B = 0.0f,
		float DeltaSeconds = 0.033f,
		float Time = 1.0f,
		float FirstDelay = 0.0f
	);
	
	virtual void Activate() override;

	/** 异步操作开始时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="异步操作开始时触发"))
	FAsyncDelegate OnStartDelegate;

	/** 异步操作更新时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="异步操作更新时触发"))
	FAsyncDelegate OnUpdateDelegate;
	
	/** 异步操作完成时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="异步操作完成时触发"))
	FAsyncDelegate OnCompleteDelegate;

	/** 进度更新时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="进度更新时触发"))
	FAsyncDelegate OnProgressDelegate;

	/**
	 * 处理异步工具错误
	 * @param ErrorType - 错误类型
	 * @param ErrorMessage - 错误消息
	 * @param Context - 错误发生的上下文
	 */
	static void HandleAsyncError(
		EAsyncToolsErrorType ErrorType,
		const FString& ErrorMessage,
		const FString& Context
	);

private:
	/** 是否使用曲线 */
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	bool bUseCurve = false;
	
	/** 是否已暂停 */
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	bool bPaused = false;
	
	/** 是否已取消 */
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	bool bCancelled = false;
	
	/** 是否循环 */
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	bool bLoop = false;

	float Time = 0.0f;
	float LastTime = 0.f;
	float DeltaSeconds = 0.0f;
	float FirstDelay = 0.0f;
	float CurveValue = 0.f;
	float AValue = 0.f;
	float BValue = 0.f;
	float TimeScale = 1.0f;
	
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	UObject* WorldContext;
	UWorld* World;
	UCurveFloat* CurveFloat;
	FTimerHandle TimerHandle;
	
public:
	/** 更新处理函数 */
	UFUNCTION()
	void OnUpdate();

	/** 暂停异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Pause Async Action", ToolTip="暂停当前的异步操作"))
	void Pause();

	/** 恢复异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Resume Async Action", ToolTip="恢复当前的异步操作"))
	void Resume();

	/** 取消异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Cancel Async Action", ToolTip="取消当前的异步操作"))
	void Cancel();

	/** 设置是否循环 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Set Loop Mode", ToolTip="设置异步操作是否循环"))
	void SetLoop(bool bInLoop);

	/** 设置时间缩放 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Set Time Scale", ToolTip="设置时间缩放系数"))
	void SetTimeScale(float InTimeScale);

	/** 更新曲线参数 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Update Curve Parameters", ToolTip="更新曲线的A和B参数"))
	void UpdateCurveParams(float InA, float InB);

	/** 
	 * 打印调试信息到屏幕和日志
	 * 屏幕显示会保持在固定位置并定期更新
	 * @param bPrintToScreen 是否在屏幕上显示
	 * @param bPrintToLog 是否输出到日志
	 * @param TextColor 文本颜色
	 * @param Duration 显示持续时间(秒)
	 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(
			DisplayName="Print Debug Info", 
			Keywords="debug,log,screen,display",
			ToolTip="在屏幕上显示调试信息并输出到日志",
			AdvancedDisplay="bPrintToScreen,bPrintToLog,TextColor,Duration"
		))
	void PrintDebugInfo(
		UPARAM(DisplayName="Print To Screen") bool bPrintToScreen = true,
		UPARAM(DisplayName="Print To Log") bool bPrintToLog = true,
		UPARAM(DisplayName="Text Color") FLinearColor TextColor = FLinearColor(1.0f, 1.0f, 0.0f),
		UPARAM(DisplayName="Duration") float Duration = 2.0f
	) const;
};
