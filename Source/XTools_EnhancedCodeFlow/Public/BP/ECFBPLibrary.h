// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

/**
 * 用于在蓝图中启动代码流程功能的静态函数库
 */

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "BP/ECFHandleBP.h"
#include "ECFActionSettings.h"
#include "ECFTypes.h"
#include "ECFInstanceIdBP.h"
#include "ECFBPLibrary.generated.h"

UENUM(BlueprintType)
enum class ETimeLockOutputType : uint8
{
	Exec UMETA(DisplayName = "执行"),
	Locked UMETA(DisplayName = "已锁定")
};

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/*^^^ ECF系统控制函数 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * 设置ECF系统的暂停状态
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DisplayName = "ECF - 设置暂停状态", Tooltip="设置ECF系统的暂停状态", Category = "XTools|ECF|系统控制"))
	static void ECFSetPause(const UObject* WorldContextObject, bool bPaused);

	/**
	 * 获取ECF系统的暂停状态
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DisplayName = "ECF - 获取暂停状态", Tooltip="获取ECF系统当前是否处于暂停状态", Category = "XTools|ECF|系统控制"))
	static void ECFGetPause(const UObject* WorldContextObject, UPARAM(DisplayName = "是否暂停") bool& bIsPaused);

	/**
	 * 检查指定句柄的动作是否正在运行
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DisplayName = "ECF - 动作是否运行中", Tooltip="检查指定句柄的动作是否正在运行", Category = "XTools|ECF|动作控制"))
	static void ECFIsActionRunning(UPARAM(DisplayName = "正在运行") bool& bIsRunning, const UObject* WorldContextObject, const FECFHandleBP& Handle);

	/**
	 * 暂停正在运行的动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DisplayName = "ECF - 暂停动作", Tooltip="暂停指定句柄的动作", Category = "XTools|ECF|动作控制"))
	static void ECFPauseAction(const UObject* WorldContextObject, const FECFHandleBP& Handle);

	/**
	 * 恢复暂停的动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DisplayName = "ECF - 恢复动作", Tooltip="恢复指定句柄的暂停动作", Category = "XTools|ECF|动作控制"))
	static void ECFResumeAction(const UObject* WorldContextObject, const FECFHandleBP& Handle);

	/**
	 * 检查指定句柄的动作是否处于暂停状态
	 * 如果动作不存在则返回false
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DisplayName = "ECF - 动作是否暂停", Tooltip="检查指定句柄的动作是否处于暂停状态", Category = "XTools|ECF|动作控制"))
	static void ECFIsActionPaused(UPARAM(DisplayName = "正在运行") bool& bIsRunning, UPARAM(DisplayName = "已暂停") bool& bIsPaused, const UObject* WorldContextObject, const FECFHandleBP& Handle);
	
	/*^^^ 停止ECF动作函数 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * 停止指定句柄的动作，并使该句柄失效
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete", DisplayName = "ECF - 停止动作", Tooltip="停止指定句柄的动作，可选择是否执行完成回调", Category = "XTools|ECF|动作控制"))
	static void ECFStopAction(const UObject* WorldContextObject, UPARAM(ref) FECFHandleBP& Handle, bool bComplete = false);

	/**
	 * 停止指定实例ID的所有动作
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete", DisplayName = "ECF - 停止实例动作", Tooltip="停止指定实例ID的所有动作，可选择是否执行完成回调", Category = "XTools|ECF|动作控制"))
	static void ECFStopInstancedActions(const UObject* WorldContextObject, FECFInstanceIdBP InstanceId, bool bComplete = false);

	/**
	 * 停止所有运行中的动作
	 * 如果指定了所有者，则只停止该所有者的动作
	 * 否则停止所有地方的动作
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 停止所有动作", Tooltip="停止所有运行中的动作，可指定所有者和是否执行完成回调", Category = "XTools|ECF|动作控制"))
	static void ECFStopAllActions(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/*^^^ 句柄和实例ID ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
	
	/**
	 * 检查给定的句柄是否有效
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ECF - 句柄是否有效", Tooltip="检查给定的动作句柄是否有效", Category = "XTools|ECF|句柄"))
	static void IsECFHandleValid(UPARAM(DisplayName = "是否有效") bool& bOutIsValid, const FECFHandleBP& Handle);

	/**
	 * 返回一个新的实例ID
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ECF - 获取新实例ID", Tooltip="生成并返回一个新的实例ID", Category = "XTools|ECF|实例"))
	static void ECFGetNewInstanceId(UPARAM(DisplayName = "实例ID") FECFInstanceIdBP& OutInstanceId);

	/**
	 * 检查给定的实例ID是否有效，如果无效则创建新的
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ECF - 验证实例ID", Tooltip="检查实例ID是否有效，无效则创建新的", Category = "XTools|ECF|实例"))
	static void ECFValidateInstanceId(UPARAM(Ref, DisplayName="实例ID") FECFInstanceIdBP& InInstanceId, UPARAM(DisplayName = "实例ID") FECFInstanceIdBP& OutInstanceId);

	/**
	 * 检查给定的实例ID是否有效
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ECF - 实例ID是否有效", Tooltip="检查给定的实例ID是否有效", Category = "XTools|ECF|实例"))
	static void IsECFInstanceIdValid(UPARAM(DisplayName = "是否有效") bool& bIsValid, const FECFInstanceIdBP& InstanceId);

	/*^^^ 时间锁定 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * 允许在指定时间内只执行一次代码（在指定的秒数内锁定代码的执行能力）
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", ExpandEnumAsExecs = "OutExecs", AdvancedDisplay = "Settings", DisplayName = "ECF - 时间锁定", Tooltip="时间锁定功能(实例化)：在指定时间内限制相同实例ID的代码执行\n\nLockTime参数：锁定时间，在此时间内再次调用会进入Locked输出引脚\n\n执行流程：\n1. 首次调用走Exec输出引脚\n2. 锁定期间调用走Locked输出引脚\n3. 锁定时间结束后，又可以执行\n\n注意：必须使用相同的实例ID才能正确跟踪锁定状态", Category = "XTools|ECF|时间控制"))
	static void ECFTimeLock(const UObject* WorldContextObject, ETimeLockOutputType& OutExecs, UPARAM(DisplayName = "句柄") FECFHandleBP& OutHandle, float LockTime, UPARAM(Ref) FECFInstanceIdBP& InstanceId, FECFActionSettings Settings);

	/**
	 * 移除所有时间锁定
	 * @param InOwner [可选] - 如果指定，则只移除该所有者的时间锁
	 *                         否则移除所有地方的时间锁
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "InOwner", DisplayName = "ECF - 移除所有时间锁", Tooltip="移除所有时间锁定，可指定所有者", Category = "XTools|ECF|时间控制"))
	static void ECFRemoveAllTimeLocks(const UObject* WorldContextObject, UObject* InOwner = nullptr);


	/*^^^ 异步动作清理 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * 移除所有延迟动作
	 * 如果指定了所有者，则只移除该所有者的延迟动作
	 * 否则移除所有地方的延迟动作
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有延迟", Tooltip="移除所有延迟动作，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllDelays(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有"等待执行"动作
	 * 如果指定了所有者，则只移除该所有者的等待执行动作
	 * 否则移除所有地方的等待执行动作
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有等待执行", Tooltip="移除所有等待执行的动作，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllWaitAndExecutes(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有"循环执行"动作
	 * 如果指定了所有者，则只移除该所有者的循环执行动作
	 * 否则移除所有地方的循环执行动作
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有循环执行", Tooltip="移除所有循环执行的动作，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void RemoveAllWhileTrueExecutes(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有异步执行动作
	 * 注意：这不会停止已经在运行的异步线程
	 * 只会取消对它们的追踪，当异步任务结束时不会触发回调
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有异步执行", Tooltip="移除所有异步执行的动作（不会停止已运行的异步线程）", Category = "XTools|ECF|清理"))
	static void RemoveAllRunAsyncThen(const UObject* WorldContextObject, UObject* InOwner = nullptr);

	/**
	 * 移除所有定时器
	 * 如果指定了所有者，则只移除该所有者的定时器
	 * 否则移除所有地方的定时器
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有定时器", Tooltip="移除所有定时器，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllTickers(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有时间线
	 * 如果指定了所有者，则只移除该所有者的时间线
	 * 否则移除所有地方的时间线
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有时间线", Tooltip="移除所有时间线，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllTimelines(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有向量时间线
	 * 如果指定了所有者，则只移除该所有者的向量时间线
	 * 否则移除所有地方的向量时间线
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有向量时间线", Tooltip="移除所有向量类型的时间线，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllTimelinesVector(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有颜色时间线
	 * 如果指定了所有者，则只移除该所有者的颜色时间线
	 * 否则移除所有地方的颜色时间线
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有颜色时间线", Tooltip="移除所有颜色类型的时间线，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllTimelinesLinearColor(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有自定义时间线
	 * 如果指定了所有者，则只移除该所有者的自定义时间线
	 * 否则移除所有地方的自定义时间线
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有自定义时间线", Tooltip="移除所有自定义时间线，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllCustomTimelines(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有自定义向量时间线
	 * 如果指定了所有者，则只移除该所有者的自定义向量时间线
	 * 否则移除所有地方的自定义向量时间线
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有自定义向量时间线", Tooltip="移除所有自定义向量类型的时间线，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllCustomTimelinesVector(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有自定义颜色时间线
	 * 如果指定了所有者，则只移除该所有者的自定义颜色时间线
	 * 否则移除所有地方的自定义颜色时间线
	 * bComplete参数决定停止时是否执行完成回调，或仅仅停止动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有自定义颜色时间线", Tooltip="移除所有自定义颜色类型的时间线，可指定所有者和是否执行完成回调", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllCustomTimelinesLinearColor(const UObject* WorldContextObject, bool bComplete = false, UObject* InOwner = nullptr);

	/**
	 * 移除所有限制执行次数的动作
	 * 如果指定了所有者，则只移除该所有者的动作
	 * 否则移除所有地方的动作
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bComplete, InOwner", DisplayName = "ECF - 移除所有限制执行次数", Tooltip="移除所有限制执行次数的动作，可指定所有者", Category = "XTools|ECF|清理"))
	static void ECFRemoveAllDoNoMoreThanXTimes(const UObject* WorldContextObject, UObject* InOwner = nullptr);

	/*^^^ 类型转换 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

	/**
	 * 将ECF句柄转换为字符串格式
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ECF - 句柄转字符串", Tooltip="将ECF句柄转换为字符串格式", CompactNodeTitle = "->", BlueprintAutocast), Category = "XTools|ECF|转换")
	static FString Conv_ECFHandleToString(const FECFHandleBP& Handle);

	/**
	 * 将ECF实例ID转换为字符串格式
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ECF - 实例ID转字符串", Tooltip="将ECF实例ID转换为字符串格式", CompactNodeTitle = "->", BlueprintAutocast), Category = "XTools|ECF|转换")
	static FString Conv_ECFInstanceIdToString(const FECFInstanceIdBP& InstanceId);

	/*^^^ 结束 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
};
