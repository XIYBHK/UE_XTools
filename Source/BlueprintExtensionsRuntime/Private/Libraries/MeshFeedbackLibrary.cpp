#include "Libraries/MeshFeedbackLibrary.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	bool HasMeshTypeFlag(int32 MeshTypes, EXToolsMeshFeedbackComponentType MeshType)
	{
		return (MeshTypes & static_cast<int32>(MeshType)) != 0;
	}

	bool IsGeometryCollectionMesh(const UMeshComponent* MeshComponent)
	{
		return MeshComponent && MeshComponent->IsA<UGeometryCollectionComponent>();
	}

	bool IsStaticMesh(const UMeshComponent* MeshComponent)
	{
		return MeshComponent && MeshComponent->IsA<UStaticMeshComponent>();
	}

	bool IsSkeletalMesh(const UMeshComponent* MeshComponent)
	{
		return MeshComponent && MeshComponent->IsA<USkeletalMeshComponent>();
	}

	bool MatchesMeshType(const UMeshComponent* MeshComponent, int32 MeshTypes)
	{
		if (!MeshComponent || MeshTypes == static_cast<int32>(EXToolsMeshFeedbackComponentType::None))
		{
			return false;
		}

		if (IsStaticMesh(MeshComponent))
		{
			return HasMeshTypeFlag(MeshTypes, EXToolsMeshFeedbackComponentType::StaticMesh);
		}

		if (IsSkeletalMesh(MeshComponent))
		{
			return HasMeshTypeFlag(MeshTypes, EXToolsMeshFeedbackComponentType::SkeletalMesh);
		}

		if (IsGeometryCollectionMesh(MeshComponent))
		{
			return HasMeshTypeFlag(MeshTypes, EXToolsMeshFeedbackComponentType::GeometryCollection);
		}

		return HasMeshTypeFlag(MeshTypes, EXToolsMeshFeedbackComponentType::OtherMesh);
	}

	bool MatchesCollectOptions(const UMeshComponent* MeshComponent, const FXToolsMeshFeedbackCollectOptions& Options)
	{
		if (!IsValid(MeshComponent))
		{
			return false;
		}

		if (!Options.bIncludeUnregisteredComponents && !MeshComponent->IsRegistered())
		{
			return false;
		}

		if (!Options.bIncludeHiddenComponents && !MeshComponent->IsVisible())
		{
			return false;
		}

		if (!Options.RequiredComponentTag.IsNone() && !MeshComponent->ComponentHasTag(Options.RequiredComponentTag))
		{
			return false;
		}

		return MatchesMeshType(MeshComponent, Options.MeshTypes);
	}

	void AddMIDEntry(
		UMeshComponent* MeshComponent,
		int32 MaterialIndex,
		UMaterialInterface* OriginalMaterial,
		UMaterialInstanceDynamic* MID,
		bool bWasAlreadyMID,
		bool bCreatedMID,
		TArray<UMaterialInstanceDynamic*>& OutMIDs,
		TArray<FXToolsMeshFeedbackMIDEntry>& OutEntries)
	{
		if (!MID)
		{
			return;
		}

		FXToolsMeshFeedbackMIDEntry Entry;
		Entry.MeshComponent = MeshComponent;
		Entry.MaterialIndex = MaterialIndex;
		Entry.OriginalMaterial = OriginalMaterial;
		Entry.MID = MID;
		Entry.bWasAlreadyMID = bWasAlreadyMID;
		Entry.bCreatedMID = bCreatedMID;

		OutMIDs.Add(MID);
		OutEntries.Add(Entry);
	}
}

void UMeshFeedbackLibrary::CollectActorMeshComponents(
	AActor* TargetActor,
	const FXToolsMeshFeedbackCollectOptions& Options,
	TArray<UMeshComponent*>& OutMeshComponents)
{
	OutMeshComponents.Reset();

	if (!IsValid(TargetActor))
	{
		return;
	}

	TInlineComponentArray<UMeshComponent*> MeshComponents(TargetActor, Options.bIncludeFromChildActors);
	TSet<UMeshComponent*> VisitedComponents;
	VisitedComponents.Reserve(MeshComponents.Num());
	OutMeshComponents.Reserve(MeshComponents.Num());

	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (VisitedComponents.Contains(MeshComponent))
		{
			continue;
		}

		VisitedComponents.Add(MeshComponent);

		if (MatchesCollectOptions(MeshComponent, Options))
		{
			OutMeshComponents.Add(MeshComponent);
		}
	}
}

void UMeshFeedbackLibrary::CreateDynamicMaterialInstancesForMeshes(
	const TArray<UMeshComponent*>& MeshComponents,
	const FXToolsMeshFeedbackMIDOptions& Options,
	TArray<UMaterialInstanceDynamic*>& OutMIDs,
	TArray<FXToolsMeshFeedbackMIDEntry>& OutEntries)
{
	if (Options.bClearOutputs)
	{
		OutMIDs.Reset();
		OutEntries.Reset();
	}

	TSet<UMeshComponent*> VisitedComponents;
	VisitedComponents.Reserve(MeshComponents.Num());

	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent) || VisitedComponents.Contains(MeshComponent))
		{
			continue;
		}

		VisitedComponents.Add(MeshComponent);

		const int32 NumMaterials = MeshComponent->GetNumMaterials();
		if (NumMaterials <= 0)
		{
			continue;
		}

		OutMIDs.Reserve(OutMIDs.Num() + NumMaterials);
		OutEntries.Reserve(OutEntries.Num() + NumMaterials);

		for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
		{
			UMaterialInterface* OriginalMaterial = MeshComponent->GetMaterial(MaterialIndex);
			if (!OriginalMaterial && Options.bSkipNullMaterials)
			{
				continue;
			}

			UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(OriginalMaterial);
			const bool bWasAlreadyMID = ExistingMID != nullptr;
			UMaterialInstanceDynamic* MID = nullptr;
			bool bCreatedMID = false;

			if (bWasAlreadyMID && Options.bReuseExistingMID)
			{
				MID = ExistingMID;
			}
			else if (bWasAlreadyMID && !Options.bReuseExistingMID)
			{
				MID = UMaterialInstanceDynamic::Create(OriginalMaterial, MeshComponent, Options.OptionalMIDName);
				if (MID)
				{
					MeshComponent->SetMaterial(MaterialIndex, MID);
					bCreatedMID = true;
				}
			}
			else
			{
				MID = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, OriginalMaterial, Options.OptionalMIDName);
				bCreatedMID = MID != nullptr;
			}

			AddMIDEntry(
				MeshComponent,
				MaterialIndex,
				OriginalMaterial,
				MID,
				bWasAlreadyMID,
				bCreatedMID,
				OutMIDs,
				OutEntries);
		}
	}
}

void UMeshFeedbackLibrary::CreateDynamicMaterialInstancesForActorMeshes(
	AActor* TargetActor,
	const FXToolsMeshFeedbackCollectOptions& CollectOptions,
	const FXToolsMeshFeedbackMIDOptions& MIDOptions,
	TArray<UMeshComponent*>& OutMeshComponents,
	TArray<UMaterialInstanceDynamic*>& OutMIDs,
	TArray<FXToolsMeshFeedbackMIDEntry>& OutEntries)
{
	CollectActorMeshComponents(TargetActor, CollectOptions, OutMeshComponents);
	CreateDynamicMaterialInstancesForMeshes(OutMeshComponents, MIDOptions, OutMIDs, OutEntries);
}
