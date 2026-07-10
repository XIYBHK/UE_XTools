#include "Libraries/SplineFollowLibrary.h"

#include "BlueprintExtensionsRuntime.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "XToolsErrorReporter.h"

namespace
{
	void ResetResult(FXToolsSplineFollowResult& Result)
	{
		Result = FXToolsSplineFollowResult();
	}

	bool ValidateSplineFollowInput(AActor* TargetActor, USplineComponent* SplineComponent, FXToolsSplineFollowResult& OutResult)
	{
		ResetResult(OutResult);

		if (!IsValid(TargetActor))
		{
			return false;
		}

		if (!IsValid(SplineComponent))
		{
			return false;
		}

		if (SplineComponent->GetNumberOfSplinePoints() < 2)
		{
			return false;
		}

		return true;
	}

	FVector GetOffsetLocationAtDistance(
		USplineComponent* SplineComponent,
		double Distance,
		double RightOffset,
		bool bUseSplineScaleForRightOffset,
		bool bClampRightOffsetToSplineScaleRange,
		double SplineScaleRangeHalfWidth)
	{
		const FVector BaseLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
			static_cast<float>(Distance),
			ESplineCoordinateSpace::World);

		const FVector RightVector = SplineComponent->GetRightVectorAtDistanceAlongSpline(
			static_cast<float>(Distance),
			ESplineCoordinateSpace::World);

		const FVector SplineScale = SplineComponent->GetScaleAtDistanceAlongSpline(static_cast<float>(Distance));
		const double OffsetScale = bUseSplineScaleForRightOffset ? static_cast<double>(SplineScale.Y) : 1.0;
		const double SafeSplineScaleRangeHalfWidth = FMath::Max(SplineScaleRangeHalfWidth, 0.0);
		const double ClampedRightOffset = (bClampRightOffsetToSplineScaleRange && SafeSplineScaleRangeHalfWidth > static_cast<double>(KINDA_SMALL_NUMBER))
			? FMath::Clamp(RightOffset, -SafeSplineScaleRangeHalfWidth, SafeSplineScaleRangeHalfWidth)
			: RightOffset;
		const double ScaledRightOffset = ClampedRightOffset * OffsetScale;
		if (bUseSplineScaleForRightOffset && bClampRightOffsetToSplineScaleRange)
		{
			const double MaxAbsRightOffset = SafeSplineScaleRangeHalfWidth * FMath::Abs(OffsetScale);
			if (MaxAbsRightOffset > static_cast<double>(KINDA_SMALL_NUMBER))
			{
				return BaseLocation + RightVector * static_cast<float>(FMath::Clamp(ScaledRightOffset, -MaxAbsRightOffset, MaxAbsRightOffset));
			}
		}

		return BaseLocation + RightVector * static_cast<float>(ScaledRightOffset);
	}

	double WrapSplineDistance(double Distance, double SplineLength)
	{
		if (SplineLength <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 0.0;
		}

		double WrappedDistance = FMath::Fmod(Distance, SplineLength);
		if (WrappedDistance < 0.0)
		{
			WrappedDistance += SplineLength;
		}
		return WrappedDistance;
	}

	double FindSplineDistanceClosestToLocation(USplineComponent* SplineComponent, const FVector& WorldLocation)
	{
		if (!SplineComponent)
		{
			return 0.0;
		}

		const float ClosestInputKey = SplineComponent->FindInputKeyClosestToWorldLocation(WorldLocation);
		return static_cast<double>(SplineComponent->GetDistanceAlongSplineAtSplineInputKey(ClosestInputKey));
	}

	double CalculateOffsetPathLength(
		USplineComponent* SplineComponent,
		double TargetDistance,
		double SplineLength,
		double RightOffset,
		bool bUseSplineScaleForRightOffset,
		bool bClampRightOffsetToSplineScaleRange,
		double SplineScaleRangeHalfWidth,
		double SpeedSampleDistance,
		int32 SpeedSampleSegments,
		bool bWrapDistance)
	{
		if (!SplineComponent || SpeedSampleDistance <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 0.0;
		}

		const double SafeSampleDistance = FMath::Min(
			FMath::Abs(SpeedSampleDistance),
			FMath::Max(SplineLength, 0.0));
		if (SafeSampleDistance <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 0.0;
		}

		double WindowStart = TargetDistance - SafeSampleDistance * 0.5;
		double WindowEnd = TargetDistance + SafeSampleDistance * 0.5;
		double WindowLength = SafeSampleDistance;

		if (bWrapDistance)
		{
			const int32 SegmentCount = FMath::Clamp(SpeedSampleSegments, 1, 32);
			double PathLength = 0.0;
			FVector PreviousLocation = GetOffsetLocationAtDistance(
				SplineComponent,
				WrapSplineDistance(WindowStart, SplineLength),
				RightOffset,
				bUseSplineScaleForRightOffset,
				bClampRightOffsetToSplineScaleRange,
				SplineScaleRangeHalfWidth);

			for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
			{
				const double Alpha = static_cast<double>(SegmentIndex) / static_cast<double>(SegmentCount);
				const double Distance = WrapSplineDistance(WindowStart + WindowLength * Alpha, SplineLength);
				const FVector CurrentLocation = GetOffsetLocationAtDistance(
					SplineComponent,
					Distance,
					RightOffset,
					bUseSplineScaleForRightOffset,
					bClampRightOffsetToSplineScaleRange,
					SplineScaleRangeHalfWidth);

				PathLength += static_cast<double>(FVector::Distance(PreviousLocation, CurrentLocation));
				PreviousLocation = CurrentLocation;
			}

			return PathLength;
		}

		if (WindowStart < 0.0)
		{
			WindowEnd = FMath::Min(WindowEnd - WindowStart, SplineLength);
			WindowStart = 0.0;
		}

		if (WindowEnd > SplineLength)
		{
			const double Overflow = WindowEnd - SplineLength;
			WindowStart = FMath::Max(WindowStart - Overflow, 0.0);
			WindowEnd = SplineLength;
		}

		WindowLength = WindowEnd - WindowStart;
		if (WindowLength <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 0.0;
		}

		const int32 SegmentCount = FMath::Clamp(SpeedSampleSegments, 1, 32);
		double PathLength = 0.0;
		FVector PreviousLocation = GetOffsetLocationAtDistance(
			SplineComponent,
			WindowStart,
			RightOffset,
			bUseSplineScaleForRightOffset,
			bClampRightOffsetToSplineScaleRange,
			SplineScaleRangeHalfWidth);

		for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
		{
			const double Alpha = static_cast<double>(SegmentIndex) / static_cast<double>(SegmentCount);
			const double Distance = FMath::Lerp(WindowStart, WindowEnd, Alpha);
			const FVector CurrentLocation = GetOffsetLocationAtDistance(
				SplineComponent,
				Distance,
				RightOffset,
				bUseSplineScaleForRightOffset,
				bClampRightOffsetToSplineScaleRange,
				SplineScaleRangeHalfWidth);

			PathLength += static_cast<double>(FVector::Distance(PreviousLocation, CurrentLocation));
			PreviousLocation = CurrentLocation;
		}

		return PathLength;
	}

	float CalculateOffsetPathSpeedScale(
		USplineComponent* SplineComponent,
		double TargetDistance,
		double SplineLength,
		double RightOffset,
		bool bUseSplineScaleForRightOffset,
		bool bClampRightOffsetToSplineScaleRange,
		double SplineScaleRangeHalfWidth,
		EXToolsSplineFollowSpeedMode SpeedMode,
		double ReferenceRightOffset,
		double SpeedSampleDistance,
		int32 SpeedSampleSegments,
		bool bWrapDistance,
		float MinSpeedScale,
		float MaxSpeedScale,
		double& OutCurrentPathLength,
		double& OutReferencePathLength,
		float& OutRawSpeedScale)
	{
		OutCurrentPathLength = 0.0;
		OutReferencePathLength = 0.0;
		OutRawSpeedScale = 1.0f;

		if (!SplineComponent || SpeedMode == EXToolsSplineFollowSpeedMode::None)
		{
			return 1.0f;
		}

		const double SafeMinScale = FMath::Max(static_cast<double>(MinSpeedScale), 0.0);
		const double SafeMaxScale = FMath::Max(static_cast<double>(MaxSpeedScale), SafeMinScale);
		if (SafeMaxScale <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 1.0f;
		}

		const double CurrentPathLength = CalculateOffsetPathLength(
			SplineComponent,
			TargetDistance,
			SplineLength,
			RightOffset,
			bUseSplineScaleForRightOffset,
			bClampRightOffsetToSplineScaleRange,
			SplineScaleRangeHalfWidth,
			SpeedSampleDistance,
			SpeedSampleSegments,
			bWrapDistance);
		OutCurrentPathLength = CurrentPathLength;
		if (CurrentPathLength <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 1.0f;
		}

		double ReferencePathLength = 0.0;
		if (SpeedMode == EXToolsSplineFollowSpeedMode::MatchReferenceOffset)
		{
			ReferencePathLength = CalculateOffsetPathLength(
				SplineComponent,
				TargetDistance,
				SplineLength,
				ReferenceRightOffset,
				bUseSplineScaleForRightOffset,
				bClampRightOffsetToSplineScaleRange,
				SplineScaleRangeHalfWidth,
				SpeedSampleDistance,
				SpeedSampleSegments,
				bWrapDistance);
		}
		else
		{
			ReferencePathLength = CalculateOffsetPathLength(
				SplineComponent,
				TargetDistance,
				SplineLength,
				0.0,
				false,
				false,
				0.0,
				SpeedSampleDistance,
				SpeedSampleSegments,
				bWrapDistance);
		}

		if (ReferencePathLength <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 1.0f;
		}

		OutReferencePathLength = ReferencePathLength;
		const double RawSpeedScale = CurrentPathLength / ReferencePathLength;
		OutRawSpeedScale = static_cast<float>(RawSpeedScale);
		return static_cast<float>(FMath::Clamp(RawSpeedScale, SafeMinScale, SafeMaxScale));
	}

	void DrawSplineFollowDebug(
		const AActor* TargetActor,
		const FVector& CurrentSplineLocation,
		const FVector& LookAheadLocation,
		const FVector& TargetLocation,
		float DebugDrawTime,
		float DebugPointSize)
	{
		if (!TargetActor || !TargetActor->GetWorld())
		{
			return;
		}

		const float SafePointSize = FMath::Max(DebugPointSize, 4.0f);
		const bool bPersistent = false;
		const float LineThickness = FMath::Max(SafePointSize * 0.12f, 1.5f);
		const float ArrowSize = SafePointSize * 3.0f;
		const FVector ActorLocation = TargetActor->GetActorLocation();

		DrawDebugSphere(TargetActor->GetWorld(), CurrentSplineLocation, SafePointSize * 0.75f, 12, FColor::Blue, bPersistent, DebugDrawTime, 0, LineThickness);
		DrawDebugSphere(TargetActor->GetWorld(), LookAheadLocation, SafePointSize, 12, FColor::Yellow, bPersistent, DebugDrawTime, 0, LineThickness);
		DrawDebugSphere(TargetActor->GetWorld(), TargetLocation, SafePointSize, 12, FColor::Green, bPersistent, DebugDrawTime, 0, LineThickness);

		DrawDebugDirectionalArrow(TargetActor->GetWorld(), CurrentSplineLocation, LookAheadLocation, ArrowSize, FColor::Yellow, bPersistent, DebugDrawTime, 0, LineThickness);
		DrawDebugDirectionalArrow(TargetActor->GetWorld(), ActorLocation, TargetLocation, ArrowSize, FColor::Green, bPersistent, DebugDrawTime, 0, LineThickness);
		DrawDebugLine(TargetActor->GetWorld(), LookAheadLocation, TargetLocation, FColor::Cyan, bPersistent, DebugDrawTime, 0, LineThickness);
	}

	bool RestoreCapturedCharacterMovementValues(
		UCharacterMovementComponent* MovementComponent,
		FXToolsSplineFollowState& State,
		bool bRestoreWalkSpeed,
		bool bRestoreAcceleration,
		bool bClearCapturedValues)
	{
		if (!IsValid(MovementComponent))
		{
			return false;
		}

		bool bHandled = false;
		if (bRestoreWalkSpeed && State.bHasCapturedBaseMaxWalkSpeed && State.CapturedBaseMaxWalkSpeed > 0.0f)
		{
			MovementComponent->MaxWalkSpeed = State.CapturedBaseMaxWalkSpeed;
			bHandled = true;
			if (bClearCapturedValues)
			{
				State.bHasCapturedBaseMaxWalkSpeed = false;
				State.CapturedBaseMaxWalkSpeed = 0.0f;
			}
		}

		if (bRestoreAcceleration && State.bHasCapturedBaseMaxAcceleration && State.CapturedBaseMaxAcceleration > 0.0f)
		{
			MovementComponent->MaxAcceleration = State.CapturedBaseMaxAcceleration;
			bHandled = true;
			if (bClearCapturedValues)
			{
				State.bHasCapturedBaseMaxAcceleration = false;
				State.CapturedBaseMaxAcceleration = 0.0f;
			}
		}

		return bHandled;
	}
}

void USplineFollowLibrary::ResetSplineFollowState(FXToolsSplineFollowState& State)
{
	const bool bHasCapturedBaseMaxWalkSpeed = State.bHasCapturedBaseMaxWalkSpeed;
	const float CapturedBaseMaxWalkSpeed = State.CapturedBaseMaxWalkSpeed;
	const bool bHasCapturedBaseMaxAcceleration = State.bHasCapturedBaseMaxAcceleration;
	const float CapturedBaseMaxAcceleration = State.CapturedBaseMaxAcceleration;

	State = FXToolsSplineFollowState();
	State.bHasCapturedBaseMaxWalkSpeed = bHasCapturedBaseMaxWalkSpeed;
	State.CapturedBaseMaxWalkSpeed = CapturedBaseMaxWalkSpeed;
	State.bHasCapturedBaseMaxAcceleration = bHasCapturedBaseMaxAcceleration;
	State.CapturedBaseMaxAcceleration = CapturedBaseMaxAcceleration;
}

bool USplineFollowLibrary::CalculateInitialSplineRightOffset(
	AActor* TargetActor,
	USplineComponent* SplineComponent,
	double& OutRightOffset,
	double& OutDistance,
	FVector& OutSplineLocation,
	bool bUseSplineScaleForRightOffset,
	bool bClampRightOffsetToSplineScaleRange,
	double SplineScaleRangeHalfWidth,
	bool bConstrainToXY)
{
	OutRightOffset = 0.0;
	OutDistance = 0.0;
	OutSplineLocation = FVector::ZeroVector;

	if (!IsValid(TargetActor))
	{
		XTOOLS_LOG_WARNING(LogBlueprintExtensionsRuntime, TEXT("计算样条初始右偏移失败：目标Actor为空或无效"));
		return false;
	}

	if (!IsValid(SplineComponent))
	{
		XTOOLS_LOG_WARNING(LogBlueprintExtensionsRuntime, TEXT("计算样条初始右偏移失败：样条组件为空或无效"));
		return false;
	}

	if (SplineComponent->GetNumberOfSplinePoints() < 2)
	{
		XTOOLS_LOG_WARNING(LogBlueprintExtensionsRuntime, TEXT("计算样条初始右偏移失败：样条组件至少需要2个控制点"));
		return false;
	}

	const float ClosestInputKey = SplineComponent->FindInputKeyClosestToWorldLocation(TargetActor->GetActorLocation());
	OutDistance = static_cast<double>(SplineComponent->GetDistanceAlongSplineAtSplineInputKey(ClosestInputKey));
	OutSplineLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(OutDistance),
		ESplineCoordinateSpace::World);

	FVector RightVector = SplineComponent->GetRightVectorAtDistanceAlongSpline(
		static_cast<float>(OutDistance),
		ESplineCoordinateSpace::World);
	FVector Delta = TargetActor->GetActorLocation() - OutSplineLocation;

	if (bConstrainToXY)
	{
		RightVector.Z = 0.0;
		Delta.Z = 0.0;
		RightVector = RightVector.GetSafeNormal();
	}

	if (RightVector.IsNearlyZero())
	{
		XTOOLS_LOG_WARNING(LogBlueprintExtensionsRuntime, TEXT("计算样条初始右偏移失败：样条右向量无效"));
		return false;
	}

	const double WorldRightOffset = static_cast<double>(FVector::DotProduct(Delta, RightVector));
	const double SafeSplineScaleRangeHalfWidth = FMath::Max(SplineScaleRangeHalfWidth, 0.0);
	if (!bUseSplineScaleForRightOffset)
	{
		OutRightOffset = (bClampRightOffsetToSplineScaleRange && SafeSplineScaleRangeHalfWidth > static_cast<double>(KINDA_SMALL_NUMBER))
			? FMath::Clamp(WorldRightOffset, -SafeSplineScaleRangeHalfWidth, SafeSplineScaleRangeHalfWidth)
			: WorldRightOffset;
		return true;
	}

	const FVector SplineScale = SplineComponent->GetScaleAtDistanceAlongSpline(static_cast<float>(OutDistance));
	const double OffsetScale = static_cast<double>(SplineScale.Y);
	if (FMath::Abs(OffsetScale) <= static_cast<double>(KINDA_SMALL_NUMBER))
	{
		XTOOLS_LOG_WARNING(LogBlueprintExtensionsRuntime, TEXT("计算样条初始右偏移失败：样条Y缩放接近0"));
		return false;
	}

	OutRightOffset = WorldRightOffset / OffsetScale;
	if (bClampRightOffsetToSplineScaleRange && SafeSplineScaleRangeHalfWidth > static_cast<double>(KINDA_SMALL_NUMBER))
	{
		OutRightOffset = FMath::Clamp(OutRightOffset, -SafeSplineScaleRangeHalfWidth, SafeSplineScaleRangeHalfWidth);
	}
	return true;
}

EXToolsSplineFollowStatus USplineFollowLibrary::CalculateSplineFollowTarget(
	AActor* TargetActor,
	USplineComponent* SplineComponent,
	FXToolsSplineFollowState& State,
	FXToolsSplineFollowResult& OutResult,
	double RightOffset,
	double LookAheadDistance,
	EXToolsSplineFollowEndBehavior EndBehavior,
	EXToolsSplineFollowSpeedMode SpeedMode,
	bool bReverse,
	double EndAcceptanceDistance,
	bool bConstrainToXY,
	bool bUseSplineScaleForRightOffset,
	bool bClampRightOffsetToSplineScaleRange,
	double SplineScaleRangeHalfWidth,
	double ReferenceRightOffset,
	double SpeedSampleDistance,
	int32 SpeedSampleSegments,
	float MinSpeedScale,
	float MaxSpeedScale,
	bool bDrawDebug,
	float DebugDrawTime,
	float DebugPointSize)
{
	if (!ValidateSplineFollowInput(TargetActor, SplineComponent, OutResult))
	{
		return EXToolsSplineFollowStatus::Invalid;
	}

	const double SplineLength = static_cast<double>(SplineComponent->GetSplineLength());
	if (SplineLength <= static_cast<double>(KINDA_SMALL_NUMBER))
	{
		return EXToolsSplineFollowStatus::Invalid;
	}

	const FVector ActorLocation = TargetActor->GetActorLocation();
	const double CurrentDistance = FindSplineDistanceClosestToLocation(SplineComponent, ActorLocation);
	const FVector CurrentSplineLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(CurrentDistance),
		ESplineCoordinateSpace::World);
	const double SafeLookAheadDistance = FMath::Max(FMath::Abs(LookAheadDistance), static_cast<double>(KINDA_SMALL_NUMBER));
	const double SafeEndAcceptanceDistance = FMath::Max(EndAcceptanceDistance, 0.0);
	const bool bShouldLoop = EndBehavior == EXToolsSplineFollowEndBehavior::Loop;
	const bool bIsClosedLoop = SplineComponent->IsClosedLoop();
	const bool bNeedsOpenLoopTransition = bShouldLoop && !bIsClosedLoop;
	const bool bWrapDistance = bShouldLoop && bIsClosedLoop;

	if (!State.bHasRuntimeReverse || (State.bHasDistanceCache && State.bLastReverse != bReverse && EndBehavior != EXToolsSplineFollowEndBehavior::PingPong))
	{
		State.bHasRuntimeReverse = true;
		State.bRuntimeReverse = bReverse;
	}

	bool bEffectiveReverse = EndBehavior == EXToolsSplineFollowEndBehavior::PingPong ? State.bRuntimeReverse : bReverse;
	bool bReachedEnd = bEffectiveReverse
		? CurrentDistance <= SafeEndAcceptanceDistance
		: CurrentDistance >= SplineLength - SafeEndAcceptanceDistance;

	if (EndBehavior == EXToolsSplineFollowEndBehavior::PingPong && bReachedEnd)
	{
		State.bRuntimeReverse = !State.bRuntimeReverse;
		bEffectiveReverse = State.bRuntimeReverse;
		bReachedEnd = false;
		State.bHasDistanceCache = false;
	}

	if (bNeedsOpenLoopTransition)
	{
		const double OpenLoopTargetDistance = State.bOpenLoopTransitionToEnd ? SplineLength : 0.0;
		if (State.bOpenLoopTransitionActive)
		{
			FVector ToOpenLoopTarget = GetOffsetLocationAtDistance(
				SplineComponent,
				OpenLoopTargetDistance,
				RightOffset,
				bUseSplineScaleForRightOffset,
				bClampRightOffsetToSplineScaleRange,
				SplineScaleRangeHalfWidth) - ActorLocation;
			if (bConstrainToXY)
			{
				ToOpenLoopTarget.Z = 0.0;
			}

			if (ToOpenLoopTarget.Size() <= SafeEndAcceptanceDistance)
			{
				State.bOpenLoopTransitionActive = false;
				State.bHasDistanceCache = false;
				State.LastTargetDistance = OpenLoopTargetDistance;
				State.bLastReverse = bEffectiveReverse;
			}
		}

		if (!State.bOpenLoopTransitionActive && bReachedEnd)
		{
			State.bOpenLoopTransitionActive = true;
			State.bOpenLoopTransitionToEnd = bEffectiveReverse;
			State.bHasDistanceCache = false;
		}
	}

	const double DirectionSign = bEffectiveReverse ? -1.0 : 1.0;
	const double RawTargetDistance = CurrentDistance + DirectionSign * SafeLookAheadDistance;
	double CandidateTargetDistance = 0.0;
	if (bShouldLoop && bIsClosedLoop)
	{
		CandidateTargetDistance = WrapSplineDistance(RawTargetDistance, SplineLength);
	}
	else
	{
		CandidateTargetDistance = FMath::Clamp(RawTargetDistance, 0.0, SplineLength);
	}

	const bool bUseDistanceCache = !bShouldLoop || bNeedsOpenLoopTransition;
	const bool bDirectionChanged = State.bHasDistanceCache && State.bLastReverse != bEffectiveReverse;
	if (!bUseDistanceCache || !State.bHasDistanceCache || bDirectionChanged)
	{
		State.bHasDistanceCache = true;
		State.LastTargetDistance = CandidateTargetDistance;
		State.bLastReverse = bEffectiveReverse;
	}
	else
	{
		State.LastTargetDistance = bEffectiveReverse
			? FMath::Min(State.LastTargetDistance, CandidateTargetDistance)
			: FMath::Max(State.LastTargetDistance, CandidateTargetDistance);
	}

	const double TargetDistance = State.bOpenLoopTransitionActive
		? (State.bOpenLoopTransitionToEnd ? SplineLength : 0.0)
		: bShouldLoop && bIsClosedLoop
		? WrapSplineDistance(State.LastTargetDistance, SplineLength)
		: FMath::Clamp(State.LastTargetDistance, 0.0, SplineLength);
	const bool bShouldStopAtEnd = EndBehavior == EXToolsSplineFollowEndBehavior::Stop;

	const FVector TargetLocation = GetOffsetLocationAtDistance(
		SplineComponent,
		TargetDistance,
		RightOffset,
		bUseSplineScaleForRightOffset,
		bClampRightOffsetToSplineScaleRange,
		SplineScaleRangeHalfWidth);
	const FVector LookAheadLocation = GetOffsetLocationAtDistance(
		SplineComponent,
		TargetDistance,
		0.0,
		false,
		false,
		0.0);

	float SpeedScale = 1.0f;
	double CurrentOffsetPathLength = 0.0;
	double ReferencePathLength = 0.0;
	float RawSpeedScale = 1.0f;
	if (SpeedMode != EXToolsSplineFollowSpeedMode::None && !State.bOpenLoopTransitionActive)
	{
		SpeedScale = CalculateOffsetPathSpeedScale(
			SplineComponent,
			TargetDistance,
			SplineLength,
			RightOffset,
			bUseSplineScaleForRightOffset,
			bClampRightOffsetToSplineScaleRange,
			SplineScaleRangeHalfWidth,
			SpeedMode,
			ReferenceRightOffset,
			SpeedSampleDistance,
			SpeedSampleSegments,
			bWrapDistance,
			MinSpeedScale,
			MaxSpeedScale,
			CurrentOffsetPathLength,
			ReferencePathLength,
			RawSpeedScale);
	}

	FVector MoveDirection = TargetLocation - ActorLocation;
	if (bConstrainToXY)
	{
		MoveDirection.Z = 0.0;
	}
	MoveDirection = MoveDirection.GetSafeNormal();

	const bool bReachedStoppingEnd = bReachedEnd && bShouldStopAtEnd && !State.bOpenLoopTransitionActive;

	OutResult.Status = bReachedStoppingEnd ? EXToolsSplineFollowStatus::ReachedEnd : EXToolsSplineFollowStatus::Moving;
	OutResult.TargetLocation = TargetLocation;
	OutResult.MoveDirection = MoveDirection;
	OutResult.CurrentSplineLocation = CurrentSplineLocation;
	OutResult.LookAheadLocation = LookAheadLocation;
	OutResult.CurrentDistance = CurrentDistance;
	OutResult.TargetDistance = TargetDistance;
	OutResult.SplineLength = SplineLength;
	OutResult.SpeedScale = SpeedScale;
	OutResult.CurrentOffsetPathLength = CurrentOffsetPathLength;
	OutResult.ReferencePathLength = ReferencePathLength;
	OutResult.RawSpeedScale = RawSpeedScale;
	OutResult.bReachedEnd = bReachedStoppingEnd;

	if (bDrawDebug)
	{
		DrawSplineFollowDebug(
			TargetActor,
			CurrentSplineLocation,
			LookAheadLocation,
			TargetLocation,
			DebugDrawTime,
			DebugPointSize);
	}

	return OutResult.Status;
}

EXToolsSplineFollowStatus USplineFollowLibrary::AddMovementInputAlongSpline(
	APawn* Pawn,
	USplineComponent* SplineComponent,
	FXToolsSplineFollowState& State,
	FXToolsSplineFollowResult& OutResult,
	double RightOffset,
	double LookAheadDistance,
	EXToolsSplineFollowEndBehavior EndBehavior,
	EXToolsSplineFollowSpeedMode SpeedMode,
	float MovementScale,
	bool bReverse,
	double EndAcceptanceDistance,
	bool bConstrainToXY,
	bool bUseSplineScaleForRightOffset,
	bool bClampRightOffsetToSplineScaleRange,
	double SplineScaleRangeHalfWidth,
	double ReferenceRightOffset,
	double SpeedSampleDistance,
	int32 SpeedSampleSegments,
	float MinSpeedScale,
	float MaxSpeedScale,
	bool bForce,
	bool bApplySpeedToCharacterMovement,
	bool bScaleCharacterAcceleration,
	float BaseMaxWalkSpeed,
	float SpeedUpdateTolerance,
	float MinWalkSpeed,
	float MaxWalkSpeed,
	bool bDrawDebug,
	float DebugDrawTime,
	float DebugPointSize)
{
	const EXToolsSplineFollowStatus Status = CalculateSplineFollowTarget(
		Pawn,
		SplineComponent,
		State,
		OutResult,
		RightOffset,
		LookAheadDistance,
		EndBehavior,
		SpeedMode,
		bReverse,
		EndAcceptanceDistance,
		bConstrainToXY,
		bUseSplineScaleForRightOffset,
		bClampRightOffsetToSplineScaleRange,
		SplineScaleRangeHalfWidth,
		ReferenceRightOffset,
		SpeedSampleDistance,
		SpeedSampleSegments,
		MinSpeedScale,
		MaxSpeedScale,
		bDrawDebug,
		DebugDrawTime,
		DebugPointSize);

	bool bAppliedCharacterSpeed = false;
	ACharacter* Character = IsValid(Pawn) ? Cast<ACharacter>(Pawn) : nullptr;
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	if (IsValid(MovementComponent))
	{
		if (bApplySpeedToCharacterMovement)
		{
			if (!State.bHasCapturedBaseMaxWalkSpeed || State.CapturedBaseMaxWalkSpeed <= 0.0f || BaseMaxWalkSpeed > 0.0f)
			{
				State.CapturedBaseMaxWalkSpeed = BaseMaxWalkSpeed > 0.0f ? BaseMaxWalkSpeed : MovementComponent->MaxWalkSpeed;
				State.bHasCapturedBaseMaxWalkSpeed = State.CapturedBaseMaxWalkSpeed > 0.0f;
			}

			if (!State.bHasCapturedBaseMaxAcceleration || State.CapturedBaseMaxAcceleration <= 0.0f)
			{
				State.CapturedBaseMaxAcceleration = MovementComponent->MaxAcceleration;
				State.bHasCapturedBaseMaxAcceleration = State.CapturedBaseMaxAcceleration > 0.0f;
			}

			if (State.bHasCapturedBaseMaxWalkSpeed)
			{
				const bool bShouldApplySpeedScale = Status == EXToolsSplineFollowStatus::Moving
					&& SpeedMode != EXToolsSplineFollowSpeedMode::None;
				if (bShouldApplySpeedScale)
				{
					ApplySplineFollowSpeedToCharacter(
						Character,
						State.CapturedBaseMaxWalkSpeed,
						OutResult.SpeedScale,
						SpeedUpdateTolerance,
						MinWalkSpeed,
						MaxWalkSpeed);
					bAppliedCharacterSpeed = true;
				}
				else
				{
					RestoreCapturedCharacterMovementValues(MovementComponent, State, true, false, false);
				}
			}

			if (bScaleCharacterAcceleration && State.bHasCapturedBaseMaxAcceleration)
			{
				const bool bShouldApplyAccelerationScale = Status == EXToolsSplineFollowStatus::Moving
					&& SpeedMode != EXToolsSplineFollowSpeedMode::None;
				if (bShouldApplyAccelerationScale)
				{
					const float DesiredAccelerationScale = FMath::Max(OutResult.SpeedScale, 1.0f);
					const float TargetAcceleration = State.CapturedBaseMaxAcceleration * DesiredAccelerationScale;
					if (FMath::Abs(MovementComponent->MaxAcceleration - TargetAcceleration) > SpeedUpdateTolerance)
					{
						MovementComponent->MaxAcceleration = TargetAcceleration;
					}
				}
				else
				{
					RestoreCapturedCharacterMovementValues(MovementComponent, State, false, true, false);
				}
			}
			else if (State.bHasCapturedBaseMaxAcceleration)
			{
				RestoreCapturedCharacterMovementValues(MovementComponent, State, false, true, true);
			}
		}
		else
		{
			RestoreCapturedCharacterMovementValues(MovementComponent, State, true, true, true);
		}
	}

	if (Status == EXToolsSplineFollowStatus::Moving && IsValid(Pawn) && !OutResult.MoveDirection.IsNearlyZero())
	{
		const float EffectiveMovementScale = bAppliedCharacterSpeed
			? MovementScale
			: MovementScale * OutResult.SpeedScale;
		Pawn->AddMovementInput(OutResult.MoveDirection, EffectiveMovementScale, bForce);
	}

	return Status;
}

EXToolsSplineFollowStatus USplineFollowLibrary::GetSplineAIMoveToDestination(
	AActor* TargetActor,
	USplineComponent* SplineComponent,
	FXToolsSplineFollowState& State,
	FVector& OutDestination,
	FXToolsSplineFollowResult& OutResult,
	double RightOffset,
	double LookAheadDistance,
	EXToolsSplineFollowEndBehavior EndBehavior,
	EXToolsSplineFollowSpeedMode SpeedMode,
	bool bReverse,
	double EndAcceptanceDistance,
	bool bConstrainToXY,
	bool bUseSplineScaleForRightOffset,
	bool bClampRightOffsetToSplineScaleRange,
	double SplineScaleRangeHalfWidth,
	double ReferenceRightOffset,
	double SpeedSampleDistance,
	int32 SpeedSampleSegments,
	float MinSpeedScale,
	float MaxSpeedScale,
	bool bDrawDebug,
	float DebugDrawTime,
	float DebugPointSize)
{
	const EXToolsSplineFollowStatus Status = CalculateSplineFollowTarget(
		TargetActor,
		SplineComponent,
		State,
		OutResult,
		RightOffset,
		LookAheadDistance,
		EndBehavior,
		SpeedMode,
		bReverse,
		EndAcceptanceDistance,
		bConstrainToXY,
		bUseSplineScaleForRightOffset,
		bClampRightOffsetToSplineScaleRange,
		SplineScaleRangeHalfWidth,
		ReferenceRightOffset,
		SpeedSampleDistance,
		SpeedSampleSegments,
		MinSpeedScale,
		MaxSpeedScale,
		bDrawDebug,
		DebugDrawTime,
		DebugPointSize);

	OutDestination = OutResult.TargetLocation;
	return Status;
}

bool USplineFollowLibrary::ApplySplineFollowSpeedToCharacter(
	ACharacter* Character,
	float BaseMaxWalkSpeed,
	float SpeedScale,
	float UpdateTolerance,
	float MinWalkSpeed,
	float MaxWalkSpeed)
{
	if (!IsValid(Character))
	{
		return false;
	}

	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	if (!IsValid(MovementComponent))
	{
		return false;
	}

	const float SafeBaseSpeed = FMath::Max(BaseMaxWalkSpeed, 0.0f);
	const float SafeSpeedScale = FMath::Max(SpeedScale, 0.0f);
	float TargetWalkSpeed = SafeBaseSpeed * SafeSpeedScale;

	const float SafeMinWalkSpeed = FMath::Max(MinWalkSpeed, 0.0f);
	const float SafeMaxWalkSpeed = MaxWalkSpeed > 0.0f ? FMath::Max(MaxWalkSpeed, SafeMinWalkSpeed) : 0.0f;
	TargetWalkSpeed = FMath::Max(TargetWalkSpeed, SafeMinWalkSpeed);
	if (SafeMaxWalkSpeed > 0.0f)
	{
		TargetWalkSpeed = FMath::Min(TargetWalkSpeed, SafeMaxWalkSpeed);
	}

	const float SafeUpdateTolerance = FMath::Max(UpdateTolerance, 0.0f);
	if (FMath::Abs(MovementComponent->MaxWalkSpeed - TargetWalkSpeed) <= SafeUpdateTolerance)
	{
		return false;
	}

	MovementComponent->MaxWalkSpeed = TargetWalkSpeed;
	return true;
}

bool USplineFollowLibrary::RestoreSplineFollowCharacterMovement(
	ACharacter* Character,
	FXToolsSplineFollowState& State,
	bool bRestoreWalkSpeed,
	bool bRestoreAcceleration,
	bool bClearCapturedValues)
{
	if (!IsValid(Character))
	{
		return false;
	}

	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	if (!IsValid(MovementComponent))
	{
		return false;
	}

	return RestoreCapturedCharacterMovementValues(
		MovementComponent,
		State,
		bRestoreWalkSpeed,
		bRestoreAcceleration,
		bClearCapturedValues);
}
