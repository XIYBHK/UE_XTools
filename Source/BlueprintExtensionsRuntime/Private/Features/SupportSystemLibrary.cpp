#include "Features/SupportSystemLibrary.h"

#include "XToolsBlueprintHelpers.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	void ResetStabilizeHeightOutputs(float& AveragePressureFactor, FVector& AverageImpactNormal)
	{
		AveragePressureFactor = 0.0f;
		AverageImpactNormal = FVector::UpVector;
	}

	bool IsFiniteStabilizeHeightInput(float StableHeight, float GripHeight, float GripStrength, float DeltaTime, float ErrorRange, float Kp, float Ki, float Kd)
	{
		return FMath::IsFinite(StableHeight)
			&& FMath::IsFinite(GripHeight)
			&& FMath::IsFinite(GripStrength)
			&& FMath::IsFinite(DeltaTime)
			&& FMath::IsFinite(ErrorRange)
			&& FMath::IsFinite(Kp)
			&& FMath::IsFinite(Ki)
			&& FMath::IsFinite(Kd);
	}

	bool ContainsInvalidTransform(const TArray<FTransform>& Transforms)
	{
		for (const FTransform& Transform : Transforms)
		{
			if (Transform.ContainsNaN())
			{
				return true;
			}
		}

		return false;
	}
}

TArray<FTransform> USupportSystemLibrary::GetLocalFulcrumTransform(const UPrimitiveComponent* TargetComponent, const FVector& PlaneBase)
{
	TArray<FTransform> FulcrumTransformArray;
	if (!IsValid(TargetComponent) || PlaneBase.ContainsNaN())
	{
		return FulcrumTransformArray;
	}

	const FVector ComponentExtent = TargetComponent->CalcBounds(FTransform::Identity).BoxExtent;
	const float BottomZ = TargetComponent->GetComponentTransform().InverseTransformPosition(PlaneBase).Z;

	FulcrumTransformArray.Add(FTransform(FVector(ComponentExtent.X, ComponentExtent.Y, BottomZ)));
	FulcrumTransformArray.Add(FTransform(FVector(-ComponentExtent.X, ComponentExtent.Y, BottomZ)));
	FulcrumTransformArray.Add(FTransform(FVector(ComponentExtent.X, -ComponentExtent.Y, BottomZ)));
	FulcrumTransformArray.Add(FTransform(FVector(-ComponentExtent.X, -ComponentExtent.Y, BottomZ)));

	return FulcrumTransformArray;
}

TArray<FTransform> USupportSystemLibrary::GetWorldFulcrumTransform(const FTransform& ObjectTransform, const TArray<FTransform>& FulcrumTransformArray)
{
	TArray<FTransform> WorldTransforms;
	if (ObjectTransform.ContainsNaN() || ContainsInvalidTransform(FulcrumTransformArray))
	{
		return WorldTransforms;
	}

	for (const FTransform& Transform : FulcrumTransformArray)
	{
		WorldTransforms.Add(Transform * ObjectTransform);
	}

	return WorldTransforms;
}

void USupportSystemLibrary::StabilizeHeight(
	UObject* WorldContextObject,
	UPrimitiveComponent* TargetComponent,
	const TArray<FTransform>& WorldFulcrumTransform,
	float StableHeight,
	float GripHeight,
	float GripStrength,
	float DeltaTime,
	float ErrorRange,
	TArray<float>& LastError,
	TArray<float>& IntegralError,
	float Kp,
	float Ki,
	float Kd,
	ETraceTypeQuery ChannelType,
	float& AveragePressureFactor,
	FVector& AverageImpactNormal,
	bool DrawDebug)
{
	ResetStabilizeHeightOutputs(AveragePressureFactor, AverageImpactNormal);

	if (!IsValid(TargetComponent)
		|| !WorldContextObject
		|| WorldFulcrumTransform.Num() == 0
		|| !IsFiniteStabilizeHeightInput(StableHeight, GripHeight, GripStrength, DeltaTime, ErrorRange, Kp, Ki, Kd)
		|| DeltaTime <= KINDA_SMALL_NUMBER
		|| ErrorRange <= KINDA_SMALL_NUMBER
		|| StableHeight < 0.0f
		|| GripHeight < 0.0f
		|| GripStrength < 0.0f)
	{
		return;
	}

	if (ContainsInvalidTransform(WorldFulcrumTransform))
	{
		LastError.Reset();
		IntegralError.Reset();
		return;
	}

	UWorld* World = XToolsBlueprintHelpers::GetValidWorld(WorldContextObject);
	if (!World)
	{
		return;
	}

	const float MassPerFulcrum = FMath::Max(TargetComponent->GetMass(), 0.0f) / WorldFulcrumTransform.Num();
	const ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(ChannelType);

	FCollisionQueryParams CollisionParams(SCENE_QUERY_STAT(StabilizeHeight), false, TargetComponent->GetOwner());
	CollisionParams.AddIgnoredComponent(TargetComponent);

	LastError.SetNum(WorldFulcrumTransform.Num());
	IntegralError.SetNum(WorldFulcrumTransform.Num());

	TArray<FHitResult> HitResults;
	HitResults.SetNum(WorldFulcrumTransform.Num());

	float TotalPressureFactor = 0.0f;
	FVector TotalImpactNormal = FVector::ZeroVector;

	for (int32 Index = 0; Index < WorldFulcrumTransform.Num(); ++Index)
	{
		const FTransform& FulcrumTransform = WorldFulcrumTransform[Index];
		const FVector SupportAxis = FulcrumTransform.GetUnitAxis(EAxis::Z);
		const FVector Start = FulcrumTransform.GetLocation();
		const FVector End = Start - SupportAxis * (StableHeight + GripHeight);
		const bool bHit = World->LineTraceSingleByChannel(HitResults[Index], Start, End, CollisionChannel, CollisionParams);

		if (DrawDebug)
		{
			DrawDebugLine(World, Start, End, FColor::Yellow, false, 0.0f, 0, 1.0f);
		}

		if (bHit)
		{
			const FVector StableLocation = Start - SupportAxis * StableHeight;
			const float Error = FMath::Clamp(FVector::DotProduct(HitResults[Index].ImpactPoint - StableLocation, SupportAxis), -ErrorRange, ErrorRange);
			TotalPressureFactor += FMath::Clamp(Error / ErrorRange, -1.0f, 1.0f);
			TotalImpactNormal += HitResults[Index].ImpactNormal;
		}
		else
		{
			TotalPressureFactor += -1.0f;
			TotalImpactNormal += SupportAxis;
		}
	}

	AveragePressureFactor = FMath::Clamp(TotalPressureFactor / WorldFulcrumTransform.Num(), -1.0f, 1.0f);
	AverageImpactNormal = TotalImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);

	for (int32 Index = 0; Index < WorldFulcrumTransform.Num(); ++Index)
	{
		const FTransform& FulcrumTransform = WorldFulcrumTransform[Index];
		const FVector SupportAxis = FulcrumTransform.GetUnitAxis(EAxis::Z);
		const FVector StableLocation = FulcrumTransform.GetLocation() - SupportAxis * StableHeight;
		const FHitResult& HitResult = HitResults[Index];

		if (HitResult.bBlockingHit)
		{
			if (DrawDebug)
			{
				DrawDebugPoint(World, HitResult.ImpactPoint, 10.0f, FColor::Red, false, 0.0f);
				DrawDebugLine(World, StableLocation, HitResult.ImpactPoint, FColor::Blue, false, 0.0f, 0, 1.0f);
			}

			const float Error = FMath::Clamp(FVector::DotProduct(HitResult.ImpactPoint - StableLocation, SupportAxis), -ErrorRange, ErrorRange);
			const float Dot = FMath::Clamp(FVector::DotProduct(SupportAxis, AverageImpactNormal), -1.0f, 1.0f);
			const float Angle = FMath::Acos(Dot);
			const float NormalFactor = FMath::Clamp(1.0f - (Angle / (PI / 4.0f)), 0.0f, 1.0f);
			const float Alpha = FMath::Exp(-DeltaTime);
			const float SmoothedError = Alpha * LastError[Index] + (1.0f - Alpha) * Error;

			IntegralError[Index] = FMath::Clamp(IntegralError[Index] + SmoothedError * DeltaTime, -ErrorRange, ErrorRange);

			const float Derivative = (SmoothedError - LastError[Index]) / DeltaTime;
			float Output = Kp * SmoothedError + Ki * IntegralError[Index] + Kd * Derivative;

			if (Error > 0.0f)
			{
				const FVector Force = SupportAxis * Output * MassPerFulcrum * NormalFactor;
				TargetComponent->AddForceAtLocation(Force, FulcrumTransform.GetLocation());
			}
			else if (Error < 0.0f)
			{
				const float GripPressure = FMath::Clamp(AveragePressureFactor + 1.0f, 0.0f, 1.0f);
				Output *= GripStrength * GripPressure;
				const FVector Force = SupportAxis * Output * MassPerFulcrum * NormalFactor;
				TargetComponent->AddForceAtLocation(Force, FulcrumTransform.GetLocation());
			}

			LastError[Index] = SmoothedError;
		}
		else
		{
			IntegralError[Index] = 0.0f;
			LastError[Index] = 0.0f;
		}

		if (DrawDebug)
		{
			DrawDebugPoint(World, StableLocation, 10.0f, FColor::Green, false, 0.0f);
		}
	}
}
