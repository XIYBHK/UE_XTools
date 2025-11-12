#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MathExtensionsLibrary.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UMathExtensionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region StableFrame
	
	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Math|StableFrame", meta = (DisplayName = "稳定帧时间", CompactNodeTitle = "StableFrame"))
	static float StableFrame(float DeltaTime, const TArray<float>& PastDeltaTime);
	
#pragma endregion
	
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region KeepDecimals
	
	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Math|KeepDecimals", meta = (DisplayName = "保留小数(浮点)", CompactNodeTitle = "KeepDcm.Float"))
	static float KeepDecimals_Float(float Value, int32 DecimalPlaces);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Math|KeepDecimals", meta = (DisplayName = "保留小数(字符串)", CompactNodeTitle = "KeepDcm.FStr"))
	static FString KeepDecimals_FloatString(float Value, int32 DecimalPlaces);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Math|KeepDecimals", meta = (DisplayName = "保留小数(Vec2)", CompactNodeTitle = "KeepDcm.Vec2"))
	static FVector2D KeepDecimals_Vec2(FVector2D Value, int32 DecimalPlaces);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Math|KeepDecimals", meta = (DisplayName = "保留小数(Vec3)", CompactNodeTitle = "KeepDcm.Vec3"))
	static FVector KeepDecimals_Vec3(FVector Value, int32 DecimalPlaces);

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Sort
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Math|Sort", meta = (DisplayName = "排序插入浮点数", CompactNodeTitle = "SortInsert", ToolTip = "将浮点数插入到已排序数组的正确位置，保持数组排序状态。使用二分查找定位，适合维护实时排行榜等有序列表。"))
	static void SortInsertFloat(UPARAM(ref) TArray<double>& InOutArray, double InsertElement, bool SortAsendant);

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Units

private:
	
	static void UnitValueScale(FProperty* ValueProperty, void* ValueAddr, double Scale);

	static void UnitValueAcosD(FProperty* ValueProperty, void* ValueAddr);

public:
	
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "XTools|Blueprint Extensions|Math|Units", meta=(DisplayName = "米转UE单位", CompactNodeTitle = "Meter>Unit", CustomStructureParam = "Value"))
	static void MeterToUnit(UPARAM(ref) const int32& Value);
	DECLARE_FUNCTION(execMeterToUnit);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "XTools|Blueprint Extensions|Math|Units", meta=(DisplayName = "英里/时转UE单位", CompactNodeTitle = "MPH>Unit", CustomStructureParam = "Value"))
	static void MphToUnit(UPARAM(ref) const int32& Value);
	DECLARE_FUNCTION(execMphToUnit);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "XTools|Blueprint Extensions|Math|Units", meta=(DisplayName = "余弦转角度", CompactNodeTitle = "COS>Degree", CustomStructureParam = "Value"))
	static void CosToDegree(UPARAM(ref) const int32& Value);
	DECLARE_FUNCTION(execCosToDegree);

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
};