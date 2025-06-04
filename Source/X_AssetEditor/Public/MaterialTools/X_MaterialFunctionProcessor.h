// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialFunction.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "AssetRegistry/AssetData.h"
#include "MaterialTools/X_MaterialFunctionParams.h"

/**
 * 材质函数处理器类
 * 负责材质函数批量处理和高级操作
 */
class X_ASSETEDITOR_API FX_MaterialFunctionProcessor
{
public:
    /**
     * 为多个资产添加材质函数
     * @param SourceObjects - 源对象
     * @param MaterialFunction - 材质函数
     * @param NodeName - 节点名称
     * @param PosX - X坐标
     * @param PosY - Y坐标
     * @param bSetupConnections - 是否设置连接
     * @param Params - 材质函数参数
     * @return 处理结果
     */
    static FMaterialProcessResult AddFunctionToMultipleMaterials(
        const TArray<UObject*>& SourceObjects,
        UMaterialFunctionInterface* MaterialFunction,
        const FName& NodeName,
        int32 PosX = 0,
        int32 PosY = 0,
        bool bSetupConnections = false,
        const TSharedPtr<FX_MaterialFunctionParams>& Params = nullptr);

    /**
     * 为多个资产添加菲涅尔函数
     * @param SourceObjects - 源对象
     * @return 处理结果
     */
    static FMaterialProcessResult AddFresnelToAssets(
        const TArray<UObject*>& SourceObjects);
        
    /**
     * 处理资产上的材质函数应用
     * @param SelectedAssets - 选中的资产
     * @param MaterialFunction - 要应用的材质函数
     * @param TargetNode - 目标节点名称
     */
    static void ProcessAssetMaterialFunction(
        const TArray<FAssetData>& SelectedAssets,
        UMaterialFunctionInterface* MaterialFunction,
        const FName& TargetNode);

    /**
     * 处理Actor上的材质函数应用
     * @param SelectedActors - 选中的Actor
     * @param MaterialFunction - 要应用的材质函数
     * @param TargetNode - 目标节点名称
     */
    static void ProcessActorMaterialFunction(
        const TArray<AActor*>& SelectedActors,
        UMaterialFunctionInterface* MaterialFunction,
        const FName& TargetNode);
}; 