/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "CurveTools/X_CurvePresetLibrary.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
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

bool UXCurvePresetLibrary::DoesCurveFloatMatchCubicKeys(
    const UCurveFloat* Curve,
    const TArray<FVector4>& Keys)
{
    if (!IsValid(Curve) || Keys.Num() < 2)
    {
        return false;
    }

    const TArray<FRichCurveKey>& CurveKeys = Curve->FloatCurve.GetConstRefOfKeys();
    if (CurveKeys.Num() != Keys.Num())
    {
        return false;
    }

    TOptional<float> PreviousTime;
    for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
    {
        const FVector4& Expected = Keys[KeyIndex];
        const float ExpectedTime = static_cast<float>(Expected.X);
        const float ExpectedValue = static_cast<float>(Expected.Y);
        const float ExpectedArriveTangent = static_cast<float>(Expected.Z);
        const float ExpectedLeaveTangent = static_cast<float>(Expected.W);
        const ERichCurveTangentMode ExpectedTangentMode =
            FMath::IsNearlyEqual(ExpectedArriveTangent, ExpectedLeaveTangent) ? RCTM_User : RCTM_Break;
        const FRichCurveKey& Actual = CurveKeys[KeyIndex];

        if (!FMath::IsFinite(ExpectedTime) || !FMath::IsFinite(ExpectedValue)
            || !FMath::IsFinite(ExpectedArriveTangent) || !FMath::IsFinite(ExpectedLeaveTangent)
            || (PreviousTime.IsSet() && ExpectedTime <= PreviousTime.GetValue())
            || Actual.InterpMode != RCIM_Cubic
            || Actual.TangentMode != ExpectedTangentMode
            || Actual.TangentWeightMode != RCTWM_WeightedNone
            || !FMath::IsNearlyEqual(Actual.Time, ExpectedTime)
            || !FMath::IsNearlyEqual(Actual.Value, ExpectedValue)
            || !FMath::IsNearlyEqual(Actual.ArriveTangent, ExpectedArriveTangent)
            || !FMath::IsNearlyEqual(Actual.LeaveTangent, ExpectedLeaveTangent))
        {
            return false;
        }

        PreviousTime = ExpectedTime;
    }

    return true;
}

bool UXCurvePresetLibrary::SetCurveVectorCubicKeys(UCurveVector* Curve, const TArray<FVector4>& Keys)
{
    if (!IsValid(Curve) || Keys.Num() < 2)
    {
        return false;
    }

    TArray<FRichCurveKey> CurveKeys[3];
    for (TArray<FRichCurveKey>& AxisKeys : CurveKeys)
    {
        AxisKeys.Reserve(Keys.Num());
    }

    TOptional<float> PreviousTime;
    for (const FVector4& KeyData : Keys)
    {
        const float Time = static_cast<float>(KeyData.X);
        const float Values[3] = {
            static_cast<float>(KeyData.Y),
            static_cast<float>(KeyData.Z),
            static_cast<float>(KeyData.W)
        };
        if (!FMath::IsFinite(Time) || !FMath::IsFinite(Values[0])
            || !FMath::IsFinite(Values[1]) || !FMath::IsFinite(Values[2])
            || (PreviousTime.IsSet() && Time <= PreviousTime.GetValue()))
        {
            return false;
        }

        for (int32 Axis = 0; Axis < UE_ARRAY_COUNT(CurveKeys); ++Axis)
        {
            FRichCurveKey& Key = CurveKeys[Axis].Emplace_GetRef(Time, Values[Axis]);
            Key.InterpMode = RCIM_Cubic;
            Key.TangentMode = RCTM_User;
            Key.TangentWeightMode = RCTWM_WeightedNone;
            Key.ArriveTangent = 0.0f;
            Key.LeaveTangent = 0.0f;
        }
        PreviousTime = Time;
    }

    Curve->Modify();
    for (int32 Axis = 0; Axis < UE_ARRAY_COUNT(CurveKeys); ++Axis)
    {
        Curve->FloatCurves[Axis].SetKeys(CurveKeys[Axis]);
    }
    Curve->MarkPackageDirty();
    Curve->PostEditChange();
    return true;
}

bool UXCurvePresetLibrary::DoesCurveVectorMatchCubicKeys(
    const UCurveVector* Curve,
    const TArray<FVector4>& Keys)
{
    if (!IsValid(Curve) || Keys.Num() < 2)
    {
        return false;
    }

    for (const FRichCurve& AxisCurve : Curve->FloatCurves)
    {
        if (AxisCurve.GetConstRefOfKeys().Num() != Keys.Num())
        {
            return false;
        }
    }

    TOptional<float> PreviousTime;
    for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
    {
        const FVector4& Expected = Keys[KeyIndex];
        const float ExpectedTime = static_cast<float>(Expected.X);
        const float ExpectedValues[] = {
            static_cast<float>(Expected.Y),
            static_cast<float>(Expected.Z),
            static_cast<float>(Expected.W)
        };
        if (!FMath::IsFinite(ExpectedTime) || !FMath::IsFinite(ExpectedValues[0])
            || !FMath::IsFinite(ExpectedValues[1]) || !FMath::IsFinite(ExpectedValues[2])
            || (PreviousTime.IsSet() && ExpectedTime <= PreviousTime.GetValue()))
        {
            return false;
        }

        for (int32 Axis = 0; Axis < UE_ARRAY_COUNT(ExpectedValues); ++Axis)
        {
            const FRichCurveKey& Actual = Curve->FloatCurves[Axis].GetConstRefOfKeys()[KeyIndex];
            if (Actual.InterpMode != RCIM_Cubic
                || Actual.TangentMode != RCTM_User
                || Actual.TangentWeightMode != RCTWM_WeightedNone
                || !FMath::IsNearlyEqual(Actual.Time, ExpectedTime)
                || !FMath::IsNearlyEqual(Actual.Value, ExpectedValues[Axis])
                || !FMath::IsNearlyZero(Actual.ArriveTangent)
                || !FMath::IsNearlyZero(Actual.LeaveTangent))
            {
                return false;
            }
        }

        PreviousTime = ExpectedTime;
    }

    return true;
}

bool UXCurvePresetLibrary::IsCurveVectorCubicSmooth(const UCurveVector* Curve)
{
    if (!IsValid(Curve))
    {
        return false;
    }

    for (const FRichCurve& AxisCurve : Curve->FloatCurves)
    {
        const TArray<FRichCurveKey>& Keys = AxisCurve.GetConstRefOfKeys();
        if (Keys.Num() < 2)
        {
            return false;
        }

        for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
        {
            const FRichCurveKey& Key = Keys[KeyIndex];
            if (Key.InterpMode != RCIM_Cubic || Key.TangentWeightMode != RCTWM_WeightedNone
                || !FMath::IsFinite(Key.ArriveTangent) || !FMath::IsFinite(Key.LeaveTangent))
            {
                return false;
            }

            const bool bInternalKey = KeyIndex > 0 && KeyIndex < Keys.Num() - 1;
            if (bInternalKey && !FMath::IsNearlyEqual(Key.ArriveTangent, Key.LeaveTangent))
            {
                return false;
            }
        }
    }

    return true;
}
