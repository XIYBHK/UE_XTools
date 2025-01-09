// Copyright 2023 Tomasz Klin. All Rights Reserved.


#include "K2Node_ComponentTimeline.h"

#include "Kismet2/BlueprintEditorUtils.h"
#define LOCTEXT_NAMESPACE "K2Node_ComponentTimeline"

/////////////////////////////////////////////////////
// UK2Node_CompoenrntTimeline

UK2Node_ComponentTimeline::UK2Node_ComponentTimeline(const FObjectInitializer& ObjectInitializer)
	: UK2Node_BaseTimeline(ObjectInitializer)
{
}

FLinearColor UK2Node_ComponentTimeline::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.51f, 0.0f);
}

FText UK2Node_ComponentTimeline::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText Title = FText::FromName(TimelineName);

	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint != nullptr);

	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineName);
	// if a node hasn't been spawned for this node yet, then lets title it
	// after what it will do (the name would be invalid anyways)
	if (Timeline == nullptr)
	{
		// if this node hasn't spawned a
		Title = LOCTEXT("NoTimelineTitle", "Add Component Timeline...");
	}

	return Title;
}


bool UK2Node_ComponentTimeline::DoesSupportTimelines(const UBlueprint* Blueprint) const
{
	return Super::DoesSupportTimelines(Blueprint) && FBlueprintEditorUtils::IsComponentBased(Blueprint);
}

FName UK2Node_ComponentTimeline::GetRequiredNodeInBlueprint() const
{
	static FName NODE_NAME = "InitializeComponentTimelines";
	return NODE_NAME;
}

#undef LOCTEXT_NAMESPACE
