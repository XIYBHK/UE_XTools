// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistCache.h"

#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistModule.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistUtils.h"
#include "Editor.h"
#include "GeneralProjectSettings.h"
#include "JsonObjectConverter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/LazySingleton.h"
#include "Stats/StatsMisc.h"
#include "UObject/MetaData.h"

#if BA_UE_VERSION_OR_LATER(5, 0)
#include "UObject/ObjectSaveContext.h"
#endif

#define CACHE_VERSION 4

static FName NAME_BA_GRAPH_DATA = FName("BAGraphData");

FBACache& FBACache::Get()
{
	return TLazySingleton<FBACache>::Get();
}

void FBACache::TearDown()
{
	TLazySingleton<FBACache>::TearDown();
}

void FBACache::Init()
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnFilesLoaded().AddRaw(this, &FBACache::LoadCache);

	FCoreDelegates::OnPreExit.AddRaw(this, &FBACache::SaveCache);

#if BA_UE_VERSION_OR_LATER(5, 0)
	FCoreUObjectDelegates::OnObjectPreSave.AddRaw(this, &FBACache::OnObjectPreSave);
#else
	FCoreUObjectDelegates::OnObjectSaved.AddRaw(this, &FBACache::OnObjectSaved);
#endif
}

void FBACache::LoadCache()
{
	if (!UBASettings::Get().bSaveBlueprintAssistCacheToFile)
	{
		return;
	}

	if (bHasLoaded)
	{
		return;
	}

	bHasLoaded = true;

	const FString CachePath = GetCachePath();
	const FString OldCachePath = GetAlternateCachePath();

	FString FileData;
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*CachePath))
	{
		FFileHelper::LoadFileToString(FileData, *CachePath);

		if (FJsonObjectConverter::JsonObjectStringToUStruct(FileData, &CacheData, 0, 0))
		{
			UE_LOG(LogBlueprintAssist, Log, TEXT("Loaded blueprint assist cache: %s"), *GetCachePath(true));
		}
		else
		{
			UE_LOG(LogBlueprintAssist, Log, TEXT("Failed to load node size cache: %s"), *GetCachePath(true));
		}
	}
	else if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OldCachePath))
	{
		FFileHelper::LoadFileToString(FileData, *OldCachePath);

		if (FJsonObjectConverter::JsonObjectStringToUStruct(FileData, &CacheData, 0, 0))
		{
			UE_LOG(LogBlueprintAssist, Log, TEXT("Loaded blueprint assist cache from old cache path: %s"), *GetAlternateCachePath(true));
		}
		else
		{
			UE_LOG(LogBlueprintAssist, Log, TEXT("Failed to load node size cache from old cache path: %s"), *GetAlternateCachePath(true));
		}
	}

	if (CacheData.CacheVersion != CACHE_VERSION)
	{
		// clear the cache if our version doesn't match
		CacheData.PackageData.Empty();

		CacheData.CacheVersion = CACHE_VERSION;
	}

	CleanupFiles();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.OnFilesLoaded().RemoveAll(this);
}

void FBACache::SaveCache()
{
	if (!UBASettings::Get().bSaveBlueprintAssistCacheToFile)
	{
		return;
	}

	const FString CachePath = GetCachePath();

	double SaveTime = 0;

	{
		SCOPE_SECONDS_COUNTER(SaveTime);

		// Write data to file
		FString JsonAsString;
		FJsonObjectConverter::UStructToJsonObjectString(CacheData, JsonAsString, 0, 0, 0, nullptr, UBASettings_Advanced::Get().bPrettyPrintCacheJSON);
		FFileHelper::SaveStringToFile(JsonAsString, *CachePath);
	}

	UE_LOG(LogBlueprintAssist, Log, TEXT("Saved cache to %s took %.2fms"), *GetCachePath(true), SaveTime * 1000);
}

void FBACache::DeleteCache()
{
	FString CachePath = GetCachePath();
	CacheData.PackageData.Empty();

	if (FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*CachePath))
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("Deleted cache file at %s"), *GetCachePath(true));
	}
	else
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("Delete cache failed: Cache file does not exist or is read-only %s"), *GetCachePath(true));
	}
}

void FBACache::CleanupFiles()
{
	// Get all assets
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// Get package guids from assets
	TSet<FName> CurrentPackageNames;

#if BA_UE_VERSION_OR_LATER(5, 0)
	TArray<FAssetData> Assets;
	FARFilter Filter;
	AssetRegistry.GetAllAssets(Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		CurrentPackageNames.Add(Asset.PackageName);
	}
#else
	const auto& AssetDataMap = AssetRegistry.GetAssetRegistryState()->GetObjectPathToAssetDataMap();
	for (const TPair<FName, const FAssetData*>& AssetDataPair : AssetDataMap)
	{
		const FAssetData* AssetData = AssetDataPair.Value;
		CurrentPackageNames.Add(AssetData->PackageName);
	}
#endif
	// Remove missing files
	TArray<FName> OldPackageGuids;
	CacheData.PackageData.GetKeys(OldPackageGuids);
	for (FName PackageGuid : OldPackageGuids)
	{
		if (!CurrentPackageNames.Contains(PackageGuid))
		{
			CacheData.PackageData.Remove(PackageGuid);
		}
	}
}

FBAGraphData& FBACache::GetGraphData(UEdGraph* Graph)
{
	check(Graph);
	UPackage* Package = Graph->GetOutermost();

	FBAPackageData& PackageData = CacheData.PackageData.FindOrAdd(Package->GetFName());

	FBAGraphData& GraphData = PackageData.GraphData.FindOrAdd(FBAUtils::GetGraphGuid(Graph));
	if (!GraphData.bTriedLoadingMetaData)
	{
		LoadGraphDataFromPackageMetaData(Graph, GraphData);
	}

	return GraphData;
}

FString FBACache::GetProjectSavedCachePath(bool bFullPath)
{
	return FPaths::ProjectDir() / TEXT("Saved") / TEXT("BlueprintAssist") / TEXT("BlueprintAssistCache.json");
}

FString FBACache::GetPluginCachePath(bool bFullPath)
{
	FString PluginDir = IPluginManager::Get().FindPlugin("BlueprintAssist")->GetBaseDir();

	if (bFullPath)
	{
		PluginDir = FPaths::ConvertRelativePathToFull(PluginDir);
	}

	const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();
	const FGuid& ProjectID = ProjectSettings->ProjectID;

	return PluginDir + "/NodeSizeCache/" + ProjectID.ToString() + ".json";
}

FString FBACache::GetCachePath(bool bFullPath)
{
	switch (UBASettings::Get().CacheSaveLocation)
	{
		case EBACacheSaveLocation::Project:
			return GetProjectSavedCachePath(bFullPath);
		case EBACacheSaveLocation::Plugin:
			return GetPluginCachePath(bFullPath);
		default:
			return GetProjectSavedCachePath(bFullPath);
	}
}

FString FBACache::GetAlternateCachePath(bool bFullPath)
{
	switch (UBASettings::Get().CacheSaveLocation)
	{
		case EBACacheSaveLocation::Project:
			return GetPluginCachePath(bFullPath);
		case EBACacheSaveLocation::Plugin:
			return GetProjectSavedCachePath(bFullPath);
		default:
			return GetProjectSavedCachePath(bFullPath);
	}
}

void FBACache::SaveGraphDataToPackageMetaData(UEdGraph* Graph)
{
	if (!Graph)
	{
		return;
	}

	if (!GetDefault<UBASettings_Advanced>()->bStoreCacheDataInPackageMetaData)
	{
		return;
	}

	if (UPackage* AssetPackage = Graph->GetPackage())
	{
		if (UMetaData* MetaData = AssetPackage->GetMetaData())
		{
			FBAGraphData& GraphData = GetGraphData(Graph);

			GraphData.CleanupGraph(Graph);
			
			FString GraphDataAsString;
			if (FJsonObjectConverter::UStructToJsonObjectString(GraphData, GraphDataAsString))
			{
				MetaData->SetValue(Graph, NAME_BA_GRAPH_DATA, *GraphDataAsString);
			}
		}
	}
}

bool FBACache::LoadGraphDataFromPackageMetaData(UEdGraph* Graph, FBAGraphData& GraphData)
{
	if (!Graph)
	{
		return false;
	}

	if (!GetDefault<UBASettings_Advanced>()->bStoreCacheDataInPackageMetaData)
	{
		return false;
	}

	if (UPackage* AssetPackage = Graph->GetPackage())
	{
		if (UMetaData* MetaData = AssetPackage->GetMetaData())
		{
			if (const FString* GraphDataAsString = MetaData->FindValue(Graph, NAME_BA_GRAPH_DATA))
			{
				if (FJsonObjectConverter::JsonObjectStringToUStruct(*GraphDataAsString, &GraphData, 0, 0))
				{
					GraphData.bTriedLoadingMetaData = true;
					return true;
				}
			}
		}
	}

	return false;
}

void FBACache::ClearPackageMetaData(UEdGraph* Graph)
{
	if (UPackage* AssetPackage = Graph->GetPackage())
	{
		if (UMetaData* MetaData = AssetPackage->GetMetaData())
		{
			MetaData->RemoveValue(Graph, NAME_BA_GRAPH_DATA);
		}
	}
}

void FBAGraphData::CleanupGraph(UEdGraph* Graph)
{
	if (Graph == nullptr)
	{
		UE_LOG(LogBlueprintAssist, Error, TEXT("Tried to cleanup null graph"));
		return;
	}

	TSet<FGuid> CurrentNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		// Collect all node guids from the graph
		CurrentNodes.Add(FBAUtils::GetNodeGuid(Node));

		if (FBANodeData* FoundNode = NodeData.Find(FBAUtils::GetNodeGuid(Node)))
		{
			// Collect current pin guids
			TSet<FGuid> CurrentPins;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				CurrentPins.Add(Pin->PinId);
			}

			// Collect cached pin guids
			TArray<FGuid> CachedPinGuids;
			FoundNode->CachedPins.GetKeys(CachedPinGuids);

			// Cleanup missing guids
			for (FGuid PinGuid : CachedPinGuids)
			{
				if (!CurrentPins.Contains(PinGuid))
				{
					FoundNode->CachedPins.Remove(PinGuid);
				}
			}
		}
	}

	// Remove any missing guids from the cached nodes
	TArray<FGuid> CachedNodeGuids;
	NodeData.GetKeys(CachedNodeGuids);

	for (FGuid NodeGuid : CachedNodeGuids)
	{
		if (!CurrentNodes.Contains(NodeGuid))
		{
			NodeData.Remove(NodeGuid);
		}
	}
}

FBANodeData& FBAGraphData::GetNodeData(UEdGraphNode* Node)
{
	return NodeData.FindOrAdd(FBAUtils::GetNodeGuid(Node));
}

#if BA_UE_VERSION_OR_LATER(5, 0)
void FBACache::OnObjectPreSave(UObject* Object, FObjectPreSaveContext Context)
{
	OnObjectSaved(Object);
}
#endif

void FBACache::OnObjectSaved(UObject* Object)
{
	// TODO: This doesn't work because the flag to check if the cook server is in the session doesn't get reset after leaving the session! 
	// if (GUnrealEd && GUnrealEd->CookServer && GUnrealEd->CookServer->IsInSession())
	// {
	// 	return;
	// }

	// Instead we can use this legacy flag to check if the cooker is loading a package
	if (GIsCookerLoadingPackage)
	{
		return;
	}

	if (!bHasSavedThisFrame)
	{
		SaveCache();
		bHasSavedThisFrame = true;
	}

	if (UEdGraph* Graph = Cast<UEdGraph>(Object))
	{
		if (!bHasSavedMetaDataThisFrame)
		{
			if (GetDefault<UBASettings_Advanced>()->bStoreCacheDataInPackageMetaData)
			{
				SaveGraphDataToPackageMetaData(Graph);
			}
			else
			{
				// make sure we aren't storing old data if we disable this setting after using it for a while
				ClearPackageMetaData(Graph);
			}

			bHasSavedMetaDataThisFrame = true;
		}
	}

	if (bHasSavedThisFrame || bHasSavedMetaDataThisFrame)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FBACache::ResetSavedThisFrame));
	}
}