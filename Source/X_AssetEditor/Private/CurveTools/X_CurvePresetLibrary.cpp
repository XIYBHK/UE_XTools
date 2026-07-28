/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "CurveTools/X_CurvePresetLibrary.h"

#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"

bool UXCurvePresetLibrary::SetCurveFloatCubicKeys(UCurveFloat* Curve, const TArray<FVector4>& Keys)
{
    if (!IsValid(Curve) || Keys.Num() < 2)
    {
        return false;
    }

    TArray<FRichCurveKey> CurveKeys;
    CurveKeys.Reserve(Keys.Num());

    TOptional<float> PreviousTime;
    for (const FVector4& KeyData : Keys)
    {
        const float Time = static_cast<float>(KeyData.X);
        const float Value = static_cast<float>(KeyData.Y);
        const float ArriveTangent = static_cast<float>(KeyData.Z);
        const float LeaveTangent = static_cast<float>(KeyData.W);
        if (!FMath::IsFinite(Time) || !FMath::IsFinite(Value)
            || !FMath::IsFinite(ArriveTangent) || !FMath::IsFinite(LeaveTangent)
            || (PreviousTime.IsSet() && Time <= PreviousTime.GetValue()))
        {
            return false;
        }

        FRichCurveKey& Key = CurveKeys.Emplace_GetRef(Time, Value);
        Key.InterpMode = RCIM_Cubic;
        Key.TangentMode = FMath::IsNearlyEqual(ArriveTangent, LeaveTangent) ? RCTM_User : RCTM_Break;
        Key.TangentWeightMode = RCTWM_WeightedNone;
        Key.ArriveTangent = ArriveTangent;
        Key.LeaveTangent = LeaveTangent;
        PreviousTime = Time;
    }

    Curve->Modify();
    Curve->FloatCurve.SetKeys(CurveKeys);
    Curve->MarkPackageDirty();
    Curve->PostEditChange();
    return true;
}
