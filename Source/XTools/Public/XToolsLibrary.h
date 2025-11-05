/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

//  核心 UE 头文件
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "XToolsLibrary.generated.h"

//  前向声明 - 减少编译依赖
class AActor;
class UBoxComponent;
class UCurveFloat;
class USceneComponent;
class UFormationManagerComponent;

// 新增的采样模式枚举
UENUM(BlueprintType)
enum class EXToolsSamplingMethod : uint8
{
	/** 在模型表面内外一定距离内进行采样，适合生成贴近表面的效果。*/
	SurfaceProximity	UMETA(DisplayName = "表面邻近度采样"),

	/** [待实现] 对模型的内部进行完整的实体填充采样，会填满所有内部空间。*/
	Voxelize			UMETA(DisplayName = "实体填充采样 (待实现)"),

	/** UE原生表面采样：使用FMeshSurfacePointSampling直接在网格表面生成泊松分布的点。性能极高，点分布均匀，自带法线方向。*/
	NativeSurface		UMETA(DisplayName = "原生表面采样 (高性能)")
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

/**
 * 点采样配置
 * 将所有采样参数打包以简化API并提升可维护性
 */
USTRUCT(BlueprintType)
struct FPointSamplingConfig
{
	GENERATED_BODY()

	/** 采样方法：表面邻近度 或 体素化（待实现） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	EXToolsSamplingMethod Method = EXToolsSamplingMethod::SurfaceProximity;

	/** 生成点网格的间距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "0.1"))
	float GridSpacing = 10.0f;

	/** 每个采样点在各轴上的最大随机偏移量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "0.0"))
	float Noise = 0.0f;

	/** [表面邻近度模式] 检测球体半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "0.1"))
	float TraceRadius = 5.0f;

	/** 是否使用复杂碰撞（按多边形） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
	bool bUseComplexCollision = true;

	/** [优化] 是否启用模型边界剔除 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization")
	bool bEnableBoundsCulling = true;

	// --- 调试选项 ---

	/** 是否启用调试绘制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bEnableDebugDraw = false;

	/** 调试绘制时是否仅显示成功命中的点 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawOnlySuccessfulHits = true;

	/** 调试绘制持续时间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.1"))
	float DebugDrawDuration = 5.0f;
};

//  移除重复的前向声明 - 已在上方声明

/**
 * 工具库类
 */
UCLASS(meta = (ToolTip = "贝塞尔曲线、几何采样和编辑器辅助功能的实用函数库"))
class XTOOLS_API UXToolsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 
     * 从组件开始，在Actor附加层级中查找最顶层的匹配父Actor
     * @param StartComponent 要开始查找的起始组件。
     * @param ActorClass 要查找的Actor类 (可选)。
     * @param ActorTag 要匹配的Actor标签 (可选)。
     * @return 找到的最顶层的匹配父Actor，如果未找到则返回nullptr。
     * @note 该函数会从一个组件开始，沿着它的组件附加层级(GetAttachParent)一路向上查找，
     *       并检查每个组件所属的Actor，返回最后一个（即最顶层的）符合所有指定条件的父Actor。
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Actors", 
        meta = (DisplayName = "获取附加的Actor最高级", 
               DeterminesOutputType = "ActorClass",
               ToolTip = "从一个组件开始, 沿其父级链向上查找, 并返回最顶层的匹配父Actor. 注: 采用'从下至上'查找, 因其仅需一次遍历即可确定层级并找到最高匹配项, 是最高效的方式."))
    static AActor* GetTopmostAttachedActor(
        USceneComponent* StartComponent,
        TSubclassOf<AActor> ActorClass,
        FName ActorTag
    );

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
     * @param BaseChance 基础触发概率[0,1]
     * @return 返回一个数组，索引表示失败次数（0-12），值表示在该失败次数下触发成功的次数
     */
    UFUNCTION(BlueprintCallable, Category="XTools|测试", meta=(
        DisplayName = "测试PRD分布",
        BaseChance="基础概率",
        ToolTip="执行一万次PRD随机，统计每个失败次数触发成功的次数。\n返回数组中，索引表示失败次数（0-12），值表示在该失败次数下触发成功的次数"))
    static TArray<int32> TestPRDDistribution(float BaseChance);



    /**
     * 清理点阵生成缓存
     *
     * @return 清理结果信息
     */
    UFUNCTION(BlueprintCallable, Category="XTools|工具", meta=(
        DisplayName = "清理点阵生成缓存",
        ToolTip="清理'在模型中生成点阵'功能的计算缓存\n用途：释放内存、重置计算状态、解决缓存问题\n建议：关卡切换时调用，或遇到点阵生成异常时使用"))
    static FString ClearPointSamplingCache();

    /**
     * 在模型内部生成点阵（推荐使用）
     *
     * 使用配置结构体简化参数，根据选择的采样模式在目标Actor的碰撞体内生成点阵。
     * 
     * @param WorldContextObject 世界上下文对象
     * @param TargetActor 要采样的目标Actor
     * @param BoundingBox 用于定义采样区域的Box组件
     * @param Config 点采样配置（包含所有采样参数）
     * @param OutPoints [输出] 所有符合条件的点的世界坐标数组
     * @param bSuccess [输出] 操作是否成功
     */
    UFUNCTION(BlueprintCallable, Category="XTools|几何", meta=(
        DisplayName = "在模型中生成点阵",
        WorldContext="WorldContextObject",
        ToolTip="使用配置结构体在模型碰撞体内生成点阵\n推荐使用此简化API以提高代码可维护性\n\n[碰撞要求] 目标Actor必须：\n1.有StaticMeshComponent\n2.Collision Enabled=Query Only或Collision Enabled (推荐)\n3.有效的Object Type（自动获取，任意类型即可）\n\n[不影响检测] Collision Response设置（Block/Overlap/Ignore）完全不影响\n原因：使用TraceForObjects（按类型），不是TraceByChannel（按通道）\n\n提示：采样不准确时启用Complex Collision（降低性能但提高精度）"))
    static void SamplePointsInsideMesh(
        const UObject* WorldContextObject,
        AActor* TargetActor,
        UBoxComponent* BoundingBox,
        const FPointSamplingConfig& Config,
        TArray<FVector>& OutPoints,
        bool& bSuccess
    );

    /**
     * 在模型内部生成点阵（传统API，参数较多）
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
     * @param bUseComplexCollision 是否使用复杂碰撞（逐多边形），关闭可在有简单碰撞体时提升性能。
     * @param OutPoints [输出] 所有符合条件的点的世界坐标数组。
     * @param bSuccess [输出] 操作是否成功。
     * 
     * @note 建议使用 SamplePointsInsideMesh(Config) 以获得更好的可维护性
     */
    UFUNCTION(BlueprintCallable, Category="XTools|几何", meta=(
        DisplayName = "在模型中生成点阵(详细参数)",
        WorldContext="WorldContextObject",
        AdvancedDisplay="Noise,TraceRadius,bEnableDebugDraw,bDrawOnlySuccessfulHits,bEnableBoundsCulling,DebugDrawDuration,bUseComplexCollision",
        GridSpacing="10.0",
        Noise="0.0",
        TraceRadius="5.0",
        bEnableDebugDraw="false",
        bDrawOnlySuccessfulHits="true",
        bEnableBoundsCulling="true",
        DebugDrawDuration="5.0",
        ToolTip="传统API：使用独立参数在模型碰撞体内生成点阵\n建议使用SamplePointsInsideMesh(Config)以获得更好的可维护性\n\n[碰撞要求] 目标Actor必须：\n1.有StaticMeshComponent\n2.Collision Enabled=Query Only或Collision Enabled (推荐)\n3.有效的Object Type（自动获取，任意类型即可）\n\n[不影响检测] Collision Response设置（Block/Overlap/Ignore）完全不影响\n原因：使用TraceForObjects（按类型），不是TraceByChannel（按通道）\n\n提示：采样不准确时启用Complex Collision（降低性能但提高精度）"))
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
        bool& bSuccess,
        // --- Optional Input
        bool bUseComplexCollision = true
    );

private:
    // 计算曲线上某点的位置（基于参数t）
    static FVector CalculatePointAtParameter(const TArray<FVector>& Points, float t, TArray<FVector>& OutWorkPoints);
    static FVector EvaluateBezierConstantSpeed(
        UWorld* World,
        const TArray<FVector>& Points,
        float Progress,
        bool bShowDebug,
        float Duration,
        const FBezierDebugColors& DebugColors,
        const FBezierSpeedOptions& SpeedOptions,
        TArray<FVector>& WorkPoints);
    static void DrawBezierDebug(
        UWorld* World,
        const TArray<FVector>& Points,
        const TArray<FVector>& WorkPoints,
        const FBezierDebugColors& DebugColors,
        float Duration,
        const FVector& ResultPoint);

public:


};
