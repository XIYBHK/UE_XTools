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
	constexpr float MinDistance = 1.0f;
	constexpr int32 MaxAxisSamples = 128;
	constexpr int64 MaxPointBudget = 200000;

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

	int32 CalcAxisSamples(float Length, float Distance)
	{
		return FMath::Clamp(FMath::FloorToInt(Length / Distance) + 1, 1, MaxAxisSamples);
	}

	bool ExceedsPointBudget(int64 Count)
	{
		return Count > MaxPointBudget;
	}
}

UGeometryInstance::UGeometryInstance()
{
    // 禁用Tick，此组件不需要每帧更新
    PrimaryComponentTick.bCanEverTick = false;
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

    Distance = FMath::Clamp(Distance, GeometryToolInternal::MinDistance, 100000.f);
    Noise = FMath::Max(0.0f, Noise);

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
        if (!GetStaticMesh())
        {
            UE_LOG(LogGeometryTool, Warning, TEXT("[GeometryInstance] 当前组件未设置 StaticMesh，跳过 AddInstance。"));
            return FTransforms;
        }

        for (const FTransform& Transform : FTransforms)
        {
            AddInstance(Transform, true);
        }
    }

    return FTransforms;
}

void UGeometryInstance::GetPointsByCustomRect(
    FTransform OriginTransform,
    FIntVector Counts3D,
    FVector Distance3D,
    bool bIsUseWorldSpace,
    FRotator Rotator_A,
    FRotator Rotator_B,
    bool bIsUseRandomRotation,
    FVector Size_A,
    FVector Size_B,
    bool bIsUseRandomSize)
{
    if (!GetStaticMesh())
    {
        UE_LOG(LogGeometryTool, Warning, TEXT("[GeometryInstance] 当前组件未设置 StaticMesh，无法添加实例。"));
        return;
    }

    const int32 CountX = FMath::Clamp(Counts3D.X, 1, GeometryToolInternal::MaxAxisSamples);
    const int32 CountY = FMath::Clamp(Counts3D.Y, 1, GeometryToolInternal::MaxAxisSamples);
    const int32 CountZ = FMath::Clamp(Counts3D.Z, 1, GeometryToolInternal::MaxAxisSamples);

    if (GeometryToolInternal::ExceedsPointBudget(static_cast<int64>(CountX) * CountY * CountZ))
    {
        UE_LOG(LogGeometryTool, Warning, TEXT("[GeometryInstance] 自定义矩形采样点数过大，已取消生成。Count=(%d,%d,%d)"), CountX, CountY, CountZ);
        return;
    }

    const FVector SafeDistance(
        FMath::Max(0.0f, Distance3D.X),
        FMath::Max(0.0f, Distance3D.Y),
        FMath::Max(0.0f, Distance3D.Z));

    // 统一使用世界空间计算点位，避免本地/世界坐标混用导致偏移。
    const FTransform ComponentWorldTransform = GetComponentTransform();
    const FVector ForwardVector = OriginTransform.GetRotation().GetForwardVector();
    const FVector RightVector = OriginTransform.GetRotation().GetRightVector();
    const FVector UpVector = OriginTransform.GetRotation().GetUpVector();

    for (int32 Index_Z = 0; Index_Z < CountZ; ++Index_Z)
    {
        for (int32 Index_Y = 0; Index_Y < CountY; ++Index_Y)
        {
            for (int32 Index_X = 0; Index_X < CountX; ++Index_X)
            {
                FTransform InstanceTransform;

                const FVector Location = OriginTransform.GetLocation()
                    + SafeDistance.X * Index_X * ForwardVector
                    + SafeDistance.Y * Index_Y * RightVector
                    + SafeDistance.Z * Index_Z * UpVector
                    - SafeDistance.X * (CountX - 1) * 0.5f * ForwardVector
                    - SafeDistance.Y * (CountY - 1) * 0.5f * RightVector
                    - SafeDistance.Z * (CountZ - 1) * 0.5f * UpVector;

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
                    Location                // PointLocation
                );

                if (bIsUseWorldSpace)
                {
                    AddInstance(InstanceTransform, true);
                }
                else
                {
                    const FTransform LocalTransform = InstanceTransform.GetRelativeTransform(ComponentWorldTransform);
                    AddInstance(LocalTransform, false);
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

    const float Radius = Sphere->GetScaledSphereRadius();
    if (Radius <= KINDA_SMALL_NUMBER)
    {
        return FTransforms;
    }

    const FVector Origin = Sphere->GetComponentLocation();
    const float SafeDistance = FMath::Max(GeometryToolInternal::MinDistance, Distance);
    const float SphereRound = 2.0f * PI * Radius;
    const int32 NumPerRound = FMath::Clamp(FMath::FloorToInt(SphereRound / SafeDistance), 8, 180);
    const int64 EstimatedCount = static_cast<int64>(NumPerRound) * NumPerRound;
    if (GeometryToolInternal::ExceedsPointBudget(EstimatedCount))
    {
        UE_LOG(LogGeometryTool, Warning, TEXT("[GeometryInstance] 球体采样点数过大，已取消生成。NumPerRound=%d"), NumPerRound);
        return FTransforms;
    }

    const float DeltaAnglePerRound = 360.0f / NumPerRound;
    const float SafeNoise = FMath::Max(0.0f, Noise);
    const float NoiseAngle = (SafeNoise > KINDA_SMALL_NUMBER)
        ? FMath::Clamp(FMath::RadiansToDegrees(SafeNoise / Radius), 0.0f, 45.0f)
        : 0.0f;

    FTransforms.Reserve(static_cast<int32>(EstimatedCount));

    for (int32 LongitudeIndex = 0; LongitudeIndex < NumPerRound; ++LongitudeIndex)
    {
        const float Longitude = LongitudeIndex * DeltaAnglePerRound;
        const FRotator LongitudeRotator(0.0f, Longitude, 0.0f);
        const FVector AxisX = LongitudeRotator.Quaternion().GetForwardVector();
        const FVector AxisY = LongitudeRotator.Quaternion().GetUpVector();

        for (int32 LatitudeIndex = 0; LatitudeIndex < NumPerRound; ++LatitudeIndex)
        {
            const float Latitude = LatitudeIndex * DeltaAnglePerRound;
            const float LatitudeNoise = (NoiseAngle > 0.0f) ? FMath::RandRange(-NoiseAngle, NoiseAngle) : 0.0f;
            const FVector Point = Origin + GeometryToolInternal::PolarToCartesianOnPlane(
                Latitude + LatitudeNoise, Radius, AxisX, AxisY);

            FTransform InstanceTransform;
            InstanceTransform.SetLocation(Point);

            ApplyTransformParameters(
                InstanceTransform,
                bIsUseLookAtOrigin,
                bIsUseRandomRotation,
                bIsUseRandomSize,
                Rotator_A,
                Rotator_B,
                Size_A,
                Size_B,
                Rotator_Delta,
                Origin,
                Point);

            FTransforms.Add(InstanceTransform);
        }
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

    const float SafeDistance = FMath::Max(GeometryToolInternal::MinDistance, Distance);
    const float SafeNoise = FMath::Max(0.0f, Noise);

    const FVector Origin = Box->GetComponentLocation();
    const FVector BoxRange3D = 2.0f * Box->GetScaledBoxExtent();
    const FRotator Rotator = Box->GetComponentRotation();

    const int32 X = GeometryToolInternal::CalcAxisSamples(BoxRange3D.X, SafeDistance);
    const int32 Y = GeometryToolInternal::CalcAxisSamples(BoxRange3D.Y, SafeDistance);
    const int32 Z = GeometryToolInternal::CalcAxisSamples(BoxRange3D.Z, SafeDistance);

    // Box 仅采样边界，近似上限按完整体积估算做保护
    if (GeometryToolInternal::ExceedsPointBudget(static_cast<int64>(X) * Y * Z))
    {
        UE_LOG(LogGeometryTool, Warning, TEXT("[GeometryInstance] 盒体采样点数过大，已取消生成。Count=(%d,%d,%d)"), X, Y, Z);
        return FTransforms;
    }

    const FVector ForwardVector = Rotator.Quaternion().GetForwardVector();
    const FVector RightVector = Rotator.Quaternion().GetRightVector();
    const FVector UpVector = Rotator.Quaternion().GetUpVector();

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

                const float NoiseForward = FMath::RandRange(-SafeNoise, SafeNoise);
                const float NoiseRight = FMath::RandRange(-SafeNoise, SafeNoise);
                const float NoiseUp = FMath::RandRange(-SafeNoise, SafeNoise);

                FVector Location = Origin
                    + SafeDistance * Index_X * ForwardVector
                    + SafeDistance * Index_Y * RightVector
                    + SafeDistance * Index_Z * UpVector
                    - SafeDistance * (X - 1) * 0.5f * ForwardVector
                    - SafeDistance * (Y - 1) * 0.5f * RightVector
                    - SafeDistance * (Z - 1) * 0.5f * UpVector
                    + NoiseForward * ForwardVector
                    + NoiseRight * RightVector
                    + NoiseUp * UpVector;

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

    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float CylinderHalfHeight = FMath::Max(0.0f, HalfHeight - Radius);
    if (Radius <= KINDA_SMALL_NUMBER)
    {
        return FTransforms;
    }

    const float SafeDistance = FMath::Max(GeometryToolInternal::MinDistance, Distance);
    const float SafeNoise = FMath::Max(0.0f, Noise);

    const FVector Origin = Capsule->GetComponentLocation();
    const FRotator Rotator = Capsule->GetComponentRotation();
    const FVector ForwardVector = Rotator.Quaternion().GetForwardVector();
    const FVector RightVector = Rotator.Quaternion().GetRightVector();
    const FVector UpVector = Rotator.Quaternion().GetUpVector();

    const int32 AroundCount = FMath::Clamp(FMath::FloorToInt((2.0f * PI * Radius) / SafeDistance), 8, 180);
    const int32 SideCount = (CylinderHalfHeight > KINDA_SMALL_NUMBER)
        ? FMath::Max(2, FMath::FloorToInt((2.0f * CylinderHalfHeight) / SafeDistance) + 1)
        : 1;
    const int32 CapArcCount = FMath::Clamp(FMath::FloorToInt((0.5f * PI * Radius) / SafeDistance), 2, 64);

    int64 EstimatedCount = static_cast<int64>(AroundCount) * SideCount; // 圆柱侧面
    EstimatedCount += static_cast<int64>(AroundCount) * (CapArcCount - 1) * 2; // 上下半球
    EstimatedCount += 2; // 两极
    if (GeometryToolInternal::ExceedsPointBudget(EstimatedCount))
    {
        UE_LOG(LogGeometryTool, Warning, TEXT("[GeometryInstance] 胶囊采样点数过大，已取消生成。"));
        return FTransforms;
    }

    auto AddPoint = [&](const FVector& PointLocation)
    {
        FVector JitteredPoint = PointLocation
            + FMath::RandRange(-SafeNoise, SafeNoise) * ForwardVector
            + FMath::RandRange(-SafeNoise, SafeNoise) * RightVector
            + FMath::RandRange(-SafeNoise, SafeNoise) * UpVector;

        FTransform InstanceTransform;
        InstanceTransform.SetLocation(JitteredPoint);
        ApplyTransformParameters(
            InstanceTransform,
            bIsUseLookAtOrigin,
            bIsUseRandomRotation,
            bIsUseRandomSize,
            Rotator_A,
            Rotator_B,
            Size_A,
            Size_B,
            Rotator_Delta,
            Origin,
            JitteredPoint);
        FTransforms.Add(InstanceTransform);
    };

    const float AroundDelta = 360.0f / AroundCount;

    // 1) 圆柱侧面
    if (CylinderHalfHeight > KINDA_SMALL_NUMBER)
    {
        for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
        {
            const float Z = -CylinderHalfHeight + (2.0f * CylinderHalfHeight) * (static_cast<float>(SideIndex) / (SideCount - 1));
            const FVector RingCenter = Origin + Z * UpVector;

            for (int32 AroundIndex = 0; AroundIndex < AroundCount; ++AroundIndex)
            {
                const float Angle = AroundIndex * AroundDelta;
                const FVector Radial = GeometryToolInternal::PolarToCartesianOnPlane(Angle, Radius, ForwardVector, RightVector);
                AddPoint(RingCenter + Radial);
            }
        }
    }
    else
    {
        // 退化为球体时至少保留赤道圈
        for (int32 AroundIndex = 0; AroundIndex < AroundCount; ++AroundIndex)
        {
            const float Angle = AroundIndex * AroundDelta;
            AddPoint(Origin + GeometryToolInternal::PolarToCartesianOnPlane(Angle, Radius, ForwardVector, RightVector));
        }
    }

    // 2) 上下半球（不包含赤道，避免和圆柱端圈重复）
    const FVector TopCenter = Origin + CylinderHalfHeight * UpVector;
    const FVector BottomCenter = Origin - CylinderHalfHeight * UpVector;
    for (int32 ArcIndex = 1; ArcIndex < CapArcCount; ++ArcIndex)
    {
        const float Alpha = (static_cast<float>(ArcIndex) / CapArcCount) * (0.5f * PI); // (0, PI/2)
        const float RingRadius = Radius * FMath::Cos(Alpha);
        const float HeightOffset = Radius * FMath::Sin(Alpha);

        if (RingRadius <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        for (int32 AroundIndex = 0; AroundIndex < AroundCount; ++AroundIndex)
        {
            const float Angle = AroundIndex * AroundDelta;
            const FVector RingOffset = GeometryToolInternal::PolarToCartesianOnPlane(Angle, RingRadius, ForwardVector, RightVector);
            AddPoint(TopCenter + HeightOffset * UpVector + RingOffset);
            AddPoint(BottomCenter - HeightOffset * UpVector + RingOffset);
        }
    }

    // 3) 两极
    AddPoint(TopCenter + Radius * UpVector);
    AddPoint(BottomCenter - Radius * UpVector);

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
