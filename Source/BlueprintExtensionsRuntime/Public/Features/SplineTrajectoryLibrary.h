#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SplineTrajectoryLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API USplineTrajectoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Features", meta = (DisplayName = "样条轨迹-平射", CompactNodeTitle = "平射"))
	static void SplineTrajectoryFlat(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Features", meta = (DisplayName = "样条轨迹-抛射", CompactNodeTitle = "抛射"))
	static void SplineTrajectoryBallistic(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation, float Curvature);

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Features", meta = (DisplayName = "样条轨迹-导弹", CompactNodeTitle = "导弹"))
	static void SplineTrajectoryRocket(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation, float Curvature, float RandomFactor);
	
};