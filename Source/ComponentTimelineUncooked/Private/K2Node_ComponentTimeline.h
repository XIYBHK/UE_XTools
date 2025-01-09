// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

#include "K2Node_BaseTimeline.h"

#include "K2Node_ComponentTimeline.generated.h"

UCLASS()
class COMPONENTTIMELINEUNCOOKED_API UK2Node_ComponentTimeline : public UK2Node_BaseTimeline
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface.

protected:
	virtual bool DoesSupportTimelines(const UBlueprint* Blueprint) const override;
	virtual FName GetRequiredNodeInBlueprint() const override;
};


