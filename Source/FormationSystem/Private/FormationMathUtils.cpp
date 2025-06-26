#include "FormationMathUtils.h"
#include "FormationLog.h"

bool FFormationMathUtils::DoPathsIntersect(
    const FVector& Start1, const FVector& End1,
    const FVector& Start2, const FVector& End2,
    float Threshold)
{
    // 使用2D投影进行路径相交检测（忽略Z轴）
    FVector2D A(Start1.X, Start1.Y);
    FVector2D B(End1.X, End1.Y);
    FVector2D C(Start2.X, Start2.Y);
    FVector2D D(End2.X, End2.Y);

    // 计算线段AB和CD的方向向量
    FVector2D AB = B - A;
    FVector2D CD = D - C;
    FVector2D AC = C - A;

    // 使用叉积判断相交
    float CrossAB_CD = AB.X * CD.Y - AB.Y * CD.X;
    
    // 如果线段平行，检查是否重叠
    if (FMath::IsNearlyZero(CrossAB_CD, 1e-6f))
    {
        // 平行线段，检查是否在同一直线上且重叠
        float CrossAC_AB = AC.X * AB.Y - AC.Y * AB.X;
        if (FMath::IsNearlyZero(CrossAC_AB, 1e-6f))
        {
            // 在同一直线上，检查重叠
            float DotAB = FVector2D::DotProduct(AB, AB);
            if (DotAB > 1e-6f)
            {
                float t1 = FVector2D::DotProduct(AC, AB) / DotAB;
                FVector2D AD = D - A;
                float t2 = FVector2D::DotProduct(AD, AB) / DotAB;
                
                float tMin = FMath::Min(t1, t2);
                float tMax = FMath::Max(t1, t2);
                
                return (tMax >= 0.0f && tMin <= 1.0f);
            }
        }
        return false;
    }

    // 计算交点参数
    float t = (AC.X * CD.Y - AC.Y * CD.X) / CrossAB_CD;
    float u = (AC.X * AB.Y - AC.Y * AB.X) / CrossAB_CD;

    // 检查交点是否在两条线段上
    return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
}

FVector FFormationMathUtils::CalculateSeparationForce(
    int32 UnitIndex,
    const TArray<FVector>& Positions,
    const FBoidsMovementParams& Params)
{
    // 使用check验证输入参数
    check(Params.SeparationWeight >= 0.0f);
    
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

    // 验证两个数组大小匹配
    ensure(Positions.Num() == Velocities.Num());

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