// Copyright 2023 Tomasz Klin. All Rights Reserved.


#include "K2Node_ObjectTimeline.h"
#include "ComponentTimelineSettings.h"
#include "Kismet2/BlueprintEditorUtils.h"


#define LOCTEXT_NAMESPACE "K2Node_ObjectTimeline"

/////////////////////////////////////////////////////
// UK2Node_ObjectTimeline

UK2Node_ObjectTimeline::UK2Node_ObjectTimeline(const FObjectInitializer& ObjectInitializer)
	: UK2Node_BaseTimeline(ObjectInitializer)
{
}

FLinearColor UK2Node_ObjectTimeline::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.1f, 1.0f);
}

FText UK2Node_ObjectTimeline::GetNodeTitle(ENodeTitleType::Type TitleType) const
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
		Title = LOCTEXT("NoTimelineTitle", "Add UObject Timeline (Experimental)...");
	}
	return Title;
}


FText UK2Node_ObjectTimeline::GetTooltipText() const
{
	return LOCTEXT("TimelineTooltip", "This is experimental feauture!\nTimeline node allows values to be keyframed over time.\nDouble click to open timeline editor.");
}

void UK2Node_ObjectTimeline::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	if (GetDefault<UComponentTimelineSettings>()->bEnableObjectTimeline)
	{
		Super::GetMenuActions(ActionRegistrar);
	}
}

bool UK2Node_ObjectTimeline::DoesSupportTimelines(const UBlueprint* Blueprint) const
{
	return Super::DoesSupportTimelines(Blueprint) && !FBlueprintEditorUtils::IsComponentBased(Blueprint) && !FBlueprintEditorUtils::IsActorBased(Blueprint);
}

FName UK2Node_ObjectTimeline::GetRequiredNodeInBlueprint() const
{
	static FName NODE_NAME = "InitializeTimelines";
	return NODE_NAME;

}

#undef LOCTEXT_NAMESPACE
