// FormationManagerComponent.cpp - 阵型管理组件实现

#include "FormationManagerComponent.h"
#include "IFormationInterface.h"
#include "FormationMathUtils.h"
#include "FormationLog.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

// 性能优化配置常量（外部声明）
namespace FormationPerformanceConfig
{
    /** 缓存生命周期（秒） */
    constexpr double CacheLifetimeSeconds = 1.0;

    /** 相对位置权重 */
    constexpr float RelativePositionWeight = 0.7f;

    /** 绝对距离权重 */
    constexpr float AbsoluteDistanceWeight = 0.3f;

    /** 相对位置缩放因子 */
    constexpr float RelativePositionScale = 1000.0f;

    /** 最小尺寸阈值，避免除零错误 */
    constexpr float MinSizeThreshold = 1.0f;
}
#include "Kismet/KismetMathLibrary.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"

// ========== 组件生命周期管理 ==========

UFormationManagerComponent::UFormationManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    
    // 初始化变换状态
    TransitionState = FFormationTransitionState();
    
    // 初始化Boids参数
    TransitionState.BoidsParams = FBoidsMovementParams();
}

void UFormationManagerComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UFormationManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (TransitionState.bIsTransitioning)
    {
        UpdateUnitPositions(DeltaTime);
        
        if (TransitionState.Config.bShowDebug)
        {
            DrawDebugInfo();
        }
    }
}

// ========== 阵型变换控制 ==========

bool UFormationManagerComponent::IsFormationTranslation(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions)
{
    if (FromPositions.Num() == 0 || ToPositions.Num() == 0 || FromPositions.Num() != ToPositions.Num())
    {
        return false;
    }

    // 计算起始阵型的包围盒
    FBox FromAABB(FromPositions[0], FromPositions[0]);
    for (const FVector& Pos : FromPositions)
    {
        FromAABB += Pos;
    }

    // 计算目标阵型的包围盒
    FBox ToAABB(ToPositions[0], ToPositions[0]);
    for (const FVector& Pos : ToPositions)
    {
        ToAABB += Pos;
    }

    // 获取包围盒尺寸
    FVector FromSize = FromAABB.GetSize();
    FVector ToSize = ToAABB.GetSize();

    // 输出调试信息
    UE_LOG(LogFormationSystem, Log, TEXT("IsFormationTranslation: FromAABB Size=(%s)"), *FromSize.ToString());
    UE_LOG(LogFormationSystem, Log, TEXT("IsFormationTranslation: ToAABB Size=(%s)"), *ToSize.ToString());

    // 相同阵型检测：尺寸相近则认为是相同阵型的平移
    // 修复：使用相对容差替代硬编码容差值，适配不同规模的阵型
    const float RelativeTolerance = 0.1f; // 10% 相对容差
    const float SizeToleranceX = FMath::Max(FromSize.X, ToSize.X) * RelativeTolerance + 1.0f;
    const float SizeToleranceY = FMath::Max(FromSize.Y, ToSize.Y) * RelativeTolerance + 1.0f;
    const float SizeToleranceZ = FMath::Max(FromSize.Z, ToSize.Z) * RelativeTolerance + 1.0f;

    bool bIsSameFormation =
        FMath::Abs(FromSize.X - ToSize.X) < SizeToleranceX &&
        FMath::Abs(FromSize.Y - ToSize.Y) < SizeToleranceY &&
        FMath::Abs(FromSize.Z - ToSize.Z) < SizeToleranceZ;

    UE_LOG(LogFormationSystem, Log, TEXT("IsFormationTranslation: 检测结果=%s"), 
        bIsSameFormation ? TEXT("相同阵型") : TEXT("不同阵型"));

    return bIsSameFormation;
}

bool UFormationManagerComponent::StartFormationTransition(
    const TArray<AActor*>& Units,
    const FFormationData& FromFormation,
    const FFormationData& ToFormation,
    const FFormationTransitionConfig& Config)
{
    // 验证输入参数
    if (Units.Num() == 0)
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("StartFormationTransition: 单位数组为空"));
        return false;
    }

    // 检查两个阵型的位置数量是否匹配
    if (FromFormation.Positions.Num() != ToFormation.Positions.Num())
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("StartFormationTransition: 阵型位置数量不匹配 (起始: %d, 目标: %d)"),
            FromFormation.Positions.Num(), ToFormation.Positions.Num());
        return false;
    }

    // 检查单位数量是否与阵型位置数量匹配
    if (Units.Num() != FromFormation.Positions.Num())
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("StartFormationTransition: 单位数量与阵型位置数量不匹配 (单位: %d, 位置: %d)"),
            Units.Num(), FromFormation.Positions.Num());
        return false;
    }

    // 获取世界坐标位置
    TArray<FVector> FromWorldPositions = FromFormation.GetWorldPositions();
    TArray<FVector> ToWorldPositions = ToFormation.GetWorldPositions();

    // 计算最优分配
    TArray<int32> Assignment = CalculateOptimalAssignment(
        FromWorldPositions,
        ToWorldPositions,
        Config.TransitionMode
    );

    // 初始化变换状态
    TransitionState.bIsTransitioning = true;
    TransitionState.StartTime = GetWorld()->GetTimeSeconds();
    TransitionState.OverallProgress = 0.0f;
    TransitionState.Config = Config;
    
    // 清空并预分配单位变换数据数组
    TransitionState.UnitTransitions.Empty(Units.Num());
    TransitionState.UnitTransitions.Reserve(Units.Num());

    // 为每个单位设置变换数据
    for (int32 i = 0; i < Units.Num(); i++)
    {
        AActor* Actor = Units[i];
        if (!Actor)
        {
            continue;
        }

        // 创建单位变换数据
        FUnitTransitionData UnitData;
        UnitData.TargetActor = Actor;
        UnitData.StartLocation = Actor->GetActorLocation();
        UnitData.TargetLocation = ToWorldPositions[Assignment[i]];
        UnitData.StartRotation = Actor->GetActorRotation();
        
        // 计算目标朝向（朝向移动方向）
        FVector MovementDirection = UnitData.TargetLocation - UnitData.StartLocation;
        if (!MovementDirection.IsNearlyZero())
        {
            UnitData.TargetRotation = MovementDirection.Rotation();
        }
        else
        {
            UnitData.TargetRotation = UnitData.StartRotation;
        }

        UnitData.StartScale = Actor->GetActorScale3D();
        UnitData.TargetScale = UnitData.StartScale; // 保持缩放不变
        UnitData.Progress = 0.0f;
        UnitData.bCompleted = false;

        // 添加到变换数据数组
        TransitionState.UnitTransitions.Add(UnitData);
    }

    // 检测路径冲突
    TransitionState.ConflictInfo = DetectPathConflicts(Assignment, FromWorldPositions, ToWorldPositions);

    return true;
}

bool UFormationManagerComponent::StartFormationTransitionWithInterface(
    const TArray<AActor*>& Units,
    const FFormationData& FromFormation,
    const FFormationData& ToFormation,
    const FFormationTransitionConfig& Config)
{
    return StartFormationTransition(Units, FromFormation, ToFormation, Config);
}

void UFormationManagerComponent::StopFormationTransition(bool bSnapToTarget)
{
    if (!TransitionState.bIsTransitioning)
    {
        return;
    }

    if (bSnapToTarget)
    {
        for (FUnitTransitionData& UnitData : TransitionState.UnitTransitions)
        {
            if (UnitData.TargetActor.IsValid())
            {
                AActor* Actor = UnitData.TargetActor.Get();
                Actor->SetActorLocation(UnitData.TargetLocation);
                Actor->SetActorRotation(UnitData.TargetRotation);
                Actor->SetActorScale3D(UnitData.TargetScale);
            }
        }
    }

    TransitionState.bIsTransitioning = false;
    TransitionState.OverallProgress = 0.0f;
    TransitionState.UnitTransitions.Empty();
}

// ========== 算法实现 ==========

TArray<int32> UFormationManagerComponent::CalculateOptimalAssignment(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions,
    EFormationTransitionMode TransitionMode)
{
    // 性能优化：条件日志记录，避免不必要的字符串构造
    UE_LOG(LogFormationSystem, VeryVerbose, TEXT("FormationManager: 开始计算最优分配，单位数量: %d"), FromPositions.Num());

    if (FromPositions.Num() == 0 || ToPositions.Num() == 0)
    {
        return TArray<int32>();
    }

    if (FromPositions.Num() != ToPositions.Num())
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("FormationManager: 位置数量不匹配 From:%d To:%d"), FromPositions.Num(), ToPositions.Num());
        return TArray<int32>();
    }

    // 性能监控：计算耗时统计
    double StartTime = FPlatformTime::Seconds();

    // 简化的算法选择 - 使用统一的成本矩阵方法
    TArray<int32> Result = CalculateAssignmentByMode(FromPositions, ToPositions, TransitionMode);

    double ElapsedTime = FPlatformTime::Seconds() - StartTime;
    UE_LOG(LogFormationSystem, VeryVerbose, TEXT("FormationManager: 分配计算完成，耗时: %.3fms"), ElapsedTime * 1000.0);

    return Result;
}

TArray<int32> UFormationManagerComponent::CalculateAssignmentByMode(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions,
    EFormationTransitionMode Mode)
{
    // 统一的成本矩阵创建和求解流程

    // 1. 根据模式确定是否使用相对位置
    bool bUseRelativePosition = (Mode == EFormationTransitionMode::OptimizedAssignment);

    // 2. 创建基础成本矩阵
    TArray<TArray<float>> CostMatrix = CreateCostMatrix(FromPositions, ToPositions, bUseRelativePosition);

    // 3. 应用算法特定的修正
    ApplyCostModifications(CostMatrix, FromPositions, ToPositions, Mode);

    // 4. 求解分配问题
    return SolveAssignmentProblem(CostMatrix);
}

// 简化的工具函数实现

TArray<TArray<float>> UFormationManagerComponent::CreateCostMatrix(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions,
    bool bUseRelativePosition)
{
    // 性能优化：智能缓存检查
    EFormationTransitionMode CacheMode = bUseRelativePosition ?
        EFormationTransitionMode::OptimizedAssignment :
        EFormationTransitionMode::SimpleAssignment;

    uint32 PositionsHash = CalculatePositionsHash(FromPositions, ToPositions);
    double CurrentTime = FPlatformTime::Seconds();

    if (CostMatrixCache.IsValid(PositionsHash, CacheMode, CurrentTime))
    {
        UE_LOG(LogFormationSystem, VeryVerbose, TEXT("使用缓存的成本矩阵"));
        return CostMatrixCache.CostMatrix;
    }

    // 计算新的成本矩阵
    TArray<TArray<float>> NewCostMatrix;
    if (bUseRelativePosition)
    {
        NewCostMatrix = CalculateRelativePositionCostMatrix(FromPositions, ToPositions);
    }
    else
    {
        NewCostMatrix = CalculateAbsoluteDistanceCostMatrix(FromPositions, ToPositions);
    }

    // 更新缓存
    CostMatrixCache.UpdateCache(PositionsHash, CacheMode, NewCostMatrix, CurrentTime);

    return NewCostMatrix;
}

uint32 UFormationManagerComponent::CalculatePositionsHash(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions) const
{
    // 性能优化：高效的位置哈希计算
    uint32 Hash = 0;

    // 使用 UE 内置的哈希函数
    for (const FVector& Pos : FromPositions)
    {
        Hash = HashCombine(Hash, GetTypeHash(Pos));
    }

    for (const FVector& Pos : ToPositions)
    {
        Hash = HashCombine(Hash, GetTypeHash(Pos));
    }

    return Hash;
}

// 缓存系统实现

bool UFormationManagerComponent::FCostMatrixCache::IsValid(
    uint32 NewHash,
    EFormationTransitionMode NewMode,
    double CurrentTime) const
{
    return PositionsHash == NewHash &&
           Mode == NewMode &&
           (CurrentTime - CacheTime) < FormationPerformanceConfig::CacheLifetimeSeconds;
}

void UFormationManagerComponent::FCostMatrixCache::UpdateCache(
    uint32 NewHash,
    EFormationTransitionMode NewMode,
    const TArray<TArray<float>>& NewMatrix,
    double CurrentTime)
{
    PositionsHash = NewHash;
    Mode = NewMode;
    CostMatrix = NewMatrix;
    CacheTime = CurrentTime;
}

void UFormationManagerComponent::ApplyCostModifications(
    TArray<TArray<float>>& CostMatrix,
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions,
    EFormationTransitionMode Mode)
{
    // 根据算法模式应用特定的成本修正
    switch (Mode)
    {
        case EFormationTransitionMode::RTSFlockMovement:
        {
            // 应用群集行为修正
            for (int32 i = 0; i < FromPositions.Num(); i++)
            {
                for (int32 j = 0; j < ToPositions.Num(); j++)
                {
                    float FlockingBonus = CalculateFlockingBonus(i, j, FromPositions, ToPositions);
                    CostMatrix[i][j] -= FlockingBonus;
                    CostMatrix[i][j] = FMath::Max(1.0f, CostMatrix[i][j]);
                }
            }
            break;
        }

        case EFormationTransitionMode::DirectRelativePositionMatching:
        case EFormationTransitionMode::SpatialOrderMapping:
        case EFormationTransitionMode::PathAwareAssignment:
        {
            // 这些模式使用特殊算法，不需要成本矩阵修正
            // 直接调用对应的算法实现
            break;
        }

        default:
            // 其他模式不需要额外修正
            break;
    }
}

// ========== 单位位置更新 ==========

void UFormationManagerComponent::UpdateUnitPositions(float DeltaTime)
{
    if (!TransitionState.bIsTransitioning)
    {
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ElapsedTime = CurrentTime - TransitionState.StartTime;
    float Duration = FMath::Max(TransitionState.Config.Duration, 0.1f);
    float RawProgress = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
    
    float Progress = RawProgress;
    if (TransitionState.Config.bUseEasing)
    {
        Progress = ApplyEasing(RawProgress, TransitionState.Config.EasingStrength);
    }
    
    TransitionState.OverallProgress = Progress;
    
    bool bAllCompleted = true;
    
    for (FUnitTransitionData& UnitData : TransitionState.UnitTransitions)
    {
        if (UnitData.bCompleted || !UnitData.TargetActor.IsValid())
        {
            continue;
        }
        
        AActor* Actor = UnitData.TargetActor.Get();
        UnitData.Progress = Progress;
        
        FVector CurrentLocation = FMath::Lerp(UnitData.StartLocation, UnitData.TargetLocation, Progress);
        FRotator CurrentRotation = FMath::Lerp(UnitData.StartRotation, UnitData.TargetRotation, Progress);
        FVector CurrentScale = FMath::Lerp(UnitData.StartScale, UnitData.TargetScale, Progress);
        
        Actor->SetActorLocation(CurrentLocation);
        Actor->SetActorRotation(CurrentRotation);
        Actor->SetActorScale3D(CurrentScale);
        
        if (Progress >= 1.0f)
        {
            UnitData.bCompleted = true;
        }
        else
        {
            bAllCompleted = false;
        }
    }
    
    if (bAllCompleted)
    {
        TransitionState.bIsTransitioning = false;
    }
}

float UFormationManagerComponent::ApplyEasing(float Progress, float Strength) const
{
    return FFormationMathUtils::ApplyEasing(Progress, Strength);
}

// ========== 调试绘制 ==========

void UFormationManagerComponent::DrawDebugInfo() const
{
    if (!GetWorld())
    {
        return;
    }

    for (const FUnitTransitionData& UnitData : TransitionState.UnitTransitions)
    {
        if (!UnitData.TargetActor.IsValid())
        {
            continue;
        }
        
        AActor* Actor = UnitData.TargetActor.Get();
        FVector CurrentLocation = Actor->GetActorLocation();
        
        DrawDebugSphere(
            GetWorld(),
            UnitData.TargetLocation,
            20.0f,
            8,
            FColor::Green,
            false,
            TransitionState.Config.DebugDuration
        );
        
        DrawDebugLine(
            GetWorld(),
            CurrentLocation,
            UnitData.TargetLocation,
            FColor::Yellow,
            false,
            TransitionState.Config.DebugDuration,
            0,
            2.0f
        );
    }
}

// ========== Boids参数管理 ==========

void UFormationManagerComponent::SetBoidsMovementParams(const FBoidsMovementParams& NewParams)
{
    TransitionState.BoidsParams = NewParams;
}

FBoidsMovementParams UFormationManagerComponent::GetBoidsMovementParams() const
{
    return TransitionState.BoidsParams;
}

// ========== 路径冲突检测 ==========

FPathConflictInfo UFormationManagerComponent::CheckFormationPathConflicts(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions)
{
    if (FromPositions.Num() != ToPositions.Num())
    {
        return FPathConflictInfo();
    }

    TArray<int32> Assignment = CalculateOptimalAssignment(
        FromPositions,
        ToPositions,
        EFormationTransitionMode::SpatialOrderMapping
    );

    return DetectPathConflicts(Assignment, FromPositions, ToPositions);
}

// ========== 私有函数声明（需要在FormationAlgorithms.cpp中实现） ==========

// ========== 算法实现在FormationAlgorithms.cpp中 ========== 