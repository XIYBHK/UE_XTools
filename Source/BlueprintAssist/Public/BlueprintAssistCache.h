// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SGraphPin.h"
#include "BlueprintAssistGlobals.h"

#include "BlueprintAssistCache.generated.h"

USTRUCT()
struct BLUEPRINTASSIST_API FBANodeData
{
	GENERATED_USTRUCT_BODY()

protected:
	UPROPERTY()
	FIntPoint Size = FIntPoint(0, 0); // node size

	UPROPERTY()
	FIntPoint BSize = FIntPoint(0, 0); // comment bubble size
public:

	UPROPERTY()
	TMap<FGuid, float> CachedPins; // pin guid -> pin offset

	UPROPERTY()
	bool bLocked = false;

	UPROPERTY()
	FGuid NodeGroup;

	UPROPERTY()
	TArray<FGuid> NodeGroups;

	void ResetSize()
	{
		Size = FIntPoint(0, 0);
		BSize = FIntPoint(0, 0);
		CachedPins.Reset();
	}

	bool HasSize() const
	{
		return Size.X != 0 && Size.Y != 0;
	}

	void SetSize(const FVector2D& InSize)
	{
		Size.X = FMath::RoundToInt(InSize.X);
		Size.Y = FMath::RoundToInt(InSize.Y);
	}

	const FIntPoint& GetNodeSize() const { return Size; }

	bool HasCommentBubbleSize() const
	{
		return BSize.X != 0 && BSize.Y != 0;
	}

	void SetCommentBubbleSize(const FVector2D& InSize)
	{
		BSize.X = FMath::RoundToInt(InSize.X);
		BSize.Y = FMath::RoundToInt(InSize.Y);
	}

	const FIntPoint& GetCommentBubbleSize() const { return BSize; }
};

USTRUCT()
struct BLUEPRINTASSIST_API FBAGraphData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FGuid, FBANodeData> NodeData; // node guid -> node data

	void CleanupGraph(UEdGraph* Graph);

	FBANodeData& GetNodeData(UEdGraphNode* Node);

	bool bTriedLoadingMetaData = false;
};

USTRUCT()
struct BLUEPRINTASSIST_API FBAPackageData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FGuid, FBAGraphData> GraphData; // graph guid -> graph data
};

USTRUCT()
struct BLUEPRINTASSIST_API FBACacheData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<FName, FBAPackageData> PackageData; // package name -> package data

	UPROPERTY()
	TArray<FString> BookmarkedFolders;

	UPROPERTY()
	int CacheVersion = -1;
};

class BLUEPRINTASSIST_API FBACache
{
public:
	static FBACache& Get();
	static void TearDown();

	void Init();

	FBACacheData& GetCacheData() { return CacheData; }

	void LoadCache();

	void SaveCache();

	void DeleteCache();

	void CleanupFiles();

	FBAGraphData& GetGraphData(UEdGraph* Graph);

	FString GetProjectSavedCachePath(bool bFullPath = false);
	FString GetPluginCachePath(bool bFullPath = false);
	FString GetCachePath(bool bFullPath = false);
	FString GetAlternateCachePath(bool bFullPath = false);

	void SaveGraphDataToPackageMetaData(UEdGraph* Graph);
	bool LoadGraphDataFromPackageMetaData(UEdGraph* Graph, FBAGraphData& GraphData);
	void ClearPackageMetaData(UEdGraph* Graph);

	void SetBookmarkedFolder(const FString& FolderPath, int Index)
	{
		if (Index >= CacheData.BookmarkedFolders.Num())
		{
			CacheData.BookmarkedFolders.SetNum(Index + 1);
		}

		CacheData.BookmarkedFolders[Index] = FolderPath;
	}

	TOptional<FString> FindBookmarkedFolder(int Index)
	{
		return CacheData.BookmarkedFolders.IsValidIndex(Index) ? CacheData.BookmarkedFolders[Index] : TOptional<FString>();
	}

private:
	bool bHasLoaded = false;

	FBACacheData CacheData;

	bool bHasSavedThisFrame = false;
	bool bHasSavedMetaDataThisFrame = false;

#if BA_UE_VERSION_OR_LATER(5, 0)
	void OnObjectPreSave(UObject* Object, FObjectPreSaveContext Context);
#endif

	void OnObjectSaved(UObject* Object);

	void ResetSavedThisFrame()
	{
		bHasSavedThisFrame = false;
		bHasSavedMetaDataThisFrame = false;
	}
};
