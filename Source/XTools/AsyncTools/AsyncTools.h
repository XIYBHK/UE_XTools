#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsyncTools.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FAsyncDelegate,float,Time,float,CurveValue,float,A,float,B);

UCLASS()
class XTOOLS_API UAsyncTools : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
	UAsyncTools(const FObjectInitializer& ObjectInitializer);
	~UAsyncTools();
	
public:
	/**
	 * 异步操作 - 创建异步工具实例
	 * @param worldContext 世界上下文对象
	 * @param CurveFloat 使用的曲线资源
	 * @param CurveValue 曲线值
	 * @param bIsUseCurve 是否使用曲线
	 * @param A 参数A
	 * @param B 参数B
	 * @param DeltaSeconds 时间间隔
	 * @param Time 总时间
	 * @param FirstDelay 首次延迟
	 * @return 异步工具实例
	 * @throws 如果曲线资源为空且bIsUseCurve为true时抛出异常
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName="Async Action", CompactNodeTitle="AsyncAction"), Category="XTools|Async")
	static UAsyncTools* AsyncAction(
		UObject* worldContext,
		UCurveFloat* CurveFloat,
		float CurveValue,
		bool bIsUseCurve,
		float A,
		float B,
		float DeltaSeconds,
		float Time,
		float FirstDelay,
		UPARAM(DisplayName="Async Reference") UAsyncTools*& OutAsyncRef
		);
	
	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable,DisplayName=OnStart)
	FAsyncDelegate OnStartDelegate;

	UPROPERTY(BlueprintAssignable,DisplayName=OnUpdate)
	FAsyncDelegate OnUpdateDelegate;
	
	UPROPERTY(BlueprintAssignable,DisplayName=OnComplete)
	FAsyncDelegate OnCompleteDelegate;

	/** 进度百分比回调 */
	UPROPERTY(BlueprintAssignable,DisplayName=OnProgress)
	FAsyncDelegate OnProgressDelegate;

	/** 错误处理回调 */
	UPROPERTY(BlueprintAssignable,DisplayName=OnError)
	FAsyncDelegate OnErrorDelegate;

private:
	bool bIsUseCurve = false;
	bool bIsPaused = false;
	bool bIsCancelled = false;
	bool bLoop = false;
	float Time;
	float LastTime = 0.f;
	float DeltaSeconds;
	float FirstDelay;
	float CurveValue = 0.f;
	float AValue = 0.f;
	float BValue = 0.f;
	float TimeScale = 1.0f;
	
	UObject* WorldContext;
	UWorld* World;
	UCurveFloat* CurveFloat;
	FTimerHandle TimerHandle;
	
public:
	UFUNCTION()
	void OnUpdate();

	/** 暂停异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async")
	void Pause();

	/** 恢复异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async")
	void Resume();

	/** 取消异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", meta=(DisplayName="Cancel Async Action", CompactNodeTitle="Cancel"))
	void Cancel();

	/** 设置循环模式 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", meta=(DisplayName="Set Loop", CompactNodeTitle="SetLoop"))
	void SetLoop(bool bInLoop);

	/** 设置时间缩放 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async")
	void SetTimeScale(float InTimeScale);

	/** 动态调整曲线参数 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async")
	void UpdateCurveParams(float InA, float InB);

	/** 输出调试信息 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async")
	void PrintDebugInfo() const;
};
