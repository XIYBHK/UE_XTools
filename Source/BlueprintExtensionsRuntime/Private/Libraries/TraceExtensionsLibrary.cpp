#include "Libraries/TraceExtensionsLibrary.h"
#include "XToolsBlueprintHelpers.h"

#include "Engine/World.h"
#include "Engine/HitResult.h"       // FHitResult完整定义（UE 5.4+ IWYU必需）
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "Misc/ConfigCacheIni.h"

// Initialize static maps
TMap<FString, ETraceTypeQuery> UTraceExtensionsLibrary::CachedTraceChannels;
TMap<FString, EObjectTypeQuery> UTraceExtensionsLibrary::CachedObjectTypes;

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region QueryNames

    TArray<FName> UTraceExtensionsLibrary::GetTraceTypeQueryNames()
    {
        return XToolsBlueprintHelpers::GetEnumDisplayNames(StaticEnum<ETraceTypeQuery>());
    }

    TArray<FName> UTraceExtensionsLibrary::GetObjectTypeQueryNames()
    {
        return XToolsBlueprintHelpers::GetEnumDisplayNames(StaticEnum<EObjectTypeQuery>());
    }

    void UTraceExtensionsLibrary::TraceChannelType(FName InputName, FString& OutString)
    {
        const TMap<FName, FString> TraceTypeQueryMap = XToolsBlueprintHelpers::BuildEnumNameMap(StaticEnum<ETraceTypeQuery>());

        if (TraceTypeQueryMap.Contains(InputName))
        {
            OutString = TraceTypeQueryMap[InputName];
        }
        else
        {
            OutString = FString("TraceTypeQuery1");
        }
    }

    void UTraceExtensionsLibrary::TraceObjectType(FName InputName, FString& OutString)
    {
        const TMap<FName, FString> ObjectTypeQueryMap = XToolsBlueprintHelpers::BuildEnumNameMap(StaticEnum<EObjectTypeQuery>());

        if (ObjectTypeQueryMap.Contains(InputName))
        {
            OutString = ObjectTypeQueryMap[InputName];
        }
        else
        {
            OutString = FString("ObjectTypeQuery1");
        }
    }

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region LineTrace

    void UTraceExtensionsLibrary::TraceLineChannel(
        UObject* WorldContextObject,
        FVector Start,
        FVector End,
        FString TraceChannelType,
        bool bTraceComplex,
        const TArray<AActor*>& ActorsToIgnore,
        EDebugTraceType DrawDebugType,
        bool& Block,
        FVector& ImpactPoint,
        FHitResult& OutHit,
        FLinearColor TraceColor,
        FLinearColor TraceHitColor,
        float DrawTime
    )
    {
        UWorld* World = XToolsBlueprintHelpers::GetValidWorld(WorldContextObject);
        if (!World) return;

        // 设置碰撞查询参数
        FCollisionQueryParams Params(SCENE_QUERY_STAT(LineTraceSingle), bTraceComplex);
        Params.bReturnPhysicalMaterial = true;
        Params.AddIgnoredActors(ActorsToIgnore);

        // Use Cache for TraceChannelType -> Enum
        const ETraceTypeQuery TraceChannelEnum = XToolsBlueprintHelpers::GetCachedEnum<ETraceTypeQuery>(
            CachedTraceChannels, TraceChannelType, StaticEnum<ETraceTypeQuery>());

        // 执行射线检测
        bool bHit = World->LineTraceSingleByChannel(OutHit, Start, End, UEngineTypes::ConvertToCollisionChannel(TraceChannelEnum), Params);

        // 设置输出参数
        Block = bHit;
        ImpactPoint = bHit ? OutHit.ImpactPoint : FVector::ZeroVector;

        // 绘制调试线
        if (DrawDebugType != EDebugTraceType::None)
        {
            if (bHit)
            {
                DrawDebugLine(World, Start, OutHit.ImpactPoint, TraceColor.ToFColor(true), false, DrawTime, 0, 0.0f);
                DrawDebugLine(World, OutHit.ImpactPoint, End, TraceHitColor.ToFColor(true), false, DrawTime, 0, 0.0f);
                DrawDebugPoint(World, OutHit.ImpactPoint, 10.0f, TraceHitColor.ToFColor(true), false, DrawTime);
            }
            else
            {
                DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), false, DrawTime, 0, 0.0f);
            }
        }
    }

    void UTraceExtensionsLibrary::TraceLineChannelOnAxisZ(
        UObject* WorldContextObject,
        FVector PivotLocation,
        float TraceRange,
        FString TraceChannelType,
        bool bTraceComplex,
        const TArray<AActor*>& ActorsToIgnore,
        EDebugTraceType DrawDebugType,
        bool& Block,
        FVector& ImpactPoint,
        FHitResult& OutHit,
        FLinearColor TraceColor,
        FLinearColor TraceHitColor,
        float DrawTime
    )
    {
        FVector Start = PivotLocation + FVector(0, 0, TraceRange);
        FVector End = PivotLocation - FVector(0, 0, TraceRange);

        TraceLineChannel(
            WorldContextObject,
            Start,
            End,
            TraceChannelType,
            bTraceComplex,
            ActorsToIgnore,
            DrawDebugType,
            Block,
            ImpactPoint,
            OutHit,
            TraceColor,
            TraceHitColor,
            DrawTime
        );
    }

    void UTraceExtensionsLibrary::TraceLineChannelByExtension(
        UObject* WorldContextObject,
        FVector Start,
        FVector End,
        float TraceRange,
        FString TraceChannelType,
        bool bTraceComplex,
        const TArray<AActor*>& ActorsToIgnore,
        EDebugTraceType DrawDebugType,
        bool& Block,
        FVector& ImpactPoint,
        FHitResult& OutHit,
        FLinearColor TraceColor,
        FLinearColor TraceHitColor,
        float DrawTime
    )
    {
        FVector Direction = (End - Start).GetSafeNormal();
        FVector NewEnd = Start + Direction * TraceRange;

        TraceLineChannel(
            WorldContextObject,
            Start,
            NewEnd,
            TraceChannelType,
            bTraceComplex,
            ActorsToIgnore,
            DrawDebugType,
            Block,
            ImpactPoint,
            OutHit,
            TraceColor,
            TraceHitColor,
            DrawTime
        );
    }

    void UTraceExtensionsLibrary::TraceLineObject(
        UObject* WorldContextObject,
        FVector Start,
        FVector End,
        const TArray<FString>& TraceObjectType,
        bool bTraceComplex,
        const TArray<AActor*>& ActorsToIgnore,
        EDebugTraceType DrawDebugType,
        bool& Block,
        FVector& ImpactPoint,
        FHitResult& OutHit,
        FLinearColor TraceColor,
        FLinearColor TraceHitColor,
        float DrawTime
    )
    {
        UWorld* World = XToolsBlueprintHelpers::GetValidWorld(WorldContextObject);
        if (!World) return;

        // 设置碰撞查询参数
        FCollisionQueryParams Params(SCENE_QUERY_STAT(LineTraceSingle), bTraceComplex);
        Params.AddIgnoredActors(ActorsToIgnore);

        // 设置碰撞对象查询参数
        FCollisionObjectQueryParams ObjectParams;
        for (const FString& TraceChannel : TraceObjectType)
        {
            const EObjectTypeQuery ObjectTypeEnum = XToolsBlueprintHelpers::GetCachedEnum<EObjectTypeQuery>(
                CachedObjectTypes, TraceChannel, StaticEnum<EObjectTypeQuery>());

            ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectTypeEnum));
        }

        // 执行射线检测
        bool bHit = World->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, Params);

        // 设置输出参数
        Block = bHit;
        ImpactPoint = bHit ? OutHit.ImpactPoint : FVector::ZeroVector;

        // 绘制调试线
        if (DrawDebugType != EDebugTraceType::None)
        {
            if (bHit)
            {
                DrawDebugLine(World, Start, OutHit.ImpactPoint, TraceColor.ToFColor(true), false, DrawTime, 0, 0.0f);
                DrawDebugLine(World, OutHit.ImpactPoint, End, TraceHitColor.ToFColor(true), false, DrawTime, 0, 0.0f);
                DrawDebugPoint(World, OutHit.ImpactPoint, 10.0f, TraceHitColor.ToFColor(true), false, DrawTime);
            }
            else
            {
                DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), false, DrawTime, 0, 0.0f);
            }
        }
    }

#pragma endregion 

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region SphereTrace

void UTraceExtensionsLibrary::TraceSphereChannel(
    UObject* WorldContextObject,
    FVector Start,
    FVector End,
    float Radius,
    FString TraceChannelType,
    bool bTraceComplex,
    const TArray<AActor*>& ActorsToIgnore,
    EDebugTraceType DrawDebugType,
    bool& Block,
    FVector& ImpactPoint,
    FHitResult& OutHit,
    FLinearColor TraceColor,
    FLinearColor TraceHitColor,
    float DrawTime
)
{
    if (!WorldContextObject) return;

    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) return;

    // 设置碰撞查询参数
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SphereTraceSingle), bTraceComplex);
    Params.bReturnPhysicalMaterial = true;
    Params.AddIgnoredActors(ActorsToIgnore);

    // Use Cache for TraceChannelType -> Enum
    ETraceTypeQuery TraceChannelEnum;
    if (const ETraceTypeQuery* CachedEnum = CachedTraceChannels.Find(TraceChannelType))
    {
        TraceChannelEnum = *CachedEnum;
    }
    else
    {
        int64 EnumValue = StaticEnum<ETraceTypeQuery>()->GetValueByName(FName(*TraceChannelType));
        TraceChannelEnum = static_cast<ETraceTypeQuery>(EnumValue);
        CachedTraceChannels.Add(TraceChannelType, TraceChannelEnum);
    }

    // 执行球体检测
    bool bHit = World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, UEngineTypes::ConvertToCollisionChannel(TraceChannelEnum), FCollisionShape::MakeSphere(Radius), Params);

    // 设置输出参数
    Block = bHit;
    ImpactPoint = bHit ? OutHit.ImpactPoint : FVector::ZeroVector;

    // 绘制调试线
    if (DrawDebugType != EDebugTraceType::None)
    {
        if (bHit)
        {
            DrawDebugSphere(World, Start, Radius, 12, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
            DrawDebugSphere(World, OutHit.ImpactPoint, Radius, 12, TraceHitColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
            DrawDebugLine(World, Start, OutHit.ImpactPoint, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime, 0, 0.0f);
            DrawDebugLine(World, OutHit.ImpactPoint, End, TraceHitColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime, 0, 0.0f);
            DrawDebugPoint(World, OutHit.ImpactPoint, 10.0f, TraceHitColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
        }
        else
        {
            DrawDebugSphere(World, Start, Radius, 12, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
            DrawDebugSphere(World, End, Radius, 12, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
            DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime, 0, 0.0f);
        }
    }
}

void UTraceExtensionsLibrary::TraceSphereObject(
    UObject* WorldContextObject,
    FVector Start,
    FVector End,
    float Radius,
    const TArray<FString>& TraceObjectType,
    bool bTraceComplex,
    const TArray<AActor*>& ActorsToIgnore,
    EDebugTraceType DrawDebugType,
    bool& Block,
    FVector& ImpactPoint,
    FHitResult& OutHit,
    FLinearColor TraceColor,
    FLinearColor TraceHitColor,
    float DrawTime
)
{
    if (!WorldContextObject) return;

    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) return;

    // 设置碰撞查询参数
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SphereTraceSingle), bTraceComplex);
    Params.AddIgnoredActors(ActorsToIgnore);

    // 设置碰撞对象查询参数
    FCollisionObjectQueryParams ObjectParams;
    for (const FString& TraceChannel : TraceObjectType)
    {
        EObjectTypeQuery ObjectTypeEnum;
        if (const EObjectTypeQuery* CachedEnum = CachedObjectTypes.Find(TraceChannel))
        {
            ObjectTypeEnum = *CachedEnum;
        }
        else
        {
            int64 EnumValue = StaticEnum<EObjectTypeQuery>()->GetValueByName(FName(*TraceChannel));
            ObjectTypeEnum = static_cast<EObjectTypeQuery>(EnumValue);
            CachedObjectTypes.Add(TraceChannel, ObjectTypeEnum);
        }

        ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectTypeEnum));
    }

    // 执行球体检测
    bool bHit = World->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params);

    // 设置输出参数
    Block = bHit;
    ImpactPoint = bHit ? OutHit.ImpactPoint : FVector::ZeroVector;

    // 绘制调试线
    if (DrawDebugType != EDebugTraceType::None)
    {
        if (bHit)
        {
            DrawDebugSphere(World, Start, Radius, 12, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
            DrawDebugSphere(World, OutHit.ImpactPoint, Radius, 12, TraceHitColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
            DrawDebugLine(World, Start, OutHit.ImpactPoint, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime, 0, 0.0f);
            DrawDebugLine(World, OutHit.ImpactPoint, End, TraceHitColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime, 0, 0.0f);
            DrawDebugPoint(World, OutHit.ImpactPoint, 10.0f, TraceHitColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
        }
        else
        {
            DrawDebugSphere(World, Start, Radius, 12, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
            DrawDebugSphere(World, End, Radius, 12, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime);
            DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), DrawDebugType == EDebugTraceType::Persistent, DrawTime, 0, 0.0f);
        }
    }
}

#pragma endregion
