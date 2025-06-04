// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MaterialTools/X_MaterialFunctionParams.h"

/**
 * 材质函数连接器类
 * 负责处理材质函数和材质属性之间的连接
 */
class X_ASSETEDITOR_API FX_MaterialFunctionConnector
{
public:
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
        TSharedPtr<FX_MaterialFunctionParams> Params = nullptr);
        
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