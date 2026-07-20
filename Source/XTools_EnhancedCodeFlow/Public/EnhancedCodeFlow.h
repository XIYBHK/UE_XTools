// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

/**
 * 用于启动代码流动作的静态函数库
 * 必须使用此库来控制插件功能
 * 大多数函数需要 WorldContextObject 来确定函数应该在哪个确切的世界中启动
 * 这在同一编辑器实例中运行多个世界时是必需的（例如测试多人游戏时）
 * 每个动作函数都需要其所有者，以便在所有者被销毁时能够正确清理
 * 启动动作将返回一个句柄。如果句柄无效（检查 IsValid() 函数），则表示动作无法启动
 * 即使动作已完成，句柄仍然有效。要检查动作是否仍在运行，请使用 IsActionRunning(this, Handle) 函数
 * 回调应使用 lambda 表达式定义
 * 使用延迟动作的插件用法示例：
 * FECFHandle DelayHandle = FFlow::Delay(this, 2.f, [this]()
 * {
 *     // 2秒延迟后要执行的代码
 * });
 * 每个函数都接受一个可选的 FECFActionSettings 参数，可以控制 tick 间隔和忽略全局时间膨胀等
 * 更详细的说明请查看 README.md
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ECFHandle.h"
#include "ECFTypes.h"
#include "ECFActionSettings.h"
#include "ECFInstanceId.h"
#include "Coroutines/ECFCoroutineAwaiters.h"
#include "ECFConcepts.h"

/**
 * EnhancedCodeFlow 模块配置常量
 * 集中管理性能参数和默认配置选项
 */
namespace EnhancedCodeFlowConfig
{
    // 默认时间配置
    constexpr float MaxDelayTime = 3600.0f;            // 最大延迟时间（1小时）
}

class XTOOLS_ENHANCEDCODEFLOW_API FEnhancedCodeFlow
{

public:

	/*^^^ ECF 流控制函数 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * 检查给定句柄指向的动作是否正在运行
	 */
	static bool IsActionRunning(const UObject* WorldContextObject, const FECFHandle& Handle);

	/**
	 * Finds handles of running or pending action of the given Class its FECFHandles.
	 */
	static TArray<FECFHandle> GetActionsHandlesByClass(const UObject* WorldContextObject, TSubclassOf<UECFActionBase> Class);

	template<typename T>
	static TArray<FECFHandle> GetActionsHandlesByClass(const UObject* WorldContextObject)
	{
		return GetActionsHandlesByClass(WorldContextObject, T::StaticClass());
	}

	/**
	 * Finds handles of running or pending action of the given Label its FECFHandles.
	 */
	static TArray<FECFHandle> GetActionsHandlesByLabel(const UObject* WorldContextObject, const FString& Label);

	/**
	 * Returns the array of all running and pending actions. Use it mostly for debugging purposes.
	 */
	static TArray<UECFActionBase*> GetAllActions(const UObject* WorldContextObject);

	/**
	 * Returns the number of all running and pending actions. Use it mostly for debugging purposes.
	 */
	static int32 GetActionsCount(const UObject* WorldContextObject);

	/**
	 * Returns the popinter to the Action. Use it mostly for debugging purposes.
	 */
	static UECFActionBase* GetActionFromHandle(const UObject* WorldContextObject, const FECFHandle& Handle);

	/**
	 * Returns the label to the Action. Use it mostly for debugging purposes.
	 */
	static FString GetActionLabelFromHandle(const UObject* WorldContextObject, const FECFHandle& Handle);

	/**
	 * Returns the popinter to the Instanced Action. Use it mostly for debugging purposes.
	 */
	static UECFActionBase* GetActionFromInstancedId(const UObject* WorldContextObject, const FECFInstanceId& InstancedId);

	/**
	 * Pause ticking in the action pointed by given handle.
	 */
	static void PauseAction(const UObject* WorldContextObject, const FECFHandle& Handle);

	/**
	 * 恢复给定句柄指向的动作的 tick
	 */
	static void ResumeAction(const UObject* WorldContextObject, const FECFHandle& Handle);

	/**
	 * 检查给定句柄指向的动作是否已暂停
	 * 如果没有动作则返回 false
	 */
	static bool IsActionPaused(const UObject* WorldContextObject, const FECFHandle& Handle, bool &bIsPaused);

	/**
	 * Resets the action. Have in mind that not every action has reset functionality.
	 * If bCallUpdate is true - the action should run an update event (if there is any) after it's reset.
	 * Returns true if the action was reset, false if there is no action or the action doesn't support resetting.
	 */
	static bool ResetAction(const UObject* WorldContextObject, const FECFHandle& Handle, bool bCallUpdate);

	/**
	 * Sets if the ECF system is paused or not.
	 */
	static void SetPause(const UObject* WorldContextObject, bool bPaused);

	/**
	 * 检查 ECF 系统是否暂停
	 */
	static bool GetPause(const UObject* WorldContextObject);

	/*^^^ 停止 ECF 函数 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * 停止给定句柄指向的运行中动作，使给定句柄失效
	 * bComplete 参数指示动作停止时是否应该完成（运行回调），还是简单停止
	 */
	static void StopAction(const UObject* WorldContextObject, FECFHandle& Handle, bool bComplete = false);

	/**
	 * 停止具有给定实例ID的运行中动作
	 * bComplete 参数指示动作停止时是否应该完成（运行回调），还是简单停止
	 */
	static void StopInstancedAction(const UObject* WorldContextObject, FECFInstanceId InstanceId, bool bComplete = false);

	/**
	 * 停止所有运行中的动作
	 * 如果定义了所有者，将移除给定所有者的所有动作
	 * 否则将停止所有地方的所有动作
	 * bComplete 参数指示动作停止时是否应该完成（运行回调），还是简单停止
	 */
	static void StopAllActions(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * Stops all running actions of the given class.
	 * If owner is defined it will remove only actions from that given owner.
	 * bComplete param indicates if the action should be completed when stopped (run callback), or simply stopped.
	 */
	static void StopAllActionsOfClass(const UObject* WorldContextObject, TSubclassOf<UECFActionBase> Class, bool bComplete = false, UObject* InOwner = nullptr);

	template<typename T>
	static void StopAllActionsOfClass(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr)
	{
		StopAllActionsOfClass(WorldContextObject, T::StaticClass(), bComplete, InOwner);
	}

	/**
	 * Stops all running actions with the given label.
	 * If owner is defined it will remove only actions from that given owner.
	 * bComplete param indicates if the action should be completed when stopped (run callback), or simply stopped.
	 */
	static void StopAllActionsWithLabel(const UObject* WorldContextObject, const FString& Label, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Timing mods ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Gets the action time. It's not CurrentTime, but the time value used by this action, like in delay or timeline.
	 * If the action doesn't support time or there is no action, it will return -1.
	 */
	static float GetActionTime(const UObject* WorldContextObject, const FECFHandle& Handle);

	/**
	 * Sets the action time. It's not CurrentTime, but the time value used by this action, like in delay or timeline.
	 * If the action doesn't support time or there is no action, it will return false.
	 * If bCallUpdate is true - the action should run an update event (if there is any) immediately after it's time change.
	 */
	static bool SetActionTime(const UObject* WorldContextObject, const FECFHandle& Handle, float NewTime, bool bCallUpdate);

	/*^^^ Ticker ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Creates a ticker. It can tick specified amount of time or until it won't be stopped or when owning object won't be destroyed.
	 * To run ticker infinitely set InTickingTime to -1.
	 * @param InTickingTime [optional] - how long the ticker should tick. -1 means it will tick until it is explicitly stopped.
	 * @param InTickFunc - a ticking function can be:
	 *	[](float DeltaTime) -> void
	 *	[](float DeltaTime, FECFHandle TickerHandle) -> void.
	 * @param InCallbackFunc [optional] - a function which will be run after the last tick occurs. Cane be:
	 *	[](bool bStopped) -> void.
	 *	[]() -> void.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 */
	static FECFHandle AddTicker(const UObject* InOwner, TUniqueFunction<void(float/* DeltaTime*/)>&& InTickFunc, TUniqueFunction<void(bool/* bStopped*/)>&& InCallbackFunc = nullptr, const FECFActionSettings& Settings = {});
	static FECFHandle AddTicker(const UObject* InOwner, TUniqueFunction<void(float/* DeltaTime*/)>&& InTickFunc, TUniqueFunction<void()>&& InCallbackFunc = nullptr, const FECFActionSettings& Settings = {});
	static FECFHandle AddTicker(const UObject* InOwner, float InTickingTime, TUniqueFunction<void(float/* DeltaTime*/)>&& InTickFunc, TUniqueFunction<void(bool/* bStopped*/)>&& InCallbackFunc = nullptr, const FECFActionSettings& Settings = {});
	static FECFHandle AddTicker(const UObject* InOwner, float InTickingTime, TUniqueFunction<void(float/* DeltaTime*/)>&& InTickFunc, TUniqueFunction<void()>&& InCallbackFunc = nullptr, const FECFActionSettings& Settings = {});
	static FECFHandle AddTicker(const UObject* InOwner, TUniqueFunction<void(float/* DeltaTime*/, FECFHandle/* ActionHandle*/)>&& InTickFunc, TUniqueFunction<void(bool/* bStopped*/)>&& InCallbackFunc = nullptr, const FECFActionSettings& Settings = {});
	static FECFHandle AddTicker(const UObject* InOwner, TUniqueFunction<void(float/* DeltaTime*/, FECFHandle/* ActionHandle*/)>&& InTickFunc, TUniqueFunction<void()>&& InCallbackFunc = nullptr, const FECFActionSettings& Settings = {});
	static FECFHandle AddTicker(const UObject* InOwner, float InTickingTime, TUniqueFunction<void(float/* DeltaTime*/, FECFHandle/* ActionHandle*/)>&& InTickFunc, TUniqueFunction<void(bool/* bStopped*/)>&& InCallbackFunc = nullptr, const FECFActionSettings& Settings = {});
	static FECFHandle AddTicker(const UObject* InOwner, float InTickingTime, TUniqueFunction<void(float/* DeltaTime*/, FECFHandle/* ActionHandle*/)>&& InTickFunc, TUniqueFunction<void()>&& InCallbackFunc = nullptr, const FECFActionSettings& Settings = {});

	/**
	 * Removes all running tickers.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 * @param InOwner [optional] - if defined it will remove tickers only from the given owner. Otherwise
	 *                             it will remove tickers from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFTicker> instead.")]]
	static void RemoveAllTickers(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Delay ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Execute specified action after some time.
	 * @param InDelayTime - time in seconds to wait before executing action. If set to 0 it will execute in the next frame.
	 * @param InCallbackFunc - a callback with action to execute. Can be:
	 *	[](bool bStopped) -> void.
	 *	[]() -> void.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 */
	static FECFHandle Delay(const UObject* InOwner, float InDelayTime, TUniqueFunction<void(bool/* bStopped*/)>&& InCallbackFunc, const FECFActionSettings& Settings = {});
	static FECFHandle Delay(const UObject* InOwner, float InDelayTime, TUniqueFunction<void()>&& InCallbackFunc, const FECFActionSettings& Settings = {});

	/**
	 * Stops all delays.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 * @param InOwner [optional] - if defined it will remove delayed actions only from the given owner. Otherwise
	 *                             it will remove delayed actions from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFDelay> instead.")]]
	static void RemoveAllDelays(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Delay Ticks ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Execute specified action after some ticks.
	 * @param InDelayTicks - number of ticks after which the action will be executed. If set to 0 it will execute in the nest frame.
	 * @param InCallbackFunc - a callback with action to execute. Can be:
	 *	[](bool bStopped) -> void.
	 *	[]() -> void.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 */
	static FECFHandle DelayTicks(const UObject* InOwner, int32 InDelayTicks, TUniqueFunction<void(bool/* bStopped*/)>&& InCallbackFunc, const FECFActionSettings& Settings = {});
	static FECFHandle DelayTicks(const UObject* InOwner, int32 InDelayTicks, TUniqueFunction<void()>&& InCallbackFunc, const FECFActionSettings& Settings = {});

	/**
	 * Stops all delay ticks.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 * @param InOwner [optional] - if defined it will remove delayed actions only from the given owner. Otherwise
	 *                             it will remove delayed actions from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFDelayTicks> instead.")]]
	static void RemoveAllDelayTicks(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Wait And Execute ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Waits until specific conditions are made and then execute code.
	 * @param InPredicate			- a function that decides if the action should launch.
	 *								  If it returns true it means the action must be launched. Can be:
	 *									[]() -> bool,
	 *									[](float DeltaTime) -> bool
	 * @param InCallbackFunc		- a callback with action to execute. Will return bool indicating if the callback was called because of the timeout. Can be:
	 *									[](bool bTimedOut, bool bStopped) -> void.
	 *									[](bool bTimedOut) -> void.
	 *									[]() -> void.
	 * @param InTimeOut				- if greater than 0.f it will apply timeout to this action. After this time the CallbackFunc will be called with a bTimedOut parameter set to true.
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 */
	static FECFHandle WaitAndExecute(const UObject* InOwner, TUniqueFunction<bool/* bHasFinished*/()>&& InPredicate, TUniqueFunction<void(bool/* bTimedOut*/, bool/* bStopped*/)>&& InCallbackFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});
	static FECFHandle WaitAndExecute(const UObject* InOwner, TUniqueFunction<bool/* bHasFinished*/()>&& InPredicate, TUniqueFunction<void(bool/* bTimedOut*/)>&& InCallbackFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});
	static FECFHandle WaitAndExecute(const UObject* InOwner, TUniqueFunction<bool/* bHasFinished*/()>&& InPredicate, TUniqueFunction<void()>&& InCallbackFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});
	static FECFHandle WaitAndExecute(const UObject* InOwner, TUniqueFunction<bool/* bHasFinished*/(float/* DeltaTime*/)>&& InPredicate, TUniqueFunction<void(bool/* bTimedOut*/, bool/* bStopped*/)>&& InCallbackFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});
	static FECFHandle WaitAndExecute(const UObject* InOwner, TUniqueFunction<bool/* bHasFinished*/(float/* DeltaTime*/)>&& InPredicate, TUniqueFunction<void(bool/* bTimedOut*/)>&& InCallbackFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});
	static FECFHandle WaitAndExecute(const UObject* InOwner, TUniqueFunction<bool/* bHasFinished*/(float/* DeltaTime*/)>&& InPredicate, TUniqueFunction<void()>&& InCallbackFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});


	/**
	 * Stops "wait and execute" actions.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 * @param InOwner [optional] - if defined it will remove "wait and execute" actions only from the given owner.
	 *                             Otherwise it will remove all "wait and execute" actions from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFWaitAndExecute> instead.")]]
	static void RemoveAllWaitAndExecutes(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ While True Execute ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * While the specific condition is true the function will tick.
	 * @param InPredicate - a function that decides if the action should tick.
	 *                      If it returns true - it means the action will tick. Must be: []() -> bool.
	 * @param InTickFunc -  a ticking function must be: [](float DeltaTime) -> void.
	 * @param IncOmpleteFunc - called when the action stops. Will return bool indicating if the callback was called because of the timeout. Can be:
	 *							[](bool bTimedOut, bool bStopped) -> void.
	 *							[](bool bTimedOut) -> void.
	 *							[]() -> void.
	 * @param InTimeOut - if greater than 0.f it will apply timeout to this action. After this time the CompleteFunc will be called with a bTimedOut parameter set to true.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 * @param bEvaluatePredicateOnSetup - when true, evaluates the predicate immediately during setup to preserve the native C++ API behavior.
	 */
	static FECFHandle WhileTrueExecute(const UObject* InOwner, TUniqueFunction<bool/* bIsTrue*/()>&& InPredicate, TUniqueFunction<void(float/* DeltaTime*/)>&& InTickFunc, TUniqueFunction<void(bool/* bTimedOut*/, bool/* bStopped*/)>&& InCompleteFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {}, bool bEvaluatePredicateOnSetup = true);
	static FECFHandle WhileTrueExecute(const UObject* InOwner, TUniqueFunction<bool/* bIsTrue*/()>&& InPredicate, TUniqueFunction<void(float/* DeltaTime*/)>&& InTickFunc, TUniqueFunction<void(bool/* bTimedOut*/)>&& InCompleteFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {}, bool bEvaluatePredicateOnSetup = true);
	static FECFHandle WhileTrueExecute(const UObject* InOwner, TUniqueFunction<bool/* bIsTrue*/()>&& InPredicate, TUniqueFunction<void(float/* DeltaTime*/)>&& InTickFunc, TUniqueFunction<void()>&& InCompleteFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {}, bool bEvaluatePredicateOnSetup = true);

	/**
	 * Stops "while true execute" actions.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 * @param InOwner [optional] - if defined it will remove "while true execute" actions only from the given owner.
	 *							   Otherwise it will remove all "while true execute" actions from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFWhileTrueExecute> instead.")]]
	static void RemoveAllWhileTrueExecutes(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Timeline ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Adds a simple timeline that runs in a given range during a given time.
	 * @param InStartValue -    the value from which this timeline will start.
	 * @param InStopValue -     the value to which this timeline will go. Must be different than InStartValue.
	 * @param InTime -          how long the timeline will be processed? Must be greater than 0.
	 * @param InTickFunc -      ticking function executed when timeline is processed. Its param represents current value. Must be: [](float CurrentValue, float CurrentTime) -> void.
	 * @param InCallbackFunc -  [optional] function which will be launched when timeline reaches end.
	 * @param InBlendFunc -     [optional] a function used to update timeline. By default it is Linear.
	 * @param InBlendExp -      [optional] an exponent used by certain blend functions.
	 * @param InPlayRate -      [optional] timeline playback rate. Must be greater than 0.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 */
	static FECFHandle AddTimeline(const UObject* InOwner, float InStartValue, float InStopValue, float InTime, TUniqueFunction<void(float/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(float/* Value*/, float/* Time*/, bool/* bStopped*/)>&& InCallbackFunc = nullptr, EECFBlendFunc InBlendFunc = EECFBlendFunc::ECFBlend_Linear, float InBlendExp = 1.f, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});
	static FECFHandle AddTimeline(const UObject* InOwner, float InStartValue, float InStopValue, float InTime, TUniqueFunction<void(float/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(float/* Value*/, float/* Time*/)>&& InCallbackFunc = nullptr, EECFBlendFunc InBlendFunc = EECFBlendFunc::ECFBlend_Linear, float InBlendExp = 1.f, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});

	/** Stops timelines owned by InOwner, or all timelines when InOwner is null. */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFTimeline> instead.")]]
	static void RemoveAllTimelines(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Timeline Vector ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/** Adds a vector timeline that runs in a given range during a given time. */
	static FECFHandle AddTimelineVector(const UObject* InOwner, FVector InStartValue, FVector InStopValue, float InTime, TUniqueFunction<void(FVector/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(FVector/* Value*/, float/* Time*/, bool/* bStopped*/)>&& InCallbackFunc = nullptr, EECFBlendFunc InBlendFunc = EECFBlendFunc::ECFBlend_Linear, float InBlendExp = 1.f, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});
	static FECFHandle AddTimelineVector(const UObject* InOwner, FVector InStartValue, FVector InStopValue, float InTime, TUniqueFunction<void(FVector/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(FVector/* Value*/, float/* Time*/)>&& InCallbackFunc = nullptr, EECFBlendFunc InBlendFunc = EECFBlendFunc::ECFBlend_Linear, float InBlendExp = 1.f, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});

	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFTimelineVector> instead.")]]
	static void RemoveAllTimelinesVector(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Timeline Linear Color ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/** Adds a linear color timeline that runs in a given range during a given time. */
	static FECFHandle AddTimelineLinearColor(const UObject* InOwner, FLinearColor InStartValue, FLinearColor InStopValue, float InTime, TUniqueFunction<void(FLinearColor/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(FLinearColor/* Value*/, float/* Time*/, bool/* bStopped*/)>&& InCallbackFunc = nullptr, EECFBlendFunc InBlendFunc = EECFBlendFunc::ECFBlend_Linear, float InBlendExp = 1.f, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});
	static FECFHandle AddTimelineLinearColor(const UObject* InOwner, FLinearColor InStartValue, FLinearColor InStopValue, float InTime, TUniqueFunction<void(FLinearColor/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(FLinearColor/* Value*/, float/* Time*/)>&& InCallbackFunc = nullptr, EECFBlendFunc InBlendFunc = EECFBlendFunc::ECFBlend_Linear, float InBlendExp = 1.f, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});

	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFTimelineLinearColor> instead.")]]
	static void RemoveAllTimelinesLinearColor(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Custom Timeline ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/** Adds a custom timeline defined by a float curve. */
	static FECFHandle AddCustomTimeline(const UObject* InOwner, class UCurveFloat* CurveFloat, TUniqueFunction<void(float/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(float/* Value*/, float/* Time*/, bool/* bStopped*/)>&& InCallbackFunc = nullptr, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});
	static FECFHandle AddCustomTimeline(const UObject* InOwner, class UCurveFloat* CurveFloat, TUniqueFunction<void(float/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(float/* Value*/, float/* Time*/)>&& InCallbackFunc = nullptr, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});

	/** Stops custom float timelines owned by InOwner, or all when InOwner is null. */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFCustomTimeline> instead.")]]
	static void RemoveAllCustomTimelines(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Custom Timeline Vector ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Adds a custom timeline defined by a vector curve.
	 * @param CurveVector - a curve that defines this timeline.
	 * @param InTickFunc - a ticking function executed while the timeline is processed.
	 * @param InCallbackFunc - [optional] function launched when the timeline reaches the end.
	 * @param InPlayRate - [optional] timeline playback rate. Must be greater than 0.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 */
	static FECFHandle AddCustomTimelineVector(const UObject* InOwner, class UCurveVector* CurveVector, TUniqueFunction<void(FVector/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(FVector/* Value*/, float/* Time*/, bool/* bStopped*/)>&& InCallbackFunc = nullptr, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});
	static FECFHandle AddCustomTimelineVector(const UObject* InOwner, class UCurveVector* CurveVector, TUniqueFunction<void(FVector/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(FVector/* Value*/, float/* Time*/)>&& InCallbackFunc = nullptr, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});

	/**
	 * Stops custom timelines vector. Will not launch callback functions.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 * @param InOwner [optional] - if defined it will remove custom timelines only from the given owner.
	 *                             Otherwise it will remove all custom timelines from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFCustomTimelineVector> instead.")]]
	static void RemoveAllCustomTimelinesVector(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Custom Timeline LinearColor ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Adds a custom timeline defined by a linear color curve.
	 * @param CurveLinearColor - a curve that defines this timeline.
	 * @param InTickFunc - a ticking executed when timeline is processed. IOt's param represents current value. Must be: [](LinearColor CurrentValue, float CurrentTime) -> void.
	 * @param InCallbackFunc - [optional] function which will be launched when timeline reaches end. Can be:
	 *	[](LinearColor CurrentValue, float CurrentTime, bool bStopped) -> void.
	 *	[](LinearColor CurrentValue, float CurrentTime) -> void.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 */
	static FECFHandle AddCustomTimelineLinearColor(const UObject* InOwner, class UCurveLinearColor* CurveLinearColor, TUniqueFunction<void(FLinearColor/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(FLinearColor/* Value*/, float/* Time*/, bool/* bStopped*/)>&& InCallbackFunc = nullptr, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});
	static FECFHandle AddCustomTimelineLinearColor(const UObject* InOwner, class UCurveLinearColor* CurveLinearColor, TUniqueFunction<void(FLinearColor/* Value*/, float/* Time*/)>&& InTickFunc, TUniqueFunction<void(FLinearColor/* Value*/, float/* Time*/)>&& InCallbackFunc = nullptr, float InPlayRate = 1.f, const FECFActionSettings& Settings = {});

	/**
	 * Stops custom timelines linear color. Will not launch callback functions.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 * @param InOwner [optional] - if defined it will remove custom timelines only from the given owner.
	 *                             Otherwise it will remove all custom timelines from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFCustomTimelineLinearColor> instead.")]]
	static void RemoveAllCustomTimelinesLinearColor(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Time Lock ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Allow to run the code only once in a given time. (Locks the ability to run the code for a specific amount of time in seconds).
	 * @param InLockTime - time in seconds the lock will persist.
	 * @param InExecFunc - the function to execute.
	 * @param InstanceId - the id of the instance of this action.
	 */
	static FECFHandle TimeLock(const UObject* InOwner, float InLockTime, TUniqueFunction<void()>&& InExecFunc, const FECFInstanceId& InstanceId, const FECFActionSettings& Settings = {});

	/**
	 * Stops time locks.
	 * @param InOwner [optional] - if defined it will remove time locks only from the given owner.
	 *                             Otherwise it will remove all time locks from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFTimeLock> instead.")]]
	static void RemoveAllTimeLocks(const UObject* WorldContextObject, UObject* InOwner = nullptr);

	/*^^^ Do Once ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Run this code of block only once per instance.
	 * @param InExecFunc - the function to execute.
	 * @param InstanceId - the id of the instance of this action.
	 */
	static FECFHandle DoOnce(const UObject* InOwner, TUniqueFunction<void()>&& InExecFunc, const FECFInstanceId& InstanceId);

	/**
	 * Stops DoOnces.
	 * @param InOwner [optional] - if defined it will remove time locks only from the given owner.
	 *                             Otherwise it will remove all time locks from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFDoOnce> instead.")]]
	static void RemoveAllDoOnce(const UObject* WorldContextObject, UObject* InOwner = nullptr);

	/*^^^ Do N Times ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Run this code of block only N times per instance.
	 * @param InTimes - how many times it can be executed.
	 * @param InExecFunc - the function to execute. The function has a counter of executions.
	 * @param InstanceId - the id of the instance of this action.
	 */
	static FECFHandle DoNTimes(const UObject* InOwner, const uint32 InTimes, TUniqueFunction<void(int32/* Counter*/)>&& InExecFunc, const FECFInstanceId& InstanceId);

	/**
	 * Stops DoNTimes.
	 * @param InOwner [optional] - if defined it will remove time locks only from the given owner.
	 *                             Otherwise it will remove all time locks from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFDoNTimes> instead.")]]
	static void RemoveAllDoNTimes(const UObject* WorldContextObject, UObject* InOwner = nullptr);

	/*^^^ Do No More Than X Time ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Run this code of block now and if run again - wait until the time after the previous run is no shorter than the given time.
	 * It can enqueue the given number of executions (must be at least 1).
	 * @param InExecFunc - the function to execute.
	 * @param InTime - the time of for how long the two code executions should be hold.
	 * @param InMaxExecsEnqueue - how many extra executions can be enqueued (must be at least 1).
	 * @param InstanceId - the id of the instance of this action.
	 */
	static FECFHandle DoNoMoreThanXTime(const UObject* InOwner, TUniqueFunction<void()>&& InExecFunc, float InTime, int32 InMaxExecsEnqueue, FECFInstanceId& InstanceId, const FECFActionSettings& Settings = {});

	/**
	 * Stops DoNoMoreThanXTimes.
	 * @param InOwner [optional] - if defined it will remove time locks only from the given owner.
	 *                             Otherwise it will remove all time locks from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFDoNoMoreThanXTime> instead.")]]
	static void RemoveAllDoNoMoreThanXTimes(const UObject* WorldContextObject, UObject* InOwner = nullptr);

	/*^^^ Run Async Then ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Runs the given task function on a separate thread and calls the callback function when this task ends.
	 * @param InAsyncTaskFunc		- a task that will be running on a separate thread. Must be: []() -> void.
	 * @param InCallbackFunc		- a callback with action to execute when the async task ends. Will return bool indicating if the callback was called because of the timeout.
	 *	Can be: [](bool bTimedOut, bool bStopped) -> void.
	 *	Can be: [](bool bTimedOut) -> void.
	 *	Can be: []() -> void.
	 * @param InTimeOut				- if greater than 0.f it will apply timeout to this action. After this time the CallbackFunc will be called with a bTimedOut parameter set to true.
	 *								  Have in mind, that the timeout will not stop the running async thread, it just won't trigger callback when the async task ends. Handle timeout on the side of the async task itself.
	 * @param InThreadPriority		- thread priority (can be Normal or HiPriority).
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 */
	static FECFHandle RunAsyncThen(const UObject* InOwner, TUniqueFunction<void()>&& InAsyncTaskFunc, TUniqueFunction<void(bool/* bTimedOut*/, bool/* bStopped*/)>&& InCallbackFunc, float InTimeOut = 0.f, EECFAsyncPrio InThreadPriority = EECFAsyncPrio::Normal, const FECFActionSettings& Settings = {});
	static FECFHandle RunAsyncThen(const UObject* InOwner, TUniqueFunction<void()>&& InAsyncTaskFunc, TUniqueFunction<void(bool/* bStopped*/)>&& InCallbackFunc, float InTimeOut = 0.f, EECFAsyncPrio InThreadPriority = EECFAsyncPrio::Normal, const FECFActionSettings& Settings = {});
	static FECFHandle RunAsyncThen(const UObject* InOwner, TUniqueFunction<void()>&& InAsyncTaskFunc, TUniqueFunction<void()>&& InCallbackFunc, float InTimeOut = 0.f, EECFAsyncPrio InThreadPriority = EECFAsyncPrio::Normal, const FECFActionSettings& Settings = {});

	/**
	 * Stops Run Async Thens. Have in mind it will not stop running async threads.
	 * It will just forget about them and won't trigger callbacks when async tasks ends.
	 * Handle stopping async tasks inside themselves.
	 * @param InOwner [optional] - if defined it will remove time locks only from the given owner.
	 *                             Otherwise it will remove all time locks from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFRunAsyncThen> instead.")]]
	static void RemoveAllRunAsyncThen(const UObject* WorldContextObject, UObject* InOwner = nullptr);

	/*^^^ Load Objects Async ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Asynchronously loads a list of assets using StreamableManager and calls a callback when all assets are loaded.
	 * @param InObjectsToLoad		- an array of soft object paths to load.
	 * @param InCallbackFunc		- a callback function to execute when loading is complete. Can be:
	 *	[](bool bStopped) -> void.
	 *	[]() -> void.
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 * @return FECFHandle			- handle to the loading action. Can be used to pause, resume, or stop the loading.
	 */
	static FECFHandle LoadObjectsAsync(const UObject* InOwner, const TArray<FSoftObjectPath>& InObjectsToLoad, TUniqueFunction<void(bool/* bStopped*/)>&& InCallbackFunc, const FECFActionSettings& Settings = {});
	static FECFHandle LoadObjectsAsync(const UObject* InOwner, const TArray<FSoftObjectPath>& InObjectsToLoad, TUniqueFunction<void()>&& InCallbackFunc, const FECFActionSettings& Settings = {});

	/*^^^ Wait Seconds (Coroutine) ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Suspends running coroutine function for a specified time.
	 * @param InTime - time in seconds the suspension will last.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 */
	static FECFCoroutineAwaiter_WaitSeconds WaitSeconds(const UObject* InOwner, float InTime, const FECFActionSettings& Settings = {});

	/**
	 * Stops all Wait Seconds coroutine actions.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 *							   !!!Have in mind that not completed coroutine will suspend function forever!!!
	 * @param InOwner [optional] - if defined it will remove Wait Seconds actions only from the given owner. Otherwise
	 *                             it will remove Wait Seconds actions from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFWaitSeconds> instead.")]]
	static void RemoveAllWaitSeconds(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Wait Ticks (Coroutine) ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Suspends running coroutine function for a specified amount of ticks.
	 * @param InTicks - the amount of ticks the suspension will last.
	 * @param Settings [optional] - an extra settings to apply to this action.
	 */
	static FECFCoroutineAwaiter_WaitTicks WaitTicks(const UObject* InOwner, int32 InTicks, const FECFActionSettings& Settings = {});

	/**
	 * Stops all Wait Ticks coroutine actions.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 *							   !!!Have in mind that not completed coroutine will suspend function forever!!!
	 * @param InOwner [optional] - if defined it will remove Wait Ticks actions only from the given owner. Otherwise
	 *                             it will remove Wait Ticks actions from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFWaitTicks> instead.")]]
	static void RemoveAllWaitTicks(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Wait Until (Coroutine) ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Suspends running coroutine function until the given predicate won't return true.
	 * @param InPredicate			- a function that decides if the suspended function should be resumed.
	 *								  If it returns true it means the suspended function must be resumed. Can be:
	 *									[]() -> bool,
	 *									[](float DeltaTime) -> bool
	 * @param InTimeOut				- if greater than 0.f it will apply timeout to this action. After this timeout the suspended function will be resumed.
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 */
	static FECFCoroutineAwaiter_WaitUntil WaitUntil(const UObject* InOwner, TUniqueFunction<bool(float/* DeltaTime*/)>&& InPredicate, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});
	static FECFCoroutineAwaiter_WaitUntil WaitUntil(const UObject* InOwner, TUniqueFunction<bool()>&& InPredicate, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});

	/**
	 * Stops all Wait Until coroutine actions.
	 * @param bComplete			 - indicates if the action should be completed when stopped (run callback), or simply stopped.
	 *							   !!!Have in mind that not completed coroutine will suspend function forever!!!
	 * @param InOwner [optional] - if defined it will remove Wait Until actions only from the given owner. Otherwise
	 *                             it will remove Wait Until actions from everywhere.
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFWaitUntil> instead.")]]
	static void RemoveAllWaitUntil(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Wait For Flag (Coroutine) ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Suspends running coroutine function until the given flag won't return true.
	 * @param InFlag				- a pointer to a boolean flag that decides if the suspended function should be resumed.
	 *	  							  LIFETIME CONTRACT: The pointed-to bool is dereferenced every Tick until the wait completes.
	 *	  							  It MUST remain valid for the entire suspension duration. Safe sources:
	 *	  							  - A local variable in the coroutine frame (lives as long as the coroutine).
	 *	  							  - A member of an object guaranteed to outlive the wait.
	 *	  							  Unsafe: stack variables of a non-coroutine caller, or members of UObjects
	 *	  							  that may be GC'd/destroyed before the wait completes.
	 * @param InTimeOut				- if greater than 0.f it will apply timeout to this action. After this timeout the suspended function will be resumed.
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 */
	static FECFCoroutineAwaiter_WaitForFlag WaitForFlag(const UObject* InOwner, bool* bInFlag, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});

	/*^^^ Run Async And Wait (Coroutine) ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Suspends running coroutine function until the given predicate won't return true.
	 * @param InPredicate			- a function that decides if the suspended function should be resumed.
	 *								  If it returns true it means the suspended function must be resumed. Must be: [](float DeltaTime) -> bool
	 * @param InTimeOut				- if greater than 0.f it will apply timeout to this action. After this timeout the suspended function will be resumed.
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 */
	static FECFCoroutineAwaiter_RunAsyncAndWait RunAsyncAndWait(const UObject* InOwner, TUniqueFunction<void()>&& InAsyncTaskFunc, float InTimeOut = 0.f, EECFAsyncPrio InThreadPriority = EECFAsyncPrio::Normal, const FECFActionSettings& Settings = {});

	/**
	 * 停止所有异步等待协程动作
	 * @param bComplete			 - 指示动作停止时是否应该完成（运行回调），还是简单停止
	 *							   !!!注意：未完成的协程将永远挂起函数!!!
	 * @param InOwner [可选] - 如果定义，将仅从给定所有者中移除异步等待动作。否则将从所有地方移除
	 */
	[[deprecated("Function deprecated. Use StopAllActionsOfClass<UECFRunAsyncAndWait> instead.")]]
	static void RemoveAllRunAsyncAndWait(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ Wait Load Objects (Coroutine) ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Suspends running coroutine function until all assets are loaded.
	 * @param InObjectsToLoad		- an array of soft object paths to load.
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 */
	static FECFCoroutineAwaiter_WaitLoadObjects WaitLoadObjects(const UObject* InOwner, const TArray<FSoftObjectPath>& InObjectsToLoad, const FECFActionSettings& Settings = {});

	/**
	 * Suspends running coroutine function until all primary assets are loaded.
	 * @param InPrimaryAssetsToLoad	- an array of primary asset IDs to load.
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 */
	static FECFCoroutineAwaiter_WaitLoadObjects WaitLoadObjects(const UObject* InOwner, const TArray<FPrimaryAssetId>& InPrimaryAssetsToLoad, const FECFActionSettings& Settings = {});

	/**
	 * Utility function for converting an array of soft pointers to an array of soft object paths.
	 */
	template<CIsSoftPtrType T>
	static TArray<FSoftObjectPath> ConvertSoftPtrToSoftPath(const TArray<T>& InSoftPtrs)
	{
		TArray<FSoftObjectPath> Paths;
		for (const auto& SoftPtr : InSoftPtrs)
		{
			if (SoftPtr.IsNull() == false)
			{
				Paths.Add(SoftPtr.ToSoftObjectPath());
			}
		}
		return Paths;
	}

	/*^^^ Loop And Wait (Coroutine) ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * Suspends running coroutine function until the given predicate won't return true.
	 * @param InPredicate			- a function that decides if the suspended function should be resumed.
	 *								  If it returns true it means the suspended function must be resumed. Must be: []() -> bool
	 * @param InTickFunc			- a ticking function that will be called every tick while waiting. Must be: [](float DeltaTime) -> void
	 * @param InTimeOut				- if greater than 0.f it will apply timeout to this action. After this timeout the suspended function will be resumed.
	 * @param Settings [optional]	- an extra settings to apply to this action.
	 */
	static FECFCoroutineAwaiter_LoopAndWait LoopAndWait(const UObject* InOwner, TUniqueFunction<bool()>&& InPredicate, TUniqueFunction<void(float)>&& InTickFunc, float InTimeOut = 0.f, const FECFActionSettings& Settings = {});
};

using FFlow = FEnhancedCodeFlow;
