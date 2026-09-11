#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationMathUtils.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationMath_PathDistance,
    "XTools.Formation.MathUtils.PathDistanceDegeneracies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationMath_PathDistance::RunTest(const FString& Parameters)
{
    struct FCase { FVector A, B, C, D; float Threshold; bool bExpected; };
    const TArray<FCase> Cases = {
        {{0,0,0}, {100,0,0}, {0,10,0}, {100,10,0}, 10, true},
        {{0,0,0}, {100,0,0}, {0,10,0}, {100,10,0}, 9, false},
        {{0,0,0}, {100,0,0}, {0,10,0}, {100,10,0}, 0, false},
        {{50,0,0}, {50,0,0}, {0,0,0}, {100,0,0}, 0, true},
        {{50,10,0}, {50,10,0}, {0,0,0}, {100,0,0}, 10, true},
        {{50,10,0}, {50,10,0}, {0,0,0}, {100,0,0}, 9, false},
        {{0,0,0}, {0,0,0}, {3,4,0}, {3,4,0}, 5, true},
        {{0,0,0}, {0,0,0}, {3,4,0}, {3,4,0}, 4, false},
        {{0,0,0}, {100,0,0}, {50,0,0}, {150,0,0}, 0, true},
        {{0,0,0}, {100,0,0}, {110,0,0}, {150,0,0}, 10, true},
        {{0,0,0}, {100,0,0}, {110,10,0}, {110,100,0}, 14, false},
        {{0,0,0}, {100,0,0}, {110,10,0}, {110,100,0}, 15, true},
        {{-10000,0,0}, {10000,0,0}, {-10000,-1,0}, {10000,1,0}, 0, true},
        {{0,0,100}, {100,0,100}, {50,-10,-100}, {50,10,-100}, 0, true}
    };
    for (const FCase& Item : Cases)
    {
        for (int32 Order = 0; Order < 8; ++Order)
        {
            const FVector A = (Order & 1) ? Item.B : Item.A;
            const FVector B = (Order & 1) ? Item.A : Item.B;
            const FVector C = (Order & 2) ? Item.D : Item.C;
            const FVector D = (Order & 2) ? Item.C : Item.D;
            const bool bActual = (Order & 4)
                ? FFormationMathUtils::DoPathsIntersect(C, D, A, B, Item.Threshold)
                : FFormationMathUtils::DoPathsIntersect(A, B, C, D, Item.Threshold);
            TestEqual(TEXT("路径距离判断与端点及路径顺序无关"), bActual, Item.bExpected);
        }
    }
    return true;
}

#endif
