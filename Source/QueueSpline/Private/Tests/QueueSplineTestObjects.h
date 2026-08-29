/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "QueueSplineComponent.h"
#include "QueueSplineTestObjects.generated.h"

/**
 * 排队样条线目标事件测试记录器
 *
 * UFUNCTION 签名与 FQueueSplineMemberTargetEvent 完全一致（Handle + Target），
 * 通过 FScriptDelegate::BindUFunction 订阅时走标准反射调用路径，不依赖任何参数布局巧合。
 * 可编程行为（在 HandleTargetUpdated 内执行）用于构造重入、成员注销等回调期场景。
 *
 * 注：UHT 不允许 UCLASS 位于 WITH_DEV_AUTOMATION_TESTS 等预处理块内，故本类始终参与编译；
 * 无测试引用时仅增加极少量反射元数据，不影响运行时行为。
 */
UCLASS()
class UQueueSplineTargetEventRecorder : public UObject
{
	GENERATED_BODY()

public:
	/** 记录每次广播收到的成员句柄，按到达顺序追加 */
	TArray<FQueueSplineMemberHandle> ReceivedHandles;

	/** 首个外层回调（回调深度为 0）要注销的成员句柄；Id 无效表示不注销 */
	FQueueSplineMemberHandle HandleToRemoveOnFirstOuter;

	/** 外层回调（深度 0）是否递归调用一次 UpdateQueueTargets */
	bool bReenterOnOuterCallback = false;

	/** 每个回调（任意深度）是否注销当前广播的成员 */
	bool bUnregisterCurrentOnEveryCallback = false;

	/** 每个回调（任意深度）是否清空全部队列成员 */
	bool bClearMembersOnEveryCallback = false;

	/** 当前回调嵌套深度：外层派发直接触发的回调进入时为 0，递归调用内的回调进入时 >= 1 */
	int32 InCallbackDepth = 0;

	/** 是否已执行过"首外层回调注销"动作 */
	bool bRemovedInjectedMember = false;

	/** 递归调用使用的队列组件 */
	UPROPERTY()
	TObjectPtr<UQueueSplineComponent> Queue = nullptr;

	UFUNCTION()
	void HandleTargetUpdated(FQueueSplineMemberHandle Handle, FQueueSplineMoveTarget Target)
	{
		// 外层派发直接触发的回调进入时深度为 0；递归 UpdateQueueTargets 内的回调进入时 >= 1
		const bool bIsOuterCallback = (InCallbackDepth == 0);
		++InCallbackDepth;

		ReceivedHandles.Add(Handle);

		if (bUnregisterCurrentOnEveryCallback)
		{
			Queue->UnregisterQueueMember(Handle);
		}

		if (bClearMembersOnEveryCallback)
		{
			Queue->ClearQueueMembers();
		}

		if (bIsOuterCallback && !bRemovedInjectedMember && HandleToRemoveOnFirstOuter.IsValid())
		{
			bRemovedInjectedMember = true;
			Queue->UnregisterQueueMember(HandleToRemoveOnFirstOuter);
		}

		if (bIsOuterCallback && bReenterOnOuterCallback)
		{
			Queue->UpdateQueueTargets(0.0f);
		}

		--InCallbackDepth;
	}
};

UCLASS()
class UQueueSplineLifecycleTestComponent : public UQueueSplineComponent
{
	GENERATED_BODY()

public:
	void InvokeBeginPlay()
	{
		BeginPlay();
	}

	void InvokeEndPlay()
	{
		EndPlay(EEndPlayReason::RemovedFromWorld);
	}
};
