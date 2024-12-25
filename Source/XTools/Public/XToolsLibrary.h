// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Curves/CurveFloat.h"
#include "XToolsLibrary.generated.h"

// 贝塞尔曲线速度模式
UENUM(BlueprintType)
enum class EBezierSpeedMode : uint8
{
    // 默认模式（参数化t值）
    Default UMETA(DisplayName = "默认"),
    // 匀速模式
    Constant UMETA(DisplayName = "匀速"),
};

// 贝塞尔曲线调试颜色配置
USTRUCT(BlueprintType)
struct FBezierDebugColors
{
    GENERATED_BODY()

    // 控制点颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "控制点颜色"))
    FLinearColor ControlPointColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);

    // 控制线颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "控制线颜色"))
    FLinearColor ControlLineColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);

    // 中间点颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "中间点颜色"))
    FLinearColor IntermediatePointColor = FLinearColor(0.7f, 0.9f, 0.7f, 1.0f);

    // 中间线颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "中间线颜色"))
    FLinearColor IntermediateLineColor = FLinearColor(0.0f, 1.0f, 0.38f, 1.0f);

    // 结果点颜色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Colors", meta = (DisplayName = "结果点颜色"))
    FLinearColor ResultPointColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
};

// 贝塞尔曲线运动参数
USTRUCT(BlueprintType)
struct FBezierSpeedOptions
{
    GENERATED_BODY()

    // 速度模式
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (DisplayName = "速度模式"))
    EBezierSpeedMode SpeedMode = EBezierSpeedMode::Default;

    // 速率曲线（用于调整运动速率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (DisplayName = "速率曲线"))
    UCurveFloat* SpeedCurve = nullptr;
};

/**
 * 工具库类
 */
UCLASS()
class XTOOLS_API UXToolsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 计算贝塞尔曲线上的点
     * @param Points - 控制点数组
     * @param Progress - 计算进度(0-1)
     * @param bShowDebug - 是否显示调试
     * @param Duration - 调试显示持续时间
     * @param DebugColors - 调试颜色配置
     * @param SpeedOptions - 速度选项
     * @return 贝塞尔曲线上的点
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Bezier", 
        meta = (DisplayName = "计算贝塞尔曲线点", 
               Duration = 0.03))
    static FVector CalculateBezierPoint(UPARAM(ref) const TArray<FVector>& Points, 
                                      float Progress, 
                                      bool bShowDebug, 
                                      float Duration = 0.03, 
                                      FBezierDebugColors DebugColors = FBezierDebugColors(),
                                      FBezierSpeedOptions SpeedOptions = FBezierSpeedOptions());

private:
    // 计算曲线上某点的位置（基于参数t）
    static FVector CalculatePointAtParameter(const TArray<FVector>& Points, float t, TArray<FVector>& OutWorkPoints);
    
    // 计算曲线总长度
    static float CalculateCurveLength(const TArray<FVector>& Points, int32 Segments = 100);
    
    // 根据距离获取参数t
    static float GetParameterByDistance(const TArray<FVector>& Points, float Distance, float TotalLength, int32 Segments = 100);
};