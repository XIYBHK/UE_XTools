/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"

class AActor;
class UTimelineComponent;

namespace ComponentTimeline::Private
{
	UTimelineComponent* CreateTimelineInstance(AActor* ActorOwner, FName Name);
}
