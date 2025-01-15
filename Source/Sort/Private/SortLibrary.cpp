#include "SortLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(SortLibraryLog, Log, All);

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
    SortedActors = Actors;
    const int32 ArrayNum = Actors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);
    SortedDistances.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FActorDistancePair
    {
        AActor* Actor;
        float Distance;
        int32 OriginalIndex;

        FActorDistancePair() : Actor(nullptr), Distance(MAX_FLT), OriginalIndex(INDEX_NONE) {}
        FActorDistancePair(AActor* InActor, float InDistance, int32 InIndex)
            : Actor(InActor), Distance(InDistance), OriginalIndex(InIndex) {}
    };

    TArray<FActorDistancePair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Distance = MAX_FLT;
        if (AActor* Actor = SortedActors[i])
        {
            if (IsValid(Actor))
            {
                if (b2DDistance)
                {
                    // 2D距离计算（忽略Z轴）
                    FVector ActorLoc = Actor->GetActorLocation();
                    ActorLoc.Z = Location.Z; // 将Z坐标设为相同值，这样计算距离时就只考虑XY平面
                    Distance = FVector::Dist(ActorLoc, Location);
                }
                else
                {
                    // 3D距离计算
                    Distance = FVector::Dist(Actor->GetActorLocation(), Location);
                }
            }
        }
        Pairs[i] = FActorDistancePair(SortedActors[i], Distance, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FActorDistancePair& A, const FActorDistancePair& B) {
            return A.Distance < B.Distance;
        });
    }
    else
    {
        Pairs.Sort([](const FActorDistancePair& A, const FActorDistancePair& B) {
            return A.Distance > B.Distance;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedActors[i] = Pairs[i].Actor;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedDistances[i] = Pairs[i].Distance;
    }

    // 优化内存
    Pairs.Empty(0);
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
    SortedActors = Actors;
    const int32 ArrayNum = Actors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FActorHeightPair
    {
        AActor* Actor;
        float Height;
        int32 OriginalIndex;

        FActorHeightPair() : Actor(nullptr), Height(MAX_FLT), OriginalIndex(INDEX_NONE) {}
        FActorHeightPair(AActor* InActor, float InHeight, int32 InIndex)
            : Actor(InActor), Height(InHeight), OriginalIndex(InIndex) {}
    };

    TArray<FActorHeightPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Height = MAX_FLT;
        if (AActor* Actor = SortedActors[i])
        {
            if (IsValid(Actor))
            {
                Height = Actor->GetActorLocation().Z;
            }
        }
        Pairs[i] = FActorHeightPair(SortedActors[i], Height, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FActorHeightPair& A, const FActorHeightPair& B) {
            return A.Height < B.Height;
        });
    }
    else
    {
        Pairs.Sort([](const FActorHeightPair& A, const FActorHeightPair& B) {
            return A.Height > B.Height;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedActors[i] = Pairs[i].Actor;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortIntegerArray(const TArray<int32>& InArray, bool bAscending,
    TArray<int32>& SortedArray, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        SortedArray.Empty(0);
        OriginalIndices.Empty(0);
        return;
    }

    // 初始化输出数组
    SortedArray = InArray;
    const int32 ArrayNum = InArray.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FIntegerPair
    {
        int32 Value;
        int32 OriginalIndex;

        FIntegerPair() : Value(0), OriginalIndex(INDEX_NONE) {}
        FIntegerPair(int32 InValue, int32 InIndex)
            : Value(InValue), OriginalIndex(InIndex) {}
    };

    TArray<FIntegerPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        Pairs[i] = FIntegerPair(InArray[i], i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FIntegerPair& A, const FIntegerPair& B) {
            return A.Value < B.Value;
        });
    }
    else
    {
        Pairs.Sort([](const FIntegerPair& A, const FIntegerPair& B) {
            return A.Value > B.Value;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortFloatArray(const TArray<float>& InArray, bool bAscending,
    TArray<float>& SortedArray, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        SortedArray.Empty(0);
        OriginalIndices.Empty(0);
        return;
    }

    // 初始化输出数组
    SortedArray = InArray;
    const int32 ArrayNum = InArray.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FFloatPair
    {
        float Value;
        int32 OriginalIndex;

        FFloatPair() : Value(0.0f), OriginalIndex(INDEX_NONE) {}
        FFloatPair(float InValue, int32 InIndex)
            : Value(InValue), OriginalIndex(InIndex) {}
    };

    TArray<FFloatPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        Pairs[i] = FFloatPair(InArray[i], i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FFloatPair& A, const FFloatPair& B) {
            // 处理特殊浮点数
            if (FMath::IsNaN(A.Value)) return false;
            if (FMath::IsNaN(B.Value)) return true;
            if (!FMath::IsFinite(A.Value) && !FMath::IsFinite(B.Value)) return false;
            if (!FMath::IsFinite(A.Value)) return true;
            if (!FMath::IsFinite(B.Value)) return false;
            if (FMath::IsNearlyEqual(A.Value, B.Value)) return false;
            return A.Value < B.Value;
        });
    }
    else
    {
        Pairs.Sort([](const FFloatPair& A, const FFloatPair& B) {
            // 处理特殊浮点数
            if (FMath::IsNaN(A.Value)) return false;
            if (FMath::IsNaN(B.Value)) return true;
            if (!FMath::IsFinite(A.Value) && !FMath::IsFinite(B.Value)) return false;
            if (!FMath::IsFinite(A.Value)) return false;
            if (!FMath::IsFinite(B.Value)) return true;
            if (FMath::IsNearlyEqual(A.Value, B.Value)) return false;
            return A.Value > B.Value;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortStringArray(const TArray<FString>& InArray, bool bAscending,
    TArray<FString>& SortedArray, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        SortedArray.Empty(); // 确保输出数组被清空
        OriginalIndices.Empty(); // 确保原始索引数组被清空
        return;
    }

    // 输出输入数组的内容
    for (const FString& Str : InArray)
    {
        UE_LOG(SortLibraryLog, Log, TEXT("Input String: %s"), *Str);
    }

    // 初始化输出数组
    SortedArray = InArray;
    const int32 ArrayNum = InArray.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for (int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 使用数组排序
    if (bAscending)
    {
        SortedArray.Sort([](const FString& A, const FString& B) {
            return A.Compare(B, ESearchCase::IgnoreCase) < 0;
        });
    }
    else
    {
        SortedArray.Sort([](const FString& A, const FString& B) {
            return A.Compare(B, ESearchCase::IgnoreCase) > 0;
        });
    }

    // 更新原始索引
    for (int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = InArray.IndexOfByKey(SortedArray[i]);
    }

    // 输出排序后的结果
    for (const FString& Str : SortedArray)
    {
        UE_LOG(SortLibraryLog, Log, TEXT("Sorted String: %s"), *Str);
    }
}

void USortLibrary::SortNameArray(const TArray<FName>& InArray, bool bAscending,
    TArray<FName>& SortedArray, TArray<int32>& OriginalIndices)
{
    // 参数验证
    if (InArray.Num() <= 0)
    {
        SortedArray.Empty(0);
        OriginalIndices.Empty(0);
        return;
    }

    // 初始化输出数组
    SortedArray = InArray;
    const int32 ArrayNum = InArray.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FNamePair
    {
        FName Value;
        int32 OriginalIndex;

        FNamePair() : Value(NAME_None), OriginalIndex(INDEX_NONE) {}
        FNamePair(const FName& InValue, int32 InIndex)
            : Value(InValue), OriginalIndex(InIndex) {}
    };

    TArray<FNamePair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        Pairs[i] = FNamePair(InArray[i], i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FNamePair& A, const FNamePair& B) {
            return A.Value.Compare(B.Value) < 0;
        });
    }
    else
    {
        Pairs.Sort([](const FNamePair& A, const FNamePair& B) {
            return A.Value.Compare(B.Value) > 0;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedArray[i] = Pairs[i].Value;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortActorsByAxis(const TArray<AActor*>& Actors, ECoordinateAxis Axis, bool bAscending,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAxisValues)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty(0);
        OriginalIndices.Empty(0);
        SortedAxisValues.Empty(0);
        return;
    }

    // 初始化输出数组
    SortedActors = Actors;
    const int32 ArrayNum = Actors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);
    SortedAxisValues.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FActorAxisPair
    {
        AActor* Actor;
        float AxisValue;
        int32 OriginalIndex;

        FActorAxisPair() : Actor(nullptr), AxisValue(0.0f), OriginalIndex(INDEX_NONE) {}
        FActorAxisPair(AActor* InActor, float InValue, int32 InIndex)
            : Actor(InActor), AxisValue(InValue), OriginalIndex(InIndex) {}
    };

    TArray<FActorAxisPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Value = 0.0f;
        switch(Axis)
        {
            case ECoordinateAxis::X:
                Value = Actors[i]->GetActorLocation().X;
                break;
            case ECoordinateAxis::Y:
                Value = Actors[i]->GetActorLocation().Y;
                break;
            case ECoordinateAxis::Z:
                Value = Actors[i]->GetActorLocation().Z;
                break;
        }
        Pairs[i] = FActorAxisPair(Actors[i], Value, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FActorAxisPair& A, const FActorAxisPair& B) {
            return A.AxisValue < B.AxisValue;
        });
    }
    else
    {
        Pairs.Sort([](const FActorAxisPair& A, const FActorAxisPair& B) {
            return A.AxisValue > B.AxisValue;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedActors[i] = Pairs[i].Actor;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAxisValues[i] = Pairs[i].AxisValue;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortActorsByAngle(const TArray<AActor*>& Actors, const FVector& Center, const FVector& Direction,
    bool bAscending, bool b2DAngle, TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAngles)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty(0);
        OriginalIndices.Empty(0);
        SortedAngles.Empty(0);
        return;
    }

    // 初始化输出数组
    SortedActors = Actors;
    const int32 ArrayNum = Actors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);
    SortedAngles.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 标准化参考方向
    FVector NormalizedDirection = Direction.GetSafeNormal();
    if (b2DAngle)
    {
        // 如果是2D夹角，将Z设为0
        NormalizedDirection.Z = 0.0f;
        NormalizedDirection.Normalize();
    }

    // 创建排序用的键值对数组
    struct FActorAnglePair
    {
        AActor* Actor;
        float Angle;
        int32 OriginalIndex;

        FActorAnglePair() : Actor(nullptr), Angle(0.0f), OriginalIndex(INDEX_NONE) {}
        FActorAnglePair(AActor* InActor, float InAngle, int32 InIndex)
            : Actor(InActor), Angle(InAngle), OriginalIndex(InIndex) {}
    };

    TArray<FActorAnglePair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Angle = 0.0f;
        if (AActor* Actor = SortedActors[i])
        {
            if (IsValid(Actor))
            {
                FVector ToActor = Actor->GetActorLocation() - Center;
                if (b2DAngle)
                {
                    // 2D夹角计算
                    ToActor.Z = 0.0f;
                    ToActor.Normalize();
                    Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
                    
                    // 判断是顺时针还是逆时针，调整角度范围为0-360
                    float Cross = NormalizedDirection.X * ToActor.Y - NormalizedDirection.Y * ToActor.X;
                    if (Cross < 0.0f)
                    {
                        Angle = 360.0f - Angle;
                    }
                }
                else
                {
                    // 3D夹角计算
                    ToActor.Normalize();
                    Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
                }
            }
        }
        Pairs[i] = FActorAnglePair(SortedActors[i], Angle, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FActorAnglePair& A, const FActorAnglePair& B) {
            return A.Angle < B.Angle;
        });
    }
    else
    {
        Pairs.Sort([](const FActorAnglePair& A, const FActorAnglePair& B) {
            return A.Angle > B.Angle;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedActors[i] = Pairs[i].Actor;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAngles[i] = Pairs[i].Angle;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortActorsByAzimuth(const TArray<AActor*>& Actors, const FVector& Center, bool bAscending,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAzimuths)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty(0);
        OriginalIndices.Empty(0);
        SortedAzimuths.Empty(0);
        return;
    }

    // 初始化输出数组
    SortedActors = Actors;
    const int32 ArrayNum = Actors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);
    SortedAzimuths.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 创建排序用的键值对数组
    struct FActorAzimuthPair
    {
        AActor* Actor;
        float Azimuth;
        int32 OriginalIndex;

        FActorAzimuthPair() : Actor(nullptr), Azimuth(0.0f), OriginalIndex(INDEX_NONE) {}
        FActorAzimuthPair(AActor* InActor, float InAzimuth, int32 InIndex)
            : Actor(InActor), Azimuth(InAzimuth), OriginalIndex(InIndex) {}
    };

    TArray<FActorAzimuthPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Azimuth = 0.0f;
        if (AActor* Actor = SortedActors[i])
        {
            if (IsValid(Actor))
            {
                // 计算从中心点到Actor的向量（在XY平面上）
                FVector ToActor = Actor->GetActorLocation() - Center;
                ToActor.Z = 0.0f; // 忽略Z轴，只在XY平面上计算方位角

                // 计算方位角（以Y轴正方向为北，顺时针旋转）
                // atan2返回的是以X轴正方向为0度，逆时针为正的角度
                float Angle = FMath::RadiansToDegrees(FMath::Atan2(ToActor.Y, ToActor.X));
                
                // 转换为方位角（以Y轴正方向为0度，顺时针为正）
                Azimuth = FMath::Fmod(90.0f - Angle, 360.0f);
                if (Azimuth < 0.0f)
                {
                    Azimuth += 360.0f;
                }
            }
        }
        Pairs[i] = FActorAzimuthPair(SortedActors[i], Azimuth, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FActorAzimuthPair& A, const FActorAzimuthPair& B) {
            return A.Azimuth < B.Azimuth;
        });
    }
    else
    {
        Pairs.Sort([](const FActorAzimuthPair& A, const FActorAzimuthPair& B) {
            return A.Azimuth > B.Azimuth;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedActors[i] = Pairs[i].Actor;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAzimuths[i] = Pairs[i].Azimuth;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortVectorsByProjection(const TArray<FVector>& Vectors, const FVector& Direction, bool bAscending,
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedProjections)
{
    // 参数验证
    if (Vectors.Num() <= 0)
    {
        SortedVectors.Empty(0);
        OriginalIndices.Empty(0);
        SortedProjections.Empty(0);
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
        FVectorProjectionPair(const FVector& InVector, float InProjection, int32 InIndex)
            : Vector(InVector), Projection(InProjection), OriginalIndex(InIndex) {}
    };

    TArray<FVectorProjectionPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Projection = FVector::DotProduct(Vectors[i], NormalizedDirection);
        Pairs[i] = FVectorProjectionPair(Vectors[i], Projection, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FVectorProjectionPair& A, const FVectorProjectionPair& B) {
            return A.Projection < B.Projection;
        });
    }
    else
    {
        Pairs.Sort([](const FVectorProjectionPair& A, const FVectorProjectionPair& B) {
            return A.Projection > B.Projection;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedVectors[i] = Pairs[i].Vector;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedProjections[i] = Pairs[i].Projection;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortVectorsByLength(const TArray<FVector>& Vectors, bool bAscending,
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedLengths)
{
    // 参数验证
    if (Vectors.Num() <= 0)
    {
        SortedVectors.Empty(0);
        OriginalIndices.Empty(0);
        SortedLengths.Empty(0);
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
        FVectorLengthPair(const FVector& InVector, float InLength, int32 InIndex)
            : Vector(InVector), Length(InLength), OriginalIndex(InIndex) {}
    };

    TArray<FVectorLengthPair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Length = Vectors[i].Size();
        Pairs[i] = FVectorLengthPair(Vectors[i], Length, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FVectorLengthPair& A, const FVectorLengthPair& B) {
            return A.Length < B.Length;
        });
    }
    else
    {
        Pairs.Sort([](const FVectorLengthPair& A, const FVectorLengthPair& B) {
            return A.Length > B.Length;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedVectors[i] = Pairs[i].Vector;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedLengths[i] = Pairs[i].Length;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortVectorsByAxis(const TArray<FVector>& Vectors, ECoordinateAxis Axis, bool bAscending,
    TArray<FVector>& SortedVectors, TArray<int32>& OriginalIndices, TArray<float>& SortedAxisValues)
{
    // 参数验证
    if (Vectors.Num() <= 0)
    {
        SortedVectors.Empty(0);
        OriginalIndices.Empty(0);
        SortedAxisValues.Empty(0);
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
        FVectorAxisPair(const FVector& InVector, float InValue, int32 InIndex)
            : Vector(InVector), AxisValue(InValue), OriginalIndex(InIndex) {}
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
        Pairs[i] = FVectorAxisPair(Vectors[i], Value, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FVectorAxisPair& A, const FVectorAxisPair& B) {
            return A.AxisValue < B.AxisValue;
        });
    }
    else
    {
        Pairs.Sort([](const FVectorAxisPair& A, const FVectorAxisPair& B) {
            return A.AxisValue > B.AxisValue;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedVectors[i] = Pairs[i].Vector;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAxisValues[i] = Pairs[i].AxisValue;
    }

    // 优化内存
    Pairs.Empty(0);
}

void USortLibrary::SortActorsByAngleAndDistance(const TArray<AActor*>& Actors, const FVector& Center,
    const FVector& Direction, float AngleWeight, float DistanceWeight, bool bAscending, bool b2DAngle,
    TArray<AActor*>& SortedActors, TArray<int32>& OriginalIndices, TArray<float>& SortedAngles,
    TArray<float>& SortedDistances)
{
    // 参数验证
    if (Actors.Num() <= 0)
    {
        SortedActors.Empty(0);
        OriginalIndices.Empty(0);
        SortedAngles.Empty(0);
        SortedDistances.Empty(0);
        return;
    }

    // 初始化输出数组
    SortedActors = Actors;
    const int32 ArrayNum = Actors.Num();
    OriginalIndices.SetNumUninitialized(ArrayNum);
    SortedAngles.SetNumUninitialized(ArrayNum);
    SortedDistances.SetNumUninitialized(ArrayNum);

    // 初始化索引数组
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        OriginalIndices[i] = i;
    }

    // 标准化参考方向
    FVector NormalizedDirection = Direction.GetSafeNormal();
    if (b2DAngle)
    {
        // 如果是2D夹角，将Z设为0
        NormalizedDirection.Z = 0.0f;
        NormalizedDirection.Normalize();
    }

    // 标准化权重
    float TotalWeight = AngleWeight + DistanceWeight;
    if (TotalWeight <= 0.0f)
    {
        AngleWeight = DistanceWeight = 0.5f;
    }
    else
    {
        AngleWeight /= TotalWeight;
        DistanceWeight /= TotalWeight;
    }

    // 创建排序用的键值对数组
    struct FActorAngleDistancePair
    {
        AActor* Actor;
        float Angle;
        float Distance;
        float WeightedValue;
        int32 OriginalIndex;

        FActorAngleDistancePair() : Actor(nullptr), Angle(0.0f), Distance(0.0f),
            WeightedValue(0.0f), OriginalIndex(INDEX_NONE) {}
        FActorAngleDistancePair(AActor* InActor, float InAngle, float InDistance,
            float InWeightedValue, int32 InIndex)
            : Actor(InActor), Angle(InAngle), Distance(InDistance),
            WeightedValue(InWeightedValue), OriginalIndex(InIndex) {}
    };

    TArray<FActorAngleDistancePair> Pairs;
    Pairs.SetNumUninitialized(ArrayNum);

    // 找到最大距离用于归一化
    float MaxDistance = 0.0f;
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        if (AActor* Actor = SortedActors[i])
        {
            if (IsValid(Actor))
            {
                float Distance = FVector::Dist(Actor->GetActorLocation(), Center);
                MaxDistance = FMath::Max(MaxDistance, Distance);
            }
        }
    }
    MaxDistance = MaxDistance > 0.0f ? MaxDistance : 1.0f;

    // 构建键值对
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        float Angle = 0.0f;
        float Distance = MAX_FLT;
        float WeightedValue = MAX_FLT;

        if (AActor* Actor = SortedActors[i])
        {
            if (IsValid(Actor))
            {
                FVector ToActor = Actor->GetActorLocation() - Center;
                Distance = ToActor.Size();

                if (b2DAngle)
                {
                    // 2D夹角计算
                    ToActor.Z = 0.0f;
                    ToActor.Normalize();
                    Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
                    
                    // 判断是顺时针还是逆时针，调整角度范围为0-360
                    float Cross = NormalizedDirection.X * ToActor.Y - NormalizedDirection.Y * ToActor.X;
                    if (Cross < 0.0f)
                    {
                        Angle = 360.0f - Angle;
                    }
                }
                else
                {
                    // 3D夹角计算
                    ToActor.Normalize();
                    Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(NormalizedDirection, ToActor)));
                }

                // 计算加权值（归一化后的角度和距离）
                float NormalizedAngle = Angle / 360.0f;
                float NormalizedDistance = Distance / MaxDistance;
                WeightedValue = NormalizedAngle * AngleWeight + NormalizedDistance * DistanceWeight;
            }
        }
        Pairs[i] = FActorAngleDistancePair(SortedActors[i], Angle, Distance, WeightedValue, i);
    }

    // 使用数组排序
    if (bAscending)
    {
        Pairs.Sort([](const FActorAngleDistancePair& A, const FActorAngleDistancePair& B) {
            return A.WeightedValue < B.WeightedValue;
        });
    }
    else
    {
        Pairs.Sort([](const FActorAngleDistancePair& A, const FActorAngleDistancePair& B) {
            return A.WeightedValue > B.WeightedValue;
        });
    }

    // 更新结果
    for(int32 i = 0; i < ArrayNum; ++i)
    {
        SortedActors[i] = Pairs[i].Actor;
        OriginalIndices[i] = Pairs[i].OriginalIndex;
        SortedAngles[i] = Pairs[i].Angle;
        SortedDistances[i] = Pairs[i].Distance;
    }

    // 优化内存
    Pairs.Empty(0);
}

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