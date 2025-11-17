// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistFormatters/KnotTrackCreator.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistStats.h"
#include "BlueprintAssistUtils.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_GetClassDefaults.h"
#include "K2Node_Knot.h"
#include "BlueprintAssistFormatters/BlueprintAssistCommentHandler.h"
#include "BlueprintAssistFormatters/FormatterInterface.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Stats/StatsMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogBAKnotPool, NoLogging, All);

FKnotPoolData::FKnotPoolData(UK2Node_Knot* Knot, const FVector2D& InRelativeOffset)
{
	KnotNode = Knot;
	RelativeOffset = InRelativeOffset;

	for (auto Pin : FBAUtils::GetLinkedToPinsIgnoringKnots(Knot))
	{
		LinkedTo.Add(Pin->PinId);
	}
}

void FKnotTrackCreator::Init(TSharedPtr<FFormatterInterface> InFormatter, TSharedPtr<FBAGraphHandler> InGraphHandler)
{
	Formatter = InFormatter;
	GraphHandler = InGraphHandler;

	const UBASettings& BASettings = UBASettings::Get();
	NodePadding = BASettings.BlueprintFormatterSettings.Padding;
	PinPadding = BASettings.BlueprintParameterPadding;
	TrackSpacing = BASettings.BlueprintKnotTrackSpacing;
}

void FKnotTrackCreator::FormatKnotNodes()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FKnotTrackCreator::FormatKnotNodes"), STAT_KnotTrackCreator_FormatNode, STATGROUP_BA_EdGraphFormatter);
	//UE_LOG(LogKnotTrackCreator, Warning, TEXT("### Format Knot Nodes"));

	MakeKnotTrack();

	MergeNearbyKnotTracks();

	// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tPre Expand"));
	// for (TSharedPtr<FKnotNodeTrack> Track : KnotTracks)
	// {
	// 	UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t%s"), *Track->ToString());
	// 	for (FBANodePinHandle& LinkedTo : Track->LinkedTo)
	// 	{
	// 		UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\t%s"), *LinkedTo.PinName.ToString());
	// 	}
	// }

	ExpandKnotTracks();

	if (!UBASettings_Advanced::HasDebugSetting("post-align"))
	{
		// after expanding it is possible that some tracks might be able to be have a new pin-aligned solution, check this again
		// only do this for single tracks which are not looping
		for (TSharedPtr<FGroupedTracks> TrackGroup : TrackGroups)
		{
			if (TrackGroup->Tracks.Num() == 1)
			{
				const TSharedPtr<FKnotNodeTrack> Track = TrackGroup->Tracks[0];
				if (!Track->bIsLoopingTrack && !Track->HasPinToAlignTo())
				{
					if (TryAlignTrackToEndPins(Track, Formatter->GetFormattedNodes().Array()))
					{
						// UE_LOG(LogTemp, Warning, TEXT("Successully aligned %s"), *Track->ToString());
						// GraphHandler->GetGraphOverlay()->DrawBounds(Track->GetTrackBounds(), FLinearColor::Red);
					}
				}
			}
		}
	}

	if (!UBASettings_Advanced::HasDebugSetting("Useless"))
	{
		RemoveUselessCreationNodes();
	}

	CreateKnotTracks();

	// Cleanup knot node pool
	for (auto& KnotNodeData : KnotNodePool)
	{
		if (FBAUtils::GetLinkedNodes(KnotNodeData.KnotNode).Num() == 0)
		{
			FBAUtils::DeleteNode(KnotNodeData.KnotNode);
		}
	}
	KnotNodePool.Empty();

	if (UBASettings::Get().bAddKnotNodesToComments)
	{
		AddKnotNodesToComments();
	}

	for (auto TrackGroup : TrackGroups)
	{
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("LIST TRACK GROUPS"));
		if (UBASettings_Advanced::HasDebugSetting("DebugTracks"))
		{
			GraphHandler->GetGraphOverlay()->DrawBounds(TrackGroup->GetBounds().ExtendBy(FMargin(4.0f)), FLinearColor::Red);
		}

		for (auto Track : TrackGroup->Tracks)
		{
			for (const auto & Creation : Track->KnotCreations)
			{
				if (Creation->CreatedKnot)
				{
					KnotTrackGroup.Add(Creation->CreatedKnot, TrackGroup);
					KnotCreationMap.Add(Creation->CreatedKnot, Creation);
				}
			}

			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t%s"), *Track->ToString());
			if (UBASettings_Advanced::HasDebugSetting("DebugTracks"))
			{
				GraphHandler->GetGraphOverlay()->DrawBounds(Track->GetTrackBounds(), Track->HasPinToAlignTo() ? FLinearColor::Yellow : FLinearColor::White);
			}
		}
	}

	for (auto Kvp : RelativeCreationMapping)
	{
		RelativeMapping.FindOrAdd(Kvp.Key->CreatedKnot).Add(Kvp.Value);
	}

	for (TSharedPtr<FGroupedTracks> Group : TrackGroups)
	{
		for (TSharedPtr<FKnotNodeTrack> Track : Group->Tracks)
		{
			TArray<UEdGraphNode*> RelatedNodes = Track->GetRelatedNodes();
			for (TSharedPtr<FKnotNodeCreation> Creation : Track->KnotCreations)
			{
				// make all creations in tracks related to each other
				for (TSharedPtr<FKnotNodeCreation> Other : Track->KnotCreations)
				{
					if (Creation == Other)
						continue;

					RelativeMapping.FindOrAdd(Creation->CreatedKnot).Add(Other->CreatedKnot);
				}

				// make all creations related to their nodes
				for (UEdGraphNode* RelatedNode : RelatedNodes)
				{
					RelativeMapping.FindOrAdd(Creation->CreatedKnot).Add(RelatedNode);

					// for looping tracks also make the node related to the track
					if (Track->bIsLoopingTrack)
					{
						RelativeMapping.FindOrAdd(Creation->CreatedKnot).Add(RelatedNode);
					}
				}
			}
		}
	}
}

void FKnotTrackCreator::CreateKnotTracks()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FKnotTrackCreator::CreateKnotTracks"), STAT_KnotTrackCreator_CreateKnotTracks, STATGROUP_BA_EdGraphFormatter);

	if (KnotTracks.Num() == 0)
	{
		return;
	}

	if (auto BP = GraphHandler->GetBlueprint())
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}

	// we sort tracks by
	// 1. exec pin track over parameter track 
	// 2. top-most-track-height 
	// 3. top-most parent pin 
	// 4. left-most
	const auto& TrackSorter = [](const TSharedPtr<FKnotNodeTrack> TrackA, const TSharedPtr<FKnotNodeTrack> TrackB)
	{
		const bool bIsExecPinA = FBAUtils::IsExecOrDelegatePin(TrackA->GetLastPin());
		const bool bIsExecPinB = FBAUtils::IsExecOrDelegatePin(TrackB->GetLastPin());

		if (bIsExecPinA != bIsExecPinB)
		{
			return bIsExecPinA > bIsExecPinB;
		}

		if (TrackA->GetTrackHeight() != TrackB->GetTrackHeight())
		{
			return TrackA->GetTrackHeight() < TrackB->GetTrackHeight();
		}

		if (TrackA->ParentPinPos.Y != TrackB->ParentPinPos.Y)
		{
			return TrackA->ParentPinPos.Y < TrackB->ParentPinPos.Y;
		}

		return TrackA->GetTrackBounds().GetSize().X < TrackB->GetTrackBounds().GetSize().X;
	};
	KnotTracks.Sort(TrackSorter);

	// we need to save the pin height since creating knot nodes for certain nodes (K2Node_LatentAbilityCall) will cause
	// the node to recreate its pins (including their guids) and so we fail to find the cached pin height 
	TMap<TSharedPtr<FKnotNodeTrack>, float> SavedPinHeight;
	for (TSharedPtr<FKnotNodeTrack> KnotTrack : KnotTracks)
	{
		UEdGraphPin* PinToAlignTo = KnotTrack->GetPinToAlignTo();
		if (PinToAlignTo != nullptr)
		{
			SavedPinHeight.Add(KnotTrack, GraphHandler->GetPinY(PinToAlignTo));
		}
	}

	for (TSharedPtr<FKnotNodeTrack> KnotTrack : KnotTracks)
	{
		if (!KnotTrack->ParentPin.IsValid() || KnotTrack->ParentPin.GetPin() == nullptr)
		{
			continue;
		}

		if (KnotTrack->KnotCreations.Num() == 0)
		{
			continue;
		}

		// break pins
		for (FBANodePinHandle& Link : KnotTrack->LinkedTo)
		{
			UEdGraphPin* ParentPin = KnotTrack->GetParentPin();
			ParentPin->BreakLinkTo(Link.GetPin());
		}

		// sort knot creations
		auto GraphCapture = GraphHandler->GetFocusedEdGraph();
		const auto CreationSorter = [GraphCapture](TSharedPtr<FKnotNodeCreation> CreationA, TSharedPtr<FKnotNodeCreation> CreationB)
		{
			UEdGraphPin* Pin = CreationA->PinToConnectToHandle.GetPin();

			if (FBAUtils::IsExecOrDelegatePin(Pin))
			{
				return CreationA->KnotPos.X > CreationB->KnotPos.X;
			}

			return CreationA->KnotPos.X < CreationB->KnotPos.X;
		};

		if (!KnotTrack->bIsLoopingTrack)
		{
			KnotTrack->KnotCreations.StableSort(CreationSorter);
		}

		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Creating knot track %s"), *FBAUtils::GetPinName(KnotTrack->GetParentPin()));
		// if (KnotTrack->PinToAlignTo.IsValid())
		// {
		// 	UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tShould make aligned track!"));
		// }

		UEdGraphPin* PinToAlignTo = KnotTrack->GetPinToAlignTo();

		TSharedPtr<FKnotNodeCreation> LastCreation = nullptr;
		const int NumCreations = KnotTrack->KnotCreations.Num();
		for (int i = 0; i < NumCreations; i++)
		{
			TSharedPtr<FKnotNodeCreation> Creation = KnotTrack->KnotCreations[i];

			FVector2D KnotPos = Creation->KnotPos;
			KnotPos.Y = KnotTrack->GetTrackHeight();

			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Making knot creation at %s %d"), *KnotPos.ToString(), i);
			// for (FBAGraphPinHandle Pin : Creation->PinHandlesToConnectTo)
			// {
			// 	UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tPin %s"), *FBAUtils::GetPinName(Pin.GetPin()));
			// }

			if (PinToAlignTo != nullptr)
			{
				KnotPos.Y = SavedPinHeight.Contains(KnotTrack) ? SavedPinHeight[KnotTrack] : GraphHandler->GetPinY(PinToAlignTo);
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Created knot aligned to %s"), *FBAUtils::GetNodeName(PinToAlignTo->GetOwningNode()));
			}

			if (!LastCreation.IsValid()) // create a knot linked to the first pin (the fallback pin)
			{
				FBANodePinHandle ParentPin = FBANodePinHandle(KnotTrack->GetParentPin());

				if (UK2Node_Knot* KnotNode = CreateKnotNode(Creation.Get(), KnotPos, ParentPin.GetPin()))
				{
					KnotNodesSet.Add(KnotNode);
					RelativeMapping.FindOrAdd(ParentPin->GetOwningNode()).Add(KnotNode);

					if (PinToAlignTo)
					{
						// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Made aligned node"));
						PinAlignedKnots.Add(KnotNode);
					}
				}

				LastCreation = Creation;
			}
			else if (LastCreation && LastCreation->CreatedKnot) // create a knot that connects to the last knot
			{
				UK2Node_Knot* ParentKnot = LastCreation->CreatedKnot;

				const bool bCreatePinAlignedKnot = LastCreation->PinHandlesToConnectTo.Num() == 1 && PinToAlignTo != nullptr;
				if (bCreatePinAlignedKnot && NumCreations == 1) // move the parent knot to the aligned x position
				{
					// UE_LOG(LogBlueprintAssist, Warning, TEXT("Create pin aligned!"));
					for (FBAGraphPinHandle& PinHandle : Creation->PinHandlesToConnectTo)
					{
						UEdGraphPin* Pin = PinHandle.GetPin();

						UEdGraphPin* ParentPin = Pin->Direction == EGPD_Input
							? ParentKnot->GetOutputPin()
							: ParentKnot->GetInputPin();
						ParentPin->MakeLinkTo(Pin);
					}
				}
				else
				{
					// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Create normal"));
					UEdGraphPin* LastPin = KnotTrack->GetLastPin();

					UEdGraphPin* PinOnLastKnot = LastPin->Direction == EGPD_Output
						? ParentKnot->GetInputPin()
						: ParentKnot->GetOutputPin();

					if (UK2Node_Knot* NewKnot = CreateKnotNode(Creation.Get(), KnotPos, PinOnLastKnot))
					{
						KnotNodesSet.Add(NewKnot);

						RelativeMapping.FindOrAdd(Creation->OwningKnotTrack->GetParentPin()->GetOwningNode()).Add(NewKnot);
						RelativeMapping.FindOrAdd(Creation->OwningKnotTrack->GetLastPin()->GetOwningNode()).Add(NewKnot);
					}

					LastCreation = Creation;
				}
			}
		}
	}
}

void FKnotTrackCreator::ExpandKnotTracks()
{
	if (BA_DEBUG("Expand"))
	{
		return;
	}

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FKnotTrackCreator::ExpandKnotTracks"), STAT_KnotTrackCreator_ExpandKnotTracks, STATGROUP_BA_EdGraphFormatter);
	// UE_LOG(LogKnotTrackCreator, Error, TEXT("### Expanding Knot Tracks"));
	// for (auto Elem : KnotTracks)
	// {
	// 	UE_LOG(LogKnotTrackCreator, Warning, TEXT("KnotTrack %s"), *FBAUtils::GetPinName(Elem->GetParentPin()));
	// }

	// sort tracks by:
	// 1. exec over parameter
	// 2. Highest track Y
	// 3. Smallest track width
	// 4. Parent pin height
	TSharedPtr<FBAGraphHandler> GraphHandlerCapture = GraphHandler;
	const auto& ExpandTrackSorter = [GraphHandlerCapture](const TSharedPtr<FKnotNodeTrack>& TrackA, const TSharedPtr<FKnotNodeTrack>& TrackB)
	{
		if (TrackA->bIsLoopingTrack != TrackB->bIsLoopingTrack)
		{
			return TrackA->bIsLoopingTrack < TrackB->bIsLoopingTrack;
		}

		// if (bIsExecPinA && TrackA->bIsLoopingTrack != TrackB->bIsLoopingTrack)
		// {
		// 	return TrackA->bIsLoopingTrack < TrackB->bIsLoopingTrack;
		// }

		if (TrackA->GetTrackHeight() != TrackB->GetTrackHeight())
		{
			return TrackA->bIsLoopingTrack
				? TrackA->GetTrackHeight() > TrackB->GetTrackHeight()
				: TrackA->GetTrackHeight() < TrackB->GetTrackHeight();
		}

		const bool bIsExecPinA = FBAUtils::IsExecOrDelegatePin(TrackA->GetLastPin());
		const bool bIsExecPinB = FBAUtils::IsExecOrDelegatePin(TrackB->GetLastPin());
		if (bIsExecPinA != bIsExecPinB)
		{
			return bIsExecPinA < bIsExecPinB;
		}

		const float WidthA = TrackA->GetTrackBounds().GetSize().X;
		const float WidthB = TrackB->GetTrackBounds().GetSize().X;
		if (WidthA != WidthB)
		{
			return TrackA->bIsLoopingTrack
				? WidthA > WidthB
				: WidthA < WidthB;
		}

		return GraphHandlerCapture->GetPinY(TrackA->GetLastPin()) < GraphHandlerCapture->GetPinY(TrackB->GetLastPin());
	};

	const auto& OverlappingTrackSorter = [GraphHandlerCapture](const TSharedPtr<FKnotNodeTrack>& TrackA, const TSharedPtr<FKnotNodeTrack>& TrackB)
	{
		if (TrackA->bIsLoopingTrack != TrackB->bIsLoopingTrack)
		{
			return TrackA->bIsLoopingTrack < TrackB->bIsLoopingTrack;
		}

		const bool bIsExecPinA = FBAUtils::IsExecOrDelegatePin(TrackA->GetLastPin());
		const bool bIsExecPinB = FBAUtils::IsExecOrDelegatePin(TrackB->GetLastPin());
		if (bIsExecPinA != bIsExecPinB)
		{
			return bIsExecPinA > bIsExecPinB;
		}

		const float WidthA = TrackA->GetTrackBounds().GetSize().X;
		const float WidthB = TrackB->GetTrackBounds().GetSize().X;
		if (WidthA != WidthB)
		{
			return TrackA->bIsLoopingTrack
				? WidthA > WidthB
				: WidthA < WidthB;
		}

		return GraphHandlerCapture->GetPinY(TrackA->GetLastPin()) < GraphHandlerCapture->GetPinY(TrackB->GetLastPin());
	};

	TArray<TSharedPtr<FKnotNodeTrack>> SortedTracks = KnotTracks;
	SortedTracks.StableSort(ExpandTrackSorter);

	TArray<TSharedPtr<FKnotNodeTrack>> PendingTracks = SortedTracks;

	// for (auto Track : SortedTracks)
	// {
	// 	UE_LOG(LogKnotTrackCreator, Warning, TEXT("Expanding tracks %s"), *Track->ToString());
	// 	if (Track->bIsLoopingTrack)
	// 	{
	// 		UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tIsLooping"));
	// 	}
	// }

	TSet<TSharedPtr<FGroupedTracks>> PlacedGroups;
	TSet<TSharedPtr<FKnotNodeTrack>> PlacedTracks;
	while (PendingTracks.Num() > 0)
	{
		TSharedPtr<FKnotNodeTrack> CurrentTrack = PendingTracks[0];
		PlacedTracks.Add(CurrentTrack);

		// const float TrackY = CurrentTrack->GetTrackHeight();

		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Process pending Track %s (%s)"), *FBAUtils::GetPinName(CurrentTrack->GetParentPin()), *FBAUtils::GetNodeName(CurrentTrack->GetParentPin()->GetOwningNode()));

		// check against all other tracks, and find ones which overlap with the current track
		TArray<TSharedPtr<FKnotNodeTrack>> OverlappingTracks;
		OverlappingTracks.Add(CurrentTrack);

		float CurrentLowestTrackHeight = CurrentTrack->GetTrackHeight();
		FSlateRect OverlappingBounds = CurrentTrack->GetTrackBounds();
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Current Track bounds %s"), *CurrentTrack->GetTrackBounds().ToString());
		bool bFoundCollision = true;
		do
		{
			bFoundCollision = false;
			for (TSharedPtr<FKnotNodeTrack> Track : SortedTracks)
			{
				if (PlacedTracks.Contains(Track))
				{
					continue;
				}

				if (OverlappingTracks.Contains(Track))
				{
					continue;
				}

				if (Track->bIsLoopingTrack != CurrentTrack->bIsLoopingTrack)
				{
					continue;
				}

				if (FBAUtils::IsExecPin(Track->GetParentPin()) != FBAUtils::IsExecPin(CurrentTrack->GetParentPin()))
				{
					continue;
				}

				// if looping tracks share the same related nodes then they should count as 'overlapping'
				if (Track->bIsLoopingTrack) // TODO: maybe we should do this for normal tracks too?
				{
					TSet<UEdGraphNode*> OverlappingSet;
					for (TSharedPtr<FKnotNodeTrack> Overlapping : OverlappingTracks)
					{
						OverlappingSet.Append(Overlapping->GetRelatedNodes());
					}

					TSet<UEdGraphNode*> TrackSet(Track->GetRelatedNodes());

					if (OverlappingSet.Intersect(TrackSet).Num() > 0)
					{
						OverlappingTracks.Add(Track);
						PlacedTracks.Add(Track);
						bFoundCollision = true;
						continue;
					}
				}

				FSlateRect TrackBounds = Track->GetTrackBounds();

				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tOverlapping Bounds %s | %s"), *OverlappingBounds.ToString(), *TrackBounds.ToString());
				if (FSlateRect::DoRectanglesIntersect(OverlappingBounds, TrackBounds))
				{
					OverlappingTracks.Add(Track);
					PlacedTracks.Add(Track);
					bFoundCollision = true;

					OverlappingBounds.Top = FMath::Min(Track->GetTrackHeight(), OverlappingBounds.Top);
					OverlappingBounds.Left = FMath::Min(TrackBounds.Left, OverlappingBounds.Left);
					OverlappingBounds.Right = FMath::Max(TrackBounds.Right, OverlappingBounds.Right);
					OverlappingBounds.Bottom = OverlappingBounds.Top + (OverlappingTracks.Num() * TrackSpacing);

					if (CurrentTrack->HasPinToAlignTo())
					{
						// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Removed pin to align to for %s"), *CurrentTrack->ToString());
						CurrentTrack->PinToAlignTo.SetPin(nullptr);
					}

					if (Track->HasPinToAlignTo())
					{
						// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Removed pin to align to for %s"), *Track->ToString());
						Track->PinToAlignTo.SetPin(nullptr);
					}
					// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tTrack %s colliding %s"), *FBAUtils::GetPinName(Track->GetParentPin()), *FBAUtils::GetPinName(CurrentTrack->GetParentPin()));
				}
			}
		}
		while (bFoundCollision);

		// if (OverlappingTracks.Num() == 1)
		// {
		// 	PendingTracks.Remove(CurrentTrack);
		// 	continue;
		// }

		TArray<TSharedPtr<FKnotNodeTrack>> ExecTracks;

		// Group overlapping tracks by node (expect for exec tracks)
		TArray<FGroupedTracks> OverlappingGroupedTracks;
		for (TSharedPtr<FKnotNodeTrack> Track : OverlappingTracks)
		{
			if (FBAUtils::IsExecOrDelegatePin(Track->GetParentPin()) && !Track->bIsLoopingTrack)
			{
				ExecTracks.Add(Track);
				continue;
			}

			const auto MatchesNode = [&Track](const FGroupedTracks& OtherTrack)
			{
				return Track->GetParentPin()->GetOwningNode() == OtherTrack.ParentNode;
			};

			FGroupedTracks* Group = OverlappingGroupedTracks.FindByPredicate(MatchesNode);
			if (Group)
			{
				Group->Tracks.Add(Track);
			}
			else
			{
				FGroupedTracks NewGroup;
				NewGroup.ParentNode = Track->GetParentPin()->GetOwningNode();
				NewGroup.Tracks.Add(Track);
				OverlappingGroupedTracks.Add(NewGroup);
			}
		}

		ExecTracks.StableSort(OverlappingTrackSorter);

		for (auto& Group : OverlappingGroupedTracks)
		{
			Group.Init();
			Group.Tracks.StableSort(OverlappingTrackSorter);
		}

		const auto& GroupSorter = [](const FGroupedTracks& GroupA, const FGroupedTracks& GroupB)
		{
			if (GroupA.bLooping != GroupB.bLooping)
			{
				return GroupA.bLooping < GroupB.bLooping;
			}

			return GroupA.Width < GroupB.Width;
		};
		OverlappingGroupedTracks.StableSort(GroupSorter);

		TSharedPtr<FGroupedTracks> AllGroup = MakeShareable(new FGroupedTracks());
		TrackGroups.Add(AllGroup);

		int TrackCount = 0;
		for (auto Track : ExecTracks)
		{
			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tSolo track %s"), *Track->ToString());
			Track->UpdateTrackHeight(CurrentLowestTrackHeight + (TrackCount * TrackSpacing));
			TrackCount += 1;
			AllGroup->Tracks.Add(Track);
		}

		for (FGroupedTracks& Group : OverlappingGroupedTracks)
		{
			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Group %s"), *FBAUtils::GetNodeName(Group.ParentNode));
			for (TSharedPtr<FKnotNodeTrack> Track : Group.Tracks)
			{
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tTrack %s"), *Track->ToString());
				Track->UpdateTrackHeight(CurrentLowestTrackHeight + (TrackCount * TrackSpacing));
				TrackCount += 1;
				AllGroup->Tracks.Add(Track);
			}
		}

		// For looping tracks we need to move the parent nodes down so the track is above the nodes
		if (CurrentTrack->bIsLoopingTrack)
		{
			int32 TrackOffsetY = FBAUtils::IsParameterPin(CurrentTrack->ParentPin.GetPin()) 
				? UBASettings::Get().BlueprintParameterKnotSettings.LoopingOffset.Y
				: UBASettings::Get().BlueprintExecutionKnotSettings.LoopingOffset.Y;

			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Fix looping!"));
			FSlateRect ExpandedBounds = AllGroup->GetBounds();
			const float Padding = CurrentTrack->bIsLoopingTrack
				? (TrackSpacing * 2 + TrackOffsetY)
				: TrackSpacing;

			ExpandedBounds.Bottom += Padding;

			TOptional<float> LoopingDelta;
			for (TSharedPtr<FKnotNodeTrack> Track : AllGroup->Tracks)
			{
				// compare the parent node and last node to see which one is the highest
				UEdGraphNode* ParentNode = Track->GetParentPin()->GetOwningNode();
				UEdGraphNode* LastNode = Track->GetLastPin()->GetOwningNode();
				float ParentDelta = ExpandedBounds.Bottom - GraphHandler->GetCachedNodeBounds(ParentNode).Top;
				float LastDelta = ExpandedBounds.Bottom - GraphHandler->GetCachedNodeBounds(LastNode).Top;

				float LargestDelta = FMath::Max(ParentDelta, LastDelta) + 1;

				LoopingDelta = FMath::Max(LoopingDelta.Get(0.0f), LargestDelta);   
			}

			if (LoopingDelta.IsSet())
			{
				if (CurrentTrack->bIsLoopingTrack)
				{
					TSet<UEdGraphNode*> RelatedNodes;
					for (auto Track : AllGroup->Tracks)
					{
						Track->UpdateTrackHeight(Track->GetTrackHeight() - LoopingDelta.GetValue());
						UEdGraphNode* ParentNode = Track->GetParentPin()->GetOwningNode();
						UEdGraphNode* LastNode = Track->GetLastPin()->GetOwningNode();
						RelatedNodes.Add(ParentNode);
						RelatedNodes.Add(LastNode);
					}

					TSet<UEdGraphNode*> VisitedNodes;
					for (UEdGraphNode* Node : RelatedNodes)
					{
						// UE_LOG(LogTemp, Warning, TEXT("Looping moving %s by %f"), *FBAUtils::GetNodeName(Node), LoopingDelta.GetValue());
						Formatter->SetNodeY_KeepingSpacingVisited(Node, Node->NodePosY + LoopingDelta.GetValue(), VisitedNodes);
					}

					// FBAUtils::PrintNodeArray(VisitedNodes.Array(), "Looping Moooved");
				}
			}
		}

		// Collide against other placed tracks
		for (auto Group : PlacedGroups)
		{
			FSlateRect GroupBounds = AllGroup->GetBounds();
			FSlateRect PlacedBounds = Group->GetBounds();
			// PlacedBounds.Bottom += TrackSpacing;

			float Delta = PlacedBounds.Bottom - GroupBounds.Top;
			if (FSlateRect::DoRectanglesIntersect(GroupBounds, PlacedBounds))
			{
				// UE_LOG(LogKnotTrackCreator, Error, TEXT("\tTESTINGMove Group by Delta %f"), Delta);

				if (CurrentTrack->bIsLoopingTrack)
				{
					TSet<UEdGraphNode*> RelatedNodes;
					for (auto Track : AllGroup->Tracks)
					{
						UEdGraphNode* ParentNode = Track->GetParentPin()->GetOwningNode();
						UEdGraphNode* LastNode = Track->GetLastPin()->GetOwningNode();
						RelatedNodes.Append(Formatter->GetRowAndChildren(ParentNode));
						RelatedNodes.Append(Formatter->GetRowAndChildren(LastNode));
					}

					// RELATIVE POS ENABLE THIS NODES
					for (auto Track : Group->Tracks)
					{
						Track->UpdateTrackHeight(Track->GetTrackHeight() - Delta);
					}

					for (auto Node : RelatedNodes)
					{
						Formatter->SetNodePos(Node, Node->NodePosX, Node->NodePosY + Delta);
					}
				}
				else
				{
					for (TSharedPtr<FKnotNodeTrack> KnotNodeTrack : AllGroup->Tracks)
					{
						if (KnotNodeTrack->HasPinToAlignTo())
						{
							KnotNodeTrack->PinToAlignTo.SetPin(nullptr);
						}

						KnotNodeTrack->UpdateTrackHeight(KnotNodeTrack->GetTrackHeight() + Delta);
					}
				}
			}
		}

		PlacedGroups.Add(AllGroup);

		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("TRACK GROUP START %d"), CurrentTrack->bIsLoopingTrack);
		// for (auto Track : AllGroup->Tracks)
		// {
		// 	UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t%s"), *Track->ToString());
		// }
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("TRACK GROUP END"));

		for (TSharedPtr<FKnotNodeTrack> Track : PlacedTracks)
		{
			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tPlaced track %s"), *Track->ToString());
			PendingTracks.Remove(Track);
		}

		TSet<UEdGraphNode*> TrackNodes = CurrentTrack->GetNodes(GraphHandler->GetFocusedEdGraph());

		FSlateRect ExpandedBounds = AllGroup->GetBounds();// OverlappingBounds;
		const float Padding = CurrentTrack->bIsLoopingTrack ? TrackSpacing * 2 : TrackSpacing;
		ExpandedBounds.Bottom += Padding;

		// collide against each track's related nodes
		TOptional<float> RelatedBottom;
		const FSlateRect ContractedBounds = ExpandedBounds.InsetBy(FMargin(16, 0)); // contract bounds in x slightly
		for (auto Track : AllGroup->Tracks)
		{
			// UE_LOG(LogTemp, Warning, TEXT("GROUP Collision checking track %s"), *Track->ToString());
			for (UEdGraphNode* RelatedNode : Track->GetRelatedNodes())
			{
				// check bounds of each node for more accurate collision
				if (TSharedPtr<FFormatterInterface> ChildFormatter = Formatter->GetChildFormatter(RelatedNode))
				{
					UEdGraphNode* RootNode = ChildFormatter->GetRootNode();
					for (UEdGraphNode* FormattedNode : ChildFormatter->GetFormattedNodes())
					{
						// skip the root node
						if (FormattedNode == RootNode)
						{
							continue;
						}

						const FSlateRect NodeBounds = FBAUtils::GetCachedNodeBounds(GraphHandler, FormattedNode);
						if (FSlateRect::DoRectanglesIntersect(NodeBounds, ContractedBounds))
						{
							RelatedBottom = RelatedBottom.IsSet() ? FMath::Max(NodeBounds.Bottom, RelatedBottom.GetValue()) : NodeBounds.Bottom;
						}
					}
				}
			}
		}

		// move expanded bounds down
		if (RelatedBottom.IsSet())
		{
			float RelatedDeltaY = (RelatedBottom.GetValue() + Padding) - ExpandedBounds.Top;
			ExpandedBounds = ExpandedBounds.OffsetBy(FVector2D(0, RelatedDeltaY));

			// move all tracks down
			for (TSharedPtr<FKnotNodeTrack> Track : AllGroup->Tracks)
			{
				Track->UpdateTrackHeight(Track->GetTrackHeight() + RelatedDeltaY);
			}
		}

		// find the top of the tallest node the track block is colliding with
		TOptional<float> CollisionTop;

		if (!UBASettings_Advanced::HasDebugSetting("PushAligned"))
		{
			// collide against other aligned pending tracks
			for (TSharedPtr<FKnotNodeTrack> AlignedTrack : PendingTracks)
			{
				if (UEdGraphPin* AlignedPin = AlignedTrack->GetPinToAlignTo())
				{
					if (FBAUtils::IsExecPin(AlignedPin))
					{
						UEdGraphNode* Node = AlignedPin->GetOwningNode();

						bool bSkipNode = false;
						for (TSharedPtr<FKnotNodeTrack> Track : AllGroup->Tracks)
						{
							if (Node == Track->GetParentPin()->GetOwningNode() || Node == Track->GetLastPin()->GetOwningNode())
							{
								bSkipNode = true;
								break;
							}
						}

						if (bSkipNode)
						{
							continue;
						}

						const FSlateRect AlignedTrackBounds = AlignedTrack->GetTrackBounds();
						if (FSlateRect::DoRectanglesIntersect(AlignedTrackBounds, ExpandedBounds))
						{
							float DeltaY = ExpandedBounds.Bottom - AlignedTrackBounds.Top;
							Formatter->SetNodeY_KeepingSpacing(Node, Node->NodePosY + DeltaY);
						}
					}
				}
			}
		}

		// collide against nodes
		for (UEdGraphNode* Node : Formatter->GetFormattedNodes())
		{
			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Collision check for node %s"), *FBAUtils::GetNodeName(Node));
			bool bSkipNode = false;
			// for (TSharedPtr<FKnotNodeTrack> Track : PlacedTracks)
			for (TSharedPtr<FKnotNodeTrack> Track : AllGroup->Tracks)
			{
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tAGAINST track %s"), *Track->ToString());
				if (Node == Track->GetParentPin()->GetOwningNode() || Node == Track->GetLastPin()->GetOwningNode())
				{
					// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\tSkipping node %s"), *FBAUtils::GetNodeName(Node));
					bSkipNode = true;
					break;
				}

				if (auto AlignedPin = Track->GetPinToAlignTo())
				{
					if (Node == AlignedPin->GetOwningNode())
					{
						// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\tSkipping node aligned %s"), *FBAUtils::GetNodeName(Node));
						bSkipNode = true;
						break;
					}
				}
			}

			if (bSkipNode && !CurrentTrack->bIsLoopingTrack)
			{
				continue;
			}

			const FSlateRect NodeBounds = GraphHandler->GetCachedNodeBounds(Node);
			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\tChecking collision for %s | %s | %s"), *FBAUtils::GetNodeName(Node), *NodeBounds.ToString(), *ExpandedBounds.ToString());

			if (FSlateRect::DoRectanglesIntersect(NodeBounds, ExpandedBounds))
			{
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\t\tCollision with %s"), *FBAUtils::GetNodeName(Node));
				CollisionTop = CollisionTop.IsSet() ? FMath::Min(NodeBounds.Top, CollisionTop.GetValue()) : NodeBounds.Top;


				// if (CurrentTrack->bIsLoopingTrack)
				// {
				// 	float DeltaY = CollisionTop.GetValue() - ExpandedBounds.Bottom;
				// 	CurrentTrack->UpdateTrackHeight(CurrentTrack->GetTrackHeight() + DeltaY + 1);
				// }
				// else
				{
					float DeltaY = ExpandedBounds.Bottom - CollisionTop.GetValue();
					Formatter->SetNodeY_KeepingSpacing(Node, Node->NodePosY + DeltaY);
				}

				for (TSharedPtr<FKnotNodeTrack> Track : AllGroup->Tracks)
				{
					for (auto Creation : Track->KnotCreations)
					{
						RelativeCreationMapping.Add(Creation, Node);
					}
				}


				// if (CurrentTrack->bIsLoopingTrack)
				// {
				// 	Formatter->SetNodeY_KeepingSpacing(Node, Node->NodePosY + DeltaY);
				// 	UE_LOG(LogKnotTrackCreator, Error, TEXT("Moving looping track? %s"), *CurrentTrack->ToString());
				// 	// for (auto Track : AllGroup->Tracks)
				// 	// {
				// 	// 	Track->UpdateTrackHeight(Track->GetTrackHeight() - DeltaY);
				// 	// }
				// }
			}

			// ExpandedBounds = AllGroup->GetBounds();
			// ExpandedBounds.Bottom += Padding;
		}

		if (!CollisionTop.IsSet())
		{
			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\tSkip no collision top"));
			continue;
		}
	}
}

void FKnotTrackCreator::RemoveUselessCreationNodes()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FKnotTrackCreator::RemoveUselessCreationNodes"), STAT_KnotTrackCreator_RemoveUselessCreationNodes, STATGROUP_BA_EdGraphFormatter);
	for (TSharedPtr<FKnotNodeTrack> Track : KnotTracks)
	{
		if (Track->bIsLoopingTrack)
		{
			continue;
		}

		// UE_LOG(LogTemp, Warning, TEXT("CHECKING TRACK %f"), Track->GetTrackHeight());

		TSharedPtr<FKnotNodeCreation> LastValidCreation;
		TSharedPtr<FKnotNodeCreation> LastDeletedCreation;
		TArray<TSharedPtr<FKnotNodeCreation>> CreationsCopy = Track->KnotCreations;

		for (TSharedPtr<FKnotNodeCreation> Creation : CreationsCopy)
		{
			// UE_LOG(LogTemp, Warning, TEXT("Iterate %s %f"), *Creation->ToString(), Creation->KnotPos.X);
			// for (auto Pin : Creation->PinHandlesToConnectTo)
			// {
			// 	UE_LOG(LogTemp, Warning, TEXT("\t%s"), *Pin.ToString());
			// }
			
			const float PinHeight = GraphHandler->GetPinY(Creation->GetPinToConnectTo());

			const bool bHasOneConnection = Creation->PinHandlesToConnectTo.Num() == 1;

			// add the last deleted creation node's info into this creation node
			if (LastDeletedCreation)
			{
				Creation->PinHandlesToConnectTo.Add(LastDeletedCreation->PinToConnectToHandle);
				Creation->PinHandlesToConnectTo.Append(LastDeletedCreation->PinHandlesToConnectTo);
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tConsuming Last %s"), *LastDeletedCreation->ToString());
				LastDeletedCreation = nullptr;
			}

			bool bCreationValid = true;
			if (bHasOneConnection)
			{
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tChecking Pin %s %f | %f"), *FBAUtils::GetPinName(Creation->GetPinToConnectTo(), true), PinHeight, Track->GetTrackHeight());

				// give our pins to the last valid creation
				if (FMath::Abs(PinHeight - Track->GetTrackHeight()) <= UBASettings::Get().CullKnotVerticalThreshold)
				{
					// UE_LOG(LogTemp, Warning, TEXT("\tDeleting"));
					if (LastValidCreation)
					{
						// UE_LOG(LogTemp, Warning, TEXT("\t\tAdding to previous valid %s"), *LastValidCreation->ToString());
						// for (FBAGraphPinHandle PinHandlesToConnectTo : Creation->PinHandlesToConnectTo)
						// {
						// 	UE_LOG(LogTemp, Warning, TEXT("\t\t\t%s"), *PinHandlesToConnectTo.ToString());
						// }
						LastValidCreation->PinHandlesToConnectTo.Add(Creation->PinToConnectToHandle);
						LastValidCreation->PinHandlesToConnectTo.Append(Creation->PinHandlesToConnectTo);
					}
					else
					{
						LastDeletedCreation = Creation;
					}

					Track->KnotCreations.Remove(Creation);
					bCreationValid = false;
				}
			}

			if (bCreationValid && Creation)
			{
				LastValidCreation = Creation;
			}
		}
	}

	for (int i = KnotTracks.Num() - 1; i >= 0; --i)
	{
		if (KnotTracks[i]->KnotCreations.Num() == 0)
		{
			KnotTracks.RemoveAt(i);
		}
	}
}

void FKnotTrackCreator::RemoveKnotNodes(const TArray<UEdGraphNode*>& NodeTree)
{
	FBANodeData* RootData = GraphHandler->GetGraphData().GetNodeDataPtr(Formatter->GetRootNode());

	TArray<UEdGraphNode_Comment*> CommentNodes = FBAUtils::GetCommentNodesFromGraph(GraphHandler->GetFocusedEdGraph());
	for (UEdGraphNode* Node : NodeTree)
	{
		/** Delete all connections for each knot node */
		if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(Node))
		{
			const bool bUseKnotPool = UBASettings::Get().bUseKnotNodePool && UBASettings::Get().bCreateKnotNodes;

			// if we are using the knot node pool, save the relative location
			if (bUseKnotPool)
			{
				FVector2D RelativeRoot = FVector2D::Zero();
				const FBANodeData* KnotData = GraphHandler->GetGraphData().GetNodeDataPtr(KnotNode);
				if (KnotData && RootData)
				{
					RelativeRoot.X = KnotData->Last.X - RootData->Last.X;
					RelativeRoot.Y = KnotData->Last.Y - RootData->Last.Y;
				}
				else
				{
					RelativeRoot.X = KnotNode->NodePosX - Formatter->GetRootNode()->NodePosX;
					RelativeRoot.Y = KnotNode->NodePosY - Formatter->GetRootNode()->NodePosY;
				}
				KnotNodePool.Add(FKnotPoolData(KnotNode, RelativeRoot));
			}

			// disconnect and remove from comments
			{
				FBAUtils::DisconnectKnotNode(KnotNode);

				if (FCommentHandler* CH = Formatter->GetCommentHandler())
				{
					CH->DeleteNode(KnotNode);
				}

				for (auto Comment : CommentNodes)
				{
					if (Comment->GetNodesUnderComment().Contains(KnotNode))
					{
						FBAUtils::RemoveNodeFromComment(Comment, KnotNode);
					}
				}
			}

			// delete the knots if we don't use a knot node pool
			if (!bUseKnotPool)
			{
				FBAUtils::DeleteNode(KnotNode);
			}
		}
	}
}

UK2Node_Knot* FKnotTrackCreator::CreateKnotNode(FKnotNodeCreation* Creation, const FVector2D& Position, UEdGraphPin* ParentPin)
{
	if (!Creation)
	{
		return nullptr;
	}

	if (!ParentPin)
	{
		UE_LOG(LogBlueprintAssist, Error,
			TEXT("[%hs] Tried to create knot node with null parent pin\nPlease report on [https://github.com/fpwong/BlueprintAssistWiki/issues] if you are able to replicate this"),
			__FUNCTION__);
	}

	UK2Node_Knot* OptionalNodeToReuse = nullptr;
	if (UBASettings::Get().bUseKnotNodePool && KnotNodePool.Num() > 0)
	{
		OptionalNodeToReuse = GetKnotNodeFromPool(Creation, Position, ParentPin);
	}
	else
	{
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Failed to find?"));
	}

	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();
	if (UK2Node_Knot* CreatedNode = Creation->CreateKnotNode(Position, ParentPin, OptionalNodeToReuse, Graph))
	{
		if (UEdGraphPin* MainPinToConnectTo = Creation->PinToConnectToHandle.GetPin())
		{
			KnotNodeOwners.Add(CreatedNode, MainPinToConnectTo->GetOwningNode());
		}
		else
		{
			UE_LOG(LogBlueprintAssist, Error,
				TEXT("[%hs] Knot node has no pin to connect to %s\nPlease report on [https://github.com/fpwong/BlueprintAssistWiki/issues] if you are able to replicate this"),
				__FUNCTION__, *Creation->ToString());
		}
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Created node %d for %s"), CreatedNode, *FBAUtils::GetNodeName(ParentPin->GetOwningNode()));

		return CreatedNode; //Creation->CreateKnotNode(Position, ParentPin, OptionalNodeToReuse, GraphHandler->GetFocusedEdGraph());
	}
	else
	{
		UE_LOG(LogBlueprintAssist, Error,
			TEXT("[%hs] Failed to create knot node for %s\nPlease report on [https://github.com/fpwong/BlueprintAssistWiki/issues] if you are able to replicate this"),
			__FUNCTION__, *Creation->ToString());
		return nullptr;
	}
}

UK2Node_Knot* FKnotTrackCreator::GetKnotNodeFromPool(FKnotNodeCreation* Creation, const FVector2D& Position, UEdGraphPin* ParentPin)
{
	if (UBASettings::Get().bUseKnotNodePool && KnotNodePool.Num() > 0)
	{
		float Dist = FLT_MAX;
		int ClosestIndex = -1;

		UEdGraphNode* RootNode = Formatter->GetRootNode();

		TSet<int> Matches;

		TSet<FGuid> NewLinkedTo = { Creation->OwningKnotTrack->ParentPin.PinId };
		UE_LOG(LogBAKnotPool, Log, TEXT("Creation %s %s"), *Creation->OwningKnotTrack->ParentPin->PinId.ToString(), *FBAUtils::GetPinName(Creation->OwningKnotTrack->ParentPin.GetPin(), true));
		for (auto& Handle : Creation->PinHandlesToConnectTo)
		{
			if (FBAUtils::IsKnotNode(Handle.GetPin()->GetOwningNode()))
			{
				continue;
			}

			NewLinkedTo.Add(Handle.PinId);
			UE_LOG(LogBAKnotPool, Log, TEXT("Creation %s %s"), *Handle.GetPin()->PinId.ToString(), *FBAUtils::GetPinName(Handle.GetPin(), true));
		}

		for (int i = KnotNodePool.Num() - 1; i >= 0; --i)
		{
			const FKnotPoolData& Item = KnotNodePool[i];

			UE_LOG(LogBAKnotPool, Log, TEXT("\tChecking knot pool %s"), *Item.KnotNode->NodeGuid.ToString())
			for (auto Guid : Item.LinkedTo)
			{
				UE_LOG(LogBAKnotPool, Log, TEXT("\t\tExisting %s"), *Guid.ToString());
			}

			// find knots which have the same links
			if (NewLinkedTo.Difference(Item.LinkedTo).Num() == 0)
			{
				Matches.Add(i);
				UE_LOG(LogBAKnotPool, Log, TEXT("\t\tFound match"));
			}
		}

		// find any knots which match the links
		for (int i = KnotNodePool.Num() - 1; i >= 0; --i)
		{
			if (Matches.Num())
			{
				if (!Matches.Contains(i))
				{
					continue;
				}
			}

			const FKnotPoolData& Item = KnotNodePool[i];

			// sort the knot nodes by relative dist
			const FVector2D Pos = Position - FBAUtils::GetKnotNodeSize() * 0.5f;
			FVector2D RelativeDist(Pos.X - RootNode->NodePosX, Pos.Y - RootNode->NodePosY);

			UE_LOG(LogBAKnotPool, Log, TEXT("Searching relative %s | %s"), *RelativeDist.ToString(), *Item.RelativeOffset.ToString());

			const float NewDist = FVector2D::DistSquared(RelativeDist, Item.RelativeOffset);
			if (NewDist < Dist)
			{
				Dist = NewDist;
				ClosestIndex = i;
			}
		}

		if (ClosestIndex >= 0)
		{

			UK2Node_Knot* Knot = KnotNodePool[ClosestIndex].KnotNode;
			KnotNodePool.RemoveAt(ClosestIndex);

			UE_LOG(LogBAKnotPool, Log, TEXT("Found closest dist %f %s"), Dist, *Knot->NodeGuid.ToString());
			return Knot;
		}
		else
		{
			UE_LOG(LogBAKnotPool, Warning, TEXT("Failed to find knot to reuse"));
		}
	}

	return nullptr;
}

bool FKnotTrackCreator::TryAlignTrackToEndPins(TSharedPtr<FKnotNodeTrack> Track, const TArray<UEdGraphNode*>& AllNodes)
{
	const float ParentPinY = GraphHandler->GetPinY(Track->GetParentPin());
	const float LastPinY = GraphHandler->GetPinY(Track->GetLastPin());
	bool bPreferParentPin = ParentPinY > LastPinY;

	if (FBAUtils::IsExecOrDelegatePin(Track->GetParentPin()))
	{
		bPreferParentPin = true;
	}

	for (int i = 0; i < 2; ++i)
	{
		if (i == 1)
		{
			bPreferParentPin = !bPreferParentPin;
		}

		// FString PreferPinStr = bPreferParentPin ? "true" : "false";
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("AlignTrack %s ParentY %f (%s) | LastPinY %f (%s) | PreferParent %s"), *Track->ToString(), ParentPinY, *FBAUtils::GetPinName(Track->GetParentPin()), LastPinY,
		//        *FBAUtils::GetPinName(Track->GetLastPin()), *PreferPinStr);

		UEdGraphPin* SourcePin = bPreferParentPin ? Track->GetParentPin() : Track->GetLastPin();
		UEdGraphPin* OtherPin = bPreferParentPin ? Track->GetLastPin() : Track->GetParentPin();

		const FVector2D SourcePinPos = FBAUtils::GetPinPos(GraphHandler, SourcePin);
		const FVector2D OtherPinPos = FBAUtils::GetPinPos(GraphHandler, OtherPin);

		// don't align if the last pin is lower than the parent pin 
		if (!bPreferParentPin && LastPinY < ParentPinY)
		{
			continue;
		}

		const FIntPoint Padding = FBAUtils::IsParameterPin(OtherPin)
			? PinPadding
			: NodePadding;

		const FVector2D Point
			= SourcePin->Direction == EGPD_Output
			? FVector2D(OtherPinPos.X - Padding.X, SourcePinPos.Y)
			: FVector2D(OtherPinPos.X + Padding.X, SourcePinPos.Y);

		// GraphHandler->GetGraphOverlay()->DrawLine(SourcePinPos, Point);

		// UE_LOG(LogKnotTrackCreator, Error, TEXT("Checking Point %s | %s"), *Point.ToString(), *FBAUtils::GetNodeName(SourcePin->GetOwningNode()));

		bool bAnyCollision = false;

		for (UEdGraphNode* NodeToCollisionCheck : AllNodes)
		{
			FSlateRect CollisionBounds = FBAUtils::GetCachedNodeBounds(GraphHandler, NodeToCollisionCheck).ExtendBy(FMargin(0, TrackSpacing - 1));

			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Collision check against %s | %s | %s"), *FBAUtils::GetNodeName(NodeToCollisionCheck), *CollisionBounds.ToString(), *Point.ToString());

			if (NodeToCollisionCheck == SourcePin->GetOwningNode() || NodeToCollisionCheck == OtherPin->GetOwningNode())
			{
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tSkipping node"));
				continue;
			}

			if (FBAUtils::LineRectIntersection(CollisionBounds, SourcePinPos, Point))
			{
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tFound collision"));
				bAnyCollision = true;
				break;
			}
		}

		for (TSharedPtr<FKnotNodeTrack> OtherTrack : KnotTracks)
		{
			if (OtherTrack == Track)
			{
				continue;
			}

			// Possibly revert back to rect collision check
			if (FBAUtils::LineRectIntersection(OtherTrack->GetTrackBounds().ExtendBy(FMargin(0, TrackSpacing * 0.25f)), SourcePinPos, Point))
			{
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Track %s colliding with %s"), *Track->ToString(), *OtherTrack->ToString());
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tStart %s End %s"), *SourcePinPos.ToString(), *Point.ToString());
				// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tRect %s"), *MyTrackBounds.ToString());
				bAnyCollision = true;
				break;
			}
		}

		if (!bAnyCollision)
		{
			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("sucessfully found easy solution!"));
			Track->PinAlignedX = Point.X;
			Track->UpdateTrackHeight(SourcePinPos.Y);
			Track->PinToAlignTo.SetPin(SourcePin);
			return true;
		}
	}

	return false;
}

bool FKnotTrackCreator::DoesPinNeedTrack(UEdGraphPin* Pin, const TArray<UEdGraphPin*>& LinkedTo)
{
	if (LinkedTo.Num() == 0)
	{
		return false;
	}

	// if the pin is linked to multiple linked nodes, we need a knot track
	if (LinkedTo.Num() > 1)
	{
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Multiple linked to?"));
		return true;
	}

	// otherwise the pin is linked to exactly 1 node, run a collision check
	UEdGraphPin* OtherPin = LinkedTo[0];

	// need pin if there are any collisions
	return AnyCollisionBetweenPins(Pin, OtherPin);
}

bool FKnotTrackCreator::AnyCollisionBetweenPins(UEdGraphPin* Pin, UEdGraphPin* OtherPin)
{
	TSet<UEdGraphNode*> FormattedNodes = Formatter->GetFormattedNodes();

	const FVector2D PinPos = FBAUtils::GetPinPos(GraphHandler, Pin);
	const FVector2D OtherPinPos = FBAUtils::GetPinPos(GraphHandler, OtherPin);

	// UE_LOG(LogTemp, Warning, TEXT("Checking pins %s %s"), *FBAUtils::GetPinName(Pin), *FBAUtils::GetPinName(OtherPin));

	return NodeCollisionBetweenLocation(PinPos, OtherPinPos, { Pin->GetOwningNode(), OtherPin->GetOwningNode() });
}

bool FKnotTrackCreator::NodeCollisionBetweenLocation(FVector2D Start, FVector2D End, TSet<UEdGraphNode*> IgnoredNodes)
{
	TSet<UEdGraphNode*> FormattedNodes = Formatter->GetFormattedNodes();

	for (UEdGraphNode* NodeToCollisionCheck : FormattedNodes)
	{
		if (IgnoredNodes.Contains(NodeToCollisionCheck))
		{
			continue;
		}

		FSlateRect NodeBounds = FBAUtils::GetCachedNodeBounds(GraphHandler, NodeToCollisionCheck);//.ExtendBy(FMargin(0, TrackSpacing - 1));
		if (FBAUtils::LineRectIntersection(NodeBounds, Start, End))
		{
			// UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tNode collision! %s"), *FBAUtils::GetNodeName(NodeToCollisionCheck));
			return true;
		}
	}

	return false;
}

void FKnotTrackCreator::Reset()
{
	KnotNodesSet.Reset();
	KnotTracks.Reset();
	KnotNodeOwners.Reset();
}

void FKnotTrackCreator::AddNomadKnotsIntoComments()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FKnotTrackCreator::AddNomadKnotsIntoComments"), STAT_KnotTrackCreator_AddNomadKnotsIntoComments, STATGROUP_BA_EdGraphFormatter);
	FCommentHandler* CommentHandler = Formatter->GetCommentHandler();
	if (!CommentHandler)
	{
		return;
	}

	if (UBASettings_Advanced::HasDebugSetting("AddNomadKnots"))
	{
		return;
	}

	for (UEdGraphNode_Comment* Comment : CommentHandler->GetComments())
	{
		const FMargin Padding = CommentHandler->GetCommentPadding(Comment);
		FSlateRect CommentBounds = CommentHandler->GetCommentBounds(Comment).InsetBy(Padding);

		if (UBASettings_Advanced::HasDebugSetting("DebugNomadKnots"))
		{
			GraphHandler->GetGraphOverlay()->DrawBounds(CommentBounds);
		}

		for (TSharedPtr<FKnotNodeTrack> Track : KnotTracks)
		{
			if (Track->KnotCreations.Num() != 1)
			{
				continue;
			}

			if (TSharedPtr<FKnotNodeCreation> Creation = Track->KnotCreations[0])
			{
				if (UK2Node_Knot* CreatedKnotNode = Creation->CreatedKnot)
				{
					// Add nomad knots if they are located in a comment
					const FVector2D KnotPos(CreatedKnotNode->NodePosX, CreatedKnotNode->NodePosY);
					if (CommentBounds.ContainsPoint(KnotPos))
					{
						// UE_LOG(LogTemp, Warning, TEXT("Added knot %s into %s"), *FBAUtils::GetNodeName(CreatedKnotNode), *FBAUtils::GetNodeName(Comment));
						CommentHandler->AddNodeIntoComment(Comment, CreatedKnotNode);

						if (UBASettings_Advanced::HasDebugSetting("DebugNomadKnots"))
						{
							GraphHandler->GetGraphOverlay()->DrawBounds(FSlateRect(KnotPos.X - 10.0f, KnotPos.Y - 10.0f, KnotPos.X + 10.0f, KnotPos.Y + 10.0f));
						}
					}
				}
			}
		}
	}
}

void FKnotTrackCreator::MakeKnotTrack()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FKnotTrackCreator::MakeKnotTrack"), STAT_KnotTrackCreator_MakeKnotTrack, STATGROUP_BA_EdGraphFormatter);
	const TSet<UEdGraphNode*> FormattedNodes = Formatter->GetFormattedNodes();

	const auto& NotFormatted = [FormattedNodes](UEdGraphPin* Pin)
	{
		return Pin != nullptr ? !FormattedNodes.Contains(Pin->GetOwningNode()) : true;
	};

	// iterate across the pins of all nodes and determine if they require a knot track
	for (UEdGraphNode* MyNode : FormattedNodes)
	{
		// make tracks for input exec pins
		TArray<TSharedPtr<FKnotNodeTrack>> PreviousTracks;
		for (UEdGraphPin* MyPin : FBAUtils::GetExecOrDelegatePins(MyNode, EGPD_Input))
		{
			TArray<UEdGraphPin*> LinkedTo = MyPin->LinkedTo;
			LinkedTo.RemoveAll(NotFormatted);
			if (LinkedTo.Num() == 0)
			{
				continue;
			}

			if (UBASettings::Get().ExecutionWiringStyle == EBAWiringStyle::AlwaysMerge)
			{
				MakeKnotTracksForLinkedExecPins(MyPin, LinkedTo, PreviousTracks);
			}
			else
			{
				for (UEdGraphPin* Pin : LinkedTo)
				{
					MakeKnotTracksForLinkedExecPins(MyPin, { Pin }, PreviousTracks);
				}
			}
		}
	}

	const auto& SkipClassDefaults = [](UEdGraphPin* Pin)
	{
		if (Pin && Pin->GetOwningNode()->IsA<UK2Node_GetClassDefaults>())
		{
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
			{
				return true;
			}
		}

		return false;
	};

	for (UEdGraphNode* MyNode : FormattedNodes)
	{
		// make tracks for output parameter pins
		TArray<TSharedPtr<FKnotNodeTrack>> PreviousTracks;
		for (UEdGraphPin* MyPin : FBAUtils::GetParameterPins(MyNode, EGPD_Output))
		{
			TArray<UEdGraphPin*> LinkedTo = MyPin->LinkedTo;
			LinkedTo.RemoveAll(NotFormatted);
			LinkedTo.RemoveAll(SkipClassDefaults);
			if (LinkedTo.Num() == 0)
			{
				continue;
			}

			if (UBASettings::Get().ParameterWiringStyle == EBAWiringStyle::AlwaysMerge)
			{
				MakeKnotTracksForParameterPins(MyPin, LinkedTo, PreviousTracks);
			}
			else
			{
				for (UEdGraphPin* Pin : LinkedTo)
				{
					MakeKnotTracksForParameterPins(MyPin, { Pin }, PreviousTracks);
				}
			}
		}
	}
}

TSharedPtr<FKnotNodeTrack> FKnotTrackCreator::MakeKnotTracksForLinkedExecPins(UEdGraphPin* ParentPin, TArray<UEdGraphPin*> LinkedPins, TArray<TSharedPtr<FKnotNodeTrack>>& PreviousTracks)
{
	FVector2D ParentPinPos = FBAUtils::GetPinPos(GraphHandler, ParentPin);
	UEdGraphNode* ParentNode = ParentPin->GetOwningNode();

	// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Processing knot track for parent pin %s"), *FBAUtils::GetPinName(ParentPin, true));
	// for (auto Pin : LinkedPins)
	// {
	// 	UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t%s"), *FBAUtils::GetPinName(Pin));
	// }

	// check for looping pins, these are pins where 
	// the x position of the pin is less than the x value of the parent pin
	TArray<UEdGraphPin*> LoopingPins;
	for (UEdGraphPin* LinkedPin : LinkedPins)
	{
		const FVector2D LinkedPinPos = FBAUtils::GetPinPos(GraphHandler, LinkedPin);
		if (LinkedPinPos.X > ParentPinPos.X && FMath::Abs(LinkedPinPos.X - ParentPinPos.X) > 200)
		{
			LoopingPins.Add(LinkedPin);
		}
	}

	const FIntPoint& LoopingOffset = UBASettings::Get().BlueprintExecutionKnotSettings.LoopingOffset;

	// create looping tracks
	for (UEdGraphPin* OtherPin : LoopingPins)
	{
		TArray<UEdGraphPin*> TrackPins = { OtherPin };
		TSharedPtr<FKnotNodeTrack> KnotTrack = MakeShared<FKnotNodeTrack>(Formatter, GraphHandler, ParentPin, TrackPins, true);
		KnotTracks.Add(KnotTrack);

		const FVector2D OtherPinPos = FBAUtils::GetPinPos(GraphHandler, OtherPin);

		const FVector2D FirstKnotPos(ParentPinPos.X + 20 + LoopingOffset.X, KnotTrack->GetTrackHeight());
		TSharedPtr<FKnotNodeCreation> FirstLoopingKnot = MakeShared<FKnotNodeCreation>(KnotTrack, FirstKnotPos, nullptr, OtherPin);
		KnotTrack->KnotCreations.Add(FirstLoopingKnot);

		const FVector2D SecondKnotPos(OtherPinPos.X - 20 - LoopingOffset.X, KnotTrack->GetTrackHeight());
		TSharedPtr<FKnotNodeCreation> SecondLoopingKnot = MakeShared<FKnotNodeCreation>(KnotTrack, SecondKnotPos, FirstLoopingKnot, OtherPin);
		KnotTrack->KnotCreations.Add(SecondLoopingKnot);
	}

	LinkedPins.RemoveAll([&LoopingPins](UEdGraphPin* Pin) { return LoopingPins.Contains(Pin); });

	// remove pins which are left or too close to my pin
	const float Threshold = ParentPinPos.X - NodePadding.X * 1.5f;
	TSharedPtr<FBAGraphHandler> GraphHandlerRef = GraphHandler;
	const auto& IsTooCloseToParent = [GraphHandlerRef, Threshold](UEdGraphPin* Pin)
	{
		const FVector2D PinPos = FBAUtils::GetPinPos(GraphHandlerRef, Pin);
		return PinPos.X > Threshold;
	};
	LinkedPins.RemoveAll(IsTooCloseToParent);

	if (LinkedPins.Num() == 0)
	{
		return nullptr;
	}

	// sort pins by node's highest x position first then highest y position
	const auto RightTop = [](const UEdGraphPin& PinA, const UEdGraphPin& PinB)
	{
		if (PinA.GetOwningNode()->NodePosX == PinB.GetOwningNode()->NodePosX)
		{
			return PinA.GetOwningNode()->NodePosY > PinB.GetOwningNode()->NodePosY;
		}

		return PinA.GetOwningNode()->NodePosX > PinB.GetOwningNode()->NodePosX;
	};

	LinkedPins.Sort(RightTop);

	const FVector2D LastPinPos = FBAUtils::GetPinPos(GraphHandler, LinkedPins.Last());

	const float DistX = FMath::Abs(ParentPinPos.X - LastPinPos.X);
	const float DistY = FMath::Abs(ParentPinPos.Y - LastPinPos.Y);

	// skip if we are the same height as our parent pin and we only have 1 pin
	const bool bHeightDistance = DistY > 5.0f; 

	// skip the pin distance check if we are expanding by height
	const bool bPinReallyFar = DistX >= UBASettings::Get().KnotNodeDistanceThreshold;// && !UBASettings::Get().bExpandNodesByHeight;

	const bool bPinNeedsTrack = DoesPinNeedTrack(ParentPin, LinkedPins);

	const bool bPreviousHasTrack = PreviousTracks.Num() > 0;

	const FVector2D ToLast = LastPinPos - ParentPinPos;
	const bool bTooSteep = FMath::Abs(ToLast.Y) / FMath::Abs(ToLast.X) >= 2.75f;
	if (bTooSteep)
	{
		return nullptr;
	}

	// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Need reroute: %d %d %d %d %f %f"), bSkipSameHeight, bPinReallyFar, bPreviousHasTrack, bPinNeedsTrack, DistX, DistY);

	const bool bNeedsReroute = bPinNeedsTrack || bHeightDistance || bPreviousHasTrack || bPinReallyFar;
	if (!bNeedsReroute)
	{
		return nullptr;
	}

	TSharedPtr<FKnotNodeTrack> KnotTrack = MakeShared<FKnotNodeTrack>(Formatter, GraphHandler, ParentPin, LinkedPins, false);

	TryAlignTrackToEndPins(KnotTrack, Formatter->GetFormattedNodes().Array());

	// remove the first linked pins which has the same height and no collision
	const bool bSameHeightAsParentPin = FMath::Abs(KnotTrack->GetTrackHeight() - ParentPinPos.Y) < 5.f;
	if (!bSameHeightAsParentPin)
	{
		for (UEdGraphPin* LinkedPin : LinkedPins)
		{
			const FVector2D LinkedPinPos = FBAUtils::GetPinPos(GraphHandler, LinkedPin);

			const bool bSameHeight = FMath::Abs(LinkedPinPos.Y - ParentPinPos.Y) < 5.f;
			if (bSameHeight && !AnyCollisionBetweenPins(ParentPin, LinkedPin))
			{
				KnotTrack->LinkedTo.Remove(LinkedPin);
				break;
			}
		}
	}

	if (KnotTrack->LinkedTo.Num() == 0)
	{
		return nullptr;
	}

	KnotTracks.Add(KnotTrack);

	const int& OffsetX = UBASettings::Get().BlueprintExecutionKnotSettings.KnotXOffset;

	// if the track is not at the same height as the pin, then we need an
	// initial knot right of the inital pin, at the track height
	const FVector2D MyKnotPos = FVector2D(ParentPinPos.X - NodePadding.X - OffsetX, KnotTrack->GetTrackHeight());
	TSharedPtr<FKnotNodeCreation> PreviousKnot = MakeShared<FKnotNodeCreation>(KnotTrack, MyKnotPos, nullptr, KnotTrack->GetParentPin());
	KnotTrack->KnotCreations.Add(PreviousKnot);

	// // if one of our pins are straightened to the parent pin, then this pin should be used as the min pos
	// TOptional<float> AtLeastX;
	// for (FBANodePinHandle LinkedTo : KnotTrack->LinkedTo)
	// {
	// 	FPinLink PinLink(KnotTrack->ParentPin.GetPin(), LinkedTo.GetPin());
	// 	if (FBAUtils::ArePinsStraightened(GraphHandler, PinLink))
	// 	{
	// 		AtLeastX = FMath::Min(
	// 			FBAUtils::GetPinPos(GraphHandler, LinkedTo.GetPin()).X + NodePadding.X,
	// 			ParentPinPos.X - NodePadding.X);
	//
	// 		// UE_LOG(LogTemp, Warning, TEXT("Set at least X %f"), *AtLeastX);
	// 		break;
	// 	}
	// }

	// create a knot node for each of the pins remaining in linked to
	for (FBANodePinHandle& OtherPinHandle : KnotTrack->LinkedTo)
	{
		UEdGraphPin* OtherPin = OtherPinHandle.GetPin();

		const FVector2D OtherPinPos = FBAUtils::GetPinPos(GraphHandler, OtherPin);
		float KnotX = FMath::Min(OtherPinPos.X + NodePadding.X + OffsetX, ParentPinPos.X - NodePadding.X - OffsetX);

		// if (AtLeastX.IsSet())
		// {
			// KnotX = FMath::Max(KnotX, AtLeastX.GetValue());
		// }

		const FVector2D KnotPos(KnotX, KnotTrack->GetTrackHeight());

		// if the x position is very close to the previous knot's x position, 
		// we should not need to create a new knot instead we merge the locations
		if (PreviousKnot.IsValid() && FMath::Abs(KnotX - PreviousKnot->KnotPos.X) < 50)
		{
			PreviousKnot->KnotPos.X = KnotX;
			PreviousKnot->PinHandlesToConnectTo.Add(OtherPin);
			continue;
		}

		PreviousKnot = MakeShared<FKnotNodeCreation>(KnotTrack, KnotPos, PreviousKnot, OtherPin);
		KnotTrack->KnotCreations.Add(PreviousKnot);
	}

	PreviousTracks.Add(KnotTrack);

	KnotTrack->SortCreationNodes();

	return KnotTrack;
}

TSharedPtr<FKnotNodeTrack> FKnotTrackCreator::MakeKnotTracksForParameterPins(UEdGraphPin* ParentPin, TArray<UEdGraphPin*> LinkedPins, TArray<TSharedPtr<FKnotNodeTrack>>& PreviousTracks)
{
	// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Make knot tracks for parameter pin %s"), *FBAUtils::GetPinName(ParentPin));

	FVector2D ParentPinPos = FBAUtils::GetPinPos(GraphHandler, ParentPin);

	// check for looping pins, these are pins where 
	// the x position of the pin is less than the x value of the parent pin
	TArray<UEdGraphPin*> LoopingPins;
	for (UEdGraphPin* LinkedPin : LinkedPins)
	{
		const FVector2D LinkedPinPos = FBAUtils::GetPinPos(GraphHandler, LinkedPin);
		if (LinkedPinPos.X < ParentPinPos.X && FMath::Abs(LinkedPinPos.X - ParentPinPos.X) > 200)
		{
			if (FBAUtils::IsNodeImpure(ParentPin->GetOwningNode()))
			{
				// auto Bounds = FBAUtils::GetPinBounds(GraphHandler->GetGraphPanel(), LinkedPin);
				// GraphHandler->GetGraphOverlay()->DrawBounds(Bounds);

				// UE_LOG(LogTemp, Warning, TEXT("%s is looping"), *);
				LoopingPins.Add(LinkedPin);
			}
		}
	}

	const FIntPoint& LoopingOffset = UBASettings::Get().BlueprintParameterKnotSettings.LoopingOffset;

	// create looping tracks
	for (UEdGraphPin* OtherPin : LoopingPins)
	{
		TArray<UEdGraphPin*> TrackPins = { OtherPin };
		TSharedPtr<FKnotNodeTrack> KnotTrack = MakeShared<FKnotNodeTrack>(Formatter, GraphHandler, ParentPin, TrackPins, true);
		KnotTracks.Add(KnotTrack);

		const FVector2D OtherPinPos = FBAUtils::GetPinPos(GraphHandler, OtherPin);

		const FVector2D FirstKnotPos(ParentPinPos.X - 20 - LoopingOffset.X, KnotTrack->GetTrackHeight());
		TSharedPtr<FKnotNodeCreation> FirstLoopingKnot = MakeShared<FKnotNodeCreation>(KnotTrack, FirstKnotPos, nullptr, OtherPin);
		KnotTrack->KnotCreations.Add(FirstLoopingKnot);

		const FVector2D SecondKnotPos(OtherPinPos.X + 20 + LoopingOffset.X, KnotTrack->GetTrackHeight());
		TSharedPtr<FKnotNodeCreation> SecondLoopingKnot = MakeShared<FKnotNodeCreation>(KnotTrack, SecondKnotPos, FirstLoopingKnot, OtherPin);
		KnotTrack->KnotCreations.Add(SecondLoopingKnot);
	}

	LinkedPins.RemoveAll([&LoopingPins](UEdGraphPin* Pin) { return LoopingPins.Contains(Pin); });

	// remove pins which are left or too close to my pin
	const float Threshold = ParentPinPos.X + NodePadding.X * 2.0f;
	TSharedPtr<FBAGraphHandler> GraphHandlerRef = GraphHandler;

	const auto& IsTooCloseToParent = [GraphHandlerRef, Threshold](UEdGraphPin* Pin)
	{
		const FVector2D PinPos = FBAUtils::GetPinPos(GraphHandlerRef, Pin);
		return PinPos.X < Threshold;
	};

	LinkedPins.RemoveAll(IsTooCloseToParent);

	if (LinkedPins.Num() == 0)
	{
		return nullptr;
	}

	const auto LeftTop = [](const UEdGraphPin& PinA, const UEdGraphPin& PinB)
	{
		if (PinA.GetOwningNode()->NodePosX == PinB.GetOwningNode()->NodePosX)
		{
			return PinA.GetOwningNode()->NodePosY > PinB.GetOwningNode()->NodePosY;
		}

		return PinA.GetOwningNode()->NodePosX < PinB.GetOwningNode()->NodePosX;
	};
	LinkedPins.Sort(LeftTop);

	const FVector2D LastPinPos = FBAUtils::GetPinPos(GraphHandler, LinkedPins.Last());

	const float DistX = FMath::Abs(ParentPinPos.X - LastPinPos.X);
	const bool bPinReallyFar = DistX >= UBASettings::Get().KnotNodeDistanceThreshold;// && !UBASettings::Get().bExpandNodesByHeight;

	const float DistY = FMath::Abs(ParentPinPos.Y - LastPinPos.Y);

	const bool bHeightDifference = DistY > 5.0f;

	const bool bPinNeedsTrack = DoesPinNeedTrack(ParentPin, LinkedPins);

	const bool bPreviousHasTrack = PreviousTracks.Num() > 0;

	// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Needs track: %d %d %d"), bLastPinFarAway, bPreviousHasTrack, bPinNeedsTrack);

	const FVector2D ToLast = LastPinPos - ParentPinPos;
	const bool bTooSteep = FMath::Abs(ToLast.Y) / FMath::Abs(ToLast.X) >= 2.75f;
	if (bTooSteep)
	{
		return nullptr;
	}

	const bool bNeedsReroute = bPinNeedsTrack || bHeightDifference || bPreviousHasTrack || bPinReallyFar;
	if (!bNeedsReroute)
	{
		return nullptr;
	}

	// init the knot track
	TSharedPtr<FKnotNodeTrack> KnotTrack = MakeShared<FKnotNodeTrack>(Formatter, GraphHandler, ParentPin, LinkedPins, false);

	// check if the track height can simply be set to one of it's pin's height
	if (TryAlignTrackToEndPins(KnotTrack, Formatter->GetFormattedNodes().Array()))
	{
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Found a pin to align to for %s"), *FBAUtils::GetPinName(KnotTrack->GetParentPin()));
	}
	else
	{
		// UE_LOG(LogKnotTrackCreator, Warning, TEXT("Failed to find pin to align to"));
	}

	// remove any pins which has the same height and no collision
	for (UEdGraphPin* LinkedPin : LinkedPins)
	{
		const FVector2D LinkedPinPos = FBAUtils::GetPinPos(GraphHandler, LinkedPin);

		const bool bSameHeight = FMath::Abs(LinkedPinPos.Y - ParentPinPos.Y) < 5.f;
		if (bSameHeight && !AnyCollisionBetweenPins(ParentPin, LinkedPin))
		{
			KnotTrack->LinkedTo.Remove(LinkedPin);
		}
	}

	if (KnotTrack->LinkedTo.Num() == 0)
	{
		return nullptr;
	}

	const int& OffsetX = UBASettings::Get().BlueprintParameterKnotSettings.KnotXOffset;

	// Add a knot creation which links to the parent pin
	const FVector2D InitialKnotPos = FVector2D(ParentPinPos.X + PinPadding.X + OffsetX, KnotTrack->GetTrackHeight());
	TSharedPtr<FKnotNodeCreation> PreviousKnot = MakeShared<FKnotNodeCreation>(KnotTrack, InitialKnotPos, nullptr, KnotTrack->GetParentPin());
	KnotTrack->KnotCreations.Add(PreviousKnot);

	for (FBANodePinHandle& OtherPinHandle : KnotTrack->LinkedTo)
	{
		UEdGraphPin* OtherPin = OtherPinHandle.GetPin();

		const FVector2D OtherPinPos = FBAUtils::GetPinPos(GraphHandler, OtherPin);
		const float KnotX = FMath::Max(OtherPinPos.X - PinPadding.X - OffsetX, ParentPinPos.X + PinPadding.X + OffsetX);

		const FVector2D KnotPos = FVector2D(KnotX, KnotTrack->GetTrackHeight());

		// if the x position is very close to the previous knot's x position, 
		// we should not need to create a new knot instead we merge the locations
		if (PreviousKnot.IsValid() && FMath::Abs(KnotX - PreviousKnot->KnotPos.X) < 50)
		{
			PreviousKnot->KnotPos.X = KnotX;
			PreviousKnot->PinHandlesToConnectTo.Add(OtherPin);
			continue;
		}

		// Add a knot creation for each linked pin
		PreviousKnot = MakeShared<FKnotNodeCreation>(KnotTrack, KnotPos, PreviousKnot, OtherPin);
		KnotTrack->KnotCreations.Add(PreviousKnot);
	}

	PreviousTracks.Add(KnotTrack);

	KnotTracks.Add(KnotTrack);

	KnotTrack->SortCreationNodes();

	return KnotTrack;
}

void FKnotTrackCreator::MergeNearbyKnotTracks()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FKnotTrackCreator::MergeNearbyKnotTracks"), STAT_KnotTrackCreator_MergeNearbyKnotTracks, STATGROUP_BA_EdGraphFormatter);

	if (UBASettings::Get().ExecutionWiringStyle != EBAWiringStyle::MergeWhenNear && UBASettings::Get().ParameterWiringStyle != EBAWiringStyle::MergeWhenNear)
	{
		return;
	}

	TArray<TSharedPtr<FKnotNodeTrack>> PendingTracks = KnotTracks;

	if (UBASettings::Get().ExecutionWiringStyle != EBAWiringStyle::MergeWhenNear)
	{
		PendingTracks.RemoveAll([](TSharedPtr<FKnotNodeTrack> Track)
		{
			return FBAUtils::IsExecOrDelegatePin(Track->GetParentPin());
		});
	}

	if (UBASettings::Get().ParameterWiringStyle != EBAWiringStyle::MergeWhenNear)
	{
		PendingTracks.RemoveAll([](TSharedPtr<FKnotNodeTrack> Track)
		{
			return FBAUtils::IsParameterPin(Track->GetParentPin());
		});
	}

	PendingTracks.RemoveAll([](TSharedPtr<FKnotNodeTrack> Track)
	{
		return Track->bIsLoopingTrack;
	});

	TMap<UEdGraphPin*, TArray<TSharedPtr<FKnotNodeTrack>>> KnotTracksByParent;

	for (TSharedPtr<FKnotNodeTrack> KnotNodeTrack : KnotTracks)
	{
		KnotTracksByParent.FindOrAdd(KnotNodeTrack->GetParentPin()).Add(KnotNodeTrack);
	}

	for (auto& Elem : KnotTracksByParent)
	{
		if (Elem.Value.Num() <= 1)
		{
			continue;
		}

		// UE_LOG(LogKnotTrackCreator, VeryVerbose, TEXT("Checking merge %s"), *FBAUtils::GetPinName(Elem.Key));
		TArray<TSharedPtr<FKnotNodeTrack>> Tracks = Elem.Value;
		Tracks.StableSort([](const TSharedPtr<FKnotNodeTrack>& A, const TSharedPtr<FKnotNodeTrack>& B)
		{
			return A->GetTrackHeight() < B->GetTrackHeight();
		});

		TOptional<TSharedPtr<FKnotNodeTrack>> OptionalLastTrack;
		TOptional<float> LastTrackHeight; 
		for (auto& Track : Tracks)
		{
			if (LastTrackHeight.IsSet() && OptionalLastTrack.IsSet())
			{
				TSharedPtr<FKnotNodeTrack> LastTrack = OptionalLastTrack.GetValue();
				float Dist = FMath::Abs(LastTrackHeight.GetValue() - Track->GetTrackHeight());
				if (Dist <= UBASettings::Get().BlueprintKnotTrackSpacing)
				{
#if 0
					UE_LOG(LogKnotTrackCreator, VeryVerbose, TEXT("Merging tracks"));
					UE_LOG(LogKnotTrackCreator, Warning, TEXT("Source track"));
					for (TSharedPtr<FKnotNodeCreation> KnotNodeCreation : LastTrack->KnotCreations)
					{
						UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tCreation"));
						for (FBAGraphPinHandle PinHandlesToConnectTo : KnotNodeCreation->PinHandlesToConnectTo)
						{
							UE_LOG(LogTemp, Warning, TEXT("\t\t%s"), *PinHandlesToConnectTo.ToString());
						}
					}

					UE_LOG(LogKnotTrackCreator, Warning, TEXT("Merge Track"));
					for (TSharedPtr<FKnotNodeCreation> KnotNodeCreation : Track->KnotCreations)
					{
						UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tCreation"));
						for (FBAGraphPinHandle PinHandlesToConnectTo : KnotNodeCreation->PinHandlesToConnectTo)
						{
							UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\t%s"), *PinHandlesToConnectTo.ToString());
						}
					}
#endif
					// merge knot creations
					for (TSharedPtr<FKnotNodeCreation> NewCreation : Track->KnotCreations)
					{
						// skip duplicates by checking if the creation's pin handles are equal 
						bool bSkip = false;
						for (TSharedPtr<FKnotNodeCreation> Original : LastTrack->KnotCreations)
						{
							if (Original->PinHandlesToConnectTo.Difference(NewCreation->PinHandlesToConnectTo).Num() == 0)
							{
								bSkip = true;
							}
						}

						if (!bSkip)
						{
							LastTrack->KnotCreations.Add(NewCreation);
						}
					}

					LastTrack->SortCreationNodes();
					LastTrack->LinkedTo.Append(Track->LinkedTo);
					KnotTracks.Remove(Track);

					// don't update the last track if we merged
					continue;
				}
			}

			OptionalLastTrack = Track;
			LastTrackHeight = Track->GetTrackHeight();
		}
	}
}

void FKnotTrackCreator::AddKnotNodesToComments()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FKnotTrackCreator::AddKnotNodesToComments"), STAT_KnotTrackCreator_AddKnotNodesToComments, STATGROUP_BA_EdGraphFormatter);

	FCommentHandler* CommentHandler = Formatter->GetCommentHandler();
	if (!CommentHandler)
	{
		return;
	}

	if (CommentHandler->GetComments().Num() == 0)
	{
		return;
	}

	for (TSharedPtr<FKnotNodeTrack> Track : KnotTracks)
	{
		TArray<UEdGraphNode*> TrackNodes = Track->GetNodes(GraphHandler->GetFocusedEdGraph()).Array();

		// process these later in AddNomadKnotsIntoComments
		if (Track->KnotCreations.Num() <= 1)
		{
			continue;
		}

		for (const auto& Comment : CommentHandler->GetComments())
		{
			TArray<UEdGraphNode*> Containing = CommentHandler->GetNodesUnderComments(Comment);

			// Add knots if all the track nodes are contained in the comment
			if (FBAUtils::DoesArrayContainsAllItems(Containing, TrackNodes))
			{
				FCommentNodeSet NodesUnderComment = Comment->GetNodesUnderComment();
				for (TSharedPtr<FKnotNodeCreation> Creation : Track->KnotCreations)
				{
					if (!NodesUnderComment.Contains(Creation->CreatedKnot))
					{
						// if we should ignore this comment then don't track the comment in the comment handler
						if (CommentHandler->ShouldIgnoreComment(Comment))
						{
							Comment->AddNodeUnderComment(Creation->CreatedKnot);
						}
						else
						{
							CommentHandler->AddNodeIntoComment(Comment, Creation->CreatedKnot);
						}

						KnotsInComments.Add(Creation->CreatedKnot);
					}
				}
			}
		}
	}
}

void FKnotTrackCreator::PrintKnotTracks()
{
	UE_LOG(LogKnotTrackCreator, Warning, TEXT("### All Knot Tracks"));
	for (TSharedPtr<FKnotNodeTrack> Track : KnotTracks)
	{
		FString Aligned = Track->GetPinToAlignTo() != nullptr ? FString("True") : FString("False");
		FString Looping = Track->bIsLoopingTrack ? FString("True") : FString("False");
		UE_LOG(LogKnotTrackCreator, Warning, TEXT("\tKnot Tracks (%d) %s | %s | Aligned %s (%s) | Looping %s"),
			Track->KnotCreations.Num(),
			*FBAUtils::GetPinName(Track->GetParentPin(), true),
			*FBAUtils::GetPinName(Track->GetLastPin(), true),
			*Aligned, *FBAUtils::GetPinName(Track->GetPinToAlignTo(), true),
			*Looping);

		for (TSharedPtr<FKnotNodeCreation> Elem : Track->KnotCreations)
		{
			if (auto MyPin = Elem->PinToConnectToHandle.GetPin())
			{
				UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\t%s %s"), *FBAUtils::GetPinName(MyPin, true), *Elem->KnotPos.ToString());
			}

			for (auto PinHandle : Elem->PinHandlesToConnectTo)
			{
				if (auto MyPin = PinHandle.GetPin())
				{
					UE_LOG(LogKnotTrackCreator, Warning, TEXT("\t\t\t%s"), *FBAUtils::GetPinName(MyPin, true));
				}
			}
		}
	}
}

bool FKnotTrackCreator::IsPinAlignedKnot(const UK2Node_Knot* KnotNode)
{
	return PinAlignedKnots.Contains(KnotNode);
}

TSharedPtr<FGroupedTracks> FKnotTrackCreator::GetKnotGroup(const UK2Node_Knot* KnotNode)
{
	return KnotTrackGroup.FindRef(KnotNode);
}
