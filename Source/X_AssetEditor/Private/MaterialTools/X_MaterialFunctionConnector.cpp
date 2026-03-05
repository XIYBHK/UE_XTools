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
#include "MaterialGraph/MaterialGraph.h"
//  添加MakeMaterialAttributes支持
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
//  引入常量定义
#include "MaterialTools/X_MaterialConstants.h"

namespace
{
    int32 CalculateMatchScore(const FString& InputName, const FString& TargetName);

    FString NormalizeForMatch(const FString& InName)
    {
        FString Lower = InName.ToLower();
        FString Normalized;
        Normalized.Reserve(Lower.Len());
        for (TCHAR Char : Lower)
        {
            if (FChar::IsAlnum(Char))
            {
                Normalized.AppendChar(Char);
            }
        }
        return Normalized;
    }

    bool IsIgnoredPinName(const FString& PinName)
    {
        const FString Lower = PinName.ToLower();
        return Lower.StartsWith(X_MaterialConstants::Prefix_Not.ToLower())
            || Lower.StartsWith(X_MaterialConstants::Prefix_Ignore.ToLower());
    }

    TArray<FString> GetPropertyAliases(EMaterialProperty Property)
    {
        switch (Property)
        {
            case MP_BaseColor:
                return {X_MaterialConstants::Alias_Albedo, X_MaterialConstants::Alias_Diffuse};
            case MP_Metallic:
                return {X_MaterialConstants::Alias_Metalness};
            case MP_Roughness:
                return {X_MaterialConstants::Alias_Rough};
            case MP_EmissiveColor:
                return {X_MaterialConstants::Alias_Emission, X_MaterialConstants::Alias_Emissive};
            case MP_AmbientOcclusion:
                return {X_MaterialConstants::Alias_AO, X_MaterialConstants::Alias_Ambient};
            default:
                return {};
        }
    }

    bool IsNumericMaterialType(uint32 Type)
    {
        return (Type & MCT_Numeric) != 0;
    }

    bool IsTextureMaterialType(uint32 Type)
    {
        return (Type & MCT_Texture) != 0
            || Type == MCT_SparseVolumeTexture;
    }

    bool AreMaterialTypesCompatible(uint32 SourceType, uint32 TargetType)
    {
        if (SourceType == MCT_Unknown || TargetType == MCT_Unknown)
        {
            return true;
        }

        if (SourceType == TargetType)
        {
            return true;
        }

        const bool bSourceIsMaterialAttributes = (SourceType == MCT_MaterialAttributes);
        const bool bTargetAcceptsMaterialAttributes = (TargetType & MCT_MaterialAttributes) != 0;
        if (bSourceIsMaterialAttributes || bTargetAcceptsMaterialAttributes)
        {
            return bSourceIsMaterialAttributes && bTargetAcceptsMaterialAttributes;
        }

        if (IsTextureMaterialType(SourceType) || IsTextureMaterialType(TargetType))
        {
            return (SourceType & TargetType) != 0;
        }

        if (IsNumericMaterialType(SourceType) && IsNumericMaterialType(TargetType))
        {
            return true;
        }

        return (SourceType & TargetType) != 0;
    }

    uint32 GetExpectedMaterialPropertyType(EMaterialProperty Property)
    {
        return static_cast<uint32>(FMaterialAttributeDefinitionMap::GetValueType(Property));
    }

    int32 CalculateMatchScoreWithAliases(const FString& InputName, const FString& TargetName, const TArray<FString>& Aliases)
    {
        int32 Score = CalculateMatchScore(InputName, TargetName);
        for (const FString& Alias : Aliases)
        {
            Score = FMath::Max(Score, CalculateMatchScore(InputName, Alias));
        }
        return Score;
    }

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

        if (IsIgnoredPinName(InputName))
        {
            return -100;
        }

        const FString InputLower = InputName.ToLower();
        const FString TargetLower = TargetName.ToLower();
        const FString InputNormalized = NormalizeForMatch(InputName);
        const FString TargetNormalized = NormalizeForMatch(TargetName);

        if (InputNormalized.IsEmpty() || TargetNormalized.IsEmpty())
        {
            return -1;
        }

        // 规范化后完全一致，最高优先级。
        if (InputNormalized.Equals(TargetNormalized))
        {
            return 120;
        }

        if (InputLower.Equals(TargetLower))
        {
            return 110;
        }

        if (InputNormalized.StartsWith(TargetNormalized) || InputNormalized.EndsWith(TargetNormalized))
        {
            return 90;
        }

        if (InputLower.Contains(TargetLower) || InputNormalized.Contains(TargetNormalized))
        {
            return 65;
        }

        return 0;
    }

    int32 FindBestFunctionInputIndexBySemantic(
        UMaterialExpressionMaterialFunctionCall* FunctionCall,
        const FString& SemanticName,
        uint32 SourceType,
        const TSet<int32>* ExcludedInputs = nullptr)
    {
        if (!FunctionCall)
        {
            return INDEX_NONE;
        }

        int32 BestScore = -1;
        int32 BestIndex = INDEX_NONE;

        for (int32 InputIndex = 0; InputIndex < FunctionCall->FunctionInputs.Num(); ++InputIndex)
        {
            if (ExcludedInputs && ExcludedInputs->Contains(InputIndex))
            {
                continue;
            }

            const uint32 TargetInputType = static_cast<uint32>(FunctionCall->GetInputType(InputIndex));
            if (!AreMaterialTypesCompatible(SourceType, TargetInputType))
            {
                continue;
            }

            const FString InputName = FunctionCall->FunctionInputs[InputIndex].Input.InputName.ToString();
            int32 Score = CalculateMatchScore(InputName, SemanticName);
            if (Score < 0)
            {
                Score = 0;
            }

            if (Score > BestScore)
            {
                BestScore = Score;
                BestIndex = InputIndex;
            }
        }

        return BestIndex;
    }

    int32 FindBestFunctionOutputIndexBySemantic(
        UMaterialExpressionMaterialFunctionCall* FunctionCall,
        const FString& SemanticName,
        uint32 TargetType,
        int32 PreferredOutputIndex = INDEX_NONE)
    {
        if (!FunctionCall || FunctionCall->FunctionOutputs.Num() == 0)
        {
            return INDEX_NONE;
        }

        int32 BestScore = -1;
        int32 BestIndex = INDEX_NONE;

        for (int32 OutputIndex = 0; OutputIndex < FunctionCall->FunctionOutputs.Num(); ++OutputIndex)
        {
            const uint32 SourceOutputType = static_cast<uint32>(FunctionCall->GetOutputType(OutputIndex));
            if (TargetType != MCT_Unknown && !AreMaterialTypesCompatible(SourceOutputType, TargetType))
            {
                continue;
            }

            const FString OutputName = FunctionCall->FunctionOutputs[OutputIndex].Output.OutputName.ToString();
            int32 Score = CalculateMatchScore(OutputName, SemanticName);
            if (Score < 0)
            {
                Score = 0;
            }

            if (PreferredOutputIndex == OutputIndex)
            {
                Score += 25;
            }

            if (Score > BestScore)
            {
                BestScore = Score;
                BestIndex = OutputIndex;
            }
        }

        if (BestIndex == INDEX_NONE && PreferredOutputIndex != INDEX_NONE
            && FunctionCall->FunctionOutputs.IsValidIndex(PreferredOutputIndex))
        {
            const uint32 PreferredType = static_cast<uint32>(FunctionCall->GetOutputType(PreferredOutputIndex));
            if (TargetType == MCT_Unknown || AreMaterialTypesCompatible(PreferredType, TargetType))
            {
                return PreferredOutputIndex;
            }
        }

        if (BestIndex == INDEX_NONE)
        {
            return 0;
        }

        return BestIndex;
    }

    bool InsertFunctionIntoExpressionInput(
        UMaterialExpression* TargetExpression,
        int32 TargetInputIndex,
        const FString& TargetInputSemantic,
        UMaterialExpressionMaterialFunctionCall* NewFunctionCall,
        int32 PreferredOutputIndex = INDEX_NONE)
    {
        if (!TargetExpression || !NewFunctionCall)
        {
            return false;
        }

        TArrayView<FExpressionInput*> TargetInputs = TargetExpression->GetInputsView();
        if (!TargetInputs.IsValidIndex(TargetInputIndex) || !TargetInputs[TargetInputIndex])
        {
            return false;
        }

        FExpressionInput* TargetInput = TargetInputs[TargetInputIndex];
        UMaterialExpression* PreviousExpression = TargetInput->Expression;
        const int32 PreviousOutputIndex = (TargetInput->OutputIndex == INDEX_NONE) ? 0 : TargetInput->OutputIndex;

        if (PreviousExpression)
        {
            const uint32 PreviousSourceType = static_cast<uint32>(PreviousExpression->GetOutputType(PreviousOutputIndex));
            const int32 FunctionInputIndex = FindBestFunctionInputIndexBySemantic(
                NewFunctionCall,
                TargetInputSemantic,
                PreviousSourceType);

            if (FunctionInputIndex != INDEX_NONE
                && NewFunctionCall->FunctionInputs.IsValidIndex(FunctionInputIndex))
            {
                NewFunctionCall->FunctionInputs[FunctionInputIndex].Input.Connect(PreviousOutputIndex, PreviousExpression);
            }
        }

        const uint32 TargetInputType = static_cast<uint32>(TargetExpression->GetInputType(TargetInputIndex));
        const int32 FunctionOutputIndex = FindBestFunctionOutputIndexBySemantic(
            NewFunctionCall,
            TargetInputSemantic,
            TargetInputType,
            PreferredOutputIndex);

        if (FunctionOutputIndex == INDEX_NONE)
        {
            return false;
        }

        TargetInput->Connect(FunctionOutputIndex, NewFunctionCall);
        return true;
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
    TSharedPtr<FX_MaterialFunctionParams> Params,
    bool bEnableSmartConnect)
{
    if (!Material || !FunctionCall)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质或函数调用为空"));
        return false;
    }

    const bool bSmartConnectEnabled = Params.IsValid() ? Params->bEnableSmartConnect : bEnableSmartConnect;

    // 智能连接关闭：优先走手动参数；无参数时使用基础回退策略，保证显式开关始终生效。
    if (!bSmartConnectEnabled)
    {
        if (Params.IsValid())
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("用户禁用了智能连接，使用手动配置模式"));
            return ProcessManualConnections(Material, FunctionCall, ConnectionMode, Params);
        }

        UE_LOG(LogX_AssetEditor, Log, TEXT("智能连接已禁用且未提供手动参数，使用基础连接策略"));

        if (IsMaterialAttributesEnabled(Material))
        {
            return ConnectMaterialAttributesToMaterial(Material, FunctionCall, 0);
        }

        if (ConnectionMode == EConnectionMode::Add)
        {
            return CreateAddConnectionToProperty(Material, FunctionCall, 0, MP_EmissiveColor) != nullptr;
        }

        if (ConnectionMode == EConnectionMode::Multiply)
        {
            return CreateMultiplyConnectionToProperty(Material, FunctionCall, 0, MP_EmissiveColor) != nullptr;
        }

        return ConnectExpressionToMaterialProperty(Material, FunctionCall, MP_BaseColor, 0);
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
        int32 OutputIndex;
        uint32 OutputType;
        FString PropertyName;
        TArray<FString> Aliases;
        bool bConsumed = false;
    };
    TArray<FPropertyConnection> PropertyConnections;

    // 辅助函数：添加连接目标
    auto AddConnectionTarget = [&](FExpressionInput& Input, EMaterialProperty Prop, const FString& Name, const TArray<FString>& Aliases = {})
    {
        if (Input.Expression)
        {
            const int32 SourceOutputIndex = (Input.OutputIndex == INDEX_NONE) ? 0 : Input.OutputIndex;
            const uint32 SourceOutputType = static_cast<uint32>(Input.Expression->GetOutputType(SourceOutputIndex));
            PropertyConnections.Add({&Input, Prop, SourceOutputIndex, SourceOutputType, Name, Aliases, false});
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
    for (int32 FunctionInputIndex = 0; FunctionInputIndex < FunctionInputs.Num(); ++FunctionInputIndex)
    {
        const FFunctionExpressionInput& FunctionInput = FunctionInputs[FunctionInputIndex];
        const FExpressionInput& Input = FunctionInput.Input;
        const FString InputName = Input.InputName.ToString();
        const uint32 TargetInputType = static_cast<uint32>(FunctionCall->GetInputType(FunctionInputIndex));

        if (IsIgnoredPinName(InputName))
        {
            UE_LOG(LogX_AssetEditor, Verbose, TEXT("输入引脚 %s 被忽略前缀排除"), *InputName);
            continue;
        }

        int32 BestScore = 0;
        FPropertyConnection* BestConnection = nullptr;

        // 遍历所有可能的连接目标，寻找最佳匹配
        for (FPropertyConnection& Connection : PropertyConnections)
        {
            if (Connection.bConsumed)
            {
                continue; // 已被占用
            }

            if (!AreMaterialTypesCompatible(Connection.OutputType, TargetInputType))
            {
                continue;
            }

            int32 Score = CalculateMatchScoreWithAliases(InputName, Connection.PropertyName, Connection.Aliases);

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
                const int32 SourceOutputIndex = (BestConnection->OutputIndex == INDEX_NONE) ? 0 : BestConnection->OutputIndex;
                InputPtr->Connect(SourceOutputIndex, BestConnection->Input->Expression);
                BestConnection->bConsumed = true;
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
                {MP_EmissiveColor, X_MaterialConstants::EmissiveColor, {X_MaterialConstants::Alias_Emission, X_MaterialConstants::Alias_Emissive}},
                {MP_AmbientOcclusion, X_MaterialConstants::AmbientOcclusion, {X_MaterialConstants::Alias_AO}}
            };

            for (const auto& Target : Targets)
            {
                const uint32 OutputType = static_cast<uint32>(FunctionCall->GetOutputType(OutputIndex));
                const uint32 PropertyType = GetExpectedMaterialPropertyType(Target.P);
                if (!AreMaterialTypesCompatible(OutputType, PropertyType))
                {
                    continue;
                }

                int32 Score = CalculateMatchScoreWithAliases(OutputName, Target.N, Target.A);

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

    // 使用UE官方API创建Add节点（自动处理RF_Transactional、GUID、Material属性等）
    int32 PosX = FunctionCall->MaterialExpressionEditorX + 200;
    int32 PosY = FunctionCall->MaterialExpressionEditorY;
    UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionAdd::StaticClass(), PosX, PosY);
    UMaterialExpressionAdd* AddExpression = Cast<UMaterialExpressionAdd>(NewExpression);
    if (!AddExpression)
    {
        return nullptr;
    }

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

    // 使用UE官方API创建Multiply节点（自动处理RF_Transactional、GUID、Material属性等）
    int32 PosX = FunctionCall->MaterialExpressionEditorX + 200;
    int32 PosY = FunctionCall->MaterialExpressionEditorY;
    UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionMultiply::StaticClass(), PosX, PosY);
    UMaterialExpressionMultiply* MultiplyExpression = Cast<UMaterialExpressionMultiply>(NewExpression);
    if (!MultiplyExpression)
    {
        return nullptr;
    }

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

    // Step 1: 处理输入引脚的自动连接（与原有智能连接逻辑一致）
    bool bInputConnected = ProcessMaterialAttributesInputConnections(Material, FunctionCall);

    // Step 2: 处理输出引脚的连接
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

            // 智能连接到已有的MaterialAttributes表达式
            bOutputConnected = ConnectToMaterialAttributesExpression(ExistingExpression, FunctionCall, OutputIndex);
        }
    }
    else
    {
        // 没有现有连接，直接连接到材质主节点
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

    // 最终结果：输入或输出有任何连接就算成功
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

// ========== MaterialAttributes连接相关函数实现 ==========

bool FX_MaterialFunctionConnector::ProcessMaterialAttributesInputConnections(
    UMaterial* Material,
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

    // 收集可用的属性连接源（优先从MakeMaterialAttributes获取，备用从材质主引脚获取）
    struct FAvailableConnection
    {
        UMaterialExpression* Expression;
        int32 OutputIndex;
        uint32 OutputType;
        EMaterialProperty Property;
        FString PropertyName;
        bool bConsumed = false;
    };
    TArray<FAvailableConnection> AvailableConnections;

    auto AddAvailableConnection = [&AvailableConnections](FExpressionInput& SourceInput, EMaterialProperty Property, const FString& PropertyName)
    {
        if (!SourceInput.IsConnected() || !SourceInput.Expression)
        {
            return;
        }

        const int32 SourceOutputIndex = (SourceInput.OutputIndex == INDEX_NONE) ? 0 : SourceInput.OutputIndex;
        const uint32 SourceOutputType = static_cast<uint32>(SourceInput.Expression->GetOutputType(SourceOutputIndex));
        AvailableConnections.Add({SourceInput.Expression, SourceOutputIndex, SourceOutputType, Property, PropertyName, false});
    };

    // 首先尝试从MaterialAttributes连接的节点获取连接
    if (EditorOnlyData->MaterialAttributes.IsConnected())
    {
        UMaterialExpression* MAExpression = EditorOnlyData->MaterialAttributes.Expression;

        // 情况1：MaterialAttributes连接到MakeMaterialAttributes节点
        if (UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(MAExpression))
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("从MakeMaterialAttributes节点收集可用连接"));

            // 从MakeMaterialAttributes节点的输入引脚收集连接
            AddAvailableConnection(MakeMANode->BaseColor, MP_BaseColor, TEXT("basecolor"));
            AddAvailableConnection(MakeMANode->EmissiveColor, MP_EmissiveColor, TEXT("emissive"));
            AddAvailableConnection(MakeMANode->Metallic, MP_Metallic, TEXT("metallic"));
            AddAvailableConnection(MakeMANode->Roughness, MP_Roughness, TEXT("roughness"));
            AddAvailableConnection(MakeMANode->Normal, MP_Normal, TEXT("normal"));
            AddAvailableConnection(MakeMANode->Specular, MP_Specular, TEXT("specular"));
            AddAvailableConnection(MakeMANode->AmbientOcclusion, MP_AmbientOcclusion, TEXT("ambient"));
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
                    AddAvailableConnection(FoundMakeMANode->BaseColor, MP_BaseColor, TEXT("basecolor"));
                    AddAvailableConnection(FoundMakeMANode->EmissiveColor, MP_EmissiveColor, TEXT("emissive"));
                    AddAvailableConnection(FoundMakeMANode->Metallic, MP_Metallic, TEXT("metallic"));
                    AddAvailableConnection(FoundMakeMANode->Roughness, MP_Roughness, TEXT("roughness"));
                    AddAvailableConnection(FoundMakeMANode->Normal, MP_Normal, TEXT("normal"));
                    AddAvailableConnection(FoundMakeMANode->Specular, MP_Specular, TEXT("specular"));
                    AddAvailableConnection(FoundMakeMANode->AmbientOcclusion, MP_AmbientOcclusion, TEXT("ambient"));
                }
            }
            else
            {
                UE_LOG(LogX_AssetEditor, Warning, TEXT("回溯未找到MakeMaterialAttributes节点"));
            }
        }
    }

    // 如果没有从MakeMaterialAttributes获取到连接，尝试从材质主引脚获取
    if (AvailableConnections.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("从材质主引脚收集可用连接"));

        AddAvailableConnection(EditorOnlyData->BaseColor, MP_BaseColor, TEXT("basecolor"));
        AddAvailableConnection(EditorOnlyData->EmissiveColor, MP_EmissiveColor, TEXT("emissive"));
        AddAvailableConnection(EditorOnlyData->Metallic, MP_Metallic, TEXT("metallic"));
        AddAvailableConnection(EditorOnlyData->Roughness, MP_Roughness, TEXT("roughness"));
        AddAvailableConnection(EditorOnlyData->Normal, MP_Normal, TEXT("normal"));
        AddAvailableConnection(EditorOnlyData->Specular, MP_Specular, TEXT("specular"));
        AddAvailableConnection(EditorOnlyData->AmbientOcclusion, MP_AmbientOcclusion, TEXT("ambient"));
    }

    UE_LOG(LogX_AssetEditor, Log, TEXT("收集到 %d 个可用属性连接"), AvailableConnections.Num());

    if (AvailableConnections.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("没有可用的属性连接，跳过输入连接处理"));
        return false;
    }

    // 尝试将可用连接匹配到函数的输入引脚
    bool bAnyInputConnected = false;

    for (int32 InputIndex = 0; InputIndex < FunctionInputs.Num(); ++InputIndex)
    {
        const FExpressionInput& Input = FunctionInputs[InputIndex].Input;
        const FString InputName = Input.InputName.ToString();
        if (IsIgnoredPinName(InputName))
        {
            UE_LOG(LogX_AssetEditor, Verbose, TEXT("MaterialAttributes模式：输入引脚 %s 被忽略前缀排除"), *InputName);
            continue;
        }

        const uint32 TargetInputType = static_cast<uint32>(FunctionCall->GetInputType(InputIndex));
        int32 BestScore = 0;
        int32 BestConnectionIndex = INDEX_NONE;

        for (int32 ConnectionIndex = 0; ConnectionIndex < AvailableConnections.Num(); ++ConnectionIndex)
        {
            const FAvailableConnection& Connection = AvailableConnections[ConnectionIndex];
            if (Connection.bConsumed || !Connection.Expression)
            {
                continue;
            }

            if (!AreMaterialTypesCompatible(Connection.OutputType, TargetInputType))
            {
                continue;
            }

            const TArray<FString> Aliases = GetPropertyAliases(Connection.Property);
            const int32 Score = CalculateMatchScoreWithAliases(InputName, Connection.PropertyName, Aliases);
            if (Score > BestScore)
            {
                BestScore = Score;
                BestConnectionIndex = ConnectionIndex;
            }
        }

        // 保底策略：对于前两个输入，按BaseColor/Emissive顺序兜底，避免无匹配时完全断开。
        bool bUsedFallback = false;
        if (BestConnectionIndex == INDEX_NONE && InputIndex < 2)
        {
            const EMaterialProperty PreferredProperty = (InputIndex == 0) ? MP_BaseColor : MP_EmissiveColor;
            for (int32 ConnectionIndex = 0; ConnectionIndex < AvailableConnections.Num(); ++ConnectionIndex)
            {
                const FAvailableConnection& Connection = AvailableConnections[ConnectionIndex];
                if (Connection.bConsumed || !Connection.Expression || Connection.Property != PreferredProperty)
                {
                    continue;
                }

                if (!AreMaterialTypesCompatible(Connection.OutputType, TargetInputType))
                {
                    continue;
                }

                BestConnectionIndex = ConnectionIndex;
                bUsedFallback = true;
                break;
            }
        }

        if (BestConnectionIndex == INDEX_NONE || (BestScore <= 0 && !bUsedFallback))
        {
            continue;
        }

        FExpressionInput* InputPtr = const_cast<FExpressionInput*>(&Input);
        if (!InputPtr)
        {
            continue;
        }

        FAvailableConnection& BestConnection = AvailableConnections[BestConnectionIndex];
        const int32 SourceOutputIndex = (BestConnection.OutputIndex == INDEX_NONE) ? 0 : BestConnection.OutputIndex;
        InputPtr->Connect(SourceOutputIndex, BestConnection.Expression);
        BestConnection.bConsumed = true;
        bAnyInputConnected = true;

        UE_LOG(LogX_AssetEditor, Log, TEXT("MaterialAttributes模式：连接 %s 到函数输入 %s (%s)"),
            *BestConnection.PropertyName,
            *InputName,
            bUsedFallback ? TEXT("保底策略") : TEXT("名称匹配"));
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

bool FX_MaterialFunctionConnector::ConnectToMaterialAttributesExpression(
    UMaterialExpression* MaterialAttributesExpression,
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

    // 检查是否是MakeMaterialAttributes表达式
    if (ExpressionClassName.Contains(TEXT("MakeMaterialAttributes")))
    {
        return ConnectToMakeMaterialAttributesNode(MaterialAttributesExpression, FunctionCall, OutputIndex);
    }

    // 检查是否是MaterialFunctionCall（可能是另一个MaterialAttributes函数）
    if (UMaterialExpressionMaterialFunctionCall* ExistingFunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(MaterialAttributesExpression))
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("检测到现有MaterialAttributes函数，尝试连接到其输入"));

        const bool bConnected = ConnectToMaterialAttributesFunctionInputs(ExistingFunctionCall, FunctionCall, OutputIndex);
        if (bConnected)
        {
            return true;
        }

        // 智能连接失败时的保底策略：从材质的MaterialAttributes链路向前回溯，
        // 查找第一个同时拥有BaseColor和Emissive输入的节点并连接。
        UE_LOG(LogX_AssetEditor, Warning, TEXT("连接到MaterialAttributes函数输入失败，尝试回溯到BaseColor+Emissive节点作为保底方案"));

        if (UMaterial* OwningMaterial = MaterialAttributesExpression->GetTypedOuter<UMaterial>())
        {
            return FallbackConnectToFirstBaseEmissiveNode(OwningMaterial, FunctionCall, OutputIndex);
        }

        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法从MaterialAttributes表达式获取所属材质对象，保底回溯逻辑中止"));
        return false;
    }

    // 其他MaterialAttributes表达式类型
    UE_LOG(LogX_AssetEditor, Warning, TEXT("未识别的MaterialAttributes表达式类型: %s，尝试通用连接"), *ExpressionClassName);
    return ConnectToGenericMaterialAttributesExpression(MaterialAttributesExpression, FunctionCall, OutputIndex);
}

bool FX_MaterialFunctionConnector::ConnectToMaterialAttributesFunctionInputs(
    UMaterialExpressionMaterialFunctionCall* ExistingFunctionCall,
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

    const TArray<FString> ExistingInputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(ExistingFunctionCall);
    const TArray<FFunctionExpressionOutput>& NewOutputs = FunctionCall->FunctionOutputs;
    TArrayView<FExpressionInput*> ExistingInputsView = ExistingFunctionCall->GetInputsView();

    int32 BestTargetInputIndex = INDEX_NONE;
    int32 BestTargetScore = -1;

    for (int32 InputIndex = 0; InputIndex < ExistingInputNames.Num(); ++InputIndex)
    {
        if (!ExistingInputsView.IsValidIndex(InputIndex) || !ExistingInputsView[InputIndex])
        {
            continue;
        }

        const FString CandidateName = ExistingInputNames[InputIndex];
        if (IsIgnoredPinName(CandidateName))
        {
            continue;
        }

        const uint32 CandidateInputType = static_cast<uint32>(ExistingFunctionCall->GetInputType(InputIndex));
        int32 Score = 0;

        for (int32 NewOutputIndex = 0; NewOutputIndex < NewOutputs.Num(); ++NewOutputIndex)
        {
            const uint32 NewOutputType = static_cast<uint32>(FunctionCall->GetOutputType(NewOutputIndex));
            if (!AreMaterialTypesCompatible(NewOutputType, CandidateInputType))
            {
                continue;
            }

            const FString OutputName = NewOutputs[NewOutputIndex].Output.OutputName.ToString();
            Score = FMath::Max(Score, CalculateMatchScore(OutputName, CandidateName));
        }

        Score = FMath::Max(Score, CalculateMatchScore(NewFunctionName, CandidateName));

        if (NewFunctionName.Contains(TEXT("Fresnel"))
            && (CandidateName.Contains(TEXT("Emissive")) || CandidateName.Contains(TEXT("Emission")) || CandidateName.Contains(TEXT("自发光"))))
        {
            Score += 25;
        }

        if (ExistingInputsView[InputIndex]->Expression)
        {
            Score += 5;
        }

        if (Score > BestTargetScore)
        {
            BestTargetScore = Score;
            BestTargetInputIndex = InputIndex;
        }
    }

    if (BestTargetInputIndex == INDEX_NONE && ExistingInputsView.Num() > 0)
    {
        BestTargetInputIndex = 0;
    }

    if (BestTargetInputIndex == INDEX_NONE)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法找到可用输入引脚以插入MaterialAttributes函数"));
        return false;
    }

    const FString TargetSemantic = ExistingInputNames.IsValidIndex(BestTargetInputIndex)
        ? ExistingInputNames[BestTargetInputIndex]
        : ExistingFunctionCall->GetInputName(BestTargetInputIndex).ToString();

    const bool bSuccess = InsertFunctionIntoExpressionInput(
        ExistingFunctionCall,
        BestTargetInputIndex,
        TargetSemantic,
        FunctionCall,
        OutputIndex);

    if (bSuccess)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("成功插入到MaterialAttributes函数输入: %s"), *TargetSemantic);
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("插入到MaterialAttributes函数输入失败: %s"), *TargetSemantic);
    }

    return bSuccess;
}

bool FX_MaterialFunctionConnector::ConnectToGenericMaterialAttributesExpression(
    UMaterialExpression* MaterialAttributesExpression,
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

    const TArray<FString> InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(MaterialAttributesExpression);
    TArrayView<FExpressionInput*> InputsView = MaterialAttributesExpression->GetInputsView();
    const TArray<FFunctionExpressionOutput>& FunctionOutputs = FunctionCall->FunctionOutputs;

    if (InputNames.Num() == 0 || InputsView.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("表达式 %s 没有可用输入引脚"), *ExpressionClassName);
        return false;
    }

    int32 BestInputIndex = INDEX_NONE;
    int32 BestScore = -1;

    for (int32 InputIndex = 0; InputIndex < InputNames.Num(); ++InputIndex)
    {
        if (!InputsView.IsValidIndex(InputIndex) || !InputsView[InputIndex])
        {
            continue;
        }

        const FString CandidateName = InputNames[InputIndex];
        if (IsIgnoredPinName(CandidateName))
        {
            continue;
        }

        const uint32 CandidateType = static_cast<uint32>(MaterialAttributesExpression->GetInputType(InputIndex));
        int32 Score = 0;

        for (int32 OutputPinIndex = 0; OutputPinIndex < FunctionOutputs.Num(); ++OutputPinIndex)
        {
            const uint32 OutputType = static_cast<uint32>(FunctionCall->GetOutputType(OutputPinIndex));
            if (!AreMaterialTypesCompatible(OutputType, CandidateType))
            {
                continue;
            }

            const FString OutputName = FunctionOutputs[OutputPinIndex].Output.OutputName.ToString();
            Score = FMath::Max(Score, CalculateMatchScore(OutputName, CandidateName));
        }

        Score = FMath::Max(Score, CalculateMatchScore(FunctionName, CandidateName));
        if (InputsView[InputIndex]->Expression)
        {
            Score += 5;
        }

        if (Score > BestScore)
        {
            BestScore = Score;
            BestInputIndex = InputIndex;
        }
    }

    if (BestInputIndex == INDEX_NONE)
    {
        BestInputIndex = 0;
    }

    if (!InputsView.IsValidIndex(BestInputIndex))
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("未找到可插入的表达式输入位: %s"), *ExpressionClassName);
        return false;
    }

    const FString TargetSemantic = InputNames.IsValidIndex(BestInputIndex)
        ? InputNames[BestInputIndex]
        : MaterialAttributesExpression->GetInputName(BestInputIndex).ToString();

    const bool bSuccess = InsertFunctionIntoExpressionInput(
        MaterialAttributesExpression,
        BestInputIndex,
        TargetSemantic,
        FunctionCall,
        OutputIndex);

    if (!bSuccess)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("通用插入失败: %s"), *ExpressionClassName);
    }

    return bSuccess;
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

// ========== 回溯查找相关函数实现 ==========

bool FX_MaterialFunctionConnector::HasBaseAndEmissiveInputs(UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return false;
    }

    // MakeMaterialAttributes节点天然拥有BaseColor/Emissive输入
    if (UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(Expression))
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("检测到MakeMaterialAttributes节点"));
        return true;
    }

    // 对MaterialFunctionCall，检查其FunctionInputs名称中是否同时包含BaseColor和Emissive
    if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
    {
        bool bHasBase = false;
        bool bHasEmissive = false;

        for (const FFunctionExpressionInput& Input : FunctionCall->FunctionInputs)
        {
            const FString InputName = Input.Input.InputName.ToString();
            if (InputName.Contains(TEXT("BaseColor")) || InputName.Contains(TEXT("Diffuse")) || InputName.Contains(TEXT("Albedo")))
            {
                bHasBase = true;
            }
            if (InputName.Contains(TEXT("Emissive")) || InputName.Contains(TEXT("自发光")) || InputName.Contains(TEXT("Emission")))
            {
                bHasEmissive = true;
            }
        }

        if (bHasBase && bHasEmissive)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("节点 %s 拥有BaseColor+Emissive输入"),
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

    // 对MakeMaterialAttributes，收集所有已连接的输入表达式
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

    // 对MaterialFunctionCall，收集所有输入的来源表达式
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
            UE_LOG(LogX_AssetEditor, Log, TEXT("回溯找到BaseColor+Emissive节点: %s"),
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

bool FX_MaterialFunctionConnector::ConnectFunctionToBaseEmissiveNode(
    UMaterialExpression* TargetExpression,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex)
{
    if (!TargetExpression || !FunctionCall)
    {
        return false;
    }

    // 如果是MakeMaterialAttributes节点，直接复用已有的智能连接逻辑
    if (UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(TargetExpression))
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("将函数连接到MakeMaterialAttributes节点"));
        return ConnectToMakeMaterialAttributesNode(MakeMANode, FunctionCall, OutputIndex);
    }

    // 对MaterialFunctionCall，优先查找Emissive相关输入
    if (UMaterialExpressionMaterialFunctionCall* TargetFunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(TargetExpression))
    {
        const TArray<FString> InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(TargetFunctionCall);
        int32 TargetInputIndex = INDEX_NONE;
        for (int32 InputIndex = 0; InputIndex < InputNames.Num(); ++InputIndex)
        {
            const FString& InputName = InputNames[InputIndex];
            if (InputName.Contains(TEXT("Emissive")) || InputName.Contains(TEXT("自发光")) || InputName.Contains(TEXT("Emission")))
            {
                TargetInputIndex = InputIndex;
                break;
            }
        }

        if (TargetInputIndex == INDEX_NONE && InputNames.Num() > 0)
        {
            TargetInputIndex = 0;
        }

        if (TargetInputIndex == INDEX_NONE)
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("Base+Emissive节点上未找到可用输入引脚"));
            return false;
        }

        const FString TargetSemantic = InputNames.IsValidIndex(TargetInputIndex)
            ? InputNames[TargetInputIndex]
            : TEXT("Emissive");

        const bool bSuccess = InsertFunctionIntoExpressionInput(
            TargetFunctionCall,
            TargetInputIndex,
            TargetSemantic,
            FunctionCall,
            OutputIndex);

        if (bSuccess)
        {
            UE_LOG(LogX_AssetEditor, Log, TEXT("成功将函数插入到节点输入 %s"), *TargetSemantic);
        }
        else
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("插入到节点输入 %s 失败"), *TargetSemantic);
        }

        return bSuccess;
    }

    UE_LOG(LogX_AssetEditor, Warning,
        TEXT("不支持的Base+Emissive节点类型: %s"),
        *TargetExpression->GetClass()->GetName());

    return false;
}

bool FX_MaterialFunctionConnector::ConnectToMakeMaterialAttributesNode(
    UMaterialExpression* MakeMAExpression,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex)
{
    if (!MakeMAExpression || !FunctionCall)
    {
        return false;
    }

    UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(MakeMAExpression);
    if (!MakeMANode)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("表达式不是MakeMaterialAttributes类型"));
        return false;
    }

    FString FunctionName = FunctionCall->MaterialFunction ? FunctionCall->MaterialFunction->GetName() : TEXT("Unknown");
    UE_LOG(LogX_AssetEditor, Log, TEXT("连接到MakeMaterialAttributes节点，函数: %s"), *FunctionName);

    // 智能分析：检查所有输出引脚并逐个连接
    const TArray<FFunctionExpressionOutput>& FunctionOutputs = FunctionCall->FunctionOutputs;
    bool bAnyConnected = false;
    TSet<int32> UsedFunctionInputIndices;

    UE_LOG(LogX_AssetEditor, Log, TEXT("函数 %s 有 %d 个输出引脚，开始智能匹配"), *FunctionName, FunctionOutputs.Num());

    // 遍历所有输出引脚，根据名称智能匹配到MakeMaterialAttributes的对应输入
    for (int32 i = 0; i < FunctionOutputs.Num(); ++i)
    {
        const FFunctionExpressionOutput& FunctionOutput = FunctionOutputs[i];
        const FExpressionOutput& Output = FunctionOutput.Output;
        const FString OutputName = Output.OutputName.ToString().ToLower();

        UE_LOG(LogX_AssetEditor, Log, TEXT("分析输出引脚 [%d]: %s"), i, *Output.OutputName.ToString());

        // 根据输出引脚名称智能匹配MaterialProperty
        FExpressionInput* TargetInput = nullptr;
        FString TargetSemantic;
        bool bFoundMatch = false;

        if (OutputName.Contains(TEXT("basecolor")) || OutputName.Contains(TEXT("diffuse")) || OutputName.Contains(TEXT("albedo")))
        {
            TargetInput = &MakeMANode->BaseColor;
            TargetSemantic = X_MaterialConstants::BaseColor;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 BaseColor"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("metallic")))
        {
            TargetInput = &MakeMANode->Metallic;
            TargetSemantic = X_MaterialConstants::Metallic;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 Metallic"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("roughness")) || OutputName.Contains(TEXT("rough")))
        {
            TargetInput = &MakeMANode->Roughness;
            TargetSemantic = X_MaterialConstants::Roughness;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 Roughness"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("normal")))
        {
            TargetInput = &MakeMANode->Normal;
            TargetSemantic = X_MaterialConstants::Normal;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 Normal"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("emissive")) || OutputName.Contains(TEXT("emission")) || OutputName.Contains(TEXT("自发光")))
        {
            TargetInput = &MakeMANode->EmissiveColor;
            TargetSemantic = X_MaterialConstants::EmissiveColor;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 EmissiveColor"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("specular")))
        {
            TargetInput = &MakeMANode->Specular;
            TargetSemantic = X_MaterialConstants::Specular;
            bFoundMatch = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("输出引脚 '%s' 匹配到 Specular"), *Output.OutputName.ToString());
        }
        else if (OutputName.Contains(TEXT("ambient")) || OutputName.Contains(TEXT("ao")))
        {
            TargetInput = &MakeMANode->AmbientOcclusion;
            TargetSemantic = X_MaterialConstants::AmbientOcclusion;
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
                    TargetInput = &MakeMANode->EmissiveColor;
                    TargetSemantic = X_MaterialConstants::EmissiveColor;
                    bFoundMatch = true;
                    UE_LOG(LogX_AssetEditor, Log, TEXT("单输出Fresnel函数，推断为 EmissiveColor"));
                }
                else if (FunctionName.Contains(TEXT("BaseColor")) || FunctionName.Contains(TEXT("Diffuse")))
                {
                    TargetInput = &MakeMANode->BaseColor;
                    TargetSemantic = X_MaterialConstants::BaseColor;
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

        // 使用UE源码验证的直接连接方法
        if (bFoundMatch && TargetInput)
        {
            UMaterialExpression* PreviousExpression = TargetInput->Expression;
            const int32 PreviousOutputIndex = (TargetInput->OutputIndex == INDEX_NONE) ? 0 : TargetInput->OutputIndex;

            if (PreviousExpression)
            {
                const uint32 PreviousSourceType = static_cast<uint32>(PreviousExpression->GetOutputType(PreviousOutputIndex));
                const FString PreserveSemantic = TargetSemantic.IsEmpty()
                    ? Output.OutputName.ToString()
                    : TargetSemantic;

                const int32 FunctionInputIndex = FindBestFunctionInputIndexBySemantic(
                    FunctionCall,
                    PreserveSemantic,
                    PreviousSourceType,
                    &UsedFunctionInputIndices);

                if (FunctionInputIndex != INDEX_NONE
                    && FunctionCall->FunctionInputs.IsValidIndex(FunctionInputIndex))
                {
                    FExpressionInput& FunctionInput = FunctionCall->FunctionInputs[FunctionInputIndex].Input;
                    if (!FunctionInput.Expression)
                    {
                        FunctionInput.Connect(PreviousOutputIndex, PreviousExpression);
                        UsedFunctionInputIndices.Add(FunctionInputIndex);
                        UE_LOG(LogX_AssetEditor, Log, TEXT("已保留原链路：%s -> 新函数输入[%d]"),
                            *PreserveSemantic, FunctionInputIndex);
                    }
                }
            }

            TargetInput->Connect(i, FunctionCall);
            bAnyConnected = true;
            UE_LOG(LogX_AssetEditor, Log, TEXT("成功连接输出 [%d] 到MakeMaterialAttributes节点"), i);
        }
    }

    if (bAnyConnected)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("成功将函数连接到MakeMaterialAttributes节点"));
        return true;
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("未能匹配任何输出引脚到MakeMaterialAttributes节点"));
        return false;
    }
}
