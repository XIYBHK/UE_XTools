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

#pragma region CurveScale

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Math|Curve",
		meta = (DisplayName = "应用缩放曲线强度",
			Keywords = "缩放 曲线 强度 幅度 倍率 Scale Curve Strength Amount ECF Timeline",
			ToolTip = "将以(1,1,1)为无变化基准的缩放曲线值按强度增强，再乘以动效前的基础缩放。强度0保持基础缩放，1使用曲线原始效果，大于1会平滑推向最小/最大曲线倍率，避免负缩放或极端拉伸。",
			AdvancedDisplay = "MinScaleMultiplier,MaxScaleMultiplier"))
	static FVector ApplyScaleCurveAmplitude(
		UPARAM(DisplayName = "基础缩放") FVector BaseScale = FVector(1.0),
		UPARAM(DisplayName = "曲线缩放") FVector CurveScale = FVector(1.0),
		UPARAM(DisplayName = "强度", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0")) double Strength = 1.0,
		UPARAM(DisplayName = "最小曲线倍率", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0")) double MinScaleMultiplier = 0.0,
		UPARAM(DisplayName = "最大曲线倍率", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "5.0")) double MaxScaleMultiplier = 2.0);

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
