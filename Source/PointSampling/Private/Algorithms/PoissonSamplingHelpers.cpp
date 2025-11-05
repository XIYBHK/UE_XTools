/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "Algorithms/PoissonSamplingHelpers.h"
#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"
#include "Math/RandomStream.h"
#include "PointSamplingTypes.h"

// ============================================================================
// 泊松圆盘采样辅助函数实现
// ============================================================================

namespace PoissonSamplingHelpers
{
    /**
     * 根据目标点数计算合适的Radius
     */
    float CalculateRadiusFromTargetCount(int32 TargetPointCount, float Width, float Height, float Depth, bool bIs2DPlane)
    {
        if (TargetPointCount <= 0)
        {
            return -1.0f;
        }
        
        if (bIs2DPlane)
        {
            // 2D情况：根据面积估算Radius
            const float Area = Width * Height;
            // 泊松采样实际密度约为理论值的2-3倍
            // 使用1.8倍系数，使生成点数接近目标（略多10-20%，便于裁剪选优）
            return FMath::Sqrt(1.8f * Area / (TargetPointCount * PI));
        }
        else
        {
            // 3D情况：根据体积估算Radius
            const float Volume = Width * Height * Depth;
            // 泊松采样实际密度约为理论值的2-3倍
            // 使用1.0倍系数，使生成点数接近目标
            return FMath::Pow(1.0f * Volume / (TargetPointCount * PI), 1.0f / 3.0f);
        }
    }
    
    /**
     * 找到点的最近邻距离（优化版：使用平方距离）
     */
    float FindNearestDistanceSquared(const FVector& Point, const TArray<FVector>& Points, int32 ExcludeIndex)
    {
        float MinDistSq = FLT_MAX;
        
        for (int32 i = 0; i < Points.Num(); ++i)
        {
            if (i == ExcludeIndex) continue;
            
            const float DistSq = FVector::DistSquared(Point, Points[i]);
            if (DistSq < MinDistSq)
            {
                MinDistSq = DistSq;
            }
        }
        
        return MinDistSq;
    }
    
    /**
     * 智能裁剪：移除最拥挤的点，保持最优分布
     * @param Points 要裁剪的点数组
     * @param TargetCount 目标数量
     * 
     * 优化版本：批量裁剪，从O(K × N²)降低到O(N² + N log N)
     * - 一次性计算所有点的最近邻距离 O(N²)
     * - 排序 O(N log N)
     * - 批量移除 O(N)
     * 
     * 大规模裁剪时性能提升10-100倍
     */
    void TrimToOptimalDistribution(TArray<FVector>& Points, int32 TargetCount)
    {
        if (Points.Num() <= TargetCount)
        {
            return;
        }
        
        const int32 ToRemove = Points.Num() - TargetCount;
        
        // 小规模裁剪（<10个点）：使用原有的逐个移除算法
        // 原因：小规模时批量算法的开销反而更大
        if (ToRemove < 10)
        {
            while (Points.Num() > TargetCount)
            {
                int32 MostCrowdedIndex = 0;
                float MinNearestDistSq = FLT_MAX;
                
                for (int32 i = 0; i < Points.Num(); ++i)
                {
                    const float NearestDistSq = FindNearestDistanceSquared(Points[i], Points, i);
                    if (NearestDistSq < MinNearestDistSq)
                    {
                        MinNearestDistSq = NearestDistSq;
                        MostCrowdedIndex = i;
                    }
                }
                
                Points.RemoveAtSwap(MostCrowdedIndex);
            }
            return;
        }
        
        // 大规模裁剪：批量移除算法
        
        // 1. 计算所有点的最近邻距离（一次O(N²)）
        TArray<TPair<float, int32>> DistanceIndexPairs;
        DistanceIndexPairs.Reserve(Points.Num());
        
        for (int32 i = 0; i < Points.Num(); ++i)
        {
            const float NearestDistSq = FindNearestDistanceSquared(Points[i], Points, i);
            DistanceIndexPairs.Add(TPair<float, int32>(NearestDistSq, i));
        }
        
        // 2. 排序：最拥挤的点在前面（O(N log N)）
        DistanceIndexPairs.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) {
            return A.Key < B.Key;
        });
        
        // 3. 批量移除前ToRemove个最拥挤的点（O(N)）
        TSet<int32> IndicesToRemove;
        IndicesToRemove.Reserve(ToRemove);
        
        for (int32 i = 0; i < ToRemove; ++i)
        {
            IndicesToRemove.Add(DistanceIndexPairs[i].Value);
        }
        
        // 4. 构建新数组，只保留未被标记的点
        TArray<FVector> NewPoints;
        NewPoints.Reserve(TargetCount);
        
        for (int32 i = 0; i < Points.Num(); ++i)
        {
            if (!IndicesToRemove.Contains(i))
            {
                NewPoints.Add(Points[i]);
            }
        }
        
        // 5. 移动语义转移，避免拷贝
        Points = MoveTemp(NewPoints);
        
        UE_LOG(LogPointSampling, Verbose, TEXT("批量裁剪: 从 %d 移除 %d 个最拥挤点，保留 %d"), 
            Points.Num() + ToRemove, ToRemove, Points.Num());
    }
    
    /**
     * 分层网格填充：用均匀分层采样补充不足的点
     * @param Points 已有的点数组（会原地追加）
     * @param TargetCount 目标总数量
     * @param BoxSize 采样空间大小（完整尺寸，非半尺寸）
     * @param MinDist 最小距离约束（放宽的半径）
     * @param bIs2D 是否为2D平面
     * @param Stream 可选的随机流（const指针）
     *  const指针：FRandomStream的方法是const但使用mutable成员
     * 
     * 优化版本：使用空间哈希加速距离检查，从O(N × M)降低到O(N + M)
     * - 构建空间哈希表 O(N)
     * - 每个新点只检查邻近单元格（27个）O(1)
     * - 总体复杂度从O(N × M)降低到O(N + M × 27) ≈ O(N + M)
     */
    void FillWithStratifiedSampling(
        TArray<FVector>& Points,
        int32 TargetCount,
        FVector BoxSize,
        float MinDist,
        bool bIs2D,
        const FRandomStream* Stream)
    {
        const int32 Needed = TargetCount - Points.Num();
        if (Needed <= 0) return;
        
        // 计算网格大小
        int32 GridSize;
        if (bIs2D)
        {
            GridSize = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Needed)));
        }
        else
        {
            GridSize = FMath::CeilToInt(FMath::Pow(static_cast<float>(Needed), 1.0f / 3.0f));
        }
        
        // 计算单元格大小
        const FVector CellSize = BoxSize / FMath::Max(GridSize, 1);
        const float MinDistSq = MinDist * MinDist;
        
        // 构建空间哈希：用于快速邻域查找
        const float HashCellSize = MinDist;  // 哈希单元格大小等于最小距离
        
        auto GetHashKey = [HashCellSize](const FVector& P) -> FIntVector
        {
            return FIntVector(
                FMath::FloorToInt(P.X / HashCellSize),
                FMath::FloorToInt(P.Y / HashCellSize),
                FMath::FloorToInt(P.Z / HashCellSize)
            );
        };
        
        // Lambda：检查点是否与空间哈希中的点冲突
        auto IsValidAgainstHash = [&](const FVector& NewPoint, const TMap<FIntVector, TArray<FVector>>& SpatialHash) -> bool
        {
            const FIntVector CellKey = GetHashKey(NewPoint);
            
            // 检查3×3×3=27个邻近单元格
            for (int32 dx = -1; dx <= 1; ++dx)
            {
                for (int32 dy = -1; dy <= 1; ++dy)
                {
                    for (int32 dz = -1; dz <= 1; ++dz)
                    {
                        const FIntVector NeighborKey = CellKey + FIntVector(dx, dy, dz);
                        
                        if (const TArray<FVector>* Neighbors = SpatialHash.Find(NeighborKey))
                        {
                            for (const FVector& Neighbor : *Neighbors)
                            {
                                if (FVector::DistSquared(NewPoint, Neighbor) < MinDistSq)
                                {
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
            
            return true;
        };
        
        // 1. 将已有的泊松点加入空间哈希
        TMap<FIntVector, TArray<FVector>> SpatialHash;
        SpatialHash.Reserve(Points.Num() / 4 + Needed / 4);  // 估算容量
        
        for (const FVector& P : Points)
        {
            SpatialHash.FindOrAdd(GetHashKey(P)).Add(P);
        }
        
        // 2. 在网格中生成候选点（使用空间哈希加速）
        TArray<FVector> CandidatePoints;
        CandidatePoints.Reserve(Needed * 2);
        
        for (int32 i = 0; i < Needed * 2 && CandidatePoints.Num() < Needed * 2; ++i)
        {
            // 计算网格索引
            const int32 x = i % GridSize;
            const int32 y = (i / GridSize) % GridSize;
            const int32 z = bIs2D ? 0 : (i / (GridSize * GridSize)) % GridSize;
            
            // 在网格单元内随机位置
            FVector CellMin = FVector(x, y, z) * CellSize;
            
            float RandX, RandY, RandZ;
            if (Stream)
            {
                RandX = Stream->FRand();
                RandY = Stream->FRand();
                RandZ = bIs2D ? 0.0f : Stream->FRand();
            }
            else
            {
                RandX = FMath::FRand();
                RandY = FMath::FRand();
                RandZ = bIs2D ? 0.0f : FMath::FRand();
            }
            
            FVector NewPoint = CellMin + FVector(
                RandX * CellSize.X,
                RandY * CellSize.Y,
                RandZ * CellSize.Z
            );
            
            // 转换到局部空间（中心对齐）
            NewPoint -= BoxSize * 0.5f;
            if (bIs2D) NewPoint.Z = 0.0f;
            
            // 使用空间哈希检查有效性（O(1)而非O(N)）
            if (IsValidAgainstHash(NewPoint, SpatialHash))
            {
                CandidatePoints.Add(NewPoint);
                SpatialHash.FindOrAdd(GetHashKey(NewPoint)).Add(NewPoint);
            }
        }
        
        // 3. 如果候选点不够，降低距离约束继续生成（使用空间哈希）
        if (CandidatePoints.Num() < Needed)
        {
            const float RelaxedMinDistSq = MinDistSq * 0.5f;
            
            // Lambda：检查放宽距离约束的有效性
            auto IsValidWithRelaxedDistance = [&](const FVector& NewPoint, float CheckDistSq) -> bool
            {
                const FIntVector CellKey = GetHashKey(NewPoint);
                
                for (int32 dx = -1; dx <= 1; ++dx)
                {
                    for (int32 dy = -1; dy <= 1; ++dy)
                    {
                        for (int32 dz = -1; dz <= 1; ++dz)
                        {
                            const FIntVector NeighborKey = CellKey + FIntVector(dx, dy, dz);
                            
                            if (const TArray<FVector>* Neighbors = SpatialHash.Find(NeighborKey))
                            {
                                for (const FVector& Neighbor : *Neighbors)
                                {
                                    if (FVector::DistSquared(NewPoint, Neighbor) < CheckDistSq)
                                    {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                }
                
                return true;
            };
            
            for (int32 i = 0; i < Needed * 2 && CandidatePoints.Num() < Needed; ++i)
            {
                FVector RandomPoint;
                if (Stream)
                {
                    RandomPoint = FVector(
                        Stream->FRandRange(-BoxSize.X * 0.5f, BoxSize.X * 0.5f),
                        Stream->FRandRange(-BoxSize.Y * 0.5f, BoxSize.Y * 0.5f),
                        bIs2D ? 0.0f : Stream->FRandRange(-BoxSize.Z * 0.5f, BoxSize.Z * 0.5f)
                    );
                }
                else
                {
                    RandomPoint = FVector(
                        FMath::FRandRange(-BoxSize.X * 0.5f, BoxSize.X * 0.5f),
                        FMath::FRandRange(-BoxSize.Y * 0.5f, BoxSize.Y * 0.5f),
                        bIs2D ? 0.0f : FMath::FRandRange(-BoxSize.Z * 0.5f, BoxSize.Z * 0.5f)
                    );
                }
                
                if (IsValidWithRelaxedDistance(RandomPoint, RelaxedMinDistSq))
                {
                    CandidatePoints.Add(RandomPoint);
                    SpatialHash.FindOrAdd(GetHashKey(RandomPoint)).Add(RandomPoint);
                }
            }
        }
        
        // 4. 如果还不够，继续填充（保留极小距离约束，避免完全重叠）
        if (CandidatePoints.Num() < Needed)
        {
            const float MinimalDistSq = MinDistSq * 0.25f;
            const int32 MaxAttempts = Needed * 10;
            int32 Attempts = 0;
            
            // Lambda：检查极小距离约束
            auto IsValidWithMinimalDistance = [&](const FVector& NewPoint) -> bool
            {
                const FIntVector CellKey = GetHashKey(NewPoint);
                
                for (int32 dx = -1; dx <= 1; ++dx)
                {
                    for (int32 dy = -1; dy <= 1; ++dy)
                    {
                        for (int32 dz = -1; dz <= 1; ++dz)
                        {
                            const FIntVector NeighborKey = CellKey + FIntVector(dx, dy, dz);
                            
                            if (const TArray<FVector>* Neighbors = SpatialHash.Find(NeighborKey))
                            {
                                for (const FVector& Neighbor : *Neighbors)
                                {
                                    if (FVector::DistSquared(NewPoint, Neighbor) < MinimalDistSq)
                                    {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                }
                
                return true;
            };
            
            while (CandidatePoints.Num() < Needed && Attempts < MaxAttempts)
            {
                Attempts++;
                
                FVector RandomPoint;
                if (Stream)
                {
                    RandomPoint = FVector(
                        Stream->FRandRange(-BoxSize.X * 0.5f, BoxSize.X * 0.5f),
                        Stream->FRandRange(-BoxSize.Y * 0.5f, BoxSize.Y * 0.5f),
                        bIs2D ? 0.0f : Stream->FRandRange(-BoxSize.Z * 0.5f, BoxSize.Z * 0.5f)
                    );
                }
                else
                {
                    RandomPoint = FVector(
                        FMath::FRandRange(-BoxSize.X * 0.5f, BoxSize.X * 0.5f),
                        FMath::FRandRange(-BoxSize.Y * 0.5f, BoxSize.Y * 0.5f),
                        bIs2D ? 0.0f : FMath::FRandRange(-BoxSize.Z * 0.5f, BoxSize.Z * 0.5f)
                    );
                }
                
                if (IsValidWithMinimalDistance(RandomPoint))
                {
                    CandidatePoints.Add(RandomPoint);
                    SpatialHash.FindOrAdd(GetHashKey(RandomPoint)).Add(RandomPoint);
                }
            }
            
            // 如果尝试次数耗尽仍不够，记录警告
            if (CandidatePoints.Num() < Needed)
            {
                UE_LOG(LogPointSampling, Warning, 
                    TEXT("泊松采样: 空间过小，无法在保持最小距离的前提下生成 %d 个点，实际补充 %d 个（已有泊松点 %d 个）"),
                    Needed, CandidatePoints.Num(), Points.Num());
            }
        }
        
        // 随机选择需要的点数追加到原数组
        if (Stream)
        {
            // 使用流进行Fisher-Yates洗牌
            for (int32 i = CandidatePoints.Num() - 1; i > 0; --i)
            {
                const int32 j = Stream->RandRange(0, i);
                CandidatePoints.Swap(i, j);
            }
        }
        else
        {
            // 使用FMath进行Fisher-Yates洗牌
            for (int32 i = CandidatePoints.Num() - 1; i > 0; --i)
            {
                const int32 j = FMath::RandRange(0, i);
                CandidatePoints.Swap(i, j);
            }
        }
        
        // 追加到原数组
        for (int32 i = 0; i < Needed && i < CandidatePoints.Num(); ++i)
        {
            Points.Add(CandidatePoints[i]);
        }
    }
    
    /**
     * 智能调整点数到目标数量（混合策略）
     * @param Points 点数组
     * @param TargetCount 目标数量
     * @param BoxSize 采样空间大小（完整尺寸）
     * @param Radius 参考半径
     * @param bIs2D 是否为2D平面
     * @param Stream 可选的随机流（const指针）
     *  const指针：FRandomStream的方法是const但使用mutable成员
     */
    void AdjustToTargetCount(
        TArray<FVector>& Points,
        int32 TargetCount,
        FVector BoxSize,
        float Radius,
        bool bIs2D,
        const FRandomStream* Stream)
    {
        if (TargetCount <= 0) return;
        
        const int32 CurrentCount = Points.Num();
        
        if (CurrentCount == TargetCount)
        {
            // 完美匹配，无需调整
            return;
        }
        else if (CurrentCount > TargetCount)
        {
            // 点太多：智能裁剪（保留最分散的点）
            UE_LOG(LogPointSampling, Log, TEXT("泊松采样: 从 %d 个点智能裁剪到 %d（移除拥挤点）"), 
                CurrentCount, TargetCount);
            
            TrimToOptimalDistribution(Points, TargetCount);
        }
        else
        {
            // 点不足：分层网格补充
            const float RelaxedRadius = Radius * 0.6f;  // 放宽到60%
            
            UE_LOG(LogPointSampling, Log, TEXT("泊松采样: 从 %d 个点补充到 %d（分层网格填充，距离约束=%.1f）"), 
                CurrentCount, TargetCount, RelaxedRadius);
            
            FillWithStratifiedSampling(Points, TargetCount, BoxSize, RelaxedRadius, bIs2D, Stream);
        }
    }
    
    /**
     * 应用扰动噪波到点集
     * @param Points 要扰动的点数组
     * @param Radius 参考半径（扰动范围基于此值）
     * @param JitterStrength 扰动强度 0-1（0=无扰动，1=最大扰动）
     * @param Stream 可选的随机流（const指针）
     *  const指针：FRandomStream的方法是const但使用mutable成员
     */
    void ApplyJitter(TArray<FVector>& Points, float Radius, float JitterStrength, const FRandomStream* Stream)
    {
        if (JitterStrength <= 0.0f || Points.Num() == 0)
        {
            return;
        }
        
        // 扰动范围：最大为半径的50%，由强度控制
        const float MaxJitter = Radius * FMath::Clamp(JitterStrength, 0.0f, 1.0f) * 0.5f;
        
        for (FVector& Point : Points)
        {
            if (Stream)
            {
                Point.X += Stream->FRandRange(-MaxJitter, MaxJitter);
                Point.Y += Stream->FRandRange(-MaxJitter, MaxJitter);
                Point.Z += Stream->FRandRange(-MaxJitter, MaxJitter);
            }
            else
            {
                Point.X += FMath::FRandRange(-MaxJitter, MaxJitter);
                Point.Y += FMath::FRandRange(-MaxJitter, MaxJitter);
                Point.Z += FMath::FRandRange(-MaxJitter, MaxJitter);
            }
        }
    }
    
    /**
     * 应用Transform变换
     * @param CoordinateSpace 坐标空间类型：
     *        - World：世界坐标（应用位置+旋转到世界空间）
     *        - Local：局部坐标（仅应用旋转，位置相对于Box中心）
     *        - Raw：原始坐标（完全不变换，供用户自行处理）
     * @param ScaleCompensation 缩放补偿（保留接口兼容性，未使用）
     */
    void ApplyTransform(TArray<FVector>& Points, const FTransform& Transform, EPoissonCoordinateSpace CoordinateSpace, const FVector& ScaleCompensation)
    {
        switch (CoordinateSpace)
        {
            case EPoissonCoordinateSpace::World:
            {
                // 世界空间：应用位置+旋转（不应用缩放，因为Extent已包含）
                FTransform TransformNoScale = Transform;
                TransformNoScale.SetScale3D(FVector::OneVector);
                
                for (FVector& Point : Points)
                {
                    Point = TransformNoScale.TransformPosition(Point);
                }
                break;
            }
            
            case EPoissonCoordinateSpace::Local:
            {
                // 局部空间：应用缩放补偿，输出相对于Box中心的点
                // 适用场景：AddInstance(World Space = false)
                // - 使用ScaledBoxExtent采样（保证点数随缩放增加）
                // - 输出时除以父缩放（避免AddInstance的双重缩放）
                // - 旋转由HISMC的父变换自动处理
                const FVector ParentScale = Transform.GetScale3D();
                
                for (FVector& Point : Points)
                {
                    // 除以父缩放，补偿AddInstance会再次应用的缩放
                    Point.X /= ParentScale.X;
                    Point.Y /= ParentScale.Y;
                    Point.Z /= ParentScale.Z;
                }
                break;
            }
            
            case EPoissonCoordinateSpace::Raw:
            {
                // 原始空间：应用缩放补偿（与Local相同的逻辑）
                // 点相对于Box中心，未旋转，已补偿缩放
                // 供用户自行处理坐标变换
                const FVector ParentScale = Transform.GetScale3D();
                
                for (FVector& Point : Points)
                {
                    Point.X /= ParentScale.X;
                    Point.Y /= ParentScale.Y;
                    Point.Z /= ParentScale.Z;
                }
                break;
            }
            
            default:
            {
                checkNoEntry();  // 不应到达此处
                break;
            }
        }
    }

    /**
     * 在给定点周围生成随机点（2D）
     * @param Stream 可选的随机流，nullptr时使用全局随机
     *  const指针：FRandomStream的方法是const但使用mutable成员
     */
    FVector2D GenerateRandomPointAround2D(const FVector2D& Point, float MinDist, float MaxDist, const FRandomStream* Stream)
    {
        const float Angle = Stream ? Stream->FRandRange(0.0f, 2.0f * PI) : FMath::FRandRange(0.0f, 2.0f * PI);
        const float Distance = Stream ? Stream->FRandRange(MinDist, MaxDist) : FMath::FRandRange(MinDist, MaxDist);
        return FVector2D(
            Point.X + Distance * FMath::Cos(Angle),
            Point.Y + Distance * FMath::Sin(Angle)
        );
    }

    /**
     * 在给定点周围生成随机点（3D）
     * @param Stream 可选的随机流，nullptr时使用全局随机
     *  const指针：FRandomStream的方法是const但使用mutable成员
     */
    FVector GenerateRandomPointAround3D(const FVector& Point, float MinDist, float MaxDist, const FRandomStream* Stream)
    {
        // 在球面上均匀采样
        const float Theta = Stream ? Stream->FRandRange(0.0f, 2.0f * PI) : FMath::FRandRange(0.0f, 2.0f * PI);
        const float Phi = FMath::Acos(Stream ? Stream->FRandRange(-1.0f, 1.0f) : FMath::FRandRange(-1.0f, 1.0f));
        const float Distance = Stream ? Stream->FRandRange(MinDist, MaxDist) : FMath::FRandRange(MinDist, MaxDist);
        
        return FVector(
            Point.X + Distance * FMath::Sin(Phi) * FMath::Cos(Theta),
            Point.Y + Distance * FMath::Sin(Phi) * FMath::Sin(Theta),
            Point.Z + Distance * FMath::Cos(Phi)
        );
    }

    /**
     * 检查点是否有效（2D）- 优化版：使用平方距离避免开方
     */
    bool IsValidPoint2D(
        const FVector2D& Point,
        float Radius,
        float Width,
        float Height,
        const TArray<FVector2D>& Grid,
        int32 GridWidth,
        int32 GridHeight,
        float CellSize)
    {
        // 边界检查
        if (Point.X < 0 || Point.X >= Width || Point.Y < 0 || Point.Y >= Height)
        {
            return false;
        }

        // 计算网格坐标
        const int32 CellX = FMath::FloorToInt(Point.X / CellSize);
        const int32 CellY = FMath::FloorToInt(Point.Y / CellSize);

        // 检查周围的网格单元
        const int32 SearchStartX = FMath::Max(CellX - 2, 0);
        const int32 SearchStartY = FMath::Max(CellY - 2, 0);
        const int32 SearchEndX = FMath::Min(CellX + 2, GridWidth - 1);
        const int32 SearchEndY = FMath::Min(CellY + 2, GridHeight - 1);

        // 使用平方距离比较，避免开方运算
        const float RadiusSquared = Radius * Radius;

        for (int32 x = SearchStartX; x <= SearchEndX; ++x)
        {
            for (int32 y = SearchStartY; y <= SearchEndY; ++y)
            {
                const FVector2D& Neighbor = Grid[y * GridWidth + x];
                if (Neighbor != FVector2D::ZeroVector)
                {
                    const float DistSquared = FVector2D::DistSquared(Point, Neighbor);
                    if (DistSquared < RadiusSquared)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    /**
     * 检查点是否有效（3D）- 优化版：使用平方距离避免开方
     */
    bool IsValidPoint3D(
        const FVector& Point,
        float Radius,
        float Width,
        float Height,
        float Depth,
        const TArray<FVector>& Grid,
        int32 GridWidth,
        int32 GridHeight,
        int32 GridDepth,
        float CellSize)
    {
        // 边界检查
        if (Point.X < 0 || Point.X >= Width ||
            Point.Y < 0 || Point.Y >= Height ||
            Point.Z < 0 || Point.Z >= Depth)
        {
            return false;
        }

        // 计算网格坐标
        const int32 CellX = FMath::FloorToInt(Point.X / CellSize);
        const int32 CellY = FMath::FloorToInt(Point.Y / CellSize);
        const int32 CellZ = FMath::FloorToInt(Point.Z / CellSize);

        // 检查周围的网格单元
        const int32 SearchStartX = FMath::Max(CellX - 2, 0);
        const int32 SearchStartY = FMath::Max(CellY - 2, 0);
        const int32 SearchStartZ = FMath::Max(CellZ - 2, 0);
        const int32 SearchEndX = FMath::Min(CellX + 2, GridWidth - 1);
        const int32 SearchEndY = FMath::Min(CellY + 2, GridHeight - 1);
        const int32 SearchEndZ = FMath::Min(CellZ + 2, GridDepth - 1);

        // 使用平方距离比较，避免开方运算
        const float RadiusSquared = Radius * Radius;

        for (int32 x = SearchStartX; x <= SearchEndX; ++x)
        {
            for (int32 y = SearchStartY; y <= SearchEndY; ++y)
            {
                for (int32 z = SearchStartZ; z <= SearchEndZ; ++z)
                {
                    const int32 Index = z * (GridWidth * GridHeight) + y * GridWidth + x;
                    const FVector& Neighbor = Grid[Index];
                    if (Neighbor != FVector::ZeroVector)
                    {
                        const float DistSquared = FVector::DistSquared(Point, Neighbor);
                        if (DistSquared < RadiusSquared)
                        {
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }
}
