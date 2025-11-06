// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BlueprintAssistEditorFeatures.generated.h"

class UK2Node;

UCLASS()
class BLUEPRINTASSIST_API UBAEditorFeatures final : public UObject
{
	GENERATED_BODY()

public:
	void Init();
	virtual ~UBAEditorFeatures() override;

	static bool SetNodeNetFlags(UK2Node* Node, EFunctionFlags NetFlags);

protected:
	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);
};
