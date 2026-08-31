/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationSamplingLibrary.h"
#include "PointSamplingLibrary.h"
#include "Core/SamplingCache.h"
#include "Misc/AutomationTest.h"

namespace
{
	void TestFinitePoints(FAutomationTestBase& Test, const FString& Name, const TArray<FVector>& Points)
	{
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Test.TestTrue(
				*FString::Printf(TEXT("%s 的点%d坐标应为有限数"), *Name, Index),
				FMath::IsFinite(Points[Index].X) &&
				FMath::IsFinite(Points[Index].Y) &&
				FMath::IsFinite(Points[Index].Z));
		}
	}

	void TestPointCount(FAutomationTestBase& Test, const FString& Name, const TArray<FVector>& Points, int32 ExpectedCount)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s 应返回目标点数"), *Name), Points.Num(), ExpectedCount);
		TestFinitePoints(Test, Name, Points);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPointSamplingCache_UpdatingExistingEntryDoesNotEvict,
	"XTools.PointSampling.Cache.UpdatingExistingEntryDoesNotEvict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPointSamplingCache_UpdatingExistingEntryDoesNotEvict::RunTest(const FString& Parameters)
{
	constexpr int32 CacheCapacity = 50;
	FSamplingCache& Cache = FSamplingCache::Get();
	Cache.ClearCache();

	TArray<FPoissonCacheKey> Keys;
	Keys.Reserve(CacheCapacity);
	for (int32 Index = 0; Index < CacheCapacity; ++Index)
	{
		FPoissonCacheKey Key;
		Key.TargetPointCount = Index + 1;
		Keys.Add(Key);
		Cache.Store(Key, {FVector(static_cast<double>(Index), 0.0, 0.0)});
	}

	const FPoissonCacheKey& UpdatedKey = Keys.Last();
	const FVector UpdatedValue(999.0, 0.0, 0.0);
	Cache.Store(UpdatedKey, {UpdatedValue});

	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		const TOptional<TArray<FVector>> CachedPoints = Cache.GetCached(Keys[Index]);
		TestTrue(*FString::Printf(TEXT("更新已有键后缓存项%d不应被淘汰"), Index), CachedPoints.IsSet());
		if (CachedPoints.IsSet())
		{
			const FVector ExpectedValue = Index == Keys.Num() - 1
				? UpdatedValue
				: FVector(static_cast<double>(Index), 0.0, 0.0);
			TestEqual(*FString::Printf(TEXT("缓存项%d应保留最新值"), Index), CachedPoints.GetValue()[0], ExpectedValue);
		}
	}

	Cache.ClearCache();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPointSamplingCache_EquivalentHalfTurnQuaternionsShareEntry,
	"XTools.PointSampling.Cache.EquivalentHalfTurnQuaternionsShareEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPointSamplingCache_EquivalentHalfTurnQuaternionsShareEntry::RunTest(const FString& Parameters)
{
	FSamplingCache& Cache = FSamplingCache::Get();
	Cache.ClearCache();

	FPoissonCacheKey PositiveKey;
	PositiveKey.Rotation = FQuat(1.0, 0.0, 0.0, 0.0);
	FPoissonCacheKey NegativeKey = PositiveKey;
	NegativeKey.Rotation = FQuat(-1.0, 0.0, 0.0, 0.0);

	TestTrue(TEXT("符号相反的半周四元数应视为同一缓存键"), PositiveKey == NegativeKey);
	TestEqual(TEXT("等价半周四元数应生成相同哈希"), GetTypeHash(PositiveKey), GetTypeHash(NegativeKey));

	const FVector CachedValue(123.0, 456.0, 789.0);
	Cache.Store(PositiveKey, {CachedValue});
	const TOptional<TArray<FVector>> CachedPoints = Cache.GetCached(NegativeKey);
	TestTrue(TEXT("等价半周四元数应命中同一缓存项"), CachedPoints.IsSet());
	if (CachedPoints.IsSet())
	{
		TestEqual(TEXT("等价缓存键应返回原缓存值"), CachedPoints.GetValue()[0], CachedValue);
	}

	Cache.ClearCache();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_PrimitiveAlgorithms,
	"XTools.PointSampling.Formation.PrimitiveAlgorithms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_PrimitiveAlgorithms::RunTest(const FString& Parameters)
{
	constexpr EPoissonCoordinateSpace RawSpace = EPoissonCoordinateSpace::Raw;

	const TArray<FVector> SolidRectangle = UFormationSamplingLibrary::GenerateSolidRectangle(
		6, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 2, 3, 1.0f, RawSpace);
	TestPointCount(*this, TEXT("实心矩形"), SolidRectangle, 6);

	const TArray<FVector> HollowRectangle = UFormationSamplingLibrary::GenerateHollowRectangle(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 0, 0, 1.0f, RawSpace);
	TestPointCount(*this, TEXT("空心矩形"), HollowRectangle, 8);

	const TArray<FVector> SpiralRectangle = UFormationSamplingLibrary::GenerateSpiralRectangle(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 2.0f, 1.0f, RawSpace);
	TestPointCount(*this, TEXT("螺旋矩形"), SpiralRectangle, 8);
	if (SpiralRectangle.Num() == 8)
	{
		TestTrue(TEXT("螺旋矩形应从中心向外扩展"),
			SpiralRectangle[0].IsNearlyZero() &&
			SpiralRectangle.Last().SizeSquared() > SpiralRectangle[1].SizeSquared());
	}

	const TArray<FVector> SolidTriangle = UFormationSamplingLibrary::GenerateSolidTriangle(
		9, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, false, RawSpace);
	TestPointCount(*this, TEXT("实心三角形"), SolidTriangle, 9);

	const TArray<FVector> HollowTriangle = UFormationSamplingLibrary::GenerateHollowTriangle(
		9, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, false, RawSpace);
	TestPointCount(*this, TEXT("空心三角形"), HollowTriangle, 9);
	if (HollowTriangle.Num() == 9)
	{
		TestTrue(TEXT("空心三角形闭合采样不应重复首尾顶点"),
			!HollowTriangle[0].Equals(HollowTriangle.Last(), UE_KINDA_SMALL_NUMBER));
	}

	const TArray<FTransform> Circle = UFormationSamplingLibrary::GenerateCircle(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, false, false,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace);
	TestEqual(TEXT("空心圆应返回目标点数"), Circle.Num(), 12);
	for (int32 Index = 0; Index < Circle.Num(); ++Index)
	{
		const FVector Direction = Circle[Index].GetLocation().GetSafeNormal();
		TestTrue(*FString::Printf(TEXT("空心圆点%d应位于圆周"), Index),
			FMath::IsNearlyEqual(Circle[Index].GetLocation().Size(), 100.0f, UE_KINDA_SMALL_NUMBER));
		TestTrue(*FString::Printf(TEXT("空心圆点%d的变换应朝外"), Index),
			FVector::DotProduct(Circle[Index].GetRotation().RotateVector(FVector::ForwardVector), Direction) > 0.999f);
	}

	UPointSamplingLibrary::ClearPoissonSamplingCache();
	int32 CacheHitsBefore = 0;
	int32 CacheMissesBefore = 0;
	UPointSamplingLibrary::GetPoissonSamplingCacheStats(CacheHitsBefore, CacheMissesBefore);
	const TArray<FTransform> CachedCircle = UFormationSamplingLibrary::GenerateCircle(
		12, FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator, 100.0f, false, false,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, EPoissonCoordinateSpace::World,
		0.25f, 123, true);
	int32 CacheHitsAfterFirstCall = 0;
	int32 CacheMissesAfterFirstCall = 0;
	UPointSamplingLibrary::GetPoissonSamplingCacheStats(CacheHitsAfterFirstCall, CacheMissesAfterFirstCall);
	const TArray<FTransform> CachedCircleAtNewCenter = UFormationSamplingLibrary::GenerateCircle(
		12, FVector(300.0f, 0.0f, 0.0f), FRotator::ZeroRotator, 100.0f, false, false,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, EPoissonCoordinateSpace::World,
		0.25f, 123, true);
	int32 CacheHitsAfterSecondCall = 0;
	int32 CacheMissesAfterSecondCall = 0;
	UPointSamplingLibrary::GetPoissonSamplingCacheStats(CacheHitsAfterSecondCall, CacheMissesAfterSecondCall);
	TestEqual(TEXT("首次启用圆形缓存应产生一次未命中"), CacheMissesAfterFirstCall, CacheMissesBefore + 1);
	TestEqual(TEXT("相同圆形参数应命中局部几何缓存"), CacheHitsAfterSecondCall, CacheHitsAfterFirstCall + 1);
	TestEqual(TEXT("命中缓存不应增加未命中数"), CacheMissesAfterSecondCall, CacheMissesAfterFirstCall);
	if (CachedCircle.Num() == CachedCircleAtNewCenter.Num() && CachedCircle.Num() > 0)
	{
		TestTrue(TEXT("命中缓存时仍应更新场景位置"),
			CachedCircleAtNewCenter[0].GetLocation().Equals(CachedCircle[0].GetLocation() + FVector(200.0f, 0.0f, 0.0f), UE_KINDA_SMALL_NUMBER));
	}

	const TArray<FTransform> UncachedCircle = UFormationSamplingLibrary::GenerateCircle(
		12, FVector(500.0f, 0.0f, 0.0f), FRotator::ZeroRotator, 100.0f, false, false,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, EPoissonCoordinateSpace::World,
		0.25f, 123, false);
	int32 CacheHitsAfterDisabledCall = 0;
	int32 CacheMissesAfterDisabledCall = 0;
	UPointSamplingLibrary::GetPoissonSamplingCacheStats(CacheHitsAfterDisabledCall, CacheMissesAfterDisabledCall);
	TestEqual(TEXT("关闭圆形缓存不应读取缓存"), CacheHitsAfterDisabledCall, CacheHitsAfterSecondCall);
	TestEqual(TEXT("关闭圆形缓存不应写入缓存统计"), CacheMissesAfterDisabledCall, CacheMissesAfterSecondCall);
	TestEqual(TEXT("关闭圆形缓存仍应生成有效结果"), UncachedCircle.Num(), 12);
	if (CachedCircle.Num() == CachedCircleAtNewCenter.Num() && CachedCircle.Num() == UncachedCircle.Num())
	{
		for (int32 Index = 0; Index < CachedCircle.Num(); ++Index)
		{
			const FVector FirstLocalPoint = CachedCircle[Index].GetLocation() - FVector(100.0f, 0.0f, 0.0f);
			const FVector CachedLocalPoint = CachedCircleAtNewCenter[Index].GetLocation() - FVector(300.0f, 0.0f, 0.0f);
			const FVector UncachedLocalPoint = UncachedCircle[Index].GetLocation() - FVector(500.0f, 0.0f, 0.0f);
			TestTrue(*FString::Printf(TEXT("同种子扰动点%d命中缓存后应保持局部几何"), Index),
				CachedLocalPoint.Equals(FirstLocalPoint, UE_KINDA_SMALL_NUMBER));
			TestTrue(*FString::Printf(TEXT("同种子扰动点%d关闭缓存重算后应保持局部几何"), Index),
				UncachedLocalPoint.Equals(FirstLocalPoint, UE_KINDA_SMALL_NUMBER));
		}
	}

	const TArray<FTransform> SolidCircle = UFormationSamplingLibrary::GenerateCircle(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, false, true,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace);
	TestEqual(TEXT("实心圆应严格返回目标点数"), SolidCircle.Num(), 12);

	const TArray<FTransform> SolidSphere = UFormationSamplingLibrary::GenerateCircle(
		24, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace,
			0.0f, 0, true, 0.0f, 0.0f, 2.0f, 1, EPointArrayOrderMode::SphereBottomToTopClockwise);
	TestEqual(TEXT("实心球应严格返回目标点数"), SolidSphere.Num(), 24);
	if (SolidSphere.Num() == 24)
	{
		TestTrue(TEXT("球体按层排序应从下方开始"),
			SolidSphere[0].GetLocation().Z <= SolidSphere.Last().GetLocation().Z);
	}

	const TArray<FTransform> SolidSphereWithDefaultMinimum = UFormationSamplingLibrary::GenerateCircle(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace);
	TestEqual(TEXT("实心球默认最小点数不应突破总点数"), SolidSphereWithDefaultMinimum.Num(), 12);

	const TArray<FTransform> LayerSpacedSolidSphere = UFormationSamplingLibrary::GenerateCircle(
		100, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::Uniform, 50.0f, 0.0f, true, RawSpace,
		0.0f, 0, true, 25.0f, 50.0f, 2.0f, 1, EPointArrayOrderMode::SphereBottomToTopClockwise);
	TestEqual(TEXT("指定实心球层间距应保持总点数"), LayerSpacedSolidSphere.Num(), 100);
	TArray<float> LayerHeights;
	for (const FTransform& Transform : LayerSpacedSolidSphere)
	{
		const FVector Location = Transform.GetLocation();
		TestTrue(TEXT("实心球点位应位于球体边界内"), Location.Size() <= 100.01f);

		bool bKnownLayer = false;
		for (const float Height : LayerHeights)
		{
			bKnownLayer |= FMath::IsNearlyEqual(Location.Z, Height, UE_KINDA_SMALL_NUMBER);
		}
		if (!bKnownLayer)
		{
			LayerHeights.Add(Location.Z);
		}

		if (FMath::IsNearlyZero(Location.Z, UE_KINDA_SMALL_NUMBER))
		{
			const float RingRadius = FVector(Location.X, Location.Y, 0.0f).Size();
			TestTrue(TEXT("实心球中心截面的环间距应可控"),
				FMath::IsNearlyEqual(RingRadius / 25.0f, FMath::RoundToFloat(RingRadius / 25.0f), UE_KINDA_SMALL_NUMBER));
		}
	}
	TestEqual(TEXT("指定50厘米实心球层间距应生成5个纬度层"), LayerHeights.Num(), 5);

	const TArray<FTransform> ClosePackedSphere = UFormationSamplingLibrary::GenerateCircle(
		120, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::ClosePacked, 50.0f, 0.0f, true, RawSpace,
		0.0f, 0, true, 25.0f, 0.0f, 1.0f, 1, EPointArrayOrderMode::SphereBottomToTopCounterClockwise);
	TestTrue(TEXT("紧密堆叠实心球应生成点位"), ClosePackedSphere.Num() > 0);
	for (int32 PointIndex = 1; PointIndex < ClosePackedSphere.Num(); ++PointIndex)
	{
		const FVector Previous = ClosePackedSphere[PointIndex - 1].GetLocation();
		const FVector Current = ClosePackedSphere[PointIndex].GetLocation();
		TestTrue(TEXT("紧密堆叠逆时针排序应由下至上"), Previous.Z <= Current.Z + UE_KINDA_SMALL_NUMBER);
		if (!FMath::IsNearlyEqual(Previous.Z, Current.Z, UE_KINDA_SMALL_NUMBER))
		{
			continue;
		}

		const float PreviousRadius = Previous.Size2D();
		const float CurrentRadius = Current.Size2D();
		TestTrue(TEXT("紧密堆叠同层排序应由内至外"), PreviousRadius <= CurrentRadius + 0.01f);
		if (!FMath::IsNearlyEqual(PreviousRadius, CurrentRadius, 0.01f))
		{
			continue;
		}

		const float PreviousAngle = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Previous.Y, Previous.X)) + 360.0f, 360.0f);
		const float CurrentAngle = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Current.Y, Current.X)) + 360.0f, 360.0f);
		TestTrue(TEXT("紧密堆叠同环排序应为逆时针"), PreviousAngle <= CurrentAngle + 0.01f);
	}
	TArray<float> PackedLayerHeights;
	for (int32 LeftIndex = 0; LeftIndex < ClosePackedSphere.Num(); ++LeftIndex)
	{
		const FVector Left = ClosePackedSphere[LeftIndex].GetLocation();
		TestTrue(TEXT("紧密堆叠实心球点位应位于球体边界内"), Left.Size() <= 100.01f);

		bool bKnownLayer = false;
		for (const float Height : PackedLayerHeights)
		{
			bKnownLayer |= FMath::IsNearlyEqual(Left.Z, Height, UE_KINDA_SMALL_NUMBER);
		}
		if (!bKnownLayer)
		{
			PackedLayerHeights.Add(Left.Z);
		}
	}
	TestTrue(TEXT("紧密堆叠实心球应至少有三层"), PackedLayerHeights.Num() >= 3);
	if (PackedLayerHeights.Num() >= 2)
	{
		int32 BottomLayerCount = 0;
		int32 TopLayerCount = 0;
		for (const FTransform& Point : ClosePackedSphere)
		{
			BottomLayerCount += FMath::IsNearlyEqual(Point.GetLocation().Z, PackedLayerHeights[0], UE_KINDA_SMALL_NUMBER) ? 1 : 0;
			TopLayerCount += FMath::IsNearlyEqual(Point.GetLocation().Z, PackedLayerHeights.Last(), UE_KINDA_SMALL_NUMBER) ? 1 : 0;
		}
		TestEqual(TEXT("紧密堆叠实心球底层应为五点十字结构"), BottomLayerCount, 5);
		TestEqual(TEXT("紧密堆叠实心球顶层应为五点十字结构"), TopLayerCount, 5);
	}

	const TArray<FTransform> DenseLayerClosePackedSphere = UFormationSamplingLibrary::GenerateCircle(
		120, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::ClosePacked, 50.0f, 0.0f, true, RawSpace,
		0.0f, 0, true, 25.0f, 0.0f, 2.0f, 1, EPointArrayOrderMode::SphereBottomToTopCounterClockwise);
	TestTrue(TEXT("提高Z层级密度应增加紧密堆叠实际点数"), DenseLayerClosePackedSphere.Num() > ClosePackedSphere.Num());
	TArray<float> DenseLayerHeights;
	for (const FTransform& Point : DenseLayerClosePackedSphere)
	{
		const FVector Location = Point.GetLocation();
		TestTrue(TEXT("提高Z层级密度后点位应位于球体边界内"), Location.Size() <= 100.01f);
		const float Z = Location.Z;
		if (DenseLayerHeights.IsEmpty() || !FMath::IsNearlyEqual(DenseLayerHeights.Last(), Z, UE_KINDA_SMALL_NUMBER))
		{
			DenseLayerHeights.Add(Z);
		}
	}
	TestTrue(TEXT("提高Z层级密度应增加水平截面层数"), DenseLayerHeights.Num() > PackedLayerHeights.Num());
	int32 BaseCenterLayerCount = 0;
	int32 DenseCenterLayerCount = 0;
	for (const FTransform& Point : ClosePackedSphere)
	{
		BaseCenterLayerCount += FMath::IsNearlyZero(Point.GetLocation().Z, UE_KINDA_SMALL_NUMBER) ? 1 : 0;
	}
	for (const FTransform& Point : DenseLayerClosePackedSphere)
	{
		DenseCenterLayerCount += FMath::IsNearlyZero(Point.GetLocation().Z, UE_KINDA_SMALL_NUMBER) ? 1 : 0;
	}
	TestEqual(TEXT("提高Z层级密度不应增加中心截面的点数"), DenseCenterLayerCount, BaseCenterLayerCount);
	int32 CircleCacheHitsBefore = 0;
	int32 CircleCacheMissesBefore = 0;
	FSamplingCache::Get().GetCircleStats(CircleCacheHitsBefore, CircleCacheMissesBefore);
	const TArray<FTransform> CachedDenseLayerClosePackedSphere = UFormationSamplingLibrary::GenerateCircle(
		120, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::ClosePacked, 50.0f, 0.0f, true, RawSpace,
		0.0f, 0, true, 25.0f, 0.0f, 2.0f, 1, EPointArrayOrderMode::SphereBottomToTopCounterClockwise);
	int32 CircleCacheHitsAfter = 0;
	int32 CircleCacheMissesAfter = 0;
	FSamplingCache::Get().GetCircleStats(CircleCacheHitsAfter, CircleCacheMissesAfter);
	TestEqual(TEXT("紧密堆叠缓存应保持生成结果"), CachedDenseLayerClosePackedSphere.Num(), DenseLayerClosePackedSphere.Num());
	TestEqual(TEXT("相同紧密堆叠参数应命中局部几何缓存"), CircleCacheHitsAfter, CircleCacheHitsBefore + 1);
	TestEqual(TEXT("相同紧密堆叠参数不应产生新的缓存未命中"), CircleCacheMissesAfter, CircleCacheMissesBefore);
	int32 PublicCacheHits = 0;
	int32 PublicCacheMisses = 0;
	UPointSamplingLibrary::GetPoissonSamplingCacheStats(PublicCacheHits, PublicCacheMisses);
	TestTrue(TEXT("公开缓存统计应包含紧密堆叠球体缓存命中"), PublicCacheHits >= CircleCacheHitsAfter);
	TestTrue(TEXT("公开缓存统计应包含紧密堆叠球体缓存未命中"), PublicCacheMisses >= CircleCacheMissesAfter);
	for (const float LayerHeight : DenseLayerHeights)
	{
		bool bHasZeroDegreePoint = false;
		for (const FTransform& Point : DenseLayerClosePackedSphere)
		{
			const FVector Location = Point.GetLocation();
			bHasZeroDegreePoint |= FMath::IsNearlyEqual(Location.Z, LayerHeight, UE_KINDA_SMALL_NUMBER)
				&& FMath::IsNearlyZero(Location.Y, UE_KINDA_SMALL_NUMBER)
				&& Location.X >= -UE_KINDA_SMALL_NUMBER;
		}
		TestTrue(TEXT("提高Z层级密度后各层应保持固定环形相位"), bHasZeroDegreePoint);
	}
	float MinimumPackedDistance = TNumericLimits<float>::Max();
	for (int32 LeftIndex = 0; LeftIndex < ClosePackedSphere.Num(); ++LeftIndex)
	{
		const FVector Left = ClosePackedSphere[LeftIndex].GetLocation();
		for (int32 RightIndex = LeftIndex + 1; RightIndex < ClosePackedSphere.Num(); ++RightIndex)
		{
			const FVector Right = ClosePackedSphere[RightIndex].GetLocation();
			MinimumPackedDistance = FMath::Min(MinimumPackedDistance, FVector::Dist(Left, Right));
		}
	}
	TestTrue(TEXT("紧密堆叠实心球不应生成重合点"), MinimumPackedDistance > UE_KINDA_SMALL_NUMBER);
	if (PackedLayerHeights.Num() >= 2)
	{
		const float AutomaticLayerSpacing = PackedLayerHeights[1] - PackedLayerHeights[0];
		for (int32 LayerIndex = 2; LayerIndex < PackedLayerHeights.Num(); ++LayerIndex)
		{
			TestTrue(TEXT("紧密堆叠自动层距应保持一致"),
				FMath::IsNearlyEqual(PackedLayerHeights[LayerIndex] - PackedLayerHeights[LayerIndex - 1],
					AutomaticLayerSpacing, 0.05f));
		}
	}

	const TArray<FTransform> ManuallyLayerSpacedClosePackedSphere = UFormationSamplingLibrary::GenerateCircle(
		120, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::ClosePacked, 50.0f, 0.0f, true, RawSpace,
		0.0f, 0, true, 25.0f, 5.0f, 2.0f, 1, EPointArrayOrderMode::SphereBottomToTopClockwise);
	TestEqual(TEXT("紧密堆叠手动层距不应改变实际点数"), ManuallyLayerSpacedClosePackedSphere.Num(), DenseLayerClosePackedSphere.Num());
	TArray<float> ManuallyLayerHeights;
	for (const FTransform& Point : ManuallyLayerSpacedClosePackedSphere)
	{
		const float Z = Point.GetLocation().Z;
		bool bKnownLayer = false;
		for (const float Height : ManuallyLayerHeights)
		{
			bKnownLayer |= FMath::IsNearlyEqual(Z, Height, UE_KINDA_SMALL_NUMBER);
		}
		if (!bKnownLayer)
		{
			ManuallyLayerHeights.Add(Z);
		}
	}
	TestTrue(TEXT("紧密堆叠手动层距应生成多层"), ManuallyLayerHeights.Num() >= 3);
	if (ManuallyLayerHeights.Num() >= 2)
	{
		TestTrue(TEXT("紧密堆叠应使用合法范围内的手动层间距"),
			FMath::IsNearlyEqual(ManuallyLayerHeights[1] - ManuallyLayerHeights[0], 5.0f, 0.05f));
		for (int32 LayerIndex = 1; LayerIndex < ManuallyLayerHeights.Num(); ++LayerIndex)
		{
			TestTrue(TEXT("紧密堆叠手动层距不应产生断层"),
				FMath::IsNearlyEqual(ManuallyLayerHeights[LayerIndex] - ManuallyLayerHeights[LayerIndex - 1], 5.0f, 0.05f));
		}
	}

	const TArray<FTransform> WideLayerSpacedClosePackedSphere = UFormationSamplingLibrary::GenerateCircle(
		120, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, true, true,
		ECircleDistributionMode::ClosePacked, 50.0f, 0.0f, true, RawSpace,
		0.0f, 0, true, 25.0f, 40.0f, 2.0f, 1, EPointArrayOrderMode::SphereBottomToTopCounterClockwise);
	TestEqual(TEXT("调整Z轴层间距不应改变紧密堆叠实际点数"), WideLayerSpacedClosePackedSphere.Num(), DenseLayerClosePackedSphere.Num());
	if (ManuallyLayerSpacedClosePackedSphere.Num() == WideLayerSpacedClosePackedSphere.Num())
	{
		for (int32 PointIndex = 0; PointIndex < ManuallyLayerSpacedClosePackedSphere.Num(); ++PointIndex)
		{
			const float CompactRadius = ManuallyLayerSpacedClosePackedSphere[PointIndex].GetLocation().Size2D();
			const float WideRadius = WideLayerSpacedClosePackedSphere[PointIndex].GetLocation().Size2D();
			TestTrue(TEXT("Z轴层间距不应改变任何水平点位"),
				FMath::IsNearlyEqual(CompactRadius, WideRadius, UE_KINDA_SMALL_NUMBER));
		}
	}
	TArray<float> WideLayerHeights;
	for (const FTransform& Point : WideLayerSpacedClosePackedSphere)
	{
		const float Z = Point.GetLocation().Z;
		if (WideLayerHeights.IsEmpty() || !FMath::IsNearlyEqual(WideLayerHeights.Last(), Z, UE_KINDA_SMALL_NUMBER))
		{
			WideLayerHeights.Add(Z);
		}
	}
	if (WideLayerHeights.Num() >= 2)
	{
		for (int32 LayerIndex = 1; LayerIndex < WideLayerHeights.Num(); ++LayerIndex)
		{
			TestTrue(TEXT("超出球形约束的Z轴层间距应被限制为一致层高"),
				FMath::IsNearlyEqual(WideLayerHeights[LayerIndex] - WideLayerHeights[LayerIndex - 1],
					WideLayerHeights[1] - WideLayerHeights[0], 0.05f));
		}
	}
	for (const FTransform& Point : WideLayerSpacedClosePackedSphere)
	{
		TestTrue(TEXT("超出球形约束的Z轴层间距应被限制"), Point.GetLocation().Size() <= 100.01f);
	}

	const TArray<FVector> Snowflake = UFormationSamplingLibrary::GenerateSnowflake(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 150.0f, 3, 50.0f, RawSpace);
	TestPointCount(*this, TEXT("雪花形"), Snowflake, 12);

	const TArray<FVector> SnowflakeArc = UFormationSamplingLibrary::GenerateSnowflakeArc(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 150.0f, 3, 50.0f, 180.0f, 0.0f, RawSpace);
	TestPointCount(*this, TEXT("雪花弧形"), SnowflakeArc, 12);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_MilitaryAlgorithms,
	"XTools.PointSampling.Formation.MilitaryAlgorithms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_MilitaryAlgorithms::RunTest(const FString& Parameters)
{
	constexpr EPoissonCoordinateSpace RawSpace = EPoissonCoordinateSpace::Raw;
	const TArray<FVector> Wedge = UFormationSamplingLibrary::GenerateWedgeFormation(
		5, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 60.0f, RawSpace);
	TestPointCount(*this, TEXT("楔形阵"), Wedge, 5);
	if (Wedge.Num() >= 3)
	{
		TestTrue(TEXT("楔形阵应从尖端向两侧展开"), Wedge[0].IsNearlyZero() && Wedge[1].Y < 0.0f && Wedge[2].Y > 0.0f);
	}

	const TArray<FVector> Column = UFormationSamplingLibrary::GenerateColumnFormation(
		5, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, RawSpace);
	TestPointCount(*this, TEXT("纵队阵"), Column, 5);
	for (const FVector& Point : Column)
	{
		TestTrue(TEXT("纵队阵应保持在Y轴"), FMath::IsNearlyZero(Point.X) && FMath::IsNearlyZero(Point.Z));
	}

	const TArray<FVector> Line = UFormationSamplingLibrary::GenerateLineFormation(
		5, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, RawSpace);
	TestPointCount(*this, TEXT("横队阵"), Line, 5);
	for (const FVector& Point : Line)
	{
		TestTrue(TEXT("横队阵应保持在X轴"), FMath::IsNearlyZero(Point.Y) && FMath::IsNearlyZero(Point.Z));
	}

	const TArray<FVector> Vee = UFormationSamplingLibrary::GenerateVeeFormation(
		5, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 60.0f, RawSpace);
	TestPointCount(*this, TEXT("V形阵"), Vee, 5);
	if (Vee.Num() >= 3)
	{
		TestTrue(TEXT("V形阵应与楔形阵具有相反的侧向展开"),
			Vee[1].Y > 0.0f && Vee[2].Y < 0.0f);
	}

	const TArray<FVector> RightEchelon = UFormationSamplingLibrary::GenerateEchelonFormation(
		6, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 1, 30.0f, RawSpace);
	const TArray<FVector> LeftEchelon = UFormationSamplingLibrary::GenerateEchelonFormation(
		6, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, -1, 30.0f, RawSpace);
	TestPointCount(*this, TEXT("右梯形阵"), RightEchelon, 6);
	TestPointCount(*this, TEXT("左梯形阵"), LeftEchelon, 6);
	if (RightEchelon.Num() >= 4 && LeftEchelon.Num() >= 4)
	{
		TestTrue(TEXT("左右梯形阵第二层偏移方向应相反"),
			RightEchelon[3].X > LeftEchelon[3].X);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_GeometricAlgorithms,
	"XTools.PointSampling.Formation.GeometricAlgorithms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_GeometricAlgorithms::RunTest(const FString& Parameters)
{
	constexpr EPoissonCoordinateSpace RawSpace = EPoissonCoordinateSpace::Raw;
	const TArray<FVector> HexagonalGrid = UFormationSamplingLibrary::GenerateHexagonalGrid(
		7, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 1, RawSpace);
	TestPointCount(*this, TEXT("蜂巢阵"), HexagonalGrid, 7);

	const TArray<FVector> Star = UFormationSamplingLibrary::GenerateStarFormation(
		10, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 50.0f, 5, RawSpace);
	TestPointCount(*this, TEXT("星形阵"), Star, 10);
	if (Star.Num() >= 2)
	{
		TestTrue(TEXT("星形阵应交替使用内外半径"),
			FMath::IsNearlyEqual(Star[0].Size(), 100.0f, UE_KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(Star[1].Size(), 50.0f, UE_KINDA_SMALL_NUMBER));
	}

	const TArray<FVector> Archimedean = UFormationSamplingLibrary::GenerateArchimedeanSpiral(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 2.0f, RawSpace);
	TestPointCount(*this, TEXT("阿基米德螺旋"), Archimedean, 8);
	if (Archimedean.Num() >= 2)
	{
		TestTrue(TEXT("阿基米德螺旋应由中心向外扩展"),
			Archimedean[0].IsNearlyZero() && Archimedean.Last().Size() > Archimedean[1].Size());
	}

	const TArray<FVector> Logarithmic = UFormationSamplingLibrary::GenerateLogarithmicSpiral(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 1.1f, 20.0f, RawSpace);
	TestPointCount(*this, TEXT("对数螺旋"), Logarithmic, 8);

	const TArray<FVector> Heart = UFormationSamplingLibrary::GenerateHeartFormation(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, RawSpace);
	TestPointCount(*this, TEXT("心形阵"), Heart, 8);

	const TArray<FVector> Flower = UFormationSamplingLibrary::GenerateFlowerFormation(
		10, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 50.0f, 5, RawSpace);
	TestPointCount(*this, TEXT("花瓣阵"), Flower, 10);

	const TArray<FVector> GoldenSpiral = UFormationSamplingLibrary::GenerateGoldenSpiralFormation(
		8, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, RawSpace);
	TestPointCount(*this, TEXT("黄金螺旋"), GoldenSpiral, 8);
	if (GoldenSpiral.Num() >= 2)
	{
		TestTrue(TEXT("黄金螺旋应从中心扩展到最大半径"),
			GoldenSpiral[0].IsNearlyZero() &&
			FMath::IsNearlyEqual(GoldenSpiral.Last().Size(), 100.0f, UE_KINDA_SMALL_NUMBER));
	}

	const TArray<FVector> CircularGrid = UFormationSamplingLibrary::GenerateCircularGridFormation(
		12, FVector::ZeroVector, FRotator::ZeroRotator, 120.0f, 3, 4, RawSpace);
	TestPointCount(*this, TEXT("圆形网格"), CircularGrid, 12);

	const TArray<FVector> RoseCurve = UFormationSamplingLibrary::GenerateRoseCurveFormation(
		9, FVector::ZeroVector, FRotator::ZeroRotator, 100.0f, 3, RawSpace);
	TestPointCount(*this, TEXT("玫瑰曲线"), RoseCurve, 9);

	const TArray<int32> AutomaticRings;
	const TArray<FVector> ConcentricRings = UFormationSamplingLibrary::GenerateConcentricRingsFormation(
		12, FVector::ZeroVector, FRotator::ZeroRotator, AutomaticRings, 120.0f, 3, RawSpace);
	TestPointCount(*this, TEXT("同心圆环"), ConcentricRings, 12);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_SortingAlgorithms,
	"XTools.PointSampling.Formation.SortingAlgorithms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_SortingAlgorithms::RunTest(const FString& Parameters)
{
	const TArray<FVector> GridPoints = {
		FVector(0.0f, 10.0f, 0.0f),
		FVector(10.0f, -10.0f, 0.0f),
		FVector(-10.0f, -10.0f, 0.0f),
		FVector(0.0f, -10.0f, 0.0f)
	};
	const TArray<FVector> GridSorted = UFormationSamplingLibrary::SortPointArray(
		GridPoints, EPointArrayOrderMode::GridBottomToTopLeftToRight);
	TestEqual(TEXT("网格排序应保留全部点位"), GridSorted.Num(), GridPoints.Num());
	if (GridSorted.Num() == GridPoints.Num())
	{
		TestTrue(TEXT("网格排序应从下至上且每行从左至右"),
			GridSorted[0].Equals(FVector(-10.0f, -10.0f, 0.0f)) &&
			GridSorted[1].Equals(FVector(0.0f, -10.0f, 0.0f)) &&
			GridSorted[2].Equals(FVector(10.0f, -10.0f, 0.0f)));
	}

	const TArray<FVector> RingPoints = {
		FVector(0.0f, 10.0f, 0.0f),
		FVector(-10.0f, 0.0f, 0.0f),
		FVector(0.0f, -10.0f, 0.0f),
		FVector(10.0f, 0.0f, 0.0f)
	};
	const TArray<FVector> Clockwise = UFormationSamplingLibrary::SortPointArray(
		RingPoints, EPointArrayOrderMode::CircleClockwise);
	TestEqual(TEXT("圆形排序应保留全部点位"), Clockwise.Num(), RingPoints.Num());
	if (Clockwise.Num() == RingPoints.Num())
	{
		TestTrue(TEXT("圆形顺时针排序应从起始角开始"),
			Clockwise[0].Equals(FVector(10.0f, 0.0f, 0.0f)) &&
			Clockwise[1].Equals(FVector(0.0f, -10.0f, 0.0f)) &&
			Clockwise[2].Equals(FVector(-10.0f, 0.0f, 0.0f)));
	}

	TArray<FTransform> Transforms;
	for (const FVector& Point : RingPoints)
	{
		Transforms.Emplace(FRotator::ZeroRotator, Point);
	}
	const TArray<FTransform> ReversedTransforms = UFormationSamplingLibrary::SortTransformArray(
		Transforms, EPointArrayOrderMode::CircleClockwise, FVector::ZeroVector,
		FRotator::ZeroRotator, 0.0f, 1.0f, true);
	TestEqual(TEXT("变换排序应保留全部变换"), ReversedTransforms.Num(), Transforms.Num());
	if (ReversedTransforms.Num() == Transforms.Num())
	{
		TestTrue(TEXT("反转索引应反转完整排序结果"),
			ReversedTransforms[0].GetLocation().Equals(FVector(0.0f, 10.0f, 0.0f)));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationSampling_GenericDispatcher,
	"XTools.PointSampling.Formation.GenericDispatcher",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationSampling_GenericDispatcher::RunTest(const FString& Parameters)
{
	const TArray<EPointSamplingMode> Modes = {
		EPointSamplingMode::SolidRectangle,
		EPointSamplingMode::HollowRectangle,
		EPointSamplingMode::SpiralRectangle,
		EPointSamplingMode::SolidTriangle,
		EPointSamplingMode::HollowTriangle,
		EPointSamplingMode::Circle,
		EPointSamplingMode::Snowflake,
		EPointSamplingMode::SnowflakeArc,
		EPointSamplingMode::Wedge,
		EPointSamplingMode::Column,
		EPointSamplingMode::Line,
		EPointSamplingMode::Vee,
		EPointSamplingMode::Echelon,
		EPointSamplingMode::EchelonLeft,
		EPointSamplingMode::EchelonRight,
		EPointSamplingMode::HexagonalGrid,
		EPointSamplingMode::Star,
		EPointSamplingMode::ArchimedeanSpiral,
		EPointSamplingMode::LogarithmicSpiral,
		EPointSamplingMode::Heart,
		EPointSamplingMode::Flower,
		EPointSamplingMode::GoldenSpiral,
		EPointSamplingMode::CircularGrid,
		EPointSamplingMode::RoseCurve,
		EPointSamplingMode::ConcentricRings
	};

	for (const EPointSamplingMode Mode : Modes)
	{
		const TArray<FVector> Points = UPointSamplingLibrary::GenerateFormation(
			Mode, 12, FVector::ZeroVector, FRotator::ZeroRotator,
			EPoissonCoordinateSpace::Raw, 100.0f, 0.0f, 0, 3.0f, 50.0f, 5);
		TestTrue(*FString::Printf(TEXT("通用阵型模式%d应产生点位"), static_cast<int32>(Mode)), !Points.IsEmpty());
		TestFinitePoints(*this, FString::Printf(TEXT("通用阵型模式%d"), static_cast<int32>(Mode)), Points);
	}

	return true;
}

#endif
