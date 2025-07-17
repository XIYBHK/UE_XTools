#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TimerManager.h"
#include "HAL/CriticalSection.h"
#include "AsyncTools.generated.h"

// 🚀 AsyncTools 配置常量命名空间
namespace AsyncToolsConfig
{
	/** 最小时间缩放值，防止除零错误 */
	constexpr float MinTimeScale = 0.0001f;

	/** 缓存精度阈值，用于判断是否可以使用缓存值 */
	constexpr float CachePrecisionThreshold = 0.001f;

	/** 默认更新间隔 (秒) */
	constexpr float DefaultTickInterval = 0.016f; // ~60 FPS

	/** 默认时间缩放系数 */
	constexpr float DefaultTimeScale = 1.0f;

	/** 性能统计的最大记录数量 */
	constexpr int32 MaxPerformanceRecords = 10000;
}

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
 * 异步工具类，用于处理基于时间的插值和动画操作
 * 
 * 主要功能：
 * - 基于时间的线性或曲线插值
 * - 可自定义起始值和结束值
 * - 支持暂停/恢复/取消操作
 * - 支持循环模式
 * - 支持时间缩放（加速/减速）
 * - 提供多种事件委托（开始/更新/完成/进度）
 * 
 * 典型用途：
 * - UI元素动画（淡入淡出、移动、缩放）
 * - 相机平滑过渡
 * - 颜色渐变效果
 * - 数值计数器动画
 * - 定时执行任务
 */
UCLASS(Blueprintable, meta=(DisplayName="Async Tools"))
class XTOOLS_API UAsyncTools : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
	UAsyncTools(const FObjectInitializer& ObjectInitializer);
	virtual ~UAsyncTools();

	// 🚀 UObject 生命周期管理
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	
public:
	/**
	 * 创建异步操作实例（带输出引用）
	 */
	UFUNCTION(BlueprintCallable, 
		meta=(BlueprintInternalUseOnly="true", 
			DisplayName="Async Action",
				CompactNodeTitle="AsyncAction",
				ToolTip="创建一个可配置的异步操作，用于随时间推移执行插值、动画或定时任务。\n\n@param WorldContext 世界上下文对象，通常为Self。\n@param Duration 异步操作的总持续时间(秒)。\n@param StartValueA 起始值A，插值的起点。\n@param EndValueB 结束值B，插值的终点。\n@param Curve 用于控制插值过程的曲线资源，为空则使用线性插值。\n@param TickInterval 更新间隔(秒)，影响委托触发频率。\n@param StartDelay 开始前的延迟时间(秒)。\n@param OutAsyncRef [输出] 异步操作的引用，可用于后续控制(暂停/恢复/取消等)。",
				WorldContext="WorldContext",
				Duration="1.0",
				TickInterval="0.033",
				StartDelay="0.0",
				StartValueA="0.0",
				EndValueB="1.0"
				),
		Category="XTools|Async")
	static UAsyncTools* AsyncAction(
		UObject* WorldContext,
		float Duration,
		float StartValueA,
		float EndValueB,
		UCurveFloat* Curve,
		float TickInterval,
		float StartDelay,
		UPARAM(DisplayName="Async Reference") UAsyncTools*& OutAsyncRef
	);

	/**
	 * 创建异步操作实例（无输出引用）
	 */
	static UAsyncTools* AsyncAction(
		UObject* WorldContext,
		float Duration,
		float StartValueA,
		float EndValueB,
		UCurveFloat* Curve = nullptr,
		float TickInterval = 0.033f,
		float StartDelay = 0.0f
	);
	
	virtual void Activate() override;

	/** 异步操作开始时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="异步操作开始时触发。参数：Time(当前进度0-1)、CurveValue(曲线值)、A(起始值)、B(结束值)"))
	FAsyncDelegate OnStartDelegate;

	/** 异步操作更新时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="异步操作每次更新时触发，频率由TickInterval控制。参数：Time(当前进度0-1)、CurveValue(曲线值)、A(起始值)、B(结束值)"))
	FAsyncDelegate OnUpdateDelegate;
	
	/** 异步操作完成时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="异步操作完成时触发(进度达到1.0或被取消)。参数：Time(当前进度0-1)、CurveValue(曲线值)、A(起始值)、B(结束值)"))
	FAsyncDelegate OnCompleteDelegate;

	/** 进度更新时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="进度更新时触发，与OnUpdateDelegate同时调用，用于UI进度显示等场景。参数：Time(当前进度0-1)、CurveValue(曲线值)、A(起始值)、B(结束值)"))
	FAsyncDelegate OnProgressDelegate;

	/** 错误发生时触发 */
	UPROPERTY(BlueprintAssignable, Category="XTools|Async", meta=(ToolTip="异步操作发生错误时触发。参数：ErrorType(错误类型)、ErrorMessage(错误消息)、Context(错误上下文)"))
	FOnAsyncToolsError OnErrorDelegate;

	/**
	 * 🚀 实例级错误处理 - 支持 Blueprint 错误回调
	 * @param ErrorType - 错误类型
	 * @param ErrorMessage - 错误消息
	 * @param Context - 错误发生的上下文
	 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Handle Async Error", ToolTip="处理异步操作错误，会触发OnErrorDelegate委托并记录日志"))
	void HandleAsyncError(
		EAsyncToolsErrorType ErrorType,
		const FString& ErrorMessage,
		const FString& Context
	);

	/**
	 * 🚀 静态错误处理 - 用于静态函数调用
	 * @param ErrorType - 错误类型
	 * @param ErrorMessage - 错误消息
	 * @param Context - 错误发生的上下文
	 */
	static void HandleStaticAsyncError(
		EAsyncToolsErrorType ErrorType,
		const FString& ErrorMessage,
		const FString& Context
	);

private:
	/** 🚀 线程安全保护 - 保护所有状态变量的访问 */
	mutable FCriticalSection StateLock;

	/** 是否使用曲线 */
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	bool bUseCurve = false;

	// 🚀 受保护的状态变量 - 需要通过 StateLock 访问
	float Time = 0.0f;
	float LastTime = 0.0f;
	float DeltaSeconds = AsyncToolsConfig::DefaultTickInterval;
	float FirstDelay = 0.0f;
	float CurveValue = 0.0f;
	float AValue = 0.0f;
	float BValue = 0.0f;
	
	// 🚀 改进的垃圾回收保护 - 使用 TWeakObjectPtr 避免循环引用
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	TWeakObjectPtr<UObject> WorldContextWeak;

	// 🚀 使用 TWeakObjectPtr 防止强引用导致的内存泄漏
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	TWeakObjectPtr<UWorld> WorldWeak;

	// 🚀 曲线对象的安全引用
	UPROPERTY(meta=(AllowPrivateAccess="true"))
	TWeakObjectPtr<UCurveFloat> CurveFloatWeak;

	// 🚀 定时器句柄 - 需要在析构时清理
	FTimerHandle TimerHandle;

	// 🚀 性能优化 - 智能缓存系统
	struct FAsyncToolsCache
	{
		float CachedProgress = -1.0f;
		float CachedCurveValue = 0.0f;
		float CachedLastTime = -1.0f;
		bool bCacheValid = false;

		void Invalidate()
		{
			bCacheValid = false;
			CachedProgress = -1.0f;
			CachedLastTime = -1.0f;
		}

		bool IsValidForProgress(float Progress) const
		{
			return bCacheValid && FMath::IsNearlyEqual(CachedProgress, Progress, AsyncToolsConfig::CachePrecisionThreshold);
		}
	};

	// 🚀 状态管理器 - 封装状态相关操作
	struct FAsyncToolsStateManager
	{
		TAtomic<bool> bPaused{false};
		TAtomic<bool> bCancelled{false};
		TAtomic<bool> bLoop{false};
		TAtomic<bool> bIsBeingDestroyed{false};
		TAtomic<float> TimeScale{AsyncToolsConfig::DefaultTimeScale};

		bool IsActive() const
		{
			return !bCancelled.Load() && !bPaused.Load() && !bIsBeingDestroyed.Load();
		}

		void Reset()
		{
			bPaused.Store(false);
			bCancelled.Store(false);
			bLoop.Store(false);
			bIsBeingDestroyed.Store(false);
			TimeScale.Store(AsyncToolsConfig::DefaultTimeScale);
		}
	};

	mutable FAsyncToolsCache PerformanceCache;
	FAsyncToolsStateManager StateManager;

	// 🚀 性能统计
	mutable int32 UpdateCallCount = 0;
	mutable int32 CacheHitCount = 0;
	
public:
	/** 更新处理函数 */
	UFUNCTION()
	void OnUpdate();

	/** 暂停异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Pause Async Action", ToolTip="暂停当前的异步操作，可通过Resume恢复。暂停期间不会触发任何委托，但计时器会保持活跃状态。"))
	void Pause();

	/** 恢复异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Resume Async Action", ToolTip="恢复之前暂停的异步操作。恢复后会立即触发一次OnUpdateDelegate委托。"))
	void Resume();

	/** 取消异步操作 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Cancel Async Action", ToolTip="取消当前的异步操作。取消后会触发OnCompleteDelegate委托，并清理资源。取消后的操作无法恢复。"))
	void Cancel();

	/** 设置是否循环 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Set Loop Mode", ToolTip="设置异步操作是否循环。\n@param bInLoop 如果为true，操作完成后会自动重新开始；如果为false，操作完成后会触发OnCompleteDelegate并结束。"))
	void SetLoop(bool bInLoop);

	/** 设置时间缩放 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Set Time Scale", ToolTip="设置时间缩放系数，用于加速或减慢异步操作。\n@param InTimeScale 时间缩放系数，大于1加速，小于1减慢，必须大于0。"))
	void SetTimeScale(float InTimeScale);

	/** 更新曲线参数 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async", 
		meta=(DisplayName="Update Curve Parameters", ToolTip="动态更新曲线的起始值A和结束值B。\n@param InA 新的起始值A\n@param InB 新的结束值B"))
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
			ToolTip="在屏幕左上角显示异步操作的详细调试信息。\n\n@param bPrintToScreen 是否在屏幕上显示信息\n@param bPrintToLog 是否同时输出到日志窗口\n@param TextColor 显示文本的颜色\n@param Duration 显示持续时间(秒)，实际上信息会一直显示直到被覆盖",
			AdvancedDisplay="bPrintToScreen,bPrintToLog,TextColor,Duration"
		))
	void PrintDebugInfo(
		UPARAM(DisplayName="Print To Screen") bool bPrintToScreen = true,
		UPARAM(DisplayName="Print To Log") bool bPrintToLog = false,
		UPARAM(DisplayName="Text Color") FLinearColor TextColor = FLinearColor(1.0f, 1.0f, 0.0f),
		UPARAM(DisplayName="Duration") float Duration = 2.0f
	) const;

	/** 🚀 线程安全的状态查询方法 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Is Paused", ToolTip="检查异步操作是否已暂停"))
	bool IsPaused() const { return StateManager.bPaused.Load(); }

	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Is Cancelled", ToolTip="检查异步操作是否已取消"))
	bool IsCancelled() const { return StateManager.bCancelled.Load(); }

	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Is Looping", ToolTip="检查异步操作是否处于循环模式"))
	bool IsLooping() const { return StateManager.bLoop.Load(); }

	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Is Active", ToolTip="检查异步操作是否处于活动状态"))
	bool IsActive() const { return StateManager.IsActive(); }

	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Get Progress", ToolTip="获取当前进度(0.0-1.0)"))
	float GetProgress() const;

	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Get Time Scale", ToolTip="获取当前时间缩放系数"))
	float GetTimeScale() const { return StateManager.TimeScale.Load(); }

	/** 🚀 性能统计和优化方法 */
	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Get Performance Stats", ToolTip="获取性能统计信息"))
	FString GetPerformanceStats() const;

	UFUNCTION(BlueprintCallable, Category="XTools|Async",
		meta=(DisplayName="Reset Performance Stats", ToolTip="重置性能统计计数器"))
	void ResetPerformanceStats();

private:
	/** 🚀 内部性能优化方法 */
	float CalculateCurveValueOptimized(float Progress) const;
	bool ShouldUseCachedValue(float CurrentLastTime) const;
};
