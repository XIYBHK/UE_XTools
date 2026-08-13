#include "Libraries/QueueSplineLibrary.h"

#include "Components/SplineComponent.h"

namespace
{
	bool IsSplineUsable(const USplineComponent* SplineComponent)
	{
		return IsValid(SplineComponent)
			&& SplineComponent->GetNumberOfSplinePoints() >= 2
			&& SplineComponent->GetSplineLength() > KINDA_SMALL_NUMBER;
	}

	double ClampDistanceToSpline(double Distance, double SplineLength)
	{
		if (SplineLength <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 0.0;
		}

		return FMath::Clamp(Distance, 0.0, SplineLength);
	}

	double GetQueueHeadDistance(double SplineLength, const FXToolsQueueSplineConfig& Config, int32 UnitIndex)
	{
		const double SafeStartDistance = ClampDistanceToSpline(Config.StartDistance, SplineLength);
		const int32 EffectiveUnitCount = FMath::Max(Config.UnitCount, UnitIndex + 1);
		const double QueueLength = FMath::Max(Config.Spacing, 1.0) * static_cast<double>(FMath::Max(EffectiveUnitCount - 1, 0));
		if (Config.FillMode == EXToolsQueueSplineFillMode::FromStart)
		{
			return Config.bHeadAtEnd
				? SafeStartDistance + QueueLength
				: SplineLength - SafeStartDistance - QueueLength;
		}

		const double FillDistance = SplineLength * FMath::Clamp(Config.FillRatio, 0.0, 1.0);
		return Config.bHeadAtEnd ? FillDistance : SplineLength - FillDistance;
	}

	double GetSignedSideForIndex(int32 UnitIndex, const FXToolsQueueSplineConfig& Config)
	{
		// 与 UQueueSplineComponent::CalculateSlotForIndex 保持一致：含队头在内，偶数索引向右、奇数索引向左
		if (!Config.bAlternateSides)
		{
			return 0.0;
		}

		return (UnitIndex % 2) == 0 ? 1.0 : -1.0;
	}

	double GetRandomRange(FRandomStream& RandomStream, double Radius)
	{
		if (Radius <= static_cast<double>(KINDA_SMALL_NUMBER))
		{
			return 0.0;
		}

		return static_cast<double>(RandomStream.FRandRange(
			static_cast<float>(-Radius),
			static_cast<float>(Radius)));
	}
}

TArray<FXToolsQueueSplineSlot> UQueueSplineLibrary::GenerateQueueSplineSlots(
	USplineComponent* SplineComponent,
	const FXToolsQueueSplineConfig& Config)
{
	TArray<FXToolsQueueSplineSlot> Slots;
	if (!IsSplineUsable(SplineComponent) || Config.UnitCount <= 0)
	{
		return Slots;
	}

	Slots.Reserve(Config.UnitCount);
	for (int32 UnitIndex = 0; UnitIndex < Config.UnitCount; ++UnitIndex)
	{
		FXToolsQueueSplineSlot Slot;
		if (CalculateQueueSplineSlot(SplineComponent, UnitIndex, Config, Slot))
		{
			Slots.Add(Slot);
		}
	}

	return Slots;
}

bool UQueueSplineLibrary::CalculateQueueSplineSlot(
	USplineComponent* SplineComponent,
	int32 UnitIndex,
	const FXToolsQueueSplineConfig& Config,
	FXToolsQueueSplineSlot& OutSlot)
{
	OutSlot = FXToolsQueueSplineSlot();

	if (!IsSplineUsable(SplineComponent) || UnitIndex < 0)
	{
		return false;
	}

	const double SplineLength = static_cast<double>(SplineComponent->GetSplineLength());
	const double SafeSpacing = FMath::Max(Config.Spacing, 1.0);
	const double DirectionSign = Config.bHeadAtEnd ? -1.0 : 1.0;

	FRandomStream RandomStream(Config.RandomSeed + UnitIndex * 7919);
	const double DistanceJitter = GetRandomRange(RandomStream, FMath::Max(Config.DistanceJitter, 0.0));
	const double SideJitter = GetRandomRange(RandomStream, FMath::Max(Config.SideJitter, 0.0));

	double Distance = GetQueueHeadDistance(SplineLength, Config, UnitIndex)
		+ DirectionSign * (SafeSpacing * static_cast<double>(UnitIndex) + DistanceJitter);
	if (Config.bClampToSpline)
	{
		Distance = ClampDistanceToSpline(Distance, SplineLength);
	}

	const double SideSign = GetSignedSideForIndex(UnitIndex, Config);
	const double RightOffset = SideSign * FMath::Max(Config.SideOffset, 0.0) + SideJitter;
	const FVector CenterLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
		static_cast<float>(Distance),
		ESplineCoordinateSpace::World);
	const FVector RightVector = SplineComponent->GetRightVectorAtDistanceAlongSpline(
		static_cast<float>(Distance),
		ESplineCoordinateSpace::World);
	const FRotator Rotation = SplineComponent->GetRotationAtDistanceAlongSpline(
		static_cast<float>(Distance),
		ESplineCoordinateSpace::World);

	OutSlot.Index = UnitIndex;
	OutSlot.Distance = Distance;
	OutSlot.RightOffset = RightOffset;
	OutSlot.CenterLocation = CenterLocation;
	OutSlot.TargetLocation = CenterLocation + RightVector * static_cast<float>(RightOffset);
	OutSlot.TargetRotation = Rotation;

	return true;
}

double UQueueSplineLibrary::InterpQueueSplineRightOffset(
	double CurrentRightOffset,
	double TargetRightOffset,
	double DeltaTime,
	double ReturnSpeed)
{
	if (DeltaTime <= 0.0)
	{
		return CurrentRightOffset;
	}

	const float SafeReturnSpeed = static_cast<float>(FMath::Max(ReturnSpeed, 0.0));
	return static_cast<double>(FMath::FInterpTo(
		static_cast<float>(CurrentRightOffset),
		static_cast<float>(TargetRightOffset),
		static_cast<float>(DeltaTime),
		SafeReturnSpeed));
}

bool UQueueSplineLibrary::IsQueueSplineConfigValid(
	USplineComponent* SplineComponent,
	const FXToolsQueueSplineConfig& Config,
	FString& OutMessage)
{
	OutMessage.Reset();

	if (!IsValid(SplineComponent))
	{
		OutMessage = TEXT("样条组件无效。");
		return false;
	}

	if (SplineComponent->GetNumberOfSplinePoints() < 2)
	{
		OutMessage = TEXT("样条点数量不足，至少需要2个点。");
		return false;
	}

	if (SplineComponent->GetSplineLength() <= KINDA_SMALL_NUMBER)
	{
		OutMessage = TEXT("样条长度过短。");
		return false;
	}

	if (Config.UnitCount < 0)
	{
		OutMessage = TEXT("人数不能为负数。");
		return false;
	}

	if (Config.Spacing <= 0.0)
	{
		OutMessage = TEXT("前后间距必须大于0。");
		return false;
	}

	OutMessage = TEXT("配置有效。");
	return true;
}
