// Copyright 2023 Tomasz Klin. All Rights Reserved.
#include "K2Node_BaseTimeline.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintBoundNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "K2Node_Composite.h"
#include "K2Node_CallFunction.h"

UK2Node_BaseTimeline::UK2Node_BaseTimeline(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UTimelineTemplate* UK2Node_BaseTimeline::AddNewTimeline(UBlueprint* Blueprint, const FName& TimelineVarName)
{
// 	// Early out if we don't support timelines in this class
// 	if (!DoesSupportTimelines(Blueprint))
// 	{
// 		return nullptr;
// 	}

	// First look to see if we already have a timeline with that name
	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineVarName);
	if (Timeline != nullptr)
	{
		UE_LOG(LogBlueprint, Log, TEXT("AddNewTimeline: Blueprint '%s' already contains a timeline called '%s'"), *Blueprint->GetPathName(), *TimelineVarName.ToString());
		return nullptr;
	}
	else
	{
		Blueprint->Modify();
		check(nullptr != Blueprint->GeneratedClass);
		// Construct new graph with the supplied name
		const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(TimelineVarName);
		Timeline = NewObject<UTimelineTemplate>(Blueprint->GeneratedClass, TimelineTemplateName, RF_Transactional);
		Blueprint->Timelines.Add(Timeline);

		// Potentially adjust variable names for any child blueprints
		FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, TimelineVarName);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return Timeline;
	}
}

bool UK2Node_BaseTimeline::DoesSupportTimelines(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint);
}


FName UK2Node_BaseTimeline::GetRequiredNodeInBlueprint() const
{
	return NAME_None;
}

void UK2Node_BaseTimeline::PostPasteNode()
{
	UK2Node::PostPasteNode();

	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint);

	UTimelineTemplate* OldTimeline = NULL;

	//find the template with same UUID
	for (TObjectIterator<UTimelineTemplate> It; It; ++It)
	{
		UTimelineTemplate* Template = *It;
		if (Template->TimelineGuid == TimelineGuid)
		{
			OldTimeline = Template;
			break;
		}
	}

	// Make sure TimelineName is unique, and we allocate a new timeline template object for this node
	TimelineName = FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint);

	if (!OldTimeline)
	{
		if (UTimelineTemplate* Template = AddNewTimeline(Blueprint, TimelineName))
		{
			bAutoPlay = Template->bAutoPlay;
			bLoop = Template->bLoop;
			bReplicated = Template->bReplicated;
			bIgnoreTimeDilation = Template->bIgnoreTimeDilation;
		}
	}
	else
	{
		check(NULL != Blueprint->GeneratedClass);
		Blueprint->Modify();
		const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(TimelineName);
		UTimelineTemplate* Template = DuplicateObject<UTimelineTemplate>(OldTimeline, Blueprint->GeneratedClass, TimelineTemplateName);
		bAutoPlay = Template->bAutoPlay;
		bLoop = Template->bLoop;
		bReplicated = Template->bReplicated;
		bIgnoreTimeDilation = Template->bIgnoreTimeDilation;
		Template->SetFlags(RF_Transactional);
		Blueprint->Timelines.Add(Template);

		// Fix up timeline tracks to point to the proper location.  When duplicated, they're still parented to their old blueprints because we don't have the appropriate scope.  Note that we never want to fix up external curve asset references
		{
			for (auto TrackIt = Template->FloatTracks.CreateIterator(); TrackIt; ++TrackIt)
			{
				FTTFloatTrack& Track = *TrackIt;
				if (!Track.bIsExternalCurve && Track.CurveFloat && Track.CurveFloat->GetOuter()->IsA(UBlueprint::StaticClass()))
				{
					Track.CurveFloat->Rename(*Template->MakeUniqueCurveName(Track.CurveFloat, Track.CurveFloat->GetOuter()), Blueprint, REN_DontCreateRedirectors);
				}
			}

			for (auto TrackIt = Template->EventTracks.CreateIterator(); TrackIt; ++TrackIt)
			{
				FTTEventTrack& Track = *TrackIt;
				if (!Track.bIsExternalCurve && Track.CurveKeys && Track.CurveKeys->GetOuter()->IsA(UBlueprint::StaticClass()))
				{
					Track.CurveKeys->Rename(*Template->MakeUniqueCurveName(Track.CurveKeys, Track.CurveKeys->GetOuter()), Blueprint, REN_DontCreateRedirectors);
				}
			}

			for (auto TrackIt = Template->VectorTracks.CreateIterator(); TrackIt; ++TrackIt)
			{
				FTTVectorTrack& Track = *TrackIt;
				if (!Track.bIsExternalCurve && Track.CurveVector && Track.CurveVector->GetOuter()->IsA(UBlueprint::StaticClass()))
				{
					Track.CurveVector->Rename(*Template->MakeUniqueCurveName(Track.CurveVector, Track.CurveVector->GetOuter()), Blueprint, REN_DontCreateRedirectors);
				}
			}

			for (auto TrackIt = Template->LinearColorTracks.CreateIterator(); TrackIt; ++TrackIt)
			{
				FTTLinearColorTrack& Track = *TrackIt;
				if (!Track.bIsExternalCurve && Track.CurveLinearColor && Track.CurveLinearColor->GetOuter()->IsA(UBlueprint::StaticClass()))
				{
					Track.CurveLinearColor->Rename(*Template->MakeUniqueCurveName(Track.CurveLinearColor, Track.CurveLinearColor->GetOuter()), Blueprint, REN_DontCreateRedirectors);
				}
			}
		}

		FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, TimelineName);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

bool UK2Node_BaseTimeline::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	if (UK2Node::IsCompatibleWithGraph(TargetGraph))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (Blueprint)
		{
			const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema());
			check(K2Schema);

			const bool bSupportsEventGraphs = FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint);
			const bool bAllowEvents = (K2Schema->GetGraphType(TargetGraph) == GT_Ubergraph) && bSupportsEventGraphs &&
				(Blueprint->BlueprintType != BPTYPE_MacroLibrary);

			if (bAllowEvents)
			{
				return DoesSupportTimelines(Blueprint);
			}
			else
			{
				bool bCompositeOfUbberGraph = false;

				//If the composite has a ubergraph in its outer, it is allowed to have timelines
				if (bSupportsEventGraphs && K2Schema->IsCompositeGraph(TargetGraph))
				{
					while (TargetGraph)
					{
						if (UK2Node_Composite* Composite = Cast<UK2Node_Composite>(TargetGraph->GetOuter()))
						{
							TargetGraph = Cast<UEdGraph>(Composite->GetOuter());
						}
						else if (K2Schema->GetGraphType(TargetGraph) == GT_Ubergraph)
						{
							bCompositeOfUbberGraph = true;
							break;
						}
						else
						{
							TargetGraph = Cast<UEdGraph>(TargetGraph->GetOuter());
						}
					}
				}
				return bCompositeOfUbberGraph ? DoesSupportTimelines(Blueprint) : false;
			}
		}
	}

	return false;
}

void UK2Node_BaseTimeline::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UEdGraph* Graph = GetGraph();

	if (Graph)
	{
		FName RequiredNodeName = GetRequiredNodeInBlueprint();
		for (TObjectPtr<class UEdGraphNode> Node : Graph->Nodes)
		{
			UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node.Get());
			if (CallNode && CallNode->GetFunctionName() == RequiredNodeName)
				return;
		}

		// Format error message
		const FText NodeName = FText::FromString(RequiredNodeName.ToString());
		const FText Message = FText::Format(NSLOCTEXT("UK2Node_BaseTimeline", "MissingInitialization", 
			"Missing '{0}' node in the blueprint. You should call '{1}' at BeginPlay to make Timeline work. @@"), NodeName, NodeName);
		MessageLog.Error(*Message.ToString(), this);
	}
}

void UK2Node_BaseTimeline::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		auto CustomizeTimelineNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode)
		{
			UK2Node_BaseTimeline* TimelineNode = CastChecked<UK2Node_BaseTimeline>(NewNode);

			UBlueprint* Blueprint = TimelineNode->GetBlueprint();
			if (Blueprint != nullptr)
			{
				TimelineNode->TimelineName = FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint);
				if (!bIsTemplateNode && AddNewTimeline(Blueprint, TimelineNode->TimelineName))
				{
					// clear off any existing error message now that the timeline has been added
					TimelineNode->ErrorMsg.Empty();
					TimelineNode->bHasCompilerMessage = false;
				}
			}
		};

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeTimelineNodeLambda);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}