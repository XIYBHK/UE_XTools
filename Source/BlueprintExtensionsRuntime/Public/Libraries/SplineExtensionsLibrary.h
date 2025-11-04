#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SplineExtensionsLibrary.generated.h"


UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API USplineExtensionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Spline", meta = (DisplayName = "样条路径有效", CompactNodeTitle = "SplinePathValid"))
	static bool SplinePathValid(USplineComponent* SplineComponent);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Spline", meta = (DisplayName = "获取样条起点", CompactNodeTitle = "GetSplineStart"))
	static FVector GetSplineStart(USplineComponent* SplineComponent);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Spline", meta = (DisplayName = "获取样条终点", CompactNodeTitle = "GetSplineEnd"))
	static FVector GetSplineEnd(USplineComponent* SplineComponent);
	
	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Spline", meta = (DisplayName = "获取样条路径", CompactNodeTitle = "GetSplinePath"))
	static TArray<FVector> GetSplinePath(USplineComponent* SplineComponent);

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Spline", meta = (DisplayName = "简化样条", CompactNodeTitle = "SimplifySpline"))
	static void SimplifySpline(USplineComponent* SplineComponent, const TArray<FVector>& SplinePath);

};