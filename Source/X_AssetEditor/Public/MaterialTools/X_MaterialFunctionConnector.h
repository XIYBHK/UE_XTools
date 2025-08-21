// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "ScopedTransaction.h"
#include "Logging/LogMacros.h"
#include "MaterialTools/X_MaterialFunctionParams.h"
#include "X_AssetEditor.h"

class UMaterialEditorOnlyData;

/**
 * 材质函数连接器类
 * 负责处理材质函数和材质属性之间的连接
 * 支持常规引脚和MaterialAttributes引脚的智能连接
 */
class X_ASSETEDITOR_API FX_MaterialFunctionConnector
{
public:
    /**
     * 检测材质函数是否使用MaterialAttributes模式
     * @param FunctionCall - 材质函数调用
     * @return 是否使用MaterialAttributes
     */
    static bool IsUsingMaterialAttributes(UMaterialExpressionMaterialFunctionCall* FunctionCall);
    
    /**
     * 检测材质是否启用了"使用材质属性"设置
     * @param Material - 材质对象
     * @return 是否启用MaterialAttributes设置
     */
    static bool IsMaterialAttributesEnabled(UMaterial* Material);
    
    /**
     * 检测材质函数是否适合MaterialAttributes连接
     * @param FunctionCall - 材质函数调用
     * @return 是否适合MaterialAttributes连接
     */
    static bool IsFunctionSuitableForAttributes(UMaterialExpressionMaterialFunctionCall* FunctionCall);
    
    /**
     * 连接MaterialAttributes到材质主节点（使用UE官方API）
     * @param Material - 材质
     * @param FunctionCall - 材质函数调用
     * @param OutputIndex - 输出引脚索引
     * @return 是否成功连接
     */
    static bool ConnectMaterialAttributesToMaterial(UMaterial* Material, 
                                                   UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                   int32 OutputIndex = 0);
    
    /**
     * 智能连接到已有的MaterialAttributes表达式（如MakeMaterialAttributes）
     * @param MaterialAttributesExpression - 已有的MaterialAttributes表达式
     * @param FunctionCall - 要连接的函数调用
     * @param OutputIndex - 输出引脚索引
     * @return 是否成功连接
     */
    static bool ConnectToMaterialAttributesExpression(UMaterialExpression* MaterialAttributesExpression,
                                                     UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                     int32 OutputIndex = 0);
    
    /**
     * 连接到MakeMaterialAttributes节点的特定输入
     * @param MakeMAExpression - MakeMaterialAttributes表达式
     * @param FunctionCall - 要连接的函数调用
     * @param OutputIndex - 输出引脚索引
     * @return 是否成功连接
     */
    static bool ConnectToMakeMaterialAttributesNode(UMaterialExpression* MakeMAExpression,
                                                   UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                   int32 OutputIndex = 0);
    
    /**
     * 基于UE源码直接连接到MakeMaterialAttributes的成员变量
     * @param MakeMAExpression - MakeMaterialAttributes表达式
     * @param FunctionCall - 要连接的函数调用
     * @param TargetProperty - 目标材质属性
     * @param OutputIndex - 输出引脚索引
     * @return 是否成功连接
     */
    static bool ConnectToMakeMaterialAttributesDirect(UMaterialExpression* MakeMAExpression,
                                                     UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                     EMaterialProperty TargetProperty,
                                                     int32 OutputIndex = 0);
    
    /**
     * 处理MaterialAttributes模式下的输入引脚自动连接
     * @param Material - 目标材质
     * @param FunctionCall - 要连接的函数调用
     * @return 是否有任何输入连接成功
     */
    static bool ProcessMaterialAttributesInputConnections(UMaterial* Material,
                                                         UMaterialExpressionMaterialFunctionCall* FunctionCall);
    
    /**
     * 处理用户手动配置的连接
     * @param Material - 目标材质
     * @param FunctionCall - 要连接的函数调用
     * @param ConnectionMode - 连接模式
     * @param Params - 用户配置参数
     * @return 是否成功连接
     */
    static bool ProcessManualConnections(UMaterial* Material,
                                        UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                        EConnectionMode ConnectionMode,
                                        TSharedPtr<FX_MaterialFunctionParams> Params);
    
    /**
     * 处理手动模式下的MaterialAttributes连接
     * @param Material - 目标材质
     * @param FunctionCall - 要连接的函数调用
     * @param Params - 用户配置参数
     * @return 是否成功连接
     */
    static bool ProcessManualMaterialAttributesConnections(UMaterial* Material,
                                                          UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                          TSharedPtr<FX_MaterialFunctionParams> Params);
    
    /**
     * 处理手动连接到MakeMaterialAttributes节点
     * @param MakeMAExpression - MakeMaterialAttributes表达式
     * @param FunctionCall - 要连接的函数调用
     * @param Params - 用户配置参数
     * @return 是否成功连接
     */
    static bool ProcessManualConnectionsToMakeMaterialAttributes(UMaterialExpression* MakeMAExpression,
                                                               UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                               TSharedPtr<FX_MaterialFunctionParams> Params);
    
    /**
     * 连接到MaterialAttributes函数的输入
     * @param ExistingFunctionCall - 已有的MaterialAttributes函数调用
     * @param FunctionCall - 要连接的函数调用
     * @param OutputIndex - 输出引脚索引
     * @return 是否成功连接
     */
    static bool ConnectToMaterialAttributesFunctionInputs(UMaterialExpressionMaterialFunctionCall* ExistingFunctionCall,
                                                          UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                          int32 OutputIndex = 0);
    
    /**
     * 通用MaterialAttributes表达式连接
     * @param MaterialAttributesExpression - MaterialAttributes表达式
     * @param FunctionCall - 要连接的函数调用
     * @param OutputIndex - 输出引脚索引
     * @return 是否成功连接
     */
    static bool ConnectToGenericMaterialAttributesExpression(UMaterialExpression* MaterialAttributesExpression,
                                                            UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                            int32 OutputIndex = 0);

private:
    /**
     * 获取材质属性的显示名称
     * @param MaterialProperty - 材质属性枚举
     * @return 显示名称
     */
    static FString GetMaterialPropertyDisplayName(EMaterialProperty MaterialProperty);
    
    /**
     * 直接连接到材质属性（备用方案，使用映射表优化）
     * @param EditorOnlyData - 材质编辑器数据
     * @param Expression - 表达式
     * @param MaterialProperty - 材质属性
     * @param OutputIndex - 输出索引
     * @return 是否成功
     */
    static bool ConnectToMaterialPropertyDirect(UMaterialEditorOnlyData* EditorOnlyData,
                                               UMaterialExpression* Expression,
                                               EMaterialProperty MaterialProperty,
                                               int32 OutputIndex);

    // ✅ 错误恢复机制：使用UE内置事务系统
    
    /**
     * 准备材质对象用于修改（调用Modify方法支持撤销/重做）
     * @param Material - 材质对象
     * @return 是否成功准备
     */
    static bool PrepareForModification(UMaterial* Material);
    
    /**
     * 执行带事务保护的材质连接操作
     * @param Material - 材质对象
     * @param TransactionText - 事务描述文本
     * @param Operation - 要执行的操作
     * @return 操作是否成功
     */
    template<typename FunctionType>
    static bool ExecuteWithTransaction(UMaterial* Material, const FText& TransactionText, FunctionType&& Operation)
    {
        if (!Material)
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("材质对象为空，无法执行事务操作"));
            return false;
        }
        
        // ✅ 使用FScopedTransaction自动管理事务范围
        FScopedTransaction Transaction(TransactionText);
        
        // ✅ 准备对象用于修改（支持撤销/重做）
        if (!PrepareForModification(Material))
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("准备材质修改失败，取消事务"));
            return false;
        }
        
        UE_LOG(LogX_AssetEditor, Log, TEXT("开始事务: %s，材质: %s"), 
            *TransactionText.ToString(), *Material->GetName());
        
        // ✅ 执行用户操作
        bool bOperationSuccess = Operation();
        
        if (bOperationSuccess)
        {
            // ✅ 操作成功，标记材质为已修改并重新编译
            Material->MarkPackageDirty();
            Material->PostEditChange();
            
            UE_LOG(LogX_AssetEditor, Log, TEXT("事务成功完成: %s"), *TransactionText.ToString());
        }
        else
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("事务操作失败: %s，将自动回滚"), *TransactionText.ToString());
            // ✅ FScopedTransaction析构时会自动回滚事务
        }
        
        return bOperationSuccess;
    }

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