// FormationManagerComponent.cpp - 阵型管理组件实现

#include "FormationManagerComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SceneComponent.h"
#include "FormationMathUtils.h"
#include "FormationLog.h"
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
    // 使用较小的容差值，确保精确检测
    const float SizeTolerance = 10.0f;
    bool bIsSameFormation = 
        FMath::Abs(FromSize.X - ToSize.X) < SizeTolerance &&
        FMath::Abs(FromSize.Y - ToSize.Y) < SizeTolerance &&
        FMath::Abs(FromSize.Z - ToSize.Z) < SizeTolerance;

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
    UE_LOG(LogFormationSystem, Log, TEXT("FormationManager: 开始计算最优分配"));

    if (FromPositions.Num() == 0 || ToPositions.Num() == 0)
    {
        return TArray<int32>();
    }

    if (FromPositions.Num() != ToPositions.Num())
    {
        return TArray<int32>();
    }

    // 根据过渡模式选择不同的分配算法
    switch (TransitionMode)
    {
        case EFormationTransitionMode::OptimizedAssignment:
        {
            UE_LOG(LogFormationSystem, Log, TEXT("使用优化分配算法（相对位置）"));
            TArray<TArray<float>> CostMatrix = CalculateRelativePositionCostMatrix(FromPositions, ToPositions);
            return SolveAssignmentProblem(CostMatrix);
        }
            
        case EFormationTransitionMode::SimpleAssignment:
        {
            UE_LOG(LogFormationSystem, Log, TEXT("使用简单分配算法"));
            TArray<TArray<float>> CostMatrix = CalculateAbsoluteDistanceCostMatrix(FromPositions, ToPositions);
            return SolveAssignmentProblem(CostMatrix);
        }
            
        case EFormationTransitionMode::DirectRelativePositionMatching:
        {
            UE_LOG(LogFormationSystem, Log, TEXT("使用直接相对位置匹配算法"));
            return CalculateDirectRelativePositionMatching(FromPositions, ToPositions);
        }
            
        case EFormationTransitionMode::RTSFlockMovement:
        {
            UE_LOG(LogFormationSystem, Log, TEXT("使用RTS群集移动算法"));
            return CalculateRTSFlockMovementAssignment(FromPositions, ToPositions);
        }
            
        case EFormationTransitionMode::PathAwareAssignment:
        {
            UE_LOG(LogFormationSystem, Log, TEXT("使用路径感知算法"));
            return CalculatePathAwareAssignment(FromPositions, ToPositions);
        }
            
        case EFormationTransitionMode::SpatialOrderMapping:
        case EFormationTransitionMode::DistancePriorityAssignment: // 向后兼容
        default:
        {
            UE_LOG(LogFormationSystem, Log, TEXT("使用空间排序算法"));
            return CalculateSpatialOrderMapping(FromPositions, ToPositions);
        }
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