// FormationManagerComponent.cpp - 阵型管理组件实现

#include "FormationManagerComponent.h"
#include "IFormationInterface.h"
#include "FormationMathUtils.h"
#include "FormationLog.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("FormationSystem"), STATGROUP_FormationSystem, STATCAT_Advanced)
DECLARE_CYCLE_STAT(TEXT("UpdateUnitPositions"), STAT_UpdateUnitPositions, STATGROUP_FormationSystem)
DECLARE_CYCLE_STAT(TEXT("CalculateOptimalAssignment"), STAT_CalculateOptimalAssignment, STATGROUP_FormationSystem)

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

void UFormationManagerComponent::DestroyOwnerActor()
{
    AActor* Owner = GetOwner();
    if (IsValid(Owner))
    {
        Owner->Destroy();
    }
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
    // 重入语义：变换进行中再次调用会用新参数替换当前变换（下方直接覆盖 TransitionState），
    // 旧单位停在原地，且不触发旧变换的完成事件/停止事件。调用方需要避免重入时，
    // 请先用 IsTransitioning() 门控（参考 FormationTestActor::SwitchToFormation）。
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

    const auto IsFiniteVector = [](const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    };

    for (const FVector& Position : FromWorldPositions)
    {
        if (!IsFiniteVector(Position))
        {
            UE_LOG(LogFormationSystem, Warning, TEXT("StartFormationTransition: 起始阵型包含非有限位置"));
            return false;
        }
    }
    for (const FVector& Position : ToWorldPositions)
    {
        if (!IsFiniteVector(Position))
        {
            UE_LOG(LogFormationSystem, Warning, TEXT("StartFormationTransition: 目标阵型包含非有限位置"));
            return false;
        }
    }

    // 计算最优分配
    TArray<int32> Assignment = CalculateOptimalAssignment(
        FromWorldPositions,
        ToWorldPositions,
        Config.TransitionMode
    );

    if (Assignment.Num() != Units.Num())
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("StartFormationTransition: 分配结果数量无效"));
        return false;
    }
    for (int32 TargetIndex : Assignment)
    {
        if (!ToWorldPositions.IsValidIndex(TargetIndex))
        {
            UE_LOG(LogFormationSystem, Warning, TEXT("StartFormationTransition: 分配结果包含无效目标索引"));
            return false;
        }
    }

    // 先在局部收集有效单位的变换数据：全部无效时直接失败返回，
    // 不触碰进行中的过渡状态——否则旧过渡会被破坏却既无停止事件也无完成事件，
    // 绑定其上的监听者将永久悬挂。
    TArray<FUnitTransitionData> NewUnitTransitions;
    NewUnitTransitions.Reserve(Units.Num());

    for (int32 i = 0; i < Units.Num(); i++)
    {
        AActor* Actor = Units[i];
        if (!IsValid(Actor) || !Actor->GetRootComponent())
        {
            UE_LOG(LogFormationSystem, Warning,
                TEXT("StartFormationTransition: 跳过无效或无根组件的单位（索引 %d）"), i);
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

        NewUnitTransitions.Add(MoveTemp(UnitData));
    }

    if (NewUnitTransitions.IsEmpty())
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("StartFormationTransition: 没有可移动的有效单位"));
        return false;
    }

    // 校验全部通过后才提交状态并驱动 Tick
    TransitionState.bIsTransitioning = true;
    TransitionState.StartTime = GetWorld()->GetTimeSeconds();
    SetComponentTickEnabled(true);
    TransitionState.OverallProgress = 0.0f;
    TransitionState.Config = Config;
    TransitionState.UnitTransitions = MoveTemp(NewUnitTransitions);

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
    const bool bStarted = StartFormationTransition(Units, FromFormation, ToFormation, Config);
    if (bStarted)
    {
        // 变换已成功开始，通知实现了 IFormationInterface 的单位
        NotifyFormationInterfaceActors();
    }
    return bStarted;
}

void UFormationManagerComponent::NotifyFormationInterfaceActors()
{
    // 基于已计算好的变换数据，通知实现了 IFormationInterface 的单位
    for (const FUnitTransitionData& UnitData : TransitionState.UnitTransitions)
    {
        AActor* Unit = UnitData.TargetActor.Get();
        if (!IsValid(Unit) || !Unit->Implements<UFormationInterface>())
        {
            continue;
        }

        // 注意：是否参与变换应由调用方在传入 Units 前筛选。已纳入 TransitionState 的单位都会被
        // UpdateUnitPositions 统一驱动移动，故此处对所有接口单位发送回调，不再用
        // CanParticipateInFormation 二次过滤——否则会出现"跳过通知但仍被移动"的语义矛盾。
        IFormationInterface::Execute_OnFormationPositionAssigned(
            Unit, UnitData.TargetLocation, TransitionState.Config);
        IFormationInterface::Execute_OnFormationTransitionStarted(
            Unit, UnitData.StartLocation, UnitData.TargetLocation, TransitionState.Config);
    }
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
            }
        }
    }

    TransitionState.bIsTransitioning = false;
    TransitionState.OverallProgress = 0.0f;
    TransitionState.UnitTransitions.Empty();
    SetComponentTickEnabled(false);

    // 广播停止事件（正常完成走 OnFormationTransitionCompleted，两者语义独立，不互相伪装）。
    // 放在状态清理之后：监听者（如便捷节点创建的临时管理 Actor 的自销毁）观察到的是已停止的最终状态。
    // 重复调用由函数入口的 bIsTransitioning 早退拦截，不会重复广播。
    OnFormationTransitionStopped.Broadcast();
}

// ========== 算法实现 ==========

TArray<int32> UFormationManagerComponent::CalculateOptimalAssignment(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions,
    EFormationTransitionMode TransitionMode)
{
    SCOPE_CYCLE_COUNTER(STAT_CalculateOptimalAssignment);
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
    // 专用算法直接分发，不走成本矩阵路径
    switch (Mode)
    {
    case EFormationTransitionMode::DirectMapping:
        {
            // 按索引直接映射
            TArray<int32> Assignment;
            Assignment.SetNum(FromPositions.Num());
            for (int32 i = 0; i < FromPositions.Num(); ++i)
            {
                Assignment[i] = i;
            }
            return Assignment;
        }

    case EFormationTransitionMode::DirectRelativePositionMatching:
        return CalculateDirectRelativePositionMatching(FromPositions, ToPositions);

    case EFormationTransitionMode::SpatialOrderMapping:
    case EFormationTransitionMode::DistancePriorityAssignment:
        return CalculateSpatialOrderMapping(FromPositions, ToPositions);

    case EFormationTransitionMode::RTSFlockMovement:
        return CalculateRTSFlockMovementAssignment(FromPositions, ToPositions);

    case EFormationTransitionMode::PathAwareAssignment:
        return CalculatePathAwareAssignment(FromPositions, ToPositions);

    case EFormationTransitionMode::OptimizedAssignment:
    case EFormationTransitionMode::SimpleAssignment:
    default:
        break;
    }

    // OptimizedAssignment / SimpleAssignment 走成本矩阵路径
    bool bUseRelativePosition = (Mode == EFormationTransitionMode::OptimizedAssignment);
    TArray<TArray<float>> CostMatrix = CreateCostMatrix(FromPositions, ToPositions, bUseRelativePosition);
    ApplyCostModifications(CostMatrix, FromPositions, ToPositions, Mode);
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

    if (CostMatrixCache.IsValid(PositionsHash, CacheMode, CurrentTime, FromPositions, ToPositions))
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
    CostMatrixCache.UpdateCache(PositionsHash, CacheMode, NewCostMatrix, CurrentTime, FromPositions, ToPositions);

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

    // 将两个数组的元素数量纳入哈希：From/To 边界切分不同的输入（展平序列相同）不再共享同一哈希
    Hash = HashCombine(Hash, GetTypeHash(FromPositions.Num()));
    Hash = HashCombine(Hash, GetTypeHash(ToPositions.Num()));

    return Hash;
}

// 缓存系统实现

bool UFormationManagerComponent::FCostMatrixCache::IsValid(
    uint32 NewHash,
    EFormationTransitionMode NewMode,
    double CurrentTime,
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions) const
{
    // 快路径：哈希 + 模式 + TTL 初筛
    if (PositionsHash != NewHash ||
        Mode != NewMode ||
        (CurrentTime - CacheTime) >= FormationPerformanceConfig::CacheLifetimeSeconds)
    {
        return false;
    }

    // 慢路径：仅在哈希初筛通过后精确比较位置数组（TArray::operator== 先比长度再逐元素），
    // 消除 32 位截断碰撞与结构碰撞导致的误命中
    return CachedFromPositions == FromPositions && CachedToPositions == ToPositions;
}

void UFormationManagerComponent::FCostMatrixCache::UpdateCache(
    uint32 NewHash,
    EFormationTransitionMode NewMode,
    const TArray<TArray<float>>& NewMatrix,
    double CurrentTime,
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions)
{
    PositionsHash = NewHash;
    Mode = NewMode;
    CostMatrix = NewMatrix;
    CacheTime = CurrentTime;
    // TArray 拷贝赋值即深拷贝，保存输入副本而非引用，避免悬空
    CachedFromPositions = FromPositions;
    CachedToPositions = ToPositions;
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
    SCOPE_CYCLE_COUNTER(STAT_UpdateUnitPositions);
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
        
        Actor->SetActorLocation(CurrentLocation);
        Actor->SetActorRotation(CurrentRotation);
        
        if (Progress >= 1.0f)
        {
            UnitData.bCompleted = true;

            // 首次完成时通知接口（仅调用一次）
            if (IsValid(Actor) && Actor->Implements<UFormationInterface>())
            {
                IFormationInterface::Execute_OnFormationTransitionCompleted(Actor, UnitData.TargetLocation);
            }
        }
        else
        {
            bAllCompleted = false;
        }
    }
    
    if (bAllCompleted)
    {
        TransitionState.bIsTransitioning = false;
        SetComponentTickEnabled(false);

        // 通知 Owner Actor 过渡已完成，由外部决定是否销毁
        OnFormationTransitionCompleted.Broadcast();
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
