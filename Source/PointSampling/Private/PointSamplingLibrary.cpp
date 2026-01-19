/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 */

#include "PointSamplingLibrary.h"
#include "Algorithms/PoissonDiskSampling.h"
#include "FormationSamplingLibrary.h"
#include "Sampling/TextureSamplingHelper.h"

// ============================================================================
// 基础2D/3D采样
// ============================================================================

TArray<FVector2D> UPointSamplingLibrary::GeneratePoissonPoints2D(
    float Width, float Height, float Radius, int32 MaxAttempts) {
  return FPoissonDiskSampling::GeneratePoisson2D(Width, Height, Radius,
                                                 MaxAttempts);
}

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPoints3D(
    float Width, float Height, float Depth, float Radius, int32 MaxAttempts) {
  return FPoissonDiskSampling::GeneratePoisson3D(Width, Height, Depth, Radius,
                                                 MaxAttempts);
}

// ============================================================================
// Box组件采样
// ============================================================================

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPointsInBox(
    UBoxComponent *BoxComponent, float Radius, int32 MaxAttempts,
    EPoissonCoordinateSpace CoordinateSpace, int32 TargetPointCount,
    float JitterStrength, bool bUseCache) {
  return FPoissonDiskSampling::GeneratePoissonInBox(
      BoxComponent, Radius, MaxAttempts, CoordinateSpace, TargetPointCount,
      JitterStrength, bUseCache);
}

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPointsInBoxByVector(
    FVector BoxExtent, FTransform Transform, float Radius, int32 MaxAttempts,
    EPoissonCoordinateSpace CoordinateSpace, int32 TargetPointCount,
    float JitterStrength, bool bUseCache) {
  return FPoissonDiskSampling::GeneratePoissonInBoxByVector(
      BoxExtent, Transform, Radius, MaxAttempts, CoordinateSpace,
      TargetPointCount, JitterStrength, bUseCache);
}

// ============================================================================
// FromStream版本（流送）
// ============================================================================

TArray<FVector> UPointSamplingLibrary::GeneratePoissonPointsInBoxFromStream(
    const FRandomStream &RandomStream, UBoxComponent *BoxComponent,
    float Radius, int32 MaxAttempts, EPoissonCoordinateSpace CoordinateSpace,
    int32 TargetPointCount, float JitterStrength) {
  return FPoissonDiskSampling::GeneratePoissonInBoxFromStream(
      RandomStream, BoxComponent, Radius, MaxAttempts, CoordinateSpace,
      TargetPointCount, JitterStrength);
}

TArray<FVector>
UPointSamplingLibrary::GeneratePoissonPointsInBoxByVectorFromStream(
    const FRandomStream &RandomStream, FVector BoxExtent, FTransform Transform,
    float Radius, int32 MaxAttempts, EPoissonCoordinateSpace CoordinateSpace,
    int32 TargetPointCount, float JitterStrength) {
  return FPoissonDiskSampling::GeneratePoissonInBoxByVectorFromStream(
      RandomStream, BoxExtent, Transform, Radius, MaxAttempts, CoordinateSpace,
      TargetPointCount, JitterStrength);
}

// ============================================================================
// 缓存管理
// ============================================================================

void UPointSamplingLibrary::ClearPoissonSamplingCache() {
  FPoissonDiskSampling::ClearCache();
}

void UPointSamplingLibrary::GetPoissonSamplingCacheStats(int32 &OutHits,
                                                         int32 &OutMisses) {
  FPoissonDiskSampling::GetCacheStats(OutHits, OutMisses);
}

// ============================================================================
// 采样质量验证
// ============================================================================

void UPointSamplingLibrary::AnalyzeSamplingStats(const TArray<FVector> &Points,
                                                 float &OutMinDistance,
                                                 float &OutMaxDistance,
                                                 float &OutAvgDistance,
                                                 FVector &OutCentroid) {
  OutMinDistance = FLT_MAX;
  OutMaxDistance = 0.0f;
  OutAvgDistance = 0.0f;
  OutCentroid = FVector::ZeroVector;

  const int32 NumPoints = Points.Num();
  if (NumPoints < 2) {
    return;
  }

  // 对于大量点集，计算复杂度很高，记录警告
  const int32 MaxReasonablePoints = 1000;
  if (NumPoints > MaxReasonablePoints) {
    UE_LOG(LogPointSampling, Warning,
           TEXT("[采样统计] 点集较大 (%d 点)，距离计算复杂度为 "
                "O(N²)，可能影响性能"),
           NumPoints);
  }

  // 计算质心
  for (const FVector &Point : Points) {
    OutCentroid += Point;
  }
  OutCentroid /= static_cast<float>(NumPoints);

  // 计算所有点对之间的距离统计
  int32 DistanceCount = 0;
  for (int32 i = 0; i < NumPoints; ++i) {
    for (int32 j = i + 1; j < NumPoints; ++j) {
      const float Distance = FVector::Dist(Points[i], Points[j]);
      OutMinDistance = FMath::Min(OutMinDistance, Distance);
      OutMaxDistance = FMath::Max(OutMaxDistance, Distance);
      OutAvgDistance += Distance;
      ++DistanceCount;
    }
  }

  if (DistanceCount > 0) {
    OutAvgDistance /= static_cast<float>(DistanceCount);
  }
}

bool UPointSamplingLibrary::ValidatePoissonSampling(
    const TArray<FVector> &Points, float ExpectedMinDistance, float Tolerance) {
  const int32 NumPoints = Points.Num();
  if (NumPoints < 2) {
    return true; // 单点或空集认为是有效的
  }

  if (!ensure(ExpectedMinDistance > 0.0f && Tolerance >= 0.0f &&
              Tolerance <= 1.0f)) {
    UE_LOG(
        LogPointSampling, Error,
        TEXT("[泊松验证] 无效参数: ExpectedMinDistance=%.2f, Tolerance=%.2f"),
        ExpectedMinDistance, Tolerance);
    return false;
  }

  const float MinAllowedDistance = ExpectedMinDistance * (1.0f - Tolerance);

  // 对于大量点集，验证复杂度很高
  const int32 MaxReasonablePoints = 500;
  if (NumPoints > MaxReasonablePoints) {
    UE_LOG(LogPointSampling, Warning,
           TEXT("[泊松验证] 点集较大 (%d 点)，验证复杂度为 O(N²)"), NumPoints);
  }

  // 检查是否有任何点对距离小于最小允许距离
  for (int32 i = 0; i < NumPoints; ++i) {
    for (int32 j = i + 1; j < NumPoints; ++j) {
      const float Distance = FVector::Dist(Points[i], Points[j]);
      if (Distance < MinAllowedDistance) {
        UE_LOG(LogPointSampling, Warning,
               TEXT("泊松采样验证失败：点 %d 和 %d 之间的距离 %.2f "
                    "小于最小允许距离 %.2f"),
               i, j, Distance, MinAllowedDistance);
        return false;
      }
    }
  }

  UE_LOG(LogPointSampling, Log,
         TEXT("泊松采样验证通过：%d 个点，最小距离约束满足 (>= %.2f)"),
         NumPoints, MinAllowedDistance);

  return true;
}

// ============================================================================
// 通用阵型生成器
// ============================================================================

TArray<FVector> UPointSamplingLibrary::GenerateFormation(
    EPointSamplingMode Mode, int32 PointCount, FVector CenterLocation,
    FRotator Rotation, EPoissonCoordinateSpace CoordinateSpace, float Spacing,
    float JitterStrength, int32 RandomSeed, float Param1, float Param2,
    int32 Param3) {
  // 输入验证
  if (PointCount <= 0) {
    return TArray<FVector>();
  }

  // 根据阵型模式分发到具体的生成器
  switch (Mode) {
  // 矩形类阵型
  case EPointSamplingMode::SolidRectangle:
    return UFormationSamplingLibrary::GenerateSolidRectangle(
        PointCount, CenterLocation, Rotation, Spacing,
        FMath::Max(1, static_cast<int32>(Param1)), // RowCount
        FMath::Max(1, static_cast<int32>(Param2)), // ColumnCount
        1.0f, CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::HollowRectangle:
    return UFormationSamplingLibrary::GenerateHollowRectangle(
        PointCount, CenterLocation, Rotation, Spacing,
        FMath::Max(1, static_cast<int32>(Param1)), // RowCount
        FMath::Max(1, static_cast<int32>(Param2)), // ColumnCount
        1.0f, CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::SpiralRectangle:
    return UFormationSamplingLibrary::GenerateSpiralRectangle(
        PointCount, CenterLocation, Rotation, Spacing,
        FMath::Max(1.0f, Param1), // SpiralTurns
        1.0f, CoordinateSpace, JitterStrength, RandomSeed);

  // 三角形类阵型
  case EPointSamplingMode::SolidTriangle:
    return UFormationSamplingLibrary::GenerateSolidTriangle(
        PointCount, CenterLocation, Rotation, Spacing,
        Param3 > 0, // bInverted
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::HollowTriangle:
    return UFormationSamplingLibrary::GenerateHollowTriangle(
        PointCount, CenterLocation, Rotation, Spacing,
        Param3 > 0, // bInverted
        CoordinateSpace, JitterStrength, RandomSeed);

  // 圆形类阵型
  case EPointSamplingMode::Circle:
    return UFormationSamplingLibrary::GenerateCircle(
        PointCount, CenterLocation, Rotation,
        FMath::Max(10.0f, Spacing), // Radius
        false,                      // bIs3D
        static_cast<ECircleDistributionMode>(
            FMath::Clamp(Param3, 0, 2)), // DistributionMode
        FMath::Max(1.0f, Param1),        // MinDistance
        Param2,                          // StartAngle
        true,                            // bClockwise
        CoordinateSpace, JitterStrength, RandomSeed);

  // 雪花类阵型
  case EPointSamplingMode::Snowflake:
    return UFormationSamplingLibrary::GenerateSnowflake(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing),                // Radius
        FMath::Max(1, static_cast<int32>(Param1)), // SnowflakeLayers
        FMath::Max(50.0f, Param2),                 // Spacing
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::SnowflakeArc:
    return UFormationSamplingLibrary::GenerateSnowflakeArc(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing),                // Radius
        FMath::Max(1, static_cast<int32>(Param1)), // SnowflakeLayers
        FMath::Max(25.0f, Param2),                 // Spacing
        FMath::Clamp(Param1, 1.0f, 360.0f),        // ArcAngle
        0.0f,                                      // StartAngle
        CoordinateSpace, JitterStrength, RandomSeed);

  // 军事阵型
  case EPointSamplingMode::Wedge:
    return UFormationSamplingLibrary::GenerateWedgeFormation(
        PointCount, CenterLocation, Rotation, Spacing,
        FMath::Clamp(Param1, 10.0f, 90.0f), // WedgeAngle
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::Column:
    return UFormationSamplingLibrary::GenerateColumnFormation(
        PointCount, CenterLocation, Rotation, Spacing, CoordinateSpace,
        JitterStrength, RandomSeed);

  case EPointSamplingMode::Line:
    return UFormationSamplingLibrary::GenerateLineFormation(
        PointCount, CenterLocation, Rotation, Spacing, CoordinateSpace,
        JitterStrength, RandomSeed);

  case EPointSamplingMode::Vee:
    return UFormationSamplingLibrary::GenerateVeeFormation(
        PointCount, CenterLocation, Rotation, Spacing,
        FMath::Clamp(Param1, 10.0f, 90.0f), // VeeAngle
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::Echelon:
  case EPointSamplingMode::EchelonLeft:
  case EPointSamplingMode::EchelonRight: {
    const int32 Direction = (Mode == EPointSamplingMode::EchelonLeft) ? -1
                            : (Mode == EPointSamplingMode::EchelonRight)
                                ? 1
                                : Param3;
    return UFormationSamplingLibrary::GenerateEchelonFormation(
        PointCount, CenterLocation, Rotation, Spacing, Direction,
        FMath::Clamp(Param1, 5.0f, 45.0f), // EchelonAngle
        CoordinateSpace, JitterStrength, RandomSeed);
  }

  // 几何阵型
  case EPointSamplingMode::HexagonalGrid:
    return UFormationSamplingLibrary::GenerateHexagonalGrid(
        PointCount, CenterLocation, Rotation, Spacing,
        FMath::Max(1, static_cast<int32>(Param1)), // Rings
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::Star:
    return UFormationSamplingLibrary::GenerateStarFormation(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing),  // OuterRadius
        FMath::Max(25.0f, Param1),   // InnerRadius
        FMath::Clamp(Param3, 3, 12), // PointsCount
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::ArchimedeanSpiral:
    return UFormationSamplingLibrary::GenerateArchimedeanSpiral(
        PointCount, CenterLocation, Rotation,
        FMath::Max(5.0f, Spacing), // Spacing
        FMath::Max(1.0f, Param1),  // Turns
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::LogarithmicSpiral:
    return UFormationSamplingLibrary::GenerateLogarithmicSpiral(
        PointCount, CenterLocation, Rotation,
        FMath::Clamp(Param1, 1.01f, 2.0f), // GrowthFactor
        FMath::Clamp(Param2, 5.0f, 45.0f), // AngleStep
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::Heart:
    return UFormationSamplingLibrary::GenerateHeartFormation(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing), // Size
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::Flower:
    return UFormationSamplingLibrary::GenerateFlowerFormation(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing),  // OuterRadius
        FMath::Max(10.0f, Param1),   // InnerRadius
        FMath::Clamp(Param3, 3, 12), // PetalCount
        CoordinateSpace, JitterStrength, RandomSeed);

  // 高级圆形阵型
  case EPointSamplingMode::GoldenSpiral:
    return UFormationSamplingLibrary::GenerateGoldenSpiralFormation(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing), // MaxRadius
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::CircularGrid:
    return UFormationSamplingLibrary::GenerateCircularGridFormation(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing),                // MaxRadius
        FMath::Max(1, static_cast<int32>(Param1)), // RadialDivisions
        FMath::Max(1, static_cast<int32>(Param2)), // AngularDivisions
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::RoseCurve:
    return UFormationSamplingLibrary::GenerateRoseCurveFormation(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing),  // MaxRadius
        FMath::Clamp(Param3, 1, 12), // Petals
        CoordinateSpace, JitterStrength, RandomSeed);

  case EPointSamplingMode::ConcentricRings: {
    // 使用默认的每环点数配置
    TArray<int32> DefaultPointsPerRing = {6, 12, 18, 24};
    return UFormationSamplingLibrary::GenerateConcentricRingsFormation(
        PointCount, CenterLocation, Rotation,
        FMath::Max(50.0f, Spacing),                // MaxRadius
        FMath::Max(1, static_cast<int32>(Param1)), // RingCount
        DefaultPointsPerRing,                      // PointsPerRing
        CoordinateSpace, JitterStrength, RandomSeed);
  }

  // 不支持的模式
  default:
    UE_LOG(LogPointSampling, Warning,
           TEXT("[通用阵型生成器] 不支持的阵型模式: %d"),
           static_cast<int32>(Mode));
    return TArray<FVector>();
  }
}

float UPointSamplingLibrary::CalculateDistributionUniformity(
    const TArray<FVector> &Points) {
  if (Points.Num() < 3) {
    return 1.0f; // 少于3个点认为是完全均匀的
  }

  // 计算质心
  FVector Centroid = FVector::ZeroVector;
  for (const FVector &Point : Points) {
    Centroid += Point;
  }
  Centroid /= Points.Num();

  // 计算每个点到质心的距离
  TArray<float> DistancesToCentroid;
  DistancesToCentroid.Reserve(Points.Num());

  float AvgDistance = 0.0f;
  for (const FVector &Point : Points) {
    float Distance = FVector::Dist(Point, Centroid);
    DistancesToCentroid.Add(Distance);
    AvgDistance += Distance;
  }
  AvgDistance /= Points.Num();

  // 计算距离质心的标准差（越小表示分布越均匀）
  float Variance = 0.0f;
  for (float Distance : DistancesToCentroid) {
    float Diff = Distance - AvgDistance;
    Variance += Diff * Diff;
  }
  Variance /= Points.Num();

  float StdDev = FMath::Sqrt(Variance);

  // 标准化均匀性分数 (0-1, 1表示完全均匀)
  const float MaxExpectedStdDev = AvgDistance * 0.5f; // 经验值
  float UniformityScore =
      1.0f - FMath::Clamp(StdDev / MaxExpectedStdDev, 0.0f, 1.0f);

  return UniformityScore;
}

// ============================================================================
// 纹理采样（智能统一接口）
// ============================================================================

TArray<FVector> UPointSamplingLibrary::GeneratePointsFromTexture(
    UTexture2D *Texture, int32 MaxSampleSize, float Spacing,
    float PixelThreshold, float TextureScale,
    ETextureSamplingChannel SamplingChannel) {
  // 智能纹理采样（Grid算法）
  return FTextureSamplingHelper::GenerateFromTextureAuto(
      Texture, MaxSampleSize, Spacing, PixelThreshold, TextureScale,
      SamplingChannel);
}

TArray<FVector> UPointSamplingLibrary::GeneratePointsFromTextureWithPoisson(
    UTexture2D *Texture, int32 MaxSampleSize, float MinRadius, float MaxRadius,
    float PixelThreshold, float TextureScale,
    ETextureSamplingChannel SamplingChannel, int32 MaxAttempts) {
  return FTextureSamplingHelper::GenerateFromTextureAutoWithPoisson(
      Texture, MaxSampleSize, MinRadius, MaxRadius, PixelThreshold,
      TextureScale, SamplingChannel, MaxAttempts);
}
