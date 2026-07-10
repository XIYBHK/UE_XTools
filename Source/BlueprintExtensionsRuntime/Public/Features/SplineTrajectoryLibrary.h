#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SplineTrajectoryLibrary.generated.h"

class USplineComponent;

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API USplineTrajectoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|功能", meta = (DisplayName = "样条轨迹-平射", CompactNodeTitle = "平射", Keywords = "Spline Trajectory Flat Preview Path", ToolTip = "将样条组件重建为从炮口到目标点的直线路径。仅生成可视化/路径样条，不会移动子弹或施加物理。"))
	static void SplineTrajectoryFlat(UPARAM(DisplayName = "样条组件") USplineComponent* SplineComponent, UPARAM(DisplayName = "炮口变换") const FTransform& MuzzleTransform, UPARAM(DisplayName = "目标位置") const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|功能", meta = (DisplayName = "样条轨迹-抛射", CompactNodeTitle = "抛射", Keywords = "Spline Trajectory Ballistic Preview Path", ToolTip = "将样条组件重建为世界Z方向偏移的抛射预览路径。曲率可为负值，并会限制在-10到10；非有限曲率会按0处理。节点只修改样条，不会模拟真实弹道。"))
	static void SplineTrajectoryBallistic(UPARAM(DisplayName = "样条组件") USplineComponent* SplineComponent, UPARAM(DisplayName = "炮口变换") const FTransform& MuzzleTransform, UPARAM(DisplayName = "目标位置") const FVector& TargetLocation, UPARAM(DisplayName = "曲率") float Curvature);

	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|功能", meta = (DisplayName = "样条轨迹-导弹", CompactNodeTitle = "导弹", Keywords = "Spline Trajectory Rocket Missile Preview Path Random", ToolTip = "将样条组件重建为带随机末端偏转的导弹预览路径。随机因子会限制在-1到1，符号决定单侧偏转方向；需要可复现结果时请使用随机流版本。"))
	static void SplineTrajectoryRocket(UPARAM(DisplayName = "样条组件") USplineComponent* SplineComponent, UPARAM(DisplayName = "炮口变换") const FTransform& MuzzleTransform, UPARAM(DisplayName = "目标位置") const FVector& TargetLocation, UPARAM(DisplayName = "曲率") float Curvature, UPARAM(DisplayName = "随机因子") float RandomFactor);

	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|功能", meta = (DisplayName = "样条轨迹-导弹(随机流)", CompactNodeTitle = "导弹流", Keywords = "Spline Trajectory Rocket Missile Preview Path Random Stream Deterministic", ToolTip = "使用随机流重建带末端偏转的导弹预览样条，随机分布与普通导弹节点一致，随机因子限制在-1到1。适合网络、回放、工具预览等需要可复现随机结果的场景；节点只修改样条。"))
	static void SplineTrajectoryRocketFromStream(UPARAM(DisplayName = "样条组件") USplineComponent* SplineComponent, UPARAM(DisplayName = "炮口变换") const FTransform& MuzzleTransform, UPARAM(DisplayName = "目标位置") const FVector& TargetLocation, UPARAM(DisplayName = "曲率") float Curvature, UPARAM(DisplayName = "随机因子") float RandomFactor, UPARAM(ref, DisplayName = "随机流") FRandomStream& RandomStream);
	
};
