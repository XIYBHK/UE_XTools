/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AxisLockTypes.h"
#include "AxisLockerComponent.generated.h"

class UPrimitiveComponent;

/**
 * 物理轴向锁定组件（薄适配器）。
 * 默认锁定自己挂载的父级 PrimitiveComponent（与参考插件等价：挂到 Cube 下控制 Cube）；
 * 也可通过显式目标名称或运行时 SetTargetComponent 指定其他组件。
 * 所有锁定操作委托给 UAxisLockLibrary。
 */
UCLASS(ClassGroup = (XTools), meta = (BlueprintSpawnableComponent, DisplayName = "轴向锁定组件"))
class AXISLOCKER_API UAxisLockerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAxisLockerComponent();

	/** 锁定预设；为"无"时改用下面 6 个手动开关。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "预设"))
	EAxisLockPreset Preset = EAxisLockPreset::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移X", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockPositionX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移Y", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockPositionY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移Z", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockPositionZ = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转X", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockRotationX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转Y", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockRotationY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转Z", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockRotationZ = false;

	/** 是否在 BeginPlay 时按配置自动应用锁定。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "开始游戏时自动应用"))
	bool bAutoApplyOnBeginPlay = true;

	/** 可选：指定要锁定的 PrimitiveComponent 名称；留空则锁定挂载父级。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定",
		meta = (DisplayName = "目标组件名称（可选）", GetOptions = "GetTargetComponentNameOptions"))
	FName TargetComponentName = NAME_None;

	/** 运行时指定锁定目标（优先级最高）。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定", meta = (DisplayName = "设置目标组件"))
	void SetTargetComponent(UPrimitiveComponent* InTarget);

	/** 按当前配置（预设或手动开关）应用锁定。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定", meta = (DisplayName = "应用配置的锁定"))
	void ApplyConfiguredLock();

	/** 返回当前 Actor/蓝图中可作为目标的 PrimitiveComponent 名称。*/
	UFUNCTION()
	TArray<FString> GetTargetComponentNameOptions() const;

	/**
	 * 查询当前目标解析状态（纯查询，无副作用、不输出日志/屏幕提示）。
	 * @param OutTarget 解析到的目标组件；失败时为空。TargetOverrideInvalid 时为回退解析结果（可能为空）。
	 * @return 解析状态，可区分 Ready / NoTargetAvailable / NameNotFound / NoBodyInstance / TargetOverrideInvalid。
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|轴向锁定", meta = (DisplayName = "查询目标解析状态"))
	EAxisLockTargetStatus GetTargetResolveStatus(UPrimitiveComponent*& OutTarget) const;

	/** 保存目标组件当前锁定状态到栈（之后可 PopLockState 还原）。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定", meta = (DisplayName = "压入锁定状态"))
	void PushLockState();

	/** 从栈弹出并恢复最近一次保存的锁定状态。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定", meta = (DisplayName = "弹出并恢复锁定状态"))
	void PopLockState();

protected:
	virtual void BeginPlay() override;

	/** 解析最终要操作的物理组件（保留名称未找到时的既有 Warning 与屏幕提示）。*/
	UPrimitiveComponent* ResolveTarget() const;

	/** 纯解析事实源：输出解析状态，不输出日志。ResolveTarget 与 GetTargetResolveStatus 共用，避免两套逻辑漂移。*/
	UPrimitiveComponent* ResolveTargetWithStatus(EAxisLockTargetStatus& OutStatus) const;

private:
	struct FStoredAxisLockState
	{
		TWeakObjectPtr<UPrimitiveComponent> Target;
		FAxisLockState State;
	};

	/** 运行时覆盖目标（SetTargetComponent 设置），优先于 TargetComponentName 与挂载父级。*/
	TWeakObjectPtr<UPrimitiveComponent> TargetComponentOverride;

	/** 是否曾通过 SetTargetComponent 设置过非空覆盖（用于识别覆盖失效；非持久化状态）。*/
	bool bTargetOverrideWasSet = false;

	/** 临时锁定状态栈；状态始终恢复到压栈时的原目标。*/
	TArray<FStoredAxisLockState> LockStateStack;
};
