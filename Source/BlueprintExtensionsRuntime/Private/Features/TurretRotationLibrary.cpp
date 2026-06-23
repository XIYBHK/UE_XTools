#include "Features/TurretRotationLibrary.h"

namespace
{
	constexpr float FullAngleRangeTolerance = 0.01f;

	bool IsFullAngleRange(float MinDegree, float MaxDegree)
	{
		return FMath::Abs(MaxDegree - MinDegree) >= 360.0f - FullAngleRangeTolerance;
	}

	float ClampTurretAngle(float Angle, float MinDegree, float MaxDegree)
	{
		if (!IsFullAngleRange(MinDegree, MaxDegree))
		{
			return FMath::ClampAngle(Angle, MinDegree, MaxDegree);
		}

		return (MinDegree >= 0.0f && MaxDegree >= 0.0f)
			? FRotator::ClampAxis(Angle)
			: FRotator::NormalizeAxis(Angle);
	}
}

void UTurretRotationLibrary::CalculateRotateDegree(UPARAM(ref) float& CurrentDegree, const FTransform& ShaftTransform, const FVector& TargetLocation, EAxis::Type RotateAxis, float RotateSpeed, const FVector2D& RotateRange, float DeltaTime)
{
	if (!FMath::IsFinite(CurrentDegree))
	{
		CurrentDegree = 0.0f;
	}

	if (!FMath::IsFinite(RotateRange.X) || !FMath::IsFinite(RotateRange.Y))
	{
		return;
	}

	const float MinDegree = RotateRange.X;
	const float MaxDegree = RotateRange.Y;
	CurrentDegree = ClampTurretAngle(CurrentDegree, MinDegree, MaxDegree);

	if (RotateAxis != EAxis::X && RotateAxis != EAxis::Y && RotateAxis != EAxis::Z)
	{
		return;
	}

	if (ShaftTransform.ContainsNaN()
		|| TargetLocation.ContainsNaN()
		|| !FMath::IsFinite(RotateSpeed)
		|| !FMath::IsFinite(DeltaTime)
		|| RotateSpeed <= KINDA_SMALL_NUMBER
		|| DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector WorldRotateAxis = ShaftTransform.GetUnitAxis(RotateAxis);
	const FVector ProjectedLocation = FVector::PointPlaneProject(TargetLocation, ShaftTransform.GetLocation(), WorldRotateAxis);
	const FVector LocalTargetLocation = ShaftTransform.InverseTransformPosition(ProjectedLocation);

	float TargetDegree = 0.0f;
	switch (RotateAxis)
	{
	case EAxis::X:
		TargetDegree = FMath::RadiansToDegrees(FMath::Atan2(LocalTargetLocation.Y, LocalTargetLocation.Z));
		break;
	case EAxis::Y:
		TargetDegree = FMath::RadiansToDegrees(FMath::Atan2(LocalTargetLocation.Z, LocalTargetLocation.X));
		break;
	case EAxis::Z:
		TargetDegree = FMath::RadiansToDegrees(FMath::Atan2(LocalTargetLocation.Y, LocalTargetLocation.X));
		break;
	default:
		return;
	}

	TargetDegree = ClampTurretAngle(TargetDegree, MinDegree, MaxDegree);

	const float DeltaDegree = FMath::FindDeltaAngleDegrees(CurrentDegree, TargetDegree);
	const float RotateFactor = FMath::Lerp(0.25f, 1.0f, FMath::Clamp(FMath::Abs(DeltaDegree) / (0.05f * RotateSpeed), 0.0f, 1.0f));
	const float AdjustedRotateSpeed = RotateSpeed * RotateFactor;

	CurrentDegree = FMath::FInterpConstantTo(CurrentDegree, CurrentDegree + DeltaDegree, DeltaTime, AdjustedRotateSpeed);
	CurrentDegree = ClampTurretAngle(CurrentDegree, MinDegree, MaxDegree);
}
