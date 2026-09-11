#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationSamplingLibrary.h"
#include "PointSamplingLibrary.h"
#include "Algorithms/PoissonDiskSampling.h"
#include "Misc/AutomationTest.h"
#include <algorithm>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoissonSampling_GridCapacity,
    "XTools.PointSampling.Poisson.GridCapacity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPoissonSampling_GridCapacity::RunTest(const FString& Parameters)
{
    // 统一报告器同时写入模块日志与编辑器 MessageLog。
    AddExpectedError(TEXT("PoissonSampler: 网格超过"), EAutomationExpectedErrorFlags::Contains, 4);
    TestTrue(TEXT("三维网格乘积溢出应安全拒绝"),
        UPointSamplingLibrary::GeneratePoissonPoints3D(1000, 1000, 1000, 1.25f, 30).IsEmpty());
    TestTrue(TEXT("未溢出但超内存预算的网格也应拒绝"),
        UPointSamplingLibrary::GeneratePoissonPoints2D(1000, 1000, 0.3f, 30).IsEmpty());
    AddExpectedError(TEXT("PoissonSampler: 网格轴尺寸"), EAutomationExpectedErrorFlags::Contains, 2);
    const FRandomStream RejectedStream(1234);
    TestTrue(TEXT("网格轴整数转换前应验证范围"),
        FPoissonDiskSampling::GeneratePoisson2DFromStream(RejectedStream, 1.e20f, 10, 1).IsEmpty());
    TestEqual(TEXT("拒绝输入不消耗随机流"), RejectedStream.GetCurrentSeed(), 1234);

    const FRandomStream StreamA(1234), StreamB(1234);
    const TArray<FVector> Points = FPoissonDiskSampling::GeneratePoisson3DFromStream(StreamA, 100, 100, 100, 25);
    TestTrue(TEXT("正常三维采样应生成点"), Points.Num() > 1);
    TestTrue(TEXT("固定种子应可复现"), Points == FPoissonDiskSampling::GeneratePoisson3DFromStream(StreamB, 100, 100, 100, 25));
    for (int32 I = 0; I < Points.Num(); ++I)
    {
        TestTrue(TEXT("采样点保持在区域内"), Points[I].X >= 0 && Points[I].X < 100
            && Points[I].Y >= 0 && Points[I].Y < 100 && Points[I].Z >= 0 && Points[I].Z < 100);
        for (int32 J = 0; J < I; ++J)
        {
            TestTrue(TEXT("最小间距不因容量防护而改变"), FVector::DistSquared(Points[I], Points[J]) >= 625.0 - 1.e-3);
        }
    }
    // 小尺度二维采样的旧预估容量可在钳位前溢出，但真实网格很小。
    const TArray<FVector2D> SmallPoints = FPoissonDiskSampling::GeneratePoisson2DFromStream(
        FRandomStream(42), 0.001f, 0.001f, 0.000008f, 1);
    TestTrue(TEXT("小尺度有限网格的预分配应安全"), !SmallPoints.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRectangleSampling_CountContract,
    "XTools.PointSampling.Rectangle.AutoCountAndCapacity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRectangleSampling_CountContract::RunTest(const FString& Parameters)
{
    for (int32 Count : {-1, 0})
    {
        TestEqual(TEXT("实心自动点数"), UFormationSamplingLibrary::GenerateSolidRectangle(
            Count, FVector::ZeroVector, FRotator::ZeroRotator).Num(), 25);
        TestEqual(TEXT("空心自动点数"), UFormationSamplingLibrary::GenerateHollowRectangle(
            Count, FVector::ZeroVector, FRotator::ZeroRotator).Num(), 20);
        TestEqual(TEXT("显式行列应决定实心点数"), UFormationSamplingLibrary::GenerateSolidRectangle(
            Count, FVector::ZeroVector, FRotator::ZeroRotator, 100, 3, 4).Num(), 12);
        TestEqual(TEXT("显式行列应决定边框点数"), UFormationSamplingLibrary::GenerateHollowRectangle(
            Count, FVector::ZeroVector, FRotator::ZeroRotator, 100, 3, 4).Num(), 10);
    }
    for (const FIntPoint Size : {FIntPoint(1, 1), FIntPoint(1, 5), FIntPoint(5, 1)})
    {
        TestEqual(TEXT("退化边框不重复、不丢点"), UFormationSamplingLibrary::GenerateHollowRectangle(
            -1, FVector::ZeroVector, FRotator::ZeroRotator, 100, Size.X, Size.Y).Num(), Size.X * Size.Y);
    }
    AddExpectedError(TEXT("GenerateSolidRectangle: 点数超过安全预算"), EAutomationExpectedErrorFlags::Contains, 2);
    TestTrue(TEXT("显式行列乘积溢出应安全拒绝"), UFormationSamplingLibrary::GenerateSolidRectangle(
        -1, FVector::ZeroVector, FRotator::ZeroRotator, 100, 65536, 65536).IsEmpty());
    AddExpectedError(TEXT("GenerateHollowRectangle: 点数超过安全预算"), EAutomationExpectedErrorFlags::Contains, 2);
    TestTrue(TEXT("周长计算溢出应安全拒绝"), UFormationSamplingLibrary::GenerateHollowRectangle(
        -1, FVector::ZeroVector, FRotator::ZeroRotator, 100, MAX_int32, MAX_int32).IsEmpty());
    TestEqual(TEXT("显式目标数在大网格中仍可安全截断"), UFormationSamplingLibrary::GenerateSolidRectangle(
        3, FVector::ZeroVector, FRotator::ZeroRotator, 100, 65536, 65536).Num(), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriangleSampling_EquilateralLattice,
    "XTools.PointSampling.Triangle.EquilateralLattice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTriangleSampling_EquilateralLattice::RunTest(const FString& Parameters)
{
    for (int32 Count : {3, 6, 10})
    {
        for (bool bInverted : {false, true})
        {
            const TArray<FVector> Points = UFormationSamplingLibrary::GenerateSolidTriangle(
                Count, FVector::ZeroVector, FRotator::ZeroRotator, 100, bInverted, EPoissonCoordinateSpace::Raw, 0, 0);
            TestEqual(TEXT("完整三角阵列点数守恒"), Points.Num(), Count);
            for (int32 I = 0; I < Points.Num(); ++I)
            {
                const FVector Mirror(-Points[I].X, Points[I].Y, Points[I].Z);
                TestTrue(TEXT("完整三角阵列左右对称"), Points.ContainsByPredicate(
                    [&Mirror](const FVector& Point) { return Point.Equals(Mirror, 1.e-3); }));
                for (int32 J = 0; J < I; ++J)
                {
                    const double Distance = FVector::Distance(Points[I], Points[J]);
                    TestTrue(TEXT("不同点间距不小于请求间距"), Distance >= 100.0 - 1.e-3);
                    if (Count == 3)
                    {
                        TestTrue(TEXT("三点组成等边三角形"), FMath::Abs(Distance - 100.0) < 1.e-3);
                    }
                }
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSphereSampling_OrderTransitivity,
    "XTools.PointSampling.Sorting.RadialOrderTransitivity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSphereSampling_OrderTransitivity::RunTest(const FString& Parameters)
{
    const auto Polar = [](double Radius, double Degrees)
    {
        const double Angle = FMath::DegreesToRadians(Degrees);
        return FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.0);
    };
    const TArray<FVector> Points = {Polar(100.001, 30), Polar(100.0085, 20), Polar(100.016, 10), Polar(100.011, 10)};
    TArray<int32> Permutation = {0, 1, 2, 3};
    do
    {
        TArray<FVector> Input;
        TArray<FTransform> Transforms;
        for (int32 Index : Permutation)
        {
            Input.Add(Points[Index]);
            Transforms.Emplace(FRotator::ZeroRotator, Points[Index]);
        }
        for (bool bClockwise : {false, true})
        {
            const EPointArrayOrderMode Mode = bClockwise ? EPointArrayOrderMode::SphereBottomToTopClockwise
                : EPointArrayOrderMode::SphereBottomToTopCounterClockwise;
            const TArray<int32> Order = bClockwise ? TArray<int32>({0, 1, 3, 2}) : TArray<int32>({0, 3, 1, 2});
            const TArray<FVector> Sorted = UFormationSamplingLibrary::SortPointArray(Input, Mode);
            const TArray<FTransform> SortedTransforms = UFormationSamplingLibrary::SortTransformArray(Transforms, Mode);
            for (int32 Index = 0; Index < Sorted.Num(); ++Index)
            {
                TestTrue(TEXT("径向量化后排序与输入排列无关"), Sorted[Index].Equals(Points[Order[Index]], 1.e-6));
                TestTrue(TEXT("点和变换排序保持一致"), SortedTransforms[Index].GetLocation().Equals(Sorted[Index], 1.e-6));
            }
        }
    }
    while (std::next_permutation(Permutation.GetData(), Permutation.GetData() + Permutation.Num()));
    return true;
}

#endif
