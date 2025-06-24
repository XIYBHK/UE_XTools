// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuExtensions/X_MenuExtensionManager.h"
#include "AssetNaming/X_AssetNamingManager.h"
#include "MaterialTools/X_MaterialFunctionOperation.h"
#include "CollisionTools/X_CollisionManager.h"
#include "CollisionTools/X_CollisionSettingsDialog.h"

#include "ContentBrowserModule.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "X_MenuExtensionManager"

TUniquePtr<FX_MenuExtensionManager> FX_MenuExtensionManager::Instance = nullptr;

FX_MenuExtensionManager& FX_MenuExtensionManager::Get()
{
    if (!Instance.IsValid())
    {
        Instance = TUniquePtr<FX_MenuExtensionManager>(new FX_MenuExtensionManager());
    }
    return *Instance;
}

void FX_MenuExtensionManager::RegisterMenuExtensions()
{
    RegisterContentBrowserContextMenuExtender();
    RegisterLevelEditorContextMenuExtender();
}

void FX_MenuExtensionManager::UnregisterMenuExtensions()
{
    UnregisterContentBrowserContextMenuExtender();
    UnregisterLevelEditorContextMenuExtender();
}

void FX_MenuExtensionManager::RegisterContentBrowserContextMenuExtender()
{
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
    
    CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(this, &FX_MenuExtensionManager::OnExtendContentBrowserAssetSelectionMenu));
    ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FX_MenuExtensionManager::UnregisterContentBrowserContextMenuExtender()
{
    if (FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser"))
    {
        TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule->GetAllAssetViewContextMenuExtenders();
        CBMenuExtenderDelegates.RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { 
            return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; 
        });
    }
}

void FX_MenuExtensionManager::RegisterLevelEditorContextMenuExtender()
{
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    auto& LevelEditorMenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
    
    LevelEditorMenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FX_MenuExtensionManager::OnExtendLevelEditorActorContextMenu));
    LevelEditorExtenderDelegateHandle = LevelEditorMenuExtenders.Last().GetHandle();
}

void FX_MenuExtensionManager::UnregisterLevelEditorContextMenuExtender()
{
    if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
    {
        auto& LevelEditorMenuExtenders = LevelEditorModule->GetAllLevelViewportContextMenuExtenders();
        LevelEditorMenuExtenders.RemoveAll([this](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) { 
            return Delegate.GetHandle() == LevelEditorExtenderDelegateHandle; 
        });
    }
}

void FX_MenuExtensionManager::RegisterMenus()
{
    // 注册工具菜单
    UToolMenus* ToolMenus = UToolMenus::Get();
    if (!ToolMenus)
    {
        return;
    }

    // 这里可以添加主菜单项
    // 目前主要功能通过右键菜单提供
}

TSharedRef<FExtender> FX_MenuExtensionManager::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
    TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

    if (SelectedAssets.Num() > 0)
    {
        // 检查是否有资产需要重命名
        bool bHasAssetsToRename = SelectedAssets.Num() > 0;
        
        if (bHasAssetsToRename)
        {
            Extender->AddMenuExtension(
                "GetAssetActions",
                EExtensionHook::After,
                nullptr,
                FMenuExtensionDelegate::CreateRaw(this, &FX_MenuExtensionManager::AddAssetNamingMenuEntry, SelectedAssets)
            );
        }

        // 检查是否有材质相关资产
        bool bHasMaterialAssets = false;
        for (const FAssetData& Asset : SelectedAssets)
        {
            const FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
            if (AssetClassName == TEXT("Material") || 
                AssetClassName == TEXT("MaterialInstanceConstant") ||
                AssetClassName == TEXT("MaterialFunction"))
            {
                bHasMaterialAssets = true;
                break;
            }
        }

        if (bHasMaterialAssets)
        {
            Extender->AddMenuExtension(
                "GetAssetActions",
                EExtensionHook::After,
                nullptr,
                FMenuExtensionDelegate::CreateRaw(this, &FX_MenuExtensionManager::AddMaterialFunctionMenuEntry, SelectedAssets)
            );
        }

        // 检查是否有静态网格体资产
        bool bHasStaticMeshAssets = false;
        for (const FAssetData& Asset : SelectedAssets)
        {
            const FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
            if (AssetClassName == TEXT("StaticMesh"))
            {
                bHasStaticMeshAssets = true;
                break;
            }
        }

        if (bHasStaticMeshAssets)
        {
            Extender->AddMenuExtension(
                "GetAssetActions",
                EExtensionHook::After,
                nullptr,
                FMenuExtensionDelegate::CreateRaw(this, &FX_MenuExtensionManager::AddCollisionManagementMenuEntry, SelectedAssets)
            );
        }
    }

    return Extender;
}

TSharedRef<FExtender> FX_MenuExtensionManager::OnExtendLevelEditorActorContextMenu(TSharedRef<FUICommandList> CommandList, TArray<AActor*> SelectedActors)
{
    TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

    if (SelectedActors.Num() > 0)
    {
        // 检查是否有带材质的Actor
        bool bHasActorsWithMaterials = SelectedActors.Num() > 0; // 简化检查，实际可以更精确

        if (bHasActorsWithMaterials)
        {
            Extender->AddMenuExtension(
                "ActorControl",
                EExtensionHook::After,
                CommandList,
                FMenuExtensionDelegate::CreateRaw(this, &FX_MenuExtensionManager::AddActorMaterialMenuEntry, SelectedActors)
            );
        }
    }

    return Extender;
}

void FX_MenuExtensionManager::AddAssetNamingMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
    MenuBuilder.BeginSection("AssetNaming", LOCTEXT("AssetNaming", "资产命名"));
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("RenameAssets", "规范化资产命名"),
            LOCTEXT("RenameAssetsTooltip", "根据资产类型自动添加正确的前缀"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.Rename"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedAssets]()
                {
                    FX_AssetNamingManager::Get().RenameSelectedAssets();
                })
            )
        );
    }
    MenuBuilder.EndSection();
}

void FX_MenuExtensionManager::AddMaterialFunctionMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
    MenuBuilder.BeginSection("MaterialFunctions", LOCTEXT("MaterialFunctions", "材质函数"));
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("AddFresnelFunction", "添加菲涅尔函数"),
            LOCTEXT("AddFresnelFunctionTooltip", "为选中的材质添加菲涅尔效果"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.Apply"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedAssets]()
                {
                    // 将FAssetData转换为UObject*
                    TArray<UObject*> AssetObjects;
                    for (const FAssetData& AssetData : SelectedAssets)
                    {
                        if (UObject* AssetObject = AssetData.GetAsset())
                        {
                            AssetObjects.Add(AssetObject);
                        }
                    }
                    FX_MaterialFunctionOperation::AddFresnelToAssets(AssetObjects);
                })
            )
        );
    }
    MenuBuilder.EndSection();
}

void FX_MenuExtensionManager::AddCollisionManagementMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
    MenuBuilder.BeginSection("CollisionManagement", LOCTEXT("CollisionManagement", "碰撞管理"));
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("RemoveCollision", "移除碰撞"),
            LOCTEXT("RemoveCollisionTooltip", "移除选中静态网格体的所有碰撞"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.DeleteBody"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedAssets]()
                {
                    // 确认对话框
                    FText ConfirmText = FText::Format(
                        LOCTEXT("ConfirmRemoveCollision", "确定要移除 {0} 个资产的碰撞吗？\n\n此操作将删除所有简单碰撞形状，无法撤销。"),
                        FText::AsNumber(SelectedAssets.Num())
                    );

                    EAppReturnType::Type Result = FMessageDialog::Open(
                        EAppMsgType::YesNo,
                        ConfirmText,
                        LOCTEXT("RemoveCollisionTitle", "确认移除碰撞")
                    );

                    if (Result == EAppReturnType::Yes)
                    {
                        FX_CollisionManager::RemoveCollisionFromAssets(SelectedAssets);
                    }
                })
            )
        );

        MenuBuilder.AddMenuEntry(
            LOCTEXT("AddConvexCollision", "添加简单凸包碰撞"),
            LOCTEXT("AddConvexCollisionTooltip", "为选中静态网格体添加简单凸包碰撞"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.NewBody"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedAssets]()
                {
                    // 确认对话框
                    FText ConfirmText = FText::Format(
                        LOCTEXT("ConfirmAddConvexCollision", "确定要为 {0} 个资产添加凸包碰撞吗？\n\n此操作将先清除现有碰撞，然后添加新的凸包碰撞。"),
                        FText::AsNumber(SelectedAssets.Num())
                    );

                    EAppReturnType::Type Result = FMessageDialog::Open(
                        EAppMsgType::YesNo,
                        ConfirmText,
                        LOCTEXT("AddConvexCollisionTitle", "确认添加凸包碰撞")
                    );

                    if (Result == EAppReturnType::Yes)
                    {
                        FX_CollisionManager::AddConvexCollisionToAssets(SelectedAssets);
                    }
                })
            )
        );

        MenuBuilder.AddMenuEntry(
            LOCTEXT("SetCollisionComplexity", "设置碰撞复杂度"),
            LOCTEXT("SetCollisionComplexityTooltip", "批量设置选中静态网格体的碰撞复杂度"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.Properties"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedAssets]()
                {
                    SX_CollisionSettingsDialog::ShowDialog(SelectedAssets);
                })
            )
        );
    }
    MenuBuilder.EndSection();
}

void FX_MenuExtensionManager::AddActorMaterialMenuEntry(FMenuBuilder& MenuBuilder, TArray<AActor*> SelectedActors)
{
    MenuBuilder.BeginSection("ActorMaterials", LOCTEXT("ActorMaterials", "Actor材质"));
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT("AddFresnelToActors", "添加菲涅尔效果"),
            LOCTEXT("AddFresnelToActorsTooltip", "为选中Actor的材质添加菲涅尔效果"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.Apply"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedActors]()
                {
                    // 将AActor*转换为UObject*
                    TArray<UObject*> ActorObjects;
                    for (AActor* Actor : SelectedActors)
                    {
                        if (Actor)
                        {
                            ActorObjects.Add(Actor);
                        }
                    }
                    FX_MaterialFunctionOperation::AddFresnelToAssets(ActorObjects);
                })
            )
        );
    }
    MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE
