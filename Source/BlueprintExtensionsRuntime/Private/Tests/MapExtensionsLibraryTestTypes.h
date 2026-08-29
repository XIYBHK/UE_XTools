#pragma once

#include "CoreMinimal.h"
#include "MapExtensionsLibraryTestTypes.generated.h"

USTRUCT()
struct FMapExtensionsTrackedValue
{
	GENERATED_BODY()

	FMapExtensionsTrackedValue();
	FMapExtensionsTrackedValue(const FMapExtensionsTrackedValue& Other);
	FMapExtensionsTrackedValue(FMapExtensionsTrackedValue&& Other) noexcept;
	~FMapExtensionsTrackedValue();

	FMapExtensionsTrackedValue& operator=(const FMapExtensionsTrackedValue& Other) = default;
	FMapExtensionsTrackedValue& operator=(FMapExtensionsTrackedValue&& Other) noexcept = default;

	UPROPERTY()
	FString Text;

	static int32 LiveInstanceCount;
};

USTRUCT()
struct FMapExtensionsTestContainer
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<int32, FMapExtensionsTrackedValue> Values;
};
