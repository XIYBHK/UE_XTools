#include "SortLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(SortLibraryLog, Log, All);

// 键值对结构定义
namespace SortPairs
{
    struct FActorDistancePair
    {
        AActor* Value;
        float Distance;
        int32 OriginalIndex;

        FActorDistancePair() : Value(nullptr), Distance(MAX_FLT), OriginalIndex(INDEX_NONE) {}
    };

    struct FActorHeightPair
    {
        AActor* Value;
        float Height;
        int32 OriginalIndex;

        FActorHeightPair() : Value(nullptr), Height(MAX_FLT), OriginalIndex(INDEX_NONE) {}
    };

    struct FIntegerPair
    {
        int32 Value;
        int32 OriginalIndex;

        FIntegerPair() : Value(0), OriginalIndex(INDEX_NONE) {}
    };

    struct FFloatPair
    {
        float Value;
        int32 OriginalIndex;

        FFloatPair() : Value(0.0f), OriginalIndex(INDEX_NONE) {}
    };

    struct FStringPair
    {
        FString Value;
        int32 OriginalIndex;

        FStringPair() : Value(TEXT("")), OriginalIndex(INDEX_NONE) {}
    };

    struct FNamePair
    {
        FName Value;
        int32 OriginalIndex;

        FNamePair() : Value(NAME_None), OriginalIndex(INDEX_NONE) {}
    };

    struct FActorAxisPair
    {
        AActor* Value;
        float AxisValue;
        int32 OriginalIndex;

        FActorAxisPair() : Value(nullptr), AxisValue(0.0f), OriginalIndex(INDEX_NONE) {}
    };

    struct FActorAnglePair
    {
        AActor* Value;
        float Angle;
        int32 OriginalIndex;

        FActorAnglePair() : Value(nullptr), Angle(0.0f), OriginalIndex(INDEX_NONE) {}
    };

    struct FActorAzimuthPair
    {
        AActor* Value;
        float Azimuth;
        int32 OriginalIndex;

        FActorAzimuthPair() : Value(nullptr), Azimuth(0.0f), OriginalIndex(INDEX_NONE) {}
    };
}

// 辅助函数声明
template<typename T>
bool InitializeArrays(const TArray<T>& InArray, TArray<T>& OutArray, TArray<int32>& Indices);

template<typename T, typename PairType>
void UpdateSortedResults(const TArray<PairType>& Pairs, TArray<T>& SortedArray,
    TArray<int32>& OriginalIndices, TArray<float>* SortedValues = nullptr,
    TFunction<float(const PairType&)> GetSortedValue = nullptr);

template<typename T>
void OptimizeMemory(TArray<T>& Array, bool bShrinkArray = true);

template<typename T, typename PairType>
bool CreateSortPairs(const TArray<T>& InArray, TArray<PairType>& Pairs, bool PreallocateMemory = true)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        UE_LOG(SortLibraryLog, Warning, TEXT("CreateSortPairs: Input array is empty"));
        return false;
    }

    const int32 ArrayNum = InArray.Num();
    
    // 预分配内存以提高性能
    if (PreallocateMemory)
    {
        Pairs.Empty(ArrayNum);
        Pairs.SetNum(ArrayNum);
    }
    else
    {
        Pairs.Empty();
        Pairs.Reserve(ArrayNum);
    }

    // 创建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        if (PreallocateMemory)
        {
            // 安全地复制值
            Pairs[i].Value = InArray[i];
            Pairs[i].OriginalIndex = i;
        }
        else
        {
            PairType NewPair;
            NewPair.Value = InArray[i];
            NewPair.OriginalIndex = i;
            Pairs.Add(NewPair);
        }
    }

    return true;
}

void USortLibrary::SortActorsByDistance(const TArray<AActor*>& Actors, const FVector& Location, bool bAscending,
    bool b2DDistance, TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedDistances)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        SortedDistances.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(Actors, SortedActors, OriginalIndices))
    {
        return;
    }
    SortedDistances.SetNumUninitialized(Actors.Num());

    // 创建并填充键值对数组
    TArray<SortPairs::FActorDistancePair> Pairs;
    if (!CreateSortPairs(Actors, Pairs))
    {
        return;
    }

    // 计算距离
    for (auto& Pair : Pairs)
    {
        if (!IsValid(Pair.Value))
        {
            Pair.Distance = MAX_FLT;
            continue;
        }
        
        if (b2DDistance)
        {
            FVector ActorLoc = Pair.Value->GetActorLocation();
            ActorLoc.Z = Location.Z;
            Pair.Distance = FVector::Dist(ActorLoc, Location);
        }
        else
        {
            Pair.Distance = FVector::Dist(Pair.Value->GetActorLocation(), Location);
        }
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FActorDistancePair& A, const SortPairs::FActorDistancePair& B) {
        return bAscending ? A.Distance < B.Distance : A.Distance > B.Distance;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedActors[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedDistances[i] = Pairs[i].Distance;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortIntegerArray(const TArray<int32>& InArray, bool bAscending,
    TArray<int32>& SortedArray, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        SortedArray.Empty();
        OriginalIndices.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(InArray, SortedArray, OriginalIndices))
    {
        return;
    }

    // 创建并填充键值对数组
    TArray<SortPairs::FIntegerPair> Pairs;
    if (!CreateSortPairs(InArray, Pairs))
    {
        return;
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FIntegerPair& A, const SortPairs::FIntegerPair& B) {
        return bAscending ? A.Value < B.Value : A.Value > B.Value;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortFloatArray(const TArray<float>& InArray, bool bAscending,
    TArray<float>& SortedArray, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        SortedArray.Empty();
        OriginalIndices.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(InArray, SortedArray, OriginalIndices))
    {
        return;
    }

    // 创建并填充键值对数组
    TArray<SortPairs::FFloatPair> Pairs;
    if (!CreateSortPairs(InArray, Pairs))
    {
        return;
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FFloatPair& A, const SortPairs::FFloatPair& B) {
        // 处理特殊浮点数
        if (FMath::IsNaN(A.Value)) return false;
        if (FMath::IsNaN(B.Value)) return true;
        if (!FMath::IsFinite(A.Value) && !FMath::IsFinite(B.Value)) return false;
        if (!FMath::IsFinite(A.Value)) return bAscending;
        if (!FMath::IsFinite(B.Value)) return !bAscending;
        if (FMath::IsNearlyEqual(A.Value, B.Value)) return false;
        return bAscending ? A.Value < B.Value : A.Value > B.Value;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortStringArray(const TArray<FString>& InArray, bool bAscending,
    TArray<FString>& SortedArray, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        SortedArray.Empty();
        OriginalIndices.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(InArray, SortedArray, OriginalIndices))
    {
        return;
    }

    // 创建并填充键值对数组
    TArray<SortPairs::FStringPair> Pairs;
    if (!CreateSortPairs(InArray, Pairs))
    {
        return;
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FStringPair& A, const SortPairs::FStringPair& B) {
        // 确保字符串有效
        if (A.Value.IsEmpty() && B.Value.IsEmpty()) return false;
        if (A.Value.IsEmpty()) return bAscending;
        if (B.Value.IsEmpty()) return !bAscending;

        // 使用本地化感知的字符串比较
        const FText TextA = FText::FromString(A.Value);
        const FText TextB = FText::FromString(B.Value);
        
        // 使用 FText 的 Compare 函数进行比较
        const int32 CompareResult = FText::FromString(A.Value).CompareTo(FText::FromString(B.Value));
        
        // 根据升序/降序返回比较结果
        return bAscending ? CompareResult < 0 : CompareResult > 0;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortNameArray(const TArray<FName>& InArray, bool bAscending,
    TArray<FName>& SortedArray, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        SortedArray.Empty();
        OriginalIndices.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(InArray, SortedArray, OriginalIndices))
    {
        return;
    }

    // 创建并填充键值对数组
    TArray<SortPairs::FNamePair> Pairs;
    if (!CreateSortPairs(InArray, Pairs))
    {
        return;
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FNamePair& A, const SortPairs::FNamePair& B) {
        // 确保名称有效
        if (A.Value.IsNone() && B.Value.IsNone()) return false;
        if (A.Value.IsNone()) return bAscending;
        if (B.Value.IsNone()) return !bAscending;

        // 转换为字符串以支持本地化比较
        const FString StringA = A.Value.ToString();
        const FString StringB = B.Value.ToString();

        // 使用本地化感知的字符串比较
        const FText TextA = FText::FromString(StringA);
        const FText TextB = FText::FromString(StringB);
        
        // 使用 FText 的 Compare 函数进行比较
        const int32 CompareResult = FText::FromString(StringA).CompareTo(FText::FromString(StringB));
        
        // 根据升序/降序返回比较结果
        return bAscending ? CompareResult < 0 : CompareResult > 0;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortActorsByAxis(const TArray<AActor*>& Actors, ECoordinateAxis Axis, bool bAscending,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAxisValues)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        SortedAxisValues.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(Actors, SortedActors, OriginalIndices))
    {
        return;
    }
    SortedAxisValues.SetNumUninitialized(Actors.Num());

    // 创建并填充键值对数组
    TArray<SortPairs::FActorAxisPair> Pairs;
    if (!CreateSortPairs(Actors, Pairs))
    {
        return;
    }

    // 计算轴向值
    for (auto& Pair : Pairs)
    {
        if (!IsValid(Pair.Value))
        {
            Pair.AxisValue = MAX_FLT;
            continue;
        }

        switch(Axis)
        {
            case ECoordinateAxis::X:
                Pair.AxisValue = Pair.Value->GetActorLocation().X;
                break;
            case ECoordinateAxis::Y:
                Pair.AxisValue = Pair.Value->GetActorLocation().Y;
                break;
            case ECoordinateAxis::Z:
                Pair.AxisValue = Pair.Value->GetActorLocation().Z;
                break;
        }
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FActorAxisPair& A, const SortPairs::FActorAxisPair& B) {
        return bAscending ? A.AxisValue < B.AxisValue : A.AxisValue > B.AxisValue;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedActors[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAxisValues[i] = Pairs[i].AxisValue;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortActorsByAngle(const TArray<AActor*>& Actors, const FVector& Center, const FVector& Direction,
    bool bAscending, bool b2DAngle, TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAngles)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        SortedAngles.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(Actors, SortedActors, OriginalIndices))
    {
        return;
    }
    SortedAngles.SetNumUninitialized(Actors.Num());

    // 标准化参考方向
    FVector NormalizedDirection = Direction.GetSafeNormal();
    if (b2DAngle)
    {
        NormalizedDirection.Z = 0.0f;
        NormalizedDirection.Normalize();
    }

    // 创建并填充键值对数组
    TArray<SortPairs::FActorAnglePair> Pairs;
    if (!CreateSortPairs(Actors, Pairs))
    {
        return;
    }

    // 计算夹角
    for (auto& Pair : Pairs)
    {
        if (!IsValid(Pair.Value))
        {
            Pair.Angle = MAX_FLT;
            continue;
        }

        FVector ToActor = Pair.Value->GetActorLocation() - Center;
        if (b2DAngle)
        {
            ToActor.Z = 0.0f;
            ToActor.Normalize();
            float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
            float Cross = NormalizedDirection.X * ToActor.Y - NormalizedDirection.Y * ToActor.X;
            Pair.Angle = Cross < 0.0f ? 360.0f - Angle : Angle;
        }
        else
        {
            ToActor.Normalize();
            Pair.Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
        }
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FActorAnglePair& A, const SortPairs::FActorAnglePair& B) {
        return bAscending ? A.Angle < B.Angle : A.Angle > B.Angle;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedActors[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAngles[i] = Pairs[i].Angle;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortActorsByAzimuth(const TArray<AActor*>& Actors, const FVector& Center, bool bAscending,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAzimuths)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        SortedAzimuths.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(Actors, SortedActors, OriginalIndices))
    {
        return;
    }
    SortedAzimuths.SetNumUninitialized(Actors.Num());

    // 创建并填充键值对数组
    TArray<SortPairs::FActorAzimuthPair> Pairs;
    if (!CreateSortPairs(Actors, Pairs))
    {
        return;
    }

    // 计算方位角
    for (auto& Pair : Pairs)
    {
        if (!IsValid(Pair.Value))
        {
            Pair.Azimuth = MAX_FLT;
            continue;
        }

        // 计算从中心点到Actor的向量（在XY平面上）
        FVector ToActor = Pair.Value->GetActorLocation() - Center;
        ToActor.Z = 0.0f;

        // 计算方位角（以Y轴正方向为北，顺时针旋转）
        float Angle = FMath::RadiansToDegrees(FMath::Atan2(ToActor.Y, ToActor.X));
        float Azimuth = FMath::Fmod(90.0f - Angle, 360.0f);
        Pair.Azimuth = Azimuth < 0.0f ? Azimuth + 360.0f : Azimuth;
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FActorAzimuthPair& A, const SortPairs::FActorAzimuthPair& B) {
        return bAscending ? A.Azimuth < B.Azimuth : A.Azimuth > B.Azimuth;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedActors[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAzimuths[i] = Pairs[i].Azimuth;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortActorsByHeight(const TArray<AActor*>& Actors, bool bAscending,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(Actors, SortedActors, OriginalIndices))
    {
        return;
    }

    // 创建并填充键值对数组
    TArray<SortPairs::FActorHeightPair> Pairs;
    if (!CreateSortPairs(Actors, Pairs))
    {
        return;
    }

    // 计算高度
    for (auto& Pair : Pairs)
    {
        if (!IsValid(Pair.Value))
        {
            Pair.Height = MAX_FLT;
            continue;
        }
        
        Pair.Height = Pair.Value->GetActorLocation().Z;
    }

    // 执行排序
    Pairs.Sort([bAscending](const SortPairs::FActorHeightPair& A, const SortPairs::FActorHeightPair& B) {
        return bAscending ? A.Height < B.Height : A.Height > B.Height;
    });

    // 更新结果
    for(int32 i = 0; i < Pairs.Num(); ++i)
    {
        SortedActors[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortActorsByAngleAndDistance(const TArray<AActor*>& Actors, const FVector& Center,
    const FVector& Direction, float MaxAngle, float MaxDistance, float AngleWeight, float DistanceWeight, 
    bool bAscending, bool b2DAngle, TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices,
    TArray<float>& SortedAngles, TArray<float>& SortedDistances)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        SortedAngles.Empty();
        SortedDistances.Empty();
        return;
    }

    // 初始化输出数组
    if (!InitializeArrays(Actors, SortedActors, OriginalIndices))
    {
        return;
    }
    SortedAngles.SetNumUninitialized(Actors.Num());
    SortedDistances.SetNumUninitialized(Actors.Num());

    // 标准化参考方向
    FVector NormalizedDirection = Direction.GetSafeNormal();
    if (b2DAngle)
    {
        NormalizedDirection.Z = 0.0f;
        NormalizedDirection.Normalize();
    }

    // 创建并填充键值对数组
    struct FActorAngleDistancePair
    {
        AActor* Value;
        float Angle;
        float Distance;
        float SortValue; // 添加一个排序值字段，用于组合角度和距离
        int32 OriginalIndex;

        FActorAngleDistancePair() : Value(nullptr), Angle(0.0f), Distance(0.0f), SortValue(0.0f), OriginalIndex(INDEX_NONE) {}
    };

    TArray<FActorAngleDistancePair> Pairs;
    Pairs.SetNumUninitialized(Actors.Num());

    // 计算角度和距离
    int32 ValidPairCount = 0;
    float MaxFoundDistance = 0.0f; // 用于归一化距离
    float MaxFoundAngle = 0.0f;    // 用于归一化角度

    // 第一次遍历，找出最大值用于归一化
    for (const AActor* Actor : Actors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        FVector ToActor = Actor->GetActorLocation() - Center;
        
        // 计算距离
        float Distance;
        if (b2DAngle)
        {
            FVector ActorLoc = Actor->GetActorLocation();
            ActorLoc.Z = Center.Z;
            Distance = FVector::Dist(ActorLoc, Center);
        }
        else
        {
            Distance = ToActor.Size();
        }

        if (MaxDistance > 0.0f && Distance > MaxDistance)
        {
            continue;
        }

        MaxFoundDistance = FMath::Max(MaxFoundDistance, Distance);

        // 计算角度
        if (b2DAngle)
        {
            ToActor.Z = 0.0f;
            if (!ToActor.Normalize())
            {
                continue;
            }
            float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
            float Cross = NormalizedDirection.X * ToActor.Y - NormalizedDirection.Y * ToActor.X;
            Angle = Cross < 0.0f ? 360.0f - Angle : Angle;
            MaxFoundAngle = FMath::Max(MaxFoundAngle, Angle);
        }
        else
        {
            if (!ToActor.Normalize())
            {
                continue;
            }
            float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
            MaxFoundAngle = FMath::Max(MaxFoundAngle, Angle);
        }
    }

    // 防止除以零
    MaxFoundDistance = MaxFoundDistance > 0.0f ? MaxFoundDistance : 1.0f;
    MaxFoundAngle = MaxFoundAngle > 0.0f ? MaxFoundAngle : 1.0f;

    // 第二次遍历，计算实际的排序值
    for (int32 i = 0; i < Actors.Num(); ++i)
    {
        FActorAngleDistancePair& Pair = Pairs[ValidPairCount];
        Pair.Value = Actors[i];
        Pair.OriginalIndex = i;

        if (!IsValid(Pair.Value))
        {
            continue;
        }

        FVector ToActor = Pair.Value->GetActorLocation() - Center;
        
        // 计算距离
        if (b2DAngle)
        {
            FVector ActorLoc = Pair.Value->GetActorLocation();
            ActorLoc.Z = Center.Z;
            Pair.Distance = FVector::Dist(ActorLoc, Center);
        }
        else
        {
            Pair.Distance = ToActor.Size();
        }

        // 如果超出最大距离，跳过
        if (MaxDistance > 0.0f && Pair.Distance > MaxDistance)
        {
            continue;
        }

        // 计算角度
        if (b2DAngle)
        {
            ToActor.Z = 0.0f;
            if (!ToActor.Normalize())
            {
                continue;
            }
            float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
            float Cross = NormalizedDirection.X * ToActor.Y - NormalizedDirection.Y * ToActor.X;
            Pair.Angle = Cross < 0.0f ? 360.0f - Angle : Angle;
        }
        else
        {
            if (!ToActor.Normalize())
            {
                continue;
            }
            Pair.Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
        }

        // 如果超出最大角度，跳过
        if (MaxAngle > 0.0f && Pair.Angle > MaxAngle)
        {
            continue;
        }

        // 计算归一化的排序值
        float NormalizedAngle = Pair.Angle / MaxFoundAngle;
        float NormalizedDistance = Pair.Distance / MaxFoundDistance;

        // 如果权重都为0，使用默认的排序逻辑
        if (FMath::IsNearlyZero(AngleWeight) && FMath::IsNearlyZero(DistanceWeight))
        {
            Pair.SortValue = NormalizedAngle;
        }
        else
        {
            // 使用权重计算最终的排序值
            float WeightSum = AngleWeight + DistanceWeight;
            if (WeightSum > 0.0f)
            {
                Pair.SortValue = (NormalizedAngle * AngleWeight + NormalizedDistance * DistanceWeight) / WeightSum;
            }
            else
            {
                Pair.SortValue = NormalizedAngle;
            }
        }

        ValidPairCount++;
    }

    // 调整数组大小为有效的配对数量
    if (ValidPairCount == 0)
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        SortedAngles.Empty();
        SortedDistances.Empty();
        return;
    }

    Pairs.SetNum(ValidPairCount);

    // 执行排序
    Pairs.Sort([bAscending](const FActorAngleDistancePair& A, const FActorAngleDistancePair& B) {
        return bAscending ? A.SortValue < B.SortValue : A.SortValue > B.SortValue;
    });

    // 更新输出数组大小
    SortedActors.SetNum(ValidPairCount);
    OriginalIndices.SetNum(ValidPairCount);
    SortedAngles.SetNum(ValidPairCount);
    SortedDistances.SetNum(ValidPairCount);

    // 更新结果
    for(int32 i = 0; i < ValidPairCount; ++i)
    {
        SortedActors[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAngles[i] = Pairs[i].Angle;
        SortedDistances[i] = Pairs[i].Distance;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

/**
 * 初始化排序所需的输出数组和索引数组
 * @param InArray - 输入数组
 * @param OutArray - 输出数组，将被初始化为输入数组的副本
 * @param Indices - 索引数组，将被初始化为连续的索引值
 * @return bool - 初始化是否成功
 */
template<typename T>
bool InitializeArrays(const TArray<T>& InArray, TArray<T>& OutArray, TArray<int32>& Indices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        UE_LOG(SortLibraryLog, Warning, TEXT("InitializeArrays: Input array is empty"));
        return false;
    }

    // 初始化输出数组
    OutArray = InArray;
    const int32 ArrayNum = InArray.Num();
    
    // 预分配内存以提高性能
    Indices.Empty(ArrayNum);
    Indices.SetNumUninitialized(ArrayNum);
    
    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        Indices[i] = i;
    }

    return true;
}

/**
 * 更新排序后的结果数组
 * @param Pairs - 已排序的键值对数组
 * @param SortedArray - 要更新的排序结果数组
 * @param OriginalIndices - 要更新的原始索引数组
 * @param SortedValues - 可选的排序值数组
 * @param GetSortedValue - 获取排序值的函数
 */
template<typename T, typename PairType>
void UpdateSortedResults(const TArray<PairType>& Pairs, TArray<T>& SortedArray,
    TArray<int32>& OriginalIndices, TArray<float>* SortedValues,
    TFunction<float(const PairType&)> GetSortedValue)
{
    // 参数验证
    if (Pairs.Num() <= 0)
    {
        UE_LOG(SortLibraryLog, Warning, TEXT("UpdateSortedResults: Pairs array is empty"));
        return;
    }

    if (SortedValues && !GetSortedValue)
    {
        UE_LOG(SortLibraryLog, Warning, TEXT("UpdateSortedResults: GetSortedValue function is required when SortedValues is provided"));
        return;
    }

    const int32 ArrayNum = Pairs.Num();
    
    // 确保输出数组大小正确
    if (SortedArray.Num() != ArrayNum)
    {
        SortedArray.SetNum(ArrayNum);
    }
    if (OriginalIndices.Num() != ArrayNum)
    {
        OriginalIndices.SetNum(ArrayNum);
    }
    if (SortedValues && SortedValues->Num() != ArrayNum)
    {
        SortedValues->SetNum(ArrayNum);
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        if(SortedValues && GetSortedValue)
        {
            (*SortedValues)[i] = GetSortedValue(Pairs[i]);
        }
    }
}

/**
 * 优化数组内存使用
 * @param Array - 要优化的数组
 * @param bShrinkArray - 是否收缩数组内存
 */
template<typename T>
void OptimizeMemory(TArray<T>& Array, bool bShrinkArray)
{
    Array.Empty();
    if (bShrinkArray)
    {
        Array.Shrink();
    }
}

// 向量排序函数实现
void USortLibrary::SortVectorsByProjection(const TArray<FVector>& Vectors, const FVector& Direction, bool bAscending,
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedProjections)
{
    // 参数验证
    if (Vectors.Num() <= 0)
    {
        SortedVectors.Empty();
        OriginalIndices.Empty();
        SortedProjections.Empty();
        return;
    }

    // 初始化输出数组
    SortedVectors = Vectors;
    const int32 ArrayNum = Vectors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);
    SortedProjections.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 标准化方向向量
    FVector NormalizedDirection = Direction.GetSafeNormal();

    // 创建排序用的键值对数组
    struct FVectorProjectionPair
    {
        FVector Vector;
        float Projection;
        int32 OriginalIndex;

        FVectorProjectionPair() : Vector(FVector::ZeroVector), Projection(0.0f), OriginalIndex(INDEX_NONE) {}
    };

    TArray<FVectorProjectionPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Projection = FVector::DotProduct(Vectors[i], NormalizedDirection);
        Pairs[i].Vector = Vectors[i];
        Pairs[i].Projection = Projection;
        Pairs[i].OriginalIndex = i;
    }

    // 执行排序
    Pairs.Sort([bAscending](const FVectorProjectionPair& A, const FVectorProjectionPair& B) {
        return bAscending ? A.Projection < B.Projection : A.Projection > B.Projection;
    });

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedVectors[i] = Pairs[i].Vector;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedProjections[i] = Pairs[i].Projection;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortVectorsByLength(const TArray<FVector>& Vectors, bool bAscending,
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedLengths)
{
    // 参数验证
    if (Vectors.Num() <= 0)
    {
        SortedVectors.Empty();
        OriginalIndices.Empty();
        SortedLengths.Empty();
        return;
    }

    // 初始化输出数组
    SortedVectors = Vectors;
    const int32 ArrayNum = Vectors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);
    SortedLengths.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FVectorLengthPair
    {
        FVector Vector;
        float Length;
        int32 OriginalIndex;

        FVectorLengthPair() : Vector(FVector::ZeroVector), Length(0.0f), OriginalIndex(INDEX_NONE) {}
    };

    TArray<FVectorLengthPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Length = Vectors[i].Size();
        Pairs[i].Vector = Vectors[i];
        Pairs[i].Length = Length;
        Pairs[i].OriginalIndex = i;
    }

    // 执行排序
    Pairs.Sort([bAscending](const FVectorLengthPair& A, const FVectorLengthPair& B) {
        return bAscending ? A.Length < B.Length : A.Length > B.Length;
    });

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedVectors[i] = Pairs[i].Vector;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedLengths[i] = Pairs[i].Length;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

void USortLibrary::SortVectorsByAxis(const TArray<FVector>& Vectors, ECoordinateAxis Axis, bool bAscending,
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedAxisValues)
{
    // 参数验证
    if (Vectors.Num() <= 0)
    {
        SortedVectors.Empty();
        OriginalIndices.Empty();
        SortedAxisValues.Empty();
        return;
    }

    // 初始化输出数组
    SortedVectors = Vectors;
    const int32 ArrayNum = Vectors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);
    SortedAxisValues.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FVectorAxisPair
    {
        FVector Vector;
        float AxisValue;
        int32 OriginalIndex;

        FVectorAxisPair() : Vector(FVector::ZeroVector), AxisValue(0.0f), OriginalIndex(INDEX_NONE) {}
    };

    TArray<FVectorAxisPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Value = 0.0f;
        switch(Axis)
        {
            case ECoordinateAxis::X:
                Value = Vectors[i].X;
                break;
            case ECoordinateAxis::Y:
                Value = Vectors[i].Y;
                break;
            case ECoordinateAxis::Z:
                Value = Vectors[i].Z;
                break;
        }
        Pairs[i].Vector = Vectors[i];
        Pairs[i].AxisValue = Value;
        Pairs[i].OriginalIndex = i;
    }

    // 执行排序
    Pairs.Sort([bAscending](const FVectorAxisPair& A, const FVectorAxisPair& B) {
        return bAscending ? A.AxisValue < B.AxisValue : A.AxisValue > B.AxisValue;
    });

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedVectors[i] = Pairs[i].Vector;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAxisValues[i] = Pairs[i].AxisValue;
    }

    // 优化内存
    OptimizeMemory(Pairs);
}

// 数组操作函数实现
void USortLibrary::RemoveDuplicateActors(const TArray<AActor*>& InArray, TArray<AActor*>& OutArray)
{
    // 清空输出数组
    OutArray.Empty();

    // 使用集合来去重
    TSet<AActor*> UniqueActors;
    
    // 遍历输入数组
    for (AActor* Actor : InArray)
    {
        // 只添加有效的Actor
        if (IsValid(Actor))
        {
            UniqueActors.Add(Actor);
        }
    }

    // 将结果转换回数组
    OutArray = UniqueActors.Array();
}

void USortLibrary::RemoveDuplicateFloats(const TArray<float>& InArray, float Tolerance, TArray<float>& OutArray)
{
    // 清空输出数组
    OutArray.Empty();
    
    // 遍历输入数组
    for (float Value : InArray)
    {
        bool bIsDuplicate = false;
        
        // 检查是否已存在相近的值
        for (float ExistingValue : OutArray)
        {
            if (FMath::IsNearlyEqual(Value, ExistingValue, Tolerance))
            {
                bIsDuplicate = true;
                break;
            }
        }
        
        // 如果不是重复值，则添加到输出数组
        if (!bIsDuplicate)
        {
            OutArray.Add(Value);
        }
    }
}

void USortLibrary::RemoveDuplicateIntegers(const TArray<int32>& InArray, TArray<int32>& OutArray)
{
    // 使用集合来去重
    TSet<int32> UniqueInts(InArray);
    
    // 将结果转换回数组
    OutArray = UniqueInts.Array();
}

void USortLibrary::RemoveDuplicateStrings(const TArray<FString>& InArray, bool bCaseSensitive, TArray<FString>& OutArray)
{
    // 清空输出数组
    OutArray.Empty();
    
    // 遍历输入数组
    for (const FString& String : InArray)
    {
        bool bIsDuplicate = false;
        
        // 检查是否已存在相同的字符串
        for (const FString& ExistingString : OutArray)
        {
            if (bCaseSensitive)
            {
                if (String.Equals(ExistingString))
                {
                    bIsDuplicate = true;
                    break;
                }
            }
            else
            {
                if (String.Equals(ExistingString, ESearchCase::IgnoreCase))
                {
                    bIsDuplicate = true;
                    break;
                }
            }
        }
        
        // 如果不是重复值，则添加到输出数组
        if (!bIsDuplicate)
        {
            OutArray.Add(String);
        }
    }
}

void USortLibrary::RemoveDuplicateVectors(const TArray<FVector>& InArray, float Tolerance, TArray<FVector>& OutArray)
{
    // 清空输出数组
    OutArray.Empty();
    
    // 遍历输入数组
    for (const FVector& Vector : InArray)
    {
        bool bIsDuplicate = false;
        
        // 检查是否已存在相近的向量
        for (const FVector& ExistingVector : OutArray)
        {
            if (Vector.Equals(ExistingVector, Tolerance))
            {
                bIsDuplicate = true;
                break;
            }
        }
        
        // 如果不是重复值，则添加到输出数组
        if (!bIsDuplicate)
        {
            OutArray.Add(Vector);
        }
    }
}

void USortLibrary::FindDuplicateVectors(const TArray<FVector>& InArray, float Tolerance,
    TArray<int32>& DuplicateIndices, TArray<FVector>& DuplicateValues)
{
    // 清空输出数组
    DuplicateIndices.Empty();
    DuplicateValues.Empty();
    
    // 如果数组为空，直接返回
    if (InArray.Num() == 0)
    {
        return;
    }
    
    // 创建标记数组，用于记录已处理的索引
    TArray<bool> Processed;
    Processed.Init(false, InArray.Num());
    
    // 遍历所有向量
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        // 如果当前索引已被处理，跳过
        if (Processed[i])
        {
            continue;
        }
        
        // 创建当前重复组的临时数组
        TArray<int32> CurrentGroup;
        CurrentGroup.Add(i);
        
        // 查找与当前向量完全相同的其他向量
        for (int32 j = i + 1; j < InArray.Num(); ++j)
        {
            if (!Processed[j] && InArray[i].Equals(InArray[j], Tolerance))
            {
                CurrentGroup.Add(j);
                Processed[j] = true;
            }
        }
        
        // 如果找到重复项（组内超过1个元素）
        if (CurrentGroup.Num() > 1)
        {
            // 将所有索引添加到输出数组
            DuplicateIndices.Append(CurrentGroup);
            // 对于每个索引都添加对应的向量值
            for (int32 k = 0; k < CurrentGroup.Num(); ++k)
            {
                DuplicateValues.Add(InArray[i]);
            }
        }
        
        // 标记当前索引为已处理
        Processed[i] = true;
    }
}

void USortLibrary::SliceActorArrayByIndices(const TArray<AActor*>& InArray, int32 StartIndex, int32 EndIndex,
    TArray<AActor*>& OutArray)
{
    // 清空输出数组
    OutArray.Empty();
    
    // 验证索引范围
    if (InArray.Num() == 0 || !InArray.IsValidIndex(StartIndex) || !InArray.IsValidIndex(EndIndex) || StartIndex > EndIndex)
    {
        return;
    }
    
    // 截取数组
    for (int32 i = StartIndex; i <= EndIndex; ++i)
    {
        OutArray.Add(InArray[i]);
    }
}

void USortLibrary::SliceFloatArrayByIndices(const TArray<float>& InArray, int32 StartIndex, int32 EndIndex,
    TArray<float>& OutArray)
{
    // 清空输出数组
    OutArray.Empty();
    
    // 验证索引范围
    if (InArray.Num() == 0 || !InArray.IsValidIndex(StartIndex) || !InArray.IsValidIndex(EndIndex) || StartIndex > EndIndex)
    {
        return;
    }
    
    // 截取数组
    for (int32 i = StartIndex; i <= EndIndex; ++i)
    {
        OutArray.Add(InArray[i]);
    }
}

void USortLibrary::SliceIntegerArrayByIndices(const TArray<int32>& InArray, int32 StartIndex, int32 EndIndex,
    TArray<int32>& OutArray)
{
    // 清空输出数组
    OutArray.Empty();
    
    // 验证索引范围
    if (InArray.Num() == 0 || !InArray.IsValidIndex(StartIndex) || !InArray.IsValidIndex(EndIndex) || StartIndex > EndIndex)
    {
        return;
    }
    
    // 截取数组
    for (int32 i = StartIndex; i <= EndIndex; ++i)
    {
        OutArray.Add(InArray[i]);
    }
}

void USortLibrary::SliceVectorArrayByIndices(const TArray<FVector>& InArray, int32 StartIndex, int32 EndIndex,
    TArray<FVector>& OutArray)
{
    // 清空输出数组
    OutArray.Empty();
    
    // 验证索引范围
    if (InArray.Num() == 0 || !InArray.IsValidIndex(StartIndex) || !InArray.IsValidIndex(EndIndex) || StartIndex > EndIndex)
    {
        return;
    }
    
    // 截取数组
    for (int32 i = StartIndex; i <= EndIndex; ++i)
    {
        OutArray.Add(InArray[i]);
    }
}

void USortLibrary::SliceFloatArrayByValue(const TArray<float>& InArray, float MinValue, float MaxValue,
    TArray<float>& OutArray, TArray<int32>& Indices)
{
    // 清空输出数组
    OutArray.Empty();
    Indices.Empty();
    
    // 遍历输入数组
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        float Value = InArray[i];
        // 检查值是否在范围内
        if (Value >= MinValue && Value <= MaxValue)
        {
            OutArray.Add(Value);
            Indices.Add(i);
        }
    }
}

void USortLibrary::SliceIntegerArrayByValue(const TArray<int32>& InArray, int32 MinValue, int32 MaxValue,
    TArray<int32>& OutArray, TArray<int32>& Indices)
{
    // 清空输出数组
    OutArray.Empty();
    Indices.Empty();
    
    // 遍历输入数组
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        int32 Value = InArray[i];
        // 检查值是否在范围内
        if (Value >= MinValue && Value <= MaxValue)
        {
            OutArray.Add(Value);
            Indices.Add(i);
        }
    }
}

void USortLibrary::SliceVectorArrayByLength(const TArray<FVector>& InArray, float MinLength, float MaxLength,
    TArray<FVector>& OutArray, TArray<int32>& Indices, TArray<float>& Lengths)
{
    // 清空输出数组
    OutArray.Empty();
    Indices.Empty();
    Lengths.Empty();
    
    // 遍历输入数组
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        float Length = InArray[i].Size();
        // 检查长度是否在范围内
        if (Length >= MinLength && Length <= MaxLength)
        {
            OutArray.Add(InArray[i]);
            Indices.Add(i);
            Lengths.Add(Length);
        }
    }
}

void USortLibrary::SliceVectorArrayByComponent(const TArray<FVector>& InArray, ECoordinateAxis Axis,
    float MinValue, float MaxValue, TArray<FVector>& OutArray, TArray<int32>& Indices, TArray<float>& AxisValues)
{
    // 清空输出数组
    OutArray.Empty();
    Indices.Empty();
    AxisValues.Empty();
    
    // 遍历输入数组
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        float Value = 0.0f;
        switch(Axis)
        {
            case ECoordinateAxis::X:
                Value = InArray[i].X;
                break;
            case ECoordinateAxis::Y:
                Value = InArray[i].Y;
                break;
            case ECoordinateAxis::Z:
                Value = InArray[i].Z;
                break;
        }
        
        // 检查值是否在范围内
        if (Value >= MinValue && Value <= MaxValue)
        {
            OutArray.Add(InArray[i]);
            Indices.Add(i);
            AxisValues.Add(Value);
        }
    }
}

void USortLibrary::ReverseFloatArray(const TArray<float>& InArray, TArray<float>& OutArray)
{
    // 清空输出数组
    OutArray.Empty(InArray.Num());
    
    // 从后向前添加元素
    for (int32 i = InArray.Num() - 1; i >= 0; --i)
    {
        OutArray.Add(InArray[i]);
    }
}

void USortLibrary::ReverseIntegerArray(const TArray<int32>& InArray, TArray<int32>& OutArray)
{
    // 清空输出数组
    OutArray.Empty(InArray.Num());
    
    // 从后向前添加元素
    for (int32 i = InArray.Num() - 1; i >= 0; --i)
    {
        OutArray.Add(InArray[i]);
    }
}

void USortLibrary::ReverseVectorArray(const TArray<FVector>& InArray, TArray<FVector>& OutArray)
{
    // 清空输出数组
    OutArray.Empty(InArray.Num());
    
    // 从后向前添加元素
    for (int32 i = InArray.Num() - 1; i >= 0; --i)
    {
        OutArray.Add(InArray[i]);
    }
}

void USortLibrary::ReverseActorArray(const TArray<AActor*>& InArray, TArray<AActor*>& OutArray)
{
    // 清空输出数组
    OutArray.Empty(InArray.Num());
    
    // 从后向前添加元素
    for (int32 i = InArray.Num() - 1; i >= 0; --i)
    {
        OutArray.Add(InArray[i]);
    }
}

void USortLibrary::ReverseStringArray(const TArray<FString>& InArray, TArray<FString>& OutArray)
{
    // 清空输出数组
    OutArray.Empty(InArray.Num());
    
    // 从后向前添加元素
    for (int32 i = InArray.Num() - 1; i >= 0; --i)
    {
        OutArray.Add(InArray[i]);
    }
}