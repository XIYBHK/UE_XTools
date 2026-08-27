/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Async/ParallelFor.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffle_PRDWorldCleanupLifecycleIsDeterministic,
	"XTools.RandomShuffles.PRD.WorldCleanupLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_PRDWorldCleanupLifecycleIsDeterministic::RunTest(const FString& Parameters)
{
	using RandomShuffles::ShouldResetPRDStatesOnWorldCleanup;

	// 纯判定矩阵：是否应在世界清理广播时自动清空 PRD 状态
	TestTrue(TEXT("最后一个Game/PIE世界且会话结束时应清理"),
		ShouldResetPRDStatesOnWorldCleanup(true, false));
	TestFalse(TEXT("仍存在其它存活Game/PIE世界时不应清理"),
		ShouldResetPRDStatesOnWorldCleanup(true, true));
	TestFalse(TEXT("无缝切换中继跳(bSessionEnded=false)不应清理"),
		ShouldResetPRDStatesOnWorldCleanup(false, false));
	TestFalse(TEXT("bSessionEnded=false且存在其它存活世界时不应清理"),
		ShouldResetPRDStatesOnWorldCleanup(false, true));

	// 模拟多世界拆除序列：与 HandleWorldCleanup 使用同一判定驱动清理路径
	URandomShuffleArrayLibrary::ClearAllPRDStates();
	URandomShuffleArrayLibrary::ResetPRDPerformanceStats();

	FRandomStream Stream(987654);
	URandomShuffleArrayLibrary::PseudoRandomBoolFromStream(0.5f, Stream, TEXT("RandomShuffleTest.WorldCleanup.Multi"));
	FPRDPerformanceStats Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("前置：PRD调用后应存在一个状态"), Stats.StateMapSize, 1);
	TestEqual(TEXT("前置：PRD调用后统计应记录一次调用"), Stats.TotalCalls, 1);

	// 世界A清理但世界B仍存活：判定不清空，状态必须保留
	if (ShouldResetPRDStatesOnWorldCleanup(true, true))
	{
		URandomShuffleArrayLibrary::ClearPRDStatesForWorldTeardown();
	}
	Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("多世界场景：清理其中一个世界不应清空状态"), Stats.StateMapSize, 1);

	// 世界B清理（最后一个存活世界）：判定清空
	if (ShouldResetPRDStatesOnWorldCleanup(true, false))
	{
		URandomShuffleArrayLibrary::ClearPRDStatesForWorldTeardown();
	}
	Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("多世界场景：最后一个世界清理后状态应清空"), Stats.StateMapSize, 0);
	TestEqual(TEXT("自动清理路径不应重置性能统计"), Stats.TotalCalls, 1);

	// 行为级确定性：自动清理后，相同随机流必须重现首个序列（证明失败计数真正归零）
	const FString StateID = TEXT("RandomShuffleTest.WorldCleanup.Det");
	TArray<bool> FirstSequence;
	{
		FRandomStream FirstStream(1357911);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			FirstSequence.Add(URandomShuffleArrayLibrary::PseudoRandomBoolFromStream(0.3f, FirstStream, StateID));
		}
	}

	URandomShuffleArrayLibrary::ClearPRDStatesForWorldTeardown();

	TArray<bool> SecondSequence;
	{
		FRandomStream SecondStream(1357911);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			SecondSequence.Add(URandomShuffleArrayLibrary::PseudoRandomBoolFromStream(0.3f, SecondStream, StateID));
		}
	}
	TestEqual(TEXT("自动清理后相同随机流的PRD序列应可重现"), FirstSequence, SecondSequence);

	// 回归：显式 ClearAllPRDStates 保持原有行为（状态与统计同时重置）
        URandomShuffleArrayLibrary::ClearAllPRDStates();
        Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
        TestEqual(TEXT("显式清理后状态映射应为空"), Stats.StateMapSize, 0);
        TestEqual(TEXT("显式清理后统计应重置"), Stats.TotalCalls, 0);

        return true;
}

// ---------------------------------------------------------------------------
// 状态映射满表（MaxStateMapSize=1000）后的确定性回归：
// 1) 填满 1000 个不同 StateID 后映射不再增长；
// 2) 自动路径第 1001 个新 ID：现有 Error 日志触发、写入被丢弃、映射不增长；
// 3) Advanced 写路径：满表丢弃 + 同一 StateID 的去重 Warning 只打一次；
// 4) 清理后映射与去重集合一并复位，Advanced 恢复正常记录。
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffle_PRDStateMapCapIsEnforced,
	"XTools.RandomShuffles.PRD.StateMapCapIsEnforced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_PRDStateMapCapIsEnforced::RunTest(const FString& Parameters)
{
	URandomShuffleArrayLibrary::ClearAllPRDStates();

	// 1) 自动路径填满 1000 个不同 StateID（每个均为正常插入，无满表日志）
	{
		FRandomStream FillStream(20260821);
		for (int32 Index = 0; Index < 1000; ++Index)
		{
			URandomShuffleArrayLibrary::PseudoRandomBoolFromStream(0.3f, FillStream,
				FString::Printf(TEXT("RandomShuffleTest.FullMap.%d"), Index));
		}
	}
	FPRDPerformanceStats Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("填满上限后状态映射应为1000"), Stats.StateMapSize, 1000);

	// 2) 自动路径第 1001 个新 StateID：GetOrCreate 的既有 Error 触发，写入被丢弃，映射不增长
	AddExpectedError(TEXT("PRD状态映射已达到最大大小限制"));
	FRandomStream OverflowStream(1);
	URandomShuffleArrayLibrary::PseudoRandomBoolFromStream(0.3f, OverflowStream,
		TEXT("RandomShuffleTest.FullMap.OverflowAuto"));
	Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("满表后自动路径不应新增状态"), Stats.StateMapSize, 1000);

	// 3) Advanced 写路径：满表丢弃并打去重 Warning；判定本身不受满表影响（P=1 必触发）。
	// 预期日志"恰好1次"一次性覆盖三个断言点：满表首调打1条、同ID去重不再打、清理后恢复不打
	// （UE 5.3 的 Occurrences==0 语义是"至少一次"，零次断言不可用，故采用计数法）
	AddExpectedMessage(TEXT("无法记录状态 'RandomShuffleTest.FullMap.OverflowAdvanced'"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	int32 FailureCount = 0;
	float ActualChance = 0.0f;
	FRandomStream AdvancedStream(1);
	const bool bAdvancedResult = URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(
		1.0f, AdvancedStream, FailureCount, ActualChance,
		TEXT("RandomShuffleTest.FullMap.OverflowAdvanced"), 5);
	TestTrue(TEXT("满表不影响Advanced判定结果"), bAdvancedResult);
	TestEqual(TEXT("满表不影响Advanced失败次数推进"), FailureCount, 0);
	TestEqual(TEXT("满表不影响Advanced概率输出"), ActualChance, 1.0f);
	Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("满表后Advanced路径不应新增状态"), Stats.StateMapSize, 1000);

	// 同一 StateID 再次调用：去重后不再打 Warning（计入上方"恰好1次"预期）
	URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(
		1.0f, AdvancedStream, FailureCount, ActualChance,
		TEXT("RandomShuffleTest.FullMap.OverflowAdvanced"), 5);
	Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("去重不影响满表丢弃行为"), Stats.StateMapSize, 1000);

	// 4) 清理后映射与去重集合一并复位，Advanced 恢复正常记录（无Warning，仍计入"恰好1次"预期）
	URandomShuffleArrayLibrary::ClearAllPRDStates();
	URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(
		1.0f, AdvancedStream, FailureCount, ActualChance,
		TEXT("RandomShuffleTest.FullMap.OverflowAdvanced"), 5);
	Stats = URandomShuffleArrayLibrary::GetPRDPerformanceStats();
	TestEqual(TEXT("清理后Advanced应恢复状态记录"), Stats.StateMapSize, 1);

	URandomShuffleArrayLibrary::ClearAllPRDStates();

	return true;
}

// ---------------------------------------------------------------------------
// M-7 回归：自动状态路径（PseudoRandomBool/FromStream）的读-算-写原子性。
// 通过 friend 注入"始终失败"的受控随机源（0 < BaseChance < 1 时 RandomFunc()=1.0
// 必然不满足 < OutActualChance，包括概率饱和到 1.0 的严格小于情形），使失败计数
// 成为完全确定性的递增序列：
// 1) 单线程：失败计数逐次严格 +1；
// 2) 上限：持续推进后计数被钳制在 MaxFailureCount；
// 3) 多线程：同一 StateID 并发必失败调用后，失败计数严格等于总调用次数。
// 断言不含任何概率成分，也不依赖调度时序——原子实现下恒通过。
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffle_PRDAutoStateAtomicUpdate,
	"XTools.RandomShuffles.PRD.AutoStateAtomicUpdate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_PRDAutoStateAtomicUpdate::RunTest(const FString& Parameters)
{
	// 受控随机源：对任意 0 < P < 1 恒失败（OutActualChance <= 1，1.0 < OutActualChance 恒为假）
	const TFunction<float()> AlwaysFail = []() { return 1.0f; };

	URandomShuffleArrayLibrary::ClearAllPRDStates();

	// 1) 单线程确定性：失败计数逐次严格 +1，且与输出参数一致
	const FString SingleID = TEXT("RandomShuffleTest.AutoAtomic.Single");
	for (int32 Index = 0; Index < 5; ++Index)
	{
		int32 Updated = INDEX_NONE;
		TestFalse(TEXT("注入必失败随机值时不应触发"),
			URandomShuffleArrayLibrary::ApplyPRDAutoLocked(0.5f, SingleID, AlwaysFail, Updated));
		TestEqual(TEXT("失败计数应逐次严格+1"), Updated, Index + 1);
	}

	// 2) 上限确定性：超过 MaxFailureCount 后计数被钳制在上限
	const FString CapID = TEXT("RandomShuffleTest.AutoAtomic.Cap");
	int32 CapUpdated = 0;
	for (int32 Index = 0; Index < RandomShuffles::Config::MaxFailureCount + 5; ++Index)
	{
		URandomShuffleArrayLibrary::ApplyPRDAutoLocked(0.5f, CapID, AlwaysFail, CapUpdated);
	}
	TestEqual(TEXT("失败计数应被钳制在MaxFailureCount"), CapUpdated, RandomShuffles::Config::MaxFailureCount);

	// 3) 多线程原子性：同一 StateID 并发必失败调用，失败计数严格等于总调用次数
	const FString SharedID = TEXT("RandomShuffleTest.AutoAtomic.Concurrent");
	constexpr int32 NumTasks = 8;
	constexpr int32 CallsPerTask = 125;
	constexpr int32 TotalCalls = NumTasks * CallsPerTask; // 1000，远低于上限，排除钳制干扰
	static_assert(TotalCalls <= RandomShuffles::Config::MaxFailureCount, "并发总次数必须低于失败计数上限");

	TAtomic<int32> ExecutedCalls(0);
	ParallelFor(NumTasks, [&ExecutedCalls, &AlwaysFail, &SharedID](int32)
	{
		for (int32 Index = 0; Index < CallsPerTask; ++Index)
		{
			int32 Ignored = 0;
			URandomShuffleArrayLibrary::ApplyPRDAutoLocked(0.5f, SharedID, AlwaysFail, Ignored);
			++ExecutedCalls;
		}
	});
	TestEqual(TEXT("并发任务应全部执行完毕"), ExecutedCalls.Load(), TotalCalls);

	// 读取共享状态快照（friend 私有访问；锁内读取与生产路径同一把锁）
	int32 FinalCount = INDEX_NONE;
	{
		FScopeLock Lock(&URandomShuffleArrayLibrary::PRDStateLock);
		if (const int32* Value = URandomShuffleArrayLibrary::PRDStateMap.Find(FName(*SharedID)))
		{
			FinalCount = *Value;
		}
	}
	TestEqual(TEXT("并发后失败计数应严格等于总调用次数（无丢失更新）"), FinalCount, TotalCalls);

	// 清理测试状态，避免影响后续测试的 StateMapSize 断言
	URandomShuffleArrayLibrary::ClearAllPRDStates();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomShuffle_PRDInterpolatesBetweenTableEntries,
	"XTools.RandomShuffles.PRD.InterpolatesBetweenTableEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRandomShuffle_PRDInterpolatesBetweenTableEntries::RunTest(const FString& Parameters)
{
	URandomShuffleArrayLibrary::ClearAllPRDStates();

	FRandomStream Stream(12345);
	int32 FailureCount = 0;
	float Chance499 = 0.0f;
	float Chance500 = 0.0f;
	float Chance501 = 0.0f;

	URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(
		0.499f, Stream, FailureCount, Chance499, TEXT("RandomShuffleTest.Interpolation.499"), 0);
	URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(
		0.500f, Stream, FailureCount, Chance500, TEXT("RandomShuffleTest.Interpolation.500"), 0);
	URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(
		0.501f, Stream, FailureCount, Chance501, TEXT("RandomShuffleTest.Interpolation.501"), 0);

	TestTrue(TEXT("PRD实际概率应随基础概率连续递增"), Chance499 < Chance500 && Chance500 < Chance501);
	TestTrue(TEXT("0.499应使用相邻表项线性插值"), FMath::IsNearlyEqual(Chance499, 0.3010479f, 0.00001f));
	TestTrue(TEXT("0.501应使用相邻表项线性插值"), FMath::IsNearlyEqual(Chance501, 0.3031604f, 0.00001f));

	URandomShuffleArrayLibrary::ClearAllPRDStates();
	return true;
}

#endif
