/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "X_CurvePresetLibrary.generated.h"

class UCurveFloat;
class UCurveVector;

/** Editor scripting helpers used to generate curve preset assets. */
UCLASS()
class X_ASSETEDITOR_API UXCurvePresetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "XTools|资产工具|曲线预设",
        meta = (DisplayName = "设置浮点曲线三次键",
            ToolTip = "使用 FVector4 数组重建浮点曲线：X=时间，Y=值，Z=到达切线，W=离开切线。仅供预设资产生成脚本使用。"))
    static bool SetCurveFloatCubicKeys(
        UPARAM(DisplayName = "曲线") UCurveFloat* Curve,
        UPARAM(DisplayName = "键数据") const TArray<FVector4>& Keys);

    UFUNCTION(BlueprintPure, Category = "XTools|资产工具|曲线预设",
        meta = (DisplayName = "检查浮点曲线三次键",
            ToolTip = "检查浮点曲线的键数量、时间、值、切线和三次插值模式是否与键数据一致。仅供预设资产验证脚本使用。"))
    static bool DoesCurveFloatMatchCubicKeys(
        UPARAM(DisplayName = "曲线") const UCurveFloat* Curve,
        UPARAM(DisplayName = "键数据") const TArray<FVector4>& Keys);

    UFUNCTION(BlueprintCallable, Category = "XTools|资产工具|曲线预设",
        meta = (DisplayName = "设置向量曲线平滑三次键",
            ToolTip = "使用 FVector4 数组重建向量曲线：X=时间，Y/Z/W=向量值；内部锚点使用连续零切线。仅供预设资产生成脚本使用。"))
    static bool SetCurveVectorCubicKeys(
        UPARAM(DisplayName = "曲线") UCurveVector* Curve,
        UPARAM(DisplayName = "键数据") const TArray<FVector4>& Keys);

    UFUNCTION(BlueprintPure, Category = "XTools|资产工具|曲线预设",
        meta = (DisplayName = "检查向量曲线三次键",
            ToolTip = "检查向量曲线三轴的键数量、时间、值、零切线和三次插值模式是否与键数据一致。仅供预设资产验证脚本使用。"))
    static bool DoesCurveVectorMatchCubicKeys(
        UPARAM(DisplayName = "曲线") const UCurveVector* Curve,
        UPARAM(DisplayName = "键数据") const TArray<FVector4>& Keys);

    UFUNCTION(BlueprintPure, Category = "XTools|资产工具|曲线预设",
        meta = (DisplayName = "检查向量曲线三次平滑性",
            ToolTip = "检查向量曲线三轴是否使用无权重三次键，并且内部锚点左右切线连续。仅供预设资产验证脚本使用。"))
    static bool IsCurveVectorCubicSmooth(
        UPARAM(DisplayName = "曲线") const UCurveVector* Curve);
};
