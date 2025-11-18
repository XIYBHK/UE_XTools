#include "MaterialTools/X_MaterialFunctionConnector.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Engine/Engine.h"
#include "Logging/LogMacros.h"
#include "X_AssetEditor.h"
#include "MaterialTools/X_MaterialFunctionParams.h"
//  添加UE官方API支持
#include "MaterialEditingLibrary.h"
//  添加MakeMaterialAttributes支持
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
//  引入常量定义
#include "MaterialTools/X_MaterialConstants.h"

namespace
{
    /**
     * 计算字符串匹配分数
     * @param InputName 输入名称（例如函数引脚名）
     * @param TargetName 目标名称（例如材质属性名）
     * @return 匹配分数（越高越好，负数表示不匹配或黑名单）
     */
    int32 CalculateMatchScore(const FString& InputName, const FString& TargetName)
    {
        if (InputName.IsEmpty() || TargetName.IsEmpty())
        {
            return -1;
        }

        // 转换为小写进行比较
        FString InputLower = InputName.ToLower();
        FString TargetLower = TargetName.ToLower();

        // 1. 黑名单检查
        if (InputLower.StartsWith(X_MaterialConstants::Prefix_Not.ToLower()) || 
            InputLower.StartsWith(X_MaterialConstants::Prefix_Ignore.ToLower()))
        {
            return -100; // 明确排除
        }

        // 2. 完全匹配 (最高分)
        if (InputLower.Equals(TargetLower))
        {
            return 100;
        }

        // 3. 包含匹配 (中等分数)
        if (InputLower.Contains(TargetLower))
        {
            // 简单的包含可能误判，例如 "NotBaseColor" (已由黑名单处理)
            // 或者 "BaseColorMap" -> BaseColor，这是合理的
            return 50;
        }

        return 0;
    }
}

bool FX_MaterialFunctionConnector::ConnectExpressionToMaterialProperty(
    UMaterial* Material,
    UMaterialExpression* Expression,
    EMaterialProperty MaterialProperty,
    int32 OutputIndex)
{
    if (!Material || !Expression)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质或表达式为空"));
        return false;
    }

    //  优先使用UE官方API进行连接
    FString OutputName = OutputIndex == 0 ? FString() : FString::Printf(TEXT("%d"), OutputIndex);
    bool bSuccess = UMaterialEditingLibrary::ConnectMaterialProperty(Expression, OutputName, MaterialProperty);
    
    if (bSuccess)
    {
        // 获取属性名称用于日志
        FString PropertyName = GetMaterialPropertyDisplayName(MaterialProperty);
        UE_LOG(LogX_AssetEditor, Log, TEXT("使用官方API成功连接到%s"), *PropertyName);
        
        // 确保材质被标记为已修改
        Material->MarkPackageDirty();
        return true;
    }
    
    //  备用方案：使用直接连接（确保向后兼容）
    UE_LOG(LogX_AssetEditor, Warning, TEXT("官方API连接失败，尝试直接连接"));
    
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法获取材质编辑器数据"));
        return false;
    }

    //  使用映射表简化属性连接
    return ConnectToMaterialPropertyDirect(EditorOnlyData, Expression, MaterialProperty, OutputIndex);
}

//  基于UE最佳实践：使用辅助函数获取材质输入引用

FString FX_MaterialFunctionConnector::GetMaterialPropertyDisplayName(EMaterialProperty MaterialProperty)
{
    //  基于UE材质编辑器的实际显示名称（带空格格式）
    switch (MaterialProperty)
    {
        case MP_BaseColor: return TEXT("Base Color");          //  UE编辑器显示名称
        case MP_Metallic: return TEXT("Metallic");
        case MP_Specular: return TEXT("Specular");
        case MP_Roughness: return TEXT("Roughness");
        case MP_EmissiveColor: return TEXT("Emissive Color");  //  UE编辑器显示名称
        case MP_Opacity: return TEXT("Opacity");
        case MP_OpacityMask: return TEXT("Opacity Mask");      //  UE编辑器显示名称
        case MP_Normal: return TEXT("Normal");
        case MP_WorldPositionOffset: return TEXT("World Position Offset"); //  UE编辑器显示名称
        case MP_SubsurfaceColor: return TEXT("Subsurface Color");          //  UE编辑器显示名称
        case MP_AmbientOcclusion: return TEXT("Ambient Occlusion");        //  UE编辑器显示名称
        case MP_Refraction: return TEXT("Refraction");
        case MP_MaterialAttributes: return TEXT("Material Attributes");    //  UE编辑器显示名称
        default: return FString::Printf(TEXT("Unknown(%d)"), (int32)MaterialProperty);
    }
}

bool FX_MaterialFunctionConnector::ConnectToMaterialPropertyDirect(UMaterialEditorOnlyData* EditorOnlyData,
                                                                   UMaterialExpression* Expression,
                                                                   EMaterialProperty MaterialProperty,
                                                                   int32 OutputIndex)
{
    if (!EditorOnlyData || !Expression)
    {
        return false;
    }

    //  基于UE源码：所有材质输入类型都继承自FExpressionInput，都有Connect方法
    switch (MaterialProperty)
    {
        case MP_BaseColor:
            EditorOnlyData->BaseColor.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到BaseColor"));
            return true;
            
        case MP_Metallic:
            EditorOnlyData->Metallic.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到Metallic"));
            return true;
            
        case MP_Specular:
            EditorOnlyData->Specular.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到Specular"));
            return true;
            
        case MP_Roughness:
            EditorOnlyData->Roughness.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到Roughness"));
            return true;
            
        case MP_EmissiveColor:
            EditorOnlyData->EmissiveColor.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到EmissiveColor"));
            return true;
            
        case MP_Opacity:
            EditorOnlyData->Opacity.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到Opacity"));
            return true;
            
        case MP_OpacityMask:
            EditorOnlyData->OpacityMask.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到OpacityMask"));
            return true;
            
        case MP_Normal:
            EditorOnlyData->Normal.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到Normal"));
            return true;
            
        case MP_WorldPositionOffset:
            EditorOnlyData->WorldPositionOffset.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到WorldPositionOffset"));
            return true;
            
        case MP_SubsurfaceColor:
            EditorOnlyData->SubsurfaceColor.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到SubsurfaceColor"));
            return true;
            
        case MP_AmbientOcclusion:
            EditorOnlyData->AmbientOcclusion.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到AmbientOcclusion"));
            return true;
            
        case MP_Refraction:
            EditorOnlyData->Refraction.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到Refraction"));
            return true;
            
        case MP_MaterialAttributes:
            EditorOnlyData->MaterialAttributes.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("直接连接成功连接到MaterialAttributes"));
            return true;
            
        default:
            UE_LOG(LogX_AssetEditor, Warning, TEXT("不支持的材质属性类型: %d"), (int32)MaterialProperty);
            return false;
    }
}

bool FX_MaterialFunctionConnector::ConnectExpressionToMaterialPropertyByName(
    UMaterial* Material,
    UMaterialExpression* Expression,
    const FString& PropertyName,
    int32 OutputIndex)
{
    if (!Material || !Expression)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质或表达式为空"));
        return false;
    }

    //  使用常量进行比较
    EMaterialProperty Property = MP_MAX;
    if (PropertyName == X_MaterialConstants::BaseColor)
        Property = MP_BaseColor;
    else if (PropertyName == X_MaterialConstants::Metallic)
        Property = MP_Metallic;
    else if (PropertyName == X_MaterialConstants::Specular)
        Property = MP_Specular;
    else if (PropertyName == X_MaterialConstants::Roughness)
        Property = MP_Roughness;
    else if (PropertyName == X_MaterialConstants::EmissiveColor)
        Property = MP_EmissiveColor;
    else if (PropertyName == X_MaterialConstants::Opacity)
        Property = MP_Opacity;
    else if (PropertyName == X_MaterialConstants::OpacityMask)
        Property = MP_OpacityMask;
    else if (PropertyName == X_MaterialConstants::Normal)
        Property = MP_Normal;
    else if (PropertyName == X_MaterialConstants::WorldPositionOffset)
        Property = MP_WorldPositionOffset;
    else if (PropertyName == X_MaterialConstants::SubsurfaceColor)
        Property = MP_SubsurfaceColor;
    else if (PropertyName == X_MaterialConstants::AmbientOcclusion)
        Property = MP_AmbientOcclusion;
    else if (PropertyName == X_MaterialConstants::Refraction)
        Property = MP_Refraction;
    else if (PropertyName == X_MaterialConstants::MaterialAttributes)
        Property = MP_MaterialAttributes;
    else
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("未找到匹配的材质属性: %s"), *PropertyName);
        return false;
    }

    return ConnectExpressionToMaterialProperty(Material, Expression, Property, OutputIndex);
}

bool FX_MaterialFunctionConnector::SetupAutoConnections(
    UMaterial* Material, 
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    EConnectionMode ConnectionMode,
    TSharedPtr<FX_MaterialFunctionParams> Params)
{
    if (!Material || !FunctionCall)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质或函数调用为空"));
        return false;
    }

    //  最高优先级：检查用户是否禁用了智能连接
    if (Params.IsValid() && !Params->bEnableSmartConnect)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("用户禁用了智能连接，使用手动配置模式"));
        return ProcessManualConnections(Material, FunctionCall, ConnectionMode, Params);
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("正在对材质 %s 应用智能连接逻辑..."), *Material->GetName());

    //  智能连接模式：正确的优先级逻辑
    bool bShouldUseMaterialAttributes = false;
    
    // 1. 【最高优先级】检查用户是否强制指定了MaterialAttributes
    if (Params.IsValid() && Params->bUseMaterialAttributes)
    {
        bShouldUseMaterialAttributes = true;
        UE_LOG(LogX_AssetEditor, Log, TEXT("用户强制指定使用MaterialAttributes模式"));
    }
    // 2. 【核心逻辑】检查材质是否启用了"使用材质属性"设置
    else if (IsMaterialAttributesEnabled(Material))
    {
        // 材质启用了MaterialAttributes，检查函数是否适合
        if (IsFunctionSuitableForAttributes(FunctionCall))
        {
            bShouldUseMaterialAttributes = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("材质启用MaterialAttributes且函数适合，使用MaterialAttributes连接"));
        }
        else
        {
            // 材质启用了MaterialAttributes但函数不适合，给出警告但仍尝试连接
            bShouldUseMaterialAttributes = true;
            UE_LOG(LogX_AssetEditor, Warning, TEXT("材质启用MaterialAttributes但函数可能不适合，仍尝试MaterialAttributes连接"));
        }
    }
    else
    {
        // 材质未启用MaterialAttributes，强制使用常规连接
        UE_LOG(LogX_AssetEditor, Log, TEXT("材质未启用MaterialAttributes，使用常规连接模式"));
        bShouldUseMaterialAttributes = false;
    }
    
    if (bShouldUseMaterialAttributes)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("使用MaterialAttributes专用连接逻辑"));
        return ConnectMaterialAttributesToMaterial(Material, FunctionCall, 0);
    }

    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法获取材质编辑器数据"));
        return false;
    }

    // 获取函数输入和输出
    const TArray<FFunctionExpressionInput>& FunctionInputs = FunctionCall->FunctionInputs;
    const TArray<FFunctionExpressionOutput>& FunctionOutputs = FunctionCall->FunctionOutputs;

    UE_LOG(LogX_AssetEditor, Log, TEXT("函数 %s: 有 %d 个输入引脚和 %d 个输出引脚"), 
        FunctionCall->MaterialFunction ? *FunctionCall->MaterialFunction->GetName() : TEXT("未知"),
        FunctionInputs.Num(), FunctionOutputs.Num());

    bool bHasConnected = false;

    // 检查是否同时有输入和输出引脚
    bool bHasInputsAndOutputs = (FunctionInputs.Num() > 0 && FunctionOutputs.Num() > 0);
    if (bHasInputsAndOutputs)
    {
        // 强制重写连接模式为None，确保不使用Add/Multiply节点
        ConnectionMode = EConnectionMode::None;
    }

    // 记录所有可用的材质属性连接
    struct FPropertyConnection
    {
        FExpressionInput* Input;
        EMaterialProperty Property;
        FExpressionOutput* Output;
        FString PropertyName;
        TArray<FString> Aliases;
    };
    TArray<FPropertyConnection> PropertyConnections;

    // 辅助函数：添加连接目标
    auto AddConnectionTarget = [&](FExpressionInput& Input, EMaterialProperty Prop, const FString& Name, const TArray<FString>& Aliases = {})
    {
        if (Input.Expression)
        {
            PropertyConnections.Add({&Input, Prop, nullptr, Name, Aliases});
        }
    };

    // 检查现有的材质属性连接，并注册别名
    AddConnectionTarget(EditorOnlyData->BaseColor, MP_BaseColor, X_MaterialConstants::BaseColor, {X_MaterialConstants::Alias_Albedo, X_MaterialConstants::Alias_Diffuse});
    AddConnectionTarget(EditorOnlyData->Metallic, MP_Metallic, X_MaterialConstants::Metallic, {X_MaterialConstants::Alias_Metalness});
    AddConnectionTarget(EditorOnlyData->Specular, MP_Specular, X_MaterialConstants::Specular);
    AddConnectionTarget(EditorOnlyData->Roughness, MP_Roughness, X_MaterialConstants::Roughness, {X_MaterialConstants::Alias_Rough});
    AddConnectionTarget(EditorOnlyData->EmissiveColor, MP_EmissiveColor, X_MaterialConstants::EmissiveColor, {X_MaterialConstants::Alias_Emission, X_MaterialConstants::Alias_Emissive});
    AddConnectionTarget(EditorOnlyData->Normal, MP_Normal, X_MaterialConstants::Normal);
    AddConnectionTarget(EditorOnlyData->AmbientOcclusion, MP_AmbientOcclusion, X_MaterialConstants::AmbientOcclusion, {X_MaterialConstants::Alias_AO, X_MaterialConstants::Alias_Ambient});
    
    // 对于每一个输入，尝试找到匹配的现有连接
    for (const FFunctionExpressionInput& FunctionInput : FunctionInputs)
    {
        const FExpressionInput& Input = FunctionInput.Input;
        const FString InputName = Input.InputName.ToString();

        int32 BestScore = 0;
        FPropertyConnection* BestConnection = nullptr;

        // 遍历所有可能的连接目标，寻找最佳匹配
        for (FPropertyConnection& Connection : PropertyConnections)
        {
            if (Connection.Output) continue; // 已被占用

            // 1. 检查标准名称
            int32 Score = CalculateMatchScore(InputName, Connection.PropertyName);
            
            // 2. 检查别名
            for (const FString& Alias : Connection.Aliases)
            {
                int32 AliasScore = CalculateMatchScore(InputName, Alias);
                if (AliasScore > Score)
                {
                    Score = AliasScore;
                }
            }

            // 更新最佳匹配
            if (Score > BestScore)
            {
                BestScore = Score;
                BestConnection = &Connection;
            }
        }

        // 如果找到了足够好的匹配（>0分），则建立连接
        if (BestConnection && BestScore > 0)
        {
            // 连接这个材质属性到函数输入
            FExpressionInput* InputPtr = const_cast<FExpressionInput*>(&Input);
            if (InputPtr)
            {
                InputPtr->Connect(0, BestConnection->Input->Expression);
                UE_LOG(LogX_AssetEditor, Log, TEXT("自动连接 %s 到函数输入 %s (匹配度: %d)"),
                    *BestConnection->PropertyName, *InputName, BestScore);
                bHasConnected = true;
            }
        }
    }

    // 对于每一个输出，尝试连接到适当的材质属性
    if (bHasInputsAndOutputs) // 同时有输入和输出的函数，使用直接连接
    {
        for (const FFunctionExpressionOutput& FunctionOutput : FunctionOutputs)
        {
            const FExpressionOutput& Output = FunctionOutput.Output;
            const FString OutputName = Output.OutputName.ToString();
            
            // 查找输出索引
            int32 OutputIndex = 0;
            for (int32 i = 0; i < FunctionOutputs.Num(); ++i)
            {
                if (&FunctionOutputs[i] == &FunctionOutput)
                {
                    OutputIndex = i;
                    break;
                }
            }

            // 查找最佳匹配的材质属性
            int32 BestScore = 0;
            EMaterialProperty BestProperty = MP_MAX;
            FString BestPropertyName;

            // 定义要检查的属性列表
            struct FTargetProp { EMaterialProperty P; FString N; TArray<FString> A; };
            TArray<FTargetProp> Targets = {
                {MP_BaseColor, X_MaterialConstants::BaseColor, {X_MaterialConstants::Alias_Albedo}},
                {MP_Metallic, X_MaterialConstants::Metallic, {}},
                {MP_Roughness, X_MaterialConstants::Roughness, {}},
                {MP_Normal, X_MaterialConstants::Normal, {}},
                {MP_EmissiveColor, X_MaterialConstants::EmissiveColor, {X_MaterialConstants::Alias_Emission}},
                {MP_AmbientOcclusion, X_MaterialConstants::AmbientOcclusion, {X_MaterialConstants::Alias_AO}}
            };

            for (const auto& Target : Targets)
            {
                int32 Score = CalculateMatchScore(OutputName, Target.N);
                for (const FString& Alias : Target.A)
                {
                    Score = FMath::Max(Score, CalculateMatchScore(OutputName, Alias));
                }

                if (Score > BestScore)
                {
                    BestScore = Score;
                    BestProperty = Target.P;
                    BestPropertyName = Target.N;
                }
            }

            // 如果匹配成功
            if (BestScore > 0 && BestProperty != MP_MAX)
            {
                ConnectExpressionToMaterialProperty(Material, FunctionCall, BestProperty, OutputIndex);
                UE_LOG(LogX_AssetEditor, Log, TEXT("输出 %s 自动连接到 %s (匹配度: %d)"), 
                    *OutputName, *BestPropertyName, BestScore);
                bHasConnected = true;
            }
        }
    }
    // 只有输出引脚的情况，可以使用Add/Multiply节点
    else if (FunctionOutputs.Num() > 0)
    {
        // 创建一个查找输出索引的辅助函数
        auto FindOutputIndexByNameFunc = [&FunctionOutputs](const FString& Name) -> int32
        {
            for (int32 i = 0; i < FunctionOutputs.Num(); ++i)
            {
                if (FunctionOutputs[i].Output.OutputName.ToString().Contains(Name))
                {
                    return i;
                }
            }
            return 0; // 默认返回第一个输出的索引
        };
        
        // 从函数名称推断可能的连接目标
        FString FunctionName = FunctionCall->MaterialFunction ? FunctionCall->MaterialFunction->GetName() : TEXT("");
        
        // 使用传入的参数或创建临时参数
        TSharedPtr<FX_MaterialFunctionParams> UsedParams = Params;
        FX_MaterialFunctionParams TempParams;
        
        if (!UsedParams.IsValid())
        {
            // 如果没有传入参数，创建一个临时参数并根据函数名称设置
            TempParams.ConnectionMode = ConnectionMode;
            TempParams.SetupConnectionsByFunctionName(FunctionName);
            UsedParams = MakeShareable(new FX_MaterialFunctionParams(TempParams));
        }
        
        // 优先基于函数名称进行连接
        bool bHasConnectedByName = false;
        
        auto TryConnect = [&](bool bCondition, const FString& PropName, EMaterialProperty Prop)
        {
            if (bCondition && !bHasConnectedByName)
            {
                int32 OutputIdx = FindOutputIndexByNameFunc(PropName);
                if (UsedParams->ConnectionMode == EConnectionMode::Add)
                {
                    CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, Prop);
                    UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Add节点连接到%s"), *PropName);
                }
                else if (UsedParams->ConnectionMode == EConnectionMode::Multiply)
                {
                    CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, Prop);
                    UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Multiply节点连接到%s"), *PropName);
                }
                else
                {
                    ConnectExpressionToMaterialProperty(Material, FunctionCall, Prop, OutputIdx);
                    UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称直接连接到%s"), *PropName);
                }
                bHasConnected = true;
                bHasConnectedByName = true;
            }
        };

        TryConnect(UsedParams->bConnectToEmissive, X_MaterialConstants::EmissiveColor, MP_EmissiveColor);
        TryConnect(UsedParams->bConnectToBaseColor, X_MaterialConstants::BaseColor, MP_BaseColor);
        TryConnect(UsedParams->bConnectToMetallic, X_MaterialConstants::Metallic, MP_Metallic);
        TryConnect(UsedParams->bConnectToRoughness, X_MaterialConstants::Roughness, MP_Roughness);
        TryConnect(UsedParams->bConnectToNormal, X_MaterialConstants::Normal, MP_Normal);
        TryConnect(UsedParams->bConnectToAO, X_MaterialConstants::Alias_AO, MP_AmbientOcclusion);
        
        // 如果没有根据名称自动连接，则根据用户在UI中的选择进行连接
        if (!bHasConnectedByName && FunctionOutputs.Num() > 0)
        {
            int32 OutputIdx = 0; // 默认使用第一个输出
            
            // 基于用户的连接模式和勾选的属性进行连接
            // 用户勾选项的优先级：Emissive > BaseColor > Metallic > Roughness > Normal > AO
            
            // 检查用户可能勾选的属性和连接模式
            if (UsedParams->ConnectionMode == EConnectionMode::Add)
            {
                // 如果用户选择了Add连接模式，默认连接到EmissiveColor
                CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, MP_EmissiveColor);
                UE_LOG(LogX_AssetEditor, Log, TEXT("基于用户Add连接模式设置，创建Add节点连接到EmissiveColor"));
                bHasConnected = true;
            }
            else if (UsedParams->ConnectionMode == EConnectionMode::Multiply)
            {
                // 如果用户选择了Multiply连接模式，默认连接到EmissiveColor
                CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, MP_EmissiveColor);
                UE_LOG(LogX_AssetEditor, Log, TEXT("基于用户Multiply连接模式设置，创建Multiply节点连接到EmissiveColor"));
                bHasConnected = true;
            }
            else
            {
                // 默认连接到BaseColor
                ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_BaseColor, OutputIdx);
                UE_LOG(LogX_AssetEditor, Log, TEXT("将函数的第一个输出连接到BaseColor（默认行为）"));
                bHasConnected = true;
            }
        }
    }
    
    return bHasConnected;
}

UMaterialExpressionAdd* FX_MaterialFunctionConnector::CreateAddConnectionToProperty(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex,
    EMaterialProperty MaterialProperty)
{
    if (!Material || !FunctionCall)
    {
        return nullptr;
    }

    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        return nullptr;
    }

    // 创建Add节点
    UMaterialExpressionAdd* AddExpression = NewObject<UMaterialExpressionAdd>(Material);
    if (!AddExpression)
    {
        return nullptr;
    }

    // 设置Add节点位置
    AddExpression->MaterialExpressionEditorX = FunctionCall->MaterialExpressionEditorX + 200;
    AddExpression->MaterialExpressionEditorY = FunctionCall->MaterialExpressionEditorY;

    // 添加Add节点到材质
    EditorOnlyData->ExpressionCollection.Expressions.Add(AddExpression);

    // 连接函数输出到Add节点的A输入
    AddExpression->A.Connect(OutputIndex, FunctionCall);

    // 根据材质属性获取当前连接，并将其连接到Add节点的B输入
    FExpressionInput* CurrentInput = nullptr;
    switch (MaterialProperty)
    {
        case MP_BaseColor: CurrentInput = &EditorOnlyData->BaseColor; break;
        case MP_Metallic: CurrentInput = &EditorOnlyData->Metallic; break;
        case MP_Specular: CurrentInput = &EditorOnlyData->Specular; break;
        case MP_Roughness: CurrentInput = &EditorOnlyData->Roughness; break;
        case MP_EmissiveColor: CurrentInput = &EditorOnlyData->EmissiveColor; break;
        case MP_Normal: CurrentInput = &EditorOnlyData->Normal; break;
        case MP_AmbientOcclusion: CurrentInput = &EditorOnlyData->AmbientOcclusion; break;
        default: return AddExpression;
    }

    // 如果有现有连接，连接到Add节点的B输入
    if (CurrentInput && CurrentInput->Expression)
    {
        AddExpression->B.Connect(CurrentInput->OutputIndex, CurrentInput->Expression);
    }

    // 连接Add节点到材质属性
    ConnectExpressionToMaterialProperty(Material, AddExpression, MaterialProperty);

    return AddExpression;
}

UMaterialExpressionMultiply* FX_MaterialFunctionConnector::CreateMultiplyConnectionToProperty(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex,
    EMaterialProperty MaterialProperty)
{
    if (!Material || !FunctionCall)
    {
        return nullptr;
    }

    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        return nullptr;
    }

    // 创建Multiply节点
    UMaterialExpressionMultiply* MultiplyExpression = NewObject<UMaterialExpressionMultiply>(Material);
    if (!MultiplyExpression)
    {
        return nullptr;
    }

    // 设置Multiply节点位置
    MultiplyExpression->MaterialExpressionEditorX = FunctionCall->MaterialExpressionEditorX + 200;
    MultiplyExpression->MaterialExpressionEditorY = FunctionCall->MaterialExpressionEditorY;

    // 添加Multiply节点到材质
    EditorOnlyData->ExpressionCollection.Expressions.Add(MultiplyExpression);

    // 连接函数输出到Multiply节点的A输入
    MultiplyExpression->A.Connect(OutputIndex, FunctionCall);

    // 根据材质属性获取当前连接，并将其连接到Multiply节点的B输入
    FExpressionInput* CurrentInput = nullptr;
    switch (MaterialProperty)
    {
        case MP_BaseColor: CurrentInput = &EditorOnlyData->BaseColor; break;
        case MP_Metallic: CurrentInput = &EditorOnlyData->Metallic; break;
        case MP_Specular: CurrentInput = &EditorOnlyData->Specular; break;
        case MP_Roughness: CurrentInput = &EditorOnlyData->Roughness; break;
        case MP_EmissiveColor: CurrentInput = &EditorOnlyData->EmissiveColor; break;
        case MP_Normal: CurrentInput = &EditorOnlyData->Normal; break;
        case MP_AmbientOcclusion: CurrentInput = &EditorOnlyData->AmbientOcclusion; break;
        default: return MultiplyExpression;
    }

    // 如果有现有连接，连接到Multiply节点的B输入
    if (CurrentInput && CurrentInput->Expression)
    {
        MultiplyExpression->B.Connect(CurrentInput->OutputIndex, CurrentInput->Expression);
    }

    // 连接Multiply节点到材质属性
    ConnectExpressionToMaterialProperty(Material, MultiplyExpression, MaterialProperty);

    return MultiplyExpression;
}

bool FX_MaterialFunctionConnector::ProcessManualConnections(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    EConnectionMode ConnectionMode,
    TSharedPtr<FX_MaterialFunctionParams> Params)
{
    if (!Material || !FunctionCall || !Params.IsValid())
    {
        return false;
    }
    
    bool bHasConnected = false;
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData) return false;
    
    // 查找输出索引的辅助函数
    const TArray<FFunctionExpressionOutput>& FunctionOutputs = FunctionCall->FunctionOutputs;
    auto FindOutputIndexByNameFunc = [&FunctionOutputs](const FString& Name) -> int32
    {
        for (int32 i = 0; i < FunctionOutputs.Num(); ++i)
        {
            if (FunctionOutputs[i].Output.OutputName.ToString().Contains(Name))
            {
                return i;
            }
        }
        return 0; // 默认返回第一个输出的索引
    };
    
    // 处理各个手动连接选项
    auto ProcessConnection = [&](bool bConnect, const FString& PropName, EMaterialProperty Prop)
    {
        if (bConnect)
        {
            int32 OutputIdx = FindOutputIndexByNameFunc(PropName);
            
            if (Params->ConnectionMode == EConnectionMode::Add)
            {
                CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, Prop);
            }
            else if (Params->ConnectionMode == EConnectionMode::Multiply)
            {
                CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, Prop);
            }
            else
            {
                ConnectExpressionToMaterialProperty(Material, FunctionCall, Prop, OutputIdx);
            }
            bHasConnected = true;
        }
    };
    
    ProcessConnection(Params->bConnectToEmissive, X_MaterialConstants::EmissiveColor, MP_EmissiveColor);
    ProcessConnection(Params->bConnectToBaseColor, X_MaterialConstants::BaseColor, MP_BaseColor);
    ProcessConnection(Params->bConnectToMetallic, X_MaterialConstants::Metallic, MP_Metallic);
    ProcessConnection(Params->bConnectToRoughness, X_MaterialConstants::Roughness, MP_Roughness);
    ProcessConnection(Params->bConnectToNormal, X_MaterialConstants::Normal, MP_Normal);
    ProcessConnection(Params->bConnectToAO, X_MaterialConstants::Alias_AO, MP_AmbientOcclusion);
    
    return bHasConnected;
}

bool FX_MaterialFunctionConnector::IsMaterialAttributesEnabled(UMaterial* Material)
{
    if (!Material) return false;
    return Material->bUseMaterialAttributes;
}

bool FX_MaterialFunctionConnector::IsFunctionSuitableForAttributes(UMaterialExpressionMaterialFunctionCall* FunctionCall)
{
    if (!FunctionCall) return false;
    
    // 检查函数是否有MaterialAttributes类型的输出
    for (const FFunctionExpressionOutput& Output : FunctionCall->FunctionOutputs)
    {
        // 这里只是简单的名称检查，实际上应该检查类型，但UE API获取类型比较复杂
        // 通常输出MaterialAttributes的函数，其输出引脚名称会包含"Result"或"Attributes"
        if (Output.Output.OutputName.ToString().Contains(TEXT("Attributes")))
        {
            return true;
        }
    }
    
    // 或者检查输入是否接受MaterialAttributes
    for (const FFunctionExpressionInput& Input : FunctionCall->FunctionInputs)
    {
        if (Input.Input.InputName.ToString().Contains(TEXT("Attributes")))
        {
            return true;
        }
    }
    
    return false;
}

bool FX_MaterialFunctionConnector::ConnectMaterialAttributesToMaterial(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex)
{
    if (!Material || !FunctionCall) return false;
    
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData) return false;
    
    // 确保材质启用了UseMaterialAttributes
    if (!Material->bUseMaterialAttributes)
    {
        Material->bUseMaterialAttributes = true;
        Material->MarkPackageDirty();
    }
    
    // 连接到MaterialAttributes引脚
    return ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_MaterialAttributes, OutputIndex);
}