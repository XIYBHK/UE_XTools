// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistObjects/BARootObject.h"

#include "BlueprintAssistEditorFeatures.h"
#include "BlueprintAssistObjects/BAAssetEditorHandlerObject.h"

void UBARootObject::Init()
{
	AssetHandler = NewObject<UBAAssetEditorHandlerObject>();
	AssetHandler->Init();

	EditorFeatures = NewObject<UBAEditorFeatures>();
	EditorFeatures->Init();
}

void UBARootObject::Tick()
{
	AssetHandler->Tick();
}

void UBARootObject::Cleanup()
{
	if (AssetHandler)
	{
	AssetHandler->Cleanup();
		AssetHandler = nullptr;
	}

	if (EditorFeatures)
	{
		// EditorFeatures will clean up delegates in destructor
		// Mark it for garbage collection by clearing reference
		EditorFeatures = nullptr;
	}
}
