// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAAssetEditorHandlerObject.h"
#include "BlueprintAssistEditorFeatures.h"
#include "UObject/Object.h"
#include "BARootObject.generated.h"

UCLASS()
class XTOOLS_BLUEPRINTASSIST_API UBARootObject : public UObject
{
	GENERATED_BODY()

public:
	void Init();

	void Tick();

	void Cleanup();

	UBAAssetEditorHandlerObject* GetAssetHandler() const { return AssetHandler; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UBAAssetEditorHandlerObject> AssetHandler = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBAEditorFeatures> EditorFeatures = nullptr;
};
