#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TransformExtensionsLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UTransformExtensionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

#pragma region TransformMember
	
	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Transform", meta = (DisplayName = "获取位置", CompactNodeTitle = "位置"))
	static FVector TLocation(const FTransform& Transform);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Transform", meta = (DisplayName = "获取旋转", CompactNodeTitle = "旋转"))
	static FRotator TRotation(const FTransform& Transform);
	
#pragma endregion

#pragma region TransformDirection
	
	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Transform", meta = (DisplayName = "获取前向轴", CompactNodeTitle = "前向轴"))
	static FVector AxisForward(const FTransform& Transform);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Transform", meta = (DisplayName = "获取右向轴", CompactNodeTitle = "右向轴"))
	static FVector AxisRight(const FTransform& Transform);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Transform", meta = (DisplayName = "获取上向轴", CompactNodeTitle = "上向轴"))
	static FVector AxisUp(const FTransform& Transform);

#pragma endregion

};
