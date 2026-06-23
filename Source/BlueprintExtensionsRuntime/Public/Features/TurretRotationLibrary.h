#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/Axis.h"
#include "TurretRotationLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UTurretRotationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|功能", meta = (DisplayName = "计算炮塔旋转角度", CompactNodeTitle = "炮塔旋转", Keywords = "Turret Rotation Aim Degree Clamp Interp", ToolTip = "根据炮塔轴变换和目标位置计算当前旋转角度。旋转范围按UE角度区间处理，支持跨-180/180边界；范围跨度接近或超过360度时视为全范围。"))
	static void CalculateRotateDegree
	(
		UPARAM(ref, DisplayName = "当前角度") float& CurrentDegree,
		UPARAM(DisplayName = "炮塔轴变换") const FTransform& ShaftTransform,
		UPARAM(DisplayName = "目标位置") const FVector& TargetLocation,
		UPARAM(DisplayName = "旋转轴") EAxis::Type RotateAxis,
		UPARAM(DisplayName = "旋转速度") float RotateSpeed,
		UPARAM(DisplayName = "旋转范围") const FVector2D& RotateRange,
		UPARAM(DisplayName = "帧时间") float DeltaTime
	);
	
};
