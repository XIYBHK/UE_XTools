/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

// 说明：本测试类型不在编译期剔除（不加 #if WITH_EDITOR 守卫）——
// UHT 对 Game 目标生成的 .gen.cpp 不会继承该守卫，守卫反而导致打包构建
// 引用未声明类型而失败（与 FormationMovementComponentTestTypes.h 同一结论）。
// 类本身惰性无副作用，仅被编辑器自动化测试引用。
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectPoolInterface.h"
#include "ObjectPoolLifecycleTestTypes.generated.h"

/**
 * 测试专用：实现对象池生命周期接口的计数 Actor。
 * 原生覆盖三个 BlueprintNativeEvent 的 _Implementation，仅累计调用次数，
 * 供自动化测试断言 CallLifecycleEventEnhanced 的同步/异步派发行为。
 */
UCLASS()
class OBJECTPOOL_API AObjectPoolLifecycleTestActor : public AActor, public IObjectPoolInterface
{
    GENERATED_BODY()

public:
    virtual void OnPoolActorCreated_Implementation() override { CreatedCount++; }
    virtual void OnPoolActorActivated_Implementation() override { ActivatedCount++; }
    virtual void OnReturnToPool_Implementation() override { ReturnedCount++; }

    /** 各生命周期事件的累计调用次数 */
    int32 CreatedCount = 0;
    int32 ActivatedCount = 0;
    int32 ReturnedCount = 0;
};
