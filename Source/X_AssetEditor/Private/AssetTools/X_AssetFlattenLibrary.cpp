/* Copyright (c) 2025 XIYBHK. Licensed under UE_XTools License. */
#include "AssetTools/X_AssetFlattenLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "Misc/App.h"
#include "XToolsVersionCompat.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/TopLevelAssetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "X_AssetEditor.h"
#include "XToolsErrorReporter.h"

#define LOCTEXT_NAMESPACE "X_AssetFlatten"
namespace XAssetFlatten
{
    struct FEntry { FAssetData Data; FString Destination; };
    bool bMoving = false;

    FString TypeFolder(const FAssetData& Data, IAssetRegistry& Registry)
    {
        // Match actual class ancestry, including optional plugin classes, without loading assets.
        TArray<FTopLevelAssetPath> Classes;
        Registry.GetAncestorClassNames(Data.AssetClassPath, Classes);
        Classes.Add(Data.AssetClassPath);
        struct FRule { const TCHAR* ClassPath; const TCHAR* Folder; };
        static const FRule Rules[] = {
            {TEXT("/Script/UMGEditor.WidgetBlueprint"), TEXT("UI")},
            {TEXT("/Script/Engine.AnimBlueprint"), TEXT("Animations/Blueprints")},
            {TEXT("/Script/Engine.Blueprint"), TEXT("Blueprints")},
            {TEXT("/Script/Engine.MaterialInstance"), TEXT("Materials/Instances")},
            {TEXT("/Script/Engine.MaterialFunctionInterface"), TEXT("Materials/Functions")},
            {TEXT("/Script/Engine.MaterialInterface"), TEXT("Materials")},
            {TEXT("/Script/Engine.Texture"), TEXT("Textures")},
            {TEXT("/Script/Engine.SkeletalMesh"), TEXT("Meshes/Skeletal")},
            {TEXT("/Script/Engine.StaticMesh"), TEXT("Meshes/Static")},
            {TEXT("/Script/Engine.Skeleton"), TEXT("Animations/Skeletons")},
            {TEXT("/Script/Engine.AnimationAsset"), TEXT("Animations")},
            {TEXT("/Script/Engine.PhysicsAsset"), TEXT("Physics")},
            {TEXT("/Script/PhysicsCore.PhysicalMaterial"), TEXT("Physics")},
            {TEXT("/Script/Engine.SoundBase"), TEXT("Audio")},
            {TEXT("/Script/Engine.SoundClass"), TEXT("Audio")},
            {TEXT("/Script/Engine.SoundMix"), TEXT("Audio")},
            {TEXT("/Script/Niagara.NiagaraSystem"), TEXT("FX")},
            {TEXT("/Script/Niagara.NiagaraEmitter"), TEXT("FX")},
            {TEXT("/Script/Niagara.NiagaraScript"), TEXT("FX")},
            {TEXT("/Script/Engine.ParticleSystem"), TEXT("FX")},
            {TEXT("/Script/LevelSequence.LevelSequence"), TEXT("Cinematics")},
            {TEXT("/Script/Engine.DataTable"), TEXT("Data")},
            {TEXT("/Script/Engine.CurveTable"), TEXT("Data")},
            {TEXT("/Script/Engine.DataAsset"), TEXT("Data")},
            {TEXT("/Script/Engine.CurveBase"), TEXT("Curves")},
            {TEXT("/Script/Engine.UserDefinedEnum"), TEXT("Blueprints/Enums")},
            {TEXT("/Script/CoreUObject.UserDefinedStruct"), TEXT("Blueprints/Structs")},
            {TEXT("/Script/Engine.Font"), TEXT("UI/Fonts")},
            {TEXT("/Script/Engine.FontFace"), TEXT("UI/Fonts")}
        };
        for (const FRule& Rule : Rules)
        {
            if (Classes.Contains(FTopLevelAssetPath(Rule.ClassPath))) { return Rule.Folder; }
        }
        return TEXT("Other");
    }

    bool BuildPlan(const TArray<FAssetData>& Assets, FString& Folder, TArray<FEntry>& Entries, FX_AssetFlattenResult& Result, bool bOrganizeByType = false)
    {
        Folder.TrimStartAndEndInline();
        while (Folder.EndsWith(TEXT("/"))) { Folder.LeftChopInline(1); }
        if (!(Folder == TEXT("/Game") || Folder.StartsWith(TEXT("/Game/"))) ||
            !FPackageName::IsValidLongPackageName(Folder / TEXT("Asset")))
        {
            Result.Errors.Add(TEXT("目标必须是 /Game 下的有效内容目录，例如 /Game/Gathered。"));
            return false;
        }
        if (Assets.IsEmpty())
        {
            Result.Errors.Add(TEXT("没有选中资产。"));
            return false;
        }
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        Registry.SearchAllAssets(true);
        TArray<FName> Queue;
        TSet<FName> Seen;
        for (const FAssetData& Data : Assets)
        {
            if (!Data.IsValid() || !Data.PackageName.ToString().StartsWith(TEXT("/Game/")) || Data.IsRedirector())
            {
                Result.Errors.Add(FString::Printf(TEXT("请选择 /Game 下的普通资产：%s"), *Data.GetObjectPathString()));
                continue;
            }
            if (!Seen.Contains(Data.PackageName)) { Seen.Add(Data.PackageName); Queue.Add(Data.PackageName); }
        }
        // Package + empty query includes hard/soft and game/editor-only; excludes management/searchable names.
        for (int32 Index = 0; Index < Queue.Num(); ++Index)
        {
            const FName PackageName = Queue[Index];
            const FString Package = PackageName.ToString();
            UPackage* LoadedPackage = FindPackage(nullptr, *Package);
            FString SourceFile;
            if (!FPackageName::DoesPackageExist(Package, &SourceFile) || (LoadedPackage && LoadedPackage->IsDirty()))
            {
                Result.Errors.Add(FString::Printf(TEXT("请先保存资产，再收集磁盘依赖：%s"), *Package));
                continue;
            }
            // Save/rename notifications are asynchronous. Query fresh disk dependencies even within the same frame.
            Registry.ScanModifiedAssetFiles({SourceFile});
            TArray<FAssetData> PackageAssets;
            Registry.GetAssetsByPackageName(PackageName, PackageAssets);
            if (PackageAssets.Num() != 1 ||
                (PackageAssets.Num() == 1 && (PackageAssets[0].PackageFlags & PKG_ContainsMap) != 0) ||
                Package.Contains(TEXT("/__ExternalActors__/")) || Package.Contains(TEXT("/__ExternalObjects__/")))
            {
                Result.Errors.Add(FString::Printf(TEXT("不支持地图、外部对象或非单资产包：%s"), *Package));
                continue;
            }
            const FAssetData& Data = PackageAssets[0];
            // Traverse existing redirectors to their destinations; never rename redirectors as ordinary assets.
            if (!Data.IsRedirector())
            {
                const FString DestinationFolder = bOrganizeByType ? Folder / TypeFolder(Data, Registry) : Folder;
                Entries.Add({Data, DestinationFolder / Data.AssetName.ToString()});
            }
            TArray<FName> Dependencies;
            if (!Registry.GetDependencies(PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package))
            {
                Result.Errors.Add(FString::Printf(TEXT("无法读取包依赖：%s"), *Package));
                continue;
            }
            Dependencies.Sort(FNameLexicalLess());
            for (FName Dependency : Dependencies)
            {
                if (Dependency.ToString().StartsWith(TEXT("/Game/")) && !Seen.Contains(Dependency))
                {
                    Seen.Add(Dependency); Queue.Add(Dependency);
                }
            }
        }
        Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Data.PackageName.LexicalLess(B.Data.PackageName); });
        Result.CollectedCount = Entries.Num();
        TMap<FName, FName> Destinations;
        for (const FEntry& Entry : Entries)
        {
            const FName Destination(*Entry.Destination);
            if (const FName* Other = Destinations.Find(Destination))
            {
                Result.Errors.Add(FString::Printf(TEXT("同名冲突：%s 与 %s -> %s"), *Other->ToString(), *Entry.Data.PackageName.ToString(), *Entry.Destination));
            }
            Destinations.Add(Destination, Entry.Data.PackageName);
            if (Destination == Entry.Data.PackageName) { ++Result.SkippedCount; continue; }
            TArray<FAssetData> Existing;
            Registry.GetAssetsByPackageName(Destination, Existing);
            if (!Existing.IsEmpty() || FindPackage(nullptr, *Entry.Destination) || FPackageName::DoesPackageExist(Entry.Destination))
            {
                Result.Errors.Add(FString::Printf(TEXT("目标已存在（包括重定向器）：%s"), *Entry.Destination));
            }
        }
        return Result.Errors.IsEmpty();
    }
}

FX_AssetFlattenResult UX_AssetFlattenLibrary::FlattenSelectedAssets(const FString& TargetFolder)
{
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Assets);
    return FlattenAssets(Assets, TargetFolder);
}

FX_AssetFlattenResult UX_AssetFlattenLibrary::FlattenAssets(const TArray<FAssetData>& Assets, const FString& TargetFolder)
{
    return MoveAssets(Assets, TargetFolder, false);
}

FX_AssetFlattenResult UX_AssetFlattenLibrary::OrganizeAssets(const TArray<FAssetData>& Assets, const FString& TargetFolder)
{
    return MoveAssets(Assets, TargetFolder, true);
}

FX_AssetFlattenResult UX_AssetFlattenLibrary::OrganizeSelectedAssets(const FString& TargetFolder)
{
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Assets);
    return OrganizeAssets(Assets, TargetFolder);
}

FX_AssetFlattenResult UX_AssetFlattenLibrary::MoveAssets(const TArray<FAssetData>& Assets, const FString& TargetFolder, bool bOrganizeByType)
{
    FX_AssetFlattenResult Result;
    if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || XAssetFlatten::bMoving)
    {
        Result.Errors.Add(TEXT("只能在编辑器游戏线程、非 PIE 且没有正在执行的资产移动时操作。"));
        return Result;
    }
    TGuardValue<bool> Guard(XAssetFlatten::bMoving, true);
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    if (AssetTools.IsFixupReferencersInProgress())
    {
        Result.Errors.Add(TEXT("请等待当前重定向器修复完成。"));
        return Result;
    }
    FString Folder = TargetFolder;
    TArray<XAssetFlatten::FEntry> Entries;
    if (!XAssetFlatten::BuildPlan(Assets, Folder, Entries, Result, bOrganizeByType)) { return Result; }
    TArray<TStrongObjectPtr<UObject>> KeepAlive;
    TArray<FAssetRenameData> Renames;
    TArray<FString> OldPaths;
    for (const XAssetFlatten::FEntry& Entry : Entries)
    {
        if (Entry.Data.PackageName == FName(*Entry.Destination)) { continue; }
        UObject* Asset = Entry.Data.GetAsset();
        if (!IsValid(Asset) || Asset->IsA<UWorld>() || Asset->GetOutermost()->IsDirty() ||
            Asset->GetOutermost()->GetFName() != Entry.Data.PackageName)
        {
            Result.Errors.Add(FString::Printf(TEXT("加载失败或加载后包状态改变，请保存并重试：%s"), *Entry.Data.GetObjectPathString()));
            continue;
        }
        KeepAlive.Emplace(Asset);
        Renames.Emplace(Asset, FPackageName::GetLongPackagePath(Entry.Destination), Entry.Data.AssetName.ToString());
        OldPaths.Add(Entry.Data.GetObjectPathString());
    }
    if (!Result.Errors.IsEmpty()) { return Result; } // Preflight all assets before the first mutation.
    if (Renames.IsEmpty()) { Result.bSuccess = true; return Result; }
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    TSet<FName> PackagesToRefresh;
    for (const FString& OldPath : OldPaths)
    {
        const FName OldPackage(*FPackageName::ObjectPathToPackageName(OldPath));
        PackagesToRefresh.Add(OldPackage);
        TArray<FName> Referencers;
        Registry.GetReferencers(OldPackage, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
        PackagesToRefresh.Append(Referencers);
    }
    const bool bRenamed = AssetTools.RenameAssets(Renames);
    TArray<UObjectRedirector*> Redirectors;
    bool bSaved = true;
    for (int32 Index = 0; Index < Renames.Num(); ++Index)
    {
        UObject* Asset = KeepAlive[Index].Get();
        const FString Expected = Renames[Index].NewPackagePath / Renames[Index].NewName;
        if (Asset->GetOutermost()->GetName() != Expected)
        {
            Result.Errors.Add(FString::Printf(TEXT("未移动：%s"), *OldPaths[Index]));
            continue;
        }
        ++Result.MovedCount;
        PackagesToRefresh.Add(Asset->GetOutermost()->GetFName());
        // RenameAssets' bool does not report every save failure. Never fix redirects for unsaved moves.
        if (Asset->GetOutermost()->IsDirty() || !FPackageName::DoesPackageExist(Expected))
        {
            bSaved = false;
            Result.Errors.Add(FString::Printf(TEXT("移动后的包未成功保存，保留重定向器：%s"), *Expected));
        }
        if (UObjectRedirector* Redirector = FindObject<UObjectRedirector>(nullptr, *OldPaths[Index]))
        {
            Redirectors.Add(Redirector);
        }
    }
    // Rename notifications can temporarily remap the registry's dependency nodes even for unloaded maps.
    // Restore the actual saved graph before FixupReferencers queries old-package referencers.
    TArray<FString> FilesToRefresh;
    for (FName PackageName : PackagesToRefresh)
    {
        FString Filename;
        if (FPackageName::DoesPackageExist(PackageName.ToString(), &Filename)) { FilesToRefresh.Add(Filename); }
    }
    Registry.ScanModifiedAssetFiles(FilesToRefresh);
    if (bSaved && !Redirectors.IsEmpty())
    {
#if XTOOLS_ENGINE_5_4_OR_LATER
        // UE 5.4+ always opens a report, even with DeleteFixedUpRedirectors. Cancelling that
        // modal in unattended script mode can assert inside SModalEditorDialog::ShowModalDialog.
        if (FApp::IsUnattended() || GIsRunningUnattendedScript || IsRunningCommandlet())
        {
            Result.Errors.Add(TEXT("资产已移动；此引擎版本的重定向器修复需要交互窗口。已保留重定向器，请在内容浏览器中修复引用。"));
        }
        else
#endif
        {
            AssetTools.FixupReferencers(Redirectors, true, ERedirectFixupMode::DeleteFixedUpRedirectors);
        }
    }
    for (const FString& OldPath : OldPaths)
    {
        const FAssetData Data = Registry.GetAssetByObjectPath(FSoftObjectPath(OldPath));
        if (Data.IsValid() && Data.IsRedirector()) { Result.RemainingRedirectors.Add(OldPath); }
    }
    // Native rename/fixup APIs do not return every referencing-package save failure.
    // Do not report completion while a moved package or a known referencer is still dirty.
    for (FName PackageName : PackagesToRefresh)
    {
        if (UPackage* Package = FindPackage(nullptr, *PackageName.ToString()))
        {
            if (Package->IsDirty())
            {
                Result.Errors.Add(FString::Printf(TEXT("关联包仍有未保存修改，无法确认引用已落盘，请保存后检查：%s"), *PackageName.ToString()));
            }
        }
    }
    if (!bRenamed) { Result.Errors.Add(TEXT("引擎批量重命名未全部成功；已移动项目不会自动回滚。")); }
    Result.bSuccess = bRenamed && Result.Errors.IsEmpty() && Result.RemainingRedirectors.IsEmpty();
    return Result;
}

void UX_AssetFlattenLibrary::ShowDialog(const TArray<FAssetData>& Assets, bool bOrganizeByType)
{
    TSharedRef<SWindow> Window = SNew(SWindow).Title(bOrganizeByType ? LOCTEXT("OrganizeTitle", "按类型移动资产及依赖") : LOCTEXT("Title", "扁平移动资产及依赖"))
        .ClientSize(FVector2D(620, 200)).SupportsMinimize(false).SupportsMaximize(false);
    TSharedRef<SEditableTextBox> PathBox = SNew(SEditableTextBox).Text(FText::FromString(TEXT("/Game/Gathered")));
    const TWeakPtr<SWindow> WeakWindow = Window;
    Window->SetContent(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(12)
        [SNew(STextBlock).AutoWrapText(true).Text(bOrganizeByType
            ? LOCTEXT("OrganizeHelp", "移动选中资产及 /Game 硬软依赖，按类型放入 Blueprints、Materials、Meshes、Textures 等子目录，其他类型放入 Other。请先保存资产；目标路径冲突会阻止移动。")
            : LOCTEXT("Help", "移动选中资产及 /Game 硬软依赖。请先保存资产；同名冲突会阻止移动。"))]
        + SVerticalBox::Slot().AutoHeight().Padding(12, 0)[PathBox]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(12)
        [SNew(SButton).Text(LOCTEXT("Move", "检查并移动")).OnClicked_Lambda([Assets, PathBox, WeakWindow, bOrganizeByType]()
        {
            FString Folder = PathBox->GetText().ToString();
            TArray<XAssetFlatten::FEntry> Entries;
            FX_AssetFlattenResult Preview;
            if (!XAssetFlatten::BuildPlan(Assets, Folder, Entries, Preview, bOrganizeByType))
            {
                FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Join(Preview.Errors, TEXT("\n"))));
                return FReply::Handled();
            }
            FString Question = FString::Printf(TEXT("共收集 %d 个资产，%d 个需要移动到 %s。\n"), Preview.CollectedCount, Preview.CollectedCount - Preview.SkippedCount, *Folder);
            if (bOrganizeByType)
            {
                TMap<FString, int32> Counts;
                for (const XAssetFlatten::FEntry& Entry : Entries) { ++Counts.FindOrAdd(FPackageName::GetLongPackagePath(Entry.Destination)); }
                TArray<FString> Folders;
                Counts.GetKeys(Folders);
                Folders.Sort();
                for (const FString& DestinationFolder : Folders) { Question += FString::Printf(TEXT("%s：%d\n"), *DestinationFolder, Counts[DestinationFolder]); }
            }
            Question += TEXT("共享依赖也会移动，其他引用者由引擎更新。此操作会保存资产，不能依赖普通撤销恢复。\n是否执行？");
            if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Question)) != EAppReturnType::Yes) { return FReply::Handled(); }
            const FX_AssetFlattenResult Result = MoveAssets(Assets, Folder, bOrganizeByType);
            FString Message = FString::Printf(TEXT("%s\n收集 %d，已移动 %d，已在目标目录 %d，遗留重定向器 %d。"),
                Result.bSuccess ? TEXT("完成") : TEXT("未全部完成"), Result.CollectedCount, Result.MovedCount, Result.SkippedCount, Result.RemainingRedirectors.Num());
            Message += TEXT("\n") + FString::Join(Result.Errors, TEXT("\n"));
            Message += TEXT("\n") + FString::Join(Result.RemainingRedirectors, TEXT("\n"));
            if (!Result.bSuccess) { XTOOLS_LOG_WARNING(LogX_AssetEditor, Message); }
            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
            if (Result.bSuccess)
            {
                if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin()) { Pinned->RequestDestroyWindow(); }
            }
            return FReply::Handled();
        })]);
    FSlateApplication::Get().AddModalWindow(Window, nullptr);
}
#undef LOCTEXT_NAMESPACE
