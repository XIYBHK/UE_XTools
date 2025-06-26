#pragma once

#include "CoreMinimal.h"
#include "FormationTypes.generated.h"

/**
 * 阵型系统类型定义
 * 包含阵型相关的枚举和数据结构
 */

/**
 * 阵型变换模式枚举
 */
UENUM(BlueprintType)
enum class EFormationTransitionMode : uint8
{
    /** 基于相对位置的优化分配（推荐，效果最佳） */
    OptimizedAssignment     UMETA(DisplayName = "优化分配（相对位置）"),

    /** 基于绝对距离的简单分配 */
    SimpleAssignment        UMETA(DisplayName = "简单分配（绝对距离）"),

    /** 基于索引的直接映射 */
    DirectMapping           UMETA(DisplayName = "直接映射（按索引）"),

    /** 直接相对位置匹配（最直观的左对左、右对右） */
    DirectRelativePositionMatching UMETA(DisplayName = "直接相对位置匹配"),

    /** RTS群集移动：基于Boids算法的群集移动模式 */
    RTSFlockMovement UMETA(DisplayName = "RTS群集移动"),

    /** 路径感知分配：预测并避免路径冲突 */
    PathAwareAssignment UMETA(DisplayName = "路径感知分配"),

    /** 智能空间映射：基于空间排序的直接映射，支持相同阵型检测和螺旋阵型优化 */
    SpatialOrderMapping UMETA(DisplayName = "智能空间映射"),

    /** @deprecated 使用SpatialOrderMapping代替。为了向后兼容保留 */
    DistancePriorityAssignment UMETA(DisplayName = "距离优先分配（已弃用）", Hidden)
};

/**
 * 阵型类型枚举
 */
UENUM(BlueprintType)
enum class EFormationType : uint8
{
    /** 方形阵型 */
    Square      UMETA(DisplayName = "方形阵型"),
    
    /** 圆形阵型 */
    Circle      UMETA(DisplayName = "圆形阵型"),
    
    /** 线形阵型 */
    Line        UMETA(DisplayName = "线形阵型"),
    
    /** 三角形阵型 */
    Triangle    UMETA(DisplayName = "三角形阵型"),

    /** 箭头形阵型 */
    Arrow       UMETA(DisplayName = "箭头形阵型"),

    /** 螺旋线形阵型 */
    Spiral      UMETA(DisplayName = "螺旋线形阵型"),

    /** 实心圆形阵型 */
    SolidCircle UMETA(DisplayName = "实心圆形阵型"),

    /** 折线形阵型 */
    Zigzag      UMETA(DisplayName = "折线形阵型"),

    /** 自定义阵型 */
    Custom      UMETA(DisplayName = "自定义阵型")
};

/**
 * 阵型数据结构
 * 存储阵型中每个位置的坐标信息
 */
USTRUCT(BlueprintType)
struct FORMATIONSYSTEM_API FFormationData
{
    GENERATED_BODY()

    /** 阵型类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", 
              meta = (DisplayName = "阵型类型"))
    EFormationType FormationType = EFormationType::Square;

    /** 阵型中心位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", 
              meta = (DisplayName = "阵型中心"))
    FVector CenterLocation = FVector::ZeroVector;

    /** 阵型旋转 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", 
              meta = (DisplayName = "阵型旋转"))
    FRotator Rotation = FRotator::ZeroRotator;

    /** 阵型位置数组（相对于中心的偏移） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", 
              meta = (DisplayName = "位置数组"))
    TArray<FVector> Positions;

    /** 阵型尺寸参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", 
              meta = (DisplayName = "阵型尺寸"))
    FVector2D Size = FVector2D(100.0f, 100.0f);

    /** 单位间距 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", 
              meta = (DisplayName = "单位间距", ClampMin = "10.0", ClampMax = "500.0", 
                     UIMin = "50.0", UIMax = "300.0"))
    float Spacing = 100.0f;

    FFormationData()
    {
        FormationType = EFormationType::Square;
        CenterLocation = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        Size = FVector2D(100.0f, 100.0f);
        Spacing = 100.0f;
    }

    /** 获取世界坐标位置数组 */
    TArray<FVector> GetWorldPositions() const;

    /** 获取阵型的轴对齐包围盒 */
    FBox GetAABB() const;
};

/**
 * 阵型变换配置
 */
USTRUCT(BlueprintType)
struct FORMATIONSYSTEM_API FFormationTransitionConfig
{
    GENERATED_BODY()

    /** 变换模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", 
              meta = (DisplayName = "变换模式"))
    EFormationTransitionMode TransitionMode = EFormationTransitionMode::DirectRelativePositionMatching;

    /** 变换持续时间（秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", 
              meta = (DisplayName = "变换时间", ClampMin = "0.1", ClampMax = "10.0"))
    float Duration = 2.0f;

    /** 是否使用缓动曲线 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", 
              meta = (DisplayName = "使用缓动"))
    bool bUseEasing = true;

    /** 缓动强度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", 
              meta = (DisplayName = "缓动强度", ClampMin = "0.1", ClampMax = "5.0", 
                     EditCondition = "bUseEasing"))
    float EasingStrength = 2.0f;

    /** 是否显示调试信息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", 
              meta = (DisplayName = "显示调试", ToolTip = "在运行时显示阵型变换的调试可视化信息"))
    bool bShowDebug = false;

    /** 调试线条持续时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", 
              meta = (DisplayName = "调试持续时间", EditCondition = "bShowDebug", 
                     ToolTip = "调试线条和球体的显示持续时间（秒）", ClampMin = "0.1", ClampMax = "30.0"))
    float DebugDuration = 5.0f;

    FFormationTransitionConfig()
    {
        TransitionMode = EFormationTransitionMode::DirectRelativePositionMatching;
        Duration = 2.0f;
        bUseEasing = true;
        EasingStrength = 2.0f;
        bShowDebug = false;
        DebugDuration = 5.0f;
    }
};

/**
 * 单个单位的变换数据
 */
USTRUCT(BlueprintType)
struct FORMATIONSYSTEM_API FUnitTransitionData
{
    GENERATED_BODY()

    /** 目标Actor */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    TWeakObjectPtr<AActor> TargetActor;

    /** 起始位置 */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    FVector StartLocation = FVector::ZeroVector;

    /** 目标位置 */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    FVector TargetLocation = FVector::ZeroVector;

    /** 起始旋转 */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    FRotator StartRotation = FRotator::ZeroRotator;

    /** 目标旋转 */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    FRotator TargetRotation = FRotator::ZeroRotator;

    /** 起始缩放 */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    FVector StartScale = FVector::OneVector;

    /** 目标缩放 */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    FVector TargetScale = FVector::OneVector;

    /** 当前进度 (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    float Progress = 0.0f;

    /** 是否已完成变换 */
    UPROPERTY(BlueprintReadWrite, Category = "Unit")
    bool bCompleted = false;

    FUnitTransitionData()
    {
        StartLocation = FVector::ZeroVector;
        TargetLocation = FVector::ZeroVector;
        StartRotation = FRotator::ZeroRotator;
        TargetRotation = FRotator::ZeroRotator;
        StartScale = FVector::OneVector;
        TargetScale = FVector::OneVector;
        Progress = 0.0f;
        bCompleted = false;
    }
};

/**
 * Boids群集移动参数
 */
USTRUCT(BlueprintType)
struct FORMATIONSYSTEM_API FBoidsMovementParams
{
    GENERATED_BODY()

    /** 分离力权重（避免碰撞） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "分离力权重", ClampMin = "0.0", ClampMax = "10.0",
                     ToolTip = "控制单位避免与其他单位碰撞的力度。值越高，单位越倾向于保持距离"))
    float SeparationWeight = 2.0f;

    /** 对齐力权重（保持方向一致） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "对齐力权重", ClampMin = "0.0", ClampMax = "10.0",
                     ToolTip = "控制单位与邻近单位保持移动方向一致的力度"))
    float AlignmentWeight = 1.0f;

    /** 聚合力权重（保持群体） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "聚合力权重", ClampMin = "0.0", ClampMax = "10.0",
                     ToolTip = "控制单位向群体中心聚拢的力度，维持群体的凝聚性"))
    float CohesionWeight = 1.5f;

    /** 目标寻找权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "目标寻找权重", ClampMin = "0.0", ClampMax = "10.0"))
    float SeekWeight = 3.0f;

    /** 邻居检测半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "邻居半径", ClampMin = "50.0", ClampMax = "500.0"))
    float NeighborRadius = 150.0f;

    /** 分离检测半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "分离半径", ClampMin = "20.0", ClampMax = "200.0"))
    float SeparationRadius = 80.0f;

    /** 对齐半径（保持方向一致的距离） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "对齐半径", ClampMin = "50.0", ClampMax = "300.0"))
    float AlignmentRadius = 150.0f;

    /** 聚合半径（保持群体的距离） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "聚合半径", ClampMin = "50.0", ClampMax = "300.0"))
    float CohesionRadius = 150.0f;

    /** 最大移动速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "最大速度", ClampMin = "100.0", ClampMax = "1000.0"))
    float MaxSpeed = 400.0f;

    /** 最大转向力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids", 
              meta = (DisplayName = "最大转向力", ClampMin = "50.0", ClampMax = "500.0"))
    float MaxSteerForce = 200.0f;

    FBoidsMovementParams()
    {
        SeparationWeight = 2.0f;
        AlignmentWeight = 1.0f;
        CohesionWeight = 1.5f;
        SeekWeight = 3.0f;
        NeighborRadius = 150.0f;
        SeparationRadius = 80.0f;
        AlignmentRadius = 150.0f;
        CohesionRadius = 150.0f;
        MaxSpeed = 400.0f;
        MaxSteerForce = 200.0f;
    }
};

/**
 * 路径冲突信息
 */
USTRUCT(BlueprintType)
struct FORMATIONSYSTEM_API FPathConflictInfo
{
    GENERATED_BODY()

    /** 是否存在冲突 */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    bool bHasConflict = false;

    /** 冲突对数组 */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    TArray<FIntPoint> ConflictPairs;

    /** 冲突严重程度 (0.0-1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    float ConflictSeverity = 0.0f;

    /** 总冲突数量 */
    UPROPERTY(BlueprintReadOnly, Category = "Conflict")
    int32 TotalConflicts = 0;

    FPathConflictInfo()
    {
        bHasConflict = false;
        ConflictSeverity = 0.0f;
        TotalConflicts = 0;
    }
};

/**
 * 阵型变换状态数据
 */
USTRUCT(BlueprintType)
struct FORMATIONSYSTEM_API FFormationTransitionState
{
    GENERATED_BODY()

    /** 是否正在进行变换 */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsTransitioning = false;

    /** 变换开始时间 */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    float StartTime = 0.0f;

    /** 变换总进度 (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    float OverallProgress = 0.0f;

    /** 参与变换的单位数据 */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    TArray<FUnitTransitionData> UnitTransitions;

    /** 变换配置 */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FFormationTransitionConfig Config;

    /** Boids移动参数 */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FBoidsMovementParams BoidsParams;

    /** 路径冲突信息 */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FPathConflictInfo ConflictInfo;

    FFormationTransitionState()
    {
        bIsTransitioning = false;
        StartTime = 0.0f;
        OverallProgress = 0.0f;
    }
};

/**
 * 空间排序数据结构
 */
struct FSpatialSortData
{
    int32 OriginalIndex;
    float Angle;
    float DistanceToCenter;
    FVector Position;
}; 