// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "UObject/Interface.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"



// ✅ 生成的头文件必须放在最后
#include "ObjectPoolInterface.generated.h"

/**
 * 对象池生命周期接口 - UInterface部分
 * 
 * 这是UE的接口系统要求的UInterface类，用于蓝图系统识别
 * Actor可以实现此接口来处理池化生命周期事件
 */
UINTERFACE(BlueprintType, Blueprintable, meta = (
    DisplayName = "对象池接口",
    ToolTip = "Actor对象池生命周期接口。实现此接口的Actor可以接收池化生命周期事件（创建、激活、归还），实现自动状态管理和资源清理。",
    Keywords = "对象池,接口,生命周期,事件,状态管理,Actor",
    Category = "XTools|接口",
    ShortToolTip = "对象池生命周期接口"))
class OBJECTPOOL_API UObjectPoolInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * 对象池生命周期接口 - 实际接口定义
 * 
 * Actor可以实现此接口来自动处理池化过程中的状态管理
 * 所有事件都是可选的，只需实现需要的事件即可
 * 
 * 使用方法：
 * 1. 在Actor蓝图的Class Settings中添加此接口
 * 2. 实现需要的生命周期事件
 * 3. 对象池系统会自动调用这些事件
 */
class OBJECTPOOL_API IObjectPoolInterface
{
    GENERATED_BODY()

public:
    /**
     * 当Actor在池中首次创建时调用（仅第一次）
     * 
     * 用途：
     * - 一次性初始化逻辑
     * - 设置不会改变的属性
     * - 缓存组件引用
     * 
     * 注意：此事件在Actor的BeginPlay之前调用
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "对象池生命周期", meta = (
        DisplayName = "池Actor创建时",
        ToolTip = "Actor在对象池中首次创建时调用，用于一次性初始化。适合设置不会改变的属性、缓存组件引用、加载资源等操作。",
        Keywords = "创建,初始化,对象池,生命周期,首次,一次性",
        ShortToolTip = "Actor在池中创建时调用",
        CompactNodeTitle = "池创建",
        BlueprintInternalUseOnly = "false"))
    void OnPoolActorCreated();

    /**
     * 当Actor从对象池中被激活时调用
     * 
     * 用途：
     * - 重置Actor状态到初始状态
     * - 重新启用组件和功能
     * - 设置新的属性值
     * - 开始游戏逻辑
     * 
     * 注意：此时Actor已经设置了正确的Transform
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "对象池生命周期", meta = (
        DisplayName = "从池激活时",
        ToolTip = "Actor从对象池中被获取并激活时调用，用于重置状态和开始逻辑。每次从池中获取都会调用，适合重置变量、开始动画、启用AI等操作。",
        Keywords = "激活,获取,对象池,生命周期,重置,开始",
        ShortToolTip = "Actor从池中激活时调用",
        CompactNodeTitle = "池激活",
        BlueprintInternalUseOnly = "false"))
    void OnPoolActorActivated();

    /**
     * 当Actor即将归还到对象池时调用
     * 
     * 用途：
     * - 停止所有定时器和动画
     * - 清理临时引用和绑定
     * - 重置变量到默认值
     * - 禁用不需要的组件
     * 
     * 注意：此事件后Actor会被隐藏并停用
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "对象池生命周期", meta = (
        DisplayName = "归还到池时",
        ToolTip = "Actor即将归还到对象池时调用，用于清理状态和停止逻辑。适合停止动画、清理引用、重置变量、禁用组件等清理操作。",
        Keywords = "归还,回收,对象池,生命周期,清理,停止",
        ShortToolTip = "Actor归还到池时调用",
        CompactNodeTitle = "池归还",
        BlueprintInternalUseOnly = "false"))
    void OnReturnToPool();

    /**
     * C++版本的生命周期事件 - 可选实现
     * 
     * 如果需要在C++中处理生命周期事件，可以重写这些虚函数
     * 蓝图事件和C++函数会同时被调用
     */

    /**
     * C++版本：Actor在池中首次创建时调用
     */
    virtual void OnPoolActorCreated_Implementation() {}

    /**
     * C++版本：Actor从池中激活时调用
     */
    virtual void OnPoolActorActivated_Implementation() {}

    /**
     * C++版本：Actor归还到池时调用
     */
    virtual void OnReturnToPool_Implementation() {}

    /**
     * 检查Actor是否实现了对象池接口
     * @param Actor 要检查的Actor
     * @return true if the actor implements the interface
     */
    static bool DoesActorImplementInterface(const AActor* Actor)
    {
        return Actor && Actor->GetClass()->ImplementsInterface(UObjectPoolInterface::StaticClass());
    }

    /**
     * 安全调用Actor的生命周期事件
     * @param Actor 目标Actor
     * @param EventType 事件类型
     */
    static void SafeCallLifecycleEvent(AActor* Actor, const FString& EventType);

    /**
     * 增强版生命周期事件调用
     * @param Actor 目标Actor
     * @param EventType 事件类型枚举
     * @param bAsync 是否异步调用
     * @param TimeoutMs 超时时间（毫秒）
     * @return 调用是否成功
     */
    static bool CallLifecycleEventEnhanced(AActor* Actor, EObjectPoolLifecycleEvent EventType, bool bAsync = false, int32 TimeoutMs = 1000);

    /**
     * 批量调用生命周期事件
     * @param Actors Actor数组
     * @param EventType 事件类型
     * @param bAsync 是否异步调用
     * @return 成功调用的数量
     */
    static int32 BatchCallLifecycleEvents(const TArray<AActor*>& Actors, EObjectPoolLifecycleEvent EventType, bool bAsync = false);

    /**
     * 检查Actor是否实现了特定的生命周期事件
     * @param Actor 目标Actor
     * @param EventType 事件类型
     * @return true if the event is implemented
     */
    static bool HasLifecycleEvent(AActor* Actor, EObjectPoolLifecycleEvent EventType);

    /**
     * 获取Actor的生命周期事件统计
     * @param Actor 目标Actor
     * @return 生命周期统计信息
     */
    static FObjectPoolLifecycleStats GetLifecycleStats(AActor* Actor);

protected:
    /**
     * 内部辅助函数：执行蓝图事件
     * @param Actor 目标Actor
     * @param FunctionName 函数名
     */
    static void ExecuteBlueprintEvent(AActor* Actor, const FName& FunctionName);
};
