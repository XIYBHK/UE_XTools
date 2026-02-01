/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 */

#include "GeometryInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGeometryTool, Log, All);
DEFINE_LOG_CATEGORY(LogGeometryTool);

// ============================================================================
// 内部辅助函数
// ============================================================================

namespace GeometryToolInternal
{
	/**
	 * 在指定平面上根据角度和半径计算偏移量（极坐标转笛卡尔坐标）
	 * @param AngleDeg 角度（度）
	 * @param Radius 半径
	 * @param AxisX X轴方向向量
	 * @param AxisY Y轴方向向量
	 * @return 偏移向量
	 */
	FVector PolarToCartesianOnPlane(float AngleDeg, float Radius, const FVector& AxisX, const FVector& AxisY)
	{
		return AxisX * UKismetMathLibrary::DegCos(AngleDeg) * Radius +
		       AxisY * UKismetMathLibrary::DegSin(AngleDeg) * Radius;
	}
}

UGeometryInstance::UGeometryInstance()
{
}

void UGeometryInstance::BeginPlay()
{
    Super::BeginPlay();
}

void UGeometryInstance::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

TArray<FTransform> UGeometryInstance::GetPointsByShape(
    UShapeComponent* Shape,
    bool bIsAddInstance,
    float Distance,
    float Noise,
    bool bIsUseLookAtOrigin,
    FRotator Rotator_A,
    FRotator Rotator_B,
    bool bIsUseRandomRotation,
    FVector Size_A,
    FVector Size_B,
    bool bIsUseRandomSize,
    FRotator Rotator_Delta)
{
    TArray<FTransform> FTransforms;

    if (!Shape)
    {
        return FTransforms;
    }

    Distance = FMath::Clamp(Distance, 0.f, 100000.f);

    if (USphereComponent* Sphere = Cast<USphereComponent>(Shape))
    {
        FTransforms = GenerateSpherePoints(Sphere, Distance, Noise, bIsUseLookAtOrigin,
            Rotator_A, Rotator_B, bIsUseRandomRotation, Size_A, Size_B, bIsUseRandomSize, Rotator_Delta);
    }
    else if (UBoxComponent* Box = Cast<UBoxComponent>(Shape))
    {
        FTransforms = GenerateBoxPoints(Box, Distance, Noise, bIsUseLookAtOrigin,
            Rotator_A, Rotator_B, bIsUseRandomRotation, Size_A, Size_B, bIsUseRandomSize, Rotator_Delta);
    }
    else if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Shape))
    {
        FTransforms = GenerateCapsulePoints(Capsule, Distance, Noise, bIsUseLookAtOrigin,
            Rotator_A, Rotator_B, bIsUseRandomRotation, Size_A, Size_B, bIsUseRandomSize, Rotator_Delta);
    }
    else
    {
        UE_LOG(LogGeometryTool, Warning, TEXT("[GeometryInstance] 不支持的形状类型: %s"),
            *Shape->GetClass()->GetName());
    }

    if (bIsAddInstance)
    {
        for (const FTransform& Transform : FTransforms)
        {
            AddInstance(Transform, true);
        }
    }

    return FTransforms;
}

void UGeometryInstance::GetPointsByCustomRect(
    FTransform OriginTransform,
    FVector Counts3D,
    FVector Distance3D,
    bool bIsUseWorldSpace,
    FRotator Rotator_A,
    FRotator Rotator_B,
    bool bIsUseRandomRotation,
    FVector Size_A,
    FVector Size_B,
    bool bIsUseRandomSize)
{
    if (bIsUseWorldSpace && GetStaticMesh())
    {
        for (int Index_Z = 0; Index_Z < Counts3D.Z; ++Index_Z)
        {
            for (int Index_Y = 0; Index_Y < Counts3D.Y; ++Index_Y)
            {
                for (int Index_X = 0; Index_X < Counts3D.X; ++Index_X)
                {
                    FTransform InstanceTransform;

                    FVector ForwardVector = OriginTransform.GetRotation().GetForwardVector();
                    FVector RightVector = OriginTransform.GetRotation().GetRightVector();
                    FVector UpVector = OriginTransform.GetRotation().GetUpVector();

                    FVector Location = OriginTransform.GetLocation()
                        + Distance3D.X * Index_X * ForwardVector
                        + Distance3D.Y * Index_Y * RightVector
                        + Distance3D.Z * Index_Z * UpVector
                        - Distance3D.X * (Counts3D.X - 1) * 0.5 * ForwardVector
                        - Distance3D.Y * (Counts3D.Y - 1) * 0.5 * RightVector
                        - Distance3D.Z * (Counts3D.Z - 1) * 0.5 * UpVector;

                    InstanceTransform.SetLocation(Location);
                    InstanceTransform.SetRotation(OriginTransform.GetRotation());
                    InstanceTransform.SetScale3D(OriginTransform.GetScale3D());

                    ApplyTransformParameters(
                        InstanceTransform,
                        false,  // bIsUseLookAtOrigin
                        bIsUseRandomRotation,
                        bIsUseRandomSize,
                        Rotator_A, Rotator_B, Size_A, Size_B,
                        FRotator::ZeroRotator,  // Rotator_Delta
                        FVector::ZeroVector,    // Origin
                        Location                 // PointLocation
                    );

                    AddInstance(InstanceTransform, bIsUseWorldSpace);
                }
            }
        }
    }
}

TArray<FTransform> UGeometryInstance::GetPointsByCircle(
    int32 InitCount,
    float InitAngle,
    int32 Level,
    float RadiusDelta)
{
    TArray<FTransform> FTransforms;
    FTransform FirstTransform;
    FirstTransform.SetLocation(GetOwner() ? GetOwner()->GetActorLocation() : FVector(0, 0, 0));
    FirstTransform.SetRotation(FRotator(0, 0, 0).Quaternion());
    FirstTransform.SetScale3D(FVector(1, 1, 1));

    FTransforms.Add(FirstTransform);
    if (InitCount <= 2 || Level <= 1 || RadiusDelta <= 0.f)
    {
        return FTransforms;
    }

    float CurtAngle = InitAngle;
    float CurtRadius = 0.0f;
    int32 CurtCount = InitCount;

    for (int level = 1; level < Level; ++level)
    {
        CurtRadius = RadiusDelta * level;
        CurtCount = InitCount * level;

        for (int count = 0; count < CurtCount; ++count)
        {
            CurtAngle = InitAngle + (360.f / CurtCount) * count;

            FVector Point = GetOwner() ? GetOwner()->GetActorLocation() : FVector(0, 0, 0);
            Point += GeometryToolInternal::PolarToCartesianOnPlane(CurtAngle, CurtRadius, FVector(1, 0, 0), FVector(0, 1, 0));

            FTransform CurtTransform;
            CurtTransform.SetLocation(Point);
            CurtTransform.SetRotation(FRotator(0, 0, 0).Quaternion());
            CurtTransform.SetScale3D(FVector(1, 1, 1));

            FTransforms.Add(CurtTransform);
        }
    }

    return FTransforms;
}

TArray<FTransform> UGeometryInstance::GenerateSpherePoints(
    USphereComponent* Sphere,
    float Distance,
    float Noise,
    bool bIsUseLookAtOrigin,
    const FRotator& Rotator_A,
    const FRotator& Rotator_B,
    bool bIsUseRandomRotation,
    const FVector& Size_A,
    const FVector& Size_B,
    bool bIsUseRandomSize,
    const FRotator& Rotator_Delta)
{
    TArray<FTransform> FTransforms;

    if (!Sphere)
    {
        return FTransforms;
    }

    float Radius = Sphere->GetScaledSphereRadius();
    FVector Origin = Sphere->GetComponentLocation();
    float SphereRound = 2.f * Radius * 3.14159f;
    int32 NumPerRound = FMath::Floor(SphereRound / Distance);
    float DeltaAnglePerRound = FMath::Clamp(360.f / NumPerRound, 0.f, 360.f);

    float Longitude = 0.f;
    float Lantitude = 0.f;
    FRotator CurtRotator(0, 0, 0);

    while (Longitude <= 360.f)
    {
        Lantitude = 0.f;
        while (Lantitude <= 360.f)
        {
            CurtRotator.Yaw += Longitude;

            float AngleNoise = FMath::Clamp(360.f / FMath::Floor(SphereRound / Noise), 0.f, 360.f);
            float Lantitude_A = Lantitude + FMath::RandRange(0.f, AngleNoise);

            FVector Point = Origin + GeometryToolInternal::PolarToCartesianOnPlane(
                Lantitude_A, Radius,
                CurtRotator.Quaternion().GetForwardVector(),
                CurtRotator.Quaternion().GetUpVector());

            FTransform InstanceTransform;
            InstanceTransform.SetLocation(Point);

            ApplyTransformParameters(InstanceTransform, bIsUseLookAtOrigin, bIsUseRandomRotation, bIsUseRandomSize,
                Rotator_A, Rotator_B, Size_A, Size_B, Rotator_Delta, GetComponentLocation(), Point);

            FTransforms.Add(InstanceTransform);

            Lantitude += DeltaAnglePerRound;
        }
        Longitude += DeltaAnglePerRound;
    }

    return FTransforms;
}

TArray<FTransform> UGeometryInstance::GenerateBoxPoints(
    UBoxComponent* Box,
    float Distance,
    float Noise,
    bool bIsUseLookAtOrigin,
    const FRotator& Rotator_A,
    const FRotator& Rotator_B,
    bool bIsUseRandomRotation,
    const FVector& Size_A,
    const FVector& Size_B,
    bool bIsUseRandomSize,
    const FRotator& Rotator_Delta)
{
    TArray<FTransform> FTransforms;

    if (!Box)
    {
        return FTransforms;
    }

    FVector Origin = Box->GetComponentLocation();
    FVector BoxRange3D = 2 * FVector(Box->GetScaledBoxExtent().X, Box->GetScaledBoxExtent().Y, Box->GetScaledBoxExtent().Z);
    FRotator Rotator = Box->GetComponentRotation();

    int32 X = FMath::Floor(BoxRange3D.X / Distance) + 1;
    int32 Y = FMath::Floor(BoxRange3D.Y / Distance) + 1;
    int32 Z = FMath::Floor(BoxRange3D.Z / Distance) + 1;

    for (int32 Index_Z = 0; Index_Z < Z; ++Index_Z)
    {
        for (int32 Index_Y = 0; Index_Y < Y; ++Index_Y)
        {
            for (int32 Index_X = 0; Index_X < X; ++Index_X)
            {
                if (Index_Z > 0 && Index_Z <= Z - 2 && Index_Y > 0 && Index_Y <= Y - 2 && Index_X > 0 && Index_X <= X - 2)
                {
                    continue;
                }

                float RandomDistanceDelta = FMath::RandRange(Noise * -1.f, Noise);

                FVector ForwardVector = Rotator.Quaternion().GetForwardVector();
                FVector RightVector = Rotator.Quaternion().GetRightVector();
                FVector UpVector = Rotator.Quaternion().GetUpVector();

                FVector Location = Origin
                    + Distance * Index_X * ForwardVector
                    + Distance * Index_Y * RightVector
                    + Distance * Index_Z * UpVector
                    - Distance * (X - 1) * 0.5 * ForwardVector
                    - Distance * (Y - 1) * 0.5 * RightVector
                    - Distance * (Z - 1) * 0.5 * UpVector
                    + RandomDistanceDelta * ForwardVector
                    + RandomDistanceDelta * RightVector
                    + RandomDistanceDelta * UpVector;

                FTransform InstanceTransform;
                InstanceTransform.SetLocation(Location);

                ApplyTransformParameters(InstanceTransform, bIsUseLookAtOrigin, bIsUseRandomRotation, bIsUseRandomSize,
                    Rotator_A, Rotator_B, Size_A, Size_B, Rotator_Delta, Origin, Location);

                FTransforms.Add(InstanceTransform);
            }
        }
    }

    return FTransforms;
}

TArray<FTransform> UGeometryInstance::GenerateCapsulePoints(
    UCapsuleComponent* Capsule,
    float Distance,
    float Noise,
    bool bIsUseLookAtOrigin,
    const FRotator& Rotator_A,
    const FRotator& Rotator_B,
    bool bIsUseRandomRotation,
    const FVector& Size_A,
    const FVector& Size_B,
    bool bIsUseRandomSize,
    const FRotator& Rotator_Delta)
{
    TArray<FTransform> FTransforms;

    if (!Capsule)
    {
        return FTransforms;
    }

    return FTransforms;
}

void UGeometryInstance::ApplyTransformParameters(
    FTransform& OutTransform,
    bool bIsUseLookAtOrigin,
    bool bIsUseRandomRotation,
    bool bIsUseRandomSize,
    const FRotator& Rotator_A,
    const FRotator& Rotator_B,
    const FVector& Size_A,
    const FVector& Size_B,
    const FRotator& Rotator_Delta,
    const FVector& Origin,
    const FVector& PointLocation) const
{
    FRotator Rotator = FRotator(0, 0, 0);
    FVector Size = FVector(1, 1, 1);

    if (bIsUseLookAtOrigin)
    {
        Rotator = UKismetMathLibrary::FindLookAtRotation(PointLocation, Origin);
    }

    if (bIsUseRandomRotation)
    {
        Rotator = FRotator(
            FMath::RandRange(Rotator_A.Pitch, Rotator_B.Pitch),
            FMath::RandRange(Rotator_A.Yaw, Rotator_B.Yaw),
            FMath::RandRange(Rotator_A.Roll, Rotator_B.Roll));
    }

    if (bIsUseRandomSize)
    {
        Size = FVector(
            FMath::RandRange(Size_A.X, Size_B.X),
            FMath::RandRange(Size_A.Y, Size_B.Y),
            FMath::RandRange(Size_A.Z, Size_B.Z));
    }

    Rotator += Rotator_Delta;
    OutTransform.SetRotation(Rotator.Quaternion());
    OutTransform.SetScale3D(Size);
}