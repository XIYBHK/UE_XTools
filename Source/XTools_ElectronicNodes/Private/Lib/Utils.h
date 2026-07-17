#pragma once

#include "CoreMinimal.h"

namespace ENIntersectionHelpers
{
	static bool SegmentIntersection2D(
		const FVector2D& SegmentAStart,
		const FVector2D& SegmentAEnd,
		const FVector2D& SegmentBStart,
		const FVector2D& SegmentBEnd,
		FVector2D& OutIntersectionPoint)
	{
		FVector Intersection;
		if (!FMath::SegmentIntersection2D(
			FVector(SegmentAStart.X, SegmentAStart.Y, 0.0),
			FVector(SegmentAEnd.X, SegmentAEnd.Y, 0.0),
			FVector(SegmentBStart.X, SegmentBStart.Y, 0.0),
			FVector(SegmentBEnd.X, SegmentBEnd.Y, 0.0),
			Intersection))
		{
			return false;
		}

		OutIntersectionPoint = FVector2D(Intersection.X, Intersection.Y);
		return true;
	}
}
