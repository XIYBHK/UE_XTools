// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ECFTypes.generated.h"

// ECF 系统可用的混合函数
UENUM(BlueprintType)
enum class EECFBlendFunc : uint8
{
	ECFBlend_Linear UMETA(DisplayName = "线性"),
	ECFBlend_Cubic UMETA(DisplayName = "三次方"),
	ECFBlend_EaseIn UMETA(DisplayName = "缓入"),
	ECFBlend_EaseOut UMETA(DisplayName = "缓出"),
	ECFBlend_EaseInOut UMETA(DisplayName = "缓入缓出")
};

// ECF 系统中异步任务的可用优先级
UENUM(BlueprintType)
enum class EECFAsyncPrio : uint8
{
	Normal UMETA(DisplayName = "普通"),
	HiPriority UMETA(DisplayName = "高优先级")
};