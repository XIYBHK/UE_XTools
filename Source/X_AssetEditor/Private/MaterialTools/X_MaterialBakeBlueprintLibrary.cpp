/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "MaterialTools/X_MaterialBakeBlueprintLibrary.h"

#include "Engine/StaticMesh.h"
#include "IMaterialBakingModule.h"
#include "MaterialBakingStructures.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshAttributes.h"
#include "X_AssetEditor.h"

namespace
{
	FColor SampleBakedColor(const TArray<FColor>& Pixels, const FIntPoint& Size, const FVector2f& UV)
	{
		if (Pixels.Num() == 1 || Size.X <= 1 || Size.Y <= 1)
		{
			return Pixels.Num() > 0 ? Pixels[0] : FColor::White;
		}

		float U = FMath::Frac(UV.X);
		float V = FMath::Frac(UV.Y);
		if (U < 0.0f)
		{
			U += 1.0f;
		}
		if (V < 0.0f)
		{
			V += 1.0f;
		}

		const int32 X = FMath::Clamp(FMath::RoundToInt(U * static_cast<float>(Size.X - 1)), 0, Size.X - 1);
		const int32 Y = FMath::Clamp(FMath::RoundToInt(V * static_cast<float>(Size.Y - 1)), 0, Size.Y - 1);
		const int32 PixelIndex = Y * Size.X + X;
		return Pixels.IsValidIndex(PixelIndex) ? Pixels[PixelIndex] : FColor::White;
	}

	int32 FindMaterialIndexForPolygonGroup(
		const UStaticMesh* StaticMesh,
		const TPolygonGroupAttributesConstRef<FName>& MaterialSlotNames,
		const FPolygonGroupID PolygonGroupID)
	{
		if (!StaticMesh || PolygonGroupID.GetValue() == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		const FName SlotName = MaterialSlotNames[PolygonGroupID];
		const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < StaticMaterials.Num(); ++MaterialIndex)
		{
			if (StaticMaterials[MaterialIndex].MaterialSlotName == SlotName ||
				StaticMaterials[MaterialIndex].ImportedMaterialSlotName == SlotName)
			{
				return MaterialIndex;
			}
		}

		return PolygonGroupID.GetValue() < StaticMaterials.Num() ? PolygonGroupID.GetValue() : INDEX_NONE;
	}
}

FX_MaterialBakeVertexColorResult UX_MaterialBakeBlueprintLibrary::BakeStaticMeshBaseColorToVertexColors(
	UStaticMesh* StaticMesh,
	int32 LODIndex,
	int32 TextureSize,
	int32 UVChannel)
{
	FX_MaterialBakeVertexColorResult Result;
	if (!StaticMesh)
	{
		Result.Message = TEXT("静态网格体为空");
		return Result;
	}

	const int32 NumLODs = StaticMesh->GetNumSourceModels();
	if (LODIndex < 0 || LODIndex >= NumLODs)
	{
		Result.Message = FString::Printf(TEXT("LOD级别无效：%d，有效范围为0-%d"), LODIndex, FMath::Max(0, NumLODs - 1));
		return Result;
	}

	FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);
	if (!MeshDescription)
	{
		Result.Message = FString::Printf(TEXT("LOD%d没有可编辑MeshDescription源数据"), LODIndex);
		return Result;
	}

	TextureSize = FMath::Clamp(TextureSize, 1, 4096);
	UVChannel = FMath::Max(0, UVChannel);

	FStaticMeshAttributes Attributes(*MeshDescription);
	Attributes.Register(true);
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesConstRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesConstRef<FName> PolygonGroupMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	if (VertexInstanceUVs.GetNumChannels() <= UVChannel)
	{
		Result.Message = FString::Printf(TEXT("LOD%d没有UV%d，无法按材质烘焙写入顶点色"), LODIndex, UVChannel);
		return Result;
	}

	struct FBakedSlot
	{
		TArray<FColor> Pixels;
		FIntPoint Size = FIntPoint::ZeroValue;
		bool bValid = false;
	};

	TArray<FBakedSlot> BakedSlots;
	const int32 MaterialCount = StaticMesh->GetStaticMaterials().Num();
	BakedSlots.SetNum(MaterialCount);
	IMaterialBakingModule& MaterialBakingModule = FModuleManager::LoadModuleChecked<IMaterialBakingModule>(TEXT("MaterialBaking"));

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		UMaterialInterface* Material = StaticMesh->GetMaterial(MaterialIndex);
		if (!Material)
		{
			continue;
		}

		FMaterialData MaterialSettings;
		MaterialSettings.Material = Material;
		MaterialSettings.PropertySizes.Add(MP_BaseColor, FIntPoint(TextureSize, TextureSize));
		MaterialSettings.bPerformBorderSmear = true;
		MaterialSettings.bPerformShrinking = true;
		MaterialSettings.BlendMode = BLEND_Opaque;

		FMeshData MeshSettings;
		MeshSettings.MeshDescription = MeshDescription;
		MeshSettings.Mesh = StaticMesh;
		MeshSettings.MaterialIndices.Add(MaterialIndex);
		MeshSettings.TextureCoordinateIndex = UVChannel;
		MeshSettings.TextureCoordinateBox = FBox2D(FVector2D(0.0, 0.0), FVector2D(1.0, 1.0));

		TArray<FMaterialData*> MaterialSettingPtrs;
		MaterialSettingPtrs.Add(&MaterialSettings);
		TArray<FMeshData*> MeshSettingPtrs;
		MeshSettingPtrs.Add(&MeshSettings);

		FBakeOutput BakeOutput;
		MaterialBakingModule.BakeMaterials(MaterialSettingPtrs, MeshSettingPtrs, BakeOutput);

		TArray<FColor>* PropertyData = BakeOutput.PropertyData.Find(MP_BaseColor);
		FIntPoint* PropertySize = BakeOutput.PropertySizes.Find(MP_BaseColor);
		if (!PropertyData || !PropertySize || PropertyData->Num() == 0 || PropertySize->X <= 0 || PropertySize->Y <= 0)
		{
			UE_LOG(LogX_AssetEditor, Warning,
				TEXT("[材质烘焙] 材质槽%d(%s)未能烘焙BaseColor"),
				MaterialIndex,
				*Material->GetName());
			continue;
		}

		FBakedSlot& BakedSlot = BakedSlots[MaterialIndex];
		BakedSlot.Pixels = MoveTemp(*PropertyData);
		BakedSlot.Size = *PropertySize;
		BakedSlot.bValid = true;
		++Result.BakedMaterialSlotCount;
	}

	if (Result.BakedMaterialSlotCount == 0)
	{
		Result.Message = TEXT("没有任何材质槽成功烘焙BaseColor");
		return Result;
	}

	StaticMesh->Modify();
	for (const FPolygonID PolygonID : MeshDescription->Polygons().GetElementIDs())
	{
		const FPolygonGroupID PolygonGroupID = MeshDescription->GetPolygonPolygonGroup(PolygonID);
		const int32 MaterialIndex = FindMaterialIndexForPolygonGroup(StaticMesh, PolygonGroupMaterialSlotNames, PolygonGroupID);
		if (!BakedSlots.IsValidIndex(MaterialIndex) || !BakedSlots[MaterialIndex].bValid)
		{
			continue;
		}

		const FBakedSlot& BakedSlot = BakedSlots[MaterialIndex];
		TArray<FVertexInstanceID> PolygonVertexInstances = MeshDescription->GetPolygonVertexInstances(PolygonID);
		for (const FVertexInstanceID VertexInstanceID : PolygonVertexInstances)
		{
			const FColor BakedColor = SampleBakedColor(BakedSlot.Pixels, BakedSlot.Size, VertexInstanceUVs.Get(VertexInstanceID, UVChannel));
			VertexInstanceColors[VertexInstanceID] = FVector4f(FLinearColor(BakedColor));
			++Result.WrittenVertexInstanceCount;
		}
	}

	if (Result.WrittenVertexInstanceCount == 0)
	{
		Result.Message = TEXT("烘焙成功，但没有写入任何顶点实例颜色；请检查材质槽和多边形材质分配");
		return Result;
	}

	UStaticMesh::FCommitMeshDescriptionParams CommitParams;
	CommitParams.bMarkPackageDirty = true;
	CommitParams.bUseHashAsGuid = false;
	StaticMesh->CommitMeshDescription(LODIndex, CommitParams);
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();

	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("已将%d个材质槽BaseColor烘焙并写入%d个LOD%d顶点实例颜色。请保存StaticMesh资产。"),
		Result.BakedMaterialSlotCount,
		Result.WrittenVertexInstanceCount,
		LODIndex);
	UE_LOG(LogX_AssetEditor, Log, TEXT("[材质烘焙] %s: %s"), *StaticMesh->GetName(), *Result.Message);
	return Result;
}
