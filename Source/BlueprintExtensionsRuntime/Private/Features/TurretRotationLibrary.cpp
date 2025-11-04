#include "Features/TurretRotationLibrary.h"
#include "Kismet/KismetMathLibrary.h"

void UTurretRotationLibrary::CalculateRotateDegree(UPARAM(ref) float& CurrentDegree, const FTransform& ShaftTransform, const FVector& TargetLocation, EAxis::Type RotateAxis, float RotateSpeed, const FVector2D& RotateRange, float DeltaTime)
{
    // 获取旋转轴的世界坐标
    const FVector WorldRotateAxis = ShaftTransform.GetUnitAxis(RotateAxis);

    // 将TargetLocation投影到基底为ShaftTransform.Location，法线为WorldRotateAxis的世界坐标平面上
    const FVector ProjectedLocation = FVector::PointPlaneProject(TargetLocation, ShaftTransform.GetLocation(), WorldRotateAxis);

    // 将TargetLocation转换到ShaftTransform的本地坐标系
    const FVector LocalTargetLocation = ShaftTransform.InverseTransformPosition(ProjectedLocation);

    // 根据旋转轴调整用于Atan2运算的坐标轴
    float TargetDegree;
    switch (RotateAxis)
    {
        case EAxis::X:
            TargetDegree = FMath::Atan2(LocalTargetLocation.Y, LocalTargetLocation.Z) * (180.0f / PI);
            break;
        case EAxis::Y:
            TargetDegree = FMath::Atan2(LocalTargetLocation.Z, LocalTargetLocation.X) * (180.0f / PI);
            break;
        case EAxis::Z:
            TargetDegree = FMath::Atan2(LocalTargetLocation.Y, LocalTargetLocation.X) * (180.0f / PI);
            break;
        default:
            TargetDegree = 0.0f;
            break;
    }

    // 钳制角度在RotateRange范围内
    TargetDegree = FMath::Clamp(TargetDegree, RotateRange.X, RotateRange.Y);

    // 计算最短旋转方向
    const float DeltaDegree = FMath::FindDeltaAngleDegrees(CurrentDegree, TargetDegree);

    // 计算RotateFactor并Lerp为0.25-1
    const float RotateFactor = FMath::Lerp(0.25f, 1.0f, FMath::Clamp(FMath::Abs(DeltaDegree) / (0.05f * RotateSpeed), 0.0f, 1.0f));

    // 调整RotateSpeed
    const float AdjustedRotateSpeed = RotateSpeed * RotateFactor;

    // 计算新的角度
    CurrentDegree = FMath::FInterpConstantTo(CurrentDegree, CurrentDegree + DeltaDegree, DeltaTime, AdjustedRotateSpeed);
}