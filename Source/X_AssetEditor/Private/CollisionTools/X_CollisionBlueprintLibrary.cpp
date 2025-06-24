// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/X_CollisionBlueprintLibrary.h"
#include "CollisionTools/X_CollisionManager.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "AssetRegistry/AssetData.h"

bool UX_CollisionBlueprintLibrary::RemoveStaticMeshCollision(UStaticMesh* StaticMesh)
{
    if (!StaticMesh)
    {
        return false;
    }

    return FX_CollisionManager::RemoveCollisionFromMesh(StaticMesh);
}

bool UX_CollisionBlueprintLibrary::AddStaticMeshConvexCollision(UStaticMesh* StaticMesh)
{
    if (!StaticMesh)
    {
        return false;
    }

    return FX_CollisionManager::AddConvexCollisionToMesh(StaticMesh);
}

bool UX_CollisionBlueprintLibrary::SetStaticMeshCollisionComplexity(UStaticMesh* StaticMesh, EX_CollisionComplexity ComplexityType)
{
    if (!StaticMesh)
    {
        return false;
    }

    ECollisionTraceFlag TraceFlag = FX_CollisionManager::ConvertToCollisionTraceFlag(ComplexityType);
    return FX_CollisionManager::SetMeshCollisionComplexity(StaticMesh, TraceFlag);
}

FX_CollisionOperationResult UX_CollisionBlueprintLibrary::BatchRemoveStaticMeshCollision(const TArray<UStaticMesh*>& StaticMeshes)
{
    TArray<FAssetData> AssetDataArray = ConvertToAssetDataArray(StaticMeshes);
    return FX_CollisionManager::RemoveCollisionFromAssets(AssetDataArray);
}

FX_CollisionOperationResult UX_CollisionBlueprintLibrary::BatchAddStaticMeshConvexCollision(const TArray<UStaticMesh*>& StaticMeshes)
{
    TArray<FAssetData> AssetDataArray = ConvertToAssetDataArray(StaticMeshes);
    return FX_CollisionManager::AddConvexCollisionToAssets(AssetDataArray);
}

FX_CollisionOperationResult UX_CollisionBlueprintLibrary::BatchSetStaticMeshCollisionComplexity(const TArray<UStaticMesh*>& StaticMeshes, EX_CollisionComplexity ComplexityType)
{
    TArray<FAssetData> AssetDataArray = ConvertToAssetDataArray(StaticMeshes);
    return FX_CollisionManager::SetCollisionComplexity(AssetDataArray, ComplexityType);
}

EX_CollisionComplexity UX_CollisionBlueprintLibrary::GetStaticMeshCollisionComplexity(UStaticMesh* StaticMesh)
{
    if (!StaticMesh || !StaticMesh->GetBodySetup())
    {
        return EX_CollisionComplexity::UseDefault;
    }

    ECollisionTraceFlag TraceFlag = StaticMesh->GetBodySetup()->CollisionTraceFlag;
    return ConvertFromCollisionTraceFlag(TraceFlag);
}

bool UX_CollisionBlueprintLibrary::DoesStaticMeshHaveSimpleCollision(UStaticMesh* StaticMesh)
{
    if (!StaticMesh || !StaticMesh->GetBodySetup())
    {
        return false;
    }

    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    return (BodySetup->AggGeom.GetElementCount() > 0);
}

bool UX_CollisionBlueprintLibrary::DoesStaticMeshHaveComplexCollision(UStaticMesh* StaticMesh)
{
    if (!StaticMesh || !StaticMesh->GetBodySetup())
    {
        return false;
    }

    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    return BodySetup->bHasCookedCollisionData;
}

int32 UX_CollisionBlueprintLibrary::GetStaticMeshSimpleCollisionCount(UStaticMesh* StaticMesh)
{
    if (!StaticMesh || !StaticMesh->GetBodySetup())
    {
        return 0;
    }

    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    return BodySetup->AggGeom.GetElementCount();
}

FString UX_CollisionBlueprintLibrary::GetCollisionComplexityDisplayName(EX_CollisionComplexity ComplexityType)
{
    switch (ComplexityType)
    {
        case EX_CollisionComplexity::UseDefault:
            return TEXT("项目默认");
        case EX_CollisionComplexity::UseSimpleAndComplex:
            return TEXT("简单与复杂");
        case EX_CollisionComplexity::UseSimpleAsComplex:
            return TEXT("将简单碰撞用作复杂碰撞");
        case EX_CollisionComplexity::UseComplexAsSimple:
            return TEXT("将复杂碰撞用作简单碰撞");
        default:
            return TEXT("未知");
    }
}

EX_CollisionComplexity UX_CollisionBlueprintLibrary::ConvertFromCollisionTraceFlag(TEnumAsByte<ECollisionTraceFlag> TraceFlag)
{
    switch (TraceFlag.GetValue())
    {
        case CTF_UseDefault:
            return EX_CollisionComplexity::UseDefault;
        case CTF_UseSimpleAndComplex:
            return EX_CollisionComplexity::UseSimpleAndComplex;
        case CTF_UseSimpleAsComplex:
            return EX_CollisionComplexity::UseSimpleAsComplex;
        case CTF_UseComplexAsSimple:
            return EX_CollisionComplexity::UseComplexAsSimple;
        default:
            return EX_CollisionComplexity::UseDefault;
    }
}

TArray<FAssetData> UX_CollisionBlueprintLibrary::ConvertToAssetDataArray(const TArray<UStaticMesh*>& StaticMeshes)
{
    TArray<FAssetData> AssetDataArray;
    AssetDataArray.Reserve(StaticMeshes.Num());

    for (UStaticMesh* StaticMesh : StaticMeshes)
    {
        if (StaticMesh)
        {
            FAssetData AssetData(StaticMesh);
            AssetDataArray.Add(AssetData);
        }
    }

    return AssetDataArray;
}
