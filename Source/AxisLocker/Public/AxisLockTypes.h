/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/BodyInstance.h"
#include "AxisLockTypes.generated.h"

/**
 * 轴向锁定状态。
 * 既是查询返回值，也是组件临时恢复栈的元素。
 * 保存 FBodyInstance 的完整 DOF 模式、平面锁与 6 个自由度锁定开关。
 */
USTRUCT(BlueprintType)
struct AXISLOCKER_API FAxisLockState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "自由度模式"))
	TEnumAsByte<EDOFMode::Type> DOFMode = EDOFMode::SixDOF;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定平面位移"))
	bool bLockPlaneTranslation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定平面旋转"))
	bool bLockPlaneRotation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移X"))
	bool bLockPositionX = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移Y"))
	bool bLockPositionY = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移Z"))
	bool bLockPositionZ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转X"))
	bool bLockRotationX = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转Y"))
	bool bLockRotationY = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转Z"))
	bool bLockRotationZ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "自定义平面法线"))
	FVector CustomDOFPlaneNormal = FVector::ZeroVector;
};

/**
 * 高频物理玩法预设。映射关系见 UAxisLockLibrary::PresetToState。
 */
UENUM(BlueprintType)
enum class EAxisLockPreset : uint8
{
	None           UMETA(DisplayName = "无（用手动配置）"),
	StayUpright    UMETA(DisplayName = "站立不倒（锁X/Y旋转）"),
	YawOnly        UMETA(DisplayName = "只绕Z轴（锁全位移+X/Y旋转）"),
	HorizontalOnly UMETA(DisplayName = "只能水平移动（锁Z位移）"),
	FreezeAll      UMETA(DisplayName = "完全冻结（6轴全锁）")
};

/**
 * 轴向锁定目标解析状态。
 * 由 UAxisLockerComponent::GetTargetResolveStatus 返回，用于编程区分解析失败的具体原因，
 * 避免只能依赖日志/屏幕提示判断。
 */
UENUM(BlueprintType)
enum class EAxisLockTargetStatus : uint8
{
	/** 已解析到有效目标（OutTarget 可用）。*/
	Ready                 UMETA(DisplayName = "已解析"),

	/** 未指定目标且无可用挂载父级（未设置覆盖、名称为空、父级不是 PrimitiveComponent）。*/
	NoTargetAvailable     UMETA(DisplayName = "无可用目标"),

	/** 指定了目标组件名称，但 Owner 内不存在同名 PrimitiveComponent。*/
	NameNotFound          UMETA(DisplayName = "名称未找到"),

	/** 已解析到目标组件，但其没有有效的 BodyInstance。*/
	NoBodyInstance        UMETA(DisplayName = "目标无BodyInstance"),

	/** 运行时覆盖目标已失效（曾调用 SetTargetComponent，但对象已被销毁）；OutTarget 为回退解析结果（可能为空）。*/
	TargetOverrideInvalid UMETA(DisplayName = "覆盖目标已失效")
};
