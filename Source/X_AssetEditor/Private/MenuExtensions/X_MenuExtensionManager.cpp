/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "MenuExtensions/X_MenuExtensionManager.h"
#include "AssetNaming/X_AssetNamingManager.h"
#include "MaterialTools/X_MaterialFunctionOperation.h"
#include "MaterialTools/X_MaterialFunctionManager.h"  //  添加：材质函数管理器
#include "MaterialTools/X_MaterialFunctionParamDialog.h"  //  添加：参数对话框
#include "MaterialTools/X_MaterialFunctionParams.h"  //  添加：参数结构体
#include "MaterialTools/X_MaterialFunctionUI.h"  //  添加：UI函数声明
#include "CollisionTools/X_CollisionManager.h"
#include "CollisionTools/X_CollisionSettingsDialog.h"
#include "CollisionTools/X_AutoConvexDialog.h"
#include "PivotTools/X_PivotManager.h"
#include "RawMesh.h"
#include "StaticMeshEditorSubsystem.h"
#include "StaticMeshEditorSubsystemHelpers.h"

#include "ContentBrowserModule.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "Misc/MessageDialog.h"
#include "X_AssetEditor.h"
#include "Engine/StaticMeshActor.h"

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

        // 检查是否有材质相关资产或可能包含材质的资产
        bool bHasMaterialAssets = false;
        for (const FAssetData& Asset : SelectedAssets)
        {
            const FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
            if (AssetClassName == TEXT("Material") ||
                AssetClassName == TEXT("MaterialInstanceConstant") ||
                AssetClassName == TEXT("StaticMesh") ||
                AssetClassName == TEXT("SkeletalMesh") ||
                AssetClassName.Contains(TEXT("Blueprint")))
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

            Extender->AddMenuExtension(
                "GetAssetActions",
                EExtensionHook::After,
                nullptr,
                FMenuExtensionDelegate::CreateRaw(this, &FX_MenuExtensionManager::AddPivotToolsMenuEntry, SelectedAssets)
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

        // 检查是否有 StaticMeshActor
        bool bHasStaticMeshActors = false;
        for (AActor* Actor : SelectedActors)
        {
            if (Actor && Actor->IsA<AStaticMeshActor>())
            {
                bHasStaticMeshActors = true;
                break;
            }
        }

        if (bHasStaticMeshActors)
        {
            Extender->AddMenuExtension(
                "ActorControl",
                EExtensionHook::After,
                CommandList,
                FMenuExtensionDelegate::CreateRaw(this, &FX_MenuExtensionManager::AddActorPivotToolsMenuEntry, SelectedActors)
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
        //  添加任意指定材质函数功能（完整工作流程）
        MenuBuilder.AddMenuEntry(
            LOCTEXT("AddCustomMaterialFunction", "添加材质函数"),
            LOCTEXT("AddCustomMaterialFunctionTooltip", "选择并配置材质函数参数，然后添加到选中资产的材质中\n支持：材质、材质实例、静态网格体、骨骼网格体、蓝图类"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.MaterialFunction"),
            FUIAction(
                FExecuteAction::CreateStatic(&FX_MenuExtensionManager::HandleAddMaterialFunctionToAssets, SelectedAssets)
            )
        );

        //  保留原有的菲涅尔函数快捷功能
        MenuBuilder.AddMenuEntry(
            LOCTEXT("AddFresnelFunction", "添加菲涅尔函数"),
            LOCTEXT("AddFresnelFunctionTooltip", "为选中资产的材质添加菲涅尔效果\n支持：材质、材质实例、静态网格体、骨骼网格体、蓝图类"),
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

        // 子菜单：添加简单碰撞（原生）
        MenuBuilder.AddSubMenu(
            LOCTEXT("AddSimpleCollision", "添加简单碰撞"),
            LOCTEXT("AddSimpleCollisionTooltip", "使用UE原生选项添加多种简单碰撞"),
            FNewMenuDelegate::CreateLambda([SelectedAssets](FMenuBuilder& SubMenu)
            {
                auto AddShapeEntry = [&SubMenu, SelectedAssets](const FText& Label, const FText& Tooltip, uint8 ShapeType)
                {
                    SubMenu.AddMenuEntry(
                        Label,
                        Tooltip,
                        FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.NewBody"),
                        FUIAction(FExecuteAction::CreateLambda([SelectedAssets, ShapeType]()
                        {
                            FX_CollisionManager::AddSimpleCollisionToAssets(SelectedAssets, ShapeType);
                        }))
                    );
                };

                // 盒、球、胶囊
                AddShapeEntry(LOCTEXT("AddBoxCollision", "添加盒体简化碰撞"), LOCTEXT("AddBoxCollisionTooltip", "添加盒体简化碰撞"), (uint8)EScriptCollisionShapeType::Box);
                AddShapeEntry(LOCTEXT("AddSphereCollision", "添加球体简化碰撞"), LOCTEXT("AddSphereCollisionTooltip", "添加球体简化碰撞"), (uint8)EScriptCollisionShapeType::Sphere);
                AddShapeEntry(LOCTEXT("AddCapsuleCollision", "添加胶囊简化碰撞"), LOCTEXT("AddCapsuleCollisionTooltip", "添加胶囊简化碰撞"), (uint8)EScriptCollisionShapeType::Capsule);

                SubMenu.AddSeparator();
                // KDOP 族
                AddShapeEntry(LOCTEXT("AddKDOP10X", "添加10DOP-X简化碰撞"), LOCTEXT("AddKDOP10XTooltip", "添加10DOP-X简化碰撞"), (uint8)EScriptCollisionShapeType::NDOP10_X);
                AddShapeEntry(LOCTEXT("AddKDOP10Y", "添加10DOP-Y简化碰撞"), LOCTEXT("AddKDOP10YTooltip", "添加10DOP-Y简化碰撞"), (uint8)EScriptCollisionShapeType::NDOP10_Y);
                AddShapeEntry(LOCTEXT("AddKDOP10Z", "添加10DOP-Z简化碰撞"), LOCTEXT("AddKDOP10ZTooltip", "添加10DOP-Z简化碰撞"), (uint8)EScriptCollisionShapeType::NDOP10_Z);
                AddShapeEntry(LOCTEXT("AddKDOP18", "添加18DOP简化碰撞"), LOCTEXT("AddKDOP18Tooltip", "添加18DOP简化碰撞"), (uint8)EScriptCollisionShapeType::NDOP18);
                AddShapeEntry(LOCTEXT("AddKDOP26", "添加26DOP简化碰撞"), LOCTEXT("AddKDOP26Tooltip", "添加26DOP简化碰撞"), (uint8)EScriptCollisionShapeType::NDOP26);

                SubMenu.AddSeparator();
                // 简单凸包（快速一键版，使用原生AutoConvex默认参数）
                SubMenu.AddMenuEntry(
                    LOCTEXT("AddSimpleConvexCollision", "添加凸包碰撞"),
                    LOCTEXT("AddSimpleConvexCollisionTip", "基于LOD0顶点一键生成凸包碰撞"),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.NewBody"),
                    FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
                    {
                        // 默认参数
                        const int32 HullCount = 4;
                        const int32 MaxHullVerts = 16;
                        const int32 HullPrecision = 100000;

                        TArray<UStaticMesh*> Meshes;
                        for (const FAssetData& AssetData : SelectedAssets)
                        {
                            if (UStaticMesh* SM = Cast<UStaticMesh>(AssetData.GetAsset()))
                            {
                                Meshes.Add(SM);
                            }
                        }
                        if (Meshes.Num() > 0)
                        {
                            if (UStaticMeshEditorSubsystem* Sys = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>())
                            {
                                Sys->BulkSetConvexDecompositionCollisionsWithNotification(Meshes, HullCount, MaxHullVerts, HullPrecision, true);
                            }
                        }
                    }))
                );

                SubMenu.AddSeparator();
                // 自动凸包（批量）移入此菜单并弹窗参数
                SubMenu.AddMenuEntry(
                    LOCTEXT("AutoConvexBulkInSub", "添加凸包碰撞(参数)"),
                    LOCTEXT("AutoConvexBulkInSubTip", "配置参数并批量生成凸包碰撞"),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.NewBody"),
                    FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
                    {
                        int32 HullCount = 4, MaxHullVerts = 16, HullPrecision = 100000;
                        if (SX_AutoConvexDialog::ShowDialog(HullCount, MaxHullVerts, HullPrecision, HullCount, MaxHullVerts, HullPrecision))
                        {
                            TArray<UStaticMesh*> Meshes;
                            for (const FAssetData& AssetData : SelectedAssets)
                            {
                                if (UStaticMesh* SM = Cast<UStaticMesh>(AssetData.GetAsset()))
                                {
                                    Meshes.Add(SM);
                                }
                            }
                            if (Meshes.Num() > 0)
                            {
                                if (UStaticMeshEditorSubsystem* Sys = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>())
                                {
                                    Sys->BulkSetConvexDecompositionCollisionsWithNotification(Meshes, HullCount, MaxHullVerts, HullPrecision, true);
                                }
                            }
                        }
                    }))
                );
            })
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

        // 顶层不再单列“自动凸包碰撞(批量)”
    }
    MenuBuilder.EndSection();
}

void FX_MenuExtensionManager::AddActorMaterialMenuEntry(FMenuBuilder& MenuBuilder, TArray<AActor*> SelectedActors)
{
    MenuBuilder.BeginSection("ActorMaterials", LOCTEXT("ActorMaterials", "Actor材质"));
    {
        //  添加任意指定材质函数功能（完整工作流程）
        MenuBuilder.AddMenuEntry(
            LOCTEXT("AddCustomMaterialFunctionToActors", "添加材质函数"),
            LOCTEXT("AddCustomMaterialFunctionToActorsTooltip", "选择并配置材质函数参数，然后添加到选中Actor的材质"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.MaterialFunction"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedActors]()
                {
                    //  修复：直接使用SelectedActors，与原代码一致
                    // 使用简单版选择器
                    FX_MaterialFunctionUI::CreateMaterialFunctionPickerWindow(
                        FOnMaterialFunctionSelected::CreateLambda([SelectedActors](UMaterialFunctionInterface* SelectedFunction)
                        {
                            if (SelectedFunction)
                            {
                                //  修复：创建参数结构体（与原代码方式一致）
                                FX_MaterialFunctionParams Params;

                                // 设置默认参数
                                Params.NodeName = SelectedFunction->GetName();
                                Params.PosX = -300;
                                Params.PosY = 0;
                                Params.bSetupConnections = true;
                                Params.ConnectionMode = EConnectionMode::Add;

                                // 根据函数名称自动设置连接选项
                                Params.SetupConnectionsByFunctionName(SelectedFunction->GetName());

                                //  修复：根据材质函数的输入输出引脚情况设置智能连接选项（与原代码一致）
                                int32 InputCount = 0;
                                int32 OutputCount = 0;
                                FX_MaterialFunctionOperation::GetFunctionInputOutputCount(SelectedFunction, InputCount, OutputCount);

                                // 只有同时具有输入和输出引脚的材质函数才默认启用智能连接
                                // 只有输出引脚的材质函数默认禁用智能连接
                                Params.bEnableSmartConnect = (InputCount > 0 && OutputCount > 0);

                                UE_LOG(LogX_AssetEditor, Log, TEXT("材质函数 %s: 输入引脚=%d, 输出引脚=%d, 智能连接=%s"),
                                    *SelectedFunction->GetName(), InputCount, OutputCount,
                                    Params.bEnableSmartConnect ? TEXT("启用") : TEXT("禁用"));

                                // 创建结构体包装器
                                TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FX_MaterialFunctionParams::StaticStruct(), (uint8*)&Params);

                                // 显示参数对话框
                                FText DialogTitle = FText::Format(LOCTEXT("MaterialFunctionParamDialogTitleForActors", "配置材质函数参数: {0}"),
                                    FText::FromString(SelectedFunction->GetName()));

                                if (SX_MaterialFunctionParamDialog::ShowDialog(DialogTitle, StructOnScope))
                                {
                                    //  修复：使用与原代码一致的调用方式
                                    FX_MaterialFunctionOperation::ProcessActorMaterialFunction(
                                        SelectedActors,
                                        SelectedFunction,
                                        FName(*Params.NodeName),
                                        MakeShared<FX_MaterialFunctionParams>(Params)
                                    );
                                }
                            }
                        })
                    );
                })
            )
        );

        //  保留原有的菲涅尔函数快捷功能
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

void FX_MenuExtensionManager::HandleAddMaterialFunctionToAssets(TArray<FAssetData> SelectedAssets)
{
    // 使用简单版材质函数选择器
    FX_MaterialFunctionUI::CreateMaterialFunctionPickerWindow(
        FOnMaterialFunctionSelected::CreateStatic(&FX_MenuExtensionManager::OnMaterialFunctionSelected, SelectedAssets)
    );
}

void FX_MenuExtensionManager::OnMaterialFunctionSelected(UMaterialFunctionInterface* SelectedFunction, TArray<FAssetData> SelectedAssets)
{
    if (!SelectedFunction)
    {
        return;
    }

    // 创建参数结构体
    FX_MaterialFunctionParams Params;
    Params.NodeName = SelectedFunction->GetName();
    Params.PosX = -300;
    Params.PosY = 0;
    Params.bSetupConnections = true;
    Params.ConnectionMode = EConnectionMode::Add;

    // 根据函数名称自动设置连接选项
    Params.SetupConnectionsByFunctionName(SelectedFunction->GetName());

    // 根据材质函数的输入输出引脚情况设置智能连接选项
    int32 InputCount = 0;
    int32 OutputCount = 0;
    FX_MaterialFunctionOperation::GetFunctionInputOutputCount(SelectedFunction, InputCount, OutputCount);

    // 只有同时具有输入和输出引脚的材质函数才默认启用智能连接
    Params.bEnableSmartConnect = (InputCount > 0 && OutputCount > 0);

    UE_LOG(LogX_AssetEditor, Log, TEXT("材质函数 %s: 输入引脚=%d, 输出引脚=%d, 智能连接=%s"),
        *SelectedFunction->GetName(), InputCount, OutputCount,
        Params.bEnableSmartConnect ? TEXT("启用") : TEXT("禁用"));

    // 创建结构体包装器
    TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FX_MaterialFunctionParams::StaticStruct(), (uint8*)&Params);

    // 显示参数对话框
    FText DialogTitle = FText::Format(LOCTEXT("MaterialFunctionParamDialogTitle", "配置材质函数参数: {0}"),
        FText::FromString(SelectedFunction->GetName()));

    if (SX_MaterialFunctionParamDialog::ShowDialog(DialogTitle, StructOnScope))
    {
        // 使用与原代码一致的调用方式
        FX_MaterialFunctionOperation::ProcessAssetMaterialFunction(
            SelectedAssets,
            SelectedFunction,
            FName(*Params.NodeName),
            MakeShared<FX_MaterialFunctionParams>(Params)
        );
    }
}

void FX_MenuExtensionManager::AddPivotToolsMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
    MenuBuilder.BeginSection("PivotTools", LOCTEXT("PivotTools", "枢轴工具"));
    {
        // 记录 Pivot
        MenuBuilder.AddMenuEntry(
            LOCTEXT("RecordPivot", "记录 Pivot"),
            LOCTEXT("RecordPivotTooltip", "记录选中网格的当前 Pivot 状态，用于后续还原"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
            FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
            {
                FX_PivotManager::RecordPivotSnapshots(SelectedAssets);
            }))
        );

        // 还原 Pivot
        MenuBuilder.AddMenuEntry(
            LOCTEXT("RestorePivot", "还原 Pivot"),
            FText::Format(LOCTEXT("RestorePivotTooltip", "还原之前记录的 Pivot 状态\n当前有 {0} 个快照"),
                FText::AsNumber(FX_PivotManager::GetSnapshotCount())),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedAssets]()
                {
                    FX_PivotManager::RestorePivotSnapshots(SelectedAssets);
                }),
                FCanExecuteAction::CreateLambda([]()
                {
                    return FX_PivotManager::GetSnapshotCount() > 0;
                })
            )
        );

        // 清除快照
        MenuBuilder.AddMenuEntry(
            LOCTEXT("ClearPivotSnapshots", "清除所有快照"),
            FText::Format(LOCTEXT("ClearPivotSnapshotsTooltip", "清除所有已记录的 Pivot 快照\n当前有 {0} 个快照"),
                FText::AsNumber(FX_PivotManager::GetSnapshotCount())),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
            FUIAction(
                FExecuteAction::CreateLambda([]()
                {
                    FX_PivotManager::ClearPivotSnapshots();
                }),
                FCanExecuteAction::CreateLambda([]()
                {
                    return FX_PivotManager::GetSnapshotCount() > 0;
                })
            )
        );

        MenuBuilder.AddSeparator();

        MenuBuilder.AddSubMenu(
            LOCTEXT("SetPivot", "设置 Pivot"),
            LOCTEXT("SetPivotTooltip", "批量设置选中静态网格体的Pivot位置"),
            FNewMenuDelegate::CreateLambda([SelectedAssets](FMenuBuilder& SubMenu)
            {
                // 快速设置到中心
                SubMenu.AddMenuEntry(
                    LOCTEXT("SetPivotToCenter", "设置到中心"),
                    LOCTEXT("SetPivotToCenterTooltip", "将Pivot设置到边界盒中心"),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.TranslateMode"),
                    FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
                    {
                        FX_PivotManager::SetPivotToCenterForAssets(SelectedAssets);
                    }))
                );

                SubMenu.AddSeparator();

                // 其他位置选项
                auto AddPivotOption = [&SubMenu, SelectedAssets](
                    const FText& Label,
                    EPivotBoundsPoint BoundsPoint)
                {
                    SubMenu.AddMenuEntry(
                        Label,
                        FText::Format(LOCTEXT("SetPivotToBoundsPointTooltip", "将Pivot设置到{0}"), Label),
                        FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.TranslateMode"),
                        FUIAction(FExecuteAction::CreateLambda([SelectedAssets, BoundsPoint]()
                        {
                            FX_PivotManager::SetPivotForAssets(SelectedAssets, BoundsPoint);
                        }))
                    );
                };

                AddPivotOption(LOCTEXT("SetPivotToBottom", "设置到底部中心"), EPivotBoundsPoint::Bottom);
                AddPivotOption(LOCTEXT("SetPivotToTop", "设置到顶部中心"), EPivotBoundsPoint::Top);
                AddPivotOption(LOCTEXT("SetPivotToLeft", "设置到左面中心"), EPivotBoundsPoint::Left);
                AddPivotOption(LOCTEXT("SetPivotToRight", "设置到右面中心"), EPivotBoundsPoint::Right);
                AddPivotOption(LOCTEXT("SetPivotToFront", "设置到前面中心"), EPivotBoundsPoint::Front);
                AddPivotOption(LOCTEXT("SetPivotToBack", "设置到后面中心"), EPivotBoundsPoint::Back);
            })
        );
    }
    MenuBuilder.EndSection();
}

void FX_MenuExtensionManager::AddActorPivotToolsMenuEntry(FMenuBuilder& MenuBuilder, TArray<AActor*> SelectedActors)
{
    MenuBuilder.BeginSection("ActorPivotTools", LOCTEXT("ActorPivotTools", "Actor枢轴工具"));
    {
        // 提取 Actor 使用的 StaticMesh 资产
        auto GetStaticMeshAssets = [](const TArray<AActor*>& Actors) -> TArray<FAssetData>
        {
            TArray<FAssetData> Assets;
            TSet<UStaticMesh*> ProcessedMeshes;
            
            for (AActor* Actor : Actors)
            {
                if (AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor))
                {
                    if (UStaticMeshComponent* MeshComp = SMActor->GetStaticMeshComponent())
                    {
                        if (UStaticMesh* Mesh = MeshComp->GetStaticMesh())
                        {
                            if (!ProcessedMeshes.Contains(Mesh))
                            {
                                ProcessedMeshes.Add(Mesh);
                                Assets.Add(FAssetData(Mesh));
                            }
                        }
                    }
                }
            }
            return Assets;
        };

        // 记录 Pivot
        MenuBuilder.AddMenuEntry(
            LOCTEXT("RecordActorPivot", "记录 Pivot"),
            LOCTEXT("RecordActorPivotTooltip", "记录选中Actor使用的网格的当前 Pivot 状态"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
            FUIAction(FExecuteAction::CreateLambda([SelectedActors, GetStaticMeshAssets]()
            {
                TArray<FAssetData> Assets = GetStaticMeshAssets(SelectedActors);
                FX_PivotManager::RecordPivotSnapshots(Assets);
            }))
        );

        // 还原 Pivot
        MenuBuilder.AddMenuEntry(
            LOCTEXT("RestoreActorPivot", "还原 Pivot"),
            FText::Format(LOCTEXT("RestoreActorPivotTooltip", "还原之前记录的 Pivot 状态（保持Actor位置）\n当前有 {0} 个快照"),
                FText::AsNumber(FX_PivotManager::GetSnapshotCount())),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
            FUIAction(
                FExecuteAction::CreateLambda([SelectedActors]()
                {
                    FX_PivotManager::RestorePivotSnapshotsForActors(SelectedActors);
                }),
                FCanExecuteAction::CreateLambda([]()
                {
                    return FX_PivotManager::GetSnapshotCount() > 0;
                })
            )
        );

        MenuBuilder.AddSeparator();

        MenuBuilder.AddSubMenu(
            LOCTEXT("SetActorPivot", "设置 Pivot"),
            LOCTEXT("SetActorPivotTooltip", "批量设置选中Actor的Pivot位置（保持世界位置）"),
            FNewMenuDelegate::CreateLambda([SelectedActors](FMenuBuilder& SubMenu)
            {
                // 快速设置到中心
                SubMenu.AddMenuEntry(
                    LOCTEXT("SetActorPivotToCenter", "设置到中心"),
                    LOCTEXT("SetActorPivotToCenterTooltip", "将Pivot设置到边界盒中心"),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.TranslateMode"),
                    FUIAction(FExecuteAction::CreateLambda([SelectedActors]()
                    {
                        FX_PivotManager::SetPivotToCenterForActors(SelectedActors);
                    }))
                );

                SubMenu.AddSeparator();

                // 其他位置选项
                auto AddPivotOption = [&SubMenu, SelectedActors](
                    const FText& Label,
                    EPivotBoundsPoint BoundsPoint)
                {
                    SubMenu.AddMenuEntry(
                        Label,
                        FText::Format(LOCTEXT("SetActorPivotToBoundsPointTooltip", "将Pivot设置到{0}"), Label),
                        FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.TranslateMode"),
                        FUIAction(FExecuteAction::CreateLambda([SelectedActors, BoundsPoint]()
                        {
                            FX_PivotManager::SetPivotForActors(SelectedActors, BoundsPoint);
                        }))
                    );
                };

                AddPivotOption(LOCTEXT("SetActorPivotToBottom", "设置到底部中心"), EPivotBoundsPoint::Bottom);
                AddPivotOption(LOCTEXT("SetActorPivotToTop", "设置到顶部中心"), EPivotBoundsPoint::Top);
                AddPivotOption(LOCTEXT("SetActorPivotToLeft", "设置到左面中心"), EPivotBoundsPoint::Left);
                AddPivotOption(LOCTEXT("SetActorPivotToRight", "设置到右面中心"), EPivotBoundsPoint::Right);
                AddPivotOption(LOCTEXT("SetActorPivotToFront", "设置到前面中心"), EPivotBoundsPoint::Front);
                AddPivotOption(LOCTEXT("SetActorPivotToBack", "设置到后面中心"), EPivotBoundsPoint::Back);

                SubMenu.AddSeparator();

                // 世界原点
                SubMenu.AddMenuEntry(
                    LOCTEXT("SetActorPivotToWorldOrigin", "设置到世界原点"),
                    LOCTEXT("SetActorPivotToWorldOriginTooltip", "将Pivot设置到世界原点(0,0,0)"),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Viewport.TranslateMode"),
                    FUIAction(FExecuteAction::CreateLambda([SelectedActors]()
                    {
                        FX_PivotManager::SetPivotForActors(SelectedActors, EPivotBoundsPoint::WorldOrigin);
                    }))
                );
            })
        );
    }
    MenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE
