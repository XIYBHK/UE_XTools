#include "Libraries/BulletHomingLibrary.h"

#include "Components/SceneComponent.h"
#if defined(ENABLE_DRAW_DEBUG) && ENABLE_DRAW_DEBUG
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
		bool bUsesTargetComponent = false;
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

	FRotator ComposeVisualRotation(const FRotator& TargetRotation, const FRotator& VisualRotationOffset)
	{
		const FRotator SafeTargetRotation = IsFiniteRotator(TargetRotation) ? TargetRotation : FRotator::ZeroRotator;
		const FRotator SafeVisualRotationOffset = IsFiniteRotator(VisualRotationOffset) ? VisualRotationOffset : FRotator::ZeroRotator;
		const FRotator Result = (SafeTargetRotation.Quaternion() * SafeVisualRotationOffset.Quaternion()).Rotator().GetNormalized();
		return IsFiniteRotator(Result) ? Result : SafeTargetRotation.GetNormalized();
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

	FVector SlerpDirection(const FVector& CurrentDirection, const FVector& TargetDirection, float Alpha)
	{
		const FVector SafeCurrentDirection = SafeDirectionOrFallback(CurrentDirection, TargetDirection);
		const FVector SafeTargetDirection = SafeDirectionOrFallback(TargetDirection, SafeCurrentDirection);
		const float SafeAlpha = FMath::Clamp(FMath::IsFinite(Alpha) ? Alpha : 0.0f, 0.0f, 1.0f);
		const FQuat DeltaRotation = FQuat::FindBetweenNormals(SafeCurrentDirection, SafeTargetDirection);
		const FVector InterpolatedDirection = FQuat::Slerp(FQuat::Identity, DeltaRotation, SafeAlpha).RotateVector(SafeCurrentDirection);
		return SafeDirectionOrFallback(InterpolatedDirection, SafeTargetDirection);
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

	float InterpScalar(float Current, float Target, float DeltaTime, float InterpRate, bool bUseImmediateInterpolation)
	{
		if (DeltaTime <= 0.0f)
		{
			return Current;
		}

		if (bUseImmediateInterpolation)
		{
			return Target;
		}

		if (InterpRate <= 0.0f)
		{
			return Current;
		}

		return FMath::FInterpTo(Current, Target, DeltaTime, InterpRate);
	}

	FVector InterpDirection(
		const FVector& CurrentDirection,
		const FVector& TargetDirection,
		float DeltaTime,
		float InterpRate,
		bool bUseImmediateGuidance)
	{
		if (DeltaTime <= 0.0f)
		{
			return CurrentDirection;
		}

		if (bUseImmediateGuidance)
		{
			return TargetDirection;
		}

		if (InterpRate <= 0.0f)
		{
			return CurrentDirection;
		}

		return SlerpDirection(CurrentDirection, TargetDirection, DeltaTime * InterpRate);
	}

	FRotator InterpRotation(const FRotator& CurrentRotation, const FRotator& TargetRotation, float DeltaTime, float InterpRate)
	{
		if (DeltaTime <= 0.0f)
		{
			return CurrentRotation;
		}

		if (InterpRate <= 0.0f)
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
		const FXToolsBulletHomingState& State)
	{
		FBulletTargetInfo TargetInfo;
		if (IsValid(TargetComponent))
		{
			const FVector ComponentLocation = TargetComponent->GetComponentLocation();
			if (IsFiniteVector(ComponentLocation))
			{
				TargetInfo.Location = ComponentLocation;
				TargetInfo.Velocity = SafeVelocityOrZero(TargetComponent->GetComponentVelocity());
				TargetInfo.bHasLocation = true;
				TargetInfo.bHasLiveTarget = true;
				TargetInfo.bUsesTargetComponent = true;
			}
		}

		if (!TargetInfo.bHasLiveTarget && IsValid(TargetActor))
		{
			const FVector ActorLocation = TargetActor->GetActorLocation();
			if (IsFiniteVector(ActorLocation))
			{
				TargetInfo.Location = ActorLocation;
				TargetInfo.Velocity = SafeVelocityOrZero(TargetActor->GetVelocity());
				TargetInfo.bHasLocation = true;
				TargetInfo.bHasLiveTarget = true;
			}
		}

		if (!TargetInfo.bHasLiveTarget && Options.bContinueAfterTargetInvalid && State.bHasLastTargetLocation && IsFiniteVector(State.LastTargetLocation))
		{
			TargetInfo.Location = State.LastTargetLocation;
			TargetInfo.Velocity = FVector::ZeroVector;
			TargetInfo.bHasLocation = true;
			TargetInfo.bHasLiveTarget = false;
		}

		return TargetInfo;
	}

	float PositiveFiniteOrZero(float Value)
	{
		return Value > 0.0f && FMath::IsFinite(Value) ? Value : 0.0f;
	}

	float AdvanceInterpRate(float CurrentRate, float InitialRate, float GrowthRate, float MaxRate, float DeltaTime)
	{
		const float SafeInitialRate = PositiveFiniteOrZero(InitialRate);
		const float SafeGrowthRate = PositiveFiniteOrZero(GrowthRate);
		const float SafeMaxRate = FMath::Max(SafeInitialRate, PositiveFiniteOrZero(MaxRate));
		const float SafeCurrentRate = FMath::IsFinite(CurrentRate)
			? FMath::Clamp(CurrentRate, SafeInitialRate, SafeMaxRate)
			: SafeInitialRate;

		return FMath::Clamp(SafeCurrentRate + SafeGrowthRate * DeltaTime, SafeInitialRate, SafeMaxRate);
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

		if (IsValid(ProjectileMovement) && IsValid(ProjectileMovement->UpdatedComponent))
		{
			return ProjectileMovement->UpdatedComponent->GetForwardVector();
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
		const AActor* ValidTargetActor = IsValid(TargetActor) ? TargetActor : nullptr;
		const USceneComponent* ValidTargetComponent = IsValid(TargetComponent) ? TargetComponent : nullptr;
		return State.LastTargetActor.Get() != ValidTargetActor || State.LastTargetComponent.Get() != ValidTargetComponent;
	}

	void ResetTargetDependentState(FXToolsBulletHomingState& State)
	{
		State.LastTargetLocation = FVector::ZeroVector;
		State.bHasLastTargetLocation = false;
		State.LastLineOfSightDirection = FVector::ForwardVector;
		State.bHasLastLineOfSightDirection = false;
		State.ClosestDistanceToTarget = 0.0f;
		State.bHasClosestDistance = false;
		State.InitialDistanceToTarget = 0.0f;
		State.LastProjectileLocation = FVector::ZeroVector;
		State.bHasLastProjectileLocation = false;
		State.DebugTrailPoints.Reset();
	}

	void UpdateCachedTargetIdentity(
		AActor* TargetActor,
		USceneComponent* TargetComponent,
		FXToolsBulletHomingState& State)
	{
		State.LastTargetActor = IsValid(TargetActor) ? TargetActor : nullptr;
		State.LastTargetComponent = IsValid(TargetComponent) ? TargetComponent : nullptr;
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
		float DeltaTime,
		float GuidanceScale,
		FVector& OutVelocity)
	{
		OutVelocity = InputVelocity;
		if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
		{
			return false;
		}

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

		const FVector TargetVelocity = SafeVelocityOrZero(TargetInfo.Velocity);
		const FVector RelativeVelocity = InputVelocity - TargetVelocity;
		if (!IsFiniteVector(RelativeVelocity))
		{
			return false;
		}

		const float RadialSpeed = FVector::DotProduct(RelativeVelocity, LineOfSightDirection);
		const FVector TangentialVelocity = RelativeVelocity - LineOfSightDirection * RadialSpeed;
		if (!IsFiniteVector(TangentialVelocity))
		{
			return false;
		}

		const float BaseTangentialDamping = FMath::Clamp(Options.TerminalTangentialDamping, 0.0f, 1.0f) * Blend;
		constexpr float TerminalDampingReferenceFrameRate = 60.0f;
		const float DampingFrameCount = DeltaTime * TerminalDampingReferenceFrameRate;
		const float TangentialDamping = FMath::Clamp(
			1.0f - FMath::Pow(1.0f - BaseTangentialDamping, DampingFrameCount),
			0.0f,
			1.0f);
		const FVector DampedTangentialVelocity = TangentialVelocity * (1.0f - TangentialDamping);
		const float RadialPullStrength = FMath::Clamp(Options.TerminalRadialPullStrength, 0.0f, 1.0f);
		const float MinimumRadialSpeed = CurrentSpeed * RadialPullStrength * Blend;
		const float ConvergedRadialSpeed = FMath::Max(RadialSpeed, MinimumRadialSpeed);
		const FVector ConvergedRelativeVelocity = DampedTangentialVelocity + LineOfSightDirection * ConvergedRadialSpeed;
		FVector ConvergedVelocity = TargetVelocity + ConvergedRelativeVelocity;
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
			if (State.bHasLastTargetLocation && IsFiniteVector(State.LastTargetLocation))
			{
				const FVector PreviousRelativeLocation = State.LastTargetLocation - State.LastProjectileLocation;
				const FVector CurrentRelativeLocation = TargetLocation - CurrentLocation;
				if (IsFiniteVector(PreviousRelativeLocation) && IsFiniteVector(CurrentRelativeLocation))
				{
					const FVector ClosestRelativeLocation = FMath::ClosestPointOnSegment(
						FVector::ZeroVector,
						PreviousRelativeLocation,
						CurrentRelativeLocation);
					return ClosestRelativeLocation.Size();
				}
			}

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

		const FVector PreviousTargetLocation = State.bHasLastTargetLocation && IsFiniteVector(State.LastTargetLocation)
			? State.LastTargetLocation
			: TargetLocation;
		const FVector PreviousToTarget = PreviousTargetLocation - State.LastProjectileLocation;
		const FVector CurrentToTarget = TargetLocation - CurrentLocation;
		return IsFiniteVector(PreviousToTarget) && IsFiniteVector(CurrentToTarget) &&
			!PreviousToTarget.IsNearlyZero() && FVector::DotProduct(PreviousToTarget, CurrentToTarget) <= 0.0f;
	}

	EXToolsBulletHomingStatus EvaluateTerminalStatus(
		const FVector& CurrentLocation,
		const FVector& TargetLocation,
		float DistanceToTarget,
		float ClosestFrameDistance,
		const FXToolsBulletHomingOptions& Options,
		const FXToolsBulletHomingState& State)
	{
		const float CaptureRadius = PositiveFiniteOrZero(Options.CaptureRadius);
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
			const float ClosestDistanceSoFar = FMath::Min(State.ClosestDistanceToTarget, ClosestFrameDistance);
			const bool bStartedMovingAway = ClosestDistanceSoFar <= PassedTargetDistance &&
				DistanceToTarget > ClosestDistanceSoFar + PassedDistanceHysteresis;
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
			: ComposeVisualRotation(TargetRotation, Options.VisualRotationOffset);
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
#if defined(ENABLE_DRAW_DEBUG) && ENABLE_DRAW_DEBUG
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

	void RestoreTerminalStoppedProjectileMovement(FXToolsBulletHomingState& State)
	{
		if (UProjectileMovementComponent* StoppedProjectileMovement = State.TerminalStoppedProjectileMovement.Get())
		{
			if (State.SpeedBeforeTerminalStop > KINDA_SMALL_NUMBER && StoppedProjectileMovement->Velocity.IsNearlyZero())
			{
				const FVector ResumeVelocity = SafeDirectionOrFallback(State.CurrentDirection, FVector::ForwardVector) * State.SpeedBeforeTerminalStop;
				StoppedProjectileMovement->Velocity = StoppedProjectileMovement->LimitVelocity(ResumeVelocity);
				StoppedProjectileMovement->UpdateComponentVelocity();
			}

			if (State.bSimulationPausedByTerminalStatus)
			{
				StoppedProjectileMovement->bSimulationEnabled = true;
			}
		}

		State.bSimulationPausedByTerminalStatus = false;
		State.TerminalStoppedProjectileMovement.Reset();
	}

	void RestoreTerminalStoppedSpeed(
		const UProjectileMovementComponent* ProjectileMovement,
		const FXToolsBulletHomingOptions& Options,
		FXToolsBulletHomingState& State)
	{
		if (State.CurrentSpeed <= KINDA_SMALL_NUMBER)
		{
			const float ResumeSpeed = State.SpeedBeforeTerminalStop > KINDA_SMALL_NUMBER
				? State.SpeedBeforeTerminalStop
				: InitialSpeedFromInputs(ProjectileMovement, Options);
			State.CurrentSpeed = ClampSpeed(ResumeSpeed, Options.MaxSpeed);
		}

		State.SpeedBeforeTerminalStop = 0.0f;
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
	if (!Options.bApplyVelocityToProjectileMovement)
	{
		RestoreTerminalStoppedProjectileMovement(State);
	}

	const bool bHasProjectileActor = IsValid(ProjectileActor);
	const bool bHasProjectileMovement = IsValid(ProjectileMovement);
	const bool bHasUpdatedComponent = bHasProjectileMovement && IsValid(ProjectileMovement->UpdatedComponent);
	USceneComponent* UpdatedComponent = bHasUpdatedComponent ? ProjectileMovement->UpdatedComponent : nullptr;
	const bool bPausedByTerminalStatus = bHasUpdatedComponent &&
		State.bSimulationPausedByTerminalStatus &&
		State.TerminalStoppedProjectileMovement.Get() == ProjectileMovement &&
		IsTerminalStatus(State.LatchedTerminalStatus);
	const bool bCanApplyProjectileMovement = bHasUpdatedComponent &&
		UpdatedComponent->Mobility == EComponentMobility::Movable &&
		!UpdatedComponent->IsSimulatingPhysics() &&
		(ProjectileMovement->bSimulationEnabled || bPausedByTerminalStatus);
	const AActor* MovementReferenceOwner = bHasUpdatedComponent
		? UpdatedComponent->GetOwner()
		: (bHasProjectileMovement ? ProjectileMovement->GetOwner() : nullptr);
	const bool bProjectileInputsMatch = !bHasProjectileActor || !bHasProjectileMovement ||
		MovementReferenceOwner == ProjectileActor;
	if ((!bHasProjectileActor && !bHasUpdatedComponent) ||
		(Options.bApplyVelocityToProjectileMovement && !bCanApplyProjectileMovement) ||
		!bProjectileInputsMatch)
	{
		return false;
	}

	const FVector CurrentLocation = bHasUpdatedComponent
		? UpdatedComponent->GetComponentLocation()
		: ProjectileActor->GetActorLocation();
	if (!IsFiniteVector(CurrentLocation))
	{
		return false;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, FMath::IsFinite(DeltaTime) ? DeltaTime : 0.0f);
	const bool bHasValidTargetIdentity = IsValid(TargetComponent) || IsValid(TargetActor);
	FBulletTargetInfo TargetInfo = ResolveTargetInfo(
		TargetComponent,
		TargetActor,
		Options,
		State);
	AActor* ResolvedTargetActor = TargetInfo.bHasLiveTarget && IsValid(TargetActor) ? TargetActor : nullptr;
	USceneComponent* ResolvedTargetComponent = TargetInfo.bHasLiveTarget && TargetInfo.bUsesTargetComponent && IsValid(TargetComponent)
		? TargetComponent
		: nullptr;
	const bool bTargetChanged = TargetInfo.bHasLiveTarget
		? DidTargetChange(ResolvedTargetActor, ResolvedTargetComponent, State)
		: DidTargetChange(TargetActor, TargetComponent, State);
	if (TargetInfo.bHasLiveTarget && !bTargetChanged && State.bHasLastTargetLocation && SafeDeltaTime > KINDA_SMALL_NUMBER)
	{
		const FVector EstimatedVelocity = (TargetInfo.Location - State.LastTargetLocation) / SafeDeltaTime;
		if (IsFiniteVector(EstimatedVelocity) && TargetInfo.Velocity.IsNearlyZero())
		{
			TargetInfo.Velocity = EstimatedVelocity;
		}
	}
	if (bTargetChanged)
	{
		if (TargetInfo.bHasLiveTarget)
		{
			ResetTargetDependentState(State);
			UpdateCachedTargetIdentity(ResolvedTargetActor, ResolvedTargetComponent, State);
		}
		else if (!bHasValidTargetIdentity)
		{
			UpdateCachedTargetIdentity(TargetActor, TargetComponent, State);
		}
	}
	if (!TargetInfo.bHasLocation && !Options.bContinueAfterTargetInvalid && !IsTerminalStatus(State.LatchedTerminalStatus))
	{
		OutStatus = EXToolsBulletHomingStatus::TargetInvalid;
		SetOutputsFromState(ProjectileActor, VisualComponent, Options, State, OutWorldVelocity, OutActorRotation, OutVisualRotation);
		UpdateDebugTrail(CurrentLocation, Options, State);
		State.LastProjectileLocation = CurrentLocation;
		State.bHasLastProjectileLocation = true;
		return false;
	}

	const FVector InitialDirection = InitialDirectionFromInputs(ProjectileActor, ProjectileMovement);
	if (!State.bInitialized)
	{
		State.CurrentDirection = SafeDirectionOrFallback(InitialDirection, FVector::ForwardVector);
		State.CurrentSpeed = ClampSpeed(InitialSpeedFromInputs(ProjectileMovement, Options), Options.MaxSpeed);
		State.CurrentSpeedInterpRate = PositiveFiniteOrZero(Options.SpeedInterpRate);
		State.CurrentDirectionInterpRate = PositiveFiniteOrZero(Options.DirectionInterpRate);
		State.bInitialized = true;
	}
	if (TargetInfo.bHasLiveTarget && (!State.bTrackingStarted || bTargetChanged))
	{
		const bool bWasStoppedOnTerminalStatus = IsTerminalStatus(State.LatchedTerminalStatus);
		RestoreTerminalStoppedProjectileMovement(State);
		if (bWasStoppedOnTerminalStatus)
		{
			RestoreTerminalStoppedSpeed(ProjectileMovement, Options, State);
		}

		State.ElapsedTime = 0.0f;
		State.CurrentSpeedInterpRate = PositiveFiniteOrZero(Options.SpeedInterpRate);
		State.CurrentDirectionInterpRate = PositiveFiniteOrZero(Options.DirectionInterpRate);
		State.bTrackingStarted = true;
		State.LatchedTerminalStatus = EXToolsBulletHomingStatus::Tracking;
	}
	const float PreviousElapsedTime = State.ElapsedTime;
	const bool bHoldingTerminalStatus = Options.bStopMovementOnTerminalStatus && IsTerminalStatus(State.LatchedTerminalStatus);
	if (State.bTrackingStarted && !bHoldingTerminalStatus)
	{
		State.ElapsedTime += SafeDeltaTime;
	}
	if (!Options.bStopMovementOnTerminalStatus)
	{
		const bool bWasStoppedOnTerminalStatus = IsTerminalStatus(State.LatchedTerminalStatus);
		RestoreTerminalStoppedProjectileMovement(State);
		if (bWasStoppedOnTerminalStatus)
		{
			RestoreTerminalStoppedSpeed(ProjectileMovement, Options, State);
		}
		State.LatchedTerminalStatus = EXToolsBulletHomingStatus::Tracking;
	}

	FVector TargetDirection = State.CurrentDirection;
	FVector AimLocation = FVector::ZeroVector;
	EXToolsBulletHomingStatus TerminalStatus = EXToolsBulletHomingStatus::Tracking;
	const float DistanceToTarget = TargetInfo.bHasLocation
		? FVector::Dist(CurrentLocation, TargetInfo.Location)
		: 0.0f;
	if (TargetInfo.bHasLocation)
	{
		if (TargetInfo.bHasLiveTarget)
		{
			if (State.InitialDistanceToTarget <= KINDA_SMALL_NUMBER && FMath::IsFinite(DistanceToTarget))
			{
				State.InitialDistanceToTarget = DistanceToTarget;
			}

			const float ClosestFrameDistance = CalculateClosestFrameDistanceToTarget(CurrentLocation, TargetInfo.Location, State);
			TerminalStatus = EvaluateTerminalStatus(
				CurrentLocation,
				TargetInfo.Location,
				DistanceToTarget,
				ClosestFrameDistance,
				Options,
				State);
			UpdateClosestDistance(ClosestFrameDistance, State);
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

	if (Options.bStopMovementOnTerminalStatus && IsTerminalStatus(State.LatchedTerminalStatus))
	{
		TerminalStatus = State.LatchedTerminalStatus;
	}

	if (Options.bStopMovementOnTerminalStatus && IsTerminalStatus(TerminalStatus))
	{
		State.LatchedTerminalStatus = TerminalStatus;
		OutStatus = TerminalStatus;
		OutWorldVelocity = FVector::ZeroVector;
		if (State.CurrentSpeed > KINDA_SMALL_NUMBER)
		{
			State.SpeedBeforeTerminalStop = State.CurrentSpeed;
		}
		State.CurrentSpeed = 0.0f;
		const FRotator CurrentRotation = SafeDirectionOrFallback(State.CurrentDirection, FVector::ForwardVector).Rotation();
		OutActorRotation = IsValid(ProjectileActor) ? ProjectileActor->GetActorRotation() : CurrentRotation;
		OutVisualRotation = IsValid(VisualComponent) ? VisualComponent->GetComponentRotation() : ComposeVisualRotation(CurrentRotation, Options.VisualRotationOffset);

		if (Options.bApplyVelocityToProjectileMovement && IsValid(ProjectileMovement))
		{
			ApplyProjectileMovementVelocity(ProjectileMovement, FVector::ZeroVector, Options);
			State.TerminalStoppedProjectileMovement = ProjectileMovement;
			if (ProjectileMovement->bSimulationEnabled)
			{
				ProjectileMovement->bSimulationEnabled = false;
				State.bSimulationPausedByTerminalStatus = true;
			}
		}

		UpdateDebugTrail(CurrentLocation, Options, State);
		DrawHomingDebug(ProjectileActor, ProjectileMovement, Options, CurrentLocation, TargetInfo, AimLocation, OutWorldVelocity, State.DebugTrailPoints);

		State.LastProjectileLocation = CurrentLocation;
		State.bHasLastProjectileLocation = true;
		return true;
	}

	const float RequestedTargetSpeed = Options.TargetSpeed > 0.0f ? Options.TargetSpeed : State.CurrentSpeed;
	const float TargetSpeed = ClampSpeed(RequestedTargetSpeed, Options.MaxSpeed);
	const bool bUseImmediateSpeedInterpolation =
		PositiveFiniteOrZero(Options.SpeedInterpRate) <= KINDA_SMALL_NUMBER &&
		PositiveFiniteOrZero(Options.SpeedInterpRateGrowth) <= KINDA_SMALL_NUMBER;
	if (State.bTrackingStarted)
	{
		State.CurrentSpeed = ClampSpeed(
			InterpScalar(State.CurrentSpeed, TargetSpeed, SafeDeltaTime, State.CurrentSpeedInterpRate, bUseImmediateSpeedInterpolation),
			Options.MaxSpeed);
		State.CurrentSpeedInterpRate = AdvanceInterpRate(
			State.CurrentSpeedInterpRate,
			Options.SpeedInterpRate,
			Options.SpeedInterpRateGrowth,
			Options.MaxSpeedInterpRate,
			SafeDeltaTime);
	}
	const float GuidanceScale = GetLaunchGuidanceScale(Options, PreviousElapsedTime);
	const float MaxDirectionInterpRate = FMath::Max(
		PositiveFiniteOrZero(Options.DirectionInterpRate),
		PositiveFiniteOrZero(Options.MaxDirectionInterpRate));
	const bool bUseImmediateDirectionGuidance =
		PositiveFiniteOrZero(Options.DirectionInterpRate) <= KINDA_SMALL_NUMBER &&
		PositiveFiniteOrZero(Options.DirectionInterpRateGrowth) <= KINDA_SMALL_NUMBER;
	const float ProgressiveGuidanceScale = bUseImmediateDirectionGuidance
		? GuidanceScale
		: (MaxDirectionInterpRate > KINDA_SMALL_NUMBER
			? GuidanceScale * FMath::Clamp(State.CurrentDirectionInterpRate / MaxDirectionInterpRate, 0.0f, 1.0f)
			: 0.0f);
	const float FullGuidanceDistanceRatio = FMath::Clamp(Options.FullGuidanceDistanceRatio, 0.0f, 1.0f);
	const bool bUseFullGuidance = TargetInfo.bHasLiveTarget &&
		SafeDeltaTime > 0.0f &&
		FullGuidanceDistanceRatio > 0.0f &&
		State.InitialDistanceToTarget > KINDA_SMALL_NUMBER &&
		FMath::IsFinite(DistanceToTarget) &&
		DistanceToTarget / State.InitialDistanceToTarget <= FullGuidanceDistanceRatio &&
		GuidanceScale >= 1.0f - KINDA_SMALL_NUMBER;
	const FVector GuidanceCurrentDirection = SafeDirectionOrFallback(
		ResolveCurrentVelocityForGuidance(ProjectileMovement, State),
		State.CurrentDirection);
	const FVector CurrentVelocity = GuidanceCurrentDirection * State.CurrentSpeed;
	const FVector GuidanceTargetDirection = TargetInfo.bHasLocation ? TargetDirection : GuidanceCurrentDirection;
	if (TargetInfo.bHasLocation && Options.GuidanceMode == EXToolsBulletGuidanceMode::ProportionalNavigation)
	{
		const FVector LineOfSightDirection = SafeDirectionOrFallback(TargetInfo.Location - CurrentLocation, State.CurrentDirection);
		if (bUseFullGuidance)
		{
			State.CurrentDirection = TargetDirection;
		}
		else
		{
			FVector GuidedVelocity = CurrentVelocity;
			const bool bAppliedPN = TryCalculateProportionalNavigationVelocity(
				CurrentLocation,
				CurrentVelocity,
				TargetInfo,
				Options,
				SafeDeltaTime,
				ProgressiveGuidanceScale,
				GuidedVelocity);

			if (bAppliedPN)
			{
				State.CurrentDirection = SafeDirectionOrFallback(GuidedVelocity, GuidanceCurrentDirection);
			}
			else
			{
				const FVector ScaledTargetDirection = GuidanceScale < 1.0f
					? SlerpDirection(GuidanceCurrentDirection, GuidanceTargetDirection, GuidanceScale)
					: GuidanceTargetDirection;
				State.CurrentDirection = InterpDirection(
					GuidanceCurrentDirection,
					ScaledTargetDirection,
					SafeDeltaTime,
					State.CurrentDirectionInterpRate,
					bUseImmediateDirectionGuidance);
			}
		}

		FVector TerminalVelocity = State.CurrentDirection * State.CurrentSpeed;
		if (TryApplyTerminalConvergenceVelocity(CurrentLocation, TerminalVelocity, TargetInfo, Options, SafeDeltaTime, ProgressiveGuidanceScale, TerminalVelocity))
		{
			State.CurrentDirection = SafeDirectionOrFallback(TerminalVelocity, State.CurrentDirection);
			State.CurrentSpeed = ClampSpeed(TerminalVelocity.Size(), Options.MaxSpeed);
		}

		State.LastLineOfSightDirection = LineOfSightDirection;
		State.bHasLastLineOfSightDirection = true;
	}
	else
	{
		if (bUseFullGuidance)
		{
			State.CurrentDirection = TargetDirection;
		}
		else
		{
			const FVector ScaledTargetDirection = GuidanceScale < 1.0f
				? SlerpDirection(GuidanceCurrentDirection, GuidanceTargetDirection, GuidanceScale)
				: GuidanceTargetDirection;
			State.CurrentDirection = InterpDirection(
				GuidanceCurrentDirection,
				ScaledTargetDirection,
				SafeDeltaTime,
				State.CurrentDirectionInterpRate,
				bUseImmediateDirectionGuidance);
		}

		if (TargetInfo.bHasLocation)
		{
			State.LastLineOfSightDirection = SafeDirectionOrFallback(TargetInfo.Location - CurrentLocation, State.CurrentDirection);
			State.bHasLastLineOfSightDirection = true;
		}
	}

	if (State.bTrackingStarted)
	{
		State.CurrentDirectionInterpRate = AdvanceInterpRate(
			State.CurrentDirectionInterpRate,
			Options.DirectionInterpRate,
			Options.DirectionInterpRateGrowth,
			Options.MaxDirectionInterpRate,
			SafeDeltaTime);
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
	const bool bProjectileMovementUsesActorVelocityRotation =
		IsValid(ProjectileActor) &&
		IsValid(ProjectileMovement) &&
		ProjectileMovement->bRotationFollowsVelocity &&
		IsValid(ProjectileMovement->UpdatedComponent) &&
		ProjectileMovement->UpdatedComponent == ProjectileActor->GetRootComponent() &&
		!ProjectileMovement->UpdatedComponent->IsSimulatingPhysics();
	FRotator OutputActorRotation = TargetRotation;
	if (bProjectileMovementUsesActorVelocityRotation && ProjectileMovement->bRotationRemainsVertical)
	{
		OutputActorRotation.Pitch = 0.0f;
		OutputActorRotation.Yaw = FRotator::NormalizeAxis(OutputActorRotation.Yaw);
		OutputActorRotation.Roll = 0.0f;
	}
	if (Options.bUpdateActorRotation && IsValid(ProjectileActor))
	{
		const FRotator RequestedActorRotation = bProjectileMovementUsesActorVelocityRotation
			? OutputActorRotation
			: InterpRotation(ProjectileActor->GetActorRotation(), TargetRotation, SafeDeltaTime, Options.RotationInterpRate);
		ProjectileActor->SetActorRotation(RequestedActorRotation);
		OutActorRotation = ProjectileActor->GetActorRotation();
	}
	else
	{
		OutActorRotation = OutputActorRotation;
	}

	const AActor* VisualComponentOwner = IsValid(VisualComponent) ? VisualComponent->GetOwner() : nullptr;
	const bool bVisualComponentIsActorRoot =
		IsValid(VisualComponentOwner) &&
		VisualComponent == VisualComponentOwner->GetRootComponent();
	if (Options.bUpdateVisualComponentRotation && IsValid(VisualComponent) && !bVisualComponentIsActorRoot)
	{
		const FRotator TargetVisualRotation = ComposeVisualRotation(TargetRotation, Options.VisualRotationOffset);
		const FRotator RequestedVisualRotation = InterpRotation(
			VisualComponent->GetComponentRotation(),
			TargetVisualRotation,
			SafeDeltaTime,
			Options.RotationInterpRate);
		VisualComponent->SetWorldRotation(RequestedVisualRotation);
		OutVisualRotation = VisualComponent->GetComponentRotation();
	}
	else if (bVisualComponentIsActorRoot)
	{
		OutVisualRotation = VisualComponent->GetComponentRotation();
	}
	else
	{
		OutVisualRotation = ComposeVisualRotation(TargetRotation, Options.VisualRotationOffset);
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
	RestoreTerminalStoppedProjectileMovement(State);
	State = FXToolsBulletHomingState();
}
