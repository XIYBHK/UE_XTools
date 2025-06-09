// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Curves/CurveFloat.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "XToolsLibrary.generated.h"

// 新增的采样模式枚举
UENUM(BlueprintType)
enum class EXToolsSamplingMethod : uint8
{
	/** 在模型表面内外一定距离内进行采样，适合生成贴近表面的效果。*/
	SurfaceProximity	UMETA(DisplayName = "表面邻近度采样"),

	/** [待实现] 对模型的内部进行完整的实体填充采样，会填满所有内部空间。*/
	Voxelize			UMETA(DisplayName = "实体填充采样 (待实现)")
};

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

class UBoxComponent;

/**
 * 工具库类
 */
UCLASS()
class XTOOLS_API UXToolsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 
     * 按类查找父Actor
     * @param Component 要开始查找的起始组件，通常是一个场景组件（SceneComponent）
     * @param ActorClass 要查找的Actor类，必须是AActor的子类
     * @param ActorTag 要匹配的父Actor标签，为空时返回最高级父级
     * @return 找到的父Actor，如果未找到则返回nullptr
     * @note 该函数会沿着组件的父子层级向上查找，直到找到匹配指定类和标签的父Actor
     * @example 
     * // 查找标签为"MainCharacter"的父级Character
     * ACharacter* ParentCharacter = Cast<ACharacter>(
     *     UXToolsLibrary::FindParentComponentByClass(MyComponent, ACharacter::StaticClass(), "MainCharacter"));
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Components", 
        meta = (DisplayName = "按类查找父组件", 
               DeterminesOutputType = "ActorClass",
               ToolTip = "从SceneComponent开始，沿组件层级向上查找父Actor。\n规则：\n1. 同时指定类和标签时，返回第一个同时匹配的父级\n2. 只指定类时，返回最高级匹配的父级\n3. 只指定标签时，返回第一个匹配的父级\n4. 都未指定时，返回最顶层的父级\nComponent: 起始查找的SceneComponent\nActorClass: 要查找的Actor类\nActorTag: 要匹配的父Actor标签（可选）\n返回: 找到的父Actor，如果未找到则返回nullptr"))
    static AActor* FindParentComponentByClass(UActorComponent* Component, TSubclassOf<AActor> ActorClass, const FString& ActorTag = TEXT(""));

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
               WorldContext="Context",
               Duration = 0.03))
    static FVector CalculateBezierPoint(const UObject* Context,UPARAM(ref) const TArray<FVector>& Points, 
                                      float Progress, 
                                      bool bShowDebug, 
                                      float Duration = 0.03, 
                                      FBezierDebugColors DebugColors = FBezierDebugColors(),
                                      FBezierSpeedOptions SpeedOptions = FBezierSpeedOptions());

    /** 
     * 测试PRD算法的分布情况。
     * 执行一万次PRD随机，统计每个失败次数触发成功的次数。
     *
     *@param	BaseChance		基础触发概率[0,1]
     *@return	返回一个数组，索引表示失败次数（0-10），值表示在该失败次数下触发成功的次数
    */
    UFUNCTION(BlueprintCallable, Category="XTools|测试", meta=(
        DisplayName = "测试PRD分布",
        BaseChance="基础概率",
        ToolTip="执行一万次PRD随机，统计每个失败次数触发成功的次数。\n返回数组中，索引表示失败次数（0-10），值表示在该失败次数下触发成功的次数"))
    static TArray<int32> TestPRDDistribution(float BaseChance);

    /**
     * 在静态模型内部或表面生成点阵
     *
     * 根据选择的采样模式，在目标Actor的碰撞体内生成点阵。
     * 
     * @param WorldContextObject 世界上下文对象。
     * @param TargetActor 要采样的目标Actor。
     * @param BoundingBox 用于定义采样区域的Box组件。
     * @param Method 采样模式：表面邻近度 或 实体填充(待实现)。
     * @param GridSpacing 生成点阵的间距。
     * @param Noise 每个采样点在各个轴上的最大随机偏移量，用于打破网格的规律性。
     * @param TraceRadius [表面邻近度模式] 检测球体的半径。
     * @param bEnableDebugDraw 是否启用调试绘制。
     * @param bDrawOnlySuccessfulHits 调试绘制时是否只显示成功命中的点。
     * @param bEnableBoundsCulling [优化] 是否启用模型包围盒剔除，可大幅提升大范围采样时的性能。
     * @param DebugDrawDuration 调试绘制持续时间。
     * @param OutPoints [输出] 所有符合条件的点的世界坐标数组。
     * @param bSuccess [输出] 操作是否成功。
     */
    UFUNCTION(BlueprintCallable, Category="XTools|几何", meta=(
        DisplayName = "在模型中生成点阵",
        WorldContext="WorldContextObject",
        AdvancedDisplay="Noise,TraceRadius,bEnableDebugDraw,bDrawOnlySuccessfulHits,bEnableBoundsCulling,DebugDrawDuration",
        GridSpacing="10.0",
        Noise="0.0",
        TraceRadius="5.0",
        bEnableDebugDraw="false",
        bDrawOnlySuccessfulHits="true",
        bEnableBoundsCulling="true",
        DebugDrawDuration="5.0",
        ToolTip="根据选择的采样模式，在目标Actor的碰撞体内生成点阵。\n\n@param TargetActor 要采样的目标Actor。\n@param BoundingBox 用于定义采样区域的Box组件。\n@param Method 采样模式：表面邻近度 或 实体填充(待实现)。\n@param GridSpacing 生成点阵的间距。\n@param Noise 每个采样点在各个轴上的最大随机偏移量，用于打破网格的规律性。\n@param TraceRadius [表面邻近度模式] 检测球体的半径。\n@param bEnableDebugDraw 是否启用调试绘制。\n@param bDrawOnlySuccessfulHits 调试绘制时是否只显示成功命中的点。\n@param bEnableBoundsCulling [优化] 是否启用模型包围盒剔除，可大幅提升大范围采样时的性能。\n@param DebugDrawDuration 调试绘制持续时间。\n@param OutPoints [输出] 所有符合条件的点的世界坐标数组。\n@param bSuccess [输出] 操作是否成功。"))
    static void SamplePointsInsideStaticMeshWithBoxOptimized(
        // --- Inputs
        const UObject* WorldContextObject,
        AActor* TargetActor,
        UBoxComponent* BoundingBox,
        EXToolsSamplingMethod Method,
        float GridSpacing,
        float Noise,
        float TraceRadius,
        bool bEnableDebugDraw,
        bool bDrawOnlySuccessfulHits,
        bool bEnableBoundsCulling,
        float DebugDrawDuration,
        // --- Outputs
        TArray<FVector>& OutPoints,
        bool& bSuccess
    );

private:
    // 计算曲线上某点的位置（基于参数t）
    static FVector CalculatePointAtParameter(const TArray<FVector>& Points, float t, TArray<FVector>& OutWorkPoints);
    
    // 计算曲线总长度
    static float CalculateCurveLength(const TArray<FVector>& Points, int32 Segments = 100);
    
    // 根据距离获取参数t
    static float GetParameterByDistance(const TArray<FVector>& Points, float Distance, float TotalLength, int32 Segments = 100);
    
};
