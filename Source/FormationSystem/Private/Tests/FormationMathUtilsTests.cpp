#if WITH_DEV_AUTOMATION_TESTS

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

#endif
