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
