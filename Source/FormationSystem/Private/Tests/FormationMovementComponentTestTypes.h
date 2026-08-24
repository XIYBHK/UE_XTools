/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

// 说明：本测试类型不在编译期剔除（不加 #if WITH_EDITOR 守卫）——
// UHT 对 Game 目标生成的 .gen.cpp 不会继承该守卫，守卫反而导致打包构建
// 引用未声明类型而失败（与 FormationManagerComponentTestTypes.h 同一结论）。
// 类本身惰性无副作用，仅被编辑器自动化测试引用。
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FormationMovementComponent.h"
#include "FormationMovementComponentTestTypes.generated.h"

/**
 * 测试专用：阵型移动测试组件。
 * 生产路径在 BeginPlay 中缓存 OwnerCharacter，但自动化测试世界不触发 BeginPlay，
 * 故通过测试子类直接注入（OwnerCharacter 为 protected，子类可写）。
 * 不改动生产头文件的 private 访问边界，也不新增任何生产 API。
 */
UCLASS()
class FORMATIONSYSTEM_API UFormationMovementTestComponent : public UFormationMovementComponent
{
    GENERATED_BODY()

public:
    /** 测试专用：直接注入 OwnerCharacter（替代 BeginPlay 的缓存路径） */
    void SetOwnerCharacterForTest(ACharacter* InOwnerCharacter) { OwnerCharacter = InOwnerCharacter; }
};

/**
 * 测试专用：移动完成事件计数接收器。
 * 动态多播委托只能绑定 UFUNCTION，因此用计数器 UObject 验证
 * OnMovementCompleted 的广播次数与广播来源（参数签名与委托完全一致）。
 */
UCLASS()
class FORMATIONSYSTEM_API UFormationMovementTestEventReceiver : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION()
    void OnMovementCompleted(UFormationMovementComponent* MovementComponent)
    {
        CompletedCount++;
        LastComponent = MovementComponent;
    }

    /** 完成事件广播次数 */
    int32 CompletedCount = 0;

    /** 最近一次广播来源组件 */
    UFormationMovementComponent* LastComponent = nullptr;
};
