#include "Features/SplineTrajectoryLibrary.h"
#include "Components/SplineComponent.h"

void USplineTrajectoryLibrary::SplineTrajectoryFlat(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation)
{
    if (!SplineComponent)
    {
        return;
    }

    // 清空现有的样条点
    SplineComponent->ClearSplinePoints();

    // 获取起点和终点位置
    const FVector StartLocation = MuzzleTransform.GetLocation();

    // 添加起点和终点
    SplineComponent->AddSplinePoint(StartLocation, ESplineCoordinateSpace::World, false);
    SplineComponent->AddSplinePoint(TargetLocation, ESplineCoordinateSpace::World, false);

    // 更新样条曲线
    SplineComponent->UpdateSpline();
}

void USplineTrajectoryLibrary::SplineTrajectoryBallistic(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation, float Curvature)
{
    if (!SplineComponent)
    {
        return;
    }

    // 清空现有的样条点
    SplineComponent->ClearSplinePoints();

    // 获取起点和终点位置
    const FVector StartLocation = MuzzleTransform.GetLocation();
    const FVector EndLocation = TargetLocation;

    // 计算两点之间的距离
    const float Distance = FVector::Dist(StartLocation, EndLocation);
    const float CurvatureOffset = Distance * Curvature;

    // 计算C点和D点的位置
    const FVector CLocation = EndLocation + FVector(0, 0, CurvatureOffset);
    const FVector DLocation = StartLocation + FVector(0, 0, CurvatureOffset);

    // 添加起点和终点
    SplineComponent->AddSplinePoint(StartLocation, ESplineCoordinateSpace::World, false);
    SplineComponent->AddSplinePoint(EndLocation, ESplineCoordinateSpace::World, false);

    // 设置起点和终点的切线
    SplineComponent->SetTangentAtSplinePoint(0, CLocation - StartLocation, ESplineCoordinateSpace::World, false);
    SplineComponent->SetTangentAtSplinePoint(1, EndLocation - DLocation, ESplineCoordinateSpace::World, false);

    // 更新样条曲线
    SplineComponent->UpdateSpline();
}

void USplineTrajectoryLibrary::SplineTrajectoryRocket(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation, float Curvature, float RandomFactor)
{
    if (!SplineComponent)
    {
        return;
    }

    // 清空现有的样条点
    SplineComponent->ClearSplinePoints();

    // 获取起点和终点位置
    const FVector StartLocation = MuzzleTransform.GetLocation();
    const FVector EndLocation = TargetLocation;

    // 计算两点之间的距离和曲率偏移
    const float Distance = FVector::Dist(StartLocation, EndLocation);
    const float CurvatureOffset = Distance * Curvature;

    // 计算C点和D点的位置
    const FVector CLocation = EndLocation + FVector(0, 0, CurvatureOffset);
    const FVector DLocation = StartLocation + FVector(0, 0, CurvatureOffset);

    // 添加起点和终点
    SplineComponent->AddSplinePoint(StartLocation, ESplineCoordinateSpace::World, false);
    SplineComponent->AddSplinePoint(EndLocation, ESplineCoordinateSpace::World, false);

    // 设置起点的切线，方向靠近Start的前方向
    FVector StartTangent = FMath::Lerp(CLocation - StartLocation, MuzzleTransform.GetRotation().Vector() * Distance, 0.5f);
    SplineComponent->SetTangentAtSplinePoint(0, StartTangent, ESplineCoordinateSpace::World, false);

    // 设置终点的切线，方向进行随机偏转
    FVector EndTangent = EndLocation - DLocation;
    EndTangent = FRotator(0, FMath::RandRange(0.0f, RandomFactor * 90.0f), 0).RotateVector(EndTangent);
    SplineComponent->SetTangentAtSplinePoint(1, EndTangent, ESplineCoordinateSpace::World, false);

    // 更新样条曲线
    SplineComponent->UpdateSpline();
}