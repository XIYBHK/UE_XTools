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
	AssetHandler->Cleanup();
}
