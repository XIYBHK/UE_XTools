// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

#include "K2Node_HackTimeline.h"

#include "K2Node_BaseTimeline.generated.h"

UCLASS(abstract)
class COMPONENTTIMELINEUNCOOKED_API UK2Node_BaseTimeline : public UK2Node_HackTimeline
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UEdGraphNode Interface.
	virtual void PostPasteNode() override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	//~ End UEdGraphNode Interface.


	//~ Begin UK2Node Interface.
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UK2Node Interface.

protected:
	static UTimelineTemplate* AddNewTimeline(UBlueprint* Blueprint, const FName& TimelineVarName);

	virtual bool DoesSupportTimelines(const UBlueprint* Blueprint) const;
	virtual FName GetRequiredNodeInBlueprint() const;
};