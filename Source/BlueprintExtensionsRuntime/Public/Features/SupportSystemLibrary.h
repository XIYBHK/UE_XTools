#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "SupportSystemLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API USupportSystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Features", meta = (DisplayName = "获取局部支点变换", CompactNodeTitle = "局部支点"))
	static TArray<FTransform> GetLocalFulcrumTransform(const UPrimitiveComponent* TargetComponent, const FVector& PlaneBase);
	
	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Features", meta = (DisplayName = "获取世界支点变换", CompactNodeTitle = "世界支点"))
	static TArray<FTransform> GetWorldFulcrumTransform(const FTransform& ObjectTransform, const TArray<FTransform>& FulcrumTransformArray);
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Features", meta = (DisplayName = "稳定高度", CompactNodeTitle = "稳定高度", WorldContext = "WorldContextObject"))
	static void StabilizeHeight
	(
		UObject* WorldContextObject,
		UPrimitiveComponent* TargetComponent,
		const TArray<FTransform>& WorldFulcrumTransform,
		float StableHeight,
		float GripHeight,
		float GripStrength,
		float DeltaTime,
		float ErrorRange,
		UPARAM(ref) TArray<float>& LastError,
		UPARAM(ref) TArray<float>& IntegralError,
		float Kp,
		float Ki,
		float Kd,
		ETraceTypeQuery ChannelType,
		float& AveragePressureFactor, 
		FVector& AverageImpactNormal, 
		bool DrawDebug
	); 
	
};