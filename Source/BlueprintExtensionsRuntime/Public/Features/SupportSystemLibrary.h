#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "SupportSystemLibrary.generated.h"

class UPrimitiveComponent;

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API USupportSystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "XTools|蓝图扩展|功能", meta = (DisplayName = "获取局部支点变换", CompactNodeTitle = "局部支点", Keywords = "Support Fulcrum Local Transform Bounds", ToolTip = "根据组件局部包围盒生成四个局部支点。PlaneBase按世界位置输入，会转换到组件局部空间后取Z高度。"))
	static TArray<FTransform> GetLocalFulcrumTransform(UPARAM(DisplayName = "目标组件") const UPrimitiveComponent* TargetComponent, UPARAM(DisplayName = "平面基准") const FVector& PlaneBase);
	
	UFUNCTION(BlueprintPure, Category = "XTools|蓝图扩展|功能", meta = (DisplayName = "获取世界支点变换", CompactNodeTitle = "世界支点", Keywords = "Support Fulcrum World Transform", ToolTip = "把局部支点数组转换为世界支点数组。通常先用获取局部支点变换，再用Actor或组件的世界变换调用本节点；任一输入变换非法时返回空数组。"))
	static TArray<FTransform> GetWorldFulcrumTransform(UPARAM(DisplayName = "对象变换") const FTransform& ObjectTransform, UPARAM(DisplayName = "局部支点数组") const TArray<FTransform>& FulcrumTransformArray);
	
	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|功能", meta = (DisplayName = "稳定高度", CompactNodeTitle = "稳定高度", WorldContext = "WorldContextObject", Keywords = "Support Suspension Stabilize Height PID Trace Force", ToolTip = "对每个世界支点沿支点自身Z轴向下Trace，并用PID向目标组件施加支撑/抓地力。Trace会忽略目标组件和其Owner；任一支点变换非法时会清空PID缓存并返回。"))
	static void StabilizeHeight
	(
		UPARAM(DisplayName = "世界上下文") UObject* WorldContextObject,
		UPARAM(DisplayName = "目标组件") UPrimitiveComponent* TargetComponent,
		UPARAM(DisplayName = "世界支点数组") const TArray<FTransform>& WorldFulcrumTransform,
		UPARAM(DisplayName = "稳定高度") float StableHeight,
		UPARAM(DisplayName = "抓地高度") float GripHeight,
		UPARAM(DisplayName = "抓地强度") float GripStrength,
		UPARAM(DisplayName = "帧时间") float DeltaTime,
		UPARAM(DisplayName = "误差范围") float ErrorRange,
		UPARAM(ref, DisplayName = "上次误差") TArray<float>& LastError,
		UPARAM(ref, DisplayName = "积分误差") TArray<float>& IntegralError,
		UPARAM(DisplayName = "比例系数") float Kp,
		UPARAM(DisplayName = "积分系数") float Ki,
		UPARAM(DisplayName = "微分系数") float Kd,
		UPARAM(DisplayName = "Trace通道") ETraceTypeQuery ChannelType,
		UPARAM(DisplayName = "平均压力系数") float& AveragePressureFactor,
		UPARAM(DisplayName = "平均命中法线") FVector& AverageImpactNormal,
		UPARAM(DisplayName = "绘制调试") bool DrawDebug
	); 
	
};
