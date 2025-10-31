#include "MaterialTools/X_MaterialFunctionCore.h"

// 核心模块
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

// 材质相关
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialFunction.h"

// 编辑器相关
#include "Logging/LogMacros.h"
#include "X_AssetEditor.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

// 插件相关
#include "Interfaces/IPluginManager.h"
#include "Settings/X_AssetEditorSettings.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "UObject/UnrealType.h"

UMaterial* FX_MaterialFunctionCore::GetBaseMaterial(UMaterialInterface* MaterialInterface)
{
    if (!MaterialInterface)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质接口为空"));
        return nullptr;
    }

    if (UMaterial* Material = Cast<UMaterial>(MaterialInterface))
    {
        return Material;
    }
    else if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface))
    {
        UMaterialInterface* Parent = MaterialInstance->Parent;
        if (Parent)
        {
            return GetBaseMaterial(Parent);
        }
    }

    return nullptr;
}

TArray<UMaterialFunctionInterface*> FX_MaterialFunctionCore::GetAllMaterialFunctions()
{
    TArray<UMaterialFunctionInterface*> MaterialFunctions;

    // 获取资产注册表
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // 构建材质函数资产过滤器 (使用ClassPaths替代已弃用的ClassNames)
    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;

    // 查询材质函数资产
    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    // 加载所有材质函数
    for (const FAssetData& AssetData : AssetList)
    {
        UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(AssetData.GetAsset());
        if (MaterialFunction)
        {
            MaterialFunctions.Add(MaterialFunction);
        }
    }

    return MaterialFunctions;
}

UMaterialFunctionInterface* FX_MaterialFunctionCore::GetFresnelFunction()
{
    UMaterialFunctionInterface* FresnelFunction = nullptr;
    
    // 动态获取插件名称并构建材质函数路径
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(FApp::GetProjectName());
    if (!Plugin.IsValid())
    {
        // 如果无法获取当前项目插件，尝试获取运行中的插件
        for (TSharedRef<IPlugin> ActivePlugin : IPluginManager::Get().GetEnabledPlugins())
        {
            // 检查模块是否包含X_AssetEditor，这可能是我们的插件
            const TArray<FModuleDescriptor>& Modules = ActivePlugin->GetDescriptor().Modules;
            for (const FModuleDescriptor& Module : Modules)
            {
                if (Module.Name == FName("X_AssetEditor"))
                {
                    Plugin = ActivePlugin;
                    break;
                }
            }
            if (Plugin.IsValid())
                break;
        }
    }

    // 找到了我们的插件
    if (Plugin.IsValid())
    {
        // 构建材质函数路径
        FString PluginName = Plugin->GetName();
        FString FresnelPath = FString::Printf(TEXT("/%s/MaterialFunctions/MF_SM_Fresnel"), *PluginName);
        
        UE_LOG(LogX_AssetEditor, Log, TEXT("尝试从插件 %s 加载菲涅尔函数: %s"), *PluginName, *FresnelPath);
        
        // 加载材质函数
        FresnelFunction = Cast<UMaterialFunctionInterface>(StaticLoadObject(UMaterialFunctionInterface::StaticClass(), nullptr, *FresnelPath));
    }
    
    // 如果未找到，尝试硬编码路径作为备选
    if (!FresnelFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法通过插件动态路径加载菲涅尔函数，尝试使用硬编码路径"));
        
        // 尝试硬编码路径
        const FString HardcodedPath = TEXT("/X_AssetEditor/MaterialFunctions/MF_SM_Fresnel");
        FresnelFunction = Cast<UMaterialFunctionInterface>(StaticLoadObject(UMaterialFunctionInterface::StaticClass(), nullptr, *HardcodedPath));
    }
    
    // 如果仍未找到，尝试引擎默认的菲涅尔函数
    if (!FresnelFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法从插件加载菲涅尔函数，尝试使用引擎默认函数"));
        
        // 备选：使用引擎自带的菲涅尔函数
        const FString EngineFallbackPath = TEXT("/Engine/Functions/Engine_MaterialFunctions01/Fresnel");
        FresnelFunction = Cast<UMaterialFunctionInterface>(StaticLoadObject(UMaterialFunctionInterface::StaticClass(), nullptr, *EngineFallbackPath));
    }
    
    // 最后尝试通过名称搜索
    if (!FresnelFunction)
    {
        UE_LOG(LogX_AssetEditor, Error, TEXT("无法加载任何菲涅尔函数，尝试搜索包含Fresnel名称的函数"));
        
        // 搜索包含"Fresnel"名称的函数作为最后的尝试
        TArray<UMaterialFunctionInterface*> AllFunctions = GetAllMaterialFunctions();
        for (UMaterialFunctionInterface* Function : AllFunctions)
        {
            if (Function && Function->GetName().Contains(TEXT("Fresnel")))
            {
                UE_LOG(LogX_AssetEditor, Log, TEXT("找到名为 %s 的菲涅尔函数"), *Function->GetName());
                FresnelFunction = Function;
                break;
            }
        }
    }
    
    if (FresnelFunction)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("成功加载菲涅尔函数: %s"), *FresnelFunction->GetName());
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Error, TEXT("无法找到任何菲涅尔函数"));
    }
    
    return FresnelFunction;
}

void FX_MaterialFunctionCore::RecompileMaterial(UMaterial* Material)
{
    if (!Material)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质为空，无法重新编译"));
        return;
    }
    
    // 标记材质为已修改
    Material->MarkPackageDirty();
    
    // 编译材质 - 使用标准的材质更新方式
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    
    // 使用正确的方式处理材质更新，不需要额外的PropertyChangedEvent
    // UE5.3中，在大多数情况下，PreEditChange和PostEditChange已足够触发材质重编译
}

bool FX_MaterialFunctionCore::RefreshOpenMaterialEditor(UMaterial* Material)
{
    if (!Material)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质为空，无法刷新编辑器"));
        return false;
    }
    
    if (!GEditor)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("GEditor为空，无法刷新编辑器"));
        return false;
    }
    
    // 获取资产编辑器子系统
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法获取资产编辑器子系统"));
        return false;
    }
    
    // 先更新材质以确保所有更改都被应用
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    
    // 判断材质编辑器是否已打开
    TArray<IAssetEditorInstance*> OpenEditors = AssetEditorSubsystem->FindEditorsForAsset(Material);
    if (OpenEditors.Num() > 0)
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("找到材质 %s 的编辑器实例，重新打开以刷新视图"), *Material->GetName());
        
        // 通过关闭然后重新打开编辑器来强制刷新视图
        AssetEditorSubsystem->CloseAllEditorsForAsset(Material);
        AssetEditorSubsystem->OpenEditorForAsset(Material);
        
        UE_LOG(LogX_AssetEditor, Log, TEXT("已重新打开材质编辑器以刷新视图"));
        return true;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("材质编辑器未打开，已重新编译材质 %s"), *Material->GetName());
    return true;
}

UMaterialFunctionInterface* FX_MaterialFunctionCore::GetMaterialFunctionByName(const FString& FunctionName)
{
    // 获取资产注册表
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // 构建材质函数资产过滤器 (使用ClassPaths替代已弃用的ClassNames)
    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;

    // 查询材质函数资产
    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    // 查找指定名称的材质函数
    for (const FAssetData& AssetData : AssetList)
    {
        if (AssetData.AssetName.ToString().Contains(FunctionName))
        {
            UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(AssetData.GetAsset());
            if (MaterialFunction)
            {
                return MaterialFunction;
            }
        }
    }

    UE_LOG(LogX_AssetEditor, Warning, TEXT("未找到材质函数: %s"), *FunctionName);
    return nullptr;
} 