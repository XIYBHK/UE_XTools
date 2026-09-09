/* Copyright (c) 2025 XIYBHK. Licensed under UE_XTools License. */
#include "AssetTools/X_AssetFlattenLibrary.h"
#include "AssetTools/X_AssetFlattenTestAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "PackageTools.h"
#include "Engine/World.h"
#include "UObject/ObjectRedirector.h"
#include "XToolsVersionCompat.h"
#include "Misc/App.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Texture2D.h"
#include "UObject/ObjectSaveContext.h"
#include "Misc/FileHelper.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace XAssetFlattenTests
{
    struct FFixture
    {
        FString Root = TEXT("/Game/__XToolsFlattenTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        TArray<TStrongObjectPtr<UObject>> Assets;
        TArray<FString> ExtraPackages;

        UX_AssetFlattenTestAsset* Make(const FString& Relative)
        {
            return MakeTyped<UX_AssetFlattenTestAsset>(Relative);
        }

        template<typename T>
        T* MakeTyped(const FString& Relative)
        {
            const FString PackageName = Root / Relative;
            UPackage* Package = CreatePackage(*PackageName);
            T* Asset = NewObject<T>(Package,
                *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone);
            Assets.Emplace(Asset);
            ExtraPackages.Add(PackageName);
            FAssetRegistryModule::AssetCreated(Asset);
            return Asset;
        }

        bool Save()
        {
            TArray<FString> Files;
            for (const auto& Asset : Assets)
            {
                const FString File = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
                FSavePackageArgs Args;
                Args.TopLevelFlags = RF_Public | RF_Standalone;
                Args.SaveFlags = SAVE_NoError;
                if (!UPackage::SavePackage(Asset->GetOutermost(), Asset.Get(), *File, Args)) { return false; }
                Files.Add(File);
            }
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().ScanModifiedAssetFiles(Files);
            return true;
        }

        ~FFixture()
        {
            // Only this GUID-owned fixture directory is ever removed.
            TArray<UPackage*> Packages;
            for (const auto& Asset : Assets)
            {
                if (Asset.IsValid()) { Packages.AddUnique(Asset->GetOutermost()); }
            }
            for (const FString& PackageName : ExtraPackages)
            {
                if (UPackage* Package = FindPackage(nullptr, *PackageName)) { Packages.AddUnique(Package); }
            }
            Assets.Empty();
            for (UPackage* Package : Packages) { Package->SetDirtyFlag(false); }
            UPackageTools::UnloadPackages(Packages);
            const FString Directory = FPackageName::LongPackageNameToFilename(Root);
            if (Root.StartsWith(TEXT("/Game/__XToolsFlattenTest_")))
            {
                IFileManager::Get().DeleteDirectory(*Directory, false, true);
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetFlattenPreflight, "XTools.AssetEditor.Flatten.Preflight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FXAssetFlattenPreflight::RunTest(const FString& Parameters)
{
    XAssetFlattenTests::FFixture Fixture;
    auto* A = Fixture.Make(TEXT("A/Same"));
    auto* B = Fixture.Make(TEXT("B/Same"));
    if (!TestTrue(TEXT("Save fixtures"), Fixture.Save())) { return false; }
    const FString Original = A->GetPathName();
    auto Result = UX_AssetFlattenLibrary::FlattenAssets({FAssetData(A), FAssetData(B)}, Fixture.Root / TEXT("Target"));
    TestFalse(TEXT("Batch name collision rejected"), Result.bSuccess);
    TestEqual(TEXT("Collision causes zero moves"), Result.MovedCount, 0);
    TestEqual(TEXT("Source unchanged"), A->GetPathName(), Original);
    Result = UX_AssetFlattenLibrary::FlattenAssets({FAssetData(A)}, Fixture.Root / TEXT("B"));
    TestFalse(TEXT("Existing destination rejected"), Result.bSuccess);
    Result = UX_AssetFlattenLibrary::FlattenAssets({FAssetData(A)}, TEXT("/Engine/Target"));
    TestFalse(TEXT("Engine destination rejected"), Result.bSuccess);
    A->MarkPackageDirty();
    Result = UX_AssetFlattenLibrary::FlattenAssets({FAssetData(A)}, Fixture.Root / TEXT("Target"));
    TestFalse(TEXT("Dirty source rejected"), Result.bSuccess);
    A->GetOutermost()->SetDirtyFlag(false);
    Result = UX_AssetFlattenLibrary::FlattenAssets({FAssetData(A), FAssetData(A)}, Fixture.Root / TEXT("A/"));
    TestTrue(TEXT("Same folder is a no-op"), Result.bSuccess);
    TestEqual(TEXT("Duplicate selection deduplicated"), Result.CollectedCount, 1);
    TestEqual(TEXT("Already in destination skipped"), Result.SkippedCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetMoveProtectedMounts, "XTools.AssetEditor.Flatten.ProtectedMounts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FXAssetMoveProtectedMounts::RunTest(const FString& Parameters)
{
    // Emulate a plugin mount in an isolated Saved directory. Never write to real Engine/plugin content.
    struct FTestMount
    {
        FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FString Root = TEXT("/XToolsProtectedPlugin_") + Id + TEXT("/");
        FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / (TEXT("ProtectedMount_") + Id)) + TEXT("/");
        FTestMount()
        {
            IFileManager::Get().MakeDirectory(*Directory, true);
            FPackageName::RegisterMountPoint(Root, Directory);
        }
        ~FTestMount()
        {
            FPackageName::UnRegisterMountPoint(Root, Directory);
            const FString OwnerDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation"));
            if (FPaths::IsUnderDirectory(Directory, OwnerDirectory)) { IFileManager::Get().DeleteDirectory(*Directory, false, true); }
        }
    } Mount;
    XAssetFlattenTests::FFixture PluginFixture;
    PluginFixture.Root = Mount.Root + TEXT("Assets");
    auto* PluginAsset = PluginFixture.Make(TEXT("PluginDependency"));
    if (!TestTrue(TEXT("Save isolated plugin asset"), PluginFixture.Save())) { return false; }
    const FString PluginPath = PluginAsset->GetPathName();
    const FString PluginFile = FPackageName::LongPackageNameToFilename(PluginAsset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    TArray<uint8> BeforePluginBytes;
    if (!TestTrue(TEXT("Read protected file before move"), FFileHelper::LoadFileToArray(BeforePluginBytes, *PluginFile))) { return false; }

    for (bool bOrganize : {false, true})
    {
        XAssetFlattenTests::FFixture Fixture;
        auto* RootAsset = Fixture.Make(TEXT("Source/Root"));
        auto* LocalDependency = Fixture.Make(TEXT("Source/LocalDependency"));
        RootAsset->HardReference = PluginAsset;
        RootAsset->SoftReference = LocalDependency;
        const FSoftObjectPath EnginePath(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
        RootAsset->ExternalReference = EnginePath;
        if (!TestTrue(TEXT("Save mixed-mount dependencies"), Fixture.Save())) { return false; }
        const FString Target = Fixture.Root / TEXT("Target");
        auto Move = [bOrganize](const TArray<FAssetData>& Assets, const FString& Folder)
        {
            return bOrganize ? UX_AssetFlattenLibrary::OrganizeAssets(Assets, Folder) : UX_AssetFlattenLibrary::FlattenAssets(Assets, Folder);
        };
        auto Result = Move({FAssetData(PluginAsset), FAssetData(PluginAsset)}, Target);
        TestFalse(TEXT("External-only selection is not a successful move"), Result.bSuccess);
        TestEqual(TEXT("External-only selection moves nothing"), Result.MovedCount, 0);
        TestEqual(TEXT("External selection deduplicated"), Result.SkippedExternalAssets.Num(), 1);
        TestTrue(TEXT("Skipped plugin path reported"), Result.SkippedExternalAssets.Contains(PluginPath));
        TestTrue(TEXT("No project assets explained"), !Result.Errors.IsEmpty());
        for (const FString& Forbidden : {FString(TEXT("/Engine/Target")), Mount.Root + TEXT("Target"), FString(TEXT("/All/Game/Target")), FString(TEXT("/Game/../Engine/Target")), FPaths::ProjectContentDir()})
        {
            Result = Move({FAssetData(RootAsset)}, Forbidden);
            TestFalse(TEXT("Non-project or non-package destination rejected"), Result.bSuccess);
            TestEqual(TEXT("Invalid target moves nothing"), Result.MovedCount, 0);
        }
        // Engine-root metadata must be skipped before attempting to load or modify that asset.
        FAssetData EngineSelection(RootAsset);
        EngineSelection.PackageName = TEXT("/Engine/__XToolsProtected/Root");
        EngineSelection.PackagePath = TEXT("/Engine/__XToolsProtected");
        Result = Move({EngineSelection}, Target);
        TestFalse(TEXT("Engine asset selection rejected"), Result.bSuccess);
        TestEqual(TEXT("Engine selection moves nothing"), Result.MovedCount, 0);
        TestTrue(TEXT("Skipped engine path reported"), Result.SkippedExternalAssets.Contains(EngineSelection.GetObjectPathString()));

        Result = Move({FAssetData(RootAsset), FAssetData(PluginAsset), EngineSelection, FAssetData(PluginAsset)}, Target);
        for (const FString& Error : Result.Errors) { AddError(Error); }
        TestTrue(TEXT("Local asset referencing protected mounts can move"), Result.bSuccess);
        TestEqual(TEXT("Only Game dependencies gathered"), Result.CollectedCount, 2);
        TestEqual(TEXT("Only project assets moved"), Result.MovedCount, 2);
        TestEqual(TEXT("Both external mounts reported once"), Result.SkippedExternalAssets.Num(), 2);
        TestEqual(TEXT("External skips separate from already-at-target count"), Result.SkippedCount, 0);
        TestEqual(TEXT("Plugin asset stays at original path"), PluginAsset->GetPathName(), PluginPath);
        TestTrue(TEXT("Hard plugin reference preserved"), RootAsset->HardReference == PluginAsset);
        TestEqual(TEXT("Engine soft reference preserves original path"), RootAsset->ExternalReference.ToSoftObjectPath(), EnginePath);
        TArray<uint8> AfterPluginBytes;
        TestTrue(TEXT("Read protected file after move"), FFileHelper::LoadFileToArray(AfterPluginBytes, *PluginFile));
        TestTrue(TEXT("Protected dependency file unchanged"), BeforePluginBytes == AfterPluginBytes);
        Result = Move({FAssetData(RootAsset), FAssetData(PluginAsset)}, Target);
        TestTrue(TEXT("Repeated mixed selection succeeds without moves"), Result.bSuccess);
        TestEqual(TEXT("Already-at-target project assets counted separately"), Result.SkippedCount, 2);
        TestEqual(TEXT("Repeated selection still reports external skip"), Result.SkippedExternalAssets.Num(), 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetOrganizeTypes, "XTools.AssetEditor.Flatten.OrganizeByType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FXAssetOrganizeTypes::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptGuard(GIsRunningUnattendedScript, !XTOOLS_ENGINE_5_4_OR_LATER);
    XAssetFlattenTests::FFixture Fixture;
    auto* Material = Fixture.MakeTyped<UMaterial>(TEXT("Source/Materials/Same"));
    auto* Texture = Fixture.MakeTyped<UTexture2D>(TEXT("Source/Textures/Same"));
    auto* Instance = Fixture.MakeTyped<UMaterialInstanceConstant>(TEXT("Source/Instance"));
    Instance->Parent = Material;
    FTextureParameterValue Parameter;
    Parameter.ParameterInfo = FMaterialParameterInfo(TEXT("Albedo"));
    Parameter.ParameterValue = Texture;
    Instance->TextureParameterValues.Add(Parameter);
    if (!TestTrue(TEXT("Save material and texture dependency fixtures"), Fixture.Save())) { return false; }
    const FString Target = Fixture.Root / TEXT("Target");
    auto Result = UX_AssetFlattenLibrary::OrganizeAssets({FAssetData(Instance)}, Target);
    TestEqual(TEXT("Instance gathers parent and texture recursively"), Result.CollectedCount, 3);
    TestEqual(TEXT("Same names in different categories are allowed"), Result.MovedCount, 3);
    TestEqual(TEXT("Material destination"), Material->GetOutermost()->GetName(), Target / TEXT("Materials/Same"));
    TestEqual(TEXT("Texture subclass destination"), Texture->GetOutermost()->GetName(), Target / TEXT("Textures/Same"));
    TestEqual(TEXT("Instance takes precedence over material base class"), Instance->GetOutermost()->GetName(), Target / TEXT("Materials/Instances/Instance"));
    TestTrue(TEXT("Parent reference preserved across folders"), Instance->Parent == Material);
    TestTrue(TEXT("Texture reference preserved across folders"), Instance->TextureParameterValues.Num() == 1 && Instance->TextureParameterValues[0].ParameterValue == Texture);
#if XTOOLS_ENGINE_5_4_OR_LATER
    if (FApp::IsUnattended() && !Result.RemainingRedirectors.IsEmpty())
    {
        TestFalse(TEXT("Unattended fixup explicitly incomplete"), Result.bSuccess);
        TestTrue(TEXT("Pending fixup explains incomplete result"), !Result.Errors.IsEmpty());
    }
    else
#endif
    {
        for (const FString& Error : Result.Errors) { AddError(Error); }
        TestTrue(TEXT("Categorized move and fixup complete"), Result.bSuccess);
    }
    Result = UX_AssetFlattenLibrary::OrganizeAssets({FAssetData(Instance)}, Target);
    TestTrue(TEXT("Repeating categorized move is a no-op"), Result.bSuccess);
    TestEqual(TEXT("All categorized dependencies skipped"), Result.SkippedCount, 3);

    auto* A = Fixture.Make(TEXT("Source/A/Duplicate"));
    auto* B = Fixture.Make(TEXT("Source/B/Duplicate"));
    if (!TestTrue(TEXT("Save collision fixtures"), Fixture.Save())) { return false; }
    Result = UX_AssetFlattenLibrary::OrganizeAssets({FAssetData(A), FAssetData(B)}, Target);
    TestFalse(TEXT("Same category and name conflict rejected"), Result.bSuccess);
    TestEqual(TEXT("Conflict prevents every move"), Result.MovedCount, 0);
    auto* Existing = Fixture.Make(TEXT("Target/Data/Duplicate"));
    if (!TestTrue(TEXT("Save occupied categorized destination"), Fixture.Save())) { return false; }
    Result = UX_AssetFlattenLibrary::OrganizeAssets({FAssetData(A)}, Target);
    TestFalse(TEXT("Occupied categorized destination rejected"), Result.bSuccess);
    TestEqual(TEXT("Occupied destination causes no moves"), Result.MovedCount, 0);
    Result = UX_AssetFlattenLibrary::OrganizeAssets({FAssetData(Existing)}, Target);
    TestTrue(TEXT("Custom data asset subclass already categorized is skipped"), Result.bSuccess);
    TestEqual(TEXT("Data asset skipped"), Result.SkippedCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetFlattenReferences, "XTools.AssetEditor.Flatten.References",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FXAssetFlattenReferences::RunTest(const FString& Parameters)
{
    XAssetFlattenTests::FFixture Fixture;
    auto* A = Fixture.Make(TEXT("A/RootAsset"));
    auto* B = Fixture.Make(TEXT("B/HardAsset"));
    auto* C = Fixture.Make(TEXT("C/SoftAsset"));
    auto* External = Fixture.Make(TEXT("Outside/Referencer"));
    A->HardReference = B;
    B->SoftReference = C;
    C->HardReference = A; // Dependency cycle must terminate.
    External->HardReference = B;
    External->SoftReference = A;
    if (!TestTrue(TEXT("Save reference fixtures"), Fixture.Save())) { return false; }
    const FString ExternalPath = External->GetPathName();
    UPackage* ExternalPackage = External->GetOutermost();
    Fixture.Assets.RemoveAt(3);
    TestTrue(TEXT("Unload outside referencer before moving"), UPackageTools::UnloadPackages({ExternalPackage}));
    External = nullptr;
    const FString Target = Fixture.Root / TEXT("Target");
    const auto Result = UX_AssetFlattenLibrary::FlattenAssets({FAssetData(A)}, Target);
    for (const FString& Error : Result.Errors) { AddError(Error); }
    TestTrue(TEXT("Move and fixup completed"), Result.bSuccess);
    TestEqual(TEXT("Recursive hard/soft dependencies, cycle deduplicated"), Result.CollectedCount, 3);
    TestEqual(TEXT("All gathered assets moved"), Result.MovedCount, 3);
    TestEqual(TEXT("No remaining redirectors"), Result.RemainingRedirectors.Num(), 0);
    // RenameAssets may have loaded it again. Unload once more to verify the saved file.
    if (UPackage* ReloadedPackage = FindPackage(nullptr, *FPackageName::ObjectPathToPackageName(ExternalPath)))
    {
        if (!TestTrue(TEXT("Unload updated referencer for disk verification"), UPackageTools::UnloadPackages({ReloadedPackage}))) { return false; }
    }
    External = LoadObject<UX_AssetFlattenTestAsset>(nullptr, *ExternalPath);
    if (!TestNotNull(TEXT("Reload outside referencer"), External)) { return false; }
    Fixture.Assets.Emplace(External);
    TestEqual(TEXT("Outside referencer not moved"), External->GetPathName(), ExternalPath);
    TestTrue(TEXT("Hard reference preserved"), External->HardReference == B);
    TestEqual(TEXT("Outside soft reference updated"), External->SoftReference.ToSoftObjectPath().ToString(), A->GetPathName());
    TestEqual(TEXT("Transitive soft reference updated"), B->SoftReference.ToSoftObjectPath().ToString(), C->GetPathName());
    TestFalse(TEXT("Outside referencer saved"), External->GetOutermost()->IsDirty());
    const auto Again = UX_AssetFlattenLibrary::FlattenAssets({FAssetData(A)}, Target);
    for (const FString& Error : Again.Errors) { AddError(Error); }
    TestTrue(TEXT("Repeated operation succeeds"), Again.bSuccess);
    TestEqual(TEXT("Repeated operation moves nothing"), Again.MovedCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetMoveDirtyReferencer, "XTools.AssetEditor.Flatten.DirtyReferencerResult",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FXAssetMoveDirtyReferencer::RunTest(const FString& Parameters)
{
    XAssetFlattenTests::FFixture Fixture;
    auto* Asset = Fixture.Make(TEXT("Source/Asset"));
    auto* External = Fixture.Make(TEXT("Outside/Referencer"));
    External->SoftReference = Asset;
    if (!TestTrue(TEXT("Save fixtures"), Fixture.Save())) { return false; }
    const TWeakObjectPtr<UPackage> ReferencerPackage = External->GetOutermost();
    bool bDirtiedAfterSave = false;
    // Simulate an editor post-save callback leaving unsaved changes in a referencer.
    const FDelegateHandle Handle = UPackage::PackageSavedWithContextEvent.AddLambda(
        [ReferencerPackage, &bDirtiedAfterSave](const FString&, UPackage* Package, FObjectPostSaveContext)
        {
            if (Package == ReferencerPackage.Get())
            {
                Package->SetDirtyFlag(true);
                bDirtiedAfterSave = true;
            }
        });
    const auto Result = UX_AssetFlattenLibrary::OrganizeAssets({FAssetData(Asset)}, Fixture.Root / TEXT("Target"));
    UPackage::PackageSavedWithContextEvent.Remove(Handle);
    TestTrue(TEXT("Fault injection exercised a real referencer save"), bDirtiedAfterSave);
    TestEqual(TEXT("Asset itself moved"), Result.MovedCount, 1);
    TestFalse(TEXT("Dirty referencer prevents complete-success report"), Result.bSuccess);
    const FString ReferencerName = External->GetOutermost()->GetName();
    TestTrue(TEXT("Result identifies unsaved referencer"), Result.Errors.ContainsByPredicate(
        [ReferencerName](const FString& Error) { return Error.Contains(ReferencerName); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAssetFlattenMapReferencer, "XTools.AssetEditor.Flatten.UnloadedMapReferencer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FXAssetFlattenMapReferencer::RunTest(const FString& Parameters)
{
    // Exercise script save semantics: unattended editor mode otherwise cancels native fixup saves.
    TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, !XTOOLS_ENGINE_5_4_OR_LATER);
    XAssetFlattenTests::FFixture Fixture;
    auto* Asset = Fixture.Make(TEXT("Source/ReferencedAsset"));
    if (!TestTrue(TEXT("Save asset"), Fixture.Save())) { return false; }
    const FString OldPath = Asset->GetPathName();
    const FString MapPackageName = Fixture.Root / TEXT("Outside/RefMap");
    Fixture.ExtraPackages.Add(MapPackageName);
    UPackage* MapPackage = CreatePackage(*MapPackageName);
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("RefMap"), MapPackage, false);
    World->SetFlags(RF_Public | RF_Standalone);
    World->ExtraReferencedObjects.Add(Asset);
    FAssetRegistryModule::AssetCreated(World);
    const FString MapFile = FPackageName::LongPackageNameToFilename(MapPackageName, FPackageName::GetMapPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(MapFile), true);
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    if (!TestTrue(TEXT("Save map referencer"), UPackage::SavePackage(MapPackage, World, *MapFile, SaveArgs))) { return false; }
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    Registry.ScanModifiedAssetFiles({MapFile});
    TestTrue(TEXT("Unload referencer map"), UPackageTools::UnloadPackages({MapPackage}));
    World = nullptr;
    TArray<FName> BeforeReferencers;
    Registry.GetReferencers(Asset->GetOutermost()->GetFName(), BeforeReferencers);
    TestTrue(TEXT("Registry contains unloaded map referencer before move"), BeforeReferencers.Contains(*MapPackageName));
    const FString Target = Fixture.Root / TEXT("Target");
    const auto Result = UX_AssetFlattenLibrary::FlattenAssets({FAssetData(Asset)}, Target);
#if XTOOLS_ENGINE_5_4_OR_LATER
    if (FApp::IsUnattended())
    {
        TestFalse(TEXT("Unattended fixup reported incomplete without crashing"), Result.bSuccess);
        TestEqual(TEXT("Move completed"), Result.MovedCount, 1);
        TestEqual(TEXT("Redirector retained for interactive fixup"), Result.RemainingRedirectors.Num(), 1);
        TestTrue(TEXT("Original path still resolves"), LoadObject<UX_AssetFlattenTestAsset>(nullptr, *OldPath) == Asset);
        return true;
    }
#endif
    for (const FString& Error : Result.Errors) { AddError(Error); }
    TestTrue(TEXT("Move with unloaded map reference completes"), Result.bSuccess);
    TestEqual(TEXT("Map itself is not gathered"), Result.CollectedCount, 1);
    TestEqual(TEXT("Redirector removed after fixup"), Result.RemainingRedirectors.Num(), 0);
    Registry.ScanModifiedAssetFiles({MapFile});
    TArray<FName> Dependencies;
    Registry.GetDependencies(*MapPackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
    TestTrue(TEXT("Saved map references new package"), Dependencies.Contains(Asset->GetOutermost()->GetFName()));
    TestFalse(TEXT("Saved map no longer references old package"), Dependencies.Contains(*FPackageName::ObjectPathToPackageName(OldPath)));
    return true;
}
#endif
