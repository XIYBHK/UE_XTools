#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "RandomShuffleArrayLibrary.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRandomShuffle_PRDUpperProbability,
    "XTools.RandomShuffles.PRD.UpperProbabilityExpectation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_PRDUpperProbability::RunTest(const FString& Parameters)
{
    const FString StateID = TEXT("Test.PRD.UpperProbabilityExpectation");
    FRandomStream Stream(54321);
    float PreviousChance = 0.0f;
    for (float Probability : {0.99f, 0.9901f, 0.991f, 0.995f, 0.999f, 0.99999f, 1.0f})
    {
        float Chance = 0.0f;
        int32 FailureCount = 0;
        URandomShuffleArrayLibrary::PseudoRandomBoolAdvanced(Probability, FailureCount, Chance, StateID, 0);
        TestTrue(TEXT("高概率首试概率应严格递增"), Chance > PreviousChance);
        PreviousChance = Chance;

        // 直接枚举等待时间分布，验证长期期望，而非用随机频率复述实现公式。
        double Survival = 1.0;
        double ExpectedAttempts = 0.0;
        for (int32 Attempt = 1; Attempt <= 3; ++Attempt)
        {
            ExpectedAttempts += Survival;
            Survival *= 1.0 - FMath::Min(1.0, static_cast<double>(Chance) * Attempt);
        }
        TestTrue(TEXT("期望触发率应匹配请求概率"), FMath::Abs(1.0 / ExpectedAttempts - Probability) < 1.e-6);
        float StreamChance = 0.0f;
        URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(
            Probability, Stream, FailureCount, StreamChance, StateID, 0);
        TestEqual(TEXT("流送和非流送使用一致的概率"), StreamChance, Chance);
        URandomShuffleArrayLibrary::PseudoRandomBoolAdvanced(Probability, FailureCount, Chance, StateID, 1);
        TestEqual(TEXT("该区间失败一次后应必定触发"), Chance, 1.0f);
        TestEqual(TEXT("成功重置失败计数"), FailureCount, 0);
    }
    URandomShuffleArrayLibrary::ClearPRDState(StateID);
    return true;
}

#endif
