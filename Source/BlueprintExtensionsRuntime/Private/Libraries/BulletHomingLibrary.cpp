#include "Libraries/BulletHomingLibrary.h"

#include "Components/SceneComponent.h"
#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"

namespace
{
	struct FBulletTargetInfo
	{
		bool bHasLocation = false;
		bool bHasLiveTarget = false;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
	};

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool IsFiniteRotator(const FRotator& Value)
	{
		return FMath::IsFinite(Value.Pitch) && FMath::IsFinite(Value.Yaw) && FMath::IsFinite(Value.Roll);
	}

	FVector SafeDirectionOrFallback(const FVector& Direction, const FVector& Fallback)
	{
		const FVector SafeDirection = Direction.GetSafeNormal();
		if (!SafeDirection.IsNearlyZero() && IsFiniteVector(SafeDirection))
		{
			return SafeDirection;
		}

		const FVector SafeFallback = Fallback.GetSafeNormal();
		if (!SafeFallback.IsNearlyZero() && IsFiniteVector(SafeFallback))
		{
			return SafeFallback;
		}

		return FVector::ForwardVector;
	}

	float ClampSpeed(float Speed, float MaxSpeed)
	{
		const float SafeSpeed = FMath::Max(0.0f, FMath::IsFinite(Speed) ? Speed : 0.0f);
		if (MaxSpeed > 0.0f && FMath::IsFinite(MaxSpeed))
		{
			return FMath::Min(SafeSpeed, MaxSpeed);
		}

		return SafeSpeed;
	}

	float InterpScalar(float Current, float Target, float DeltaTime, float InterpRate)
	{
		if (InterpRate <= 0.0f || DeltaTime <= 0.0f)
		{
			return Target;
		}

		return FMath::FInterpTo(Current, Target, DeltaTime, InterpRate);
	}

	FVector InterpDirection(const FVector& CurrentDirection, const FVector& TargetDirection, float DeltaTime, float InterpRate)
	{
		if (InterpRate <= 0.0f || DeltaTime <= 0.0f)
		{
			return TargetDirection;
		}

		return SafeDirectionOrFallback(FMath::VInterpTo(CurrentDirection, TargetDirection, DeltaTime, InterpRate), TargetDirection);
	}

	FRotator InterpRotation(const FRotator& CurrentRotation, const FRotator& TargetRotation, float DeltaTime, float InterpRate)
	{
		if (InterpRate <= 0.0f || DeltaTime <= 0.0f)
		{
			return TargetRotation;
		}

		const FRotator Result = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, InterpRate);
		return IsFiniteRotator(Result) ? Result : TargetRotation;
	}

	FVector SafeVelocityOrZero(const FVector& Velocity)
	{
		return IsFiniteVector(Velocity) ? Velocity : FVector::ZeroVector;
	}

	FBulletTargetInfo ResolveTargetInfo(
		const USceneComponent* TargetComponent,
		const AActor* TargetActor,
		const FXToolsBulletHomingOptions& Options,
		const FXToolsBulletHomingState& State,
		float DeltaTime)
	{
		FBulletTargetInfo TargetInfo;
		if (IsValid(TargetComponent))
		{
			TargetInfo.Location = TargetComponent->GetComponentLocation();
			TargetInfo.Velocity = SafeVelocityOrZero(TargetComponent->GetComponentVelocity());
			TargetInfo.bHasLocation = IsFiniteVector(TargetInfo.Location);
			TargetInfo.bHasLiveTarget = TargetInfo.bHasLocation;
		}
		else if (IsValid(TargetActor))
		{
			TargetInfo.Location = TargetActor->GetActorLocation();
			TargetInfo.Velocity = SafeVelocityOrZero(TargetActor->GetVelocity());
			TargetInfo.bHasLocation = IsFiniteVector(TargetInfo.Location);
			TargetInfo.bHasLiveTarget = TargetInfo.bHasLocation;
		}
		else if (Options.bContinueAfterTargetInvalid && State.bHasLastTargetLocation && IsFiniteVector(State.LastTargetLocation))
		{
			TargetInfo.Location = State.LastTargetLocation;
			TargetInfo.Velocity = FVector::ZeroVector;
			TargetInfo.bHasLocation = true;
			TargetInfo.bHasLiveTarget = false;
		}

		if (TargetInfo.bHasLiveTarget && State.bHasLastTargetLocation && DeltaTime > KINDA_SMALL_NUMBER)
		{
			const FVector EstimatedVelocity = (TargetInfo.Location - State.LastTargetLocation) / DeltaTime;
			if (IsFiniteVector(EstimatedVelocity) && TargetInfo.Velocity.IsNearlyZero())
			{
				TargetInfo.Velocity = EstimatedVelocity;
			}
		}

		return TargetInfo;
	}

	float PositiveFiniteOrZero(float Value)
	{
		return Value > 0.0f && FMath::IsFinite(Value) ? Value : 0.0f;
	}

	FVector ResolveCurrentVelocityForGuidance(
		const UProjectileMovementComponent* ProjectileMovement,
		const FXToolsBulletHomingState& State)
	{
		if (IsValid(ProjectileMovement) && !ProjectileMovement->Velocity.IsNearlyZero() && IsFiniteVector(ProjectileMovement->Velocity))
		{
			return ProjectileMovement->Velocity;
		}

		const FVector StateVelocity = State.CurrentDirection * State.CurrentSpeed;
		return IsFiniteVector(StateVelocity) ? StateVelocity : FVector::ZeroVector;
	}

	FVector InitialDirectionFromInputs(const AActor* ProjectileActor, const UProjectileMovementComponent* ProjectileMovement)
	{
		if (IsValid(ProjectileMovement) && !ProjectileMovement->Velocity.IsNearlyZero() && IsFiniteVector(ProjectileMovement->Velocity))
		{
			return ProjectileMovement->Velocity;
		}

		if (IsValid(ProjectileActor))
		{
			return ProjectileActor->GetActorForwardVector();
		}

		return FVector::ForwardVector;
	}

	float InitialSpeedFromInputs(
		const UProjectileMovementComponent* ProjectileMovement,
		const FXToolsBulletHomingOptions& Options)
	{
		const float ExplicitInitialSpeed = PositiveFiniteOrZero(Options.InitialSpeed);
		if (ExplicitInitialSpeed > 0.0f)
		{
			return ExplicitInitialSpeed;
		}

		if (IsValid(ProjectileMovement) && !ProjectileMovement->Velocity.IsNearlyZero() && IsFiniteVector(ProjectileMovement->Velocity))
		{
			return ProjectileMovement->Velocity.Size();
		}

		if (IsValid(ProjectileMovement))
		{
			const float ProjectileInitialSpeed = PositiveFiniteOrZero(ProjectileMovement->InitialSpeed);
			if (ProjectileInitialSpeed > 0.0f)
			{
				return ProjectileInitialSpeed;
			}
		}

		return PositiveFiniteOrZero(Options.TargetSpeed);
	}

	bool DidTargetChange(
		const AActor* TargetActor,
		const USceneComponent* TargetComponent,
		const FXToolsBulletHomingState& State)
	{
		return State.LastTargetActor.Get() != TargetActor || State.LastTargetComponent.Get() != TargetComponent;
	}

	void ResetTargetDependentState(FXToolsBulletHomingState& State)
	{
		State.LastTargetLocation = FVector::ZeroVector;
		State.bHasLastTargetLocation = false;
		State.LastLineOfSightDirection = FVector::ForwardVector;
		State.bHasLastLineOfSightDirection = false;
		State.ClosestDistanceToTarget = 0.0f;
		State.bHasClosestDistance = false;
		State.LastProjectileLocation = FVector::ZeroVector;
		State.bHasLastProjectileLocation = false;
		State.DebugTrailPoints.Reset();
	}

	void UpdateCachedTargetIdentity(
		AActor* TargetActor,
		USceneComponent* TargetComponent,
		FXToolsBulletHomingState& State)
	{
		State.LastTargetActor = TargetActor;
		State.LastTargetComponent = TargetComponent;
	}

	float GetLaunchGuidanceScale(const FXToolsBulletHomingOptions& Options, float ElapsedTime)
	{
		if (Options.LaunchStraightTime <= 0.0f || ElapsedTime >= Options.LaunchStraightTime)
		{
			return 1.0f;
		}

		return FMath::Clamp(Options.LaunchGuidanceScale, 0.0f, 1.0f);
	}

	FVector CalculatePredictiveTargetLocation(
		const FVector& CurrentLocation,
		const FBulletTargetInfo& TargetInfo,
		const FXToolsBulletHomingOptions& Options,
		const FXToolsBulletHomingState& State)
	{
		if (Options.MaxPredictionTime <= 0.0f || State.CurrentSpeed <= KINDA_SMALL_NUMBER || !IsFiniteVector(TargetInfo.Velocity))
		{
			return TargetInfo.Location;
		}

		const float DistanceToTarget = FVector::Dist(CurrentLocation, TargetInfo.Location);
		const float RawPredictionTime = (DistanceToTarget / FMath::Max(State.CurrentSpeed, KINDA_SMALL_NUMBER)) * FMath::Max(Options.PredictionTimeScale, 0.0f);
		const float PredictionTime = FMath::Clamp(RawPredictionTime, 0.0f, FMath::Max(Options.MaxPredictionTime, 0.0f));
		const FVector PredictedLocation = TargetInfo.Location + TargetInfo.Velocity * PredictionTime;
		return IsFiniteVector(PredictedLocation) ? PredictedLocation : TargetInfo.Location;
	}

	bool TryCalculateProportionalNavigationVelocity(
		const FVector& CurrentLocation,
		const FVector& CurrentVelocity,
		const FBulletTargetInfo& TargetInfo,
		const FXToolsBulletHomingOptions& Options,
		float DeltaTime,
		float GuidanceScale,
		FVector& OutVelocity)
	{
		OutVelocity = CurrentVelocity;
		if (DeltaTime <= KINDA_SMALL_NUMBER || Options.NavigationGain <= 0.0f || !IsFiniteVector(CurrentLocation) ||
			!IsFiniteVector(CurrentVelocity) || CurrentVelocity.IsNearlyZero() || !IsFiniteVector(TargetInfo.Location))
		{
			return false;
		}

		const FVector RelativePosition = TargetInfo.Location - CurrentLocation;
		const float RangeSq = RelativePosition.SizeSquared();
		if (!FMath::IsFinite(RangeSq) || RangeSq <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector LineOfSightDirection = RelativePosition.GetSafeNormal();
		if (LineOfSightDirection.IsNearlyZero() || !IsFiniteVector(LineOfSightDirection) || !IsFiniteVector(TargetInfo.Velocity))
		{
			return false;
		}

		const FVector RelativeVelocity = TargetInfo.Velocity - CurrentVelocity;
		if (!IsFiniteVector(RelativeVelocity))
		{
			return false;
		}

		const float ClosingSpeed = FMath::Max(0.0f, -FVector::DotProduct(RelativeVelocity, LineOfSightDirection));
		if (ClosingSpeed <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector LineOfSightAngularRate = FVector::CrossProduct(RelativePosition, RelativeVelocity) / RangeSq;
		if (!IsFiniteVector(LineOfSightAngularRate))
		{
			return false;
		}

		FVector GuidanceAcceleration = FVector::CrossProduct(LineOfSightAngularRate, LineOfSightDirection) *
			(Options.NavigationGain * ClosingSpeed * FMath::Clamp(GuidanceScale, 0.0f, 1.0f));
		if (!IsFiniteVector(GuidanceAcceleration))
		{
			return false;
		}

		const float MaxGuidanceAcceleration = PositiveFiniteOrZero(Options.MaxGuidanceAcceleration);
		if (MaxGuidanceAcceleration > 0.0f)
		{
			GuidanceAcceleration = GuidanceAcceleration.GetClampedToMaxSize(MaxGuidanceAcceleration);
		}

		const FVector NewVelocity = CurrentVelocity + GuidanceAcceleration * DeltaTime;
		if (!IsFiniteVector(NewVelocity) || NewVelocity.IsNearlyZero())
		{
			return false;
		}

		OutVelocity = NewVelocity;
		return true;
	}

	float SmoothConvergenceBlend(float RawBlend)
	{
		const float ClampedBlend = FMath::Clamp(RawBlend, 0.0f, 1.0f);
		return ClampedBlend * ClampedBlend * (3.0f - 2.0f * ClampedBlend);
	}

	bool TryApplyTerminalConvergenceVelocity(
		const FVector& CurrentLocation,
		const FVector& InputVelocity,
		const FBulletTargetInfo& TargetInfo,
		const FXToolsBulletHomingOptions& Options,
		float GuidanceScale,
		FVector& OutVelocity)
	{
		OutVelocity = InputVelocity;
		const float ConvergenceDistance = PositiveFiniteOrZero(Options.TerminalConvergenceDistance);
		if (ConvergenceDistance <= KINDA_SMALL_NUMBER || !IsFiniteVector(CurrentLocation) || !IsFiniteVector(InputVelocity) ||
			InputVelocity.IsNearlyZero() || !IsFiniteVector(TargetInfo.Location))
		{
			return false;
		}

		const FVector ToTarget = TargetInfo.Location - CurrentLocation;
		const float DistanceToTarget = ToTarget.Size();
		if (!FMath::IsFinite(DistanceToTarget) || DistanceToTarget <= KINDA_SMALL_NUMBER || DistanceToTarget >= ConvergenceDistance)
		{
			return false;
		}

		const FVector LineOfSightDirection = ToTarget / DistanceToTarget;
		if (!IsFiniteVector(LineOfSightDirection) || LineOfSightDirection.IsNearlyZero())
		{
			return false;
		}

		const float CaptureRadius = FMath::Clamp(PositiveFiniteOrZero(Options.CaptureRadius), 0.0f, ConvergenceDistance - KINDA_SMALL_NUMBER);
		const float BlendRange = FMath::Max(ConvergenceDistance - CaptureRadius, KINDA_SMALL_NUMBER);
		const float RawBlend = 1.0f - ((DistanceToTarget - CaptureRadius) / BlendRange);
		const float Blend = SmoothConvergenceBlend(RawBlend) * FMath::Clamp(GuidanceScale, 0.0f, 1.0f);
		if (Blend <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float CurrentSpeed = InputVelocity.Size();
		if (!FMath::IsFinite(CurrentSpeed) || CurrentSpeed <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float RadialSpeed = FVector::DotProduct(InputVelocity, LineOfSightDirection);
		const FVector TangentialVelocity = InputVelocity - LineOfSightDirection * RadialSpeed;
		if (!IsFiniteVector(TangentialVelocity))
		{
			return false;
		}

		const float TangentialDamping = FMath::Clamp(Options.TerminalTangentialDamping, 0.0f, 1.0f) * Blend;
		const FVector DampedTangentialVelocity = TangentialVelocity * (1.0f - TangentialDamping);
		const float RadialPullStrength = FMath::Clamp(Options.TerminalRadialPullStrength, 0.0f, 1.0f);
		const float MinimumRadialSpeed = CurrentSpeed * RadialPullStrength * Blend;
		const float ConvergedRadialSpeed = FMath::Max(RadialSpeed, MinimumRadialSpeed);
		FVector ConvergedVelocity = DampedTangentialVelocity + LineOfSightDirection * ConvergedRadialSpeed;
		if (!IsFiniteVector(ConvergedVelocity) || ConvergedVelocity.IsNearlyZero())
		{
			return false;
		}

		const float OutputSpeed = ClampSpeed(FMath::Max(CurrentSpeed, ConvergedVelocity.Size()), Options.MaxSpeed);
		if (OutputSpeed > KINDA_SMALL_NUMBER)
		{
			ConvergedVelocity = SafeDirectionOrFallback(ConvergedVelocity, InputVelocity) * OutputSpeed;
		}

		if (!IsFiniteVector(ConvergedVelocity) || ConvergedVelocity.IsNearlyZero())
		{
			return false;
		}

		OutVelocity = ConvergedVelocity;
		return true;
	}

	bool IsTerminalStatus(EXToolsBulletHomingStatus Status)
	{
		return Status == EXToolsBulletHomingStatus::Captured || Status == EXToolsBulletHomingStatus::PassedTarget;
	}

	float CalculateClosestFrameDistanceToTarget(
		const FVector& CurrentLocation,
		const FVector& TargetLocation,
		const FXToolsBulletHomingState& State)
	{
		if (State.bHasLastProjectileLocation && IsFiniteVector(State.LastProjectileLocation))
		{
			const FVector ClosestPoint = FMath::ClosestPointOnSegment(TargetLocation, State.LastProjectileLocation, CurrentLocation);
			return FVector::Dist(ClosestPoint, TargetLocation);
		}

		return FVector::Dist(CurrentLocation, TargetLocation);
	}

	bool DidCrossTargetPlane(
		const FVector& CurrentLocation,
		const FVector& TargetLocation,
		const FXToolsBulletHomingState& State)
	{
		if (!State.bHasLastProjectileLocation || !IsFiniteVector(State.LastProjectileLocation))
		{
			return false;
		}

		const FVector PreviousToTarget = TargetLocation - State.LastProjectileLocation;
		const FVector CurrentToTarget = TargetLocation - CurrentLocation;
		return IsFiniteVector(PreviousToTarget) && IsFiniteVector(CurrentToTarget) &&
			!PreviousToTarget.IsNearlyZero() && FVector::DotProduct(PreviousToTarget, CurrentToTarget) <= 0.0f;
	}

	EXToolsBulletHomingStatus EvaluateTerminalStatus(
		const FVector& CurrentLocation,
		const FVector& TargetLocation,
		float DistanceToTarget,
		const FXToolsBulletHomingOptions& Options,
		const FXToolsBulletHomingState& State)
	{
		const float CaptureRadius = PositiveFiniteOrZero(Options.CaptureRadius);
		const float ClosestFrameDistance = CalculateClosestFrameDistanceToTarget(CurrentLocation, TargetLocation, State);
		if (CaptureRadius > 0.0f)
		{
			if (ClosestFrameDistance <= CaptureRadius)
			{
				return EXToolsBulletHomingStatus::Captured;
			}
		}

		const float PassedTargetDistance = PositiveFiniteOrZero(Options.PassedTargetDistance);
		if ((Options.bDetectPassedTarget || Options.bCaptureWhenPassedTarget) && PassedTargetDistance > 0.0f && State.bHasClosestDistance)
		{
			constexpr float PassedDistanceHysteresis = 1.0f;
			const bool bStartedMovingAway = State.ClosestDistanceToTarget <= PassedTargetDistance &&
				DistanceToTarget > State.ClosestDistanceToTarget + PassedDistanceHysteresis;
			const bool bCrossedTargetPlane = ClosestFrameDistance <= PassedTargetDistance &&
				DidCrossTargetPlane(CurrentLocation, TargetLocation, State);
			if (bStartedMovingAway || bCrossedTargetPlane)
			{
				return Options.bCaptureWhenPassedTarget ? EXToolsBulletHomingStatus::Captured : EXToolsBulletHomingStatus::PassedTarget;
			}
		}

		return EXToolsBulletHomingStatus::Tracking;
	}

	void UpdateClosestDistance(float DistanceToTarget, FXToolsBulletHomingState& State)
	{
		if (!FMath::IsFinite(DistanceToTarget))
		{
			return;
		}

		if (!State.bHasClosestDistance || DistanceToTarget < State.ClosestDistanceToTarget)
		{
			State.ClosestDistanceToTarget = DistanceToTarget;
			State.bHasClosestDistance = true;
		}
	}

	void UpdateDebugTrail(const FVector& CurrentLocation, const FXToolsBulletHomingOptions& Options, FXToolsBulletHomingState& State)
	{
		if (!Options.bDrawDebug || !Options.bDrawDebugTrail || !IsFiniteVector(CurrentLocation))
		{
			return;
		}

		if (State.DebugTrailPoints.Num() == 0 || !State.DebugTrailPoints.Last().Equals(CurrentLocation, KINDA_SMALL_NUMBER))
		{
			State.DebugTrailPoints.Add(CurrentLocation);
		}

		const int32 MaxTrailPoints = FMath::Clamp(Options.MaxDebugTrailPoints, 2, 512);
		const int32 ExcessCount = State.DebugTrailPoints.Num() - MaxTrailPoints;
		if (ExcessCount > 0)
		{
			State.DebugTrailPoints.RemoveAt(0, ExcessCount, false);
		}
	}

	void SetOutputsFromState(
		const AActor* ProjectileActor,
		const USceneComponent* VisualComponent,
		const FXToolsBulletHomingOptions& Options,
		const FXToolsBulletHomingState& State,
		FVector& OutWorldVelocity,
		FRotator& OutActorRotation,
		FRotator& OutVisualRotation)
	{
		const FVector SafeDirection = SafeDirectionOrFallback(State.CurrentDirection, FVector::ForwardVector);
		OutWorldVelocity = SafeDirection * ClampSpeed(State.CurrentSpeed, Options.MaxSpeed);

		const FRotator TargetRotation = SafeDirection.Rotation();
		OutActorRotation = IsValid(ProjectileActor) ? ProjectileActor->GetActorRotation() : TargetRotation;
		OutVisualRotation = IsValid(VisualComponent)
			? VisualComponent->GetComponentRotation()
			: (TargetRotation + Options.VisualRotationOffset).GetNormalized();
	}

	UWorld* ResolveWorld(const AActor* ProjectileActor, const UProjectileMovementComponent* ProjectileMovement)
	{
		if (IsValid(ProjectileActor))
		{
			return ProjectileActor->GetWorld();
		}

		return IsValid(ProjectileMovement) ? ProjectileMovement->GetWorld() : nullptr;
	}

	void DrawHomingDebug(
		const AActor* ProjectileActor,
		const UProjectileMovementComponent* ProjectileMovement,
		const FXToolsBulletHomingOptions& Options,
		const FVector& CurrentLocation,
		const FBulletTargetInfo& TargetInfo,
		const FVector& AimLocation,
		const FVector& WorldVelocity,
		const TArray<FVector>& TrailPoints)
	{
#if ENABLE_DRAW_DEBUG
		if (!Options.bDrawDebug)
		{
			return;
		}

		UWorld* World = ResolveWorld(ProjectileActor, ProjectileMovement);
		if (!World)
		{
			return;
		}

		const float LifeTime = FMath::Max(Options.DebugDrawTime, 0.0f);
		constexpr uint8 DepthPriority = 0;
		constexpr float LineThickness = 1.5f;

		if (TargetInfo.bHasLocation)
		{
			const FColor TargetLineColor = TargetInfo.bHasLiveTarget ? FColor::Yellow : FColor::Orange;
			DrawDebugLine(World, CurrentLocation, TargetInfo.Location, TargetLineColor, false, LifeTime, DepthPriority, LineThickness);
			DrawDebugSphere(World, TargetInfo.Location, 16.0f, 12, TargetLineColor, false, LifeTime, DepthPriority, LineThickness);

			if (!AimLocation.Equals(TargetInfo.Location, KINDA_SMALL_NUMBER))
			{
				DrawDebugLine(World, CurrentLocation, AimLocation, FColor::Blue, false, LifeTime, DepthPriority, LineThickness);
				DrawDebugSphere(World, AimLocation, 12.0f, 12, FColor::Blue, false, LifeTime, DepthPriority, LineThickness);
			}

			const float CaptureRadius = PositiveFiniteOrZero(Options.CaptureRadius);
			if (CaptureRadius > 0.0f && TargetInfo.bHasLiveTarget)
			{
				DrawDebugSphere(World, TargetInfo.Location, CaptureRadius, 24, FColor::Green, false, LifeTime, DepthPriority, 0.75f);
			}
		}

		const FVector VelocityDirection = WorldVelocity.GetSafeNormal();
		if (!VelocityDirection.IsNearlyZero() && IsFiniteVector(VelocityDirection))
		{
			const float VelocityLineLength = FMath::Max(Options.DebugVelocityLineLength, 0.0f);
			if (VelocityLineLength > KINDA_SMALL_NUMBER)
			{
				DrawDebugLine(World, CurrentLocation, CurrentLocation + VelocityDirection * VelocityLineLength, FColor::Cyan, false, LifeTime, DepthPriority, LineThickness);
			}
		}

		if (Options.bDrawDebugTrail && TrailPoints.Num() >= 2)
		{
			for (int32 PointIndex = 1; PointIndex < TrailPoints.Num(); ++PointIndex)
			{
				const FVector& PreviousPoint = TrailPoints[PointIndex - 1];
				const FVector& CurrentPoint = TrailPoints[PointIndex];
				if (IsFiniteVector(PreviousPoint) && IsFiniteVector(CurrentPoint))
				{
					DrawDebugLine(World, PreviousPoint, CurrentPoint, FColor::Purple, false, LifeTime, DepthPriority, 1.25f);
				}
			}
		}
#else
		(void)ProjectileActor;
		(void)ProjectileMovement;
		(void)Options;
		(void)CurrentLocation;
		(void)TargetInfo;
		(void)AimLocation;
		(void)WorldVelocity;
		(void)TrailPoints;
#endif
	}

	float ResolveProjectileMovementMaxSpeed(const FXToolsBulletHomingOptions& Options)
	{
		return Options.MaxSpeed > 0.0f && FMath::IsFinite(Options.MaxSpeed) ? Options.MaxSpeed : 0.0f;
	}

	void ApplyProjectileMovementVelocity(
		UProjectileMovementComponent* ProjectileMovement,
		const FVector& Velocity,
		const FXToolsBulletHomingOptions& Options)
	{
		if (!IsValid(ProjectileMovement))
		{
			return;
		}

		ProjectileMovement->MaxSpeed = ResolveProjectileMovementMaxSpeed(Options);
		ProjectileMovement->Velocity = ProjectileMovement->LimitVelocity(Velocity);
		ProjectileMovement->UpdateComponentVelocity();
	}
}

bool UBulletHomingLibrary::UpdateHomingProjectileMovement(
	AActor* ProjectileActor,
	UProjectileMovementComponent* ProjectileMovement,
	AActor* TargetActor,
	USceneComponent* TargetComponent,
	USceneComponent* VisualComponent,
	float DeltaTime,
	const FXToolsBulletHomingOptions& Options,
	FXToolsBulletHomingState& State,
	EXToolsBulletHomingStatus& OutStatus,
	FVector& OutWorldVelocity,
	FRotator& OutActorRotation,
	FRotator& OutVisualRotation)
{
	OutStatus = EXToolsBulletHomingStatus::Invalid;
	OutWorldVelocity = FVector::ZeroVector;
	OutActorRotation = IsValid(ProjectileActor) ? ProjectileActor->GetActorRotation() : FRotator::ZeroRotator;
	OutVisualRotation = IsValid(VisualComponent) ? VisualComponent->GetComponentRotation() : FRotator::ZeroRotator;

	if (!IsValid(ProjectileActor) && !IsValid(ProjectileMovement))
	{
		return false;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, FMath::IsFinite(DeltaTime) ? DeltaTime : 0.0f);
	if (DidTargetChange(TargetActor, TargetComponent, State))
	{
		ResetTargetDependentState(State);
		UpdateCachedTargetIdentity(TargetActor, TargetComponent, State);
	}
	const FBulletTargetInfo TargetInfo = ResolveTargetInfo(TargetComponent, TargetActor, Options, State, SafeDeltaTime);

	const FVector CurrentLocation = IsValid(ProjectileMovement) && IsValid(ProjectileMovement->UpdatedComponent)
		? ProjectileMovement->UpdatedComponent->GetComponentLocation()
		: (IsValid(ProjectileActor) ? ProjectileActor->GetActorLocation() : FVector::ZeroVector);

	const FVector InitialDirection = InitialDirectionFromInputs(ProjectileActor, ProjectileMovement);
	if (!State.bInitialized)
	{
		State.CurrentDirection = SafeDirectionOrFallback(InitialDirection, IsValid(ProjectileActor) ? ProjectileActor->GetActorForwardVector() : FVector::ForwardVector);
		State.CurrentSpeed = ClampSpeed(InitialSpeedFromInputs(ProjectileMovement, Options), Options.MaxSpeed);
		State.bInitialized = true;
	}
	const float PreviousElapsedTime = State.ElapsedTime;
	State.ElapsedTime += SafeDeltaTime;

	FVector TargetDirection = State.CurrentDirection;
	FVector AimLocation = FVector::ZeroVector;
	EXToolsBulletHomingStatus TerminalStatus = EXToolsBulletHomingStatus::Tracking;
	if (TargetInfo.bHasLocation)
	{
		const float DistanceToTarget = FVector::Dist(CurrentLocation, TargetInfo.Location);
		if (TargetInfo.bHasLiveTarget)
		{
			TerminalStatus = EvaluateTerminalStatus(CurrentLocation, TargetInfo.Location, DistanceToTarget, Options, State);
			UpdateClosestDistance(DistanceToTarget, State);
		}
		else
		{
			TerminalStatus = EXToolsBulletHomingStatus::TargetInvalid;
		}

		State.LastTargetLocation = TargetInfo.Location;
		State.bHasLastTargetLocation = true;

		AimLocation = TargetInfo.Location;
		if (Options.GuidanceMode == EXToolsBulletGuidanceMode::PredictiveIntercept)
		{
			AimLocation = CalculatePredictiveTargetLocation(CurrentLocation, TargetInfo, Options, State);
		}

		TargetDirection = SafeDirectionOrFallback(AimLocation - CurrentLocation, State.CurrentDirection);
	}
	else if (!Options.bContinueAfterTargetInvalid)
	{
		OutStatus = EXToolsBulletHomingStatus::TargetInvalid;
		SetOutputsFromState(ProjectileActor, VisualComponent, Options, State, OutWorldVelocity, OutActorRotation, OutVisualRotation);
		UpdateDebugTrail(CurrentLocation, Options, State);
		State.LastProjectileLocation = CurrentLocation;
		State.bHasLastProjectileLocation = true;
		return false;
	}

	if (Options.bStopMovementOnTerminalStatus && IsTerminalStatus(TerminalStatus))
	{
		OutStatus = TerminalStatus;
		OutWorldVelocity = FVector::ZeroVector;
		State.CurrentSpeed = 0.0f;
		const FRotator CurrentRotation = SafeDirectionOrFallback(State.CurrentDirection, FVector::ForwardVector).Rotation();
		OutActorRotation = IsValid(ProjectileActor) ? ProjectileActor->GetActorRotation() : CurrentRotation;
		OutVisualRotation = IsValid(VisualComponent) ? VisualComponent->GetComponentRotation() : (CurrentRotation + Options.VisualRotationOffset).GetNormalized();

		if (IsValid(ProjectileMovement))
		{
			ApplyProjectileMovementVelocity(ProjectileMovement, FVector::ZeroVector, Options);
		}

		UpdateDebugTrail(CurrentLocation, Options, State);
		DrawHomingDebug(ProjectileActor, ProjectileMovement, Options, CurrentLocation, TargetInfo, AimLocation, OutWorldVelocity, State.DebugTrailPoints);

		State.LastProjectileLocation = CurrentLocation;
		State.bHasLastProjectileLocation = true;
		return true;
	}

	const float RequestedTargetSpeed = Options.TargetSpeed > 0.0f ? Options.TargetSpeed : State.CurrentSpeed;
	const float TargetSpeed = ClampSpeed(RequestedTargetSpeed, Options.MaxSpeed);
	State.CurrentSpeed = ClampSpeed(InterpScalar(State.CurrentSpeed, TargetSpeed, SafeDeltaTime, Options.SpeedInterpRate), Options.MaxSpeed);
	const float GuidanceScale = GetLaunchGuidanceScale(Options, PreviousElapsedTime);
	const FVector GuidanceCurrentDirection = SafeDirectionOrFallback(
		ResolveCurrentVelocityForGuidance(ProjectileMovement, State),
		State.CurrentDirection);
	const FVector CurrentVelocity = GuidanceCurrentDirection * State.CurrentSpeed;
	if (TargetInfo.bHasLocation && Options.GuidanceMode == EXToolsBulletGuidanceMode::ProportionalNavigation)
	{
		const FVector LineOfSightDirection = SafeDirectionOrFallback(TargetInfo.Location - CurrentLocation, State.CurrentDirection);
		FVector GuidedVelocity = CurrentVelocity;
		const bool bAppliedPN = TryCalculateProportionalNavigationVelocity(
			CurrentLocation,
			CurrentVelocity,
			TargetInfo,
			Options,
			SafeDeltaTime,
			GuidanceScale,
			GuidedVelocity);

		if (bAppliedPN)
		{
			State.CurrentDirection = SafeDirectionOrFallback(GuidedVelocity, State.CurrentDirection);
		}
		else
		{
			const FVector ScaledTargetDirection = GuidanceScale < 1.0f
				? SafeDirectionOrFallback(FMath::Lerp(State.CurrentDirection, TargetDirection, GuidanceScale), State.CurrentDirection)
				: TargetDirection;
			State.CurrentDirection = InterpDirection(State.CurrentDirection, ScaledTargetDirection, SafeDeltaTime, Options.DirectionInterpRate);
		}

		FVector TerminalVelocity = State.CurrentDirection * State.CurrentSpeed;
		if (TryApplyTerminalConvergenceVelocity(CurrentLocation, TerminalVelocity, TargetInfo, Options, GuidanceScale, TerminalVelocity))
		{
			State.CurrentDirection = SafeDirectionOrFallback(TerminalVelocity, State.CurrentDirection);
		}

		State.LastLineOfSightDirection = LineOfSightDirection;
		State.bHasLastLineOfSightDirection = true;
	}
	else
	{
		const FVector ScaledTargetDirection = GuidanceScale < 1.0f
			? SafeDirectionOrFallback(FMath::Lerp(State.CurrentDirection, TargetDirection, GuidanceScale), State.CurrentDirection)
			: TargetDirection;
		State.CurrentDirection = InterpDirection(State.CurrentDirection, ScaledTargetDirection, SafeDeltaTime, Options.DirectionInterpRate);

		if (TargetInfo.bHasLocation)
		{
			State.LastLineOfSightDirection = SafeDirectionOrFallback(TargetInfo.Location - CurrentLocation, State.CurrentDirection);
			State.bHasLastLineOfSightDirection = true;
		}
	}

	OutWorldVelocity = State.CurrentDirection * State.CurrentSpeed;
	if (!IsFiniteVector(OutWorldVelocity))
	{
		OutWorldVelocity = FVector::ZeroVector;
		OutStatus = EXToolsBulletHomingStatus::Invalid;
		UpdateDebugTrail(CurrentLocation, Options, State);
		State.LastProjectileLocation = CurrentLocation;
		State.bHasLastProjectileLocation = true;
		return false;
	}

	if (Options.bApplyVelocityToProjectileMovement && IsValid(ProjectileMovement))
	{
		ApplyProjectileMovementVelocity(ProjectileMovement, OutWorldVelocity, Options);
		OutWorldVelocity = ProjectileMovement->Velocity;
		State.CurrentSpeed = OutWorldVelocity.Size();
		State.CurrentDirection = SafeDirectionOrFallback(OutWorldVelocity, State.CurrentDirection);
	}

	const FRotator TargetRotation = State.CurrentDirection.Rotation();
	if (Options.bUpdateActorRotation && IsValid(ProjectileActor))
	{
		OutActorRotation = InterpRotation(ProjectileActor->GetActorRotation(), TargetRotation, SafeDeltaTime, Options.RotationInterpRate);
		ProjectileActor->SetActorRotation(OutActorRotation);
	}
	else
	{
		OutActorRotation = TargetRotation;
	}

	if (Options.bUpdateVisualComponentRotation && IsValid(VisualComponent))
	{
		const FRotator TargetVisualRotation = (TargetRotation + Options.VisualRotationOffset).GetNormalized();
		OutVisualRotation = InterpRotation(VisualComponent->GetComponentRotation(), TargetVisualRotation, SafeDeltaTime, Options.RotationInterpRate);
		VisualComponent->SetWorldRotation(OutVisualRotation);
	}
	else
	{
		OutVisualRotation = (TargetRotation + Options.VisualRotationOffset).GetNormalized();
	}

	UpdateDebugTrail(CurrentLocation, Options, State);
	DrawHomingDebug(ProjectileActor, ProjectileMovement, Options, CurrentLocation, TargetInfo, AimLocation, OutWorldVelocity, State.DebugTrailPoints);

	State.LastProjectileLocation = CurrentLocation;
	State.bHasLastProjectileLocation = true;

	OutStatus = TargetInfo.bHasLiveTarget ? TerminalStatus : EXToolsBulletHomingStatus::TargetInvalid;
	return true;
}

void UBulletHomingLibrary::ResetHomingProjectileState(FXToolsBulletHomingState& State)
{
	State = FXToolsBulletHomingState();
}
