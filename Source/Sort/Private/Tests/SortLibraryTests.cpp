/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "SortLibrary.h"
#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Internationalization/Text.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
	/** 构造位于指定位置的测试Actor：NewObject + 根组件定位，无需真实World，测试结束后由GC回收 */
	AActor* MakeTestActorAt(const FVector& Location)
	{
		AActor* Actor = NewObject<AActor>(GetTransientPackage());
		USceneComponent* Root = NewObject<USceneComponent>(Actor);
		Actor->SetRootComponent(Root);
		Root->SetWorldLocation(Location);
		return Actor;
	}
}

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
	FSortLibrary_AngleAndDistanceTieStableAscending,
	"XTools.Sort.Library.AngleAndDistanceTieStableAscending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_AngleAndDistanceTieStableAscending::RunTest(const FString& Parameters)
{
	// 几何条件：中心=原点，参考方向=+X，权重全0（默认只按夹角评分），MaxAngle/MaxDistance=0不限制。
	// +X射线上三个Actor的夹角恒为acos(1.0)=0度，评分逐位相等（不依赖浮点误差）；
	// +Y上的Actor夹角为90度，评分归一化后恰为1.0。
	AActor* YActor = MakeTestActorAt(FVector(0.0f, 100.0f, 0.0f));   // 输入下标0，评分1.0
	AActor* XFar = MakeTestActorAt(FVector(300.0f, 0.0f, 0.0f));    // 输入下标1，评分0.0
	AActor* XNear = MakeTestActorAt(FVector(100.0f, 0.0f, 0.0f));   // 输入下标2，评分0.0
	AActor* XMid = MakeTestActorAt(FVector(200.0f, 0.0f, 0.0f));    // 输入下标3，评分0.0
	const TArray<AActor*> Actors = { YActor, XFar, XNear, XMid };

	TArray<AActor*> SortedActors;
	TArray<int32> OriginalIndices;
	TArray<float> SortedAngles;
	TArray<float> SortedDistances;
	USortLibrary::SortActorsByAngleAndDistance(Actors, FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f),
		0.0f, 0.0f, 0.0f, 0.0f, SortedActors, OriginalIndices, SortedAngles, SortedDistances, true, false);

	TestEqual(TEXT("升序应保留全部四个Actor"), SortedActors.Num(), 4);
	TestTrue(TEXT("升序：等分三项应保持输入顺序（下标1,2,3），夹角90度项（下标0）最后"),
		OriginalIndices == TArray<int32>({1, 2, 3, 0}));
	TestTrue(TEXT("升序首个输出应是输入第2个Actor（XFar）"), SortedActors[0] == XFar);
	TestTrue(TEXT("升序末位输出应是夹角90度的Actor"), SortedActors.Last() == YActor);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSortLibrary_AngleAndDistanceTieStableDescending,
	"XTools.Sort.Library.AngleAndDistanceTieStableDescending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_AngleAndDistanceTieStableDescending::RunTest(const FString& Parameters)
{
	// 几何条件与升序用例相同：+X射线三项评分逐位相等（0.0），+Y一项评分1.0。
	// 输入顺序刻意打乱为 [XFar, XNear, XMid, YActor]（下标0,1,2,3）。
	AActor* XFar = MakeTestActorAt(FVector(300.0f, 0.0f, 0.0f));    // 输入下标0，评分0.0
	AActor* XNear = MakeTestActorAt(FVector(100.0f, 0.0f, 0.0f));   // 输入下标1，评分0.0
	AActor* XMid = MakeTestActorAt(FVector(200.0f, 0.0f, 0.0f));    // 输入下标2，评分0.0
	AActor* YActor = MakeTestActorAt(FVector(0.0f, 100.0f, 0.0f));  // 输入下标3，评分1.0
	const TArray<AActor*> Actors = { XFar, XNear, XMid, YActor };

	TArray<AActor*> SortedActors;
	TArray<int32> OriginalIndices;
	TArray<float> SortedAngles;
	TArray<float> SortedDistances;
	USortLibrary::SortActorsByAngleAndDistance(Actors, FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f),
		0.0f, 0.0f, 0.0f, 0.0f, SortedActors, OriginalIndices, SortedAngles, SortedDistances, false, false);

	TestEqual(TEXT("降序应保留全部四个Actor"), SortedActors.Num(), 4);
	TestTrue(TEXT("降序：评分1.0项（下标3）最先，等分三项仍按输入顺序（下标0,1,2）而非倒序"),
		OriginalIndices == TArray<int32>({3, 0, 1, 2}));
	TestTrue(TEXT("降序首个输出应是评分最高的Actor"), SortedActors[0] == YActor);
	TestTrue(TEXT("降序等分组首个应是输入第1个Actor（XFar）"), SortedActors[1] == XFar);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSortLibrary_AngleAndDistanceSkipsInvalidActors,
	"XTools.Sort.Library.AngleAndDistanceSkipsInvalidActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_AngleAndDistanceSkipsInvalidActors::RunTest(const FString& Parameters)
{
	// 几何条件：+X射线上三个有效Actor评分逐位相等（0.0）。
	// 输入混入 nullptr（下标1）与 MarkAsGarbage 标记的无效Actor（下标3）。
	AActor* XNear = MakeTestActorAt(FVector(100.0f, 0.0f, 0.0f));       // 输入下标0
	AActor* XMid = MakeTestActorAt(FVector(200.0f, 0.0f, 0.0f));       // 输入下标2
	AActor* XFar = MakeTestActorAt(FVector(300.0f, 0.0f, 0.0f));       // 输入下标4
	AActor* GarbageActor = MakeTestActorAt(FVector(400.0f, 0.0f, 0.0f));
	GarbageActor->MarkAsGarbage();
	TestFalse(TEXT("前置条件：标记垃圾后应视为无效Actor"), IsValid(GarbageActor));

	const TArray<AActor*> Actors = { XNear, nullptr, XMid, GarbageActor, XFar };

	TArray<AActor*> SortedActors;
	TArray<int32> OriginalIndices;
	TArray<float> SortedAngles;
	TArray<float> SortedDistances;
	USortLibrary::SortActorsByAngleAndDistance(Actors, FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f),
		0.0f, 0.0f, 0.0f, 0.0f, SortedActors, OriginalIndices, SortedAngles, SortedDistances, true, false);

	TestEqual(TEXT("无效与空Actor应被忽略，仅保留三个有效项"), SortedActors.Num(), 3);
	TestTrue(TEXT("OriginalIndices应仍对应原输入下标0,2,4且保持输入顺序"),
		OriginalIndices == TArray<int32>({0, 2, 4}));
	TestTrue(TEXT("输出不应包含nullptr或无效Actor"),
		SortedActors[0] == XNear && SortedActors[1] == XMid && SortedActors[2] == XFar);

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

// ---------------------------------------------------------------------------
// 自然排序黄金测试（Schwartzian 预解析优化后的语义回归）：
// 覆盖数字段识别、前导零、末尾长度决胜、Primary 级别大小写折叠、
// 段类型不匹配（数字侧视为空文本段）、相同键的 OriginalIndex 平局规则。
// 预期顺序中仅依赖 ICU 的跨文化稳定行为（空串最前、拉丁字母序、Primary 忽略大小写），
// 不依赖特定文化的 CJK 排序（中文组见 NaturalSortChinesePrefix 的动态方向断言）。
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSortLibrary_NaturalSortGoldenOrder,
	"XTools.Sort.Library.NaturalSortGoldenOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_NaturalSortGoldenOrder::RunTest(const FString& Parameters)
{
	// 输入下标：File10=0 File2=1 File1=2 File20=3 File007=4 File07=5 File7=6 ""=7 0=8 1a=9 a1=10
	//           Item10=11 item2=12 k1=13 K1=14 k01=15
	const TArray<FString> Input = {
		TEXT("File10"), TEXT("File2"), TEXT("File1"), TEXT("File20"),
		TEXT("File007"), TEXT("File07"), TEXT("File7"),
		TEXT(""), TEXT("0"), TEXT("1a"), TEXT("a1"),
		TEXT("Item10"), TEXT("item2"), TEXT("k1"), TEXT("K1"), TEXT("k01")
	};

	// 升序黄金序：
	// ""(空串) < "0"(全零数字段) < "1a"(数字段1) < "a1"(文本段a) < File1 < File2 < File7
	// < File07 < File007(前导零按总长度决胜) < File10 < File20(有效数字长度)
	// < item2 < Item10(文本段 Primary 相等后按数字段：2 < 10)
	// < k1 < K1(Primary 级别忽略大小写，与 k1 同分，按原始索引平局) < k01(长度决胜)
	const TArray<FString> ExpectedAscending = {
		TEXT(""), TEXT("0"), TEXT("1a"), TEXT("a1"),
		TEXT("File1"), TEXT("File2"), TEXT("File7"), TEXT("File07"), TEXT("File007"), TEXT("File10"), TEXT("File20"),
		TEXT("item2"), TEXT("Item10"),
		TEXT("k1"), TEXT("K1"), TEXT("k01")
	};
	const TArray<int32> ExpectedAscendingIndices = {7, 8, 9, 10, 2, 1, 6, 5, 4, 0, 3, 12, 11, 13, 14, 15};

	TArray<FString> SortedStrings;
	TArray<int32> OriginalIndices;
	USortLibrary::SortStringArray(Input, SortedStrings, OriginalIndices, true);
	TestTrue(TEXT("自然排序升序黄金序应逐元素一致"), SortedStrings == ExpectedAscending);
	TestTrue(TEXT("自然排序升序应输出正确的原始索引"), OriginalIndices == ExpectedAscendingIndices);

	// 降序：整体反序；同分对（k1/K1）在降序分支的反向比较器下按索引倒序（既有规则，与升序分支不同）
	const TArray<FString> ExpectedDescending = {
		TEXT("k01"), TEXT("K1"), TEXT("k1"),
		TEXT("Item10"), TEXT("item2"),
		TEXT("File20"), TEXT("File10"), TEXT("File007"), TEXT("File07"), TEXT("File7"), TEXT("File2"), TEXT("File1"),
		TEXT("a1"), TEXT("1a"), TEXT("0"), TEXT("")
	};
	const TArray<int32> ExpectedDescendingIndices = {15, 14, 13, 11, 12, 3, 0, 4, 5, 6, 1, 2, 10, 9, 8, 7};

	USortLibrary::SortStringArray(Input, SortedStrings, OriginalIndices, false);
	TestTrue(TEXT("自然排序降序黄金序应逐元素一致"), SortedStrings == ExpectedDescending);
	TestTrue(TEXT("自然排序降序应输出正确的原始索引"), OriginalIndices == ExpectedDescendingIndices);

	// FName 数组回归：自然数字顺序 + 前导零
	TArray<FName> SortedNames;
	USortLibrary::SortNameArray(
		{FName(TEXT("Item10")), FName(TEXT("Item2")), FName(TEXT("Item07")), FName(TEXT("Item7")), FName(TEXT("Item1"))},
		SortedNames, OriginalIndices, true);
	TestTrue(TEXT("名称自然排序应处理数字与前导零"),
		SortedNames == TArray<FName>({FName(TEXT("Item1")), FName(TEXT("Item2")), FName(TEXT("Item7")), FName(TEXT("Item07")), FName(TEXT("Item10"))})
		&& OriginalIndices == TArray<int32>({4, 1, 3, 2, 0}));

	return true;
}

// ---------------------------------------------------------------------------
// 中文前缀自然排序：数字段语义与文化无关可硬断言；
// CJK 文本段的相对顺序不硬编码（依赖当前文化），以 FText::CompareTo 的实际方向作为预期。
// 这正是"文本段走当前文化的文化敏感比较"这一契约的可执行验证。
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSortLibrary_NaturalSortChinesePrefix,
	"XTools.Sort.Library.NaturalSortChinesePrefix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_NaturalSortChinesePrefix::RunTest(const FString& Parameters)
{
	// 输入下标：乙1=0 甲1=1 甲2=2 甲10=3 甲07=4 甲007=5
	const TArray<FString> Input = {
		TEXT("乙1"), TEXT("甲1"), TEXT("甲2"), TEXT("甲10"), TEXT("甲07"), TEXT("甲007")
	};

	// 甲组内部顺序（数字段语义，文化无关）：甲1 < 甲2 < 甲7 系列 < 甲10，前导零按总长度决胜
	const TArray<FString> JiaGroup = {TEXT("甲1"), TEXT("甲2"), TEXT("甲07"), TEXT("甲007"), TEXT("甲10")};

	// 甲/乙的相对方向取自当前文化的 FText Primary 比较（与排序器使用的比较一致）
	const int32 JiaVersusYi = FText::FromString(TEXT("甲")).CompareTo(FText::FromString(TEXT("乙")), ETextComparisonLevel::Primary);

	TArray<FString> SortedStrings;
	TArray<int32> OriginalIndices;
	USortLibrary::SortStringArray(Input, SortedStrings, OriginalIndices, true);

	if (JiaVersusYi < 0)
	{
		// 甲 < 乙：乙1 排最后，输入下标 0
		TArray<FString> Expected = JiaGroup;
		Expected.Add(TEXT("乙1"));
		TestTrue(TEXT("中文前缀排序（甲<乙）应保持自然数字顺序"), SortedStrings == Expected);
		TestTrue(TEXT("中文前缀排序（甲<乙）应输出正确的原始索引"),
			OriginalIndices == TArray<int32>({1, 2, 4, 5, 3, 0}));
	}
	else
	{
		// 乙 < 甲（或相等时按长度/索引）：乙1 排最前
		TArray<FString> Expected = {TEXT("乙1")};
		Expected.Append(JiaGroup);
		TestTrue(TEXT("中文前缀排序（乙<甲）应保持自然数字顺序"), SortedStrings == Expected);
		TestTrue(TEXT("中文前缀排序（乙<甲）应输出正确的原始索引"),
			OriginalIndices == TArray<int32>({0, 1, 2, 4, 5, 3}));
	}

	// 数字段断言（与文化方向无关）：甲2 必须排在甲10 之前，甲1 必须在乙1 之前（同为各文本组首元素）
	const int32 Jia2Pos = SortedStrings.IndexOfByKey(TEXT("甲2"));
	const int32 Jia10Pos = SortedStrings.IndexOfByKey(TEXT("甲10"));
	TestTrue(TEXT("中文前缀数字段应按数值排序（甲2 < 甲10）"), Jia2Pos < Jia10Pos);

	return true;
}

// ---------------------------------------------------------------------------
// 属性排序路径（字符串类属性）：验证预提取优化后的自然排序语义。
// 使用 AActor::Tags（TArray<FName>）的反射属性直接调用通用属性排序，
// 覆盖 FName 属性的取值、ToString 与自然键排序的完整链路。
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSortLibrary_PropertySortNaturalString,
	"XTools.Sort.Library.PropertySortNaturalString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSortLibrary_PropertySortNaturalString::RunTest(const FString& Parameters)
{
	FArrayProperty* TagsProperty = CastField<FArrayProperty>(AActor::StaticClass()->FindPropertyByName(TEXT("Tags")));
	TestNotNull(TEXT("应找到 AActor::Tags 的反射属性"), TagsProperty);
	if (!TagsProperty)
	{
		return false;
	}

	// 输入下标：Item10=0 Item2=1 Item1=2 Item07=3 Item7=4
	TArray<FName> Tags = {
		FName(TEXT("Item10")), FName(TEXT("Item2")), FName(TEXT("Item1")),
		FName(TEXT("Item07")), FName(TEXT("Item7"))
	};

	TArray<int32> OriginalIndices;
	USortLibrary::GenericSortArrayByProperty(&Tags, TagsProperty, NAME_None, true, OriginalIndices);
	TestTrue(TEXT("FName 属性升序应使用自然数字顺序（含前导零长度决胜）"),
		Tags == TArray<FName>({FName(TEXT("Item1")), FName(TEXT("Item2")), FName(TEXT("Item7")), FName(TEXT("Item07")), FName(TEXT("Item10"))}));
	TestTrue(TEXT("FName 属性升序应输出正确的原始索引"),
		OriginalIndices == TArray<int32>({2, 1, 4, 3, 0}));

	// 降序回归
	TArray<FName> DescendingTags = {
		FName(TEXT("Item10")), FName(TEXT("Item2")), FName(TEXT("Item1")),
		FName(TEXT("Item07")), FName(TEXT("Item7"))
	};
	USortLibrary::GenericSortArrayByProperty(&DescendingTags, TagsProperty, NAME_None, false, OriginalIndices);
	TestTrue(TEXT("FName 属性降序应为升序的反序"),
		DescendingTags == TArray<FName>({FName(TEXT("Item10")), FName(TEXT("Item07")), FName(TEXT("Item7")), FName(TEXT("Item2")), FName(TEXT("Item1"))}));

	// FString/FText 属性分支与 FName 分支共享同一排序键与回填逻辑，
	// 仅属性提取调用不同（GetPropertyValue / ToString），由 FName 路径覆盖全链路。

	return true;
}

#endif
