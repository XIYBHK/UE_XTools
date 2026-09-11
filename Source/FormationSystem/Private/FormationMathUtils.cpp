#include "FormationMathUtils.h"
#include "FormationLog.h"
#include "Math/UnrealMathUtility.h"

bool FFormationMathUtils::DoPathsIntersect(
    const FVector& Start1, const FVector& End1,
    const FVector& Start2, const FVector& End2,
    float Threshold)
{
    if (Start1.ContainsNaN() || End1.ContainsNaN() || Start2.ContainsNaN() || End2.ContainsNaN()
        || !FMath::IsFinite(Threshold))
    {
        return false;
    }

    // 保持 XY 投影契约。先处理相交，否则平面线段的最近点对至少包含一个端点。
    const FVector A(Start1.X, Start1.Y, 0.0);
    const FVector B(End1.X, End1.Y, 0.0);
    const FVector C(Start2.X, Start2.Y, 0.0);
    const FVector D(End2.X, End2.Y, 0.0);
    const double Cross = (B.X - A.X) * (D.Y - C.Y) - (B.Y - A.Y) * (D.X - C.X);
    FVector Intersection;
    if (Cross != 0.0 && FMath::SegmentIntersection2D(A, B, C, D, Intersection))
    {
        return true;
    }

    // UE 的点到线段投影支持零长度线段；四个方向同时检查保证交换路径后结果一致。
    const double MinDistanceSquared = FMath::Min(
        FMath::Min(FVector::DistSquared(A, FMath::ClosestPointOnSegment(A, C, D)),
                   FVector::DistSquared(B, FMath::ClosestPointOnSegment(B, C, D))),
        FMath::Min(FVector::DistSquared(C, FMath::ClosestPointOnSegment(C, A, B)),
                   FVector::DistSquared(D, FMath::ClosestPointOnSegment(D, A, B))));
    const double SafeThreshold = FMath::Max(0.0, static_cast<double>(Threshold));
    return MinDistanceSquared <= SafeThreshold * SafeThreshold;
}

// TODO: 使用空间分区将 O(N^2) 邻域查询优化为 O(N)
FVector FFormationMathUtils::CalculateSeparationForce(
    int32 UnitIndex,
    const TArray<FVector>& Positions,
    const FBoidsMovementParams& Params)
{
    if (Params.SeparationWeight < 0.0f)
    {
        UE_LOG(LogFormationSystem, Warning,
            TEXT("CalculateSeparationForce: SeparationWeight 不能为负数: %.3f"),
            Params.SeparationWeight);
        return FVector::ZeroVector;
    }
    
    if (!Positions.IsValidIndex(UnitIndex))
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("CalculateSeparationForce: 无效的单位索引 %d (数组大小: %d)"), UnitIndex, Positions.Num());
        return FVector::ZeroVector;
    }

    const FVector& UnitPos = Positions[UnitIndex];
    FVector SeparationForce = FVector::ZeroVector;
    int32 NeighborCount = 0;

    for (int32 i = 0; i < Positions.Num(); i++)
    {
        if (i != UnitIndex)
        {
            FVector ToNeighbor = Positions[i] - UnitPos;
            float Distance = ToNeighbor.Size();

            if (Distance > 0.0f && Distance < Params.SeparationRadius)
            {
                // 分离力与距离成反比
                FVector AwayFromNeighbor = -ToNeighbor.GetSafeNormal() / Distance;
                SeparationForce += AwayFromNeighbor;
                NeighborCount++;
            }
        }
    }

    if (NeighborCount > 0)
    {
        SeparationForce /= NeighborCount;
        SeparationForce = SeparationForce.GetSafeNormal() * Params.MaxSpeed;
    }

    return SeparationForce * Params.SeparationWeight;
}

FVector FFormationMathUtils::CalculateAlignmentForce(
    int32 UnitIndex,
    const TArray<FVector>& Positions,
    const TArray<FVector>& Velocities,
    const FBoidsMovementParams& Params)
{
    // 使用ensure验证输入参数
    ensure(Params.AlignmentWeight >= 0.0f);
    
    if (!Positions.IsValidIndex(UnitIndex) || !Velocities.IsValidIndex(UnitIndex))
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("CalculateAlignmentForce: 无效的单位索引 %d (位置数组大小: %d, 速度数组大小: %d)"), 
            UnitIndex, Positions.Num(), Velocities.Num());
        return FVector::ZeroVector;
    }

    if (Positions.Num() != Velocities.Num())
    {
        UE_LOG(LogFormationSystem, Warning,
            TEXT("CalculateAlignmentForce: 位置和速度数组大小不一致 (%d != %d)"),
            Positions.Num(), Velocities.Num());
        return FVector::ZeroVector;
    }

    const FVector& UnitPos = Positions[UnitIndex];
    FVector AverageVelocity = FVector::ZeroVector;
    int32 NeighborCount = 0;

    for (int32 i = 0; i < Positions.Num(); i++)
    {
        if (i != UnitIndex)
        {
            float Distance = FVector::Dist(UnitPos, Positions[i]);
            if (Distance < Params.AlignmentRadius)
            {
                AverageVelocity += Velocities[i];
                NeighborCount++;
            }
        }
    }

    if (NeighborCount > 0)
    {
        AverageVelocity /= NeighborCount;
        AverageVelocity = AverageVelocity.GetSafeNormal() * Params.MaxSpeed;
        return (AverageVelocity - Velocities[UnitIndex]) * Params.AlignmentWeight;
    }

    return FVector::ZeroVector;
}

FVector FFormationMathUtils::CalculateCohesionForce(
    int32 UnitIndex,
    const TArray<FVector>& Positions,
    const FBoidsMovementParams& Params)
{
    // 使用ensure验证输入参数
    ensure(Params.CohesionWeight >= 0.0f);
    
    if (!Positions.IsValidIndex(UnitIndex))
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("CalculateCohesionForce: 无效的单位索引 %d (数组大小: %d)"), UnitIndex, Positions.Num());
        return FVector::ZeroVector;
    }

    const FVector& UnitPos = Positions[UnitIndex];
    FVector CenterOfMass = FVector::ZeroVector;
    int32 NeighborCount = 0;

    for (int32 i = 0; i < Positions.Num(); i++)
    {
        if (i != UnitIndex)
        {
            float Distance = FVector::Dist(UnitPos, Positions[i]);
            if (Distance < Params.CohesionRadius)
            {
                CenterOfMass += Positions[i];
                NeighborCount++;
            }
        }
    }

    if (NeighborCount > 0)
    {
        CenterOfMass /= NeighborCount;
        FVector ToCenterOfMass = (CenterOfMass - UnitPos).GetSafeNormal() * Params.MaxSpeed;
        return ToCenterOfMass * Params.CohesionWeight;
    }

    return FVector::ZeroVector;
}

FVector FFormationMathUtils::CalculateSeekForce(
    const FVector& CurrentPos,
    const FVector& TargetPos,
    const FVector& CurrentVelocity,
    const FBoidsMovementParams& Params)
{
    // 使用ensure验证输入参数
    ensure(Params.SeekWeight >= 0.0f);
    ensure(Params.MaxSpeed > 0.0f);
    ensure(Params.MaxSteerForce > 0.0f);
    
    FVector DesiredVelocity = (TargetPos - CurrentPos).GetSafeNormal() * Params.MaxSpeed;
    FVector SteeringForce = DesiredVelocity - CurrentVelocity;
    
    // 限制转向力
    if (SteeringForce.SizeSquared() > FMath::Square(Params.MaxSteerForce))
    {
        SteeringForce = SteeringForce.GetSafeNormal() * Params.MaxSteerForce;
    }
    
    return SteeringForce * Params.SeekWeight;
}

FVector FFormationMathUtils::LimitVector(const FVector& Vector, float MaxMagnitude)
{
    // 使用ensure验证输入参数
    ensure(MaxMagnitude >= 0.0f);
    
    if (Vector.SizeSquared() > MaxMagnitude * MaxMagnitude)
    {
        return Vector.GetSafeNormal() * MaxMagnitude;
    }
    return Vector;
}

float FFormationMathUtils::ApplyEasing(float Progress, float Strength)
{
    // 使用ensure验证输入参数
    ensure(Progress >= 0.0f && Progress <= 1.0f);
    ensure(Strength > 0.0f);
    
    // 使用幂函数实现缓动
    return FMath::Pow(Progress, Strength);
}
