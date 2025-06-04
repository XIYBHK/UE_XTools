// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetData.h"

/**
 * 材质函数核心功能类
 * 提供基础材质操作功能
 */
class X_ASSETEDITOR_API FX_MaterialFunctionCore
{
public:
    /**
     * 获取所有材质函数
     * @return 材质函数列表
     */
    static TArray<UMaterialFunctionInterface*> GetAllMaterialFunctions();

    /**
     * 获取基础材质
     * @param MaterialInterface - 材质接口
     * @return 基础材质
     */
    static UMaterial* GetBaseMaterial(UMaterialInterface* MaterialInterface);

    /**
     * 获取菲涅尔函数
     * @return 菲涅尔材质函数
     */
    static UMaterialFunctionInterface* GetFresnelFunction();

    /**
     * 重新编译材质
     * @param Material - 要重新编译的材质
     */
    static void RecompileMaterial(UMaterial* Material);

    /**
     * 刷新已打开的材质编辑器
     * @param Material - 要刷新的材质
     * @return 是否成功刷新
     */
    static bool RefreshOpenMaterialEditor(UMaterial* Material);
    
    /**
     * 通过名称获取材质函数
     * @param FunctionName - 材质函数名称
     * @return 材质函数
     */
    static UMaterialFunctionInterface* GetMaterialFunctionByName(const FString& FunctionName);
}; 