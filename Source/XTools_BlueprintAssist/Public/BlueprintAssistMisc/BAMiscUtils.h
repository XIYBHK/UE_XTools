// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistTabHandler.h"
#include "BlueprintAssistObjects/BAAssetEditorHandlerObject.h"
#include "Widgets/Notifications/SNotificationList.h"

class IAssetFamily;
class BA_SUBOBJECT_EDITOR_TREE_NODE;

struct XTOOLS_BLUEPRINTASSIST_API FBAMiscUtils
{
	static AActor* GetSCSNodeDefaultActor(TSharedPtr<BA_SUBOBJECT_EDITOR_TREE_NODE> Node, UBlueprint* Blueprint);

	static bool IsSCSActorNode(TSharedPtr<BA_SUBOBJECT_EDITOR_TREE_NODE> Node);

	template<class AssetClass, class EditorClass>
	static EditorClass* GetEditorFromActiveTabCasted()
	{
		if (UBAAssetEditorHandlerObject* EditorHandler = UBAAssetEditorHandlerObject::Get())
		{
			return EditorHandler->GetEditorFromTabCasted<AssetClass, EditorClass>(FBATabHandler::Get().GetLastMajorTab());
		}

		return nullptr;
	}

	template<class AssetClass>
	static AssetClass* GetAssetForActiveTab()
	{
		if (UBAAssetEditorHandlerObject* EditorHandler = UBAAssetEditorHandlerObject::Get())
		{
			return EditorHandler->GetAssetFromTab<AssetClass>(FBATabHandler::Get().GetLastMajorTab());
		}

		return nullptr;
	}

	static TArray<FString> ParseStringIntoArray(const FString& String, bool bToLower = true);
	static FString ParseSearchTextByFilter(const FString& SearchText, const FString& FilterString);

	static TSharedPtr<IAssetFamily> GetAssetFamilyForObject(UObject* Object);

	template<typename T>
	static TArray<TWeakObjectPtr<T>> AsWeakObjectPtrArray(const TArray<T*>& InObjects)
	{
		TArray<TWeakObjectPtr<T>> TypedObjects;
		TypedObjects.Reserve(InObjects.Num());
		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			TypedObjects.Add(*ObjIt);
		}

		return TypedObjects;
	}

#if BA_UE_VERSION_OR_LATER(5,0)
	template<typename T>
	static TArray<TWeakObjectPtr<T>> AsWeakObjectPtrArray(const TArray<TObjectPtr<T>>& InObjects)
	{
		TArray<TWeakObjectPtr<T>> TypedObjects;
		TypedObjects.Reserve(InObjects.Num());
		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			TypedObjects.Add(*ObjIt);
		}

		return TypedObjects;
	}
#endif

	template<typename T>
	static TArray<T*> AsPtrArray(const TArray<TWeakObjectPtr<T>>& InObjects)
	{
		TArray<T*> OutPtrs;
		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			if (ObjIt->IsValid())
			{
				OutPtrs.Add(ObjIt->Get());
			}
		}

		return OutPtrs;
	}

	template<typename T>
	static TSet<TWeakObjectPtr<T>> AsWeakObjectPtrSet(const TArray<T*>& InObjects)
	{
		TSet<TWeakObjectPtr<T>> TypedObjects;
		TypedObjects.Reserve(InObjects.Num());
		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			TypedObjects.Add(*ObjIt);
		}

		return TypedObjects;
	}

	template<typename T>
	static TSet<T*> AsPtrSet(const TSet<TWeakObjectPtr<T>>& InObjects)
	{
		TSet<T*> OutPtrs;
		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			if (ObjIt->IsValid())
			{
				OutPtrs.Add(ObjIt->Get());
			}
		}

		return OutPtrs;
	}

	static void MessageLogError(const FText& ErrorMsg);

	static bool IsBlueprintImplementableEvent(UFunction* Function);
	static bool IsBlueprintNativeEvent(UFunction* Function);

	static FString GetInputChordName(const FInputChord& Chord);

	static FString CompressString(const FString& InString, FName FormatName = NAME_Gzip);

	static bool DecompressString(const FString& InCompressedString, FString& OutDecompressedString, FName FormatName = NAME_Gzip);

	static void WriteTextToFile(TCHAR* FullPath, TCHAR* Content);

	static void ClipboardCopy(const FString& String);

	static TSharedPtr<SNotificationItem> ShowSimpleSlateNotification(const FText& Msg, SNotificationItem::ECompletionState State, float ExpireDuration = 3.0f);
};
