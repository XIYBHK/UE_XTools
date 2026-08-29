#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationMathUtils.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFormationMathUtils_RejectsNegativeSeparationWeight,
    "XTools.Formation.MathUtils.RejectsNegativeSeparationWeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMathUtils_RejectsNegativeSeparationWeight::RunTest(const FString& Parameters)
{
    FBoidsMovementParams BoidsParams;
    BoidsParams.SeparationWeight = -1.0f;

    TArray<FVector> Positions;
    Positions.Add(FVector::ZeroVector);
    Positions.Add(FVector(10.0f, 0.0f, 0.0f));

    AddExpectedError(TEXT("CalculateSeparationForce: SeparationWeight"), EAutomationExpectedErrorFlags::Contains);
    const FVector SeparationForce = FFormationMathUtils::CalculateSeparationForce(0, Positions, BoidsParams);

    TestTrue(TEXT("Negative separation weight returns zero force"), SeparationForce.IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFormationMathUtils_RejectsMismatchedAlignmentInputs,
    "XTools.Formation.MathUtils.RejectsMismatchedAlignmentInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMathUtils_RejectsMismatchedAlignmentInputs::RunTest(const FString& Parameters)
{
    FBoidsMovementParams BoidsParams;
    TArray<FVector> Positions = { FVector::ZeroVector, FVector(10.0f, 0.0f, 0.0f) };
    TArray<FVector> Velocities = { FVector::ZeroVector };

    AddExpectedError(TEXT("位置和速度数组大小不一致"), EAutomationExpectedErrorFlags::Contains, 1);
    const FVector AlignmentForce =
        FFormationMathUtils::CalculateAlignmentForce(0, Positions, Velocities, BoidsParams);

    TestTrue(TEXT("数组长度不一致时应安全返回零向量"), AlignmentForce.IsNearlyZero());
    return true;
}

#endif
