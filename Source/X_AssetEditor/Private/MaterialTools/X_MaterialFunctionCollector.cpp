#include "MaterialTools/X_MaterialFunctionCollector.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Logging/LogMacros.h"
#include "X_AssetEditor.h"
#include "MaterialTools/X_MaterialFunctionCore.h"

// 并行处理相关
#include "Async/ParallelFor.h"
#include "HAL/CriticalSection.h"

TArray<UMaterial*> FX_MaterialFunctionCollector::CollectMaterialsFromAsset(const FAssetData& Asset)
{
    TArray<UMaterial*> Materials;
    
    // 加载资产
    UObject* AssetObject = Asset.GetAsset();
    if (!AssetObject)
    {
        return Materials;
    }
    
    // 处理不同类型的资产
    if (UMaterial* Material = Cast<UMaterial>(AssetObject))
    {
        Materials.Add(Material);
    }
    else if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(AssetObject))
    {
        UMaterial* BaseMaterial = FX_MaterialFunctionCore::GetBaseMaterial(MaterialInstance);
        if (BaseMaterial)
        {
            Materials.AddUnique(BaseMaterial);
        }
    }
    else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetObject))
    {
        // 处理静态网格体的材质
        for (const FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
        {
            if (StaticMaterial.MaterialInterface)
            {
                UMaterial* BaseMaterial = FX_MaterialFunctionCore::GetBaseMaterial(StaticMaterial.MaterialInterface);
                if (BaseMaterial)
                {
                    Materials.AddUnique(BaseMaterial);
                }
            }
        }
    }
    else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(AssetObject))
    {
        // 处理骨骼网格体的材质
        for (const FSkeletalMaterial& SkeletalMaterial : SkeletalMesh->GetMaterials())
        {
            if (SkeletalMaterial.MaterialInterface)
            {
                UMaterial* BaseMaterial = FX_MaterialFunctionCore::GetBaseMaterial(SkeletalMaterial.MaterialInterface);
                if (BaseMaterial)
                {
                    Materials.AddUnique(BaseMaterial);
                }
            }
        }
    }
    
    return Materials;
}

TArray<UMaterial*> FX_MaterialFunctionCollector::CollectMaterialsFromActor(AActor* Actor)
{
    TArray<UMaterial*> Materials;
    
    if (!Actor)
    {
        return Materials;
    }
    
    // 获取所有网格体组件
    TArray<UMeshComponent*> MeshComponents;
    Actor->GetComponents<UMeshComponent>(MeshComponents);
    
    // 处理所有网格体组件的材质
    for (UMeshComponent* MeshComponent : MeshComponents)
    {
        if (MeshComponent)
        {
            const int32 NumMaterials = MeshComponent->GetNumMaterials();
            for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
            {
                UMaterialInterface* MaterialInterface = MeshComponent->GetMaterial(MaterialIndex);
                if (MaterialInterface)
                {
                    UMaterial* BaseMaterial = FX_MaterialFunctionCore::GetBaseMaterial(MaterialInterface);
                    if (BaseMaterial)
                    {
                        Materials.AddUnique(BaseMaterial);
                    }
                }
            }
        }
    }
    
    return Materials;
}

TArray<UMaterial*> FX_MaterialFunctionCollector::CollectMaterialsFromAssetParallel(const TArray<FAssetData>& Assets)
{
    TArray<UMaterial*> AllMaterials;
    
    // 并行处理，提高性能
    FCriticalSection CriticalSection;
    ParallelFor(Assets.Num(), [&](int32 Index)
    {
        // 收集单个资产的材质
        TArray<UMaterial*> AssetMaterials = CollectMaterialsFromAsset(Assets[Index]);
        
        // 线程安全地添加到结果
        FScopeLock Lock(&CriticalSection);
        for (UMaterial* Material : AssetMaterials)
        {
            AllMaterials.AddUnique(Material);
        }
    });
    
    return AllMaterials;
}

TArray<UMaterial*> FX_MaterialFunctionCollector::CollectMaterialsFromActorParallel(const TArray<AActor*>& Actors)
{
    TArray<UMaterial*> AllMaterials;
    
    // 并行处理，提高性能
    FCriticalSection CriticalSection;
    ParallelFor(Actors.Num(), [&](int32 Index)
    {
        // 收集单个Actor的材质
        TArray<UMaterial*> ActorMaterials = CollectMaterialsFromActor(Actors[Index]);
        
        // 线程安全地添加到结果
        FScopeLock Lock(&CriticalSection);
        for (UMaterial* Material : ActorMaterials)
        {
            AllMaterials.AddUnique(Material);
        }
    });
    
    return AllMaterials;
}

TArray<UMaterialInterface*> FX_MaterialFunctionCollector::CollectMaterialsFromAssets(
    TArray<UObject*> SourceObjects)
{
    TArray<UMaterialInterface*> CollectedMaterials;
    
    for (UObject* Object : SourceObjects)
    {
        if (!Object)
        {
            continue;
        }
        
        // 处理材质和材质实例
        if (UMaterialInterface* ObjectMaterial = Cast<UMaterialInterface>(Object))
        {
            CollectedMaterials.Add(ObjectMaterial);
        }
        // 处理静态网格体
        else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
        {
            const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
            for (const FStaticMaterial& StaticMaterial : StaticMaterials)
            {
                if (StaticMaterial.MaterialInterface)
                {
                    CollectedMaterials.Add(StaticMaterial.MaterialInterface);
                }
            }
        }
        // 处理骨骼网格体
        else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
        {
            const TArray<FSkeletalMaterial>& SkeletalMaterials = SkeletalMesh->GetMaterials();
            for (const FSkeletalMaterial& SkeletalMaterial : SkeletalMaterials)
            {
                if (SkeletalMaterial.MaterialInterface)
                {
                    CollectedMaterials.Add(SkeletalMaterial.MaterialInterface);
                }
            }
        }
        // 处理Actor（包含网格体组件）
        else if (AActor* Actor = Cast<AActor>(Object))
        {
            // 获取Actor的所有网格体组件，包括可能嵌套在其他组件中的组件
            TArray<UMeshComponent*> MeshComponents;
            Actor->GetComponents<UMeshComponent>(MeshComponents, true); // 第二个参数true表示包括子组件
            
            UE_LOG(LogX_AssetEditor, Log, TEXT("处理Actor %s 的材质，找到 %d 个网格体组件"), 
                *Actor->GetName(), MeshComponents.Num());
            
            for (UMeshComponent* MeshComp : MeshComponents)
            {
                if (MeshComp)
                {
                    int32 MaterialCount = MeshComp->GetNumMaterials();
                    UE_LOG(LogX_AssetEditor, Log, TEXT("  组件 %s 有 %d 个材质槽"), 
                        *MeshComp->GetName(), MaterialCount);
                    
                    for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; MaterialIndex++)
                    {
                        UMaterialInterface* SlotMaterial = MeshComp->GetMaterial(MaterialIndex);
                        if (SlotMaterial)
                        {
                            UE_LOG(LogX_AssetEditor, Log, TEXT("    槽 %d: 材质 %s"), 
                                MaterialIndex, *SlotMaterial->GetName());
                            CollectedMaterials.Add(SlotMaterial);
                        }
                    }
                }
            }
        }
    }
    
    return CollectedMaterials;
} 