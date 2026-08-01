/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RandomSample.h"
#include "RandomShuffleArrayLibrary.h"
#include "WeightPoolSample.h"

namespace
{
	struct FStreamRandom
	{
		explicit FStreamRandom(FRandomStream& InStream)
			: Stream(InStream)
		{
		}

		float operator()(float Min, float Max) const
		{
			return Stream.FRandRange(Min, Max);
		}

		FRandomStream& Stream;
	};

	bool ContainsOnly(const TArray<int32>& Values, const TArray<int32>& AllowedValues)
	{
		for (const int32 Value : Values)
		{
			if (!AllowedValues.Contains(Value))
			{
				return false;
			}
		}
		return true;
	}

	int32 CountValue(const TArray<int32>& Values, int32 Value)
	{
		int32 Count = 0;
		for (const int32 Candidate : Values)
		{
			Count += Candidate == Value ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffle_UnweightedSamplingIsReproducible,
	"XTools.RandomShuffles.Sampling.UnweightedIsReproducible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_UnweightedSamplingIsReproducible::RunTest(const FString& Parameters)
{
	const TArray<int32> Input = { 10, 20, 30 };
	TArray<int32> FirstResult;
	TArray<int32> SecondResult;
	FirstResult.SetNumUninitialized(8);
	SecondResult.SetNumUninitialized(8);

	FRandomStream FirstStream(13579);
	FRandomStream SecondStream(13579);
	RandomShuffles::UniformRandomSample(
		Input.GetData(), Input.GetData() + Input.Num(), FirstResult.GetData(), FirstResult.Num(), FStreamRandom(FirstStream));
	RandomShuffles::UniformRandomSample(
		Input.GetData(), Input.GetData() + Input.Num(), SecondResult.GetData(), SecondResult.Num(), FStreamRandom(SecondStream));

	TestEqual(TEXT("固定随机流的均匀采样应可重现"), FirstResult, SecondResult);
	TestEqual(TEXT("均匀采样应返回请求数量"), FirstResult.Num(), 8);
	TestTrue(TEXT("均匀采样结果应全部来自输入数组"), ContainsOnly(FirstResult, Input));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffle_WeightedSamplingHonorsEligibleValues,
	"XTools.RandomShuffles.Sampling.WeightedHonorsEligibleValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_WeightedSamplingHonorsEligibleValues::RunTest(const FString& Parameters)
{
	const TArray<int32> Input = { 10, 20, 30 };
	const TArray<float> Weights = { 1.0f, 0.0f, 2.0f };
	TArray<int32> FirstResult;
	TArray<int32> SecondResult;
	FirstResult.SetNumUninitialized(12);
	SecondResult.SetNumUninitialized(12);

	FRandomStream FirstStream(24680);
	FRandomStream SecondStream(24680);
	RandomShuffles::RandomSample(
		Input.GetData(), Input.GetData() + Input.Num(), Weights.GetData(), FirstResult.GetData(), FirstResult.Num(), FStreamRandom(FirstStream));
	RandomShuffles::RandomSample(
		Input.GetData(), Input.GetData() + Input.Num(), Weights.GetData(), SecondResult.GetData(), SecondResult.Num(), FStreamRandom(SecondStream));

	TestEqual(TEXT("固定随机流的加权采样应可重现"), FirstResult, SecondResult);
	TestEqual(TEXT("加权采样应返回请求数量"), FirstResult.Num(), 12);
	TestTrue(TEXT("零权重元素不应被选中"), !FirstResult.Contains(20));
	TestTrue(TEXT("加权采样结果应全部来自正权重输入"), ContainsOnly(FirstResult, { 10, 30 }));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffle_StrictWeightSamplingMatchesAllocation,
	"XTools.RandomShuffles.Sampling.StrictWeightMatchesAllocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_StrictWeightSamplingMatchesAllocation::RunTest(const FString& Parameters)
{
	const TArray<int32> Input = { 10, 20, 30 };
	const TArray<float> Weights = { 1.0f, 2.0f, 3.0f };
	TArray<int32> FirstResult;
	TArray<int32> SecondResult;
	FirstResult.SetNumUninitialized(12);
	SecondResult.SetNumUninitialized(12);

	FRandomStream FirstStream(112233);
	FRandomStream SecondStream(112233);
	RandomShuffles::WeightPoolSample(
		Input.GetData(), Input.GetData() + Input.Num(), Weights.GetData(), FirstResult.GetData(), FirstResult.Num(), FStreamRandom(FirstStream));
	RandomShuffles::WeightPoolSample(
		Input.GetData(), Input.GetData() + Input.Num(), Weights.GetData(), SecondResult.GetData(), SecondResult.Num(), FStreamRandom(SecondStream));

	TestEqual(TEXT("固定随机流的严格权重采样应可重现"), FirstResult, SecondResult);
	TestEqual(TEXT("严格权重采样应返回请求数量"), FirstResult.Num(), 12);
	TestEqual(TEXT("权重1应分配2个结果"), CountValue(FirstResult, 10), 2);
	TestEqual(TEXT("权重2应分配4个结果"), CountValue(FirstResult, 20), 4);
	TestEqual(TEXT("权重3应分配6个结果"), CountValue(FirstResult, 30), 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffle_PRDStateAndBoundsAreControlled,
	"XTools.RandomShuffles.PRD.StateAndBoundsAreControlled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_PRDStateAndBoundsAreControlled::RunTest(const FString& Parameters)
{
	URandomShuffleArrayLibrary::ClearAllPRDStates();
	URandomShuffleArrayLibrary::ResetPRDPerformanceStats();

	FRandomStream Stream(424242);
	TestFalse(TEXT("零概率PRD不应触发"),
		URandomShuffleArrayLibrary::PseudoRandomBoolFromStream(0.0f, Stream, TEXT("RandomShuffleTest.Zero")));

	int32 FailureCount = INDEX_NONE;
	float ActualChance = -1.0f;
	TestTrue(TEXT("全概率PRD应始终触发"),
		URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(1.0f, Stream, FailureCount, ActualChance, TEXT("RandomShuffleTest.One"), 12));
	TestEqual(TEXT("触发后失败次数应重置"), FailureCount, 0);
	TestEqual(TEXT("全概率PRD的实际概率应为1"), ActualChance, 1.0f);

	TestFalse(TEXT("高级零概率PRD不应触发"),
		URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(0.0f, Stream, FailureCount, ActualChance, TEXT("RandomShuffleTest.Manual"), 37));
	TestEqual(TEXT("零概率PRD应清除失败次数"), FailureCount, 0);
	TestEqual(TEXT("零概率PRD的实际概率应为0"), ActualChance, 0.0f);

	FPRDPerformanceStats Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("不同StateID应分别记录状态"), Stats.StateMapSize, 3);
	TestEqual(TEXT("PRD统计应记录全部调用"), Stats.TotalCalls, 3);

	URandomShuffleArrayLibrary::ClearPRDState(TEXT("RandomShuffleTest.One"));
	Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("清理单个StateID不应影响其他状态"), Stats.StateMapSize, 2);

	URandomShuffleArrayLibrary::ClearAllPRDStates();
	Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("清理全部状态后映射应为空"), Stats.StateMapSize, 0);
	TestEqual(TEXT("清理全部状态后统计应重置"), Stats.TotalCalls, 0);

	return true;
}

#endif
