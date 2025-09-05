// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/X_CoACDMeshOps.h"
#include "CollisionTools/X_CoACDAdapter.h"
#include "Engine/StaticMesh.h"
#include "RawMesh.h"

void BuildInputFromRawMesh(const FRawMesh& Raw, FCoACDInputBuffers& Out)
{
    Out.Vertices.SetNumUninitialized(Raw.VertexPositions.Num() * 3);
    for (int32 i = 0; i < Raw.VertexPositions.Num(); ++i)
    {
        const FVector3f& P = Raw.VertexPositions[i];
        Out.Vertices[i*3+0] = (double)P.X;
        Out.Vertices[i*3+1] = (double)P.Y;
        Out.Vertices[i*3+2] = (double)P.Z;
    }
    Out.Indices.SetNumUninitialized(Raw.WedgeIndices.Num());
    for (int32 i = 0; i < Raw.WedgeIndices.Num(); ++i)
    {
        Out.Indices[i] = Raw.WedgeIndices[i];
    }
    Out.MeshView.vertices_count = (uint64)Raw.VertexPositions.Num();
    Out.MeshView.vertices_ptr = Out.Vertices.GetData();
    Out.MeshView.triangles_count = (uint64)(Raw.WedgeIndices.Num() / 3);
    Out.MeshView.triangles_ptr = Out.Indices.GetData();
}

void CompactUnusedVertices(FRawMesh& RawMesh)
{
    TSet<uint32> UsedVIDs;
    UsedVIDs.Reserve(RawMesh.WedgeIndices.Num());
    for (int32 W : RawMesh.WedgeIndices)
    {
        if (W >= 0)
        {
            UsedVIDs.Add((uint32)W);
        }
    }

    TArray<int32> MapOldToNew;
    MapOldToNew.Init(-1, RawMesh.VertexPositions.Num());
    TArray<uint32> NewIndexOrder;
    NewIndexOrder.Reserve(UsedVIDs.Num());

    int32 NewCounter = 0;
    for (uint32 Old = 0; Old < (uint32)RawMesh.VertexPositions.Num(); ++Old)
    {
        if (UsedVIDs.Contains(Old))
        {
            MapOldToNew[Old] = NewCounter++;
            NewIndexOrder.Add(Old);
        }
    }

    if (NewCounter == RawMesh.VertexPositions.Num())
    {
        return;
    }

    TArray<FVector3f> NewPositions;
    NewPositions.SetNumUninitialized(NewCounter);
    for (int32 i = 0; i < NewCounter; ++i)
    {
        NewPositions[i] = RawMesh.VertexPositions[NewIndexOrder[i]];
    }
    RawMesh.VertexPositions = MoveTemp(NewPositions);

    for (uint32& Idx : RawMesh.WedgeIndices)
    {
        const int32 Old = (int32)Idx;
        Idx = (uint32)MapOldToNew[Old];
    }
}

bool DeleteWedgesByMaterialIDs(FRawMesh& RawMesh, const TArray<int32>& MaterialIDs, bool bCleanUpVertexPositions)
{
    if (!MaterialIDs.ContainsByPredicate([](int32 i){ return i >= 0; }))
    {
        return false;
    }

    bool bModified = false;
    for (int32 FaceIdx = RawMesh.FaceMaterialIndices.Num() - 1; FaceIdx >= 0; --FaceIdx)
    {
        if (MaterialIDs.Contains(RawMesh.FaceMaterialIndices[FaceIdx]))
        {
            RawMesh.FaceMaterialIndices.RemoveAt(FaceIdx);
            RawMesh.FaceSmoothingMasks.RemoveAt(FaceIdx);

            for (int32 j = 2; j >= 0; --j)
            {
                const int32 WedgeK = 3 * FaceIdx + j;
                if (RawMesh.WedgeColors.IsValidIndex(WedgeK)) RawMesh.WedgeColors.RemoveAt(WedgeK);
                if (RawMesh.WedgeIndices.IsValidIndex(WedgeK)) RawMesh.WedgeIndices.RemoveAt(WedgeK);
                if (RawMesh.WedgeTangentX.IsValidIndex(WedgeK)) RawMesh.WedgeTangentX.RemoveAt(WedgeK);
                if (RawMesh.WedgeTangentY.IsValidIndex(WedgeK)) RawMesh.WedgeTangentY.RemoveAt(WedgeK);
                if (RawMesh.WedgeTangentZ.IsValidIndex(WedgeK)) RawMesh.WedgeTangentZ.RemoveAt(WedgeK);
                for (int32 m = 0; m < MAX_MESH_TEXTURE_COORDS; ++m)
                {
                    if (RawMesh.WedgeTexCoords[m].IsValidIndex(WedgeK)) RawMesh.WedgeTexCoords[m].RemoveAt(WedgeK);
                }
            }
            bModified = true;
        }
    }

    if (bModified && bCleanUpVertexPositions)
    {
        TSet<uint32> UsedVIDs(RawMesh.WedgeIndices);
        TArray<uint32> UnusedVIDs;
        UnusedVIDs.Reserve(RawMesh.VertexPositions.Num());
        for (int32 i = RawMesh.VertexPositions.Num() - 1; i >= 0; --i)
        {
            if (!UsedVIDs.Contains((uint32)i)) UnusedVIDs.Add((uint32)i);
        }
        for (uint32 ID : UnusedVIDs)
        {
            RawMesh.VertexPositions.RemoveAt((int32)ID);
            for (uint32& WID : RawMesh.WedgeIndices)
            {
                ensure(WID != ID);
                if (WID > ID) WID -= 1;
            }
        }
    }
    return bModified;
}

void FilterRawMeshByKeywords(UStaticMesh* StaticMesh, FRawMesh& Raw, const TArray<FString>& Keywords)
{
    if (Keywords.Num() <= 0 || !StaticMesh) return;
    TArray<int32> BlacklistMaterialIDs;
    const TArray<FStaticMaterial>& StaticMats = StaticMesh->GetStaticMaterials();
    for (int32 Idx = 0; Idx < StaticMats.Num(); ++Idx)
    {
        const FString SlotNameStr = StaticMats[Idx].MaterialSlotName.ToString();
        const UMaterialInterface* Mat = StaticMats[Idx].MaterialInterface;
        const FString MatNameStr = Mat ? Mat->GetName() : FString();
        const FString MatPathStr = Mat ? Mat->GetPathName() : FString();
        const FString ElementStrEn = FString::Printf(TEXT("Element %d"), Idx);
        const FString ElementStrZh = FString::Printf(TEXT("元素%d"), Idx);
        const FString IndexStr = FString::FromInt(Idx);

        for (const FString& KWRaw : Keywords)
        {
            const FString KW = KWRaw.TrimStartAndEnd();
            if (KW.IsEmpty()) continue;

            bool bMatch = false;
            bMatch |= SlotNameStr.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= MatNameStr.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= MatPathStr.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= ElementStrEn.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= ElementStrZh.Contains(KW, ESearchCase::IgnoreCase);
            bMatch |= (KW.Equals(IndexStr, ESearchCase::IgnoreCase));
            if (bMatch)
            {
                BlacklistMaterialIDs.Add(Idx);
                break;
            }
        }
    }
    if (BlacklistMaterialIDs.Num() > 0)
    {
        DeleteWedgesByMaterialIDs(Raw, BlacklistMaterialIDs, /*bCleanUpVertexPositions*/ true);
    }
}


