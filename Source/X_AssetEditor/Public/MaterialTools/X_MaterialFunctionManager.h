/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/SWindow.h"
#include "MaterialTools/X_MaterialFunctionOperation.h"
#include "MaterialTools/X_MaterialFunctionParams.h"

// 材质表达式相关头文件
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

class UMaterialExpressionMaterialFunctionCall;
class SWindow;

// 材质函数选择回调
DECLARE_DELEGATE_OneParam(FOnMaterialFunctionSelected, UMaterialFunctionInterface*);

// 材质节点选择回调
DECLARE_DELEGATE_OneParam(FOnMaterialNodeSelected, FName);

/**
 * 材质函数管理器
 * 作为门面类，提供统一的接口，委托具体实现给其他模块
 */
class X_ASSETEDITOR_API FX_MaterialFunctionManager
{
public:
    // 材质基础操作
    static UMaterial* GetBaseMaterial(UMaterialInterface* MaterialInterface);
    static TArray<UMaterialFunctionInterface*> GetAllMaterialFunctions();
    static UMaterialFunctionInterface* GetFresnelFunction();
    static void RecompileMaterial(UMaterial* Material);
    static bool RefreshOpenMaterialEditor(UMaterial* Material);
    
    // 材质函数操作
    static UMaterialExpressionMaterialFunctionCall* FindNodeInMaterial(UMaterial* Material, const FName& NodeName);
    static bool DoesMaterialContainFunction(UMaterial* Material, UMaterialFunctionInterface* Function);
    static UMaterialExpressionMaterialFunctionCall* AddFunctionToMaterial(UMaterial* Material, UMaterialFunctionInterface* Function, const FName& NodeName, int32 PosX = 0, int32 PosY = 0);
    
    // 材质属性连接
    static bool ConnectExpressionToMaterialProperty(UMaterial* Material, UMaterialExpression* Expression, EMaterialProperty MaterialProperty, int32 OutputIndex = 0);
    static bool ConnectExpressionToMaterialPropertyByName(UMaterial* Material, UMaterialExpression* Expression, const FString& PropertyName, int32 OutputIndex = 0);

    
    // 批量处理接口
    static FMaterialProcessResult AddFunctionToMultipleMaterials(const TArray<UObject*>& SourceObjects, UMaterialFunctionInterface* MaterialFunction, const FName& NodeName, int32 PosX = 0, int32 PosY = 0, bool bSetupConnections = false, const FX_MaterialFunctionParams* Params = nullptr);
    static FMaterialProcessResult AddFresnelToAssets(const TArray<UObject*>& SourceObjects);
    
    // 材质表达式创建
    static UMaterialExpressionMaterialFunctionCall* CreateMaterialFunctionCallExpression(UMaterial* Material, UMaterialFunctionInterface* Function, int32 PosX = 0, int32 PosY = 0);

    
    // UI操作
    static TSharedRef<SWindow> CreateMaterialFunctionPickerWindow(FOnMaterialFunctionSelected OnFunctionSelected);
    static TSharedRef<SWindow> ShowNewMaterialFunctionPicker(FOnMaterialFunctionSelected OnFunctionSelected);
    static TSharedRef<SWindow> CreateNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected);
    static TArray<TSharedPtr<FName>> GetCommonNodeNames();
    
    // 特定功能接口

    
    /**
     * 从资产收集材质
     * @param SourceObjects - 源对象列表
     * @return 材质接口列表
     */
    static TArray<UMaterialInterface*> CollectMaterialsFromAssets(TArray<UObject*> SourceObjects);
    

};