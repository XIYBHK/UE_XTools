/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectPoolM14TestTypes.generated.h"

/** M-14 回归测试类型：属于有效 Actor 类，但因 Abstract 标记无法生成。 */
UCLASS(Abstract)
class OBJECTPOOL_API AObjectPoolM14AbstractTestActor : public AActor
{
    GENERATED_BODY()
};
