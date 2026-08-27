/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

// 说明：本测试类不在编译期剔除（不加 #if WITH_EDITOR 守卫）——
// UHT 对 Game 目标生成的 .gen.cpp 不会继承该守卫，守卫反而导致打包构建
// 引用未声明类型而失败。类本身惰性无副作用，仅被编辑器自动化测试引用。
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
