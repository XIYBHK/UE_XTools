/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AxisLockTypes.h"
#include "AxisLockLibrary.generated.h"

class UPrimitiveComponent;
struct FBodyInstance;

/**
 * 物理 Actor 轴向锁定函数库（无状态）。
 * 输入任意已开启物理模拟的 UPrimitiveComponent，锁定/解锁/查询其自由度。
 */
UCLASS()
class AXISLOCKER_API UAxisLockLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 按 6 轴任意组合锁定目标组件的自由度（底层 SixDOF）。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "锁定轴向",
			Keywords = "物理,锁定,轴,DOF,自由度,旋转,位移",
			ToolTip = "锁定目标组件的位移/旋转自由度（需已开启物理模拟）。\n参数:\nTarget - 目标物理组件\nbLockPosX/Y/Z - 锁定对应轴的位移\nbLockRotX/Y/Z - 锁定对应轴的旋转"
		))
	static void LockAxes(
		UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target,
		UPARAM(DisplayName = "锁定位移X") bool bLockPosX,
		UPARAM(DisplayName = "锁定位移Y") bool bLockPosY,
		UPARAM(DisplayName = "锁定位移Z") bool bLockPosZ,
		UPARAM(DisplayName = "锁定旋转X") bool bLockRotX,
		UPARAM(DisplayName = "锁定旋转Y") bool bLockRotY,
		UPARAM(DisplayName = "锁定旋转Z") bool bLockRotZ);

	/** 解除目标组件的全部自由度锁定。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "解除全部锁定",
			Keywords = "物理,解锁,DOF,自由度",
			ToolTip = "解除目标组件的所有位移/旋转锁定。"
		))
	static void UnlockAll(UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target);

	/** 查询目标组件当前的轴向锁定状态。*/
	UFUNCTION(BlueprintPure, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "获取锁定状态",
			Keywords = "物理,查询,锁定,状态,DOF",
			ToolTip = "读取目标组件当前 DOF 模式、平面锁与 6 个自由度锁定情况。目标无效时返回全部未锁定。"
		))
	static FAxisLockState GetLockState(UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target);

	/** 目标组件是否有任意一个轴被锁定。*/
	UFUNCTION(BlueprintPure, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "是否有轴被锁定",
			Keywords = "物理,查询,锁定,DOF",
			ToolTip = "目标组件只要有任意位移/旋转轴被锁定就返回 true。"
		))
	static bool IsAnyAxisLocked(UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target);

	/** 将预设转换为锁定状态（纯映射，不接触物理）。*/
	UFUNCTION(BlueprintPure, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "预设转锁定状态",
			Keywords = "物理,预设,锁定,状态,DOF",
			ToolTip = "把 EAxisLockPreset 预设转换为对应的 6 轴锁定状态。"
		))
	static FAxisLockState PresetToState(UPARAM(DisplayName = "预设") EAxisLockPreset Preset);

	/** 将预设组合一键应用到目标组件。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "应用锁定预设",
			Keywords = "物理,预设,锁定,DOF,站立不倒,只绕Z轴",
			ToolTip = "把高频物理玩法预设一键应用到目标组件（需已开启物理模拟）。"
		))
	static void ApplyPreset(
		UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target,
		UPARAM(DisplayName = "预设") EAxisLockPreset Preset);

	/** 按完整状态恢复目标组件的 DOF 配置；成功返回 true。*/
	static bool ApplyLockState(UPrimitiveComponent* Target, const FAxisLockState& State);

private:
	/**
	 * 取目标的 BodyInstance 并做校验。
	 * 目标无效/无 BodyInstance 时返回 nullptr 并警告；
	 * 未开启物理模拟时仅警告（仍返回 BodyInstance，因为开启模拟后锁定即生效）。
	 */
	static FBodyInstance* GetValidBodyInstance(UPrimitiveComponent* Target);
};
