/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetData.h"

class AActor;

/**
 * 材质收集器类
 * 负责从各种资源中收集材质
 */
class X_ASSETEDITOR_API FX_MaterialFunctionCollector
{
public:
    /**
     * 从资产收集材质
     * @param Asset - 资产数据
     * @return 材质列表
     */
    static TArray<UMaterial*> CollectMaterialsFromAsset(const FAssetData& Asset);

    /**
     * 从Actor收集材质
     * @param Actor - Actor对象
     * @return 材质列表
     */
    static TArray<UMaterial*> CollectMaterialsFromActor(AActor* Actor);

    /**
     * 并行从资产收集材质
     * @param Assets - 资产列表
     * @return 材质列表
     */
    static TArray<UMaterial*> CollectMaterialsFromAssetParallel(const TArray<FAssetData>& Assets);

    /**
     * 并行从Actor收集材质
     * @param Actors - Actor列表
     * @return 材质列表
     */
    static TArray<UMaterial*> CollectMaterialsFromActorParallel(const TArray<AActor*>& Actors);
    
    /**
     * 从资产收集材质接口
     * @param SourceObjects - 源对象列表
     * @return 材质接口列表
     */
    static TArray<UMaterialInterface*> CollectMaterialsFromAssets(TArray<UObject*> SourceObjects);
}; 