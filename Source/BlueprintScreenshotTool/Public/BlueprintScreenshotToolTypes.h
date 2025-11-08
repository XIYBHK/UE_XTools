// Copyright 2024 Gradess Games. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "IImageWrapper.h"
#include "BlueprintScreenshotToolTypes.generated.h"

USTRUCT(BlueprintType)
struct BLUEPRINTSCREENSHOTTOOL_API FBSTScreenshotData
{
	GENERATED_BODY()

	/** 默认构造函数，初始化所有成员 */
	FBSTScreenshotData()
		: Size(0, 0, 0)
	{
	}

	/** 带参数的构造函数，用于便捷初始化 */
	FBSTScreenshotData(const TArray<FColor>& InColorData, const FIntVector& InSize)
		: ColorData(InColorData)
		, Size(InSize)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screenshot Data")
	TArray<FColor> ColorData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screenshot Data")
	FIntVector Size;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screenshot Data")
	FString CustomName;

	FORCEINLINE FIntPoint GetPointSize() const
	{
		return FIntPoint(Size.X, Size.Y);
	}

	FORCEINLINE bool IsValid() const
	{
		return ColorData.Num() > 0 && Size.X > 0 && Size.Y > 0;
	}
};

UENUM()
enum class EBSTImageFormat : uint8
{
	PNG = static_cast<uint8>(EImageFormat::PNG) UMETA(DisplayName = "PNG"),
	JPG = static_cast<uint8>(EImageFormat::JPEG) UMETA(DisplayName = "JPG"),
};
