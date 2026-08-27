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

	/** UE 原生表面采样：使用 FMeshSurfacePointSampling 直接在网格表面生成泊松分布的点。性能高、分布均匀、自带法线方向。注意：仅在编辑器中可用（依赖 MeshDescription）。*/
	NativeSurface		UMETA(DisplayName = "原生表面采样 (仅编辑器)")
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

	// 将输入时间进度映射为路径进度；曲线斜率表示相对速度。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed",
		meta = (DisplayName = "进度映射曲线",
			ToolTip = "横轴为输入进度，纵轴为路径进度。通常应从(0,0)单调递增到(1,1)，曲线斜率表示相对速度。"))
	UCurveFloat* SpeedCurve = nullptr;
};

/** 贝塞尔轨迹的二维平滑噪声参数。横向和纵向振幅相同时对应原始导弹方案。 */
USTRUCT(BlueprintType)
struct FBezierNoiseOptions
{
    GENERATED_BODY()

    /** 沿最小旋转标架右轴、上轴的噪声振幅。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "贝塞尔|噪声",
        meta = (DisplayName = "噪声振幅", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
    FVector2D Amplitude = FVector2D(100.0, 100.0);

    /** 每秒噪声变化频率。噪声按采样时间变化，不受飞行总时长影响。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "贝塞尔|噪声",
        meta = (DisplayName = "噪声频率", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "Hz"))
    float Frequency = 2.0f;

    /** 右轴噪声通道种子。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "贝塞尔|噪声", meta = (DisplayName = "横向噪声种子"))
    int32 SeedX = 17;

    /** 上轴噪声通道种子。应与横向种子不同。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "贝塞尔|噪声", meta = (DisplayName = "纵向噪声种子"))
    int32 SeedY = 83;
};

/**
 * Double Reflection 最小旋转标架（RMF）的跨帧朝向状态。
 * 为每枚导弹保留一个变量，并将它作为可变引用传给轨迹节点；节点会原地更新状态。
 * 状态同时持有匀速模式的弧长缓存；内部字段不向蓝图公开，首帧或重新开始轨迹前使用重置节点初始化。
 */
USTRUCT(BlueprintType, meta = (DisplayName = "贝塞尔旋转标架状态"))
struct FBezierRotationFrameState
{
    GENERATED_BODY()

    bool bInitialized = false;

    /** 上一帧未叠加噪声的贝塞尔位置。 */
    FVector PreviousCurvePosition = FVector::ZeroVector;

    /** 上一帧基准曲线的单位切线。 */
    FVector PreviousTangent = FVector::ForwardVector;

    /** 上一帧基准曲线的单位上方向；未初始化时可用它指定初始滚转方向。 */
    FVector PreviousNormal = FVector::UpVector;

    /** 上一帧叠加噪声后的最终位置，用于计算实际移动方向。 */
    FVector PreviousOutputPosition = FVector::ZeroVector;

    /** 匀速模式建立缓存时使用的控制点快照。 */
    TArray<FVector> CachedArcLengthControlPoints;

    /** 均匀参数采样对应的累计弧长；仅在控制点变化时重建。 */
    TArray<float> CachedCumulativeArcLengths;

    float CachedTotalArcLength = 0.0f;

    /** 用于验证缓存生命周期，不参与轨迹计算。 */
    int32 ArcLengthCacheBuildGeneration = 0;

    void Reset(const FVector& InitialUpDirection = FVector::UpVector)
    {
        bInitialized = false;
        PreviousCurvePosition = FVector::ZeroVector;
        PreviousTangent = FVector::ForwardVector;
        PreviousNormal = !InitialUpDirection.ContainsNaN() && !InitialUpDirection.IsNearlyZero()
            ? InitialUpDirection.GetSafeNormal()
            : FVector::UpVector;
        PreviousOutputPosition = FVector::ZeroVector;
        CachedArcLengthControlPoints.Reset();
        CachedCumulativeArcLengths.Reset();
        CachedTotalArcLength = 0.0f;
        ArcLengthCacheBuildGeneration = 0;
    }
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "500.0", ForceUnits = "cm"))
	float GridSpacing = 10.0f;

	/** 每个采样点在各轴上的最大随机偏移量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0", ForceUnits = "cm"))
	float Noise = 0.0f;

	/** [表面邻近度模式] 检测球体半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "100.0", ForceUnits = "cm"))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (EditCondition = "bEnableDebugDraw"))
	bool bDrawOnlySuccessfulHits = true;

	/** 调试绘制持续时间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "30.0", ForceUnits = "s", EditCondition = "bEnableDebugDraw"))
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
     * 从组件开始，在附加层级中查找最顶层的匹配父Actor
     * @param StartComponent 要开始查找的起始组件。
     * @param ActorClass 要查找的Actor类 (可选)。
     * @param ActorTag 要匹配的Actor标签 (可选)。
     * @return 找到的最顶层的匹配父Actor，如果未找到则返回nullptr。
     * @note 该函数会从任意SceneComponent开始，沿着组件附加层级(GetAttachParent)一路向上查找，
     *       包含起始组件所属Actor，并返回最后一个（即最顶层的）符合所有指定条件的Actor。
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Actor|附加关系",
        meta = (DisplayName = "获取最高附加父Actor",
               DeterminesOutputType = "ActorClass",
               ToolTip = "从任意场景组件开始，沿组件附加父级链向上查找，包含自身Actor，并返回最顶层的匹配Actor。可按Actor类和Actor标签过滤。"))
    static AActor* GetTopmostAttachedActor(
        UPARAM(DisplayName = "起始组件") USceneComponent* StartComponent,
        UPARAM(DisplayName = "Actor类") TSubclassOf<AActor> ActorClass,
        UPARAM(DisplayName = "Actor标签") FName ActorTag
    );

    /**
     * 获取所有附加的子Actor（递归查找）
     * 使用C++原生迭代遍历，性能比蓝图快，且不会触发无限循环报错。
     * 
     * @param ParentActor 要查找的父级Actor
     * @param OutAllChildren 输出的所有子级Actor（包含子级的子级）
     * @param bIncludeSelf 结果是否包含ParentActor自身
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Actor|附加关系",
        meta = (DisplayName = "获取所有附加子Actor",
               Keywords = "Get Attached Child Children Recursively BFS",
               ToolTip = "递归获取所有附加的子Actor（包括子级的子级）。使用C++迭代遍历并自动去重，适用于复杂层级结构的Actor查找。"))
    static void GetAllAttachedActorsRecursively(
        UPARAM(DisplayName = "父级Actor") AActor* ParentActor,
        UPARAM(DisplayName = "所有子Actor") TArray<AActor*>& OutAllChildren,
        UPARAM(DisplayName = "包含自身") bool bIncludeSelf = false
    );

    /** 纯计算贝塞尔曲线点，不访问世界也不产生调试绘制。 */
    UFUNCTION(BlueprintPure, Category = "XTools|贝塞尔",
        meta = (DisplayName = "计算贝塞尔曲线点",
                ReturnDisplayName = "曲线点",
                Keywords = "贝塞尔 曲线 求值 De Casteljau Bezier Evaluate",
                ToolTip = "按进度计算任意阶贝塞尔曲线点。速度选项可先映射进度，并可选择弧长近似匀速；该节点不访问世界或绘制调试图形。\n\n性能提示：此节点每次调用将重建弧长表，请勿在 Tick 中逐帧调用；需要高频使用请改用带状态的导弹轨迹接口。"))
    static FVector CalculateBezierPoint(
        UPARAM(ref, DisplayName = "控制点") const TArray<FVector>& Points,
        UPARAM(DisplayName = "进度") float Progress,
        UPARAM(DisplayName = "速度选项") FBezierSpeedOptions SpeedOptions = FBezierSpeedOptions());

    /** 计算贝塞尔曲线点，并在指定世界中绘制本次求值过程。 */
    UFUNCTION(BlueprintCallable, Category = "XTools|贝塞尔",
        meta = (DisplayName = "计算并绘制贝塞尔曲线点",
                WorldContext = "Context",
                AdvancedDisplay = "Duration,DebugColors,SpeedOptions",
                ReturnDisplayName = "曲线点",
                Keywords = "贝塞尔 曲线 调试 绘制 De Casteljau Bezier Debug Draw",
                ToolTip = "按进度计算任意阶贝塞尔曲线点，并绘制控制点、控制线和本次De Casteljau求值过程。"))
    static FVector CalculateAndDrawBezierPoint(
        const UObject* Context,
        UPARAM(ref, DisplayName = "控制点") const TArray<FVector>& Points,
        UPARAM(DisplayName = "进度") float Progress,
        UPARAM(DisplayName = "调试持续时间", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s")) float Duration = 0.03f,
        UPARAM(DisplayName = "调试颜色") FBezierDebugColors DebugColors = FBezierDebugColors(),
        UPARAM(DisplayName = "速度选项") FBezierSpeedOptions SpeedOptions = FBezierSpeedOptions());

    /**
     * 根据发射方向和目标表面法线生成三次贝塞尔导弹轨迹的四个控制点。
     */
    UFUNCTION(BlueprintPure, Category = "XTools|贝塞尔",
        meta = (DisplayName = "生成贝塞尔导弹控制点",
                ReturnDisplayName = "控制点",
                Keywords = "贝塞尔 导弹 控制点 发射方向 目标法线 Cubic Bezier Missile",
                ToolTip = "生成三次贝塞尔导弹控制点。目标法线表示目标端控制柄方向，常用命中表面法线或希望从目标点向外偏出的方向；方向会自动单位化，无效方向自动回退。"))
    static TArray<FVector> BuildBezierMissileControlPoints(
        UPARAM(DisplayName = "起点位置") FVector StartLocation,
        UPARAM(DisplayName = "起点方向") FVector StartDirection,
        UPARAM(DisplayName = "目标位置") FVector TargetLocation,
        UPARAM(DisplayName = "目标法线",
            meta = (ToolTip = "目标端控制柄方向。常用命中表面法线，或希望曲线在目标端向外偏出的方向。")) FVector TargetNormal,
        UPARAM(DisplayName = "起点控制距离") float StartControlDistance,
        UPARAM(DisplayName = "目标控制距离") float TargetControlDistance);

    /** 重置一枚导弹持有的最小旋转标架状态，可指定初始上方向。 */
    UFUNCTION(BlueprintCallable, Category = "XTools|贝塞尔",
        meta = (DisplayName = "重置贝塞尔导弹轨迹状态",
                Keywords = "贝塞尔 导弹 重置 状态 初始上方向 Reset Missile State",
                ToolTip = "清除上一条轨迹的标架记录和匀速缓存。首次使用可省略；重新发射、更换轨迹或对象池复用前应调用。初始上方向用于确定首帧滚转。"))
    static void ResetBezierMissileTrajectoryState(
        UPARAM(ref, DisplayName = "标架状态") FBezierRotationFrameState& InOutFrame,
        UPARAM(DisplayName = "初始上方向") FVector InitialUpDirection = FVector::UpVector);

    /**
     * 计算带端点衰减噪声和最小旋转标架的贝塞尔轨迹。
     * Progress 经速率曲线后驱动端点包络，再按速度模式映射到曲线参数；ElapsedTime 只驱动噪声相位。
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|贝塞尔",
        meta = (DisplayName = "计算贝塞尔导弹轨迹",
                WorldContext = "Context",
                AdvancedDisplay = "SpeedOptions,bShowDebug,Duration,DebugColors",
                Keywords = "贝塞尔 导弹 噪声 最小旋转标架 Double Reflection RMF Missile Noise",
                ToolTip = "按进度计算带平滑噪声的贝塞尔导弹位置和朝向。进度控制轨迹位置及端点衰减，噪声采样时间（秒）控制噪声变化；每枚导弹应持续传入自己的标架状态。"))
    static void CalculateBezierMissileTrajectory(
        const UObject* Context,
        UPARAM(ref, DisplayName = "控制点") const TArray<FVector>& Points,
        UPARAM(DisplayName = "进度") float Progress,
        UPARAM(DisplayName = "噪声采样时间",
            meta = (ToolTip = "累计秒数，仅控制噪声相位，不控制轨迹位置。")) float ElapsedTime,
        UPARAM(ref, DisplayName = "标架状态") FBezierRotationFrameState& InOutFrame,
        UPARAM(DisplayName = "位置") FVector& OutPosition,
        UPARAM(DisplayName = "切线") FVector& OutTangent,
        UPARAM(DisplayName = "旋转") FRotator& OutRotation,
        UPARAM(DisplayName = "噪声选项") FBezierNoiseOptions NoiseOptions = FBezierNoiseOptions(),
        UPARAM(DisplayName = "速度选项") FBezierSpeedOptions SpeedOptions = FBezierSpeedOptions(),
        UPARAM(DisplayName = "显示调试") bool bShowDebug = false,
        UPARAM(DisplayName = "调试持续时间") float Duration = 0.03f,
        UPARAM(DisplayName = "调试颜色") FBezierDebugColors DebugColors = FBezierDebugColors());

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
        UPARAM(DisplayName="网格间距", meta=(UIMin="1.0", UIMax="500.0", ForceUnits="cm")) float GridSpacing,
        UPARAM(DisplayName="噪声偏移", meta=(UIMin="0.0", UIMax="100.0", ForceUnits="cm")) float Noise,
        UPARAM(DisplayName="检测半径", meta=(UIMin="0.1", UIMax="100.0", ForceUnits="cm")) float TraceRadius,
        bool bEnableDebugDraw,
        bool bDrawOnlySuccessfulHits,
        bool bEnableBoundsCulling,
        UPARAM(DisplayName="调试持续时间", meta=(UIMin="0.1", UIMax="30.0", ForceUnits="s")) float DebugDrawDuration,
        // --- Outputs
        TArray<FVector>& OutPoints,
        bool& bSuccess,
        // --- Optional Input
        bool bUseComplexCollision = true
    );

private:
    // 计算曲线上某点的位置（基于参数）
    static FVector CalculatePointAtParameter(const TArray<FVector>& Points, float Parameter, TArray<FVector>& OutWorkPoints);
    static FVector CalculatePointAtParameterFast(const TArray<FVector>& Points, float Parameter);
    static FVector CalculateDerivativeAtParameter(const TArray<FVector>& Points, float Parameter);
    static float ResolveBezierParameter(
        const TArray<FVector>& Points,
        float Progress,
        const FBezierSpeedOptions& SpeedOptions,
        float& OutMotionProgress,
        FBezierRotationFrameState* InOutTrajectoryState = nullptr);
    static FVector EvaluateBezierPoint(
        const TArray<FVector>& Points,
        float Progress,
        const FBezierSpeedOptions& SpeedOptions,
        TArray<FVector>* OutWorkPoints = nullptr);
    static void DrawBezierDebug(
        UWorld* World,
        const TArray<FVector>& Points,
        const TArray<FVector>& WorkPoints,
        const FBezierDebugColors& DebugColors,
        float Duration,
        const FVector& ResultPoint);

public:


};
