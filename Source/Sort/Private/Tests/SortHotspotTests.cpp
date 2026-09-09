/* Copyright (c) 2025 XIYBHK. Licensed under UE_XTools License. */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "SortLibrary.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include <limits>

namespace
{
	TArray<int32> ReferenceDuplicates(const TArray<FVector>& Values, float Tolerance)
	{
		const float SafeTolerance = FMath::IsFinite(Tolerance) ? FMath::Max(0.f, Tolerance) : 0.f;
		TArray<bool> Marked;
		Marked.Init(false, Values.Num());
		for (int32 A = 0; A < Values.Num(); ++A)
		{
			for (int32 B = A + 1; B < Values.Num(); ++B)
			{
				if (Values[A].Equals(Values[B], SafeTolerance)) { Marked[A] = Marked[B] = true; }
			}
		}
		TArray<int32> Indices;
		for (int32 Index = 0; Index < Values.Num(); ++Index) { if (Marked[Index]) { Indices.Add(Index); } }
		return Indices;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSortFindDuplicatesReferenceTest,
	"XTools.Sort.Hotspots.FindDuplicatesReference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSortFindDuplicatesReferenceTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(7031);
	TArray<FVector> Values;
	for (int32 Index = 0; Index < 4096; ++Index)
	{
		Values.Add(FVector(Random.FRandRange(-100, 100), Random.FRandRange(-100, 100), Random.FRandRange(-100, 100)));
	}
	Values.Append({FVector(1000, 0, 0), FVector(1000.75, 0, 0), FVector(1001.5, 0, 0),
		FVector::ZeroVector, FVector(-0.0, 0, 0), FVector(1.e300), FVector(1.e300),
		FVector(2251799813685247.0), FVector(2251799813685248.0)});
	const FVector FirstValue = Values[0];
	Values.Add(FirstValue);
	for (bool bNonFinite : {false, true})
	{
		if (bNonFinite)
		{
			Values.Add(FVector(std::numeric_limits<double>::infinity(), 0, 0));
			FVector NonFinite = FVector::ZeroVector;
			NonFinite.X = std::numeric_limits<double>::quiet_NaN();
			Values.Add(NonFinite);
		}
		for (float Tolerance : {0.f, -1.f, 1.f, 1.e-20f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
		{
			const TArray<int32> Expected = ReferenceDuplicates(Values, Tolerance);
			TArray<int32> Actual;
			TArray<FVector> Duplicates;
			USortLibrary::FindDuplicateVectors(Values, Actual, Duplicates, Tolerance);
			TestTrue(TEXT("所有参与重复的元素按原下标输出"), Actual == Expected);
			TestEqual(TEXT("值与下标数量一致"), Duplicates.Num(), Actual.Num());
			for (int32 Index = 0; Index < Actual.Num(); ++Index)
			{
				TestTrue(TEXT("输出保留输入值"), Duplicates[Index] == Values[Actual[Index]]);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSortFindDuplicatesAliasTest,
	"XTools.Sort.Hotspots.FindDuplicatesAlias", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSortFindDuplicatesAliasTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Values = {FVector(0, 0, 0), FVector(0.75, 0, 0), FVector(1.5, 0, 0), FVector(10)};
	TArray<int32> Indices;
	USortLibrary::FindDuplicateVectors(Values, Indices, Values, 1.f);
	TestTrue(TEXT("非传递容差链三个元素均为重复，且支持输入输出别名"), Indices == TArray<int32>({0, 1, 2}));
	TestEqual(TEXT("别名输出保留三个值"), Values.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSortHotspotScaleTest,
	"XTools.Sort.Hotspots.Scale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSortHotspotScaleTest::RunTest(const FString& Parameters)
{
	for (int32 Count : {100, 1000, 10000})
	{
		TArray<FVector> Values;
		for (int32 Index = 0; Index < Count; ++Index) { Values.Add(FVector(Index * 10.0, Index % 17, 0)); }
		TArray<int32> Indices;
		TArray<FVector> Duplicates;
		double Started = FPlatformTime::Seconds();
		USortLibrary::FindDuplicateVectors(Values, Indices, Duplicates, 0.01f);
		AddInfo(FString::Printf(TEXT("FindDuplicatesScale: %d unique, %.3f ms"), Count, (FPlatformTime::Seconds() - Started) * 1000));
		TestEqual(TEXT("无重复"), Indices.Num(), 0);
		for (FVector& Value : Values) { Value = FVector(1); }
		Started = FPlatformTime::Seconds();
		USortLibrary::FindDuplicateVectors(Values, Indices, Duplicates, 0.01f);
		AddInfo(FString::Printf(TEXT("FindDuplicatesScale: %d identical, %.3f ms"), Count, (FPlatformTime::Seconds() - Started) * 1000));
		TestEqual(TEXT("密集重复全部标记"), Indices.Num(), Count);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSortStringHotspotTest,
	"XTools.Sort.Hotspots.StringReferenceAndScale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSortStringHotspotTest::RunTest(const FString& Parameters)
{
	TArray<FString> Values = {TEXT(""), TEXT(""), TEXT("Alpha"), TEXT("alpha"), TEXT("ALPHA"),
		TEXT("中文"), TEXT("中文"), TEXT("Ä"), TEXT("ä"), TEXT("Σ"), TEXT("σ"), TEXT("ς"), TEXT("İ"), TEXT("i"), TEXT("ı"), TEXT("I"), TEXT("ß"), TEXT("SS")};
	for (int32 Index = 0; Index < 10000; ++Index) { Values.Add(FString::Printf(TEXT("Asset_%05d_%s"), Index, *FString::ChrN(128, TEXT('A')))); }
	for (bool bSensitive : {false, true})
	{
		TArray<FString> Expected;
		for (const FString& Value : Values)
		{
			if (!Expected.ContainsByPredicate([&](const FString& Existing) { return Value.Equals(Existing, bSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase); })) { Expected.Add(Value); }
		}
		TArray<FString> Actual;
		const double Started = FPlatformTime::Seconds();
		USortLibrary::RemoveDuplicateStrings(Values, Actual, bSensitive);
		AddInfo(FString::Printf(TEXT("StringScale: %d strings, sensitive=%d, %.3f ms"), Values.Num(), bSensitive, (FPlatformTime::Seconds() - Started) * 1000));
		TestEqual(TEXT("字符串数量与逐项比较一致"), Actual.Num(), Expected.Num());
		for (int32 Index = 0; Index < FMath::Min(Actual.Num(), Expected.Num()); ++Index) { TestTrue(TEXT("首现文本大小写不变"), Actual[Index].Equals(Expected[Index], ESearchCase::CaseSensitive)); }
	}
	return true;
}

#endif
