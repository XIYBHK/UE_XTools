// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAAssetEditorHandlerObject.h"
#include "BlueprintAssistEditorFeatures.h"
#include "UObject/Object.h"
#include "BARootObject.generated.h"

UCLASS()
class BLUEPRINTASSIST_API UBARootObject : public UObject
{
	GENERATED_BODY()

public:
	void Init();

	void Tick();

	void Cleanup();

	UBAAssetEditorHandlerObject* GetAssetHandler() const { return AssetHandler; }

private:
	UPROPERTY()
	UBAAssetEditorHandlerObject* AssetHandler = nullptr;

	UPROPERTY()
	UBAEditorFeatures* EditorFeatures = nullptr;
};
