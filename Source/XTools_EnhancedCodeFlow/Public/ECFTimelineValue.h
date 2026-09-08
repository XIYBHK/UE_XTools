// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFTypes.h"
#include "Math/Color.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"

namespace ECFInternal
{
	inline float ZeroTimelineTangent(float) { return 0.f; }
	inline FVector ZeroTimelineTangent(const FVector&) { return FVector::ZeroVector; }
	inline FLinearColor ZeroTimelineTangent(const FLinearColor&) { return FLinearColor::Transparent; }

	// Shared by Tick and SetActionTime. Scheduling, endpoints and callbacks stay in
	// the reflected action classes. Unknown blend values retain the previous value.
	template<typename ValueType>
	void EvaluateTimelineValue(ValueType& CurrentValue, const ValueType& StartValue, const ValueType& StopValue,
		float Alpha, EECFBlendFunc BlendFunc, float BlendExp)
	{
		switch (BlendFunc)
		{
		case EECFBlendFunc::ECFBlend_Linear:
			CurrentValue = FMath::Lerp(StartValue, StopValue, Alpha);
			break;
		case EECFBlendFunc::ECFBlend_Cubic:
			CurrentValue = FMath::CubicInterp(StartValue, ZeroTimelineTangent(StartValue), StopValue, ZeroTimelineTangent(StopValue), Alpha);
			break;
		case EECFBlendFunc::ECFBlend_EaseIn:
			CurrentValue = FMath::Lerp(StartValue, StopValue, FMath::Pow(Alpha, BlendExp));
			break;
		case EECFBlendFunc::ECFBlend_EaseOut:
			CurrentValue = FMath::Lerp(StartValue, StopValue, FMath::Pow(Alpha, 1.f / BlendExp));
			break;
		case EECFBlendFunc::ECFBlend_EaseInOut:
			CurrentValue = FMath::InterpEaseInOut(StartValue, StopValue, Alpha, BlendExp);
			break;
		}
	}
}
