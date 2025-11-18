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

    //  基于UE源码的标准属性名称映射
    EMaterialProperty Property = MP_MAX;
    if (PropertyName == TEXT("BaseColor"))
        Property = MP_BaseColor;
    else if (PropertyName == TEXT("Metallic"))
        Property = MP_Metallic;
    else if (PropertyName == TEXT("Specular"))
        Property = MP_Specular;
    else if (PropertyName == TEXT("Roughness"))
        Property = MP_Roughness;
    else if (PropertyName == TEXT("EmissiveColor"))
        Property = MP_EmissiveColor;
    else if (PropertyName == TEXT("Opacity"))
        Property = MP_Opacity;
    else if (PropertyName == TEXT("OpacityMask"))
        Property = MP_OpacityMask;
    else if (PropertyName == TEXT("Normal"))
        Property = MP_Normal;
    else if (PropertyName == TEXT("WorldPositionOffset"))
        Property = MP_WorldPositionOffset;
    else if (PropertyName == TEXT("SubsurfaceColor"))
        Property = MP_SubsurfaceColor;
    else if (PropertyName == TEXT("AmbientOcclusion"))
        Property = MP_AmbientOcclusion;
    else if (PropertyName == TEXT("Refraction"))
        Property = MP_Refraction;
    else if (PropertyName == TEXT("MaterialAttributes"))
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
    };
    TArray<FPropertyConnection> PropertyConnections;

    // 检查现有的材质属性连接
    if (EditorOnlyData->BaseColor.Expression)
        PropertyConnections.Add({&EditorOnlyData->BaseColor, MP_BaseColor, nullptr});
    if (EditorOnlyData->Metallic.Expression)
        PropertyConnections.Add({&EditorOnlyData->Metallic, MP_Metallic, nullptr});
    if (EditorOnlyData->Specular.Expression)
        PropertyConnections.Add({&EditorOnlyData->Specular, MP_Specular, nullptr});
    if (EditorOnlyData->Roughness.Expression)
        PropertyConnections.Add({&EditorOnlyData->Roughness, MP_Roughness, nullptr});
    if (EditorOnlyData->EmissiveColor.Expression)
        PropertyConnections.Add({&EditorOnlyData->EmissiveColor, MP_EmissiveColor, nullptr});
    if (EditorOnlyData->Normal.Expression)
        PropertyConnections.Add({&EditorOnlyData->Normal, MP_Normal, nullptr});
    if (EditorOnlyData->AmbientOcclusion.Expression)
        PropertyConnections.Add({&EditorOnlyData->AmbientOcclusion, MP_AmbientOcclusion, nullptr});
    
    // 对于每一个输入，尝试找到匹配的现有连接
    for (const FFunctionExpressionInput& FunctionInput : FunctionInputs)
    {
        const FExpressionInput& Input = FunctionInput.Input;
        const FString InputName = Input.InputName.ToString().ToLower();

        // 尝试根据名称匹配
        for (FPropertyConnection& Connection : PropertyConnections)
        {
            if (!Connection.Output) // 只处理还没有匹配输出的连接
            {
                // 检查名称匹配
                FString PropertyName;
                switch (Connection.Property)
                {
                case MP_BaseColor: PropertyName = TEXT("basecolor"); break;
                case MP_Metallic: PropertyName = TEXT("metallic"); break;
                case MP_Specular: PropertyName = TEXT("specular"); break;
                case MP_Roughness: PropertyName = TEXT("roughness"); break;
                case MP_EmissiveColor: PropertyName = TEXT("emissive"); break;
                case MP_Normal: PropertyName = TEXT("normal"); break;
                case MP_AmbientOcclusion: PropertyName = TEXT("ambient"); break;
                default: continue;
                }

                // 如果名称匹配，创建连接
                if (InputName.Contains(PropertyName))
                {
                    // 连接这个材质属性到函数输入
                    FExpressionInput* InputPtr = const_cast<FExpressionInput*>(&Input);
                    if (InputPtr)
                    {
                        InputPtr->Connect(0, Connection.Input->Expression);
                        UE_LOG(LogX_AssetEditor, Log, TEXT("自动连接 %s 到函数输入 %s"),
                            *PropertyName, *Input.InputName.ToString());
                        bHasConnected = true;
                    }
                    break;
                }
            }
        }
    }

    // 对于每一个输出，尝试连接到适当的材质属性
    if (bHasInputsAndOutputs) // 同时有输入和输出的函数，使用直接连接
    {
        for (const FFunctionExpressionOutput& FunctionOutput : FunctionOutputs)
        {
            const FExpressionOutput& Output = FunctionOutput.Output;
            const FString OutputName = Output.OutputName.ToString().ToLower();
            
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

            // 根据输出名称匹配连接目标
            bool bOutputConnected = false;
            
            if (OutputName.Contains(TEXT("basecolor")))
            {
                EditorOnlyData->BaseColor.Connect(OutputIndex, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到BaseColor"));
                bOutputConnected = true;
                bHasConnected = true;
            }
            else if (OutputName.Contains(TEXT("metallic")))
            {
                EditorOnlyData->Metallic.Connect(OutputIndex, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Metallic"));
                bOutputConnected = true;
                bHasConnected = true;
            }
            else if (OutputName.Contains(TEXT("roughness")))
            {
                EditorOnlyData->Roughness.Connect(OutputIndex, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Roughness"));
                bOutputConnected = true;
                bHasConnected = true;
            }
            else if (OutputName.Contains(TEXT("normal")))
            {
                EditorOnlyData->Normal.Connect(OutputIndex, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Normal"));
                bOutputConnected = true;
                bHasConnected = true;
            }
            else if (OutputName.Contains(TEXT("emissive")))
            {
                EditorOnlyData->EmissiveColor.Connect(OutputIndex, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到EmissiveColor"));
                bOutputConnected = true;
                bHasConnected = true;
            }
            else if (OutputName.Contains(TEXT("ambient")) || OutputName.Contains(TEXT("ao")))
            {
                EditorOnlyData->AmbientOcclusion.Connect(OutputIndex, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到AmbientOcclusion"));
                bOutputConnected = true;
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
        
        if (UsedParams->bConnectToEmissive)
        {
            int32 OutputIdx = FindOutputIndexByNameFunc(TEXT("Emissive"));
            
            // 根据连接模式处理
            if (UsedParams->ConnectionMode == EConnectionMode::Add)
            {
                // 创建Add节点连接到EmissiveColor
                CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, MP_EmissiveColor);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Add节点连接到EmissiveColor"));
            }
            else if (UsedParams->ConnectionMode == EConnectionMode::Multiply)
            {
                // 创建Multiply节点连接到EmissiveColor
                CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, MP_EmissiveColor);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Multiply节点连接到EmissiveColor"));
            }
            else
            {
                // 直接连接
                EditorOnlyData->EmissiveColor.Connect(OutputIdx, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称直接连接到EmissiveColor"));
            }
            
            bHasConnected = true;
            bHasConnectedByName = true;
        }
        else if (UsedParams->bConnectToBaseColor)
        {
            int32 OutputIdx = FindOutputIndexByNameFunc(TEXT("BaseColor"));
            
            // 根据连接模式处理
            if (UsedParams->ConnectionMode == EConnectionMode::Add)
            {
                // 创建Add节点连接到BaseColor
                CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, MP_BaseColor);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Add节点连接到BaseColor"));
            }
            else if (UsedParams->ConnectionMode == EConnectionMode::Multiply)
            {
                // 创建Multiply节点连接到BaseColor
                CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, MP_BaseColor);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Multiply节点连接到BaseColor"));
            }
            else
            {
                // 直接连接
                EditorOnlyData->BaseColor.Connect(OutputIdx, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称直接连接到BaseColor"));
            }
            
            bHasConnected = true;
            bHasConnectedByName = true;
        }
        else if (UsedParams->bConnectToMetallic)
        {
            int32 OutputIdx = FindOutputIndexByNameFunc(TEXT("Metallic"));
            
            // 根据连接模式处理
            if (UsedParams->ConnectionMode == EConnectionMode::Add)
            {
                // 创建Add节点连接到Metallic
                CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, MP_Metallic);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Add节点连接到Metallic"));
            }
            else if (UsedParams->ConnectionMode == EConnectionMode::Multiply)
            {
                // 创建Multiply节点连接到Metallic
                CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, MP_Metallic);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Multiply节点连接到Metallic"));
            }
            else
            {
                // 直接连接
                EditorOnlyData->Metallic.Connect(OutputIdx, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称直接连接到Metallic"));
            }
            
            bHasConnected = true;
            bHasConnectedByName = true;
        }
        else if (UsedParams->bConnectToRoughness)
        {
            int32 OutputIdx = FindOutputIndexByNameFunc(TEXT("Roughness"));
            
            // 根据连接模式处理
            if (UsedParams->ConnectionMode == EConnectionMode::Add)
            {
                // 创建Add节点连接到Roughness
                CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, MP_Roughness);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Add节点连接到Roughness"));
            }
            else if (UsedParams->ConnectionMode == EConnectionMode::Multiply)
            {
                // 创建Multiply节点连接到Roughness
                CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, MP_Roughness);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Multiply节点连接到Roughness"));
            }
            else
            {
                // 直接连接
                EditorOnlyData->Roughness.Connect(OutputIdx, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称直接连接到Roughness"));
            }
            
            bHasConnected = true;
            bHasConnectedByName = true;
        }
        else if (UsedParams->bConnectToNormal)
        {
            int32 OutputIdx = FindOutputIndexByNameFunc(TEXT("Normal"));
            
            // 根据连接模式处理
            if (UsedParams->ConnectionMode == EConnectionMode::Add)
            {
                // 创建Add节点连接到Normal
                CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, MP_Normal);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Add节点连接到Normal"));
            }
            else if (UsedParams->ConnectionMode == EConnectionMode::Multiply)
            {
                // 创建Multiply节点连接到Normal
                CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, MP_Normal);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Multiply节点连接到Normal"));
            }
            else
            {
                // 直接连接
                EditorOnlyData->Normal.Connect(OutputIdx, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称直接连接到Normal"));
            }
            
            bHasConnected = true;
            bHasConnectedByName = true;
        }
        else if (UsedParams->bConnectToAO)
        {
            int32 OutputIdx = FindOutputIndexByNameFunc(TEXT("AO"));
            
            // 根据连接模式处理
            if (UsedParams->ConnectionMode == EConnectionMode::Add)
            {
                // 创建Add节点连接到AO
                CreateAddConnectionToProperty(Material, FunctionCall, OutputIdx, MP_AmbientOcclusion);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Add节点连接到AmbientOcclusion"));
            }
            else if (UsedParams->ConnectionMode == EConnectionMode::Multiply)
            {
                // 创建Multiply节点连接到AO
                CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIdx, MP_AmbientOcclusion);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称使用Multiply节点连接到AmbientOcclusion"));
            }
            else
            {
                // 直接连接
                EditorOnlyData->AmbientOcclusion.Connect(OutputIdx, FunctionCall);
                UE_LOG(LogX_AssetEditor, Log, TEXT("根据函数名称直接连接到AmbientOcclusion"));
            }
            
            bHasConnected = true;
            bHasConnectedByName = true;
        }
        
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
                EditorOnlyData->BaseColor.Connect(OutputIdx, FunctionCall);
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
        case MP_BaseColor:
            CurrentInput = &EditorOnlyData->BaseColor;
            break;
        case MP_Metallic:
            CurrentInput = &EditorOnlyData->Metallic;
            break;
        case MP_Specular:
            CurrentInput = &EditorOnlyData->Specular;
            break;
        case MP_Roughness:
            CurrentInput = &EditorOnlyData->Roughness;
            break;
        case MP_EmissiveColor:
            CurrentInput = &EditorOnlyData->EmissiveColor;
            break;
        case MP_Normal:
            CurrentInput = &EditorOnlyData->Normal;
            break;
        case MP_AmbientOcclusion:
            CurrentInput = &EditorOnlyData->AmbientOcclusion;
            break;
        default:
            // 不支持的属性类型
            return AddExpression;
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
        case MP_BaseColor:
            CurrentInput = &EditorOnlyData->BaseColor;
            break;
        case MP_Metallic:
            CurrentInput = &EditorOnlyData->Metallic;
            break;
        case MP_Specular:
            CurrentInput = &EditorOnlyData->Specular;
            break;
        case MP_Roughness:
            CurrentInput = &EditorOnlyData->Roughness;
            break;
        case MP_EmissiveColor:
            CurrentInput = &EditorOnlyData->EmissiveColor;
            break;
        case MP_Normal:
            CurrentInput = &EditorOnlyData->Normal;
            break;
        case MP_AmbientOcclusion:
            CurrentInput = &EditorOnlyData->AmbientOcclusion;
            break;
        default:
            // 不支持的属性类型
            return MultiplyExpression;
    }

    // 如果有现有连接，连接到Multiply节点的B输入
    if (CurrentInput && CurrentInput->Expression)
    {
        MultiplyExpression->B.Connect(CurrentInput->OutputIndex, CurrentInput->Expression);
    }
    else
    {
        // 如果没有现有连接，设置默认乘数为1
        // 注意：UE5.3源码显示ConstA和ConstB都是float类型
        MultiplyExpression->ConstB = 1.0f;
        
        // 对于颜色属性，也设置ConstA为1.0以确保不会影响颜色
        if (MaterialProperty == MP_BaseColor || MaterialProperty == MP_EmissiveColor)
        {
            MultiplyExpression->ConstA = 1.0f;
        }
    }

    // 连接Multiply节点到材质属性
    ConnectExpressionToMaterialProperty(Material, MultiplyExpression, MaterialProperty);

    return MultiplyExpression;
}

//  新增：MaterialAttributes相关功能

bool FX_MaterialFunctionConnector::IsUsingMaterialAttributes(UMaterialExpressionMaterialFunctionCall* FunctionCall)
{
    if (!FunctionCall)
    {
        return false;
    }
    
    #if WITH_EDITOR
    // 优化：使用UE官方API进行判断，更准确可靠
    // IsResultMaterialAttributes是UMaterialExpression的虚方法
    // 各种特殊表达式（如SetMaterialAttributes）会覆盖此方法返回true
    bool bIsResultMaterialAttributes = FunctionCall->IsResultMaterialAttributes(0);
    
    if (bIsResultMaterialAttributes)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("通过UE官方API检测到MaterialAttributes输出"));
        return true;
    }
    
    // 备用检测：通过函数名称进行推断（处理某些边界情况）
    if (FunctionCall->MaterialFunction)
    {
        FString FunctionName = FunctionCall->MaterialFunction->GetName();
        if (FunctionName.Contains(TEXT("MaterialAttributes")) ||
            FunctionName.Contains(TEXT("MA_")) ||
            FunctionName.Contains(TEXT("MakeMA")) ||
            FunctionName.Contains(TEXT("SetMA")) ||
            FunctionName.Contains(TEXT("BlendMA")) ||
            FunctionName.Contains(TEXT("SM_")) ||
            FunctionName.Contains(TEXT("MF_SM_")) ||
            FunctionName.Contains(TEXT("SurfaceMaterial")) ||
            FunctionName.Contains(TEXT("LayerMaterial")))
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("通过函数名称推断MaterialAttributes模式: %s"), *FunctionName);
            return true;
        }
    }
    #endif
    
    return false;
}

bool FX_MaterialFunctionConnector::ProcessManualConnections(UMaterial* Material,
                                                            UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                            EConnectionMode ConnectionMode,
                                                            TSharedPtr<FX_MaterialFunctionParams> Params)
{
    if (!Material || !FunctionCall || !Params.IsValid())
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("参数无效，无法处理手动连接"));
        return false;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("处理用户手动配置连接"));
    
    bool bAnyConnected = false;
    
    //  检查用户是否强制指定了MaterialAttributes模式
    if (Params->bUseMaterialAttributes)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("用户手动指定使用MaterialAttributes连接"));
        return ConnectMaterialAttributesToMaterial(Material, FunctionCall, 0);
    }
    
    //  关键修复：即使在手动模式下，也要检查材质是否启用了MaterialAttributes
    bool bMaterialUsesAttributes = IsMaterialAttributesEnabled(Material);
    if (bMaterialUsesAttributes)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：检测到材质启用MaterialAttributes，使用MaterialAttributes连接方式"));
        return ProcessManualMaterialAttributesConnections(Material, FunctionCall, Params);
    }
    
    //  处理常规的手动连接配置（材质未启用MaterialAttributes）
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法获取材质编辑器数据"));
        return false;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：使用常规连接方式"));
    
    //  根据用户勾选的属性进行连接
    if (Params->bConnectToBaseColor)
    {
        bool bSuccess = ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_BaseColor, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动连接到BaseColor成功"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToMetallic)
    {
        bool bSuccess = ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_Metallic, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动连接到Metallic成功"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToRoughness)
    {
        bool bSuccess = ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_Roughness, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动连接到Roughness成功"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToNormal)
    {
        bool bSuccess = ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_Normal, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动连接到Normal成功"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToEmissive)
    {
        bool bSuccess = ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_EmissiveColor, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动连接到EmissiveColor成功"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToAO)
    {
        bool bSuccess = ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_AmbientOcclusion, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动连接到AmbientOcclusion成功"));
            bAnyConnected = true;
        }
    }
    
    if (bAnyConnected)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("手动连接完成，成功连接了 %d 个属性"), 
            (Params->bConnectToBaseColor ? 1 : 0) + 
            (Params->bConnectToMetallic ? 1 : 0) + 
            (Params->bConnectToRoughness ? 1 : 0) + 
            (Params->bConnectToNormal ? 1 : 0) + 
            (Params->bConnectToEmissive ? 1 : 0) + 
            (Params->bConnectToAO ? 1 : 0));
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("用户没有选择任何连接选项，或所有连接都失败"));
    }
    
    return bAnyConnected;
}

bool FX_MaterialFunctionConnector::ProcessManualMaterialAttributesConnections(UMaterial* Material,
                                                                              UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                              TSharedPtr<FX_MaterialFunctionParams> Params)
{
    if (!Material || !FunctionCall || !Params.IsValid())
    {
        return false;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：处理MaterialAttributes连接"));
    
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        return false;
    }
    
    bool bAnyConnected = false;
    
    //  检查MaterialAttributes引脚是否已有连接（通常连接到MakeMaterialAttributes）
    if (EditorOnlyData->MaterialAttributes.IsConnected())
    {
        UMaterialExpression* ExistingExpression = EditorOnlyData->MaterialAttributes.Expression;
        if (ExistingExpression)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：找到MaterialAttributes表达式: %s"), 
                *ExistingExpression->GetClass()->GetName());
            
            //  如果是MakeMaterialAttributes节点，手动连接到特定输入
            if (ExistingExpression->GetClass()->GetName().Contains(TEXT("MakeMaterialAttributes")))
            {
                return ProcessManualConnectionsToMakeMaterialAttributes(ExistingExpression, FunctionCall, Params);
            }
        }
    }
    
    //  没有现有MaterialAttributes表达式，直接连接到材质主节点的MaterialAttributes
    UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：直接连接到材质主节点的MaterialAttributes"));
    
    // 在手动模式下，我们需要根据用户选择的输出来连接
    // 这里我们假设函数有合适的输出（在智能模式下会自动匹配）
    bool bSuccess = ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_MaterialAttributes, 0);
    if (bSuccess)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：成功连接到MaterialAttributes主节点"));
        bAnyConnected = true;
    }
    
    return bAnyConnected;
}

bool FX_MaterialFunctionConnector::ProcessManualConnectionsToMakeMaterialAttributes(UMaterialExpression* MakeMAExpression,
                                                                                   UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                                   TSharedPtr<FX_MaterialFunctionParams> Params)
{
    if (!MakeMAExpression || !FunctionCall || !Params.IsValid())
    {
        return false;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：连接到MakeMaterialAttributes节点的特定输入"));
    
    bool bAnyConnected = false;
    
    //  根据用户手动选择的属性，连接到MakeMaterialAttributes的对应输入
    if (Params->bConnectToBaseColor)
    {
        bool bSuccess = ConnectToMakeMaterialAttributesDirect(MakeMAExpression, FunctionCall, MP_BaseColor, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：成功连接到MakeMaterialAttributes.BaseColor"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToMetallic)
    {
        bool bSuccess = ConnectToMakeMaterialAttributesDirect(MakeMAExpression, FunctionCall, MP_Metallic, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：成功连接到MakeMaterialAttributes.Metallic"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToRoughness)
    {
        bool bSuccess = ConnectToMakeMaterialAttributesDirect(MakeMAExpression, FunctionCall, MP_Roughness, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：成功连接到MakeMaterialAttributes.Roughness"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToNormal)
    {
        bool bSuccess = ConnectToMakeMaterialAttributesDirect(MakeMAExpression, FunctionCall, MP_Normal, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：成功连接到MakeMaterialAttributes.Normal"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToEmissive)
    {
        bool bSuccess = ConnectToMakeMaterialAttributesDirect(MakeMAExpression, FunctionCall, MP_EmissiveColor, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：成功连接到MakeMaterialAttributes.EmissiveColor"));
            bAnyConnected = true;
        }
    }
    
    if (Params->bConnectToAO)
    {
        bool bSuccess = ConnectToMakeMaterialAttributesDirect(MakeMAExpression, FunctionCall, MP_AmbientOcclusion, 0);
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：成功连接到MakeMaterialAttributes.AmbientOcclusion"));
            bAnyConnected = true;
        }
    }
    
    if (bAnyConnected)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("手动模式：MakeMaterialAttributes连接完成"));
    }
    
    return bAnyConnected;
}

bool FX_MaterialFunctionConnector::IsMaterialAttributesEnabled(UMaterial* Material)
{
    if (!Material)
    {
        return false;
    }
    
    //  核心逻辑：检查材质是否启用了"使用材质属性"设置
    // 在UE中，这通常通过检查材质的bUseMaterialAttributes属性
    // 或者通过检查MaterialAttributes输入是否被暴露/可用
    
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        return false;
    }
    
    // 方法1: 检查MaterialAttributes输入是否已有连接（明确启用的标志）
    if (EditorOnlyData->MaterialAttributes.IsConnected())
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("材质的MaterialAttributes引脚已有连接，确认启用MaterialAttributes"));
        return true;
    }
    
    // 方法2: 检查常规引脚是否全部未连接（可能启用了MaterialAttributes但未连接）
    bool bHasAnyRegularConnection = EditorOnlyData->BaseColor.IsConnected() ||
                                    EditorOnlyData->Metallic.IsConnected() ||
                                    EditorOnlyData->Specular.IsConnected() ||
                                    EditorOnlyData->Roughness.IsConnected() ||
                                    EditorOnlyData->EmissiveColor.IsConnected() ||
                                    EditorOnlyData->Normal.IsConnected() ||
                                    EditorOnlyData->Opacity.IsConnected() ||
                                    EditorOnlyData->OpacityMask.IsConnected() ||
                                    EditorOnlyData->AmbientOcclusion.IsConnected();
    
    // 如果没有任何常规连接，可能是启用了MaterialAttributes但还没连接
    if (!bHasAnyRegularConnection)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("材质没有常规属性连接，推测启用了MaterialAttributes模式"));
        return true;
    }
    
    // 默认情况：有常规连接，说明使用传统模式
    UE_LOG(LogX_AssetEditor, Log, TEXT("材质有常规属性连接，使用传统连接模式"));
    return false;
}

bool FX_MaterialFunctionConnector::IsFunctionSuitableForAttributes(UMaterialExpressionMaterialFunctionCall* FunctionCall)
{
    if (!FunctionCall || !FunctionCall->MaterialFunction)
    {
        return false;
    }
    
    //  检查函数是否适合MaterialAttributes连接
    // 通过函数名称判断
    FString FunctionName = FunctionCall->MaterialFunction->GetName();
    bool bIsSuitableFunction = FunctionName.Contains(TEXT("MaterialAttributes")) ||
                               FunctionName.Contains(TEXT("MA_")) ||
                               FunctionName.Contains(TEXT("MakeMA")) ||
                               FunctionName.Contains(TEXT("SetMA")) ||
                               FunctionName.Contains(TEXT("BlendMA")) ||
                               FunctionName.Contains(TEXT("SM_")) ||
                               FunctionName.Contains(TEXT("MF_SM_")) ||
                               FunctionName.Contains(TEXT("Fresnel")) ||
                               FunctionName.Contains(TEXT("SurfaceMaterial")) ||
                               FunctionName.Contains(TEXT("ComplexMaterial")) ||
                               FunctionName.Contains(TEXT("LayerMaterial"));
    
    if (bIsSuitableFunction)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("函数 %s 适合MaterialAttributes连接"), *FunctionName);
        return true;
    }
    
    // 检查函数输出
    const TArray<FFunctionExpressionOutput>& FunctionOutputs = FunctionCall->FunctionOutputs;
    for (const FFunctionExpressionOutput& Output : FunctionOutputs)
    {
        FString OutputName = Output.Output.OutputName.ToString();
        if (OutputName.IsEmpty() || 
            OutputName.Contains(TEXT("MaterialAttributes")) ||
            OutputName.Contains(TEXT("Material Attributes")))
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("通过输出引脚检测函数适合MaterialAttributes连接"));
            return true;
        }
    }
    
    return false;
}

bool FX_MaterialFunctionConnector::ConnectMaterialAttributesToMaterial(UMaterial* Material, 
                                                                       UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                       int32 OutputIndex)
{
    if (!Material || !FunctionCall)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质或函数调用为空"));
        return false;
    }
    
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法获取材质编辑器数据"));
        return false;
    }
    
    //  Step 1: 处理输入引脚的自动连接（与原有智能连接逻辑一致）
    bool bInputConnected = ProcessMaterialAttributesInputConnections(Material, FunctionCall);
    
    //  Step 2: 处理输出引脚的连接
    bool bOutputConnected = false;
    
    // 检查MaterialAttributes引脚是否已有连接
    if (EditorOnlyData->MaterialAttributes.IsConnected())
    {
        // 已有连接，找到连接的源表达式（如MakeMaterialAttributes）
        UMaterialExpression* ExistingExpression = EditorOnlyData->MaterialAttributes.Expression;
        if (ExistingExpression)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("检测到MaterialAttributes已连接到表达式: %s"), 
                *ExistingExpression->GetClass()->GetName());
            
            //  智能连接到已有的MaterialAttributes表达式
            bOutputConnected = ConnectToMaterialAttributesExpression(ExistingExpression, FunctionCall, OutputIndex);
        }
    }
    else
    {
        //  没有现有连接，直接连接到材质主节点
        UE_LOG(LogX_AssetEditor, Log, TEXT("MaterialAttributes引脚未连接，直接连接到材质主节点"));
        
        // 优先使用UE官方API
        bool bSuccess = UMaterialEditingLibrary::ConnectMaterialProperty(
            FunctionCall, 
            OutputIndex == 0 ? FString() : FString::Printf(TEXT("%d"), OutputIndex),
            MP_MaterialAttributes
        );
        
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("成功使用官方API连接MaterialAttributes到材质主节点"));
        }
        else
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("使用官方API连接失败，尝试直接连接"));
            EditorOnlyData->MaterialAttributes.Connect(OutputIndex, FunctionCall);
            bSuccess = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("通过直接连接成功连接MaterialAttributes"));
        }
        
        bOutputConnected = bSuccess;
    }
    
    //  最终结果：输入或输出有任何连接就算成功
    bool bAnyConnected = bInputConnected || bOutputConnected;
    
    if (bAnyConnected)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("MaterialAttributes连接完成 - 输入连接: %s, 输出连接: %s"), 
            bInputConnected ? TEXT("成功") : TEXT("无"), 
            bOutputConnected ? TEXT("成功") : TEXT("无"));
        Material->MarkPackageDirty();
        Material->PostEditChange();
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("MaterialAttributes连接完全失败"));
    }
    
    return bAnyConnected;
}

bool FX_MaterialFunctionConnector::ProcessMaterialAttributesInputConnections(UMaterial* Material,
                                                                             UMaterialExpressionMaterialFunctionCall* FunctionCall)
{
    if (!Material || !FunctionCall)
    {
        return false;
    }
    
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        return false;
    }
    
    // 获取函数的输入引脚
    const TArray<FFunctionExpressionInput>& FunctionInputs = FunctionCall->FunctionInputs;
    
    if (FunctionInputs.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("函数没有输入引脚，跳过输入连接处理"));
        return false;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("MaterialAttributes模式：处理 %d 个输入引脚的自动连接"), FunctionInputs.Num());
    
    //  收集可用的属性连接源（优先从MakeMaterialAttributes获取，备用从材质主引脚获取）
    struct FAvailableConnection
    {
        UMaterialExpression* Expression;
        int32 OutputIndex;
        EMaterialProperty Property;
        FString PropertyName;
    };
    TArray<FAvailableConnection> AvailableConnections;
    
    //  首先尝试从MaterialAttributes连接的节点获取连接
    if (EditorOnlyData->MaterialAttributes.IsConnected())
    {
        UMaterialExpression* MAExpression = EditorOnlyData->MaterialAttributes.Expression;
        
        // 情况1：MaterialAttributes连接到MakeMaterialAttributes节点
        if (UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(MAExpression))
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("从MakeMaterialAttributes节点收集可用连接"));
            
            // 从MakeMaterialAttributes节点的输入引脚收集连接
            if (MakeMANode->BaseColor.IsConnected())
                AvailableConnections.Add({MakeMANode->BaseColor.Expression, MakeMANode->BaseColor.OutputIndex, MP_BaseColor, TEXT("basecolor")});
            if (MakeMANode->EmissiveColor.IsConnected())
                AvailableConnections.Add({MakeMANode->EmissiveColor.Expression, MakeMANode->EmissiveColor.OutputIndex, MP_EmissiveColor, TEXT("emissive")});
            if (MakeMANode->Metallic.IsConnected())
                AvailableConnections.Add({MakeMANode->Metallic.Expression, MakeMANode->Metallic.OutputIndex, MP_Metallic, TEXT("metallic")});
            if (MakeMANode->Roughness.IsConnected())
                AvailableConnections.Add({MakeMANode->Roughness.Expression, MakeMANode->Roughness.OutputIndex, MP_Roughness, TEXT("roughness")});
            if (MakeMANode->Normal.IsConnected())
                AvailableConnections.Add({MakeMANode->Normal.Expression, MakeMANode->Normal.OutputIndex, MP_Normal, TEXT("normal")});
            if (MakeMANode->Specular.IsConnected())
                AvailableConnections.Add({MakeMANode->Specular.Expression, MakeMANode->Specular.OutputIndex, MP_Specular, TEXT("specular")});
            if (MakeMANode->AmbientOcclusion.IsConnected())
                AvailableConnections.Add({MakeMANode->AmbientOcclusion.Expression, MakeMANode->AmbientOcclusion.OutputIndex, MP_AmbientOcclusion, TEXT("ambient")});
        }
        // 情况2：MaterialAttributes连接到MaterialFunctionCall（如MF_VT_Mat）
        else if (UMaterialExpressionMaterialFunctionCall* FunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(MAExpression))
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("检测到MaterialFunctionCall节点，回溯查找MakeMaterialAttributes节点"));
            
            // 回溯查找第一个同时拥有BaseColor和EmissiveColor输入的节点
            UMaterialExpression* TargetNode = FindFirstNodeWithBaseAndEmissiveInputs(MAExpression);
            if (TargetNode)
            {
                if (UMaterialExpressionMakeMaterialAttributes* FoundMakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(TargetNode))
                {
                    UE_LOG(LogX_AssetEditor, Log, TEXT("回溯找到MakeMaterialAttributes节点，从该节点收集连接"));
                    
                    // 从MakeMaterialAttributes节点的输入引脚收集连接
                    if (FoundMakeMANode->BaseColor.IsConnected())
                        AvailableConnections.Add({FoundMakeMANode->BaseColor.Expression, FoundMakeMANode->BaseColor.OutputIndex, MP_BaseColor, TEXT("basecolor")});
                    if (FoundMakeMANode->EmissiveColor.IsConnected())
                        AvailableConnections.Add({FoundMakeMANode->EmissiveColor.Expression, FoundMakeMANode->EmissiveColor.OutputIndex, MP_EmissiveColor, TEXT("emissive")});
                    if (FoundMakeMANode->Metallic.IsConnected())
                        AvailableConnections.Add({FoundMakeMANode->Metallic.Expression, FoundMakeMANode->Metallic.OutputIndex, MP_Metallic, TEXT("metallic")});
                    if (FoundMakeMANode->Roughness.IsConnected())
                        AvailableConnections.Add({FoundMakeMANode->Roughness.Expression, FoundMakeMANode->Roughness.OutputIndex, MP_Roughness, TEXT("roughness")});
                    if (FoundMakeMANode->Normal.IsConnected())
                        AvailableConnections.Add({FoundMakeMANode->Normal.Expression, FoundMakeMANode->Normal.OutputIndex, MP_Normal, TEXT("normal")});
                    if (FoundMakeMANode->Specular.IsConnected())
                        AvailableConnections.Add({FoundMakeMANode->Specular.Expression, FoundMakeMANode->Specular.OutputIndex, MP_Specular, TEXT("specular")});
                    if (FoundMakeMANode->AmbientOcclusion.IsConnected())
                        AvailableConnections.Add({FoundMakeMANode->AmbientOcclusion.Expression, FoundMakeMANode->AmbientOcclusion.OutputIndex, MP_AmbientOcclusion, TEXT("ambient")});
                }
            }
            else
            {
                UE_LOG(LogX_AssetEditor, Warning, TEXT("回溯未找到MakeMaterialAttributes节点"));
            }
        }
    }
    
    //  如果没有从MakeMaterialAttributes获取到连接，尝试从材质主引脚获取
    if (AvailableConnections.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("从材质主引脚收集可用连接"));
        
        if (EditorOnlyData->BaseColor.IsConnected())
            AvailableConnections.Add({EditorOnlyData->BaseColor.Expression, EditorOnlyData->BaseColor.OutputIndex, MP_BaseColor, TEXT("basecolor")});
        if (EditorOnlyData->EmissiveColor.IsConnected())
            AvailableConnections.Add({EditorOnlyData->EmissiveColor.Expression, EditorOnlyData->EmissiveColor.OutputIndex, MP_EmissiveColor, TEXT("emissive")});
        if (EditorOnlyData->Metallic.IsConnected())
            AvailableConnections.Add({EditorOnlyData->Metallic.Expression, EditorOnlyData->Metallic.OutputIndex, MP_Metallic, TEXT("metallic")});
        if (EditorOnlyData->Roughness.IsConnected())
            AvailableConnections.Add({EditorOnlyData->Roughness.Expression, EditorOnlyData->Roughness.OutputIndex, MP_Roughness, TEXT("roughness")});
        if (EditorOnlyData->Normal.IsConnected())
            AvailableConnections.Add({EditorOnlyData->Normal.Expression, EditorOnlyData->Normal.OutputIndex, MP_Normal, TEXT("normal")});
        if (EditorOnlyData->Specular.IsConnected())
            AvailableConnections.Add({EditorOnlyData->Specular.Expression, EditorOnlyData->Specular.OutputIndex, MP_Specular, TEXT("specular")});
        if (EditorOnlyData->AmbientOcclusion.IsConnected())
            AvailableConnections.Add({EditorOnlyData->AmbientOcclusion.Expression, EditorOnlyData->AmbientOcclusion.OutputIndex, MP_AmbientOcclusion, TEXT("ambient")});
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("收集到 %d 个可用属性连接"), AvailableConnections.Num());
    
    if (AvailableConnections.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("没有可用的属性连接，跳过输入连接处理"));
        return false;
    }
    
    //  尝试将可用连接匹配到函数的输入引脚
    bool bAnyInputConnected = false;
    
    //  通用方案：检查函数是否同时有输入和输出引脚
    const TArray<FFunctionExpressionOutput>& FunctionOutputs = FunctionCall->FunctionOutputs;
    bool bHasInputsAndOutputs = (FunctionInputs.Num() >= 2 && FunctionOutputs.Num() > 0);
    
    if (bHasInputsAndOutputs)
    {
        //  对于有输入和输出的函数（如菲涅尔），使用插入模式：
        //  将函数插入到BaseColor和EmissiveColor的连接中作为中间节点
        UE_LOG(LogX_AssetEditor, Log, TEXT("检测到有输入输出的函数，使用插入模式连接逻辑"));
        
        //  查找BaseColor和EmissiveColor的源连接
        const FAvailableConnection* BaseColorConn = nullptr;
        const FAvailableConnection* EmissiveConn = nullptr;
        
        for (const FAvailableConnection& Connection : AvailableConnections)
        {
            if (Connection.Property == MP_BaseColor)
            {
                BaseColorConn = &Connection;
            }
            else if (Connection.Property == MP_EmissiveColor)
            {
                EmissiveConn = &Connection;
            }
        }
        
        //  连接第一个输入到BaseColor源（插入到BaseColor连接中）
        if (BaseColorConn && FunctionInputs.Num() > 0)
        {
            const FExpressionInput& Input = FunctionInputs[0].Input;
            FExpressionInput* InputPtr = const_cast<FExpressionInput*>(&Input);
            if (InputPtr && BaseColorConn->Expression)
            {
                InputPtr->Connect(BaseColorConn->OutputIndex, BaseColorConn->Expression);
                UE_LOG(LogX_AssetEditor, Log, TEXT("插入模式：连接第一个输入 '%s' 到 BaseColor源节点"),
                    *Input.InputName.ToString());
                bAnyInputConnected = true;
            }
        }
        
        //  连接第二个输入到EmissiveColor源（插入到EmissiveColor连接中）
        if (EmissiveConn && FunctionInputs.Num() > 1)
        {
            const FExpressionInput& Input = FunctionInputs[1].Input;
            FExpressionInput* InputPtr = const_cast<FExpressionInput*>(&Input);
            if (InputPtr && EmissiveConn->Expression)
            {
                InputPtr->Connect(EmissiveConn->OutputIndex, EmissiveConn->Expression);
                UE_LOG(LogX_AssetEditor, Log, TEXT("插入模式：连接第二个输入 '%s' 到 EmissiveColor源节点"),
                    *Input.InputName.ToString());
                bAnyInputConnected = true;
            }
        }
    }
    else
    {
        //  普通函数（只有输入或只有输出）：使用名称匹配逻辑
        for (const FFunctionExpressionInput& FunctionInput : FunctionInputs)
        {
            const FExpressionInput& Input = FunctionInput.Input;
            const FString InputName = Input.InputName.ToString().ToLower();
            
            UE_LOG(LogX_AssetEditor, Log, TEXT("处理函数输入引脚: %s"), *Input.InputName.ToString());
            
            // 尝试根据名称匹配
            for (const FAvailableConnection& Connection : AvailableConnections)
            {
                if (InputName.Contains(Connection.PropertyName))
                {
                    // 连接这个属性到函数输入
                    FExpressionInput* InputPtr = const_cast<FExpressionInput*>(&Input);
                    if (InputPtr && Connection.Expression)
                    {
                        InputPtr->Connect(Connection.OutputIndex, Connection.Expression);
                        UE_LOG(LogX_AssetEditor, Log, TEXT("MaterialAttributes模式：自动连接 %s 到函数输入 %s"),
                            *Connection.PropertyName, *Input.InputName.ToString());
                        bAnyInputConnected = true;
                    }
                    break; // 找到匹配就停止
                }
            }
        }
    }
    
    if (bAnyInputConnected)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("MaterialAttributes模式：输入引脚自动连接完成"));
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("MaterialAttributes模式：没有找到匹配的输入引脚连接"));
    }
    
    return bAnyInputConnected;
}

bool FX_MaterialFunctionConnector::ConnectToMaterialAttributesExpression(UMaterialExpression* MaterialAttributesExpression,
                                                                         UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                         int32 OutputIndex)
{
    if (!MaterialAttributesExpression || !FunctionCall)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("MaterialAttributes表达式或函数调用为空"));
        return false;
    }
    
    FString ExpressionClassName = MaterialAttributesExpression->GetClass()->GetName();
    FString FunctionName = FunctionCall->MaterialFunction ? FunctionCall->MaterialFunction->GetName() : TEXT("Unknown");
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("尝试将函数 %s 连接到 MaterialAttributes表达式 %s"), 
        *FunctionName, *ExpressionClassName);
    
    //  检查是否是MakeMaterialAttributes表达式
    if (ExpressionClassName.Contains(TEXT("MakeMaterialAttributes")))
    {
        return ConnectToMakeMaterialAttributesNode(MaterialAttributesExpression, FunctionCall, OutputIndex);
    }
    
    //  检查是否是MaterialFunctionCall（可能是另一个MaterialAttributes函数）
    if (UMaterialExpressionMaterialFunctionCall* ExistingFunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(MaterialAttributesExpression))
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("检测到现有MaterialAttributes函数，尝试连接到其输入"));

        const bool bConnected = ConnectToMaterialAttributesFunctionInputs(ExistingFunctionCall, FunctionCall, OutputIndex);
        if (bConnected)
        {
            return true;
        }

        //  智能连接失败时的保底策略：从材质的MaterialAttributes链路向前回溯，
        //  查找第一个同时拥有BaseColor和Emissive输入的节点并连接。
        UE_LOG(LogX_AssetEditor, Warning, TEXT("连接到MaterialAttributes函数输入失败，尝试回溯到BaseColor+Emissive节点作为保底方案"));

        if (UMaterial* OwningMaterial = MaterialAttributesExpression->GetTypedOuter<UMaterial>())
        {
            return FallbackConnectToFirstBaseEmissiveNode(OwningMaterial, FunctionCall, OutputIndex);
        }

        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法从MaterialAttributes表达式获取所属材质对象，保底回溯逻辑中止"));
        return false;
    }
    
    //  其他MaterialAttributes表达式类型
    UE_LOG(LogX_AssetEditor, Warning, TEXT("未识别的MaterialAttributes表达式类型: %s，尝试通用连接"), *ExpressionClassName);
    return ConnectToGenericMaterialAttributesExpression(MaterialAttributesExpression, FunctionCall, OutputIndex);
}

bool FX_MaterialFunctionConnector::ConnectToMakeMaterialAttributesNode(UMaterialExpression* MakeMAExpression,
                                                                       UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                       int32 OutputIndex)
{
    if (!MakeMAExpression || !FunctionCall)
    {
        return false;
    }
    
    FString FunctionName = FunctionCall->MaterialFunction->GetName();
    UE_LOG(LogX_AssetEditor, Log, TEXT("连接到MakeMaterialAttributes节点，函数: %s"), *FunctionName);
    
    //  智能分析：检查所有输出引脚并逐个连接
    const TArray<FFunctionExpressionOutput>& FunctionOutputs = FunctionCall->FunctionOutputs;
    bool bAnyConnected = false;
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("函数 %s 有 %d 个输出引脚，开始智能匹配"), *FunctionName, FunctionOutputs.Num());
    
    // 遍历所有输出引脚，根据名称智能匹配到MakeMaterialAttributes的对应输入
    for (int32 i = 0; i < FunctionOutputs.Num(); ++i)
    {
        const FFunctionExpressionOutput& FunctionOutput = FunctionOutputs[i];
        const FExpressionOutput& Output = FunctionOutput.Output;
        const FString OutputName = Output.OutputName.ToString().ToLower();
        
        UE_LOG(LogX_AssetEditor, Log, TEXT("分析输出引脚 [%d]: %s"), i, *Output.OutputName.ToString());
        
        //  根据输出引脚名称智能匹配MaterialProperty
        EMaterialProperty TargetProperty = MP_EmissiveColor;  //  默认初始化，避免编译警告
        bool bFoundMatch = false;
        
        if (OutputName.Contains(TEXT("basecolor")))
        {
            TargetProperty = MP_BaseColor;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 BaseColor"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("metallic")))
        {
            TargetProperty = MP_Metallic;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 Metallic"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("roughness")))
        {
            TargetProperty = MP_Roughness;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 Roughness"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("normal")))
        {
            TargetProperty = MP_Normal;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 Normal"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("emissive")))
        {
            TargetProperty = MP_EmissiveColor;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 EmissiveColor"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("specular")))
        {
            TargetProperty = MP_Specular;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 Specular"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("ambient")) || OutputName.Contains(TEXT("ao")))
        {
            TargetProperty = MP_AmbientOcclusion;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 AmbientOcclusion"), *Output.OutputName.ToString());
        }
        else
        {
            // 如果只有一个输出引脚且无法识别，尝试根据函数名称推断
            if (FunctionOutputs.Num() == 1)
            {
                if (FunctionName.Contains(TEXT("Fresnel")))
                {
                    TargetProperty = MP_EmissiveColor;
                    bFoundMatch = true;
                    UE_LOG(LogX_AssetEditor, Log, TEXT("单输出Fresnel函数，推断为 EmissiveColor"));
                }
                else if (FunctionName.Contains(TEXT("BaseColor")) || FunctionName.Contains(TEXT("Diffuse")))
                {
                    TargetProperty = MP_BaseColor;
                    bFoundMatch = true;
                    UE_LOG(LogX_AssetEditor, Log, TEXT("单输出BaseColor函数，推断为 BaseColor"));
                }
            }
            
            if (!bFoundMatch)
            {
                UE_LOG(LogX_AssetEditor, Warning, TEXT("无法识别输出引脚 '%s'，跳过"), *Output.OutputName.ToString());
                continue;
            }
        }
        
        //  优化：优先使用经过UE源码验证的直接连接方法
        FString PropertyName = GetMaterialPropertyDisplayName(TargetProperty);
        
        UE_LOG(LogX_AssetEditor, Log, TEXT("连接输出引脚 '%s' 到 MakeMaterialAttributes.%s"), 
            *Output.OutputName.ToString(), *PropertyName);
        
        //  直接使用经过验证的连接方法（性能更好，更可靠）
        bool bSuccess = ConnectToMakeMaterialAttributesDirect(MakeMAExpression, FunctionCall, TargetProperty, i);
        
        if (!bSuccess)
        {
            //  备选方案：官方API（作为调试选项）
            FString OutputPinName = Output.OutputName.ToString();
            UE_LOG(LogX_AssetEditor, Warning, TEXT("直接连接失败，尝试官方API：输出引脚='%s', 目标属性='%s'"), 
                *OutputPinName, *PropertyName);
            
            bSuccess = UMaterialEditingLibrary::ConnectMaterialExpressions(
                FunctionCall, 
                OutputPinName,
                MakeMAExpression,
                PropertyName
            );
            
            if (bSuccess)
            {
                UE_LOG(LogX_AssetEditor, Log, TEXT("官方API连接成功"));
            }
            else
            {
                UE_LOG(LogX_AssetEditor, Warning, TEXT("所有连接方法都失败"));
            }
        }
        
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("成功连接输出引脚 [%d] '%s' 到 MakeMaterialAttributes.%s"), 
                i, *Output.OutputName.ToString(), *PropertyName);
            bAnyConnected = true;
        }
        else
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("连接输出引脚 [%d] '%s' 到 MakeMaterialAttributes.%s 失败"), 
                i, *Output.OutputName.ToString(), *PropertyName);
        }
    }
    
    if (!bAnyConnected && FunctionOutputs.Num() > 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("所有输出引脚连接失败，函数: %s"), *FunctionName);
    }
    
    return bAnyConnected;
}

bool FX_MaterialFunctionConnector::ConnectToMakeMaterialAttributesDirect(UMaterialExpression* MakeMAExpression,
                                                                         UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                         EMaterialProperty TargetProperty,
                                                                         int32 OutputIndex)
{
    if (!MakeMAExpression || !FunctionCall)
    {
        return false;
    }
    
    //  基于UE源码：直接转换为MakeMaterialAttributes类型
    UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(MakeMAExpression);
    if (!MakeMANode)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("表达式不是MakeMaterialAttributes类型"));
        return false;
    }
    
    //  基于UE源码：直接访问对应的成员变量
    FExpressionInput* TargetInput = nullptr;
    FString PropertyName;
    
    switch (TargetProperty)
    {
        case MP_BaseColor:
            TargetInput = &MakeMANode->BaseColor;
            PropertyName = TEXT("BaseColor");
            break;
        case MP_EmissiveColor:
            TargetInput = &MakeMANode->EmissiveColor;
            PropertyName = TEXT("EmissiveColor");
            break;
        case MP_Metallic:
            TargetInput = &MakeMANode->Metallic;
            PropertyName = TEXT("Metallic");
            break;
        case MP_Roughness:
            TargetInput = &MakeMANode->Roughness;
            PropertyName = TEXT("Roughness");
            break;
        case MP_Normal:
            TargetInput = &MakeMANode->Normal;
            PropertyName = TEXT("Normal");
            break;
        case MP_Specular:
            TargetInput = &MakeMANode->Specular;
            PropertyName = TEXT("Specular");
            break;
        case MP_AmbientOcclusion:
            TargetInput = &MakeMANode->AmbientOcclusion;
            PropertyName = TEXT("AmbientOcclusion");
            break;
        default:
            UE_LOG(LogX_AssetEditor, Warning, TEXT("不支持的MaterialProperty: %d"), (int32)TargetProperty);
            return false;
    }
    
    if (!TargetInput)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法获取目标输入引脚"));
        return false;
    }
    
    //  直接使用FExpressionInput的Connect方法
    TargetInput->Connect(OutputIndex, FunctionCall);
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("成功直接连接到MakeMaterialAttributes的 %s 成员变量"), *PropertyName);
    
    return true;
}

bool FX_MaterialFunctionConnector::ConnectToMaterialAttributesFunctionInputs(UMaterialExpressionMaterialFunctionCall* ExistingFunctionCall,
                                                                             UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                             int32 OutputIndex)
{
    if (!ExistingFunctionCall || !FunctionCall)
    {
        return false;
    }
    
    FString ExistingFunctionName = ExistingFunctionCall->MaterialFunction ? ExistingFunctionCall->MaterialFunction->GetName() : TEXT("Unknown");
    FString NewFunctionName = FunctionCall->MaterialFunction ? FunctionCall->MaterialFunction->GetName() : TEXT("Unknown");
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("尝试将 %s 连接到现有MaterialAttributes函数 %s 的输入"), 
        *NewFunctionName, *ExistingFunctionName);
    
    //  获取已有函数的输入引脚
    const TArray<FFunctionExpressionInput>& ExistingInputs = ExistingFunctionCall->FunctionInputs;
    
    //  根据新函数特性找到合适的输入引脚
    FString TargetInputName;
    if (NewFunctionName.Contains(TEXT("Fresnel")))
    {
        // 查找Emissive相关输入
        for (const FFunctionExpressionInput& Input : ExistingInputs)
        {
            FString InputName = Input.Input.InputName.ToString();
            if (InputName.Contains(TEXT("Emissive")) || InputName.Contains(TEXT("自发光")))
            {
                TargetInputName = InputName;
                break;
            }
        }
        if (TargetInputName.IsEmpty())
        {
            TargetInputName = TEXT("Emissive Color");  // 备用名称
        }
    }
    else
    {
        // 尝试找到第一个可用的输入
        if (ExistingInputs.Num() > 0)
        {
            TargetInputName = ExistingInputs[0].Input.InputName.ToString();
        }
    }
    
    if (TargetInputName.IsEmpty())
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法找到合适的输入引脚连接到MaterialAttributes函数"));
        return false;
    }
    
    //  使用官方API连接
    bool bSuccess = UMaterialEditingLibrary::ConnectMaterialExpressions(
        FunctionCall,
        OutputIndex == 0 ? FString() : FString::Printf(TEXT("%d"), OutputIndex),
        ExistingFunctionCall,
        TargetInputName
    );
    
    if (bSuccess)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("成功连接到MaterialAttributes函数的 %s 输入"), *TargetInputName);
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("连接到MaterialAttributes函数的 %s 输入失败"), *TargetInputName);
    }
    
    return bSuccess;
}

bool FX_MaterialFunctionConnector::ConnectToGenericMaterialAttributesExpression(UMaterialExpression* MaterialAttributesExpression,
                                                                               UMaterialExpressionMaterialFunctionCall* FunctionCall,
                                                                               int32 OutputIndex)
{
    if (!MaterialAttributesExpression || !FunctionCall)
    {
        return false;
    }
    
    FString ExpressionClassName = MaterialAttributesExpression->GetClass()->GetName();
    FString FunctionName = FunctionCall->MaterialFunction ? FunctionCall->MaterialFunction->GetName() : TEXT("Unknown");
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("尝试通用连接：函数 %s 到表达式 %s"), *FunctionName, *ExpressionClassName);
    
    //  尝试简单的表达式替换策略
    // 如果无法找到合适的输入引脚，可以考虑创建新的连接
    UE_LOG(LogX_AssetEditor, Warning, TEXT("暂不支持连接到 %s 类型的表达式，可能需要手动连接"), *ExpressionClassName);
    
    return false;
}

bool FX_MaterialFunctionConnector::FallbackConnectToFirstBaseEmissiveNode(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex)
{
    if (!Material || !FunctionCall)
    {
        return false;
    }
    
    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        return false;
    }
    
    FExpressionInput& MaterialAttributesInput = EditorOnlyData->MaterialAttributes;
    if (!MaterialAttributesInput.IsConnected())
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("保底回溯：材质的MaterialAttributes引脚未连接，跳过保底逻辑"));
        return false;
    }
    
    UMaterialExpression* RootExpression = MaterialAttributesInput.Expression;
    if (!RootExpression)
    {
        return false;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("保底回溯：从表达式 %s 开始搜索BaseColor+Emissive节点"),
        *RootExpression->GetClass()->GetName());
    
    UMaterialExpression* TargetExpression = FindFirstNodeWithBaseAndEmissiveInputs(RootExpression);
    if (!TargetExpression)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("保底回溯：未找到同时拥有BaseColor和Emissive输入的节点"));
        return false;
    }
    
    return ConnectFunctionToBaseEmissiveNode(TargetExpression, FunctionCall, OutputIndex);
}

UMaterialExpression* FX_MaterialFunctionConnector::FindFirstNodeWithBaseAndEmissiveInputs(
    UMaterialExpression* StartExpression)
{
    if (!StartExpression)
    {
        return nullptr;
    }
    
    TSet<UMaterialExpression*> Visited;
    TQueue<UMaterialExpression*> Queue;
    Queue.Enqueue(StartExpression);
    
    while (!Queue.IsEmpty())
    {
        UMaterialExpression* Current = nullptr;
        Queue.Dequeue(Current);
        if (!Current || Visited.Contains(Current))
        {
            continue;
        }
        
        Visited.Add(Current);
        
        if (HasBaseAndEmissiveInputs(Current))
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("保底回溯：找到BaseColor+Emissive节点: %s"),
                *Current->GetClass()->GetName());
            return Current;
        }
        
        TArray<UMaterialExpression*> UpstreamExpressions;
        CollectUpstreamExpressions(Current, UpstreamExpressions);
        
        for (UMaterialExpression* Upstream : UpstreamExpressions)
        {
            if (Upstream && !Visited.Contains(Upstream))
            {
                Queue.Enqueue(Upstream);
            }
        }
    }
    
    return nullptr;
}

bool FX_MaterialFunctionConnector::HasBaseAndEmissiveInputs(UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return false;
    }
    
    //  MakeMaterialAttributes节点天然拥有BaseColor/Emissive输入
    if (UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(Expression))
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("保底回溯：检测到MakeMaterialAttributes节点"));
        return true;
    }
    
    //  对MaterialFunctionCall，检查其FunctionInputs名称中是否同时包含BaseColor和Emissive
    if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
    {
        bool bHasBase = false;
        bool bHasEmissive = false;
        
        for (const FFunctionExpressionInput& Input : FunctionCall->FunctionInputs)
        {
            const FString InputName = Input.Input.InputName.ToString();
            if (InputName.Contains(TEXT("BaseColor")))
            {
                bHasBase = true;
            }
            if (InputName.Contains(TEXT("Emissive")) || InputName.Contains(TEXT("自发光")))
            {
                bHasEmissive = true;
            }
        }
        
        if (bHasBase && bHasEmissive)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("保底回溯：节点 %s 拥有BaseColor+Emissive输入"),
                *Expression->GetClass()->GetName());
            return true;
        }
    }
    
    return false;
}

void FX_MaterialFunctionConnector::CollectUpstreamExpressions(
    UMaterialExpression* Expression,
    TArray<UMaterialExpression*>& OutUpstreamExpressions)
{
    OutUpstreamExpressions.Reset();
    
    if (!Expression)
    {
        return;
    }
    
    //  对MakeMaterialAttributes，收集所有已连接的输入表达式
    if (UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(Expression))
    {
        auto CollectIfConnected = [&OutUpstreamExpressions](FExpressionInput& Input)
        {
            if (Input.IsConnected() && Input.Expression)
            {
                OutUpstreamExpressions.Add(Input.Expression);
            }
        };
        
        CollectIfConnected(MakeMANode->BaseColor);
        CollectIfConnected(MakeMANode->EmissiveColor);
        CollectIfConnected(MakeMANode->Metallic);
        CollectIfConnected(MakeMANode->Roughness);
        CollectIfConnected(MakeMANode->Normal);
        CollectIfConnected(MakeMANode->Specular);
        CollectIfConnected(MakeMANode->AmbientOcclusion);
        
        return;
    }
    
    //  对MaterialFunctionCall，收集所有输入的来源表达式
    if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
    {
        for (const FFunctionExpressionInput& FuncInput : FunctionCall->FunctionInputs)
        {
            const FExpressionInput& Input = FuncInput.Input;
            if (Input.IsConnected() && Input.Expression)
            {
                OutUpstreamExpressions.Add(Input.Expression);
            }
        }
        
        return;
    }
}

bool FX_MaterialFunctionConnector::ConnectFunctionToBaseEmissiveNode(
    UMaterialExpression* TargetExpression,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex)
{
    if (!TargetExpression || !FunctionCall)
    {
        return false;
    }
    
    //  如果是MakeMaterialAttributes节点，直接复用已有的智能连接逻辑
    if (TargetExpression->GetClass()->GetName().Contains(TEXT("MakeMaterialAttributes")))
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("保底回溯：将函数连接到MakeMaterialAttributes节点"));
        return ConnectToMakeMaterialAttributesNode(TargetExpression, FunctionCall, OutputIndex);
    }
    
    //  对MaterialFunctionCall，优先查找Emissive相关输入
    if (UMaterialExpressionMaterialFunctionCall* TargetFunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(TargetExpression))
    {
        FString TargetInputName;
        
        for (const FFunctionExpressionInput& Input : TargetFunctionCall->FunctionInputs)
        {
            const FString InputName = Input.Input.InputName.ToString();
            if (InputName.Contains(TEXT("Emissive")) || InputName.Contains(TEXT("自发光")))
            {
                TargetInputName = InputName;
                break;
            }
        }
        if (TargetInputName.IsEmpty() && TargetFunctionCall->FunctionInputs.Num() > 0)
        {
            //  找不到Emissive专用输入时，退而求其次使用第一个输入
            TargetInputName = TargetFunctionCall->FunctionInputs[0].Input.InputName.ToString();
        }
        
        if (TargetInputName.IsEmpty())
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("保底回溯：Base+Emissive节点上未找到可用输入引脚"));
            return false;
        }
        
        const FString OutputPinName = (OutputIndex == 0)
            ? FString()
            : FString::Printf(TEXT("%d"), OutputIndex);
        
        const bool bSuccess = UMaterialEditingLibrary::ConnectMaterialExpressions(
            FunctionCall,
            OutputPinName,
            TargetFunctionCall,
            TargetInputName);
        
        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("保底回溯：成功将函数输出连接到节点输入 %s"), *TargetInputName);
        }
        else
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("保底回溯：连接到节点输入 %s 失败"), *TargetInputName);
        }
        
        return bSuccess;
    }
    
    UE_LOG(LogX_AssetEditor, Warning,
        TEXT("保底回溯：不支持的Base+Emissive节点类型: %s"),
        *TargetExpression->GetClass()->GetName());
    
    return false;
}

bool FX_MaterialFunctionConnector::PrepareForModification(UMaterial* Material)
{
    if (!Material)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质对象为空，无法准备修改"));
        return false;
    }
    
    //  使用UE标准的Modify方法支持撤销/重做
    bool bResult = Material->Modify();
    
    if (bResult)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("成功准备材质修改: %s"), *Material->GetName());
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("准备材质修改失败: %s"), *Material->GetName());
    }
    
    return bResult;
}

//  模板函数已在头文件中实现 