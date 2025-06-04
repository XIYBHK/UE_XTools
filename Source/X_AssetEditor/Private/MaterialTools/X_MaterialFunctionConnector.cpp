#include "MaterialTools/X_MaterialFunctionConnector.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Logging/LogMacros.h"
#include "X_AssetEditor.h"
#include "MaterialTools/X_MaterialFunctionParams.h"

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

    UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
    if (!EditorOnlyData)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法获取材质编辑器数据"));
        return false;
    }

    switch (MaterialProperty)
    {
        case MP_BaseColor:
            EditorOnlyData->BaseColor.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到BaseColor"));
            return true;
        case MP_Metallic:
            EditorOnlyData->Metallic.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Metallic"));
            return true;
        case MP_Specular:
            EditorOnlyData->Specular.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Specular"));
            return true;
        case MP_Roughness:
            EditorOnlyData->Roughness.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Roughness"));
            return true;
        case MP_EmissiveColor:
            EditorOnlyData->EmissiveColor.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到EmissiveColor"));
            return true;
        case MP_Opacity:
            EditorOnlyData->Opacity.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Opacity"));
            return true;
        case MP_OpacityMask:
            EditorOnlyData->OpacityMask.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到OpacityMask"));
            return true;
        case MP_Normal:
            EditorOnlyData->Normal.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Normal"));
            return true;
        case MP_WorldPositionOffset:
            EditorOnlyData->WorldPositionOffset.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到WorldPositionOffset"));
            return true;
        case MP_SubsurfaceColor:
            EditorOnlyData->SubsurfaceColor.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到SubsurfaceColor"));
            return true;
        case MP_AmbientOcclusion:
            EditorOnlyData->AmbientOcclusion.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到AmbientOcclusion"));
            return true;
        case MP_Refraction:
            EditorOnlyData->Refraction.Connect(OutputIndex, Expression);
            UE_LOG(LogX_AssetEditor, Log, TEXT("已连接到Refraction"));
            return true;
        default:
            UE_LOG(LogX_AssetEditor, Warning, TEXT("不支持的材质属性类型"));
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

    // 尝试将字符串属性名转换为EMaterialProperty
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
    else
        return false;

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

    UE_LOG(LogX_AssetEditor, Log, TEXT("正在对材质 %s 应用智能连接逻辑..."), *Material->GetName());

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