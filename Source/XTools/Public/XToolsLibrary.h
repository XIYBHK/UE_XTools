// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 核心 UE 头文件
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "XToolsLibrary.generated.h"

// ✅ 前向声明 - 减少编译依赖
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

// ✅ 移除重复的前向声明 - 已在上方声明

/**
 * 工具库类
 */
UCLASS()
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
     * 生成泊松圆盘采样点（2D）
     * 使用泊松圆盘采样算法在指定区域内生成均匀分布的随机点，确保点之间的最小距离。
     * 
     * @param Width 采样区域的宽度
     * @param Height 采样区域的高度
     * @param Radius 点之间的最小距离
     * @param MaxAttempts 在标记一个点为不活跃前的最大尝试次数（默认15）
     * @return 生成的2D点数组
     * 
     * 示例用途：
     * - 程序化生成均匀分布的植被/道具位置
     * - 粒子系统的初始位置
     * - UI元素的布局
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|算法", 
        meta = (DisplayName = "生成泊松采样点2D",
               ToolTip = "2D平面泊松采样，生成均匀分布点\n\n参数：Width/Height(区域), Radius(点间距), MaxAttempts(尝试次数)\n用途：植被分布、粒子位置、UI布局"))
    static TArray<FVector2D> GeneratePoissonPoints2D(
        float Width = 1000.0f, 
        float Height = 1000.0f, 
        float Radius = 50.0f, 
        int32 MaxAttempts = 15
    );

    /**
     * 生成泊松圆盘采样点（3D）
     * 使用泊松圆盘采样算法在3D空间中生成均匀分布的随机点。
     * 
     * @param Width X轴范围
     * @param Height Y轴范围  
     * @param Depth Z轴范围
     * @param Radius 点之间的最小距离
     * @param MaxAttempts 在标记一个点为不活跃前的最大尝试次数（默认30）
     * @return 生成的3D点数组
     * 
     * 示例用途：
     * - 3D空间中的物体分布
     * - 星空生成
     * - 粒子云效果
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|算法",
        meta = (DisplayName = "生成泊松采样点3D",
               ToolTip = "3D空间泊松采样，生成均匀分布点\n\n参数：Width/Height/Depth(区域), Radius(点间距), MaxAttempts(尝试次数)\n用途：物体分布、星空生成、体积填充"))
    static TArray<FVector> GeneratePoissonPoints3D(
        float Width = 1000.0f,
        float Height = 1000.0f,
        float Depth = 1000.0f,
        float Radius = 50.0f,
        int32 MaxAttempts = 30
    );

    /**
     * 在盒体范围内生成泊松采样点（自动2D/3D）
     * 直接使用BoxComponent的范围生成泊松分布点，支持结果缓存
     * 
     * @param BoundingBox 用于定义采样区域的Box组件
     * @param Radius 点之间的最小距离（<=0 时根据目标点数自动计算）
     * @param MaxAttempts 在标记一个点为不活跃前的最大尝试次数（默认30，构造函数建议5-10）
     * @param bWorldSpace 是否返回世界坐标（true=世界坐标, false=局部坐标）
     * @param TargetPointCount 目标点数量（<=0时忽略，由Radius控制；>0时自动计算Radius并严格裁剪到目标数量）
     * @param bUseCache 是否使用结果缓存（构造函数建议开启，大幅提升重复调用性能。⚠️注意：启用后Actor移动不会触发数据更新）
     * @return 生成的点数组（局部坐标或世界坐标）
     * 
     * 智能特性：
     * - Z轴为0时自动切换为2D平面采样（XY平面）
     * - Z轴>0时使用3D体积采样
     * - 支持通过 Radius 或 TargetPointCount 控制点数量（TargetPointCount严格遵循）
     * - 结果缓存：相同参数直接返回缓存结果，几乎无开销
     * 
     * 控制点数的两种方式：
     * 1. 方式A：指定 Radius（点间距）→ 点数由算法自动生成
     * 2. 方式B：指定 TargetPointCount（目标点数）→ 自动计算合适的Radius
     * 
     * 性能优化建议：
     * - 构造函数使用：MaxAttempts=5-10, bUseCache=true
     * - 运行时使用：MaxAttempts=15-30, bUseCache=false
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|算法",
        meta = (DisplayName = "在盒体内生成泊松采样点",
               AdvancedDisplay = "TargetPointCount,JitterStrength,bUseCache",
               ToolTip = "盒体泊松采样（自动2D/3D，支持缓存）\n\n核心参数：\n• Radius: 点间距（<=0则由TargetPointCount计算）\n• TargetPointCount: 严格遵循目标点数（>0时裁剪到精确数量）\n• JitterStrength: 扰动强度0-1（0=规则分布，1=最大随机偏移）\n• bWorldSpace: true=世界坐标（位置+旋转）, false=局部坐标（仅旋转）\n• bUseCache: 启用缓存（注意：Actor移动不会更新）"))
    static TArray<FVector> GeneratePoissonPointsInBox(
        UBoxComponent* BoundingBox,
        float Radius = 50.0f,
        int32 MaxAttempts = 30,
        bool bWorldSpace = false,
        int32 TargetPointCount = 0,
        float JitterStrength = 0.0f,
        bool bUseCache = true
    );

    /**
     * 在指定范围内生成泊松采样点（向量参数版本）
     * 使用向量直接定义采样范围，不依赖Box组件
     * 
     * @param BoxExtent 盒体半尺寸（直接连接GetScaledBoxExtent节点，内部会×2得到完整范围）
     * @param Transform 盒体的变换（位置、旋转、缩放。⚠️注意：缩放不会重复应用，因为BoxExtent已包含缩放）
     * @param Radius 点之间的最小距离（<=0 时根据目标点数自动计算）
     * @param MaxAttempts 在标记一个点为不活跃前的最大尝试次数（默认30）
     * @param bWorldSpace 是否返回世界坐标（true=世界坐标，false=局部坐标）
     * @param TargetPointCount 目标点数量（<=0时忽略，由Radius控制；>0时自动计算Radius并严格裁剪到目标数量）
     * @param bUseCache 是否使用结果缓存（默认true。⚠️注意：启用后Actor移动不会触发数据更新）
     * @return 生成的点数组
     * 
     * 使用方法：
     * - 直接连接 GetScaledBoxExtent → BoxExtent（半尺寸会自动×2处理）
     * - 缓存包含旋转信息，正确处理Transform变化
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|算法",
        meta = (DisplayName = "在范围内生成泊松采样点（向量版）",
               AdvancedDisplay = "TargetPointCount,JitterStrength,bUseCache",
               ToolTip = "向量参数泊松采样（不依赖Box组件）\n\n参数：\n• BoxExtent: 半尺寸（直接连GetScaledBoxExtent）\n• Transform: 变换（缩放已含在Extent中）\n• TargetPointCount: 严格遵循目标点数（>0时裁剪到精确数量）\n• JitterStrength: 扰动强度0-1（0=规则分布，1=最大随机偏移）\n• bWorldSpace: true=世界坐标（位置+旋转）, false=局部坐标（仅旋转）\n• bUseCache: 启用缓存（注意：Actor移动不更新）"))
    static TArray<FVector> GeneratePoissonPointsInBoxByVector(
        FVector BoxExtent,
        FTransform Transform,
        float Radius = 50.0f,
        int32 MaxAttempts = 30,
        bool bWorldSpace = false,
        int32 TargetPointCount = 0,
        float JitterStrength = 0.0f,
        bool bUseCache = true
    );

    /**
     * 从流送在盒体内生成泊松采样点（确定性随机）
     * 
     * 使用 FRandomStream 进行确定性随机采样，相同种子产生相同结果。
     * 不使用缓存，每次调用都会重新生成。
     */
    UFUNCTION(BlueprintPure, Category = "XTools|算法",
        meta = (DisplayName = "从流送在盒体内生成泊松采样点",
               ScriptMethod = "GeneratePoissonPointsInBox",
               ScriptMethodMutable,
               AdvancedDisplay = "TargetPointCount,JitterStrength",
               ToolTip = "确定性泊松采样（相同种子=相同结果）\n\n参数：\n• RandomStream: 随机流（控制种子）\n• BoundingBox: 采样区域\n• TargetPointCount: 严格遵循目标点数\n• JitterStrength: 扰动强度0-1（0=规则分布，1=最大随机偏移）\n• bWorldSpace: true=世界坐标, false=局部坐标"))
    static TArray<FVector> GeneratePoissonPointsInBoxFromStream(
        const FRandomStream& RandomStream,
        UBoxComponent* BoundingBox,
        float Radius = 50.0f,
        int32 MaxAttempts = 30,
        bool bWorldSpace = false,
        int32 TargetPointCount = 0,
        float JitterStrength = 0.0f
    );

    /**
     * 从流送在范围内生成泊松采样点（向量版，确定性随机）
     * 
     * 使用 FRandomStream 进行确定性随机采样，相同种子产生相同结果。
     * 不使用缓存，每次调用都会重新生成。
     */
    UFUNCTION(BlueprintPure, Category = "XTools|算法",
        meta = (DisplayName = "从流送在范围内生成泊松采样点（向量版）",
               ScriptMethod = "GeneratePoissonPointsInBoxByVector",
               ScriptMethodMutable,
               AdvancedDisplay = "TargetPointCount,JitterStrength",
               ToolTip = "确定性泊松采样-向量版（相同种子=相同结果）\n\n参数：\n• RandomStream: 随机流（控制种子）\n• BoxExtent: 半尺寸（连GetScaledBoxExtent）\n• Transform: 变换\n• TargetPointCount: 严格遵循目标点数\n• JitterStrength: 扰动强度0-1（0=规则分布，1=最大随机偏移）\n• bWorldSpace: true=世界坐标, false=局部坐标"))
    static TArray<FVector> GeneratePoissonPointsInBoxByVectorFromStream(
        const FRandomStream& RandomStream,
        FVector BoxExtent,
        FTransform Transform,
        float Radius = 50.0f,
        int32 MaxAttempts = 30,
        bool bWorldSpace = false,
        int32 TargetPointCount = 0,
        float JitterStrength = 0.0f
    );

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
     * @param bUseComplexCollision 是否使用复杂碰撞（逐多边形），关闭可在有简单碰撞体时提升性能。
     * @param OutPoints [输出] 所有符合条件的点的世界坐标数组。
     * @param bSuccess [输出] 操作是否成功。
     */
    UFUNCTION(BlueprintCallable, Category="XTools|几何", meta=(
        DisplayName = "在模型中生成点阵",
        WorldContext="WorldContextObject",
        AdvancedDisplay="Noise,TraceRadius,bEnableDebugDraw,bDrawOnlySuccessfulHits,bEnableBoundsCulling,DebugDrawDuration,bUseComplexCollision",
        GridSpacing="10.0",
        Noise="0.0",
        TraceRadius="5.0",
        bEnableDebugDraw="false",
        bDrawOnlySuccessfulHits="true",
        bEnableBoundsCulling="true",
        DebugDrawDuration="5.0",
        ToolTip="根据选择的采样模式，在目标Actor的碰撞体内生成点阵。\n\n@param TargetActor 要采样的目标Actor。\n@param BoundingBox 用于定义采样区域的Box组件。\n@param Method 采样模式：表面邻近度 或 实体填充(待实现)。\n@param GridSpacing 生成点阵的间距。\n@param Noise 每个采样点在各个轴上的最大随机偏移量，用于打破网格的规律性。\n@param TraceRadius [表面邻近度模式] 检测球体的半径。\n@param bUseComplexCollision 是否使用复杂碰撞（逐多边形），关闭可在有简单碰撞体时提升性能。\n@param bEnableDebugDraw 是否启用调试绘制。\n@param bDrawOnlySuccessfulHits 调试绘制时是否只显示成功命中的点。\n@param bEnableBoundsCulling [优化] 是否启用模型包围盒剔除，可大幅提升大范围采样时的性能。\n@param DebugDrawDuration 调试绘制持续时间。\n@param OutPoints [输出] 所有符合条件的点的世界坐标数组。\n@param bSuccess [输出] 操作是否成功。"))
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

public:


};
