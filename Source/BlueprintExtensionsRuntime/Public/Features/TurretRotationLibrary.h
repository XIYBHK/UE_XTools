#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TurretRotationLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UTurretRotationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Features", meta = (DisplayName = "计算炮塔旋转角度", CompactNodeTitle = "炮塔旋转"))
	static void CalculateRotateDegree
	(
		UPARAM(ref) float& CurrentDegree,
		const FTransform& ShaftTransform,
		const FVector& TargetLocation,
		EAxis::Type RotateAxis,
		float RotateSpeed,
		const FVector2D& RotateRange,
		float DeltaTime
	);
	
};