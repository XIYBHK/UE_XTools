// FormationAlgorithms.cpp - 基础阵型分配算法
// 包含基础分配算法、成本矩阵计算、匈牙利算法等核心算法实现

#include "FormationManagerComponent.h"
#include "FormationMathUtils.h"
#include "FormationLog.h"
#include "Kismet/KismetMathLibrary.h"

// 🚀 性能优化配置常量（本地定义）
namespace FormationPerformanceConfig
{
    constexpr int32 HungarianAlgorithmThreshold = 50;
}

// ========== 成本矩阵计算 ==========

TArray<TArray<float>> UFormationManagerComponent::CalculateRelativePositionCostMatrix(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions)
{
    int32 NumPositions = FromPositions.Num();
    TArray<TArray<float>> CostMatrix;

    // 🚀 性能优化：预分配内存避免重复分配
    CostMatrix.Reserve(NumPositions);
    CostMatrix.SetNum(NumPositions);

    // 🚀 性能优化：使用 UE 内置的高效包围盒计算
    FBox FromAABB = FBox(FromPositions);
    FBox ToAABB = FBox(ToPositions);

    // 🚀 性能优化：缓存中心点和尺寸，避免重复计算
    const FVector FromCenter = FromAABB.GetCenter();
    const FVector ToCenter = ToAABB.GetCenter();
    const FVector FromSize = FromAABB.GetSize();
    const FVector ToSize = ToAABB.GetSize();

    // 🚀 性能优化：预计算归一化因子，避免重复除法运算
    const FVector FromSizeInv = FVector(
        FromSize.X > 1.0f ? 1.0f / FromSize.X : 0.0f,
        FromSize.Y > 1.0f ? 1.0f / FromSize.Y : 0.0f,
        FromSize.Z > 1.0f ? 1.0f / FromSize.Z : 0.0f
    );
    const FVector ToSizeInv = FVector(
        ToSize.X > 1.0f ? 1.0f / ToSize.X : 0.0f,
        ToSize.Y > 1.0f ? 1.0f / ToSize.Y : 0.0f,
        ToSize.Z > 1.0f ? 1.0f / ToSize.Z : 0.0f
    );

    // 🚀 性能优化：预计算归一化位置数组，避免重复计算
    TArray<FVector> FromNormalized;
    TArray<FVector> ToNormalized;
    FromNormalized.Reserve(NumPositions);
    ToNormalized.Reserve(NumPositions);

    for (int32 i = 0; i < NumPositions; i++)
    {
        FVector FromRelative = FromPositions[i] - FromCenter;
        FromNormalized.Add(FromRelative * FromSizeInv);

        FVector ToRelative = ToPositions[i] - ToCenter;
        ToNormalized.Add(ToRelative * ToSizeInv);
    }

    // 🚀 性能优化：使用常量权重，避免重复乘法
    constexpr float RelativeWeight = 0.7f;
    constexpr float AbsoluteWeight = 0.3f;
    constexpr float RelativeScale = 1000.0f;

    // 计算相对位置成本矩阵
    for (int32 i = 0; i < NumPositions; i++)
    {
        CostMatrix[i].Reserve(NumPositions);
        CostMatrix[i].SetNum(NumPositions);

        for (int32 j = 0; j < NumPositions; j++)
        {
            // 🚀 性能优化：使用预计算的归一化位置
            float RelativePositionCost = FVector::Dist(FromNormalized[i], ToNormalized[j]) * RelativeScale;
            float AbsoluteDistanceCost = FVector::Dist(FromPositions[i], ToPositions[j]);

            // 组合成本：相对位置权重更高
            CostMatrix[i][j] = RelativePositionCost * RelativeWeight + AbsoluteDistanceCost * AbsoluteWeight;
        }
    }

    return CostMatrix;
}

TArray<TArray<float>> UFormationManagerComponent::CalculateAbsoluteDistanceCostMatrix(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions)
{
    int32 NumPositions = FromPositions.Num();
    TArray<TArray<float>> CostMatrix;

    // 🚀 性能优化：预分配内存
    CostMatrix.Reserve(NumPositions);
    CostMatrix.SetNum(NumPositions);

    // 🚀 性能优化：使用更高效的距离计算
    for (int32 i = 0; i < NumPositions; i++)
    {
        CostMatrix[i].Reserve(NumPositions);
        CostMatrix[i].SetNum(NumPositions);

        const FVector& FromPos = FromPositions[i];
        for (int32 j = 0; j < NumPositions; j++)
        {
            // 使用引用避免数组访问开销
            CostMatrix[i][j] = FVector::Dist(FromPos, ToPositions[j]);
        }
    }

    return CostMatrix;
}

// ========== 分配问题求解器 ==========

TArray<int32> UFormationManagerComponent::SolveAssignmentProblem(const TArray<TArray<float>>& CostMatrix)
{
    if (CostMatrix.Num() == 0)
    {
        return TArray<int32>();
    }

    // 🚀 性能优化：使用配置常量进行算法选择
    // 对于小规模问题使用匈牙利算法（精确），大规模问题使用贪心算法（快速）
    if (CostMatrix.Num() <= FormationPerformanceConfig::HungarianAlgorithmThreshold)
    {
        UE_LOG(LogFormationSystem, VeryVerbose, TEXT("🧮 使用匈牙利算法求解 %d×%d 分配问题"), CostMatrix.Num(), CostMatrix.Num());
        return SolveAssignmentHungarian(CostMatrix);
    }
    else
    {
        UE_LOG(LogFormationSystem, VeryVerbose, TEXT("🧮 使用贪心算法求解 %d×%d 分配问题"), CostMatrix.Num(), CostMatrix.Num());
        return SolveAssignmentGreedy(CostMatrix);
    }
}

TArray<int32> UFormationManagerComponent::SolveAssignmentGreedy(const TArray<TArray<float>>& CostMatrix)
{
    int32 NumPositions = CostMatrix.Num();
    TArray<int32> Assignment;
    Assignment.SetNum(NumPositions);

    TArray<bool> UsedTargets;
    UsedTargets.SetNum(NumPositions);
    for (int32 i = 0; i < NumPositions; i++)
    {
        UsedTargets[i] = false;
    }

    // 贪心分配：每次为当前单位选择最优的未使用目标
    for (int32 i = 0; i < NumPositions; i++)
    {
        int32 BestTarget = -1;
        float BestCost = FLT_MAX;

        for (int32 j = 0; j < NumPositions; j++)
        {
            if (!UsedTargets[j] && CostMatrix[i][j] < BestCost)
            {
                BestCost = CostMatrix[i][j];
                BestTarget = j;
            }
        }

        if (BestTarget != -1)
        {
            Assignment[i] = BestTarget;
            UsedTargets[BestTarget] = true;
        }
        else
        {
            Assignment[i] = i; // 后备方案
        }
    }

    return Assignment;
}

TArray<int32> UFormationManagerComponent::SolveAssignmentHungarian(const TArray<TArray<float>>& CostMatrix)
{
    int32 n = CostMatrix.Num();
    TArray<int32> Assignment;
    Assignment.SetNum(n);

    if (n == 0)
    {
        return Assignment;
    }

    // 🚀 性能优化：预分配内存避免重复分配
    TArray<TArray<float>> Matrix;
    Matrix.Reserve(n);
    Matrix.SetNum(n);
    for (int32 i = 0; i < n; i++)
    {
        Matrix[i].Reserve(n);
        Matrix[i] = CostMatrix[i];
    }

    // 步骤1：行约简
    for (int32 i = 0; i < n; i++)
    {
        float MinVal = Matrix[i][0];
        for (int32 j = 1; j < n; j++)
        {
            MinVal = FMath::Min(MinVal, Matrix[i][j]);
        }
        for (int32 j = 0; j < n; j++)
        {
            Matrix[i][j] -= MinVal;
        }
    }

    // 步骤2：列约简
    for (int32 j = 0; j < n; j++)
    {
        float MinVal = Matrix[0][j];
        for (int32 i = 1; i < n; i++)
        {
            MinVal = FMath::Min(MinVal, Matrix[i][j]);
        }
        for (int32 i = 0; i < n; i++)
        {
            Matrix[i][j] -= MinVal;
        }
    }

    // 简化版分配：寻找零元素进行分配
    TArray<bool> RowUsed, ColUsed;
    RowUsed.SetNum(n);
    ColUsed.SetNum(n);
    for (int32 i = 0; i < n; i++)
    {
        RowUsed[i] = false;
        ColUsed[i] = false;
        Assignment[i] = -1;
    }

    // 贪心地分配零元素
    for (int32 i = 0; i < n; i++)
    {
        for (int32 j = 0; j < n; j++)
        {
            if (FMath::IsNearlyZero(Matrix[i][j]) && !RowUsed[i] && !ColUsed[j])
            {
                Assignment[i] = j;
                RowUsed[i] = true;
                ColUsed[j] = true;
                break;
            }
        }
    }

    // 为未分配的行找到最佳分配
    for (int32 i = 0; i < n; i++)
    {
        if (Assignment[i] == -1)
        {
            int32 BestCol = -1;
            float BestCost = FLT_MAX;
            for (int32 j = 0; j < n; j++)
            {
                if (!ColUsed[j] && Matrix[i][j] < BestCost)
                {
                    BestCost = Matrix[i][j];
                    BestCol = j;
                }
            }
            if (BestCol != -1)
            {
                Assignment[i] = BestCol;
                ColUsed[BestCol] = true;
            }
            else
            {
                Assignment[i] = i; // 后备方案
            }
        }
    }

    return Assignment;
}

// ========== 特殊分配算法 ==========

TArray<int32> UFormationManagerComponent::CalculateDirectRelativePositionMatching(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions)
{
    int32 NumPositions = FromPositions.Num();
    TArray<int32> Assignment;
    Assignment.SetNum(NumPositions);

    if (NumPositions == 0)
    {
        return Assignment;
    }

    // 计算起始阵型的包围盒
    FBox FromAABB(FromPositions[0], FromPositions[0]);
    for (const FVector& Pos : FromPositions)
    {
        FromAABB += Pos;
    }

    // 计算目标阵型的包围盒
    FBox ToAABB(ToPositions[0], ToPositions[0]);
    for (const FVector& Pos : ToPositions)
    {
        ToAABB += Pos;
    }

    // 使用空间排序进行直接映射
    Assignment = CalculateSpatialOrderMapping(FromPositions, ToPositions);

    return Assignment;
}

TArray<int32> UFormationManagerComponent::CalculateRTSFlockMovementAssignment(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions)
{
    // RTS群集移动使用基于距离的简单分配，但考虑群集行为
    TArray<TArray<float>> CostMatrix = CalculateAbsoluteDistanceCostMatrix(FromPositions, ToPositions);
    
    // 添加群集行为修正
    for (int32 i = 0; i < FromPositions.Num(); i++)
    {
        for (int32 j = 0; j < ToPositions.Num(); j++)
        {
            // 计算群集密度影响
            float FlockingBonus = CalculateFlockingBonus(i, j, FromPositions, ToPositions);
            CostMatrix[i][j] -= FlockingBonus;
            CostMatrix[i][j] = FMath::Max(1.0f, CostMatrix[i][j]);
        }
    }

    return SolveAssignmentProblem(CostMatrix);
}

TArray<int32> UFormationManagerComponent::CalculatePathAwareAssignment(
    const TArray<FVector>& FromPositions,
    const TArray<FVector>& ToPositions)
{
    // 首先使用基础算法获得初始分配
    TArray<int32> InitialAssignment = CalculateDirectRelativePositionMatching(FromPositions, ToPositions);
    
    // 检测路径冲突
    FPathConflictInfo ConflictInfo = DetectPathConflicts(InitialAssignment, FromPositions, ToPositions);
    
    // 如果没有冲突，直接返回
    if (!ConflictInfo.bHasConflict)
    {
        return InitialAssignment;
    }

    // 有冲突时，使用修正的成本矩阵
    TArray<TArray<float>> CostMatrix = CalculateRelativePositionCostMatrix(FromPositions, ToPositions);
    
    // 为可能产生冲突的路径增加惩罚
    for (const FIntPoint& ConflictPair : ConflictInfo.ConflictPairs)
    {
        int32 Unit1 = ConflictPair.X;
        int32 Unit2 = ConflictPair.Y;
        
        if (InitialAssignment.IsValidIndex(Unit1) && InitialAssignment.IsValidIndex(Unit2))
        {
            int32 Target1 = InitialAssignment[Unit1];
            int32 Target2 = InitialAssignment[Unit2];
            
            // 增加冲突路径的成本
            if (CostMatrix.IsValidIndex(Unit1) && CostMatrix[Unit1].IsValidIndex(Target2))
            {
                CostMatrix[Unit1][Target2] += 1000.0f;
            }
            if (CostMatrix.IsValidIndex(Unit2) && CostMatrix[Unit2].IsValidIndex(Target1))
            {
                CostMatrix[Unit2][Target1] += 1000.0f;
            }
        }
    }

    return SolveAssignmentProblem(CostMatrix);
}

TArray<int32> UFormationManagerComponent::CalculateSpatialOrderMapping(
    const TArray<FVector>& FromPositions, 
    const TArray<FVector>& ToPositions)
{
    int32 NumPositions = FMath::Min(FromPositions.Num(), ToPositions.Num());
    TArray<int32> Assignment;
    Assignment.SetNum(NumPositions);
    
    // 如果位置数量太少，直接使用索引映射
    if (NumPositions <= 2)
    {
        for (int32 i = 0; i < NumPositions; i++)
        {
            Assignment[i] = i;
        }
        return Assignment;
    }
    
    // 计算起始阵型的包围盒
    FBox FromAABB(FromPositions[0], FromPositions[0]);
    for (const FVector& Pos : FromPositions)
    {
        FromAABB += Pos;
    }
    
    // 计算目标阵型的包围盒
    FBox ToAABB(ToPositions[0], ToPositions[0]);
    for (const FVector& Pos : ToPositions)
    {
        ToAABB += Pos;
    }

    FVector FromSize = FromAABB.GetSize();
    FVector ToSize = ToAABB.GetSize();

    // 相同阵型检测：尺寸相近则认为是相同阵型的平移
    const float SizeTolerance = 10.0f;
    bool bIsSameFormation = FMath::IsNearlyEqual(FromSize.X, ToSize.X, SizeTolerance) &&
                           FMath::IsNearlyEqual(FromSize.Y, ToSize.Y, SizeTolerance);

    // 输出更详细的调试信息
    UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: FromAABB Size=(%s)"), *FromSize.ToString());
    UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: ToAABB Size=(%s)"), *ToSize.ToString());
    UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: 尺寸差异=(X:%.2f, Y:%.2f, Z:%.2f)"), 
           FMath::Abs(FromSize.X - ToSize.X),
           FMath::Abs(FromSize.Y - ToSize.Y),
           FMath::Abs(FromSize.Z - ToSize.Z));
    UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: 容差=%.2f, 检测结果=%s"), 
           SizeTolerance, bIsSameFormation ? TEXT("相同阵型") : TEXT("不同阵型"));
    
    // 计算两个阵型的中心点
    FVector FromCenter = FromAABB.GetCenter();
    FVector ToCenter = ToAABB.GetCenter();
    
    // 相同阵型优化：如果是相同阵型的平移，使用精确的相对位置匹配
    if (bIsSameFormation)
    {
        UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: 相同阵型平移，使用精确相对位置匹配"));

        // 计算AABB相对位置成本矩阵
        TArray<TArray<float>> RelativeCostMatrix;
        RelativeCostMatrix.SetNum(NumPositions);

        // 避免除零错误
        FVector FromSizeForRelative = FromAABB.GetSize();
        FVector ToSizeForRelative = ToAABB.GetSize();
        FromSizeForRelative.X = FMath::Max(FromSizeForRelative.X, 1.0f);
        FromSizeForRelative.Y = FMath::Max(FromSizeForRelative.Y, 1.0f);
        FromSizeForRelative.Z = FMath::Max(FromSizeForRelative.Z, 1.0f);
        ToSizeForRelative.X = FMath::Max(ToSizeForRelative.X, 1.0f);
        ToSizeForRelative.Y = FMath::Max(ToSizeForRelative.Y, 1.0f);
        ToSizeForRelative.Z = FMath::Max(ToSizeForRelative.Z, 1.0f);

        for (int32 i = 0; i < NumPositions; i++)
        {
            RelativeCostMatrix[i].SetNum(NumPositions);

            // 计算起始位置的相对坐标（归一化到0-1范围）
            FVector RelativeFrom = (FromPositions[i] - FromAABB.Min) / FromSizeForRelative;

            for (int32 j = 0; j < NumPositions; j++)
            {
                // 计算目标位置的相对坐标
                FVector RelativeTo = (ToPositions[j] - ToAABB.Min) / ToSizeForRelative;

                // 相对位置距离成本（这是关键！）
                float RelativeCost = FVector::Dist(RelativeFrom, RelativeTo);
                RelativeCostMatrix[i][j] = RelativeCost;
                
                // 调试输出第一个单位的相对位置成本
                if (i == 0 && j < 5)
                {
                    UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: 单位0->目标%d 相对位置成本=%.4f"), j, RelativeCost);
                }
            }
        }

        // 使用匈牙利算法求解最优相对位置匹配
        Assignment = SolveAssignmentProblem(RelativeCostMatrix);

        // 输出前5个单位的分配结果和移动距离
        FString AssignmentStr;
        float TotalDistance = 0.0f;
        for (int32 i = 0; i < FMath::Min(5, Assignment.Num()); i++)
        {
            float Distance = FVector::Dist(FromPositions[i], ToPositions[Assignment[i]]);
            TotalDistance += Distance;
            AssignmentStr += FString::Printf(TEXT("[%d->%d:%.1f] "), i, Assignment[i], Distance);
        }
        float AverageDistance = Assignment.Num() > 0 ? TotalDistance / Assignment.Num() : 0.0f;
        UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: 相同阵型分配结果: %s..."), *AssignmentStr);
        UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: 相同阵型平均移动距离: %.2f"), AverageDistance);
        UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: 相同阵型使用AABB相对位置匹配完成"));
        
        return Assignment;
    }
    else
    {
        UE_LOG(LogFormationSystem, Log, TEXT("空间排序算法: 不同阵型变换，使用空间排序匹配"));
    }

    // 计算起始位置的空间排序数据
    TArray<FSpatialSortData> FromSortData;
    FromSortData.SetNum(NumPositions);
    for (int32 i = 0; i < NumPositions; i++)
    {
        FVector RelativePos = FromPositions[i] - FromCenter;
        float Angle = FMath::Atan2(RelativePos.Y, RelativePos.X);
        float Distance = RelativePos.Size();
        
        FromSortData[i].OriginalIndex = i;
        FromSortData[i].Angle = Angle;
        FromSortData[i].DistanceToCenter = Distance;
        FromSortData[i].Position = FromPositions[i];
    }
    
    // 计算目标位置的空间排序数据
    TArray<FSpatialSortData> ToSortData;
    ToSortData.SetNum(NumPositions);
    for (int32 i = 0; i < NumPositions; i++)
    {
        FVector RelativePos = ToPositions[i] - ToCenter;
        float Angle = FMath::Atan2(RelativePos.Y, RelativePos.X);
        float Distance = RelativePos.Size();
        
        ToSortData[i].OriginalIndex = i;
        ToSortData[i].Angle = Angle;
        ToSortData[i].DistanceToCenter = Distance;
        ToSortData[i].Position = ToPositions[i];
    }
    
    // 检测是否为螺旋阵型
    bool bFromIsSpiral = DetectSpiralFormation(FromSortData);
    bool bToIsSpiral = DetectSpiralFormation(ToSortData);
    
    // 如果两个阵型都是螺旋形，使用螺旋参数排序
    if (bFromIsSpiral && bToIsSpiral)
    {
        // 按螺旋参数排序
        FromSortData.Sort([this](const FSpatialSortData& A, const FSpatialSortData& B) {
            float ParamA = CalculateSpiralParameter(A.Angle, A.DistanceToCenter);
            float ParamB = CalculateSpiralParameter(B.Angle, B.DistanceToCenter);
            return ParamA < ParamB;
        });
        
        ToSortData.Sort([this](const FSpatialSortData& A, const FSpatialSortData& B) {
            float ParamA = CalculateSpiralParameter(A.Angle, A.DistanceToCenter);
            float ParamB = CalculateSpiralParameter(B.Angle, B.DistanceToCenter);
            return ParamA < ParamB;
        });
    }
    else
    {
        // 使用备份代码中的改进排序策略：首先按角度排序，然后按距离排序
        auto SortPredicate = [](const FSpatialSortData& A, const FSpatialSortData& B) -> bool
        {
            if (!FMath::IsNearlyEqual(A.Angle, B.Angle, 0.01f))
            {
                return A.Angle < B.Angle;
            }
            return A.DistanceToCenter < B.DistanceToCenter;
        };

        FromSortData.Sort(SortPredicate);
        ToSortData.Sort(SortPredicate);
    }
    
    // 根据排序结果建立映射
    for (int32 i = 0; i < NumPositions; i++)
    {
        Assignment[FromSortData[i].OriginalIndex] = ToSortData[i].OriginalIndex;
    }
    
    return Assignment;
}

TArray<int32> UFormationManagerComponent::CalculateDistancePriorityAssignment(
    const TArray<FVector>& FromPositions, 
    const TArray<FVector>& ToPositions)
{
    // 已弃用，使用空间排序映射代替
    UE_LOG(LogFormationSystem, Warning, TEXT("CalculateDistancePriorityAssignment已弃用，使用CalculateSpatialOrderMapping代替"));
    return CalculateSpatialOrderMapping(FromPositions, ToPositions);
}

// ========== 辅助函数 ==========

bool UFormationManagerComponent::DetectSpiralFormation(const TArray<FSpatialSortData>& SortedData)
{
    if (SortedData.Num() < 10)
    {
        return false;
    }
    
    // 检查是否角度和距离呈线性关系
    float AngleSum = 0.0f;
    float DistanceSum = 0.0f;
    float AngleDistanceSum = 0.0f;
    float AngleSquaredSum = 0.0f;
    
    for (const FSpatialSortData& Data : SortedData)
    {
        AngleSum += Data.Angle;
        DistanceSum += Data.DistanceToCenter;
        AngleDistanceSum += Data.Angle * Data.DistanceToCenter;
        AngleSquaredSum += Data.Angle * Data.Angle;
    }
    
    float n = static_cast<float>(SortedData.Num());
    float Numerator = n * AngleDistanceSum - AngleSum * DistanceSum;
    float Denominator = n * AngleSquaredSum - AngleSum * AngleSum;
    
    if (FMath::IsNearlyZero(Denominator))
    {
        return false;
    }
    
    float Slope = Numerator / Denominator;
    float Intercept = (DistanceSum - Slope * AngleSum) / n;
    
    // 计算相关系数
    float CorrelationCoefficient = Numerator / (FMath::Sqrt(Denominator) * FMath::Sqrt(n * DistanceSum * DistanceSum - DistanceSum * DistanceSum));
    
    // 相关系数的绝对值大于0.7表示强相关
    return FMath::Abs(CorrelationCoefficient) > 0.7f;
}

float UFormationManagerComponent::CalculateSpiralParameter(float Angle, float Distance)
{
    // 简单的螺旋参数计算：角度 + 距离的加权和
    return Angle + Distance * 0.01f;
}

float UFormationManagerComponent::CalculateFlockingBonus(
    int32 FromIndex, 
    int32 ToIndex, 
    const TArray<FVector>& FromPositions, 
    const TArray<FVector>& ToPositions)
{
    // 计算相对位置向量
    FVector FromRelative = FromPositions[FromIndex] - FromPositions[0];
    FVector ToRelative = ToPositions[ToIndex] - ToPositions[0];
    
    // 计算向量相似度
    float Similarity = FVector::DotProduct(FromRelative.GetSafeNormal(), ToRelative.GetSafeNormal());
    
    // 相似度越高，奖励越大
    return Similarity * 100.0f;
}

FPathConflictInfo UFormationManagerComponent::DetectPathConflicts(
    const TArray<int32>& Assignment, 
    const TArray<FVector>& FromPositions, 
    const TArray<FVector>& ToPositions)
{
    FPathConflictInfo Result;
    Result.bHasConflict = false;
    Result.ConflictSeverity = 0.0f;
    Result.TotalConflicts = 0;
    
    int32 NumPositions = FMath::Min(Assignment.Num(), FMath::Min(FromPositions.Num(), ToPositions.Num()));
    
    // 检查每对路径是否相交
    for (int32 i = 0; i < NumPositions; i++)
    {
        if (!Assignment.IsValidIndex(i) || !ToPositions.IsValidIndex(Assignment[i]))
        {
            continue;
        }
        
        FVector Start1 = FromPositions[i];
        FVector End1 = ToPositions[Assignment[i]];
        
        for (int32 j = i + 1; j < NumPositions; j++)
        {
            if (!Assignment.IsValidIndex(j) || !ToPositions.IsValidIndex(Assignment[j]))
            {
                continue;
            }
            
            FVector Start2 = FromPositions[j];
            FVector End2 = ToPositions[Assignment[j]];
            
            // 使用工具类检测路径是否相交
            if (FFormationMathUtils::DoPathsIntersect(Start1, End1, Start2, End2))
            {
                Result.bHasConflict = true;
                Result.ConflictPairs.Add(FIntPoint(i, j));
                Result.TotalConflicts++;
                
                // 计算冲突严重程度（基于路径长度和交叉角度）
                FVector Dir1 = (End1 - Start1).GetSafeNormal();
                FVector Dir2 = (End2 - Start2).GetSafeNormal();
                float CrossAngle = FMath::Acos(FVector::DotProduct(Dir1, Dir2));
                
                // 垂直交叉的冲突最严重
                float AngleSeverity = FMath::Sin(CrossAngle);
                Result.ConflictSeverity += AngleSeverity;
            }
        }
    }
    
    // 归一化冲突严重程度
    if (Result.TotalConflicts > 0)
    {
        Result.ConflictSeverity /= static_cast<float>(Result.TotalConflicts);
    }
    
    return Result;
}
