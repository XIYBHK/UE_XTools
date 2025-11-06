#include "Libraries/TraceExtensionsLibrary.h"

#include "Engine/World.h"
#include "Engine/HitResult.h"       // FHitResult完整定义（UE 5.4+ IWYU必需）
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "Misc/ConfigCacheIni.h"

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region QueryNames

    TArray<FName> UTraceExtensionsLibrary::GetTraceTypeQueryNames()
    {
        TArray<FName> Keys;
        UEnum* EnumPtr = StaticEnum<ETraceTypeQuery>();
        if (EnumPtr)
        {
            for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
            {
                FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
                FString EnumName = EnumPtr->GetNameStringByIndex(i);

                // 移除DisplayName和EnumName中的空格和下划线
                FString DisplayNameCleaned = DisplayName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
                FString EnumNameCleaned = EnumName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));

                // 如果DisplayNameCleaned和EnumNameCleaned相等，则跳过
                if (DisplayNameCleaned != EnumNameCleaned)
                {
                    FName Key = FName(*DisplayName);
                    Keys.Add(FName(*DisplayName)); // 使用原版参数
                }
            }
        }
        return Keys;
    }

    TArray<FName> UTraceExtensionsLibrary::GetObjectTypeQueryNames()
    {
        TArray<FName> Keys;
        UEnum* EnumPtr = StaticEnum<EObjectTypeQuery>();
        if (EnumPtr)
        {
            for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
            {
                FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
                FString EnumName = EnumPtr->GetNameStringByIndex(i);

                // 移除DisplayName和EnumName中的空格和下划线
                FString DisplayNameCleaned = DisplayName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
                FString EnumNameCleaned = EnumName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));

                // 如果DisplayNameCleaned和EnumNameCleaned相等，则跳过
                if (DisplayNameCleaned != EnumNameCleaned)
                {
                    FName Key = FName(*DisplayName);
                    Keys.Add(FName(*DisplayName)); // 使用原版参数
                }
            }
        }
        return Keys;
    }

    void UTraceExtensionsLibrary::TraceChannelType(FName InputName, FString& OutString)
    {
        TMap<FName, FString> TraceTypeQueryMap;
        UEnum* EnumPtr = StaticEnum<ETraceTypeQuery>();
        if (EnumPtr)
        {
            for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
            {
                FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
                FString EnumName = EnumPtr->GetNameStringByIndex(i);

                // 移除DisplayName和EnumName中的空格和下划线
                FString DisplayNameCleaned = DisplayName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
                FString EnumNameCleaned = EnumName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));

                // 如果DisplayNameCleaned和EnumNameCleaned相等，则跳过
                if (DisplayNameCleaned != EnumNameCleaned)
                {
                    FName Key = FName(*DisplayName);
                    TraceTypeQueryMap.Add(FName(*DisplayName), EnumName); // 使用原版参数
                }
            }
        }

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
        TMap<FName, FString> ObjectTypeQueryMap;
        UEnum* EnumPtr = StaticEnum<EObjectTypeQuery>();
        if (EnumPtr)
        {
            for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
            {
                FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
                FString EnumName = EnumPtr->GetNameStringByIndex(i);

                // 移除DisplayName和EnumName中的空格和下划线
                FString DisplayNameCleaned = DisplayName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
                FString EnumNameCleaned = EnumName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));

                // 如果DisplayNameCleaned和EnumNameCleaned相等，则跳过
                if (DisplayNameCleaned != EnumNameCleaned)
                {
                    FName Key = FName(*DisplayName);
                    ObjectTypeQueryMap.Add(FName(*DisplayName), EnumName); // 使用原版参数
                }
            }
        }

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
        if (!WorldContextObject) return;

        UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
        if (!World) return;

        // 设置碰撞查询参数
        FCollisionQueryParams Params(SCENE_QUERY_STAT(LineTraceSingle), bTraceComplex);
        Params.bReturnPhysicalMaterial = true;
        Params.AddIgnoredActors(ActorsToIgnore);

        // 直接将TraceChannelType转换为枚举值
        int64 EnumValue = StaticEnum<ETraceTypeQuery>()->GetValueByName(FName(*TraceChannelType));
        ETraceTypeQuery TraceChannelEnum = static_cast<ETraceTypeQuery>(EnumValue);

        // 获取枚举的显示名称
        FString EnumDisplayName = StaticEnum<ETraceTypeQuery>()->GetDisplayNameTextByValue(EnumValue).ToString();

        // 执行射线检测
        bool bHit = World->LineTraceSingleByChannel(OutHit, Start, End, UEngineTypes::ConvertToCollisionChannel(TraceChannelEnum), Params);

        // 设置输出参数
        Block = bHit;
        ImpactPoint = bHit ? OutHit.ImpactPoint : FVector::ZeroVector;

        // 绘制调试线并打印输入的TraceChannelType和枚举的显示名称
        if (DrawDebugType != EDebugTraceType::None)
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Input TraceChannelType: %s"), *TraceChannelType));
                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(TEXT("EnumDisplayName: %s"), *EnumDisplayName));
            }

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
        if (!WorldContextObject) return;

        UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
        if (!World) return;

        // 设置碰撞查询参数
        FCollisionQueryParams Params(SCENE_QUERY_STAT(LineTraceSingle), bTraceComplex);
        Params.AddIgnoredActors(ActorsToIgnore);

        // 设置碰撞对象查询参数
        FCollisionObjectQueryParams ObjectParams;
        for (const FString& TraceChannel : TraceObjectType)
        {
            int64 EnumValue = StaticEnum<EObjectTypeQuery>()->GetValueByName(FName(*TraceChannel));
            EObjectTypeQuery ObjectTypeEnum = static_cast<EObjectTypeQuery>(EnumValue);

            ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectTypeEnum));

            // 获取枚举的显示名称
            FString EnumDisplayName = StaticEnum<EObjectTypeQuery>()->GetDisplayNameTextByValue(EnumValue).ToString();

            // 打印输入的TraceObjectType和枚举的显示名称
            if (DrawDebugType != EDebugTraceType::None && GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Input TraceObjectType: %s"), *TraceChannel));
                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(TEXT("EnumDisplayName: %s"), *EnumDisplayName));
            }
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

    UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
    if (!World) return;

    // 设置碰撞查询参数
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SphereTraceSingle), bTraceComplex);
    Params.bReturnPhysicalMaterial = true;
    Params.AddIgnoredActors(ActorsToIgnore);

    // 直接将TraceChannelType转换为枚举值
    int64 EnumValue = StaticEnum<ETraceTypeQuery>()->GetValueByName(FName(*TraceChannelType));
    ETraceTypeQuery TraceChannelEnum = static_cast<ETraceTypeQuery>(EnumValue);

    // 获取枚举的显示名称
    FString EnumDisplayName = StaticEnum<ETraceTypeQuery>()->GetDisplayNameTextByValue(EnumValue).ToString();

    // 执行球体检测
    bool bHit = World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, UEngineTypes::ConvertToCollisionChannel(TraceChannelEnum), FCollisionShape::MakeSphere(Radius), Params);

    // 设置输出参数
    Block = bHit;
    ImpactPoint = bHit ? OutHit.ImpactPoint : FVector::ZeroVector;

    // 绘制调试线并打印输入的TraceChannelType和枚举的显示名称
    if (DrawDebugType != EDebugTraceType::None)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Input TraceChannelType: %s"), *TraceChannelType));
            GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(TEXT("EnumDisplayName: %s"), *EnumDisplayName));
        }

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

    UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
    if (!World) return;

    // 设置碰撞查询参数
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SphereTraceSingle), bTraceComplex);
    Params.AddIgnoredActors(ActorsToIgnore);

    // 设置碰撞对象查询参数
    FCollisionObjectQueryParams ObjectParams;
    for (const FString& TraceChannel : TraceObjectType)
    {
        int64 EnumValue = StaticEnum<EObjectTypeQuery>()->GetValueByName(FName(*TraceChannel));
        EObjectTypeQuery ObjectTypeEnum = static_cast<EObjectTypeQuery>(EnumValue);

        ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectTypeEnum));

        // 获取枚举的显示名称
        FString EnumDisplayName = StaticEnum<EObjectTypeQuery>()->GetDisplayNameTextByValue(EnumValue).ToString();

        // 打印输入的TraceObjectType和枚举的显示名称
        if (DrawDebugType != EDebugTraceType::None && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Input TraceObjectType: %s"), *TraceChannel));
            GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(TEXT("EnumDisplayName: %s"), *EnumDisplayName));
        }
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

//————————————————————————————————————————————————————————————————————————————————————————————————————