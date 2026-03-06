#include "MaterialTools/X_MaterialFunctionManager.h"

// 核心模块
#include "Modules/ModuleManager.h"

// 材质相关
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialInstance.h"

// 网格体相关
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"

// 编辑器相关
#include "Logging/LogMacros.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

// 内容浏览器相关
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"

// 窗口和Slate
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"

#include "MaterialTools/X_MaterialFunctionCore.h"
#include "MaterialTools/X_MaterialFunctionOperation.h"
#include "MaterialTools/X_MaterialFunctionUI.h"
#include "X_AssetEditor.h"

// 不再需要外部函数声明，直接使用FX_MaterialFunctionUI类

//-----------------------------------------------------------------------------
// 材质基础操作
//-----------------------------------------------------------------------------



UMaterial* FX_MaterialFunctionManager::GetBaseMaterial(UMaterialInterface* MaterialInterface)
{
    return FX_MaterialFunctionCore::GetBaseMaterial(MaterialInterface);
}

TArray<UMaterialFunctionInterface*> FX_MaterialFunctionManager::GetAllMaterialFunctions()
{
    return FX_MaterialFunctionCore::GetAllMaterialFunctions();
}

UMaterialFunctionInterface* FX_MaterialFunctionManager::GetFresnelFunction()
{
    return FX_MaterialFunctionCore::GetFresnelFunction();
}

void FX_MaterialFunctionManager::RecompileMaterial(UMaterial* Material)
{
    FX_MaterialFunctionCore::RecompileMaterial(Material);
}

bool FX_MaterialFunctionManager::RefreshOpenMaterialEditor(UMaterial* Material)
{
    return FX_MaterialFunctionCore::RefreshOpenMaterialEditor(Material);
}

//-----------------------------------------------------------------------------
// 材质函数操作
//-----------------------------------------------------------------------------

UMaterialExpressionMaterialFunctionCall* FX_MaterialFunctionManager::FindNodeInMaterial(
    UMaterial* Material,
    const FName& NodeName)
{
    return FX_MaterialFunctionOperation::FindNodeInMaterial(Material, NodeName);
}

bool FX_MaterialFunctionManager::DoesMaterialContainFunction(
    UMaterial* Material,
    UMaterialFunctionInterface* Function)
{
    return FX_MaterialFunctionOperation::DoesMaterialContainFunction(Material, Function);
}

UMaterialExpressionMaterialFunctionCall* FX_MaterialFunctionManager::AddFunctionToMaterial(
    UMaterial* Material,
    UMaterialFunctionInterface* Function,
    const FName& NodeName,
    int32 PosX,
    int32 PosY)
{
    return FX_MaterialFunctionOperation::AddFunctionToMaterial(Material, Function, NodeName, PosX, PosY, true, true, EConnectionMode::Add, nullptr);
}

//-----------------------------------------------------------------------------
// 材质属性连接
//-----------------------------------------------------------------------------

bool FX_MaterialFunctionManager::ConnectExpressionToMaterialProperty(
    UMaterial* Material,
    UMaterialExpression* Expression,
    EMaterialProperty MaterialProperty,
    int32 OutputIndex)
{
    return FX_MaterialFunctionOperation::ConnectExpressionToMaterialProperty(Material, Expression, MaterialProperty, OutputIndex);
}

bool FX_MaterialFunctionManager::ConnectExpressionToMaterialPropertyByName(
    UMaterial* Material,
    UMaterialExpression* Expression,
    const FString& PropertyName,
    int32 OutputIndex)
{
    return FX_MaterialFunctionOperation::ConnectExpressionToMaterialPropertyByName(Material, Expression, PropertyName, OutputIndex);
}





//-----------------------------------------------------------------------------
// 批量处理接口
//-----------------------------------------------------------------------------

FMaterialProcessResult FX_MaterialFunctionManager::AddFunctionToMultipleMaterials(
    const TArray<UObject*>& SourceObjects,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& NodeName,
    int32 PosX,
    int32 PosY,
    bool bSetupConnections,
    const FX_MaterialFunctionParams* Params)
{
    // 创建一个TSharedPtr包装原始参数
    TSharedPtr<FX_MaterialFunctionParams> ParamsPtr = nullptr;
    if (Params)
    {
        ParamsPtr = MakeShareable(new FX_MaterialFunctionParams(*Params));
    }
    
    return FX_MaterialFunctionOperation::AddFunctionToMultipleMaterials(
        SourceObjects, 
        MaterialFunction, 
        NodeName, 
        PosX, 
        PosY, 
        bSetupConnections,
        ParamsPtr);
}

FMaterialProcessResult FX_MaterialFunctionManager::AddFresnelToAssets(
    const TArray<UObject*>& SourceObjects)
{
    return FX_MaterialFunctionOperation::AddFresnelToAssets(SourceObjects);
}

//-----------------------------------------------------------------------------
// 材质表达式创建
//-----------------------------------------------------------------------------

UMaterialExpressionMaterialFunctionCall* FX_MaterialFunctionManager::CreateMaterialFunctionCallExpression(
    UMaterial* Material,
    UMaterialFunctionInterface* Function,
    int32 PosX,
    int32 PosY)
{
    return FX_MaterialFunctionOperation::CreateMaterialFunctionCallExpression(Material, Function, PosX, PosY);
}



//-----------------------------------------------------------------------------
// UI操作
//-----------------------------------------------------------------------------

TSharedRef<SWindow> FX_MaterialFunctionManager::CreateMaterialFunctionPickerWindow(FOnMaterialFunctionSelected OnFunctionSelected)
{
    return FX_MaterialFunctionUI::CreateMaterialFunctionPickerWindow(OnFunctionSelected);
}

TSharedRef<SWindow> FX_MaterialFunctionManager::ShowMaterialFunctionPicker(FOnMaterialFunctionSelected OnFunctionSelected)
{
    return FX_MaterialFunctionUI::ShowMaterialFunctionPickerWindow(OnFunctionSelected);
}

TSharedRef<SWindow> FX_MaterialFunctionManager::ShowNewMaterialFunctionPicker(FOnMaterialFunctionSelected OnFunctionSelected)
{
    return FX_MaterialFunctionUI::ShowMaterialFunctionPickerWindow(OnFunctionSelected);
}

TSharedRef<SWindow> FX_MaterialFunctionManager::CreateNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected)
{
    return FX_MaterialFunctionUI::CreateNodePickerWindow(OnNodeSelected);
}

TSharedRef<SWindow> FX_MaterialFunctionManager::ShowNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected)
{
    return FX_MaterialFunctionUI::ShowNodePickerWindow(OnNodeSelected);
}

TArray<TSharedPtr<FName>> FX_MaterialFunctionManager::GetCommonNodeNames()
{
    return FX_MaterialFunctionUI::GetCommonNodeNames();
}



// 添加一个通用方法，从各种资产类型中收集材质接口
TArray<UMaterialInterface*> FX_MaterialFunctionManager::CollectMaterialsFromAssets(
    TArray<UObject*> SourceObjects)
{
    return FX_MaterialFunctionOperation::CollectMaterialsFromAssets(SourceObjects);
}
