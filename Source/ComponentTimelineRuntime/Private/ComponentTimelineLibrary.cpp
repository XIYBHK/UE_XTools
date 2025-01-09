// Copyright 2023 Tomasz Klin. All Rights Reserved.


#include "ComponentTimelineLibrary.h"
#include "Engine/TimelineTemplate.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/TimelineComponent.h"
#include "Engine/World.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"

namespace
{
	void BindTimeline(UTimelineTemplate* TimelineTemplate, UObject* BlueprintOwner, AActor* ActorOwner)
	{
		if (!IsValid(BlueprintOwner)
			|| !IsValid(ActorOwner)
			|| !TimelineTemplate
			|| BlueprintOwner->IsTemplate())
		{
			return;
		}

		FName NewName = TimelineTemplate->GetVariableName();

		// Find property with the same name as the template and assign the new Timeline to it
		UClass* ComponentClass = BlueprintOwner->GetClass();
		FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(ComponentClass, TimelineTemplate->GetVariableName());
		if (Prop)
		{
			// Check if TimelineComponent is already created. This Prevent agains call InitializeTimeLineComponent multiple times.
			UObject* CurrentValue = Prop->GetObjectPropertyValue_InContainer(BlueprintOwner);
			if (CurrentValue) 
				return;
		}

		FName Name = MakeUniqueObjectName(ActorOwner,  UTimelineComponent::StaticClass(), *FString::Printf(TEXT("%s_%s"), *BlueprintOwner->GetName(), *TimelineTemplate->GetVariableName().ToString()));
		UTimelineComponent* NewTimeline = NewObject<UTimelineComponent>(ActorOwner, Name);
		NewTimeline->CreationMethod = EComponentCreationMethod::UserConstructionScript; // Indicate it comes from a blueprint so it gets cleared when we rerun construction scripts
		ActorOwner->BlueprintCreatedComponents.Add(NewTimeline); // Add to array so it gets saved
		NewTimeline->SetNetAddressable();	// This component has a stable name that can be referenced for replication

		NewTimeline->SetPropertySetObject(BlueprintOwner); // Set which object the timeline should drive properties on
		NewTimeline->SetDirectionPropertyName(TimelineTemplate->GetDirectionPropertyName());

		NewTimeline->SetTimelineLength(TimelineTemplate->TimelineLength); // copy length
		NewTimeline->SetTimelineLengthMode(TimelineTemplate->LengthMode);

		NewTimeline->PrimaryComponentTick.TickGroup = TimelineTemplate->TimelineTickGroup;
		if (Prop)
		{
			Prop->SetObjectPropertyValue_InContainer(BlueprintOwner, NewTimeline);
		}
		// Event tracks
		// In the template there is a track for each function, but in the runtime Timeline each key has its own delegate, so we fold them together
		for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->EventTracks.Num(); TrackIdx++)
		{
			const FTTEventTrack* EventTrackTemplate = &TimelineTemplate->EventTracks[TrackIdx];
			if (EventTrackTemplate->CurveKeys != nullptr)
			{
				// Create delegate for all keys in this track
				FScriptDelegate EventDelegate;
				EventDelegate.BindUFunction(BlueprintOwner, EventTrackTemplate->GetFunctionName());

				// Create an entry in Events for each key of this track
				for (auto It(EventTrackTemplate->CurveKeys->FloatCurve.GetKeyIterator()); It; ++It)
				{
					NewTimeline->AddEvent(It->Time, FOnTimelineEvent(EventDelegate));
				}
			}
		}

		// Float tracks
		for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->FloatTracks.Num(); TrackIdx++)
		{
			const FTTFloatTrack* FloatTrackTemplate = &TimelineTemplate->FloatTracks[TrackIdx];
			if (FloatTrackTemplate->CurveFloat != NULL)
			{
				NewTimeline->AddInterpFloat(FloatTrackTemplate->CurveFloat, FOnTimelineFloat(), FloatTrackTemplate->GetPropertyName(), FloatTrackTemplate->GetTrackName());
			}
		}

		// Vector tracks
		for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->VectorTracks.Num(); TrackIdx++)
		{
			const FTTVectorTrack* VectorTrackTemplate = &TimelineTemplate->VectorTracks[TrackIdx];
			if (VectorTrackTemplate->CurveVector != NULL)
			{
				NewTimeline->AddInterpVector(VectorTrackTemplate->CurveVector, FOnTimelineVector(), VectorTrackTemplate->GetPropertyName(), VectorTrackTemplate->GetTrackName());
			}
		}

		// Linear color tracks
		for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->LinearColorTracks.Num(); TrackIdx++)
		{
			const FTTLinearColorTrack* LinearColorTrackTemplate = &TimelineTemplate->LinearColorTracks[TrackIdx];
			if (LinearColorTrackTemplate->CurveLinearColor != NULL)
			{
				NewTimeline->AddInterpLinearColor(LinearColorTrackTemplate->CurveLinearColor, FOnTimelineLinearColor(), LinearColorTrackTemplate->GetPropertyName(), LinearColorTrackTemplate->GetTrackName());
			}
		}

		// Set up delegate that gets called after all properties are updated
		FScriptDelegate UpdateDelegate;
		UpdateDelegate.BindUFunction(BlueprintOwner, TimelineTemplate->GetUpdateFunctionName());
		NewTimeline->SetTimelinePostUpdateFunc(FOnTimelineEvent(UpdateDelegate));

		// Set up finished delegate that gets called after all properties are updated
		FScriptDelegate FinishedDelegate;
		FinishedDelegate.BindUFunction(BlueprintOwner, TimelineTemplate->GetFinishedFunctionName());
		NewTimeline->SetTimelineFinishedFunc(FOnTimelineEvent(FinishedDelegate));

		NewTimeline->RegisterComponent();

			// Start playing now, if desired
		if (TimelineTemplate->bAutoPlay)
		{
			// Needed for autoplay timelines in cooked builds, since they won't have Activate() called via the Play call below
			NewTimeline->bAutoActivate = true;
			NewTimeline->Play();
		}

		// Set to loop, if desired
		if (TimelineTemplate->bLoop)
		{
			NewTimeline->SetLooping(true);
		}

		// Set replication, if desired
		if (TimelineTemplate->bReplicated)
		{
			NewTimeline->SetIsReplicated(true);
		}

		// Set replication, if desired
		if (TimelineTemplate->bIgnoreTimeDilation)
		{
			NewTimeline->SetIgnoreTimeDilation(true);
		}
	}
}


void UComponentTimelineLibrary::InitializeComponentTimelines(UActorComponent* Component)
{
	if (!IsValid(Component))
	{
		UE_LOG(LogTemp, Error, TEXT("[InitializeComponentTimelines] Invalid component object"));
		return;
	}

	InitializeTimelines(Component, Component->GetOwner());
}

void UComponentTimelineLibrary::InitializeTimelines(UObject* BlueprintOwner, AActor* ActorOwner)
{
	if (!IsValid(BlueprintOwner) || !IsValid(ActorOwner))
	{
		UE_LOG(LogTemp, Error, TEXT("[UComponentTimelineLibrary] Invalid objects"));
		return;
	}

	// Generate the parent blueprint hierarchy for this actor, so we can run all the construction scripts sequentially
	TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
	const bool bErrorFree = UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(BlueprintOwner->GetClass(), ParentBPClassStack);

	// If this actor has a blueprint lineage, go ahead and run the construction scripts from least derived to most
	if ((ParentBPClassStack.Num() > 0))
	{
		if (bErrorFree)
		{

			// Prevent user from spawning actors in User Construction Script
			FGuardValue_Bitfield(BlueprintOwner->GetWorld()->bIsRunningConstructionScript, true);
			for (int32 i = ParentBPClassStack.Num() - 1; i >= 0; i--)
			{
				const UBlueprintGeneratedClass* CurrentBPGClass = ParentBPClassStack[i];
				check(CurrentBPGClass);

				if (const UBlueprintGeneratedClass* BPGC = Cast<const UBlueprintGeneratedClass>(CurrentBPGClass))
				{
					for (UTimelineTemplate* TimelineTemplate : BPGC->Timelines)
					{
						// Not fatal if NULL, but shouldn't happen and ignored if not wired up in graph
						if (TimelineTemplate)
						{
							BindTimeline(TimelineTemplate, BlueprintOwner, ActorOwner);
						}
					}
				}
			}
		}
	}
}

