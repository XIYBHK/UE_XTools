/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "SortLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/LogMacros.h"
#include "Internationalization/Text.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "Algo/Reverse.h"
#include "SortAPI.h"
#include "XToolsErrorReporter.h"

/**
 * 自然排序比较器，用于处理字符串中的数字和中文
 */
struct FNaturalSortComparator
{
    static int32 Compare(const FString& A, const FString& B)
    {
        int32 IndexA = 0, IndexB = 0;
        while (IndexA < A.Len() && IndexB < B.Len())
        {
            // 如果两者都是数字，则按数值比较
            if (FChar::IsDigit(A[IndexA]) && FChar::IsDigit(B[IndexB]))
            {
                int64 NumA = 0, NumB = 0;
                while (IndexA < A.Len() && FChar::IsDigit(A[IndexA]))
                {
                    NumA = NumA * 10 + (A[IndexA++] - '0');
                }
                while (IndexB < B.Len() && FChar::IsDigit(B[IndexB]))
                {
                    NumB = NumB * 10 + (B[IndexB++] - '0');
                }

                if (NumA != NumB)
                {
                    return NumA < NumB ? -1 : 1;
                }
            }
            else
            {
                // 提取非数字部分进行比较
                int32 StartA = IndexA, StartB = IndexB;
                while (IndexA < A.Len() && !FChar::IsDigit(A[IndexA]))
                {
                    IndexA++;
                }
                while (IndexB < B.Len() && !FChar::IsDigit(B[IndexB]))
                {
                    IndexB++;
                }

                FString SubA = A.Mid(StartA, IndexA - StartA);
                FString SubB = B.Mid(StartB, IndexB - StartB);

                // 使用FText进行文化敏感的比较，支持中文拼音
                FText TextA = FText::FromString(SubA);
                FText TextB = FText::FromString(SubB);
                int32 Result = TextA.CompareTo(TextB, ETextComparisonLevel::Primary);

                if (Result != 0)
                {
                    return Result;
                }
            }
        }

        // 如果一个字符串是另一个的前缀，则较短的优先
        if (A.Len() < B.Len()) return -1;
        if (A.Len() > B.Len()) return 1;

        return 0;
    }
};

namespace SortLibrary_Private
{
    /**
     * @brief 一个通用的排序模板函数，用于处理各种类型的稳定排序。
     * 此函数封装了创建键值对、对其进行排序，然后填充输出数组的通用模式。
     * @tparam ElementType 输入数组中元素的类型 (例如, AActor*, int32, FVector)。
     * @tparam SortKeyType 用于排序的键的类型 (例如, float, int32)。
     * @param InArray 要排序的元素数组。
     * @param bAscending true 为升序排序, false 为降序。
     * @param GetSortKey 一个Lambda函数，接受一个 ElementType 并返回其 SortKeyType。
     * @param SortedArray 用于存放已排序元素的输出数组。
     * @param OriginalIndices 用于存放已排序元素原始索引的输出数组。
     * @param SortedKeys (可选) 用于存放已排序键的输出数组。
     */
    template<typename ElementType, typename SortKeyType>
    void GenericSort(
        const TArray<ElementType>& InArray,
        bool bAscending,
        TFunctionRef<SortKeyType(const ElementType&)> GetSortKey,
        TArray<ElementType>& SortedArray,
        TArray<int32>& OriginalIndices,
        TArray<SortKeyType>* SortedKeys = nullptr)
    {
        // 如果输入为空，则清空输出数组
        if (InArray.IsEmpty())
        {
            SortedArray.Empty();
            OriginalIndices.Empty();
            if (SortedKeys)
            {
                SortedKeys->Empty();
            }
            return;
        }

        // 内部结构，用于将值与其排序键和原始索引配对，以实现稳定排序
        struct FSortPair
        {
            ElementType Value;
            SortKeyType Key;
            int32 OriginalIndex;

            // 用于排序的比较运算符（稳定排序的并列项处理）
            bool operator<(const FSortPair& Other) const
            {
                if constexpr (TIsFloatingPoint<SortKeyType>::Value)
                {
                    if (FMath::IsNaN(Key)) return false;
                    if (FMath::IsNaN(Other.Key)) return true;
                }

                if (Key < Other.Key)
                {
                    return true;
                }
                if (!(Other.Key < Key))
                {
                    // Key 等价时按原始索引作为稳定并列项
                    return OriginalIndex < Other.OriginalIndex;
                }
                return false;
            }
        };

        TArray<FSortPair> Pairs;
        Pairs.Reserve(InArray.Num());

        // 填充配对数组，如果ElementType是指针，则跳过无效对象
        for (int32 i = 0; i < InArray.Num(); ++i)
        {
            if constexpr (TIsPointer<ElementType>::Value)
            {
                if (!IsValid(InArray[i]))
                {
                    continue;
                }
            }
            Pairs.Emplace(FSortPair{InArray[i], GetSortKey(InArray[i]), i});
        }
        
        // 执行排序
        if (bAscending)
        {
            Pairs.Sort([](const FSortPair& A, const FSortPair& B) { return A < B; });
        }
        else
        {
            Pairs.Sort([](const FSortPair& A, const FSortPair& B) { return B < A; });
        }

        // 从已排序的配对中填充输出数组
        const int32 SortedNum = Pairs.Num();
        SortedArray.SetNum(SortedNum);
        OriginalIndices.SetNum(SortedNum);
        if (SortedKeys)
        {
            SortedKeys->SetNum(SortedNum);
        }

        for (int32 i = 0; i < SortedNum; ++i)
        {
            SortedArray[i] = Pairs[i].Value;
            OriginalIndices[i] = Pairs[i].OriginalIndex;
            if (SortedKeys)
            {
                (*SortedKeys)[i] = Pairs[i].Key;
            }
        }
    }
} // 命名空间 SortLibrary_Private

//~ Actor排序函数
// =================================================================================================

void USortLibrary::SortActorsByDistance(const TArray<AActor*>& Actors, const FVector& Location, 
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedDistances,
    bool bAscending, bool b2DDistance)
{
    auto GetDistance = [&](const AActor* Actor) -> float
    {
        const FVector ActorLocation = Actor->GetActorLocation();
        return b2DDistance ? FVector::Dist2D(ActorLocation, Location) : FVector::Dist(ActorLocation, Location);
    };

    SortLibrary_Private::GenericSort<AActor*, float>(Actors, bAscending, GetDistance, SortedActors, OriginalIndices, &SortedDistances);
}

void USortLibrary::SortActorsByHeight(const TArray<AActor*>& Actors, 
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, bool bAscending)
{
    auto GetHeight = [](const AActor* Actor) -> float
    {
        return Actor->GetActorLocation().Z;
    };
    SortLibrary_Private::GenericSort<AActor*, float>(Actors, bAscending, GetHeight, SortedActors, OriginalIndices);
}

void USortLibrary::SortActorsByAxis(const TArray<AActor*>& Actors, ECoordinateAxis Axis,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAxisValues, bool bAscending)
{
    auto GetAxisValue = [&](const AActor* Actor) -> float
    {
        const FVector Location = Actor->GetActorLocation();
        switch (Axis)
        {
            case ECoordinateAxis::X: return Location.X;
            case ECoordinateAxis::Y: return Location.Y;
            case ECoordinateAxis::Z: return Location.Z;
            default: return 0.0f;
        }
    };
    SortLibrary_Private::GenericSort<AActor*, float>(Actors, bAscending, GetAxisValue, SortedActors, OriginalIndices, &SortedAxisValues);
}

void USortLibrary::SortActorsByAngle(const TArray<AActor*>& Actors, const FVector& Center, const FVector& Direction,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAngles,
    bool bAscending, bool b2DAngle)
{
    const FVector NormalizedDirection = b2DAngle ? FVector(Direction.X, Direction.Y, 0).GetSafeNormal() : Direction.GetSafeNormal();

    auto GetAngle = [&](const AActor* Actor) -> float
    {
        FVector ToActor = Actor->GetActorLocation() - Center;
        if (b2DAngle)
        {
            ToActor.Z = 0.0f;
        }
        
        ToActor.Normalize();

        const float Dot = FVector::DotProduct(NormalizedDirection, ToActor);
        float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

        // 对于2D角度，判断是否为优角 (> 180度)
        if (b2DAngle)
        {
            const float CrossZ = NormalizedDirection.X * ToActor.Y - NormalizedDirection.Y * ToActor.X;
            if (CrossZ < 0.0f)
            {
                AngleDegrees = 360.0f - AngleDegrees;
            }
        }
        return AngleDegrees;
    };

    SortLibrary_Private::GenericSort<AActor*, float>(Actors, bAscending, GetAngle, SortedActors, OriginalIndices, &SortedAngles);
}

void USortLibrary::SortActorsByAzimuth(const TArray<AActor*>& Actors, const FVector& Center,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAzimuths, bool bAscending)
{
    auto GetAzimuth = [&](const AActor* Actor) -> float
    {
        const FVector ToActor = Actor->GetActorLocation() - Center;
        const float AngleRadians = FMath::Atan2(ToActor.Y, ToActor.X);
        const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);
        // 从标准角度转换为方位角 (正北=0, 正东=90)
        float Azimuth = 90.0f - AngleDegrees;
        if (Azimuth < 0.0f)
        {
            Azimuth += 360.0f;
        }
        return Azimuth;
    };

    SortLibrary_Private::GenericSort<AActor*, float>(Actors, bAscending, GetAzimuth, SortedActors, OriginalIndices, &SortedAzimuths);
}

void USortLibrary::SortActorsByAngleAndDistance(const TArray<AActor*>& Actors, const FVector& Center,
    const FVector& Direction, float MaxAngle, float MaxDistance, float AngleWeight, float DistanceWeight,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices,
    TArray<float>& SortedAngles, TArray<float>& SortedDistances, bool bAscending, bool b2DAngle)
{
    if (Actors.IsEmpty())
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        SortedAngles.Empty();
        SortedDistances.Empty();
        return;
    }

    const FVector NormalizedDirection = b2DAngle ? FVector(Direction.X, Direction.Y, 0).GetSafeNormal() : Direction.GetSafeNormal();

    struct FActorSortInfo
    {
        AActor* Actor;
        int32 OriginalIndex;
        float Angle;
        float Distance;
        float Score;

        bool operator<(const FActorSortInfo& Other) const { return Score < Other.Score; }
    };

    TArray<FActorSortInfo> FilteredPairs;
    FilteredPairs.Reserve(Actors.Num());

    // 第一次遍历: 过滤并计算值
    for (int32 i = 0; i < Actors.Num(); ++i)
    {
        AActor* Actor = Actors[i];
        if (!IsValid(Actor)) continue;

        const FVector ActorLocation = Actor->GetActorLocation();
        FVector ToActor = ActorLocation - Center;
        
        // 计算距离
        const float Distance = b2DAngle ? FVector::Dist2D(ActorLocation, Center) : ToActor.Size();
        if (MaxDistance > 0.0f && Distance > MaxDistance) continue;
        
        // 计算角度
        if (b2DAngle) ToActor.Z = 0.0f;
        ToActor.Normalize();
        const float Dot = FVector::DotProduct(NormalizedDirection, ToActor);
        float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));
        if (b2DAngle)
        {
            const float CrossZ = NormalizedDirection.X * ToActor.Y - NormalizedDirection.Y * ToActor.X;
            if (CrossZ < 0.0f) Angle = 360.0f - Angle;
        }
        if (MaxAngle > 0.0f && Angle > MaxAngle) continue;
        
        FilteredPairs.Add({Actor, i, Angle, Distance, 0.0f});
    }

    if (FilteredPairs.IsEmpty())
    {
        SortedActors.Empty();
        OriginalIndices.Empty();
        SortedAngles.Empty();
        SortedDistances.Empty();
        return;
    }

    // 第二次遍历: 归一化并计算得分
    float MaxFoundAngle = 0.0f, MaxFoundDistance = 0.0f;
    for (const auto& Pair : FilteredPairs)
    {
        MaxFoundAngle = FMath::Max(MaxFoundAngle, Pair.Angle);
        MaxFoundDistance = FMath::Max(MaxFoundDistance, Pair.Distance);
    }
    
    // 避免除以零
    if (FMath::IsNearlyZero(MaxFoundAngle)) MaxFoundAngle = 1.0f;
    if (FMath::IsNearlyZero(MaxFoundDistance)) MaxFoundDistance = 1.0f;

    const float TotalWeight = AngleWeight + DistanceWeight;
    const bool bUseDefaultSort = FMath::IsNearlyZero(TotalWeight);

    for (auto& Pair : FilteredPairs)
    {
        const float NormAngle = Pair.Angle / MaxFoundAngle;
        const float NormDistance = Pair.Distance / MaxFoundDistance;
        Pair.Score = bUseDefaultSort ? NormAngle : (NormAngle * AngleWeight + NormDistance * DistanceWeight) / TotalWeight;
    }
    
    // 排序
    if (bAscending)
    {
        FilteredPairs.Sort();
    }
    else
    {
        FilteredPairs.Sort([](const auto& A, const auto& B){ return B < A; });
    }

    // 填充输出数组
    const int32 Count = FilteredPairs.Num();
    SortedActors.SetNum(Count);
    OriginalIndices.SetNum(Count);
    SortedAngles.SetNum(Count);
    SortedDistances.SetNum(Count);
    for (int32 i = 0; i < Count; ++i)
    {
        SortedActors[i] = FilteredPairs[i].Actor;
        OriginalIndices[i] = FilteredPairs[i].OriginalIndex;
        SortedAngles[i] = FilteredPairs[i].Angle;
        SortedDistances[i] = FilteredPairs[i].Distance;
    }
}

//~ 基础类型排序函数
// =================================================================================================

void USortLibrary::SortIntegerArray(const TArray<int32>& InArray, 
    TArray<int32>& SortedArray, TArray<int32>& OriginalIndices, bool bAscending)
{
    SortLibrary_Private::GenericSort<int32, int32>(InArray, bAscending, [](int32 Val){ return Val; }, SortedArray, OriginalIndices);
}

void USortLibrary::SortFloatArray(const TArray<float>& InArray, 
    TArray<float>& SortedArray, TArray<int32>& OriginalIndices, bool bAscending)
{
    SortLibrary_Private::GenericSort<float, float>(InArray, bAscending, [](float Val){ return Val; }, SortedArray, OriginalIndices);
}

void USortLibrary::SortStringArray(const TArray<FString>& InArray, 
    TArray<FString>& SortedArray, TArray<int32>& OriginalIndices, bool bAscending)
{
    // 使用自然排序来处理字符串中的数字部分
    if (InArray.IsEmpty())
    {
        SortedArray.Empty();
        OriginalIndices.Empty();
        return;
    }

    struct FSortPair
    {
        FString Value;
        int32 OriginalIndex;

        bool operator<(const FSortPair& Other) const
        {
            const int32 Cmp = FNaturalSortComparator::Compare(Value, Other.Value);
            if (Cmp < 0) return true;
            if (Cmp > 0) return false;
            return OriginalIndex < Other.OriginalIndex;
        }
    };

    TArray<FSortPair> Pairs;
    Pairs.Reserve(InArray.Num());

    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        Pairs.Emplace(FSortPair{InArray[i], i});
    }

    if (bAscending)
    {
        Pairs.Sort();
    }
    else
    {
        Pairs.Sort([](const FSortPair& A, const FSortPair& B) { return B < A; });
    }

    const int32 SortedNum = Pairs.Num();
    SortedArray.SetNum(SortedNum);
    OriginalIndices.SetNum(SortedNum);

    for (int32 i = 0; i < SortedNum; ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }
}

void USortLibrary::SortNameArray(const TArray<FName>& InArray, 
    TArray<FName>& SortedArray, TArray<int32>& OriginalIndices, bool bAscending)
{
    // 使用自然排序来处理名称中的数字部分
    if (InArray.IsEmpty())
    {
        SortedArray.Empty();
        OriginalIndices.Empty();
        return;
    }

    struct FSortPair
    {
        FName Value;
        FString StringValue;
        int32 OriginalIndex;

        bool operator<(const FSortPair& Other) const
        {
            const int32 Cmp = FNaturalSortComparator::Compare(StringValue, Other.StringValue);
            if (Cmp < 0) return true;
            if (Cmp > 0) return false;
            return OriginalIndex < Other.OriginalIndex;
        }
    };

    TArray<FSortPair> Pairs;
    Pairs.Reserve(InArray.Num());

    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        FString StringValue = InArray[i].ToString();
        Pairs.Emplace(FSortPair{InArray[i], MoveTemp(StringValue), i});
    }

    if (bAscending)
    {
        Pairs.Sort();
    }
    else
    {
        Pairs.Sort([](const FSortPair& A, const FSortPair& B) { return B < A; });
    }

    const int32 SortedNum = Pairs.Num();
    SortedArray.SetNum(SortedNum);
    OriginalIndices.SetNum(SortedNum);

    for (int32 i = 0; i < SortedNum; ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }
}

//~ 向量排序函数
// =================================================================================================

void USortLibrary::SortVectorsByProjection(const TArray<FVector>& Vectors, const FVector& Direction, 
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedProjections, bool bAscending)
{
    const FVector NormalizedDirection = Direction.GetSafeNormal();
    auto GetProjection = [&](const FVector& Vector)
    {
        return FVector::DotProduct(Vector, NormalizedDirection);
    };
    SortLibrary_Private::GenericSort<FVector, float>(Vectors, bAscending, GetProjection, SortedVectors, OriginalIndices, &SortedProjections);
}

void USortLibrary::SortVectorsByLength(const TArray<FVector>& Vectors, 
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedLengths, bool bAscending)
{
    auto GetLength = [](const FVector& Vector)
    {
        return Vector.Size();
    };
    SortLibrary_Private::GenericSort<FVector, float>(Vectors, bAscending, GetLength, SortedVectors, OriginalIndices, &SortedLengths);
}

void USortLibrary::SortVectorsByAxis(const TArray<FVector>& Vectors, ECoordinateAxis Axis, 
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedAxisValues, bool bAscending)
{
    auto GetAxisValue = [&](const FVector& Vector) -> float
    {
        switch (Axis)
        {
            case ECoordinateAxis::X: return Vector.X;
            case ECoordinateAxis::Y: return Vector.Y;
            case ECoordinateAxis::Z: return Vector.Z;
            default: return 0.0f;
        }
    };
    SortLibrary_Private::GenericSort<FVector, float>(Vectors, bAscending, GetAxisValue, SortedVectors, OriginalIndices, &SortedAxisValues);
}

namespace SortLibrary_Private
{
    /**
     * 通用数组截取模板函数，减少重复代码
     */
    template<typename ElementType>
    void GenericSliceByIndices(const TArray<ElementType>& InArray, int32 StartIndex, int32 EndIndex, TArray<ElementType>& OutArray)
    {
        OutArray.Empty();
        if (InArray.IsValidIndex(StartIndex) && InArray.IsValidIndex(EndIndex) && StartIndex <= EndIndex)
        {
            const int32 Count = EndIndex - StartIndex + 1;
            OutArray.Reserve(Count);
            for (int32 i = StartIndex; i <= EndIndex; ++i)
            {
                OutArray.Add(InArray[i]);
            }
        }
    }
} // namespace SortLibrary_Private

//~ 数组截取函数
// =================================================================================================

void USortLibrary::SliceActorArrayByIndices(const TArray<AActor*>& InArray, int32 StartIndex, int32 EndIndex, TArray<AActor*>& OutArray)
{
    SortLibrary_Private::GenericSliceByIndices(InArray, StartIndex, EndIndex, OutArray);
}

void USortLibrary::SliceFloatArrayByIndices(const TArray<float>& InArray, int32 StartIndex, int32 EndIndex, TArray<float>& OutArray)
{
    SortLibrary_Private::GenericSliceByIndices(InArray, StartIndex, EndIndex, OutArray);
}

void USortLibrary::SliceIntegerArrayByIndices(const TArray<int32>& InArray, int32 StartIndex, int32 EndIndex, TArray<int32>& OutArray)
{
    SortLibrary_Private::GenericSliceByIndices(InArray, StartIndex, EndIndex, OutArray);
}

void USortLibrary::SliceVectorArrayByIndices(const TArray<FVector>& InArray, int32 StartIndex, int32 EndIndex, TArray<FVector>& OutArray)
{
    SortLibrary_Private::GenericSliceByIndices(InArray, StartIndex, EndIndex, OutArray);
}

void USortLibrary::SliceFloatArrayByValue(const TArray<float>& InArray, float MinValue, float MaxValue, TArray<float>& OutArray, TArray<int32>& Indices)
{
    OutArray.Empty();
    Indices.Empty();
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        const float& Value = InArray[i];
        if (Value >= MinValue && Value <= MaxValue)
        {
            OutArray.Add(Value);
            Indices.Add(i);
        }
    }
}

void USortLibrary::SliceIntegerArrayByValue(const TArray<int32>& InArray, int32 MinValue, int32 MaxValue, TArray<int32>& OutArray, TArray<int32>& Indices)
{
    OutArray.Empty();
    Indices.Empty();
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        const int32& Value = InArray[i];
        if (Value >= MinValue && Value <= MaxValue)
        {
            OutArray.Add(Value);
            Indices.Add(i);
        }
    }
}

void USortLibrary::SliceVectorArrayByLength(const TArray<FVector>& InArray, float MinLength, float MaxLength, TArray<FVector>& OutArray, TArray<int32>& Indices, TArray<float>& Lengths)
{
    OutArray.Empty();
    Indices.Empty();
    Lengths.Empty();
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        const float Length = InArray[i].Size();
        if (Length >= MinLength && Length <= MaxLength)
        {
            OutArray.Add(InArray[i]);
            Indices.Add(i);
            Lengths.Add(Length);
        }
    }
}

void USortLibrary::SliceVectorArrayByComponent(const TArray<FVector>& InArray, ECoordinateAxis Axis, float MinValue, float MaxValue, TArray<FVector>& OutArray, TArray<int32>& Indices, TArray<float>& AxisValues)
{
    OutArray.Empty();
    Indices.Empty();
    AxisValues.Empty();
    for (int32 i = 0; i < InArray.Num(); ++i)
    {
        float Value = 0.0f;
        switch(Axis)
        {
            case ECoordinateAxis::X: Value = InArray[i].X; break;
            case ECoordinateAxis::Y: Value = InArray[i].Y; break;
            case ECoordinateAxis::Z: Value = InArray[i].Z; break;
        }
        if (Value >= MinValue && Value <= MaxValue)
        {
            OutArray.Add(InArray[i]);
            Indices.Add(i);
            AxisValues.Add(Value);
        }
    }
}


//~ 数组反转函数
// =================================================================================================

void USortLibrary::ReverseFloatArray(const TArray<float>& InArray, TArray<float>& OutArray)
{
    OutArray = InArray;
    Algo::Reverse(OutArray);
}

void USortLibrary::ReverseIntegerArray(const TArray<int32>& InArray, TArray<int32>& OutArray)
{
    OutArray = InArray;
    Algo::Reverse(OutArray);
}

void USortLibrary::ReverseVectorArray(const TArray<FVector>& InArray, TArray<FVector>& OutArray)
{
    OutArray = InArray;
    Algo::Reverse(OutArray);
}

void USortLibrary::ReverseActorArray(const TArray<AActor*>& InArray, TArray<AActor*>& OutArray)
{
    OutArray = InArray;
    Algo::Reverse(OutArray);
}

void USortLibrary::ReverseStringArray(const TArray<FString>& InArray, TArray<FString>& OutArray)
{
    OutArray = InArray;
    Algo::Reverse(OutArray);
}

//~ 数组去重函数
// =================================================================================================

void USortLibrary::RemoveDuplicateActors(const TArray<AActor*>& InArray, TArray<AActor*>& OutArray)
{
    TSet<AActor*> UniqueActors;
    UniqueActors.Reserve(InArray.Num());
    for (AActor* Actor : InArray)
    {
        if (IsValid(Actor))
        {
            UniqueActors.Add(Actor);
        }
    }
    OutArray = UniqueActors.Array();
}

void USortLibrary::RemoveDuplicateFloats(const TArray<float>& InArray, TArray<float>& OutArray, float Tolerance)
{
    OutArray.Empty();
    if (InArray.IsEmpty()) return;

    TArray<float> SortedCopy = InArray;
    SortedCopy.Sort();

    OutArray.Add(SortedCopy[0]);
    for (int32 i = 1; i < SortedCopy.Num(); ++i)
    {
        if (!FMath::IsNearlyEqual(SortedCopy[i], OutArray.Last(), Tolerance))
        {
            OutArray.Add(SortedCopy[i]);
        }
    }
}

void USortLibrary::RemoveDuplicateIntegers(const TArray<int32>& InArray, TArray<int32>& OutArray)
{
    TSet<int32> UniqueInts(InArray);
    OutArray = UniqueInts.Array();
}

// 为TSet提供不区分大小写的字符串比较功能，符合UE最佳实践
struct FCaseInsensitiveStringKeyFuncs
{
    // TSet needs us to define these types
    using KeyInitType = const FString&;
    using ElementInitType = const FString&;
    static constexpr bool bAllowDuplicateKeys = false;

    /**
     * @return The key from an element.
     */
    static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
    {
        return Element;
    }

    /**
     * @return True if the keys are equal.
     */
    static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
    {
        return A.Equals(B, ESearchCase::IgnoreCase);
    }

    /**
     * @return The hash for a key.
     */
    static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
    {
        return GetTypeHash(Key.ToLower());
    }
};

void USortLibrary::RemoveDuplicateStrings(const TArray<FString>& InArray, TArray<FString>& OutArray, bool bCaseSensitive)
{
    OutArray.Empty();
    if (InArray.IsEmpty()) return;

    if (bCaseSensitive)
    {
        // 区分大小写：使用标准的TSet，性能很好
        TSet<FString> UniqueStrings(InArray);
        OutArray = UniqueStrings.Array();
    }
    else
    {
        // 不区分大小写：使用自定义KeyFuncs的TSet，实现O(N)的性能
        TSet<FString, FCaseInsensitiveStringKeyFuncs> UniqueStrings;
        UniqueStrings.Reserve(InArray.Num());
        for (const FString& Str : InArray)
        {
            UniqueStrings.Add(Str);
        }
        OutArray = UniqueStrings.Array();
    }
}

void USortLibrary::RemoveDuplicateVectors(const TArray<FVector>& InArray, TArray<FVector>& OutArray, float Tolerance)
{
    OutArray.Empty();
    if (InArray.IsEmpty()) return;

    TArray<FVector> SortedCopy = InArray;
    SortedCopy.Sort([](const FVector& A, const FVector& B) {
        if (A.X != B.X) return A.X < B.X;
        if (A.Y != B.Y) return A.Y < B.Y;
        return A.Z < B.Z;
    });

    OutArray.Reserve(SortedCopy.Num());
    OutArray.Add(SortedCopy[0]);
    for (int32 i = 1; i < SortedCopy.Num(); ++i)
    {
        if (!SortedCopy[i].Equals(OutArray.Last(), Tolerance))
        {
            OutArray.Add(SortedCopy[i]);
        }
    }
    OutArray.Shrink();
}

void USortLibrary::FindDuplicateVectors(const TArray<FVector>& InArray, TArray<int32>& DuplicateIndices, TArray<FVector>& DuplicateValues, float Tolerance)
{
    DuplicateIndices.Empty();
    DuplicateValues.Empty();
    if (InArray.Num() < 2) return;

    struct FVectorIndexPair { FVector Vector; int32 Index; };
    TArray<FVectorIndexPair> Pairs;
    Pairs.Reserve(InArray.Num());
    for(int32 i=0; i<InArray.Num(); ++i)
    {
        Pairs.Add({InArray[i], i});
    }

    Pairs.Sort([](const FVectorIndexPair& A, const FVectorIndexPair& B) {
        const FVector& V1 = A.Vector;
        const FVector& V2 = B.Vector;
        if (V1.X != V2.X) return V1.X < V2.X;
        if (V1.Y != V2.Y) return V1.Y < V2.Y;
        return V1.Z < V2.Z;
    });

    TArray<bool> IsDuplicate;
    IsDuplicate.Init(false, InArray.Num());
    bool bInDuplicateGroup = false;

    for (int32 i = 0; i < Pairs.Num() - 1; ++i)
    {
        if (Pairs[i].Vector.Equals(Pairs[i+1].Vector, Tolerance))
        {
            IsDuplicate[Pairs[i].Index] = true;
            IsDuplicate[Pairs[i+1].Index] = true;
        }
    }
    
    for(int32 i=0; i < IsDuplicate.Num(); ++i)
    {
        if(IsDuplicate[i])
        {
            DuplicateIndices.Add(i);
            DuplicateValues.Add(InArray[i]);
        }
    }
}

//~ 通用属性排序函数 (基于GitHub项目的改进)
// =================================================================================================

void USortLibrary::SortArrayByPropertyInPlace(TArray<int32>& TargetArray, FName PropertyName, bool bAscending, TArray<int32>& OriginalIndices)
{
    // 这个函数永远不会被调用，因为它使用CustomThunk
    check(0);
}

DEFINE_FUNCTION(USortLibrary::execSortArrayByPropertyInPlace)
{
    // 使用UE官方的CustomThunk模式 - 参考KismetArrayLibrary
    Stack.MostRecentProperty = nullptr;
    Stack.StepCompiledIn<FArrayProperty>(NULL);
    void* ArrayAddr = Stack.MostRecentPropertyAddress;
    FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

    if (!ArrayProperty)
    {
        Stack.bArrayContextFailed = true;
        return;
    }

    P_GET_PROPERTY(FNameProperty, PropertyName);
    P_GET_UBOOL(bAscending);
    P_GET_TARRAY_REF(int32, OriginalIndices);

    P_FINISH;

    P_NATIVE_BEGIN;
    UE_LOG(LogSort, Warning, TEXT("execSortArrayByPropertyInPlace: ArrayAddr=%p, ArrayProperty=%p, PropertyName=%s"),
        ArrayAddr, ArrayProperty, *PropertyName.ToString());

    if (ArrayAddr && ArrayProperty)
    {
        GenericSortArrayByProperty(ArrayAddr, ArrayProperty, PropertyName, bAscending, OriginalIndices);
    }
    else
    {
        FXToolsErrorReporter::Error(
            LogSort,
            FString::Printf(TEXT("execSortArrayByPropertyInPlace: 无效参数 - ArrayAddr=%p, ArrayProperty=%p"),
                ArrayAddr, ArrayProperty),
            TEXT("execSortArrayByPropertyInPlace"));
        OriginalIndices.Empty();
    }
    P_NATIVE_END;
}

void USortLibrary::GenericSortArrayByProperty(void* TargetArray, FArrayProperty* ArrayProp, FName PropertyName, bool bAscending, TArray<int32>& OriginalIndices)
{
    UE_LOG(LogSort, Warning, TEXT("GenericSortArrayByProperty: 开始执行 - TargetArray=%p, ArrayProp=%p, PropertyName=%s"),
        TargetArray, ArrayProp, *PropertyName.ToString());

    if (!TargetArray || !ArrayProp)
    {
        UE_LOG(LogSort, Error, TEXT("GenericSortArrayByProperty: 参数无效 - TargetArray=%p, ArrayProp=%p"),
            TargetArray, ArrayProp);
        OriginalIndices.Empty();
        return;
    }

    UE_LOG(LogSort, Warning, TEXT("GenericSortArrayByProperty: 创建ArrayHelper"));
    FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
    UE_LOG(LogSort, Warning, TEXT("GenericSortArrayByProperty: ArrayHelper创建成功，数组大小=%d"), ArrayHelper.Num());
    if (ArrayHelper.Num() < 2)
    {
        // 数组太小，不需要排序
        OriginalIndices.SetNum(ArrayHelper.Num());
        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            OriginalIndices[i] = i;
        }
        return;
    }

    FProperty* SortProp = nullptr;

    // 根据数组元素类型查找排序属性
    if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(ArrayProp->Inner))
    {
        // 对象数组：在类中查找属性
        SortProp = FindPropertyByName<FProperty>(ObjectProp->PropertyClass, PropertyName);
    }
    else if (const FStructProperty* StructProp = CastField<FStructProperty>(ArrayProp->Inner))
    {
        // 结构体数组：在结构体中查找属性
        SortProp = FindPropertyByName<FProperty>(StructProp->Struct, PropertyName);
    }
    else
    {
        // 基础类型数组：直接使用元素属性
        SortProp = ArrayProp->Inner;
    }

    if (!SortProp)
    {
        UE_LOG(LogSort, Warning, TEXT("无法找到属性: %s"), *PropertyName.ToString());
        // 返回原始索引顺序
        OriginalIndices.SetNum(ArrayHelper.Num());
        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            OriginalIndices[i] = i;
        }
        return;
    }

    // 创建索引数组并进行排序
    TArray<int32> Indices;
    Indices.SetNum(ArrayHelper.Num());
    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        Indices[i] = i;
    }

    // 计算深度限制（IntroSort模式：2*log2(n)）
    const int32 DepthLimit = ArrayHelper.Num() > 0 ? 2 * FMath::CeilLogTwo(ArrayHelper.Num()) : 0;
    
    // 使用带深度限制的快速排序（IntroSort）
    QuickSortByProperty(ArrayHelper, ArrayProp->Inner, SortProp, Indices, 0, ArrayHelper.Num() - 1, bAscending, DepthLimit);

    // 根据排序后的索引重新排列数组
    TArray<uint8> TempStorage;
    const int32 ElementSize = ArrayProp->Inner->GetSize();
    TempStorage.SetNumUninitialized(ElementSize * ArrayHelper.Num());

    // 复制排序后的元素到临时存储
    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        const int32 SourceIndex = Indices[i];
        void* SourcePtr = ArrayHelper.GetRawPtr(SourceIndex);
        void* DestPtr = TempStorage.GetData() + (i * ElementSize);
        ArrayProp->Inner->CopyCompleteValue(DestPtr, SourcePtr);
    }

    // 将排序后的元素复制回原数组
    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        void* SourcePtr = TempStorage.GetData() + (i * ElementSize);
        void* DestPtr = ArrayHelper.GetRawPtr(i);
        ArrayProp->Inner->CopyCompleteValue(DestPtr, SourcePtr);
    }

    // 返回原始索引
    OriginalIndices = MoveTemp(Indices);
}

template<typename T>
T* USortLibrary::FindPropertyByName(const UStruct* Owner, FName FieldName)
{
    if (FieldName.IsNone() || !Owner)
    {
        return nullptr;
    }

    // 通过比较FName（整数）而不是字符串来搜索，性能更好
    for (TFieldIterator<T> It(Owner); It; ++It)
    {
        if ((It->GetFName() == FieldName) || (It->GetAuthoredName() == FieldName.ToString()))
        {
            return *It;
        }
    }

    return nullptr;
}

bool USortLibrary::ComparePropertyValues(const FProperty* Property, const void* LeftValuePtr, const void* RightValuePtr, bool bAscending)
{
    if (!Property || !LeftValuePtr || !RightValuePtr)
    {
        return false;
    }

    bool bResult = false;

    // 根据属性类型进行比较
    if (const FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
    {
        if (NumericProp->IsFloatingPoint())
        {
            const double LeftValue = NumericProp->GetFloatingPointPropertyValue(LeftValuePtr);
            const double RightValue = NumericProp->GetFloatingPointPropertyValue(RightValuePtr);
            bResult = LeftValue < RightValue;
        }
        else if (NumericProp->IsInteger())
        {
            const int64 LeftValue = NumericProp->GetSignedIntPropertyValue(LeftValuePtr);
            const int64 RightValue = NumericProp->GetSignedIntPropertyValue(RightValuePtr);
            bResult = LeftValue < RightValue;
        }
    }
    else if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        const bool LeftValue = BoolProp->GetPropertyValue(LeftValuePtr);
        const bool RightValue = BoolProp->GetPropertyValue(RightValuePtr);
        bResult = !LeftValue && RightValue; // false < true
    }
    else if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        const FString LeftValue = NameProp->GetPropertyValue(LeftValuePtr).ToString();
        const FString RightValue = NameProp->GetPropertyValue(RightValuePtr).ToString();
        bResult = FNaturalSortComparator::Compare(LeftValue, RightValue) < 0;
    }
    else if (const FStrProperty* StringProp = CastField<FStrProperty>(Property))
    {
        const FString& LeftValue = StringProp->GetPropertyValue(LeftValuePtr);
        const FString& RightValue = StringProp->GetPropertyValue(RightValuePtr);
        bResult = FNaturalSortComparator::Compare(LeftValue, RightValue) < 0;
    }
    #if WITH_EDITOR
    else if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        const FString LeftValue = TextProp->GetPropertyValue(LeftValuePtr).ToString();
        const FString RightValue = TextProp->GetPropertyValue(RightValuePtr).ToString();
        bResult = FNaturalSortComparator::Compare(LeftValue, RightValue) < 0;
    }
    #endif
    else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        const int64 LeftValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(LeftValuePtr);
        const int64 RightValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(RightValuePtr);
        bResult = LeftValue < RightValue;
    }

    return bResult == bAscending;
}

void USortLibrary::HeapSortByProperty(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, TArray<int32>& Indices, int32 Low, int32 High, bool bAscending)
{
    // 使用TFunction替代lambda递归
    TFunction<void(int32, int32)> Heapify;
    Heapify = [&](int32 Root, int32 End)
    {
        int32 Largest = Root;
        int32 Left = 2 * Root + 1 - Low;
        int32 Right = 2 * Root + 2 - Low;

        if (Left <= End - Low)
        {
            void* LeftValue = USortLibrary::GetPropertyValuePtr(ArrayHelper, InnerProp, SortProp, Indices[Left + Low]);
            void* LargestValue = USortLibrary::GetPropertyValuePtr(ArrayHelper, InnerProp, SortProp, Indices[Largest]);
            if (USortLibrary::ComparePropertyValues(SortProp, LargestValue, LeftValue, bAscending))
            {
                Largest = Left + Low;
            }
        }

        if (Right <= End - Low)
        {
            void* RightValue = USortLibrary::GetPropertyValuePtr(ArrayHelper, InnerProp, SortProp, Indices[Right + Low]);
            void* LargestValue = USortLibrary::GetPropertyValuePtr(ArrayHelper, InnerProp, SortProp, Indices[Largest]);
            if (USortLibrary::ComparePropertyValues(SortProp, LargestValue, RightValue, bAscending))
            {
                Largest = Right + Low;
            }
        }

        if (Largest != Root)
        {
            Indices.Swap(Root, Largest);
            Heapify(Largest, End);
        }
    };

    // Build heap
    for (int32 i = (High - Low) / 2 - 1 + Low; i >= Low; --i)
    {
        Heapify(i, High);
    }

    // Extract elements from heap
    for (int32 i = High; i > Low; --i)
    {
        Indices.Swap(Low, i);
        Heapify(Low, i - 1);
    }
}

void USortLibrary::QuickSortByProperty(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, TArray<int32>& Indices, int32 Low, int32 High, bool bAscending, int32 DepthLimit)
{
    // 深度限制保护：防止栈溢出
    if (DepthLimit == 0)
    {
        // 回退到堆排序（非递归）
        HeapSortByProperty(ArrayHelper, InnerProp, SortProp, Indices, Low, High, bAscending);
        return;
    }

    if (Low < High)
    {
        const int32 PivotIndex = PartitionByProperty(ArrayHelper, InnerProp, SortProp, Indices, Low, High, bAscending);
        QuickSortByProperty(ArrayHelper, InnerProp, SortProp, Indices, Low, PivotIndex - 1, bAscending, DepthLimit - 1);
        QuickSortByProperty(ArrayHelper, InnerProp, SortProp, Indices, PivotIndex + 1, High, bAscending, DepthLimit - 1);
    }
}

int32 USortLibrary::PartitionByProperty(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, TArray<int32>& Indices, int32 Low, int32 High, bool bAscending)
{
    const int32 PivotIndex = Indices[High];
    void* PivotValuePtr = GetPropertyValuePtr(ArrayHelper, InnerProp, SortProp, PivotIndex);

    int32 i = Low - 1;

    for (int32 j = Low; j < High; ++j)
    {
        const int32 CurrentIndex = Indices[j];
        void* CurrentValuePtr = GetPropertyValuePtr(ArrayHelper, InnerProp, SortProp, CurrentIndex);

        if (ComparePropertyValues(SortProp, CurrentValuePtr, PivotValuePtr, bAscending))
        {
            ++i;
            Indices.Swap(i, j);
        }
    }

    Indices.Swap(i + 1, High);
    return i + 1;
}

void* USortLibrary::GetPropertyValuePtr(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, int32 ElementIndex)
{
    void* ElementPtr = ArrayHelper.GetRawPtr(ElementIndex);

    if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(InnerProp))
    {
        // 对象属性：先获取对象，再获取属性值
        UObject* Object = ObjectProp->GetObjectPropertyValue(ElementPtr);
        if (Object)
        {
            return SortProp->ContainerPtrToValuePtr<void>(Object);
        }
        return nullptr;
    }
    else
    {
        // 结构体属性：直接获取属性值
        return SortProp->ContainerPtrToValuePtr<void>(ElementPtr);
    }
}



