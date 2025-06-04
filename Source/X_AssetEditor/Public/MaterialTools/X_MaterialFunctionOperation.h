// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "X_MaterialFunctionParams.h"

class AActor;

/**
 * 材质处理结果结构
 * 记录批量处理材质的结果统计
 */
struct FMaterialProcessResult
{
    int32 TotalSourceObjects = 0;
    int32 TotalMaterials = 0;
    int32 SuccessCount = 0;
    int32 FailedCount = 0;
    int32 AlreadyHasFunctionCount = 0;

    FString GetSummaryString() const
    {
        return FString::Printf(TEXT("处理结果: 总源对象=%d, 总材质=%d, 成功=%d, 失败=%d, 已有函数=%d"),
            TotalSourceObjects, TotalMaterials, SuccessCount, FailedCount, AlreadyHasFunctionCount);
    }
};

/**
 * 材质函数操作类
 * 负责材质函数在材质上的应用和操作
 */
class X_ASSETEDITOR_API FX_MaterialFunctionOperation
{
public:
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
     * 获取基础材质（委托到Core）
     * @param MaterialInterface - 材质接口
     * @return 基础材质
     */
    static UMaterial* GetBaseMaterial(UMaterialInterface* MaterialInterface);
    
    /**
     * 向材质添加材质函数
     * @param Material - 材质
     * @param MaterialFunction - 材质函数
     * @param TargetNode - 目标节点名称
     */
    static void AddMaterialFunctionToMaterial(
        UMaterial* Material,
        UMaterialFunctionInterface* MaterialFunction,
        const FName& TargetNode,
        const TSharedPtr<FX_MaterialFunctionParams>& UserParams = nullptr);

public:
    /**
     * 在材质中查找节点
     * @param Material - 材质
     * @param NodeName - 节点名称
     * @return 找到的材质函数调用表达式
     */
    static UMaterialExpressionMaterialFunctionCall* FindNodeInMaterial(
        UMaterial* Material, 
        const FName& NodeName);

    /**
     * 检查材质是否包含指定函数
     * @param Material - 材质
     * @param Function - 材质函数
     * @return 是否包含
     */
    static bool DoesMaterialContainFunction(
        UMaterial* Material,
        UMaterialFunctionInterface* Function);

    /**
     * 添加函数到材质
     * @param Material - 材质
     * @param Function - 材质函数
     * @param NodeName - 节点名称
     * @param PosX - X坐标
     * @param PosY - Y坐标
     * @param bSetupConnections - 是否设置连接
     * @param bEnableSmartConnect - 是否启用智能连接
     * @param ConnectionMode - 连接模式
     * @return 创建的函数调用表达式
     */
    static UMaterialExpressionMaterialFunctionCall* AddFunctionToMaterial(
        UMaterial* Material,
        UMaterialFunctionInterface* Function,
        const FName& NodeName,
        int32 PosX = 0,
        int32 PosY = 0,
        bool bSetupConnections = true,
        bool bEnableSmartConnect = true,
        EConnectionMode ConnectionMode = EConnectionMode::Add,
        const TSharedPtr<FX_MaterialFunctionParams>& UserParams = nullptr);

    /**
     * 添加函数到材质（简化版本，使用UserParams中的设置）
     * @param Material - 材质
     * @param Function - 材质函数
     * @param NodeName - 节点名称
     * @param UserParams - 用户参数
     * @return 创建的函数调用表达式
     */
    static UMaterialExpressionMaterialFunctionCall* AddFunctionToMaterial(
        UMaterial* Material,
        UMaterialFunctionInterface* Function,
        const FName& NodeName,
        const TSharedPtr<FX_MaterialFunctionParams>& UserParams);

    /**
     * 连接表达式到材质属性
     * @param Material - 材质
     * @param Expression - 表达式
     * @param MaterialProperty - 材质属性
     * @param OutputIndex - 输出索引
     * @return 是否成功
     */
    static bool ConnectExpressionToMaterialProperty(
        UMaterial* Material,
        UMaterialExpression* Expression,
        EMaterialProperty MaterialProperty,
        int32 OutputIndex = 0);

    /**
     * 通过名称连接表达式到材质属性
     * @param Material - 材质
     * @param Expression - 表达式
     * @param PropertyName - 属性名称
     * @param OutputIndex - 输出索引
     * @return 是否成功
     */
    static bool ConnectExpressionToMaterialPropertyByName(
        UMaterial* Material,
        UMaterialExpression* Expression,
        const FString& PropertyName,
        int32 OutputIndex = 0);
        
    /**
     * 自动设置材质函数的智能连接
     * @param Material - 材质
     * @param FunctionCall - 材质函数调用表达式
     * @param ConnectionMode - 连接模式
     * @param Params - 材质函数参数，可选，包含用户设置的连接选项
     * @return 是否成功设置连接
     */
    static bool SetupAutoConnections(
        UMaterial* Material, 
        UMaterialExpressionMaterialFunctionCall* FunctionCall,
        EConnectionMode ConnectionMode,
        const TSharedPtr<FX_MaterialFunctionParams>& Params = nullptr);
        
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
     * 从资产收集材质
     * @param SourceObjects - 源对象列表
     * @return 材质接口列表
     */
    static TArray<UMaterialInterface*> CollectMaterialsFromAssets(
        TArray<UObject*> SourceObjects);
        
    /**
     * 创建材质函数调用表达式
     * @param Material - 材质
     * @param Function - 材质函数
     * @param PosX - X坐标
     * @param PosY - Y坐标
     * @return 创建的表达式
     */
    static UMaterialExpressionMaterialFunctionCall* CreateMaterialFunctionCallExpression(
        UMaterial* Material,
        UMaterialFunctionInterface* Function,
        int32 PosX = 0,
        int32 PosY = 0);

    /**
     * 检查函数是否有输入和输出
     * @param Function - 材质函数
     * @return 是否同时具有输入和输出引脚
     */
    static bool CheckFunctionHasInputsAndOutputs(UMaterialFunctionInterface* Function);

    /**
     * 获取函数的输入输出数量
     * @param Function - 材质函数
     * @param OutInputCount - 输出参数，输入引脚数量
     * @param OutOutputCount - 输出参数，输出引脚数量
     */
    static void GetFunctionInputOutputCount(
        UMaterialFunctionInterface* Function,
        int32& OutInputCount, 
        int32& OutOutputCount);
        
    /**
     * 创建Add节点连接到指定材质属性
     * @param Material - 材质
     * @param FunctionCall - 材质函数调用表达式
     * @param OutputIndex - 输出引脚索引
     * @param MaterialProperty - 目标材质属性
     * @return 创建的Add表达式
     */
    static UMaterialExpressionAdd* CreateAddConnectionToProperty(
        UMaterial* Material,
        UMaterialExpressionMaterialFunctionCall* FunctionCall,
        int32 OutputIndex,
        EMaterialProperty MaterialProperty);
        
    /**
     * 创建Multiply节点连接到指定材质属性
     * @param Material - 材质
     * @param FunctionCall - 材质函数调用表达式
     * @param OutputIndex - 输出引脚索引
     * @param MaterialProperty - 目标材质属性
     * @return 创建的Multiply表达式
     */
    static UMaterialExpressionMultiply* CreateMultiplyConnectionToProperty(
        UMaterial* Material,
        UMaterialExpressionMaterialFunctionCall* FunctionCall,
        int32 OutputIndex,
        EMaterialProperty MaterialProperty);
};