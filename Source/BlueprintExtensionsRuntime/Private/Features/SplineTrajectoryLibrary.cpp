#include "Features/SplineTrajectoryLibrary.h"

#include "Components/SplineComponent.h"

namespace
{
	constexpr float MaxTrajectoryCurvatureMagnitude = 10.0f;
	constexpr float MaxRocketRandomFactorMagnitude = 1.0f;
	constexpr float MaxRocketRandomYawDegrees = 90.0f;

	bool IsValidTrajectoryInput(const USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation)
	{
		return IsValid(SplineComponent)
			&& !MuzzleTransform.ContainsNaN()
			&& !TargetLocation.ContainsNaN();
	}

	float SanitizeCurvature(float Curvature)
	{
		return FMath::IsFinite(Curvature) ? FMath::Clamp(Curvature, -MaxTrajectoryCurvatureMagnitude, MaxTrajectoryCurvatureMagnitude) : 0.0f;
	}

	float SanitizeRandomFactor(float RandomFactor)
	{
		return FMath::IsFinite(RandomFactor) ? FMath::Clamp(RandomFactor, -MaxRocketRandomFactorMagnitude, MaxRocketRandomFactorMagnitude) : 0.0f;
	}

	void BuildRocketTrajectory(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation, float Curvature, float RandomFactor, FRandomStream* RandomStream)
	{
		if (!IsValidTrajectoryInput(SplineComponent, MuzzleTransform, TargetLocation))
		{
			return;
		}

		const float SafeCurvature = SanitizeCurvature(Curvature);
		const float SafeRandomFactor = SanitizeRandomFactor(RandomFactor);
		const FVector StartLocation = MuzzleTransform.GetLocation();
		const FVector EndLocation = TargetLocation;
		const float Distance = FVector::Dist(StartLocation, EndLocation);
		const float CurvatureOffset = Distance * SafeCurvature;

		const FVector CLocation = EndLocation + FVector(0.0f, 0.0f, CurvatureOffset);
		const FVector DLocation = StartLocation + FVector(0.0f, 0.0f, CurvatureOffset);

		SplineComponent->ClearSplinePoints(false);
		SplineComponent->AddSplinePoint(StartLocation, ESplineCoordinateSpace::World, false);
		SplineComponent->AddSplinePoint(EndLocation, ESplineCoordinateSpace::World, false);

		const FVector ForwardTangent = MuzzleTransform.GetUnitAxis(EAxis::X) * Distance;
		const FVector StartTangent = FMath::Lerp(CLocation - StartLocation, ForwardTangent, 0.5f);
		SplineComponent->SetTangentAtSplinePoint(0, StartTangent, ESplineCoordinateSpace::World, false);

		FVector EndTangent = EndLocation - DLocation;
		const float MaxYawOffset = SafeRandomFactor * MaxRocketRandomYawDegrees;
		const float YawOffset = RandomStream ? RandomStream->FRandRange(0.0f, MaxYawOffset) : FMath::FRandRange(0.0f, MaxYawOffset);
		EndTangent = FRotator(0.0f, YawOffset, 0.0f).RotateVector(EndTangent);
		SplineComponent->SetTangentAtSplinePoint(1, EndTangent, ESplineCoordinateSpace::World, false);

		SplineComponent->UpdateSpline();
	}
}

void USplineTrajectoryLibrary::SplineTrajectoryFlat(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation)
{
	if (!IsValidTrajectoryInput(SplineComponent, MuzzleTransform, TargetLocation))
	{
		return;
	}

	const FVector StartLocation = MuzzleTransform.GetLocation();

	SplineComponent->ClearSplinePoints(false);
	SplineComponent->AddSplinePoint(StartLocation, ESplineCoordinateSpace::World, false);
	SplineComponent->AddSplinePoint(TargetLocation, ESplineCoordinateSpace::World, false);
	SplineComponent->UpdateSpline();
}

void USplineTrajectoryLibrary::SplineTrajectoryBallistic(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation, float Curvature)
{
	if (!IsValidTrajectoryInput(SplineComponent, MuzzleTransform, TargetLocation))
	{
		return;
	}

	const FVector StartLocation = MuzzleTransform.GetLocation();
	const FVector EndLocation = TargetLocation;
	const float Distance = FVector::Dist(StartLocation, EndLocation);
	const float CurvatureOffset = Distance * SanitizeCurvature(Curvature);

	const FVector CLocation = EndLocation + FVector(0.0f, 0.0f, CurvatureOffset);
	const FVector DLocation = StartLocation + FVector(0.0f, 0.0f, CurvatureOffset);

	SplineComponent->ClearSplinePoints(false);
	SplineComponent->AddSplinePoint(StartLocation, ESplineCoordinateSpace::World, false);
	SplineComponent->AddSplinePoint(EndLocation, ESplineCoordinateSpace::World, false);
	SplineComponent->SetTangentAtSplinePoint(0, CLocation - StartLocation, ESplineCoordinateSpace::World, false);
	SplineComponent->SetTangentAtSplinePoint(1, EndLocation - DLocation, ESplineCoordinateSpace::World, false);
	SplineComponent->UpdateSpline();
}

void USplineTrajectoryLibrary::SplineTrajectoryRocket(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation, float Curvature, float RandomFactor)
{
	BuildRocketTrajectory(SplineComponent, MuzzleTransform, TargetLocation, Curvature, RandomFactor, nullptr);
}

void USplineTrajectoryLibrary::SplineTrajectoryRocketFromStream(USplineComponent* SplineComponent, const FTransform& MuzzleTransform, const FVector& TargetLocation, float Curvature, float RandomFactor, FRandomStream& RandomStream)
{
	BuildRocketTrajectory(SplineComponent, MuzzleTransform, TargetLocation, Curvature, RandomFactor, &RandomStream);
}
