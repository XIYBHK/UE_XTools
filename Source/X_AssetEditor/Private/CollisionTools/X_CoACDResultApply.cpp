// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/X_CoACDResultApply.h"
#include "CollisionTools/X_CoACDAdapter.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConvexElem.h"

bool ApplyResultToBodySetup(UStaticMesh* StaticMesh, const FCoACD_MeshArray& Result, bool bRemoveExistingCollision)
{
    if (!StaticMesh)
    {
        return false;
    }
    StaticMesh->Modify();
    StaticMesh->CreateBodySetup();
    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    if (!BodySetup) return false;
    if (bRemoveExistingCollision)
    {
        BodySetup->RemoveSimpleCollision();
    }
    if (Result.meshes_count == 0 || !Result.meshes_ptr) return false;

    auto& ConvexElems = BodySetup->AggGeom.ConvexElems;
    const int32 FirstIdx = ConvexElems.Num();
    ConvexElems.AddDefaulted((int32)Result.meshes_count);
    for (uint64 i = 0; i < Result.meshes_count; ++i)
    {
        FKConvexElem& Elem = ConvexElems[FirstIdx + (int32)i];
        const FCoACD_Mesh& RM = Result.meshes_ptr[i];
        if (RM.vertices_count == 0 || !RM.vertices_ptr || RM.triangles_count == 0 || !RM.triangles_ptr)
        {
            continue;
        }
        Elem.VertexData.SetNumUninitialized((int32)RM.vertices_count);
        for (uint64 p = 0; p < RM.vertices_count; ++p)
        {
            Elem.VertexData[(int32)p].X = RM.vertices_ptr[(size_t)p * 3 + 0];
            Elem.VertexData[(int32)p].Y = RM.vertices_ptr[(size_t)p * 3 + 1];
            Elem.VertexData[(int32)p].Z = RM.vertices_ptr[(size_t)p * 3 + 2];
        }
        Elem.IndexData.SetNumUninitialized((int32)RM.triangles_count * 3);
        for (int32 p = 0; p < Elem.IndexData.Num(); ++p)
        {
            Elem.IndexData[p] = RM.triangles_ptr[p];
        }
        Elem.UpdateElemBox();
    }
    BodySetup->InvalidatePhysicsData();
    return true;
}


