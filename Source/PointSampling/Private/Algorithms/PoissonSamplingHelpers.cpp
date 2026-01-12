/*
* 测试泊松采样算法的一致性
* 这个函数用于验证优化后的算法是否保持了基本的采样特性
*/
void TestPoissonSamplingConsistency()
{
    // 测试参数
    const float TestWidth = 1000.0f;
    const float TestHeight = 1000.0f;
    const float TestRadius = 50.0f;
    const int32 MaxAttempts = 30;
    const int32 TestRuns = 5;

    UE_LOG(LogPointSampling, Log, TEXT("=== 泊松采样一致性测试开始 ==="));

    for (int32 Run = 0; Run < TestRuns; ++Run)
    {
        // 测试2D采样
        TArray<FVector2D> Points2D = FPoissonDiskSampling::GeneratePoisson2D(
            TestWidth, TestHeight, TestRadius, MaxAttempts);

        // 基本验证
        bool bHasValidPoints = Points2D.Num() > 0;
        bool bAllPointsInBounds = true;
        bool bMinimumDistanceMaintained = true;

        for (const FVector2D& Point : Points2D)
        {
            // 检查边界
            if (Point.X < 0 || Point.X >= TestWidth || Point.Y < 0 || Point.Y >= TestHeight)
            {
                bAllPointsInBounds = false;
                break;
            }

            // 检查最小距离（近似检查）
            for (const FVector2D& OtherPoint : Points2D)
            {
                if (&Point != &OtherPoint)
                {
                    float Distance = FVector2D::Distance(Point, OtherPoint);
                    if (Distance < TestRadius * 0.8f) // 允许一些误差
                    {
                        bMinimumDistanceMaintained = false;
                        break;
                    }
                }
            }
            if (!bMinimumDistanceMaintained) break;
        }

        UE_LOG(LogPointSampling, Log, TEXT("测试运行 %d/2D: 点数=%d, 边界有效=%s, 距离有效=%s"),
            Run + 1, Points2D.Num(),
            bAllPointsInBounds ? TEXT("是") : TEXT("否"),
            bMinimumDistanceMaintained ? TEXT("是") : TEXT("否"));

        // 测试3D采样
        TArray<FVector> Points3D = FPoissonDiskSampling::GeneratePoisson3D(
            TestWidth, TestHeight, TestWidth, TestRadius, MaxAttempts);

        bAllPointsInBounds = true;
        bMinimumDistanceMaintained = true;

        for (const FVector& Point : Points3D)
        {
            // 检查边界
            if (Point.X < 0 || Point.X >= TestWidth ||
                Point.Y < 0 || Point.Y >= TestHeight ||
                Point.Z < 0 || Point.Z >= TestWidth)
            {
                bAllPointsInBounds = false;
                break;
            }

            // 检查最小距离（近似检查）
            for (const FVector& OtherPoint : Points3D)
            {
                if (&Point != &OtherPoint)
                {
                    float Distance = FVector::Distance(Point, OtherPoint);
                    if (Distance < TestRadius * 0.8f)
                    {
                        bMinimumDistanceMaintained = false;
                        break;
                    }
                }
            }
            if (!bMinimumDistanceMaintained) break;
        }

        UE_LOG(LogPointSampling, Log, TEXT("测试运行 %d/3D: 点数=%d, 边界有效=%s, 距离有效=%s"),
            Run + 1, Points3D.Num(),
            bAllPointsInBounds ? TEXT("是") : TEXT("否"),
            bMinimumDistanceMaintained ? TEXT("是") : TEXT("否"));
    }

    UE_LOG(LogPointSampling, Log, TEXT("=== 泊松采样一致性测试完成 ==="));
}