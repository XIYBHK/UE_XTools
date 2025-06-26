#include "FormationTypes.h"
#include "FormationLog.h"

TArray<FVector> FFormationData::GetWorldPositions() const
{
    TArray<FVector> WorldPositions;
    WorldPositions.Reserve(Positions.Num());

    // 应用旋转和平移到每个位置
    FTransform Transform(Rotation, CenterLocation, FVector::OneVector);
    
    for (const FVector& LocalPos : Positions)
    {
        FVector WorldPos = Transform.TransformPosition(LocalPos);
        WorldPositions.Add(WorldPos);
    }

    return WorldPositions;
}

FBox FFormationData::GetAABB() const
{
    ensure(Positions.Num() > 0);
    
    if (Positions.Num() == 0)
    {
        UE_LOG(LogFormationSystem, Warning, TEXT("尝试获取空阵型的包围盒"));
        return FBox(CenterLocation, CenterLocation);
    }

    // 创建包围盒
    FBox Box(Positions[0], Positions[0]);
    for (int32 i = 1; i < Positions.Num(); i++)
    {
        Box += Positions[i];
    }

    // 应用旋转和平移
    FTransform Transform(Rotation, CenterLocation, FVector::OneVector);
    return Box.TransformBy(Transform);
} 