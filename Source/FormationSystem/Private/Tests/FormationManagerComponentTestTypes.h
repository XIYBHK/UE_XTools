/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FormationManagerComponentTestTypes.generated.h"

/**
 * 测试专用：阵型过渡事件计数接收器。
 * 动态多播委托只能绑定 UFUNCTION，因此用计数器 UOBJECT 验证
 * OnFormationTransitionCompleted / OnFormationTransitionStopped 的广播次数（无需 World）。
 */
UCLASS()
class FORMATIONSYSTEM_API UFormationTestEventReceiver : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION()
    void OnCompleted() { CompletedCount++; }

    UFUNCTION()
    void OnStopped() { StoppedCount++; }

    /** 完成事件广播次数 */
    int32 CompletedCount = 0;

    /** 停止事件广播次数 */
    int32 StoppedCount = 0;
};

#endif // WITH_EDITOR
