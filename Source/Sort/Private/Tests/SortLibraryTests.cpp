/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "SortLibrary.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSortLibrary_SortsBasicValues,
	"XTools.Sort.Library.SortsBasicValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_SortsBasicValues::RunTest(const FString& Parameters)
{
	TArray<int32> SortedIntegers;
	TArray<int32> OriginalIndices;
	USortLibrary::SortIntegerArray({3, 1, 2}, SortedIntegers, OriginalIndices);
	TestTrue(TEXT("整数升序排序应保持原始索引"),
		SortedIntegers == TArray<int32>({1, 2, 3}) && OriginalIndices == TArray<int32>({1, 2, 0}));

	TArray<float> SortedFloats;
	USortLibrary::SortFloatArray({1.0f, 3.0f, 2.0f}, SortedFloats, OriginalIndices, false);
	TestTrue(TEXT("浮点降序排序应正确"), SortedFloats == TArray<float>({3.0f, 2.0f, 1.0f}));

	TArray<FString> SortedStrings;
	USortLibrary::SortStringArray({TEXT("Item10"), TEXT("Item2"), TEXT("Item1")}, SortedStrings, OriginalIndices);
	TestTrue(TEXT("字符串排序应使用自然数字顺序"),
		SortedStrings == TArray<FString>({TEXT("Item1"), TEXT("Item2"), TEXT("Item10")}));

	TArray<FName> SortedNames;
	USortLibrary::SortNameArray({FName(TEXT("Item10")), FName(TEXT("Item2")), FName(TEXT("Item1"))}, SortedNames, OriginalIndices);
	TestTrue(TEXT("名称排序应使用自然数字顺序"),
		SortedNames == TArray<FName>({FName(TEXT("Item1")), FName(TEXT("Item2")), FName(TEXT("Item10"))}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSortLibrary_SortsAndSlicesVectors,
	"XTools.Sort.Library.SortsAndSlicesVectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_SortsAndSlicesVectors::RunTest(const FString& Parameters)
{
	const TArray<FVector> Vectors = {
		FVector(3.0f, 0.0f, 0.0f),
		FVector(1.0f, 0.0f, 0.0f),
		FVector(2.0f, 0.0f, 0.0f)
	};
	TArray<FVector> SortedVectors;
	TArray<int32> OriginalIndices;
	TArray<float> Values;
	USortLibrary::SortVectorsByProjection(Vectors, FVector::ForwardVector, SortedVectors, OriginalIndices, Values);
	TestTrue(TEXT("向量投影排序应按X轴升序"),
		SortedVectors[0].X == 1.0f && Values == TArray<float>({1.0f, 2.0f, 3.0f}));

	USortLibrary::SortVectorsByLength(Vectors, SortedVectors, OriginalIndices, Values, false);
	TestTrue(TEXT("向量长度排序应支持降序"), SortedVectors[0].X == 3.0f);

	USortLibrary::SortVectorsByAxis(Vectors, ECoordinateAxis::X, SortedVectors, OriginalIndices, Values);
	TestTrue(TEXT("向量轴排序应输出轴值"), Values == TArray<float>({1.0f, 2.0f, 3.0f}));

	USortLibrary::SortVectorsUnified(Vectors, EVectorSortMode::ByAxis, FVector::ZeroVector, ECoordinateAxis::X, SortedVectors, OriginalIndices);
	TestTrue(TEXT("统一向量排序应转发轴排序"), SortedVectors[0].X == 1.0f && OriginalIndices[0] == 1);

	TArray<int32> IntegerSlice;
	USortLibrary::SliceIntegerArrayByIndices({0, 1, 2, 3}, 1, 2, IntegerSlice);
	TestTrue(TEXT("整数索引切片应包含边界"), IntegerSlice == TArray<int32>({1, 2}));

	TArray<float> FloatSlice;
	TArray<int32> SliceIndices;
	USortLibrary::SliceFloatArrayByValue({1.0f, 2.0f, 3.0f}, 1.5f, 3.0f, FloatSlice, SliceIndices);
	TestTrue(TEXT("浮点值切片应返回值和原始索引"),
		FloatSlice == TArray<float>({2.0f, 3.0f}) && SliceIndices == TArray<int32>({1, 2}));

	TArray<FVector> VectorSlice;
	USortLibrary::SliceVectorArrayByLength(Vectors, 1.5f, 2.5f, VectorSlice, SliceIndices, Values);
	TestTrue(TEXT("向量长度切片应保留范围内元素"),
		VectorSlice.Num() == 1 && VectorSlice[0].X == 2.0f && SliceIndices[0] == 2 && Values[0] == 2.0f);

	USortLibrary::SliceVectorArrayByComponent(Vectors, ECoordinateAxis::X, 1.0f, 2.0f, VectorSlice, SliceIndices, Values);
	TestTrue(TEXT("向量分量切片应包含边界"), VectorSlice.Num() == 2 && Values == TArray<float>({1.0f, 2.0f}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSortLibrary_ReversesAndDeduplicatesValues,
	"XTools.Sort.Library.ReversesAndDeduplicatesValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_ReversesAndDeduplicatesValues::RunTest(const FString& Parameters)
{
	TArray<int32> ReversedIntegers;
	USortLibrary::ReverseIntegerArray({1, 2, 3}, ReversedIntegers);
	TestTrue(TEXT("整数反转应保留所有元素"), ReversedIntegers == TArray<int32>({3, 2, 1}));

	TArray<FVector> ReversedVectors;
	USortLibrary::ReverseVectorArray({FVector(1.0f, 0.0f, 0.0f), FVector(2.0f, 0.0f, 0.0f)}, ReversedVectors);
	TestTrue(TEXT("向量反转应交换顺序"), ReversedVectors[0].X == 2.0f);

	TArray<float> UniqueFloats;
	USortLibrary::RemoveDuplicateFloats({1.0f, 1.001f, 2.0f}, UniqueFloats, 0.01f);
	TestTrue(TEXT("浮点去重应尊重容差"), UniqueFloats == TArray<float>({1.0f, 2.0f}));

	TArray<int32> UniqueIntegers;
	USortLibrary::RemoveDuplicateIntegers({1, 1, 2}, UniqueIntegers);
	TestEqual(TEXT("整数去重应移除重复项"), UniqueIntegers.Num(), 2);

	TArray<FString> UniqueStrings;
	USortLibrary::RemoveDuplicateStrings({TEXT("A"), TEXT("a"), TEXT("B")}, UniqueStrings, false);
	TestEqual(TEXT("忽略大小写字符串去重应移除重复项"), UniqueStrings.Num(), 2);

	const TArray<FVector> InputVectors = {
		FVector::ZeroVector,
		FVector(0.001f, 0.0f, 0.0f),
		FVector(1.0f, 0.0f, 0.0f)
	};
	TArray<FVector> UniqueVectors;
	USortLibrary::RemoveDuplicateVectors(InputVectors, UniqueVectors, 0.01f);
	TestEqual(TEXT("向量去重应尊重容差"), UniqueVectors.Num(), 2);

	TArray<int32> DuplicateIndices;
	TArray<FVector> DuplicateValues;
	USortLibrary::FindDuplicateVectors(InputVectors, DuplicateIndices, DuplicateValues, 0.01f);
	TestTrue(TEXT("重复向量检测应报告重复组的全部成员"),
		DuplicateIndices == TArray<int32>({0, 1}) && DuplicateValues.Num() == 2);

	return true;
}

#endif
