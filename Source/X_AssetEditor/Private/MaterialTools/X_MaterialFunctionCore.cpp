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
#include "MaterialEditingLibrary.h"
#include "MaterialEditorUtilities.h"
#include "MaterialGraph/MaterialGraph.h"

UMaterial* FX_MaterialFunctionCore::GetBaseMaterial(UMaterialInterface* MaterialInterface)
{
    if (!MaterialInterface)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质接口为空"));
        return nullptr;
    }

    // UMaterialInterface::GetMaterial() 已经实现了递归查找基础材质的逻辑
    // 优化：直接使用UE官方API，无需手动递归
    return MaterialInterface->GetMaterial();
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

    auto TryLoadFresnelAtPath = [](const FString& BasePath) -> UMaterialFunctionInterface*
    {
        // 优先尝试标准对象路径：/Mount/Path/Asset.Asset
        const FString ObjectPath = BasePath + TEXT(".MF_SM_Fresnel");
        if (UMaterialFunctionInterface* Loaded = Cast<UMaterialFunctionInterface>(
            StaticLoadObject(UMaterialFunctionInterface::StaticClass(), nullptr, *ObjectPath)))
        {
            return Loaded;
        }

        // 兼容旧写法：/Mount/Path/Asset
        return Cast<UMaterialFunctionInterface>(
            StaticLoadObject(UMaterialFunctionInterface::StaticClass(), nullptr, *BasePath));
    };
    
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
        
        FresnelFunction = TryLoadFresnelAtPath(FresnelPath);
    }

    // 如果未找到，尝试硬编码路径作为备选
    if (!FresnelFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("无法通过插件动态路径加载菲涅尔函数，尝试使用硬编码路径"));

        // 优先尝试常见的 XTools 挂载点，再兼容旧的模块挂载点
        FresnelFunction = TryLoadFresnelAtPath(TEXT("/XTools/MaterialFunctions/MF_SM_Fresnel"));
        if (!FresnelFunction)
        {
            FresnelFunction = TryLoadFresnelAtPath(TEXT("/X_AssetEditor/MaterialFunctions/MF_SM_Fresnel"));
        }
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

    // 使用官方库路径，确保依赖材质实例与视口同步刷新链路完整执行。
    UMaterialEditingLibrary::RecompileMaterial(Material);
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
    
    // 仅刷新已打开的编辑器，不主动打开新标签页，避免批处理时打断用户工作流。
    const auto OpenEditors = AssetEditorSubsystem->FindEditorsForAsset(Material);
    if (OpenEditors.Num() == 0)
    {
        return false;
    }

    const UX_AssetEditorSettings* Settings = GetDefault<UX_AssetEditorSettings>();
    const bool bEnableCloseReopenFallback = Settings && Settings->bEnableCloseReopenFallbackForMaterialRefresh;

    bool bRefreshedByGraphPath = false;

    // 对于插件侧直接改Expression数据的场景，优先重建Graph并触发材质编辑器刷新链路，
    // 避免仅PostEditChange导致节点/连线显示不同步。
    if (Material->MaterialGraph)
    {
        const TSharedPtr<IMaterialEditor> MaterialEditor = FMaterialEditorUtilities::GetIMaterialEditorForObject(Material->MaterialGraph);
        if (MaterialEditor.IsValid())
        {
            Material->MaterialGraph->RebuildGraph();
            FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(Material->MaterialGraph);
            FMaterialEditorUtilities::ForceRefreshExpressionPreviews(Material->MaterialGraph);
            FMaterialEditorUtilities::UpdateDetailView(Material->MaterialGraph);
            bRefreshedByGraphPath = true;
        }
        else
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("材质编辑器实例不可用，Graph刷新链路未执行: %s"), *Material->GetName());
        }
    }

    // Graph路径不可用时，退回通用重编译链路。
    if (!bRefreshedByGraphPath)
    {
        RecompileMaterial(Material);

        // 可选兜底：在刷新链路不可用时，执行一次关开编辑器。
        if (bEnableCloseReopenFallback)
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("执行材质编辑器关开兜底刷新: %s"), *Material->GetName());
            AssetEditorSubsystem->CloseAllEditorsForAsset(Material);
            AssetEditorSubsystem->OpenEditorForAsset(Material);
            return true;
        }
    }

    Material->MarkPackageDirty();
    UE_LOG(LogX_AssetEditor, Log, TEXT("材质编辑器已打开，已执行刷新（GraphPath=%s）"),
        bRefreshedByGraphPath ? TEXT("true") : TEXT("false"));
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
