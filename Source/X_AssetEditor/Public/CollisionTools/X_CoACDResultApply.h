// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
struct FCoACD_MeshArray;

/** 将 CoACD 结果写回到 StaticMesh 的 BodySetup */
bool ApplyResultToBodySetup(UStaticMesh* StaticMesh, const FCoACD_MeshArray& Result, bool bRemoveExistingCollision);


