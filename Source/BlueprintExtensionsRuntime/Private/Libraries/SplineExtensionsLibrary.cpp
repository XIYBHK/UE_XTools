#include "Libraries/SplineExtensionsLibrary.h"
#include "Components/SplineComponent.h"

bool USplineExtensionsLibrary::SplinePathValid(USplineComponent* SplineComponent)
{
	if (!SplineComponent)
	{
		return false;
	}

	return SplineComponent->GetNumberOfSplinePoints() > 1;
}

FVector USplineExtensionsLibrary::GetSplineStart(USplineComponent* SplineComponent)
{
	if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() == 0)
	{
		return FVector::ZeroVector;
	}

	return SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
}

FVector USplineExtensionsLibrary::GetSplineEnd(USplineComponent* SplineComponent)
{
	int32 NumPoints = SplineComponent ? SplineComponent->GetNumberOfSplinePoints() : 0;
	if (NumPoints == 0)
	{
		return FVector::ZeroVector;
	}

	return SplineComponent->GetLocationAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::World);
}

TArray<FVector> USplineExtensionsLibrary::GetSplinePath(USplineComponent* SplineComponent)
{
	TArray<FVector> SplinePath;

	if (!SplineComponent)
	{
		return SplinePath;
	}

	int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
	for (int32 i = 0; i < NumPoints; ++i)
	{
		FVector Location = SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		SplinePath.Add(Location);
	}

	return SplinePath;
}

void USplineExtensionsLibrary::SimplifySpline(USplineComponent* SplineComponent, const TArray<FVector>& SplinePath)
{
	if (!SplineComponent || SplinePath.Num() == 0)
	{
		return;
	}

	// 清空现有的样条点
	SplineComponent->ClearSplinePoints();

	// 添加新的样条点
	for (const FVector& Location : SplinePath)
	{
		SplineComponent->AddSplinePoint(Location, ESplineCoordinateSpace::World, true);
	}

	// 更新样条曲线
	SplineComponent->UpdateSpline();

	// 简化样条点
	for (int32 i = 1; i < SplineComponent->GetNumberOfSplinePoints() - 1; ++i)
	{
		FVector PrevLocation = SplineComponent->GetLocationAtSplinePoint(i - 1, ESplineCoordinateSpace::World);
		FVector CurrentLocation = SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		FVector NextLocation = SplineComponent->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World);

		// 计算前后点的切线
		FVector PrevTangent = (CurrentLocation - PrevLocation).GetSafeNormal();
		FVector NextTangent = (NextLocation - CurrentLocation).GetSafeNormal();

		// 如果前后点的切线相同，则移除当前点
		if (PrevTangent.Equals(NextTangent, KINDA_SMALL_NUMBER))
		{
			SplineComponent->RemoveSplinePoint(i);
			--i; // 调整索引以处理移除后的点
		}
		else
		{
			// 调整当前点的切线
			FVector ArriveTangent = (CurrentLocation - PrevLocation) / 2.0f;
			FVector LeaveTangent = (NextLocation - CurrentLocation) / 2.0f;

			SplineComponent->SetTangentAtSplinePoint(i, ArriveTangent, ESplineCoordinateSpace::World, true);
			SplineComponent->SetTangentAtSplinePoint(i, LeaveTangent, ESplineCoordinateSpace::World, false);
		}
	}

	// 再次更新样条曲线
	SplineComponent->UpdateSpline();
}